# Obol Coverage Baseline and Expansion Plan

*Baseline captured: 2026-02-23.  Build: Debug + OSMesa, `COIN_COVERAGE=ON`.*

---

## Baseline metrics (overall project)

| Metric | Count | Total | Percentage |
|--------|------:|------:|-----------:|
| Lines (all files) | 36,056 | 97,507 | **37.0 %** |
| Lines (`src/` only) | 27,572 | 87,284 | **31.6 %** |
| Functions (`src/` only) | 6,560 | 16,102 | **40.7 %** |

---

## Per-subsystem breakdown (`src/`)

| Subsystem | Lines hit | Total lines | Line % | Funcs hit | Total funcs | Func % |
|-----------|----------:|------------:|-------:|----------:|------------:|-------:|
| `sensors` | 503 | 686 | **73.3 %** | 106 | 144 | **73.6 %** |
| `lists` | 356 | 483 | **73.7 %** | 75 | 102 | **73.5 %** |
| `elements` | 3,894 | 6,431 | **60.6 %** | 1,269 | 1,769 | **71.7 %** |
| `io` | 1,180 | 2,262 | **52.2 %** | 198 | 352 | **56.2 %** |
| `nodekits` | 955 | 2,016 | **47.4 %** | 170 | 344 | **49.4 %** |
| `engines` | 1,496 | 3,550 | **42.1 %** | 283 | 856 | **33.1 %** |
| `threads` | 223 | 542 | **41.1 %** | 46 | 84 | **54.8 %** |
| `glue` | 937 | 2,315 | **40.5 %** | 63 | 290 | **21.7 %** |
| `misc` | 1,752 | 4,846 | **36.2 %** | 549 | 1,160 | **47.3 %** |
| `fields` | 1,813 | 5,400 | **33.6 %** | 794 | 2,183 | **36.4 %** |
| `actions` | 1,007 | 3,120 | **32.3 %** | 224 | 560 | **40.0 %** |
| `rendering` | 1,365 | 4,375 | **31.2 %** | 122 | 650 | **18.8 %** |
| `nodes` | 3,173 | 10,384 | **30.6 %** | 845 | 1,983 | **42.6 %** |
| `shapenodes` | 2,343 | 7,731 | **30.3 %** | 283 | 652 | **43.4 %** |
| `base` | 2,723 | 9,801 | **27.8 %** | 415 | 1,249 | **33.2 %** |
| `shaders` | 447 | 1,704 | **26.2 %** | 187 | 462 | **40.5 %** |
| `events` | 121 | 585 | **20.7 %** | 48 | 88 | **54.5 %** |
| `draggers` | 920 | 5,499 | **16.7 %** | 179 | 679 | **26.4 %** |
| `shadows` | 141 | 1,239 | **11.4 %** | 46 | 117 | **39.3 %** |
| `geo` | 72 | 622 | **11.6 %** | 29 | 145 | **20.0 %** |
| `projectors` | 49 | 730 | **6.7 %** | 11 | 101 | **10.9 %** |
| `manips` | 48 | 1,158 | **4.1 %** | 37 | 217 | **17.1 %** |
| `collision` | 5 | 1,289 | **0.4 %** | 3 | 85 | **3.5 %** |
| `profiler` | 23 | 2,745 | **0.8 %** | 9 | 305 | **3.0 %** |
| `hardcopy` | 17 | 1,215 | **1.4 %** | 10 | 158 | **6.3 %** |
| `tools` | 0 | 36 | **0.0 %** | 0 | 8 | **0.0 %** |

---

## Top files by uncovered lines (opportunities)

Files with the most uncovered lines that are **realistically testable** (skipping
profiler, hardcopy, collision and large third-party files):

| File | Uncovered | Total | Line % |
|------|----------:|------:|-------:|
| `fonts/struetype.h` (font rasterizer) | 1,648 | 2,269 | 27 % |
| `glue/gl.cpp` (GL dispatch) | 1,093 | 1,969 | 45 % |
| `actions/SoGLRenderAction.cpp` | 680 | 862 | 21 % |
| `shadows/SoShadowGroup.cpp` | 1,019 | 1,107 | 8 % |
| `draggers/SoDragger.cpp` | 561 | 703 | 20 % |
| `shapenodes/SoShape.cpp` | 554 | 756 | 27 % |
| `rendering/SoGLImage.cpp` | 551 | 855 | 36 % |
| `base/SbDPMatrix.cpp` | 536 | 666 | 20 % |
| `nodekits/SoBaseKit.cpp` | 516 | 894 | 42 % |
| `io/SoInput.cpp` | 490 | 870 | 44 % |
| `rendering/SoOffscreenRenderer.cpp` | 405 | 550 | 26 % |
| `shapenodes/SoIndexedFaceSet.cpp` | 308 | 445 | 31 % |
| `shapenodes/SoQuadMesh.cpp` | 305 | 418 | 27 % |
| `shapenodes/SoFaceSet.cpp` | 286 | 482 | 41 % |
| `elements/GL/SoGLLazyElement.cpp` | 317 | 712 | 56 % |
| `base/SbMatrix.cpp` | 331 | 694 | 52 % |

---

## Expansion plan (priority order)

Each item lists the planned test, the subsystem it improves, and an estimated
lines-uncovered gain.

### Tier 1 — High impact, low effort (~10–15 % total coverage gain)

These render a scene or exercise well-understood APIs and can be written quickly
as self-validating rendering tests.

| Priority | Test to add | Subsystem(s) improved | Estimated gain |
|----------|-------------|----------------------|----------------|
| 1 | **`render_indexed_face_set`** — large `SoIndexedFaceSet` scene with normals, texture coords, `SoCoordinate3`, `SoNormal`, `SoTextureCoordinate2` | `shapenodes`, `bundles`, `elements` | +300 lines |
| 2 | **`render_quad_mesh`** — `SoQuadMesh` grid with per-vertex material binding | `shapenodes`, `nodes` | +300 lines |
| 3 | **`render_indexed_line_set`** — `SoIndexedLineSet` multi-segment polylines | `shapenodes` | +350 lines |
| 4 | **`render_point_set`** — `SoPointSet` + `SoVertexProperty` large point cloud | `shapenodes`, `nodes` | +180 lines |
| 5 | **`render_lod`** — `SoLOD` and `SoLevelOfDetail` switching between representations | `nodes` | +250 lines |
| 6 | **`render_scene_texture`** — `SoSceneTexture2` offscreen render-to-texture | `nodes`, `rendering` | +570 lines |
| 7 | **`render_shadow`** — `SoShadowGroup` + `SoShadowStyle` with a casting sphere | `shadows` | +1,000 lines |
| 8 | **`render_array_multiple_copy`** — `SoArray` and `SoMultipleCopy` instanced geometry | `nodes` | +180 lines |

### Tier 2 — Medium impact, medium effort (~5–8 % gain)

Require slightly more complex scenes or specific conditions:

| Priority | Test to add | Subsystem(s) improved | Estimated gain | Status |
|----------|-------------|----------------------|----------------|--------|
| 9 | **`render_shape_hints`** — `SoShapeHints` (SOLID/COUNTERCLOCKWISE/CONVEX) effect on backface culling and normals | `nodes`, `shapenodes` | +180 lines | ✅ Done |
| 10 | **`render_environment`** — `SoEnvironment` fog, ambient colour | `nodes` | +140 lines | ✅ Done |
| 11 | **`render_texture3`** — `SoTexture3` (3-D texture) applied to a cube | `nodes`, `rendering` | +185 lines | ✅ Done |
| 12 | **`render_bump_map`** — `SoBumpMap` + `SoBumpMapCoordinate` on a sphere | `nodes`, `rendering` | +125 lines | ✅ Done |
| 13 | **`render_texture_transform`** — `SoTexture2Transform` scale/rotate/translate | `nodes` | +90 lines | ✅ Done |
| 14 | **`render_depth_buffer`** — `SoDepthBuffer` enable/disable/func modes | `nodes` | +80 lines | ✅ Done |
| 15 | **`render_alpha_test`** — `SoAlphaTest` threshold scene | `nodes` | +50 lines | ✅ Done |
| 16 | **Action integration** — `SoRayPickAction` against a complex scene (face set + transforms); validate picked path, point, detail | `actions`, `misc` | +400 lines | ✅ Done (added to `tests/actions/test_actions_suite.cpp`) |
| 17 | **Matrix math** — `SbDPMatrix` operations: mult, invert, decompose | `base` | +450 lines | ✅ Done (added to `tests/base/test_sb_types.cpp`) |
| 18 | **`SbMatrix` full suite** — det, factor, inverse, multVecMatrix, multDirMatrix | `base` | +300 lines | ✅ Done (added to `tests/base/test_sb_types.cpp`) |
| 19 | **`SoField` connection / notification** — connect/disconnect, isConnected, notify propagation | `fields`, `misc` | +350 lines | ✅ Done (`tests/fields/test_field_connections.cpp`) |
| 20 | **`SoNodeKit` traversal** — instantiate a nodekit, set part, traverse with `SoGetBoundingBoxAction` | `nodekits` | +450 lines | ✅ Done (`tests/nodes/test_nodekit_traversal.cpp`) |

> **Note on control images (Priority 9–15):** Control images for Tier 2 rendering tests were
> generated using the bundled OSMesa backend (`COIN3D_USE_OSMESA=ON`) because the GLX CI
> environment had difficulties with context setup.  These should be regenerated from a GLX
> environment when possible (see `tests/rendering/generate_controls.sh`).

### Tier 3 — Lower priority / harder to test

| Priority | Area | Reason / approach |
|----------|------|-------------------|
| 21 | `draggers/` (16 % line coverage) | Need display + event simulation; start with simple draggers (`SoTranslatePlaneDragger`) |
| 22 | `manips/` (4 %) | Built on draggers; test manipulator attach/detach lifecycle |
| 23 | `rendering/SoOffscreenRenderer.cpp` (26 %) | Test getBuffer(), getViewportRegion(), multiple renders with same renderer |
| 24 | `glue/gl.cpp` (45 %) | Exercises GL entry-point dispatch; covered by rendering tests, diminishing returns |
| 25 | `io/SoInput.cpp` + `SoOutput.cpp` | Add edge cases: binary read, nested files, invalid inputs |
| 26 | `fonts/` (34 %) | SoText2 / SoText3 visual tests once font baseline settled |
| 27 | `profiler/` (1 %) | Enable profiling in a render loop and verify statistics |
| 28 | `collision/` (0 %) | `SoIntersectionDetectionAction` against a scene with known overlaps |

---

## How to regenerate the baseline

```bash
# Full workflow:
cmake -S . -B build_cov \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCOIN3D_USE_OSMESA=ON \
    -DCOIN_COVERAGE=ON

cmake --build build_cov --parallel $(nproc)
cmake --build build_cov --target coverage

# Open the report:
xdg-open build_cov/coverage_report/index.html
```

The `coverage` target resets gcov counters, runs the full CTest suite (continuing
even if some tests fail), captures lcov data, strips system/third-party paths, and
emits an HTML report to `<build>/coverage_report/`.

---

## Coverage targets

| Milestone | Line % goal | Notes |
|-----------|------------:|-------|
| Baseline (current) | 31.6 % | `src/` only; Feb 2026 |
| After Tier 1 | ~ 45 % | +~13 % from 8 rendering tests |
| After Tier 2 | ~ 55 % | +~10 % from unit/integration tests |
| After Tier 3 | ~ 65 % | +~10 % from harder subsystems |
| Long-term goal | ≥ 70 % | Covers all actively-maintained APIs |
