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
├── Acceleration structures (rebuilt lazily)
│   ├── CadInstanceBVH   (world-space AABB tree of all instances)
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
#include <obol/cad/SoCADAssembly.h>
#include <obol/cad/SoCADDetail.h>

SoCADAssembly::initClass();   // also calls SoCADDetail::initClass()
```

### Adding parts

```cpp
using namespace obol;

// Create a wire-only part (no tessellation needed)
WireRep wire;
WirePolyline poly;
poly.points = { SbVec3f(0,0,0), SbVec3f(1,0,0), SbVec3f(1,1,0) };
wire.polylines.push_back(poly);
wire.bounds.setBounds(SbVec3f(0,0,0), SbVec3f(1,1,0));

PartGeometry geom;
geom.wire = std::move(wire);

PartId pid = CadIdBuilder::hash128("my_part_name");
assembly->upsertPart(pid, geom);
```

### Adding instances

```cpp
InstanceRecord rec;
rec.part        = pid;
rec.parent      = CadIdBuilder::Root();
rec.childName   = "wheel";
rec.occurrenceIndex = 0;
rec.boolOp      = 0;  // union
rec.localToRoot.makeIdentity();
rec.style.hasColorOverride = true;
rec.style.color = SbColor4f(1, 0.5f, 0, 1);

InstanceId iid = assembly->upsertInstanceAuto(rec);
```

### Batch updates (performance)

Wrap multiple inserts in `beginUpdate()` / `endUpdate()` to defer internal
rebuilds until the end of the batch:

```cpp
assembly->beginUpdate();
for (const auto& part : newParts) {
    assembly->upsertPart(part.id, part.geom);
}
for (const auto& inst : newInstances) {
    assembly->upsertInstanceAuto(inst);
}
assembly->endUpdate();  // one rebuild instead of N
```

### Fast transform edits

Interactive tools (draggers, manipulators) should use the fast-path APIs
to avoid rebuilding the entire frame plan:

```cpp
assembly->updateInstanceTransform(iid, newMatrix);   // O(1) + BVH refit
assembly->updateInstanceStyle(iid, newStyle);        // O(1), no BVH rebuild
```

### Querying instance data (for on-demand node materialisation)

After a pick, retrieve the full record for an instance to build an explicit
scene-graph node (e.g. for interactive editing):

```cpp
std::optional<obol::InstanceRecord> rec = assembly->getInstanceRecord(iid);
if (rec) {
    auto* shape = new MyPartShape(rec->part, rec->localToRoot, rec->style);
    sceneRoot->addChild(shape);
    // Suppress double-rendering: hide the instance from aggregate rendering.
    assembly->setHiddenInstances({ iid });
}
```

When the user confirms the edit, call `assembly->upsertInstance(iid, updatedRec)`
and clear the hidden set to return to aggregate rendering.

---

## Render modes (drawMode field)

| Value                | Wire polylines | Triangles | Depth-only triangles |
|----------------------|:--------------:|:---------:|:--------------------:|
| `WIREFRAME`          | ✓              | –         | optional (see below) |
| `SHADED`             | –              | ✓         | –                    |
| `SHADED_WITH_EDGES`  | ✓              | ✓         | –                    |

### Wireframe occlusion

When `drawMode = WIREFRAME` and `wireframeOcclusion = TRUE`, the renderer
runs a depth-only triangle pass (using coarse LoD if available) before the
wire pass.  This makes auxiliary `OCCLUDED` objects (see DepthPolicy) respect
the CAD surfaces even in wireframe mode.

---

## Pick modes (pickMode field)

| Value           | Algorithm                                                       |
|-----------------|-----------------------------------------------------------------|
| `PICK_AUTO`     | EDGE in wireframe; TRIANGLE in shaded                           |
| `PICK_EDGE`     | Always pick against wire polyline segments                      |
| `PICK_TRIANGLE` | Always pick against shaded triangle mesh (Möller–Trumbore BVH)  |
| `PICK_BOUNDS`   | Bounding-box proxy only (fastest; least precise)                |
| `PICK_HYBRID`   | Try edge first, then triangle, then bounds                      |

Picking returns an `SoCADDetail` attached to `SoPickedPoint`.

Edge-pick tolerance is specified in screen-space pixels via the
`edgePickTolerancePx` field.  The renderer converts it to a world-space
tolerance based on the current view volume and viewport, so it remains
visually consistent across zoom levels.

### Pick result detail

```cpp
SoPickedPoint* pp = ...; // from SoRayPickAction
const SoCADDetail* detail = 
    dynamic_cast<const SoCADDetail*>(pp->getDetail());
if (detail) {
    obol::InstanceId iid = detail->getInstanceId();
    obol::PartId     pid = detail->getPartId();
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

---

## DepthPolicy for auxiliary objects

`obol::DepthPolicy` (in `include/obol/render/DepthPolicy.h`) controls how
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
`CadIdBuilder::extendNameOccBool` to derive deterministic IDs from the
traversal path:

```cpp
using namespace obol;
InstanceId root  = CadIdBuilder::Root();
InstanceId car   = CadIdBuilder::extendNameOccBool(root,  "car",    0, 0);
InstanceId wheel = CadIdBuilder::extendNameOccBool(car,   "wheel",  0, 0); // FL
InstanceId bolt  = CadIdBuilder::extendNameOccBool(wheel, "bolt",   2, 0); // 3rd bolt
```

The same traversal order always produces the same InstanceId.  Different
occurrence indices or sibling orders produce different IDs.

### Part IDs

```cpp
PartId pid = CadIdBuilder::hash128("my_solid_name");  // from a string key
PartId pid2 = CadIdBuilder::hash128(keyBytes, keyLen); // from raw bytes
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

### POP-buffer quantisation

Both `SegmentPopLod` and `TrianglePopLod` use a POP-inspired discretisation:

1. Normalise all coordinates to `[0, 1]` relative to the part bounding box.
2. At LoD level `L` the grid has `2^(L+1)` cells per axis.
3. A primitive is **degenerate** (dropped) if all its endpoints snap to the
   same grid cell.
4. `minLevelForSegment(i)` / `minLevelForTriangle(i)` is the lowest level at
   which primitive `i` is non-degenerate.

### Level selection

The renderer selects a LoD level for each part based on the distance from
the camera to the instance's world-space bounding sphere:

```
ratio  = dist / radius
level  = floor(255 / (1 + ratio * 0.5))
```

This heuristic gives full detail (`level = 255`) up to `radius * 2` from
the camera and progressively coarser levels as the object recedes.  LoD is
only applied in Tier-0 (immediate-mode) rendering; the VBO-loop and
instanced tiers draw full detail at all distances (level 255 passes through
unchanged).

### Focus set (selected/hovered instances)

Force full-detail LoD for selected or hovered instances:

```cpp
assembly->setSelectedInstances({ pickedIid });
// Renderer will use level 255 for these instances regardless of distance.
```

---

## Rendering tiers

| Tier | Requirements | Method |
|------|-------------|--------|
| 2 | GL 3.1 + instanced shaders | One draw call per unique part |
| 1 | GL 2.0 + GLSL 1.10 + VBOs | Per-instance loop, frustum-culled |
| 0 | GL 1.1 (Mesa swrast fallback) | `glBegin`/`glEnd`, frustum-culled, LoD-aware |

Per-instance frustum culling is active in Tier 0 and Tier 1: each instance's
world bounding box is tested against the six frustum planes before issuing
any draw call, skipping fully off-screen instances at no GPU cost.

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
* **Tier-2 LoD/frustum culling**: the instanced path (Tier 2) does not yet
  apply per-instance frustum culling or LoD; frustum and LoD features are
  active in Tier 0 and Tier 1 only.
* **Wireframe occlusion**: the `wireframeOcclusion` field is exposed but the
  depth-only triangle prepass is not yet implemented.
* **ID stability across reloads**: without stable per-node GUIDs,
  InstanceIds may change if the traversal order changes.
* **Analytic curves**: wireframe geometry is stored as polylines.  Analytic
  arc/nurbs picking (snapping) is not implemented in v1.

---

## File index

| Path | Description |
|------|-------------|
| `include/obol/cad/CadIds.h`        | 128-bit ID types + `CadIdBuilder` |
| `include/obol/cad/SoCADAssembly.h` | Main assembly node API            |
| `include/obol/cad/SoCADDetail.h`   | Pick-result detail class          |
| `include/obol/render/DepthPolicy.h`| Depth policy enum                 |
| `src/cad/CadIds.cpp`               | FNV-1a 128-bit hash implementation|
| `src/cad/SoCADAssembly.cpp`        | Node render/pick/bbox actions     |
| `src/cad/SoCADDetail.cpp`          | Detail SO_DETAIL_SOURCE           |
| `src/cad/CadFramePlan.h`           | Internal frame plan structs       |
| `src/cad/CadGpuResources.h/.cpp`   | Per-context VBO cache (isUpToDate fast-path) |
| `src/cad/lod/SegmentPopLod.h/.cpp` | POP LoD for segments              |
| `src/cad/lod/TrianglePopLod.h/.cpp`| POP LoD for triangles             |
| `src/cad/picking/CadPicking.h/.cpp`| CPU BVH picking (edge + triangle) |
| `tests/cad/test_cad_ids.cpp`       | Unit tests: ID generation         |
| `tests/cad/test_segment_lod.cpp`   | Unit tests: segment LoD           |
| `tests/cad/test_triangle_lod.cpp`  | Unit tests: triangle LoD          |
| `tests/cad/test_cad_picking.cpp`   | Unit tests: edge + triangle picking |
