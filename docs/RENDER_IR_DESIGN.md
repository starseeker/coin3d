# Backend-Neutral Render IR Direction

Status: exploratory design direction, not an implementation commitment.

This note records a preferred long-term direction for hosting Obol on modern
rendering APIs without reducing the scene graph to a pre-baked triangle soup.
It is intended to seed a deeper architecture and feasibility investigation.

## Executive recommendation

Preserve Obol's scene graph, actions, and element-based inherited state as the
semantic front end. Add a new render action that translates the effective
state at drawable boundaries into a backend-neutral render intermediate
representation (IR). Pass that IR through a frame planner, then execute the
planned frame with OpenGL, Vulkan, Direct3D, or another backend.

```text
Scene graph + SoState elements
             |
             v
   semantic render action
             |
             v
    backend-neutral IR
             |
             v
       frame planner
             |
       planned frame
             |
       +-----+-----+
       |     |     |
       v     v     v
      GL   Vulkan  D3D
```

The IR must preserve live Obol semantics, resource identity, and legal ordering.
It must not bake lighting into vertex colors, permanently transform geometry to
world space, or assume that the frame is a single opaque forward pass.

The current OpenGL renderer should remain the compatibility baseline while the
new path grows alongside it. A new backend should not be considered complete
until it consumes this common IR and passes the same semantic and visual tests
as at least one other backend.

## Why a context-manager-only abstraction is insufficient

System OpenGL and OSMesa can be selected behind the same context-manager API
because both expose OpenGL semantics and the same family of entry points. A
Direct3D or Vulkan implementation cannot usefully impersonate that contract.

`SoDB::ContextManager::renderScene()` is a useful integration seam for an
alternative renderer, but it does not define how scene semantics become GPU
work. A backend implemented entirely behind that method tends to become a
separate scene collector with its own incomplete interpretation of Obol. That
is the pattern visible in the current Vulkan demonstration.

The durable abstraction belongs above API-specific command encoding:

1. Interpret the scene graph once in Obol terms.
2. Represent the interpreted frame without GL, Vulkan, or D3D objects.
3. Plan work according to backend capabilities.
4. Encode API-specific commands and manage API-specific resources.

## Current code as a starting point

Several existing pieces are useful evidence and migration aids:

- `SoGLRenderAction` defines the broad compatibility target, including
  traversal ordering, transparency policies, multipass behavior, callbacks,
  caches, and path traversal.
- `SoState` and elements already model much of Obol's inherited semantic state.
  The problem is that many `SoGL*Element` classes realize changes immediately
  as OpenGL state rather than describing them independently.
- `SoSceneRenderAction` demonstrates API-independent traversal and access to
  effective state through `SoCallbackAction`.
- `SoSceneCollector` demonstrates geometry generation, light extraction,
  screen-space proxies, overlays, and coarse scene invalidation.
- The NanoRT, Embree, and Vulkan paths identify which data a non-GL renderer
  needs and where flattening loses behavior.

`SoSceneCollector` should not become the final raster IR. It currently emits
world-space triangles, duplicates material data, substitutes proxy geometry,
and supports CPU-oriented shading and overlays. Those are reasonable choices
for a raytracing adapter, but they discard object-space resource identity and
make retained GPU resources, instancing, dynamic transforms, textures,
pipelines, and multipass effects unnecessarily expensive or impossible.

## Design principles

### Preserve semantics before optimizing

Initial IR emission should preserve traversal order exactly. Reordering is a
planner optimization and must require explicit proof that a draw is reorderable.
Opaque sorting, instancing, pass merging, and state deduplication must never
silently alter callback, transparency, depth, stencil, or path-sensitive
behavior.

### Describe intent, not API calls

The semantic layer should describe concepts such as depth comparison, texture
combination, two-sided lighting, line style, and transparency class. It should
not expose `glEnable`, Vulkan descriptor sets, D3D root signatures, or native
pipeline objects.

### Retain resource identity

Geometry, images, samplers, and generated shader variants need stable handles
and independent versioning. A draw references resources; it does not contain a
fresh copy of all resource data.

### Make unsupported behavior explicit

Every backend should publish capabilities. The planner must choose a documented
fallback or report an unsupported feature. Silent approximation should be
limited to compatibility policies selected by the application.

### Keep extensions possible without corrupting the abstraction

Portable extensions should emit semantic IR through a public extension
interface. Native callbacks may exist as an explicit backend-specific escape
hatch, but they create ordering barriers and make a scene non-portable. A GL
callback must not be represented as though it works on Vulkan or D3D.

### Support incremental adoption

The existing GL path must continue to work while nodes and features migrate.
The new renderer should be opt-in until its declared feature tier is coherent.

## Proposed architecture

### 1. Semantic render action

A provisional `SoRenderIRAction` would traverse the scene using dedicated
backend-neutral elements. At each drawable boundary it would snapshot or
intern the effective state needed by that draw and emit an IR item.

This should be a new action rather than a mode inside `SoGLRenderAction`.
Sharing semantic helpers is desirable; sharing GL-specific element classes is
not. Nodes should eventually have a backend-neutral render method or action
method in addition to their existing `GLRender()` implementation.

The action is responsible for:

- traversal, path, separator, switch, LOD, and culling semantics;
- collecting effective transforms, camera, viewport, material, texture,
  lighting, clipping, draw-style, and raster state;
- obtaining object-space geometry or procedural geometry descriptors;
- assigning stable resource handles and observing source versions;
- preserving callbacks and explicit ordering barriers;
- declaring nested render-target dependencies such as scene textures;
- attaching source node/path information for diagnostics and picking.

It is not responsible for Vulkan render passes, D3D resource barriers,
OpenGL state calls, pipeline compilation, or descriptor allocation.

### 2. Semantic render IR

The IR should be typed C++ data with a versioned internal schema. It need not be
a public ABI initially. A frame should contain four related kinds of data.

#### Frame and view description

- render extent, display extent, pixel format, sample count, and color space;
- one or more views with camera and projection data;
- clear operations and viewport/scissor definitions;
- requested output and readback operations;
- frame-level feature and quality policy.

Coordinate conventions must be defined here, including handedness, clip-space
depth range, framebuffer origin, front-face winding, and texture origin. The IR
should use one canonical convention; each executor performs its API conversion
in one well-tested location.

#### Resource declarations

- object-space vertex/index data and vertex layout;
- procedural geometry descriptors where retaining the primitive is valuable;
- images, mip levels, image formats, and update regions;
- sampler descriptions;
- shader source or portable material-program descriptions;
- render targets and transient attachments;
- stable resource handle, source identity, and content version.

Resource declarations describe desired content, not allocation. Backends own
native buffers, images, heaps, and synchronization objects.

#### Interned semantic state

A draw should reference immutable, deduplicated state blocks. Candidate state
includes:

- model, view, projection, normal, and texture transforms;
- material channels, shininess, emissive behavior, and color-material policy;
- texture bindings, coordinate generation, texture combination, and samplers;
- active lights and lighting model;
- clip planes;
- primitive topology, face orientation, culling, polygon mode, line/point size;
- depth, stencil, alpha-test, blending, color-mask, and multisample policy;
- fog and other fixed-function compatibility semantics;
- selection, picking, annotation, overlay, and diagnostic metadata.

The state is semantic even when a backend realizes most of it as a pipeline
key. Backends may lower one state block into shaders plus fixed pipeline state.

#### Ordered work items

The core work item is a draw referencing geometry, a range, instance data, and
an interned state block. The IR also needs non-draw structure:

- pass or target boundaries;
- clear and resolve operations;
- scene-texture or nested-view dependencies;
- ordering barriers;
- portable extension operations;
- backend-native callback barriers where explicitly permitted;
- overlay/screen-space primitives;
- readback or presentation requests.

An explicit pass/dependency graph is preferable to recursively invoking a
backend from scene-texture nodes. It lets Vulkan and D3D plan attachment
lifetimes and barriers while GL executes equivalent ordered framebuffer work.

### 3. Frame planner

The planner converts semantic IR into a backend-independent planned frame with
capability-dependent decisions. It should:

- validate required features against backend capabilities;
- resolve fallbacks selected by policy;
- partition work into render and compute/copy passes;
- classify opaque, masked, transparent, overlay, and order-sensitive work;
- sort or batch only work marked safe to reorder;
- construct pipeline/material variant keys;
- calculate transient attachment lifetimes and aliasing opportunities;
- schedule resolves, mip generation, copies, and readbacks;
- expose deterministic diagnostics explaining each fallback or rejection.

The planner should not own native resources. It may produce API-neutral access
and lifetime information that explicit backends lower into native barriers.

### 4. Backend device and executor

Each backend should implement a narrow set of services:

- device/context creation and capability reporting;
- native resource creation, update, retirement, and cache management;
- shader/pipeline variant compilation and caching;
- planned-frame command encoding and submission;
- presentation or pixel readback;
- timestamps, counters, labels, and validation diagnostics.

An executor receives only planned frames and resource changes. It should not
traverse `SoNode` objects or reinterpret arbitrary elements. This separation is
what prevents each backend from becoming a second, divergent Obol renderer.

## State, ordering, and callbacks

Legacy OpenGL makes mutable state and immediate effects cheap to express. An
explicit API requires their dependencies to be known before command encoding.
The safe initial rule is therefore:

- draws remain in traversal order;
- state blocks are immutable values;
- a callback or unknown extension is an ordering barrier;
- nested render targets are explicit dependencies;
- optimization occurs only within planner-recognized reorderable regions.

Portable callbacks should receive an IR encoder with a constrained semantic
API. A native callback should declare its backend and affected resource/state
scope. GL native callbacks may require the GL executor to invalidate its state
cache afterward. Vulkan and D3D callbacks need command-buffer integration and
must declare resource usage; otherwise they should be rejected.

## Geometry and resource lifetime

Geometry should normally remain object-space and be shared between instances.
The resource identity can begin with the producing node/cache identity plus a
monotonic content version, but a deeper investigation must determine which
existing notification and unique-ID mechanisms reliably distinguish:

- topology changes;
- vertex/normal/color/texture-coordinate changes;
- transform-only changes;
- material or texture changes;
- camera/view changes;
- viewport-dependent tessellation or screen-space geometry changes.

The renderer must hold strong or snapshot-safe references for every source used
after traversal. Backend work may outlive the traversal and, on explicit APIs,
one or more submitted frames. Resource retirement therefore needs frame/fence
epochs rather than immediate deletion.

The IR should permit streaming updates and dirty ranges. It should not require
rehashing or recopying a complete mesh every frame. Content hashes may assist
deduplication, but identity and explicit versions should be the normal cache
key; hashing all data is not a substitute for invalidation.

## Materials and shaders

Obol's compatibility rendering semantics should be represented as a portable
material feature description. The planner derives a canonical shader-variant
key from active features such as lighting model, normal source, texture stages,
alpha test, fog, clipping, and skinning/instancing if later added.

Each backend may initially use maintained shader templates generated from that
feature key. A future investigation can evaluate a common shader language or
cross-compilation toolchain, but the IR should not depend on one until its
deployment, debugging, licensing, and generated-code quality are understood.

Application-provided GLSL cannot automatically become portable. It should be
classified as GL-only unless Obol introduces a portable shader/module API with
explicit bindings and backend variants.

## Backend-specific assessment

### OpenGL

An IR-driven GL executor is valuable even though the legacy GL path already
works. It provides the first behavioral comparison and proves the IR without
requiring an explicit-API implementation for every debugging cycle. It can
also retain compatibility-profile fallbacks where modern shader realization is
not yet available.

The existing `SoGLRenderAction` should remain the reference during migration.
Replacing it should be a late decision, not an early requirement.

### Vulkan

Vulkan is a strong validation backend because it forces explicit pipelines,
descriptors, resource transitions, pass dependencies, and lifetime handling.
The existing demo should be treated as a prototype consumer and requirements
probe. Its geometry collection and CPU Phong pre-baking should not define the
new architecture.

### Direct3D 11

D3D11 is the pragmatic first native Windows executor. It is substantially less
work than D3D12, maps naturally to a forward renderer, and supports WARP for
deterministic software rendering on hosted CI. It will not stress every
explicit lifetime and synchronization decision as strongly as Vulkan or D3D12.

### Direct3D 12

D3D12 has roughly the same architectural burden as Vulkan: explicit resource
states, descriptor management, pipeline-state objects, command queues, and
fence-based lifetime. It is attractive after the IR and planner are proven,
or earlier if validating explicit-API correctness is a primary objective.

### Software rendering

OSMesa remains the simplest way to run broad legacy GL tests in CI. WARP would
make an IR-driven D3D backend testable on Windows. Mesa lavapipe can serve the
same role for Vulkan. Software execution is a validation tool, not a reason to
force all backends through an OpenGL-shaped abstraction.

## Capability and compatibility policy

Capabilities should be structured, not a flat list of API extension strings.
Examples include attachment counts and formats, texture dimensionality,
sampler limits, clip-plane strategy, wide line support, polygon mode, sample
counts, shader stages, timestamp support, and readback behavior.

For each unsupported semantic feature, planning should produce one of:

1. exact native realization;
2. documented shader or geometry emulation;
3. application-selected approximation;
4. explicit unsupported-feature failure.

The result should be queryable before rendering and visible in diagnostics.
This is especially important for features that modern APIs do not guarantee,
such as wide lines, point rasterization details, fixed-function texture
combiners, accumulation-buffer behavior, and compatibility-profile quirks.

## Threading model

The semantic traversal should initially remain one logical ordered traversal
per scene/view. Resource preparation and backend compilation can run in
parallel once immutable IR/resource snapshots exist. Later, known-independent
subgraphs could be recorded in parallel and merged at explicit ordering points.

The design should make ownership clear:

- an action and its transient IR builder belong to one traversal thread;
- immutable resource descriptions may be shared;
- backend devices own synchronized native caches;
- submitted frame resources retire only after backend completion;
- scene mutation concurrent with traversal is unsupported unless the scene
  graph gains a separate snapshot or read/write synchronization contract.

## Diagnostics and profiling

Every IR item should optionally retain source node ID, type, path/debug label,
and planner decision metadata. Debug builds should be able to dump a frame in a
stable human-readable form and compare normalized IR across runs.

Profiler integration should report at least:

- traversal and IR construction time;
- resource extraction/upload volume;
- cache hit/miss and pipeline compilation counts;
- planner time and draw/pass counts;
- backend CPU submission and GPU timestamp ranges;
- fallback and unsupported-feature counts.

Instrumentation should remain compiled in but disabled by default, consistent
with Obol's current profiler policy.

## Testing strategy

The new architecture needs tests at several levels.

### IR semantic tests

Small scenes should assert exact normalized IR for transforms, inherited state,
separator restoration, switches, paths, LOD, materials, texture stages,
clipping, transparency classes, and nested render targets. These tests require
no graphics API and should run on every platform.

### Planner tests

Synthetic IR should test ordering preservation, safe batching, pass dependency
construction, resource lifetime calculation, fallback selection, and useful
failure diagnostics under mocked capability sets.

### Backend contract tests

Each executor should pass common tests for resource updates, target formats,
coordinate conventions, clipping, blending, depth/stencil, readback, and device
loss where applicable. Software devices should be used in CI when practical.

### Cross-backend visual tests

The same semantic scenes and tolerances should run on legacy GL, IR-driven GL,
Vulkan, and D3D as those backends mature. Reference images alone are not enough;
numeric probes for depth, object ID, pixel position, and state restoration make
failures easier to diagnose.

### Compatibility inventory

Every existing rendering test should eventually map to a declared feature and
backend support status. A backend should advertise a coherent tier rather than
silently skipping arbitrary tests.

## Incremental migration plan

### Stage 0: architecture audit

- Inventory all direct GL calls by node, element, action, cache, and helper.
- Classify each call as semantic state, geometry submission, resource
  management, pass control, query/readback, or native extension.
- Map current rendering tests to those semantics.
- Identify nodes whose behavior depends on callback ordering or GL escape
  hatches.

Deliverable: a feature matrix and several representative frame traces.

### Stage 1: minimal semantic IR

- Add the IR data model, builder, validator, and stable dump format.
- Add a new render action for camera, transforms, basic materials, lights,
  triangles, lines, and points.
- Preserve strict traversal order.
- Implement a simple IR-driven GL executor.

Deliverable: basic scenes match the legacy GL path without using
`SoSceneCollector`'s world-space flattening.

### Stage 2: retained resources and invalidation

- Introduce stable geometry/image handles and source versions.
- Add per-backend resource caches and fence/epoch retirement.
- Support transform-only, material-only, and partial buffer updates.
- Add cache diagnostics and stress tests.

Deliverable: dynamic and instanced scenes avoid full-frame geometry rebuilds.

### Stage 3: state completeness and frame planning

- Add textures, samplers, texture transforms/combiners, alpha test, fog,
  clipping, depth/stencil, blend modes, and overlays.
- Add planner regions, pipeline keys, safe batching, and capability fallbacks.
- Represent scene textures and multipass dependencies explicitly.

Deliverable: a substantial portable rendering-test subset with shared planner
behavior.

### Stage 4: first explicit/native backend

- Make the existing Vulkan work consume planned frames, or implement a D3D11
  executor if Windows CI and deployment are the immediate priority.
- Use lavapipe or WARP in CI.
- Compare both semantic output and performance counters against IR-driven GL.

Deliverable: two materially different APIs consume the same front end and
planner without backend scene traversal.

### Stage 5: advanced compatibility

- Shadows, advanced transparency, bump/normal mapping, shader extensions,
  selection/highlight rendering, stereo/multiview, and complex overlays.
- Define explicit policy for GL-native callbacks and unsupported legacy
  behavior.
- Decide whether IR-driven GL can replace portions of `SoGLRenderAction`.

Deliverable: a documented compatibility tier suitable for real applications.

## Suggested source organization

Names are provisional, but ownership boundaries should be visible in the tree.

```text
include/Inventor/render/
  SoRenderIR.h
  SoRenderCapabilities.h
  SoRenderBackend.h

src/render/
  ir/          semantic values, builder, validation, dumps
  frontend/    action, neutral elements, geometry adapters
  planner/     passes, ordering, pipeline keys, lifetimes
  resources/   identities, versions, snapshots
  backends/
    gl/
    vulkan/
    d3d11/
    d3d12/
```

Public API exposure should wait until the internal model has survived at least
two backends. Prematurely freezing the IR as ABI would make necessary schema
changes costly.

## Questions for the follow-up investigation

1. Which current `SoGL*Element` classes combine semantic state with immediate
   GL realization, and what neutral element split would minimize duplication?
2. Which node classes bypass elements and issue GL operations directly?
3. What are the exact observable ordering guarantees around callbacks,
   transparency, annotations, delayed paths, and nested scene textures?
4. Can existing node/field unique IDs provide reliable category-specific
   invalidation, or is a new versioned resource-observer layer required?
5. Which geometry generators depend on viewport, complexity, or backend state,
   and which outputs can be retained safely?
6. How should portable custom shaders and custom render nodes declare bindings,
   resources, and fallback behavior?
7. Which legacy features should be emulated, retained as GL-only, or formally
   deprecated?
8. Should the first second backend be Vulkan, to maximize architecture pressure,
   or D3D11, to minimize time to a useful Windows/WARP renderer?
9. What image tolerances and semantic probes distinguish legitimate API raster
   differences from actual incompatibility?
10. How does the IR integrate with `SoRenderManager`, `SoOffscreenRenderer`, and
    `SoDB::ContextManager` without making any one of them the backend model?

## Success criteria

This direction is validated when:

- nodes and elements express portable rendering semantics once;
- no non-GL executor traverses or interprets arbitrary scene nodes;
- object-space geometry and textures persist across frames and instances;
- GL and at least one of Vulkan or D3D consume the same planned frame;
- ordering-sensitive and multipass behavior is represented explicitly;
- unsupported features fail or fall back predictably with useful diagnostics;
- the shared test corpus demonstrates semantic and visual equivalence;
- adding another backend primarily requires resource and command execution,
  not another scene collector.

The most important early discipline is to resist calling a flattened geometry
collector the backend-neutral renderer. It is a useful adapter for raytracing
and a useful prototype, but a full raster architecture must retain the state,
resource, ordering, and pass semantics that make Obol more than a mesh loader.
