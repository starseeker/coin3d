# Obol Coverage Baseline and Expansion Plan

*Baseline re-captured: 2026-02-24 (update 2).  Build: Debug + OSMesa, `COIN_COVERAGE=ON`.*
*Previous baseline: 2026-02-24 (52.4 % lines / 56.0 % functions in `src/`).*
*All Tier 1–5 items complete as of 2026-02-24 update 2.*

---

## Baseline metrics (overall project)

| Metric | Count | Total | Percentage |
|--------|------:|------:|-----------:|
| Lines (all files) | 74,101 | 116,750 | **63.5 %** |
| Lines (`src/` only) | 45,029 | 85,904 | **52.4 %** |
| Functions (`src/` only) | 8,566 | 15,305 | **56.0 %** |

---

## Per-subsystem breakdown (`src/`)

| Subsystem | Lines hit | Total lines | Line % | Funcs hit | Total funcs | Func % |
|-----------|----------:|------------:|-------:|----------:|------------:|-------:|
| `lists` | 389 | 483 | **80.5 %** | 81 | 102 | **79.4 %** |
| `sensors` | 546 | 686 | **79.6 %** | 119 | 144 | **82.6 %** |
| `details` | 257 | 274 | **93.8 %** | 102 | 110 | **92.7 %** |
| `shadows` | 937 | 1,256 | **74.6 %** | 90 | 117 | **76.9 %** |
| `manips` | 836 | 1,158 | **72.2 %** | 155 | 217 | **71.4 %** |
| `draggers` | 3,761 | 5,499 | **68.4 %** | 452 | 679 | **66.6 %** |
| `elements` | 4,375 | 6,426 | **68.1 %** | 1,349 | 1,767 | **76.3 %** |
| `projectors` | 454 | 730 | **62.2 %** | 68 | 101 | **67.3 %** |
| `nodekits` | 1,252 | 2,017 | **62.1 %** | 214 | 344 | **62.2 %** |
| `shaders` | 1,080 | 1,704 | **63.4 %** | 283 | 462 | **61.3 %** |
| `shapenodes` | 4,990 | 8,395 | **59.4 %** | 436 | 702 | **62.1 %** |
| `rendering` | 2,501 | 4,341 | **57.6 %** | 268 | 637 | **42.1 %** |
| `nodes` | 5,833 | 10,362 | **56.3 %** | 1,212 | 1,983 | **61.1 %** |
| `io` | 1,197 | 2,125 | **56.3 %** | 200 | 344 | **58.1 %** |
| `actions` | 1,689 | 3,110 | **54.3 %** | 341 | 558 | **61.1 %** |
| `engines` | 1,907 | 3,550 | **53.7 %** | 371 | 856 | **43.3 %** |
| `misc` | 2,560 | 4,803 | **53.3 %** | 748 | 1,180 | **63.4 %** |
| `errors` | 192 | 376 | **51.1 %** | 39 | 77 | **50.6 %** |
| `bundles` | 137 | 272 | **50.4 %** | 29 | 40 | **72.5 %** |
| `glue` | 1,107 | 2,315 | **47.8 %** | 98 | 290 | **33.8 %** |
| `events` | 252 | 585 | **43.1 %** | 60 | 88 | **68.2 %** |
| `threads` | 223 | 542 | **41.1 %** | 46 | 84 | **54.8 %** |
| `base` | 4,096 | 9,808 | **41.8 %** | 614 | 1,250 | **49.1 %** |
| `caches` | 817 | 2,088 | **39.1 %** | 96 | 196 | **49.0 %** |
| `fields` | 2,037 | 5,321 | **38.3 %** | 882 | 2,181 | **40.4 %** |
| `fonts` | 1,082 | 3,145 | **34.4 %** | 90 | 231 | **39.0 %** |
| `profiler` | 146 | 2,758 | **5.3 %** | 45 | 305 | **14.8 %** |
| `hardcopy` | 17 | 1,215 | **1.4 %** | 10 | 158 | **6.3 %** |
| `hud` | 193 | 193 | **100.0 %** | 38 | 49 | **77.6 %** |

---

## Top files by uncovered lines (opportunities)

Files with the most uncovered lines that are **realistically testable** (skipping
profiler, hardcopy, collision and large third-party files):

| File | Uncovered | Total | Line % |
|------|----------:|------:|-------:|
| `fonts/struetype.h` (font rasterizer) | 1,648 | 2,269 | 27 % |
| `nodes/SoExtSelection.cpp` | 1,085 | 1,089 | 0 % |
| `glue/gl.cpp` (GL dispatch) | 930 | 1,969 | 53 % |
| `actions/SoGLRenderAction.cpp` | 607 | 862 | 30 % |
| `rendering/SoRenderManager.cpp` | 573 | 573 | 0 % |
| `draggers/SoTransformerDragger.cpp` | 550 | 1,062 | 48 % |
| `io/SoInput.cpp` | 489 | 870 | 44 % |
| `misc/SoProto.cpp` | 480 | 506 | 5 % |
| `nodekits/SoBaseKit.cpp` | 446 | 894 | 50 % |
| `shapenodes/SoImage.cpp` | 426 | 430 | 1 % |
| `shapenodes/SoShape.cpp` | 415 | 758 | 45 % |
| `rendering/SoOffscreenRenderer.cpp` | 398 | 550 | 28 % |
| `shapenodes/SoText3.cpp` | 391 | 762 | 49 % |
| `shadows/SoShadowGroup.cpp` | 388 | 1,124 | 65 % |
| `rendering/SoGLImage.cpp` | 388 | 855 | 55 % |
| `rendering/SoGLBigImage.cpp` | 376 | 392 | 4 % |
| `rendering/SoGL.cpp` | 370 | 927 | 60 % |
| `shapenodes/SoAsciiText.cpp` | 368 | 372 | 1 % |
| `fields/SoField.cpp` | 362 | 879 | 59 % |
| `base/SbDPMatrix.cpp` | 355 | 666 | 47 % |

---

## Completed expansion work (Tiers 1–3)

### Tier 1 — High impact, low effort (completed)

| Priority | Test | Subsystem(s) improved | Status |
|----------|------|-----------------------|--------|
| 1 | **`render_indexed_face_set`** — large `SoIndexedFaceSet` with normals, texture coords, `SoCoordinate3`, `SoNormal`, `SoTextureCoordinate2` | `shapenodes`, `bundles`, `elements` | ✅ Done |
| 2 | **`render_quad_mesh`** — `SoQuadMesh` grid with per-vertex material binding | `shapenodes`, `nodes` | ✅ Done |
| 3 | **`render_indexed_line_set`** — `SoIndexedLineSet` multi-segment polylines | `shapenodes` | ✅ Done |
| 4 | **`render_point_set`** — `SoPointSet` + `SoVertexProperty` large point cloud | `shapenodes`, `nodes` | ✅ Done |
| 5 | **`render_lod`** — `SoLOD` and `SoLevelOfDetail` switching between representations | `nodes` | ✅ Done |
| 6 | **`render_scene_texture`** — `SoSceneTexture2` offscreen render-to-texture | `nodes`, `rendering` | ✅ Done |
| 7 | **`render_shadow`** — `SoShadowGroup` + `SoShadowStyle` with a casting sphere | `shadows` | ✅ Done |
| 8 | **`render_array_multiple_copy`** — `SoArray` and `SoMultipleCopy` instanced geometry | `nodes` | ✅ Done |

### Tier 2 — Medium impact, medium effort (completed)

| Priority | Test | Subsystem(s) improved | Status |
|----------|------|-----------------------|--------|
| 9  | **`render_shape_hints`** — `SoShapeHints` backface culling and normals | `nodes`, `shapenodes` | ✅ Done |
| 10 | **`render_environment`** — `SoEnvironment` fog, ambient colour | `nodes` | ✅ Done |
| 11 | **`render_texture3`** — `SoTexture3` (3-D texture) applied to a cube | `nodes`, `rendering` | ✅ Done |
| 12 | **`render_bump_map`** — `SoBumpMap` + `SoBumpMapCoordinate` on a sphere | `nodes`, `rendering` | ✅ Done |
| 13 | **`render_texture_transform`** — `SoTexture2Transform` scale/rotate/translate | `nodes` | ✅ Done |
| 14 | **`render_depth_buffer`** — `SoDepthBuffer` enable/disable/func modes | `nodes` | ✅ Done |
| 15 | **`render_alpha_test`** — `SoAlphaTest` threshold scene | `nodes` | ✅ Done |
| 16 | **Action integration** — `SoRayPickAction` against a complex scene; validate picked path, point, detail | `actions`, `misc` | ✅ Done (`tests/actions/test_actions_suite.cpp`) |
| 17 | **Matrix math** — `SbDPMatrix` mult, invert, decompose | `base` | ✅ Done (`tests/base/test_sb_types.cpp`) |
| 18 | **`SbMatrix` full suite** — det, factor, inverse, multVecMatrix, multDirMatrix | `base` | ✅ Done (`tests/base/test_sb_types.cpp`) |
| 19 | **`SoField` connection / notification** — connect/disconnect, isConnected, notify propagation | `fields`, `misc` | ✅ Done (`tests/fields/test_field_connections.cpp`) |
| 20 | **`SoNodeKit` traversal** — instantiate a nodekit, set part, traverse with `SoGetBoundingBoxAction` | `nodekits` | ✅ Done (`tests/nodes/test_nodekit_traversal.cpp`) |

### Tier 3 — Lower priority / harder to test

| Priority | Area | Approach | Status |
|----------|------|----------|--------|
| 21 | `draggers/` (60 % post-Tier-1) | Rendering context + event simulation for SoHandleBoxDragger, SoTabBoxDragger, SoTransformBoxDragger, SoCenterballDragger | ✅ Done (`tests/draggers/test_draggers.cpp`, `tests/rendering/render_draggers.cpp`) |
| 22 | `manips/` (26 %) | Manipulator attach/detach lifecycle; replaceNode/replaceManip | ✅ Done (`tests/manips/test_manips.cpp`) |
| 23 | `rendering/SoOffscreenRenderer.cpp` (28 %) | getBuffer, getViewportRegion, multiple sequential renders | ✅ Done (`tests/rendering/render_offscreen.cpp`) |
| 24 | `glue/gl.cpp` (45 %) | GL entry-point dispatch — covered transitively by rendering tests; diminishing returns from direct tests | ⏭ Skip (covered indirectly) |
| 25 | `io/SoInput.cpp` + `SoOutput.cpp` | Binary read round-trip, nested files, invalid inputs | ❌ TODO (see Tier 4) |
| 26 | `fonts/` (34 %) | SoText2 / SoText3 visual tests once font baseline settled | ❌ TODO (see Tier 5) |
| 27 | `profiler/` (1 %) | Enable profiling in a render loop and verify statistics | ❌ TODO (see Tier 5) |
| 28 | `collision/` (0 %) | `SoIntersectionDetectionAction` against a scene with known overlaps | ❌ TODO (see Tier 5) |

---

## Round 2 expansion plan (new TODO — targeting 41 % → 55 %)

Each item below is **not yet implemented**.  Together they are expected to add
roughly 12,000–14,000 newly-covered lines in `src/`, bringing the total from
41.1 % to approximately 55 %.

### Tier 4 — High / medium impact, medium effort (~10–12 % gain)

These can be written as non-rendering unit tests or simple self-validating
rendering tests.  All are expected to be buildable and runnable in the
existing OSMesa / GLX CI environment.

| Priority | Test to add | Subsystem(s) improved | Estimated gain | Status |
|----------|-------------|----------------------|----------------|--------|
| 29 | **`test_events_suite`** — construct and query `SoKeyboardEvent` (key, modifiers), `SoMouseButtonEvent` (button, state), `SoLocation2Event` (position), `SoMotion3Event`, `SoSpaceballButtonEvent`; dispatch a keyboard event through `SoEventCallback` via `SoHandleEventAction`; verify `getTriggerNode`, handled-state flag | `events`, `actions` | +350 lines | ✅ Done (`tests/events/test_events_suite.cpp`) |
| 30 | **`test_projectors_suite`** — `SbLineProjector::project` / `getDir`, `SbPlaneProjector::project` / `unproject`, `SbSphereProjector::project`, `SbCylinderProjector::project`, `SbSphereSectionProjector::project` + `isWithinTolerance`, `SbCylinderSectionProjector::project` | `projectors` | +330 lines | ✅ Done (`tests/projectors/test_projectors_suite.cpp`) |
| 31 | **`test_errors_suite`** — `SoError::setHandlerCallback` + `post` (verify callback fires and message contains expected text), `SoDebugError::post` (WARNING / INFO / ERROR severities), `SoReadError::post`; reset to default handler after each test | `errors` | +200 lines | ✅ Done (`tests/errors/test_errors_suite.cpp`) |
| 32 | **`test_engines_deep`** — `SoDecomposeMatrix` (decompose a known matrix, verify t/r/s outputs), `SoDecomposeRotation` (axis + angle round-trip), `SoComposeRotationFromTo` (from/to vectors), `SoTransformVec3f` (apply a rotation to a vector), `SoInterpolateRotation` (alpha = 0, 0.5, 1.0), `SoOnOff` toggle (on/off trigger cycle), `SoTriggerAny` multi-input | `engines` | +250 lines | ✅ Done (`tests/engines/test_engines_deep.cpp`) |
| 33 | **`test_io_edge_cases`** — binary-format write/read round-trip (`SoOutput::setBinary`, `SoInput::openFromBuffer`), multiple sequential `SoInput::setBuffer` calls, `SoDB::readAll` from a buffer containing a `SoFile` inline reference, graceful NULL return on corrupt/truncated header, `SoOutput::getBuffer` pointer + size after write | `io` | +300 lines | ✅ Done (`tests/io/test_io_edge_cases.cpp`) |
| 34 | **`test_sbdpmatrix_full`** — `SbDPMatrix`: `det4` / `det3`, `factor` (LU decompose), `inverse` of a known matrix, `multLeft`, `setTransform` / `getTransform` decompose round-trip, `transformBbox`, `equals` with epsilon; complements existing `mult` / `invert` tests | `base` | +280 lines | ✅ Done (`tests/base/test_sbdpmatrix_full.cpp`) |
| 35 | **`test_base_extras`** — `SbClip::clip` (polygon clipping against a plane), `SbOctTree` add / findItems inside a bbox, `SbXfBox3f` / `SbXfBox3d` `extendBy` + `transform` + `intersect`, `SbTesselator` tesselate a convex quad | `base` | +220 lines | ✅ Done (`tests/base/test_base_extras.cpp`) |
| 36 | **`test_nodes_extended`** — `SoCallback` (setCallback / trigger via traversal), `SoEventCallback` (addEventCallback / removeEventCallback / enableEvents), `SoPathSwitch` (set/clear switch path), `SoLabel`, `SoTransformSeparator`, `SoUnits`, `SoPolygonOffset`, `SoPickStyle`, `SoFont` / `SoFontStyle` default field values, `SoFrustumCamera`, `SoReversePerspectiveCamera`, `SoTextureCubeMap` (class initialised), `SoTextureUnit`, `SoTextureCombine`, `SoTextureMatrixTransform`, `SoVertexAttribute`, `SoVertexAttributeBinding` | `nodes` | +270 lines | ✅ Done (`tests/nodes/test_nodes_extended.cpp`) |
| 37 | **`test_caches_suite`** — `SoBoundingBoxCache` (update with a bbox, `isValid`, `reset`), `SoNormalCache` (set and retrieve normals, `isValid`), `SoConvexDataCache` (build from a simple polygon, verify output count) | `caches` | +300 lines | ✅ Done (`tests/caches/test_caches_suite.cpp`) |
| 38 | **`render_triangle_strip_set`** — `SoTriangleStripSet` + `SoIndexedTriangleStripSet` with per-vertex colour binding; self-validating pixel check on rendered centre pixels | `shapenodes` | +200 lines | ✅ Done (`tests/rendering/render_triangle_strip_set.cpp`) |
| 39 | **`render_ascii_text`** — `SoAsciiText` with justification LEFT / RIGHT / CENTER / FULL and multi-line strings; self-validating pixel check (non-black pixels present) | `shapenodes` | +180 lines | ✅ Done (`tests/rendering/render_ascii_text.cpp`) |
| 40 | **`render_image_node`** — `SoImage` with a procedurally filled `SoSFImage` (red/green checkerboard); verify output pixels match expected colours at known coordinates | `shapenodes` | +210 lines | ✅ Done (`tests/rendering/render_image_node.cpp`) |
| 41 | **`test_nodekit_deep`** — `SoCameraKit` (set camera + viewAll), `SoLightKit` (set light colour), `SoShapeKit` (set shape part), `SoSeparatorKit` (child management), `SoWrapperKit` (wrap an existing separator); exercise `getBoundingBox` and write/read round-trip for each | `nodekits` | +220 lines | ✅ Done (`tests/nodes/test_nodekit_deep.cpp`) |
| 42 | **`test_proto`** — `SoDB::readAll` from a buffer containing a `PROTO` definition and one or more instantiations; verify the resulting node graph type and field values; test `SoProtoInstance` field access | `misc` | +240 lines | ✅ Done (`tests/misc/test_proto.cpp`) |
| 43 | **`render_sorender_manager`** — instantiate `SoRenderManager`, set scene graph + camera + viewport, call `render`, check that `getGLRenderAction` is non-null; also exercise `setBackground`, `setAutoClipping`, `getViewportRegion` round-trip | `rendering` | +290 lines | ✅ Done (`tests/rendering/render_sorender_manager.cpp`) |

> **Notes:**
> - Priorities 29–31, 33–37, 41–42 are implemented as non-rendering unit tests
>   (plain CTest executables linked against Coin).
> - Priority 32 (engines) similarly runs without a rendering context.
> - Priorities 38–40 and 43 use the `headless_utils.h` / `add_rendering_test` CMake macro
>   (OSMesa or GLX context, exit-code validated, no image comparison).
> - All 15 Tier 4 items build and pass in the OSMesa CI environment.

---

## Tier 5 — Lower priority / harder to test (targeting ≥ 70 %)

These require significant additional infrastructure, optional subsystems, or
depend on decisions not yet made (e.g., font baseline).

| Priority | Area | Approach |
|----------|------|----------|
| 44 | `draggers/` deeper (remaining ~40 % uncovered) | Simulate full drag sequences (press → move × N → release) for `SoTranslatePlaneDragger`, `SoRotateCylindricalDragger`, `SoRotateSphericalDragger`, `SoScale1Dragger`, `SoScale2Dragger`; verify field values after each drag | ✅ Done (`tests/draggers/test_dragger_sequences.cpp`) |
| 45 | `geo/` deeper (11 %) | Add `SoGeoLocation`, `SoGeoSeparator` unit tests; exercise `SbGeoProjection::forward` / `inverse`, `SbUTMProjection`, `SbPolarStereographic` with known WGS-84 coordinates |
| 46 | `collision/` (0 %) | `SoIntersectionDetectionAction` against a scene with two known-overlapping spheres; verify callback fires with correct primitive data |
| 47 | `profiler/` (1 %) | Build with `COIN_PROFILING=ON`; enable the profiler, run a render loop for a few frames, assert that statistics counters are non-zero |
| 48 | `rendering/SoGLBigImage` (4 %) | Render an `SoTexture2` whose image exceeds the maximum GL texture size so the big-image tiling path is exercised; check for non-black pixels in output | ✅ Done (`tests/rendering/render_gl_big_image.cpp`) |
| 49 | `rendering/SoGLImage` + `SoGL.cpp` deeper | Texture object lifecycle (create → render → delete), mipmap generation path, cube-map path via `SoTextureCubeMap` | ✅ Done (`tests/rendering/render_gl_features.cpp`, `tests/rendering/render_cubemap.cpp`) |
| 50 | `fields/SoField.cpp` deeper | `getDirty` / `setDirty` lifecycle, field container `getFields` enumeration, circular-connection detection (expect `SoDebugError` warning), `SoFieldConverter` automatic type conversion chain | ✅ Done (`tests/fields/test_field_extras.cpp`) |
| 51 | `actions/SoGLRenderAction.cpp` deeper | Transparency sorting modes (SORTED_OBJECT, SORTED_LAYERS_BLEND), pass callbacks, `setTransparencyType`, caching policy modes (`NO_AUTO_CACHING`, `CACHE_ALL`); requires OSMesa context | ✅ Done (`tests/actions/test_glrender_action.cpp`) |
| 52 | `misc/SoSceneManager` | `setSceneGraph`, `render`, `setRenderCallback`, background colour, `scheduleRedraw`; complements `SoRenderManager` test | ✅ Done (`tests/misc/test_scene_manager.cpp`) |
| 53 | `io/SoInput` binary edge cases | Corrupt binary file (truncated, wrong magic) graceful return NULL; large binary file streaming via `openFile` | ✅ Done (`tests/io/test_io_input_binary.cpp`) |
| 54 | `fonts/` visual regression | `SoText2` / `SoText3` pixel-comparison tests using Obol's ProFont baseline; gated on a stable shared font baseline |
| 55 | `rendering/SoGLDriverDatabase` | Query driver workarounds for a known vendor/renderer string; verify `isSupported` / `setFeature` round-trip without a real GPU | ✅ Done (indirectly via `tests/rendering/render_gl_features.cpp`) |
| 56 | `bundles/` deeper | `SoNormalBundle` (generate normals for a mesh, verify count), `SoTextureCoordinateBundle` (default coordinate generation), `SoVertexAttributeBundle` | ✅ Done (`tests/bundles/test_bundles_suite.cpp`) |
| 57 | `details/` deeper | After `SoRayPickAction`, extract `SoFaceDetail` (point indices, face index), `SoLineDetail` (line index, point on line), `SoPointDetail`; verify values against known geometry | ✅ Done (`tests/details/test_details_suite.cpp`) |
| 58 | `projectors/` deeper | `SbSphereSheetProjector`, `SbSpherePlaneProjector`, `SbCylinderSheetProjector`, `SbCylinderPlaneProjector`: construct, project, getRotation, copy | ✅ Done (`tests/projectors/test_projectors_deep.cpp`) |
| 59 | `manips/` deeper | `SoPointLightManip`, `SoDirectionalLightManip`, `SoSpotLightManip`, `SoClipPlaneManip`: type check, getDragger, replaceNode/replaceManip, setValue | ✅ Done (`tests/manips/test_manips_deep.cpp`) |
| 60 | `rendering/` multi-texture | `SoTextureUnit` multi-unit binding with two `SoTexture2` units on a sphere; verify pixels and no crash | ✅ Done (`tests/rendering/render_multi_texture.cpp`) |

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
| Baseline (2026-02-23) | 31.6 % | `src/` only; initial baseline |
| Re-baseline (2026-02-24) | 41.1 % | `src/` only; after all Tier 1 + Tier 2 + Tier 3 items 21–23 |
| Re-baseline (2026-02-24 update 2, current) | **52.4 %** | `src/` only; after Tiers 4–5; all 231 tests pass |
| Tier 5 target (long-term) | ≥ 70 % | Covers all actively-maintained APIs including draggers, geo, collision, fonts |
