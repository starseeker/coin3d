# Obol Code Coverage Report

Generated from an lcov coverage build (`OBOL_COVERAGE=ON`, `OBOL_USE_OSMESA=ON`,
`CMAKE_BUILD_TYPE=Debug`) with the unit test suite (191 unit tests), 77+ visual renders
via OSMesa, and most CTest rendering regression tests.

**Toolchain:** GCC + lcov 2.0, OSMesa headless rendering

---

## Overall Results (Session 5 – Final)

| Metric    | Covered | Total  | Percentage |
|-----------|---------|--------|------------|
| Lines     | 41,961  | 85,652 | **49.0%**  |
| Functions | 7,778   | 15,937 | **48.8%**  |
| Branches  | —       | —      | no data    |

*(Session 4 final: 46.9% lines / 47.8% functions at 166 tests.
Session 5 added 25 new unit tests (166→191 total, 3 iterations) specifically targeting
complex GL state rendering and dragger/manipulator interaction simulation via OSMesa:

**GL-specific rendering paths (iter 19–21):**
All SoGLRenderAction transparency types (BLEND, DELAYED_BLEND, SCREEN_DOOR, SORTED_OBJECT_BLEND,
DELAYED_ADD, SORTED_OBJECT_ADD); material state changes (multi-material, BASE_COLOR light model,
emissive, LINES/POINTS draw styles); SoShapeHints vertex ordering variations (SOLID+CCW, CW,
UNKNOWN_ORDERING two-sided lighting); GL caching (3× render cache fill+hit, material
invalidation, multi-pass rendering); shape material/normal bindings (PER_FACE IFS, PER_VERTEX
VertexProperty, OVERALL FaceSet, PackedColor IFS, PER_FACE_INDEXED IFS, PER_VERTEX_INDEXED
normals, textured IFS/FaceSet); multi-light scenes (DirectionalLight+PointLight+SpotLight);
SoClipPlane on/off; SoSceneTexture2 render-to-texture; GL smoothing; RGBA/LUMINANCE_TRANSPARENCY
components; SoLineSet PER_LINE/PER_VERTEX/VertexProperty bindings.

**Dragger/manipulator interaction simulation (iter 19):**
All major draggers with simulated press+drag+release events via SoHandleEventAction:
SoTranslate1, SoRotateSpherical, SoTrackball, SoHandleBox, SoTransformer, SoCenterball,
SoTabBox, SoTransformBox, SoRotateDisc, SoScale2, SoScale2Uniform, SoRotateCylindrical,
SoScale1, SoDragPoint.
All major manips with replaceNode+drag+replaceManip lifecycle via GL context:
SoTrackball, SoHandleBox, SoTabBox, SoTransformer, SoCenterball, SoTransformBox.

**Other additions (iter 20–21):**
SbTesselator polygon tessellation (triangle/quad/pentagon/concave L-shape);
SoText2/SoText3 GL render (all justification modes); SoAsciiText GL render;
SoExtSelection RECTANGLE/LASSO/NOLASSO lasso types; SoRayPickAction setRay/intersects;
keyboard+mouse button events via SoHandleEventAction; SoInput buffer/file/binary read;
SoSearchAction ALL/LAST/by-name/by-node.)*

Session 5 also added 10+ new visual render scenes and ran all CTest rendering
regression tests to boost GL code coverage.

---
SbMatrix deeper (multLeft/multFullMatrix/LU decomp/5-arg setTransform),
SoCamera deeper (viewAll-with-path, SoOrthographic), SoCallbackAction triangle counting
all shape types, SoShape bounding boxes (SoAsciiText/SoText3), Scene IO deeper
(SoWriteAction file/binary, SoDB::readAll), SoSeparator caching behaviors,
SoMaterial multi-array, transform hierarchy+SoGetMatrixAction compound, field engine
connections (SoInterpolateFloat/SoGate/SoSelectOne), SbDPViewVolume comprehensive,
SoRayPickAction with scenes (sphere/cube/cylinder/cone/IFS), SoInput/SoOutput deeper,
SoCallbackAction pre/post/PRUNE, SoGetBoundingBoxAction resetPath/getXfBBox,
material/normal binding variants (PER_FACE/PER_VERTEX_INDEXED), SoComplexity,
SoField setIgnored/enableNotify/getConnectedField/Engine, SoAction state during traversal,
SoNode type system (isOfType/SoType::fromName/isDerivedFrom), SoDB static APIs,
SoBase ref/getRefCount/getName, SoSeparator deep2, SoPath deeper, SoGroup deeper,
complex scene IO, SoShapeHints, SoLight fields, SbColor HSV, SbBox3f, SbPlane,
SbCylinder intersect, SoRayPickAction setPoint/setNormalizedPoint/setPickAll,
SbLine, SbXfBox3f.)*

Session 4 also rendered 67 visual scenes (vs 25 in session 3) to boost GL code coverage.

---

## Per-Subsystem Coverage (Updated)

### Well-Covered Subsystems (≥ 60%)

| Subsystem | Coverage |
|-----------|---------|
| `src/details` | ~61–82% (SoConeDetail 82%, SoCubeDetail 75%) |
| `src/elements` (non-GL) | ~85% overall |
| `src/errors` | ~83% |
| `src/manips` | ~86% average |
| `src/actions/SoGetPrimitiveCountAction` | 81% |
| `src/actions/SoGetMatrixAction` | 70% |
| `src/actions/SoWriteAction` | 59% |
| `src/actions/SoCallbackAction` | 52% |
| `src/misc/SoSceneManager` | **57%** (was 0%) |
| `src/base/SbColor4f` | 64% |
| `src/base/SbBox3d` | 93% |
| `src/base/SbBox2f` | 56% |
| `src/base/SbDPMatrix` | 53% |
| `src/base/SbDPPlane` | 60% |
| `src/rendering/SoRenderManager` | 51% |
| `src/nodes/SoCallback` | 96% |

---

### Moderate Coverage (30–60%)

| File | Coverage | Notes |
|------|----------|-------|
| `src/actions/SoSearchAction.cpp` | 32% | ALL/FIRST/LAST fully tested |
| `src/actions/SoHandleEventAction.cpp` | 32% | Event routing tested |
| `src/actions/SoGetBoundingBoxAction.cpp` | 33% | resetPath, XfBBox tested |
| `src/base/SbTime.cpp` | 38% | |
| `src/base/SbViewVolume.cpp` | 25% | |
| `src/rendering/SoGL.cpp` | 49% | |
| `src/io/SoInput.cpp` | 31% | |
| `src/io/SoOutput.cpp` | 35% | |
| `src/actions/SoCallbackAction.cpp` | 52% | pre/post/PRUNE tested |

---

### Low Coverage (< 30%)

#### Math & Geometry Primitives

| File | Coverage | Notes |
|------|----------|-------|
| `src/base/SbMatrix.cpp` | **13%** | 550 lines; LU/getTransform added; many internal paths |
| `src/base/SbRotation.cpp` | **15%** | 196 lines; slerp/multVec/scaleAngle added |
| `src/base/SbDPViewVolume.cpp` | **10%** | 438 lines; perspective/ortho/projectPointToLine/narrow tested |
| `src/base/SbCylinder.cpp` | **22%** | intersect hit/miss/enter-exit tested |
| `src/base/SbPlane.cpp` | **18%** | isInHalfSpace/getDistance/intersect tested |
| `src/base/SbBox3f.cpp` | **22%** | extendBy/intersect/getClosestPoint/getSize/transform |
| `src/base/SbXfBox3f.cpp` | **23%** | project/setTransform/extendBy/intersect |
| `src/base/SbColor.cpp` | **18%** | setHSVValue/getHSVValue/setPackedValue tested |
| `src/base/SbLine.cpp` | — | getDirection/getClosestPoint/getClosestPoints tested |

#### Actions

| File | Coverage | Notes |
|------|----------|-------|
| `src/actions/SoRayPickAction.cpp` | **12%** | setPoint/setNormalizedPoint/setPickAll/getPickedPointList tested |
| `src/actions/SoGLRenderAction.cpp` | **21%** | Visual renders exercise many paths |
| `src/actions/SoAction.cpp` | **17%** | getWhatAppliedTo/getState/hasTerminated tested |

#### Shape Nodes

| File | Coverage | Notes |
|------|----------|-------|
| `src/shapenodes/SoShape.cpp` | **12%** | BBox/triangle generation exercised via callbacks |
| `src/shapenodes/SoFaceSet.cpp` | **15%** | PER_FACE/OVERALL/PER_VERTEX bindings tested |

#### Other

| File | Coverage | Notes |
|------|----------|-------|
| `src/nodes/SoSeparator.cpp` | **16%** | renderCaching/boundingBoxCaching/pickCulling tested |
| `src/fields/SoField.cpp` | **16%** | setIgnored/enableNotify/isConnected/getConnected tested |
| `src/nodes/SoCamera.cpp` | **15%** | viewAll/getViewVolume/SoOrtho tested |
| `src/nodes/SoMaterial.cpp` | — | multi-material arrays/transparency tested |
| `src/misc/SoBase.cpp` | **19%** | ref/getRefCount/getName/setName tested |

---

### Still Very Low (< 15%)

The following subsystems remain at low coverage because they require:
- Complex GL rendering state (SoGLCacheList: 6%, SoGLImage: 13%)
- Specific file formats or large data sets  
- Interactive dragger/manipulator state machines

| File | Coverage |
|------|----------|
| `src/caches/SoGLCacheList.cpp` | 6% |
| `src/base/SbDPViewVolume.cpp` | 10% |
| `src/shapenodes/SoShape.cpp` | 12% |
| `src/base/SbMatrix.cpp` | 13% |
| `src/rendering/SoGLImage.cpp` | 13% |
| `src/shapenodes/SoProceduralShape.cpp` | 14% |
| `src/nodes/SoCamera.cpp` | 15% |
| `src/shapenodes/SoFaceSet.cpp` | 14% → 15% (PER_FACE/PER_VERTEX/texture added) |

---

## TODO / Improvement Opportunities

### Tier 1 — High Impact (next sprint)

1. **`SoShape` base class** — 12% coverage means the entire shape rendering
   infrastructure is poorly tested.
   - Missing: complete `shouldGLRender()` path, `beginSolidShape()`, pick traversal.
   - Suggested: GL render tests that exercise BOUNDING_BOX complexity type.

2. **`SoRayPickAction`** — 11% of 400 lines; pick traversal has many code paths.
   - Session 5 added setRay/setNormalizedPoint/setRadius; many internal paths still untested.
   - Suggested: `tests/actions/test_raypick_complete.cpp`

3. **`SoText2` / `SoText3` / `SoAsciiText`** — text rendering 8–12%.
   - Session 5 added LEFT/CENTER/RIGHT GL render tests; internal glyph paths may need
     a real font file to exercise fully.
   - Suggested: add visual regression tests in `tests/rendering/` for each.

4. **`SbDPViewVolume`** — 10% of 438 lines despite comprehensive API tests.
   - The remaining 90% is in internal projection math used by double-precision cameras.
   - Suggested: Use `SbDPViewVolume::getMatrices()` and transform methods.

### Tier 2 — Important (next sprint)

5. **`SoGLCacheList`** — 6% coverage; GL object cache management.
   - Session 5 tested 3× render with cache ON; need more diverse cached scenes.
   - Suggested: render same complex scene 5+ times with state changes.

6. **`SoGLImage` / `SoGLBigImage`** — 12–13%.
   - Texture lifecycle (upload, update, delete) is barely tested.
   - Suggested: tests that change texture data after first render.

7. **`SoCamera`** — 15% of 197 lines despite viewAll/getViewVolume tests.
   - Missing: `pointAt()` with up vector, `project()`, `unproject()`.
   - Suggested: `tests/nodes/test_camera.cpp`

8. **`SbMatrix`** — 13% of 550 lines despite LU decomp tests.
   - The remaining code is in internal multiplication and decomposition paths.
   - Suggested: test `SbMatrix::getTransform()` 4-arg variant, `setTransform()` with center.

### Tier 3 — Worthwhile (as time allows)

9. **Projectors** (SbCylinderSheetProjector 0%, SbSphereSheetProjector 0%) — used by draggers.
10. **`SbTesselator`** — 8%; polygon tesselation used by SoIndexedFaceSet with concave polygons.
    Session 5 added triangle/quad/pentagon/concave L-shape tests.
11. **`SoSensorManager`** — 17%; sensor queue flushing not tested.
12. **Shader subsystem** — `SoShaderObject` 20%, `SoGLSLShaderObject` 18%.

### Tier 4 — Deferred (profiler / HUD / shadows)

13. **Profiler** — 8 files at 0%. Consider whether profiling is a supported feature.
14. **HUD** — 23–31%; the overlay HUD is tested visually.
15. **Shadows** — shadow nodes at 0% unit-test; visual regression test exists.

---

## Cleanup Done Before This Run

Before the lcov run (session 3), the following fully-stubbed / never-implemented APIs were
removed from the build to eliminate false "uncovered" noise:

| Removed | Reason |
|---------|--------|
| `SoByteStream` (src + header) | Every method was `OBOL_STUB()` – never implemented |
| `SoTranSender` (src + header) | Every method was `OBOL_STUB()` – never implemented |
| `SoTranReceiver` (src + header) | Every method was `OBOL_STUB()` – never implemented |
| `SoTranscribe.h` | Header that only forwarded to the two removed classes |
| `SoNormalGenerator::setNumNormals()` | Called `OBOL_OBSOLETED()` only |
| `SoNormalGenerator::setNormal()` | Called `OBOL_OBSOLETED()` only |

---

## How to Reproduce

```bash
# Install lcov (if not present)
sudo apt-get install lcov

# Configure
cmake /path/to/obol \
  -DOBOL_USE_OSMESA=ON \
  -DOBOL_BUILD_TESTS=ON \
  -DOBOL_COVERAGE=ON \
  -DCMAKE_BUILD_TYPE=Debug \
  -B build_coverage

# Build and run coverage
cmake --build build_coverage -- -j4
cmake --build build_coverage --target coverage

# Open report
xdg-open build_coverage/coverage_report/index.html
```

