# CAD Assembly Renderer

## Overview

`SoCADAssembly` is an Inventor-style node for rendering large CAD assemblies
efficiently in Obol.  Unlike conventional Open Inventor scenes, where the
renderer walks a tree of `SoShape` nodes, `SoCADAssembly` accepts geometry
via an explicit ingestion API and renders everything from a compact
**frame plan** – a pre-sorted list of instanced draw calls.

---

## Rationale: why scene traversal doesn't scale

Open Inventor's traversal model visits every node in the scene graph for
every rendered frame.  For typical interactive scenes (hundreds of shapes)
this is fast enough.  For CAD assemblies with:

* 20 000–10 000 000 **part instances**
* 20 000–1 000 000 **unique parts**

per-node overhead dominates.  Even an O(1) operation per node becomes
expensive at 10 M nodes.

`SoCADAssembly` avoids this by:

1. Storing all instance transforms and styles in a flat hash map (not a tree).
2. Building a **CadFramePlan** once per dirty frame instead of walking children.
3. Emitting draw calls with `glMultiDrawElementsIndirect` (or a per-item
   fallback) rather than one draw call per node.

---

## Architecture

```
SoCADAssembly
│
├── Part library   (PartId → PartGeometry)
│   ├── WireRep    (polylines – no tessellation required)
│   └── TriMesh    (optional triangles for shaded mode)
│
├── Instance database  (InstanceId → InstanceData)
│   ├── transform (SbMatrix localToRoot)
│   ├── style     (color override, lineWidth)
│   └── world bounds (cached)
│
├── Hidden-instance set  (excluded from rendering and frame plan)
│   └── Use setHiddenInstances() to suppress aggregate rendering for
│       instances materialised as explicit scene-graph nodes.
│
├── Unpickable-instance set  (visible, but excluded from pick BVH)
│   └── Use setUnpickableInstances() for visible-but-not-selectable
│       instances without materialising separate scene-graph nodes.
│
├── Acceleration structures (rebuilt lazily)
│   ├── CadInstanceBVH   (world-space AABB tree of pickable instances)
│   ├── CadPartEdgeBVH   (per-part AABB tree of wire segments, built on demand)
│   └── CadPartTriBVH    (per-part AABB tree of triangles, built on demand)
│
└── CadFramePlan  (cached; rebuilt only when dirty)
    ├── visibleInstances  (sorted by partIndex for batching;
    │                      includes world bounding box for frustum culling)
    ├── wireItems         (draw items for wire pass)
    └── shadedItems       (draw items for shaded pass)
```

---

## API Usage

### Initialisation

Register the node type once at application start:

```cpp
#include <Obol/cad/SoCADAssembly.h>
#include <Obol/cad/SoCADDetail.h>
#include <Obol/cad/SoCADViewState.h>

SoCADAssembly::initClass();   // also registers CAD detail and view-state types
```

### Publishing parts and instances

Geometry validation belongs at the producer boundary.  The presentation
thread then applies one checked sparse transaction, so a rejected style, cut,
part, or instance cannot expose a partially updated scene:

`PartGeometryBuilder` is the mutable producer form.  Admission validates it
and privately constructs a const `PartGeometry` snapshot; clients cannot
construct or modify the renderer-visible type.  Move a completed builder to
transfer large arrays without copying.  An intentional lvalue admission makes
an independent snapshot, so later builder edits cannot change live geometry.

```cpp
using namespace Obol;

// Create a wire-only part (no tessellation needed).
PartGeometryBuilder builder;
WireRep wire;
WirePolyline poly;
poly.points = { SbVec3f(0,0,0), SbVec3f(1,0,0), SbVec3f(1,1,0) };
wire.polylines.push_back(poly);
wire.bounds.setBounds(SbVec3f(0,0,0), SbVec3f(1,1,0));
builder.wire = std::move(wire);

const CadGeometryAdmission admitted = cadAdmitPartGeometry(std::move(builder));
if (!admitted) {
    // Report cadGeometryErrorName(admitted.validation.error).
    return;
}

const PartId pid = CadIdBuilder::partId("my_part_name");
const InstanceId iid = CadIdBuilder::childInstance(
    CadIdBuilder::rootInstance(), "wheel", 0, 0);

InstanceRecord rec;
rec.part = pid;
rec.localToRoot.makeIdentity();
rec.style.hasColorOverride = true;
rec.style.color = SbColor4f(1, 0.5f, 0, 1);

CadSceneMutation mutation;
mutation.parts.push_back({pid, admitted.geometry});
mutation.instances.push_back({iid, rec});
const CadSceneMutationResult published =
    assembly->applySceneMutation(mutation);
if (!published) {
    // Report cadSceneMutationDomainName(published.domain) and the
    // corresponding geometry or scene validation.
    return;
}
```

### Adding a view policy

`SoCADAssembly` is one view's retained presentation. Non-geometric per-view
policy is supplied by an `SoCADViewState` node earlier in that traversal
branch. Progressive cuts are producer-authored per instance; the CAD node
does not derive them from its camera:

```cpp
SoCADViewState* view = new SoCADViewState;
view->viewIdLow.setValue(1);

root->addChild(view);
root->addChild(assembly);
```

Do not traverse one assembly from independent views.  An assembly owns
camera-local classifiers, preparation cursors, renderer resources, and the
last completed-frame report.  Admit an immutable geometry snapshot once and
copy its `ValidatedPartGeometry` token into each view-local transaction.  This
duplicates lightweight presentation records, not mesh arrays.

### Sparse transactions and complete replacement

Use one sparse transaction for progressive publication, edits, semantic
updates, and removals.  Validation is side-effect free and proportional to the
delta; `applySceneMutation()` opens one nested-safe update window only after
the complete delta is known to be valid:

```cpp
Obol::CadSceneMutation mutation;
mutation.parts = { ... };             // admitted immutable geometry
mutation.instances = { ... };
mutation.styles = { ... };
mutation.cuts = { ... };
mutation.removedInstances = { ... };
mutation.removedParts = { ... };

const Obol::CadSceneMutationResult changed =
    assembly->applySceneMutation(mutation);
if (!changed)
    return;
```

For a complete rebuild, admit immutable geometry on its producer workers and
use the checked replacement operation.  It validates both complete domains
before changing the last valid presentation.  Candidate allocation is also
completed before publication; `ResourceUnavailable` therefore leaves the
preceding scene live just like a validation rejection:

```cpp
std::vector<Obol::PartUpdate> parts = { ... };
std::vector<Obol::InstanceUpdate> instances = { ... };

const Obol::CadSceneReplacementResult replaced =
    assembly->replaceScene(parts, instances);
if (!replaced) {
    // The preceding scene is still live.  Inspect replaced.error and the
    // corresponding validation, or report ResourceUnavailable.
    return;
}
```

`replaceScene()` removes the preceding hidden/selected/unpickable sets along
with its parts, instances, retained progressive prefixes, BVHs, and cached
frame plans.  `clear()` remains available for intentionally emptying an
assembly; clients must not compose it with several checked insert operations
to emulate a replacement.

Sparse publication is deliberately proportional to the mutation rather than
the retained scene population.  `applySceneMutation()` stages rollback data
only for records and part-index buckets the journal can change.  Validation,
staging, and application therefore have the strong exception guarantee: an
allocator failure returns `CadSceneMutationDomain::ResourceUnavailable` and
leaves the preceding authoritative scene live.  Producers should still keep
sparse journals bounded and reserve a known streaming population with
`reserveStreamingCapacity()` before sustained publication.

### Fast transform edits

Interactive tools (draggers, manipulators) should use the fast-path APIs
to avoid rebuilding the entire frame plan:

```cpp
if (!assembly->updateInstanceTransform(iid, newMatrix))
    return;
if (!assembly->updateInstanceStyle(iid, newStyle))
    return;
```

### Querying instance data (for on-demand node materialisation)

After a pick, retrieve the full record for an instance to build an explicit
scene-graph node (e.g. for interactive editing):

```cpp
std::optional<Obol::InstanceRecord> rec = assembly->instanceRecord(iid);
if (rec) {
    Obol::ValidatedPartGeometry geometry =
        assembly->partGeometrySnapshot(rec->part);
    if (!geometry)
        return;
    auto* shape = new MyPartShape(rec->part, rec->localToRoot, rec->style);
    sceneRoot->addChild(shape);
    // Suppress double-rendering: hide the instance from aggregate rendering.
    assembly->setHiddenInstances({ iid });
}
```

When the user confirms the edit, publish the updated record in a
`CadSceneMutation` and clear the hidden set to return to aggregate rendering.

### Visibility and pickability sets

```cpp
assembly->setHiddenInstances(hiddenIds);         // no render, bounds, pick
assembly->setUnpickableInstances(unpickableIds); // render/bounds only
assembly->setSelectedInstances(selectedIds);     // selection/style state
```

Hidden instances are omitted from frame plans, bounding boxes, primitive counts,
and picking.  Unpickable instances remain visible and contribute bounds, but
the pick BVH ignores them.  This lets a CAD owner keep large selected,
unselected, hidden, and visible-but-not-selectable subsets aggregated as one
compiled assembly instead of expanding them into many scene-graph nodes.

If a subset needs behavior that is not stable per-instance state, such as a
different render pass, a custom manipulator, or detailed edit/inspection
semantics, create a purpose-specific CAD assembly or materialise explicit nodes
for that subset.  Bulk selection by itself should stay aggregated; individual
editing and per-primitive state are the usual promotion triggers.

---

## Render modes (`SoCADViewState::drawMode`)

| Value                | Wire polylines | Triangles | Depth-only triangles |
|----------------------|:--------------:|:---------:|:--------------------:|
| `WIREFRAME`          | ✓              | –         | optional (see below) |
| `SHADED`             | –              | ✓         | –                    |
| `SHADED_WITH_EDGES`  | ✓              | ✓         | –                    |
| `HIDDEN_LINE`        | ✓              | –         | ✓                    |

### Wireframe occlusion

When `drawMode = WIREFRAME` and `wireframeOcclusion = TRUE`, the renderer
runs a depth-only triangle pass using the same active progressive cut before the
wire pass.  This makes auxiliary `OCCLUDED` objects (see DepthPolicy) respect
the CAD surfaces even in wireframe mode.

---

## Pick modes (`SoCADViewState::pickMode`)

| Value           | Algorithm                                                       |
|-----------------|-----------------------------------------------------------------|
| `PICK_AUTO`     | EDGE in wireframe; TRIANGLE in shaded                           |
| `PICK_EDGE`     | Always pick against wire polyline segments                      |
| `PICK_TRIANGLE` | Always pick against shaded triangle mesh (Möller–Trumbore BVH)  |
| `PICK_BOUNDS`   | Bounding-box proxy only (fastest; least precise)                |
| `PICK_HYBRID`   | Try edge first, then triangle, then bounds                      |

Picking returns an `SoCADDetail` attached to `SoPickedPoint`.

Edge-pick tolerance is specified in screen-space pixels via the
`edgePickTolerancePixels` field.  The renderer converts it to a world-space
tolerance based on the current view volume and viewport, so it remains
visually consistent across zoom levels.

### Pick result detail

```cpp
SoPickedPoint* pp = ...; // from SoRayPickAction
const SoCADDetail* detail = 
    dynamic_cast<const SoCADDetail*>(pp->getDetail());
if (detail) {
    Obol::InstanceId iid = detail->getInstanceId();
    Obol::PartId     pid = detail->getPartId();
    if (detail->getPrimType() == SoCADDetail::EDGE) {
        // polyline index + segment index within that polyline
        uint32_t polyIdx = detail->getPrimIndex0();
        uint32_t segIdx  = detail->getPrimIndex1();
    } else if (detail->getPrimType() == SoCADDetail::TRIANGLE) {
        // triangle index (= mesh.indices offset / 3)
        uint32_t triIdx = detail->getPrimIndex0();
    }
}
```

Applications that need domain-specific pick metadata should subclass
`SoCADAssembly` and override `createPickDetail(const CadPickDetailRecord&)`.
The default implementation returns `SoCADDetail`; subclasses can return any
`SoDetail` subclass while preserving the accelerated CPU picking path. The
record contains the picked instance id, part id, primitive kind and indices,
model-space hit point, and interpolation parameter.

When an assembly is traversed as a normal scene-graph node, the detail is
attached to that assembly node.  If an owner delegates to an internal assembly
that is not itself in the active pick path, Obol attaches the detail to the
picked path tail so `pp->getDetail()` still returns the accelerated CAD detail.

---

## DepthPolicy for auxiliary objects

`Obol::DepthPolicy` (in `include/Obol/render/DepthPolicy.h`) controls how
non-CAD world objects interact with the depth buffer:

| Value            | GL depth test | Description                             |
|------------------|:-------------:|-----------------------------------------|
| `OCCLUDED`       | ON (GL_LESS)  | Hidden by closer CAD geometry (default) |
| `ALWAYS_VISIBLE` | OFF           | Always drawn on top                     |
| `XRAY`           | Two-pass      | Partially visible through surfaces      |

Attach a `DepthPolicy` to any auxiliary line grid, annotation, or overlay
that should be composited after the main CAD pass.

---

## ID generation

### Instance IDs (no stable GUID available)

When your CAD system has no per-node GUID (e.g. BRL-CAD comb trees), use
`CadIdBuilder::childInstance` to derive deterministic IDs from the
traversal path:

```cpp
using namespace Obol;
InstanceId root  = CadIdBuilder::rootInstance();
InstanceId car   = CadIdBuilder::childInstance(root,  "car",    0, 0);
InstanceId wheel = CadIdBuilder::childInstance(car,   "wheel",  0, 0); // FL
InstanceId bolt  = CadIdBuilder::childInstance(wheel, "bolt",   2, 0); // 3rd bolt
```

The same traversal order always produces the same InstanceId.  Different
occurrence indices or sibling orders produce different IDs.

### Part IDs

```cpp
PartId pid = CadIdBuilder::partId("my_solid_name");  // from a string key
PartId pid2 = CadIdBuilder::partId(keyBytes, keyLen); // from raw bytes
```

### Caveats

* InstanceIds are **session-stable** only as long as the traversal path is
  reproduced in the same order.  If comb-tree traversal order changes between
  sessions, IDs change.  This limits persistence across file reloads when no
  stable GUID is available.
* Two sibling combs with the same name under the same parent but different
  occurrence indices will get different IDs; however if the comb tree is
  ambiguous (no occurrence tracking), collisions are possible.

---

## LoD strategy

### Producer-authored retained prefixes

The geometry producer supplies exact coordinates in activation order, an
ordered vector of independently drawable cuts, quantization bounds, and the
minimum/resident cut interval.  Each cut records its cumulative primitive
count and its own per-axis quantization precision; no precision or population
rule is inferred from the cut ordinal.  `InstanceRecord::lodCut` selects one
active cut and `CadSceneMutation::cuts` changes only those selections.

The CAD node deliberately does not inspect camera distance, projected size, or
selection to choose a cut, and it never builds a second hierarchy. A view
owner may therefore share one immutable resident geometry snapshot between
view-local assemblies while assigning a different active cut to each view's
occurrence record. Shaded rendering, wire rendering,
hidden-line depth, and picking all clamp to the same producer-authored prefix
and snap retained exact coordinates with the same quantization rule.

Growing a progressive part appends CPU/GPU tails when possible. Changing an
active cut does not rebuild the part or upload geometry. Replacing a part is
reserved for actual prefix growth, trimming, or source invalidation.

Obol deliberately does not contain a second mesh simplifier.  Cache format,
cut scheduling, projected-error selection, residency, and frame-budget policy
belong to the geometry producer/view owner; Obol validates and renders the
explicit retained-cut contract.

---

## Rendering tiers

| Tier | Requirements | Method |
|------|-------------|--------|
| 2 | GL 3.1 + instanced/indirect shaders | Batched by part and active cut |
| 1 | GL 2.0 + GLSL 1.10 + VBOs | Per-instance loop, frustum-culled |
| 0 | GL 1.1 (Mesa swrast fallback) | `glBegin`/`glEnd`, frustum-culled, progressive-cut aware |

Per-instance frustum culling is active in Tier 0 and Tier 1: each instance's
world bounding box is tested against the six frustum planes before issuing
any draw call, skipping fully off-screen instances at no GPU cost.

Producer-authored progressive cuts are active in every tier.  The retained
frame plan bins occurrences by part and active cut so a shared part remains
batchable without forcing all occurrences to use the same detail.

GPU resource uploads are short-circuited in all tiers: `CadGpuResources`
tracks a per-part generation counter and skips the entire CPU-side
array-flattening step if the GPU data is already current.  The frame plan
itself is cached across camera moves and only rebuilt when geometry, instances,
styles, selection, or draw mode change.

---

## Limitations

* **Line width**: rendered using `glLineWidth`; many drivers clamp to 1 px.
  Thick-line rendering requires geometry shaders or triangle-based lines.
* **Transparency**: no alpha-sorting is implemented.  Semi-transparent CAD
  parts may render with incorrect blending.
* **ID stability across reloads**: without stable per-node GUIDs,
  InstanceIds may change if the traversal order changes.
* **Analytic curves**: wireframe geometry is stored as polylines.  Analytic
  arc/nurbs picking (snapping) is not implemented in v1.

---

## File index

| Path | Description |
|------|-------------|
| `include/Obol/cad/CadIds.h`        | 128-bit ID types + `CadIdBuilder` |
| `include/Obol/cad/CadGeometry.h`   | Immutable retained geometry       |
| `include/Obol/cad/CadSceneRecords.h` | Part and occurrence mutations    |
| `include/Obol/cad/CadSceneMutation.h` | Atomic sparse scene transaction |
| `include/Obol/cad/CadSceneReplacement.h` | Atomic replacement result    |
| `include/Obol/cad/CadSceneValidation.h` | Pure scene-record validation  |
| `include/Obol/cad/SoCADAssembly.h` | Main assembly node API            |
| `include/Obol/cad/SoCADDetail.h`   | Pick-result detail class          |
| `include/Obol/cad/SoCADViewState.h`| Per-view CAD render policy node   |
| `include/Obol/render/DepthPolicy.h`| Depth policy enum                 |
| `src/cad/CadIds.cpp`               | FNV-1a 128-bit hash implementation|
| `src/cad/SoCADAssembly.cpp`        | Coin node, mutation, picking, and action surface |
| `src/cad/CadAssemblyImpl.h`        | Private retained-state seam       |
| `src/cad/CadAssemblyPlan.cpp`      | Retained frame-plan/cache maintenance |
| `src/cad/CadAssemblyClassification.cpp` | Camera-local subpixel classification |
| `src/cad/CadSceneMutation.cpp`     | Sparse transaction diagnostics   |
| `src/cad/CadSceneValidation.cpp`   | Pure scene-record validation      |
| `src/cad/CadRendererGL.cpp`        | Renderer selection and state boundary |
| `src/cad/CadRendererGLFlat.cpp`    | Flattened batch execution         |
| `src/cad/CadRendererGLIndirect.cpp`| Indirect command execution        |
| `src/cad/CadRendererGLInstanced.cpp`| Instanced command execution      |
| `src/cad/CadRendererGLExecutors.cpp`| Retained/direct execution        |
| `src/cad/SoCADDetail.cpp`          | Detail SO_DETAIL_SOURCE           |
| `src/cad/SoCADViewState.cpp`       | View-state element and node        |
| `src/cad/CadFramePlan.h`           | Internal frame plan structs       |
| `src/cad/CadGpuResources.h/.cpp`   | Per-context VBO cache (isUpToDate fast-path) |
| `src/cad/picking/CadPicking.h/.cpp`| CPU BVH picking (edge + triangle) |
| `tests/cad/test_cad_ids.cpp`       | Unit tests: ID generation         |
| `tests/cad/test_cad_picking.cpp`   | Unit tests: edge + triangle picking |
