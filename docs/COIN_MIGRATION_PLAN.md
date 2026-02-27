# Obol Modernization and Refactor Plan

## Overview

This document tracks the migration and modernization of Obol (a fork of Coin3D/Open Inventor) into a lightweight, toolkit-agnostic, C++17 scene-graph library.  The goal is a self-contained core with no GUI toolkit dependencies, clean C++ threading, callback-driven context management, and full headless/CI test coverage.

For the API-level differences between Obol and upstream Coin3D see `docs/API_DIFFERENCES.md`.

---

## 1. Goals

- **Modernize scene management:** Retain the full Open Inventor 2.1 scene-graph and action system while removing heavyweight/unused subsystems.
- **Enable advanced selection and manipulation:** Support object and sub-object (vertex, edge, face) picking and manipulation as first-class features, with generic APIs.
- **Dragger/manipulator API:** Provide a robust, extensible, toolkit-agnostic dragger suite enabling interactive transformations (move, rotate, scale) across all frontends.
- **Generic rendering and event handling:** Toolkit-neutral rendering (`SoRenderManager`/`SoSceneManager`) and context management (`SoDB::ContextManager`) so applications own the OpenGL context completely.
- **Eliminate toolkit dependencies:** No direct dependencies on Qt, Xt, Win32, or other GUI toolkits from the Obol core.
- **C++17 throughout:** Replace C89-era platform shims with standard-library equivalents.
- **Lightweight and maintainable:** Only essential scene, action, and manipulation features are retained; legacy and heavyweight components are removed.

---

## 2. Migration Status and Phases

### Status Summary (as of early 2026)

All major phases of the original plan are now complete.  The codebase has been substantially modernized beyond the scope of the initial September 2025 plan.  The outstanding items are incremental improvements, not blockers.

---

### Phase 1: Core Scene Graph and Action Layer Extraction ✅ Complete

All core types are implemented and passing tests:

- **Nodes:** `SoNode`, `SoGroup`, `SoSeparator`, `SoTransform`, `SoMatrixTransform`, geometry nodes (`SoCoordinate3`, `SoIndexedFaceSet`, `SoIndexedLineSet`, `SoFaceSet`, `SoLineSet`, `SoPointSet`, `SoTriangleStripSet`, `SoQuadMesh`, `SoMarkerSet`, `SoIndexedMarkerSet`, …), appearance nodes (`SoMaterial`, `SoDrawStyle`, `SoBaseColor`, `SoPackedColor`, …), cameras (`SoPerspectiveCamera`, `SoOrthographicCamera`, `SoFrustumCamera`), lights, switches, LOD, texture, shader, and animation nodes.
- **Fields:** Complete field hierarchy (`SoField`, `SoSFFloat`, `SoSFVec3f`, `SoMFVec3f`, …) including field connections and engines.
- **Actions:** `SoGLRenderAction`, `SoGetBoundingBoxAction`, `SoRayPickAction`, `SoHandleEventAction`, `SoCallbackAction`, `SoSearchAction`, `SoGetMatrixAction`, `SoGetPrimitiveCountAction`, `SoWriteAction`, `SoBoxHighlightRenderAction`, `SoLineHighlightRenderAction`, simplify/reorganize actions.
- **Sensors:** Full sensor hierarchy (`SoNodeSensor`, `SoFieldSensor`, `SoTimerSensor`, `SoAlarmSensor`, `SoOneShotSensor`, `SoIdleSensor`).
- **Toolkit bindings (SoQt, SoXt, SoWin, etc.), VRML, ScXML, audio, geospatial, NURBS, and collision-detection removed.**

---

### Phase 2: Picking, Selection, and Sub-Object Detail ✅ Complete

All picking and selection infrastructure is in place and toolkit-agnostic:

- `SoRayPickAction` — ray/scene intersection, no GUI toolkit dependency.
- `SoPickedPoint`, `SoPath` — full pick-result description.
- Detail types: `SoFaceDetail`, `SoLineDetail`, `SoPointDetail`, `SoConeDetail`, `SoCubeDetail`, `SoCylinderDetail`, `SoTextDetail`.
- `SoSelection` and `SoExtSelection` — selection nodes with application-registerable callbacks.
- `SoEventCallback` — in-scene event delivery.
- `SoHandleEventAction` — event traversal decoupled from any GUI toolkit.
- `SoEventManager` — scene event dispatch; ScXML navigation state-machine dependency removed.
- `SoLocateHighlight` — hover/highlight support.
- Tests for pick interaction, selection callbacks, and ext-selection event sequences.

---

### Phase 3: Dragger API and Manipulator Suite ✅ Complete

The full Coin3D dragger and manipulator suite has been recovered, reviewed, and is present in the build.  All draggers operate on scene-graph events via `SoHandleEventAction`; no GUI toolkit types appear in the dragger or core scene logic.

**Dragger classes (22):**
`SoDragger`, `SoCenterballDragger`, `SoDirectionalLightDragger`, `SoDragPointDragger`, `SoHandleBoxDragger`, `SoJackDragger`, `SoPointLightDragger`, `SoRotateCylindricalDragger`, `SoRotateDiscDragger`, `SoRotateSphericalDragger`, `SoScale1Dragger`, `SoScale2Dragger`, `SoScale2UniformDragger`, `SoScaleUniformDragger`, `SoSpotLightDragger`, `SoTabBoxDragger`, `SoTabPlaneDragger`, `SoTrackballDragger`, `SoTransformBoxDragger`, `SoTransformerDragger`, `SoTranslate1Dragger`, `SoTranslate2Dragger`.

**Manipulator classes (12):**
`SoTransformManip`, `SoCenterballManip`, `SoHandleBoxManip`, `SoJackManip`, `SoTrackballManip`, `SoTransformBoxManip`, `SoTransformerManip`, `SoTabBoxManip`, `SoDirectionalLightManip`, `SoPointLightManip`, `SoSpotLightManip`, `SoClipPlaneManip`.

Dragger tests (`tests/draggers/test_draggers.cpp`, `test_dragger_sequences.cpp`) and rendering tests for draggers and manipulators are included.

---

### Phase 4: Generic Rendering and Event Handling ✅ Complete (different shape than originally planned)

The original plan called for a new `SoRenderArea` class.  In practice the equivalent functionality is provided by the combination of existing, refactored classes:

| Originally planned | What exists |
|---|---|
| `SoRenderArea::setContextCallbacks()` | `SoDB::ContextManager` abstract interface; `SoDB::init(ContextManager*)` |
| `SoRenderArea::setSceneRoot()` / `render()` | `SoRenderManager` — full rendering pipeline, pre/post-render callbacks, stereo, render modes, auto-clipping |
| `SoRenderArea::setEventCallback()` | `SoSceneManager::processEvent()` / `SoEventManager::processEvent()` |
| `SoRenderArea::setResizeCallback()` | `SoRenderManager::setWindowSize()` / `setViewportRegion()` |

Key achievements:

- **`SoDB::ContextManager`** (public abstract class in `SoDB`) — applications implement `createOffscreenContext`, `makeContextCurrent`, `restorePreviousContext`, `destroyContext` and pass the instance to `SoDB::init()`.  This is the sole point of platform-specific context logic; Obol itself has none.
- **All platform-specific GL context code removed** (~8,000 lines): `gl_wgl`, `gl_glx`, `gl_agl`, `gl_cgl`, `gl_egl` source pairs and corresponding offscreen data helpers deleted (see `docs/PLATFORM_CLEANUP_SUMMARY.md`).
- **`SoRenderManager`** — toolkit-neutral rendering with pre/post render callbacks, render modes (wireframe, points, bounding box, hidden line, …), stereo, auto-clipping.
- **`SoSceneManager`** — combined render + event manager; wraps `SoRenderManager` and `SoHandleEventAction`.
- **`SoEventManager`** — event dispatch for interactive scenes; ScXML state-machine dependency removed.
- **`SoOffscreenRenderer::setContextManager()`** — per-renderer context override without touching the global singleton.
- **Font handling** — Win32 GDI and system `libfreetype` both removed; all platforms use the embedded `struetype` TrueType rasterizer (zero external font dependency).
- **Fixed DPI** — offscreen renderer uses 72 DPI constant; applications scale as needed.
- **FLTK viewer** (`tests/obol_viewer.cpp`) demonstrates integration of all the above with a real GUI toolkit using only Obol's public API.

---

### Phase 5: Documentation, Testing, and CI ✅ Largely Complete

- **`docs/API_DIFFERENCES.md`** — comprehensive 22-section migration guide covering every behavioral difference from upstream Coin3D.
- **`docs/CONTEXT_MANAGEMENT_API.md`** — `SoDB::ContextManager` public API reference with OSMesa worked example.
- **`docs/PLATFORM_CLEANUP_SUMMARY.md`** — lists the 18 platform-specific files (~8,000 lines) removed and the portable replacements.
- **`docs/THREADING_MIGRATION.md`** — C++17 threading migration summary.
- **`docs/STORAGE_MIGRATION.md`** — storage migration status and analysis.
- **169 test files** covering core, actions, nodes, rendering, draggers, manipulators, selection, picking, threading, sensors, engines, fields, and more.
- CI runs all tests headlessly via OSMesa; image-comparison tests validate rendering output.

---

### Phase 6: Legacy Code Cleanup ✅ Complete (by deletion, not archiving)

Rather than moving legacy toolkit-specific code to a separate branch, it has been removed outright:

- ~8,000 lines of WGL/GLX/AGL/CGL/EGL platform-specific context code deleted.
- Win32 GDI font backend deleted.
- All VRML/VRML97 nodes and infrastructure (70+ headers) deleted.
- ScXML state-machine framework and navigation layer (53 headers) deleted.
- OpenAL audio subsystem deleted.
- Geospatial (`SoGeo*`) nodes deleted.
- NURBS nodes deleted.
- `SoIntersectionDetectionAction` deleted.
- `SoGlyph` (deprecated in Coin) deleted.
- `cc_string` C ADT excluded from build.
- `utf8/ini.cpp` INI file library excluded from build.
- Nine `SoGLImage` viewer-loop methods removed.
- `SoSFLong`/`SoMFLong`/`SoSFULong`/`SoMFULong` removed.
- `Inventor/C/` public header tier removed (internal C APIs replaced by `Sb*` C++ classes).

---

## 3. Additional Work Completed Beyond the Original Plan

### Threading: C++17 Migration ✅ Complete

All Coin3D C-wrapper threading primitives replaced with C++17 standard-library equivalents while preserving public `Sb*` class interfaces.  See `docs/THREADING_MIGRATION.md` for details.

| Class | C++17 backing |
|---|---|
| `SbMutex` | `std::mutex` |
| `SbThreadMutex` | `std::recursive_mutex` |
| `SbCondVar` | `std::condition_variable` |
| `SbThread` | `std::thread` |
| `SbThreadAutoLock` | `std::lock_guard` |
| `SbRWMutex` | custom using `std::shared_mutex` |
| `SbBarrier` | custom C++17 (std::barrier is C++20) |
| `SbFifo` | `std::deque` + `std::mutex` + `std::condition_variable` |

### Storage: Thread Cleanup FIXME Resolved ✅ Complete

`cc_storage_thread_cleanup()` was a multi-year unimplemented stub.  Addressed with a C++17 RAII `ThreadCleanupTrigger` + `StorageRegistry` system that automatically cleans up per-thread data when threads exit.  Full `SbStorage`/`SbTypedStorage` API compatibility preserved.  See `docs/STORAGE_MIGRATION.md`.

### New Nodes and APIs

- **`SoProceduralShape`** — callback-driven procedural geometry node; applications register shape types by name with JSON parameter schemas, bounding-box callbacks, and geometry callbacks.  Participates fully in all standard actions.
- **`SbFont`** — public C++ font management API backed by the embedded `struetype` rasterizer.  Replaces the internal `cc_flw_*` C API.
- **`SuUtils.h`** — promotes `SbAtexitStaticInternal()` to the public header so custom node classes outside the Obol source tree can use the `SO_NODE_INIT_CLASS` macros correctly.
- **HUD subsystem** (`SoHUD`, `SoHUDLabel`, `SoHUDButton`, `SoHUDKit`) — 2-D heads-up display overlay nodes.
- **Shadow effects** (`SoShadowGroup`, `SoShadowDirectionalLight`, `SoShadowSpotLight`, `SoShadowStyle`, `SoShadowCulling`) — real-time shadow rendering extension.

---

## 4. Outstanding / Future Work

### SbStorage: Full C++17 Migration (deferred to C++20+)

`SbStorage`/`SbTypedStorage` still use the original dictionary-based C implementation.  A full migration to `thread_local` is blocked by C++17's inability to enumerate thread-local instances across threads (needed for `applyToAll()` — used for cross-thread cache invalidation).  The thread-cleanup gap has been addressed (see §3).  A full migration can be revisited when C++20+ thread_local enumeration support becomes available.

### C++20 Migration Opportunities (future)

When C++20 becomes the minimum standard, consider:
- `SbBarrier` → `std::barrier`
- `SbThread` → `std::jthread` (automatic cleanup, stop tokens)
- `std::stop_token` for cooperative thread cancellation
- `std::latch` for one-shot synchronization
- `std::atomic_ref` for lock-free operations where appropriate

### Integration Samples for Additional Toolkits (optional)

The FLTK viewer demonstrates Obol integration end-to-end.  Similar examples for other toolkits (GLFW, SDL2, Qt via `QOpenGLWidget`) would lower the barrier for new integrators.  The `SoDB::ContextManager` pattern is straightforward; adapting the FLTK viewer is a good starting point.

---

## 5. API Consistency and Coding Style

- All APIs maintain the Coin3D/C++ style: class-based, field-oriented, extensible via inheritance.
- Event/callback hooks use clear, type-safe interfaces; favor simple structs or explicit methods over void-pointer callbacks where possible.
- Core remains 100% toolkit-agnostic; all platform/GUI-specific logic is handled by the application through well-defined abstract interfaces (`SoDB::ContextManager`, `SoRenderManager`, `SoEventManager`).
- C++17 is the minimum standard.

---

## 6. References

- `docs/API_DIFFERENCES.md` — complete API migration guide (Obol vs. Coin3D)
- `docs/CONTEXT_MANAGEMENT_API.md` — `SoDB::ContextManager` API reference
- `docs/PLATFORM_CLEANUP_SUMMARY.md` — platform code removal summary
- `docs/THREADING_MIGRATION.md` — C++17 threading migration details
- `docs/STORAGE_MIGRATION.md` — storage migration status and analysis
- [Coin3D upstream](https://github.com/coin3d/coin) — reference for recovered/reviewed APIs