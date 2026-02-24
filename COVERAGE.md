# Coin API Test Coverage

This file tracks which Coin APIs have tests and which still need coverage.
Tests in `tests/` subdirectories are baselined against the
`upstream` reference implementation (`COIN_TEST_SUITE` blocks).

---

## Legend

| Symbol | Meaning |
|--------|---------|
| ✅ | Tests written and baselined against upstream |
| 🔶 | Tests written but no vanilla baseline (API behavior tested) |
| ❌ | No tests yet |

---

## Base Types (`tests/base/`)

| Class | Tests | Vanilla Baseline | Notes |
|-------|-------|-----------------|-------|
| `SbVec3f` | ✅ | `src/base/SbVec3f.cpp` | toString, fromString, fromString invalid |
| `SbVec2f` | ✅ | (via SbBox2f) | Covered through box tests |
| `SbVec3d` | ✅ | `src/base/SbVec3d.cpp` | fromString |
| `SbVec3s` | ✅ | `src/base/SbVec3s.cpp` | fromString, fromInvalidString |
| `SbVec3us` | ✅ | `src/base/SbVec3us.cpp` | construction, getValue, setValue round-trip |
| `SbVec4f` | ✅ | `src/base/SbVec4f.cpp` | normalize already-normalized |
| `SbBox2f` | ✅ | `src/base/SbBox2f.cpp` | getSize, getClosestPoint (outside, center) |
| `SbBox2d` | ✅ | `src/base/SbBox2d.cpp` | getSize, getClosestPoint |
| `SbBox2i32` | ✅ | `src/base/SbBox2i32.cpp` | getSize |
| `SbBox2s` | ✅ | `src/base/SbBox2s.cpp` | getSize |
| `SbBox3f` | ✅ | `src/base/SbBox3f.cpp` | getClosestPoint (outside, center) |
| `SbBox3d` | ✅ | `src/base/SbBox3d.cpp` | getClosestPoint |
| `SbBox3i32` | ✅ | `src/base/SbBox3i32.cpp` | getSize, getClosestPoint |
| `SbBox3s` | ✅ | `src/base/SbBox3s.cpp` | getSize, getClosestPoint |
| `SbByteBuffer` | ✅ | `src/base/SbByteBuffer.cpp` | pushUnique, pushOnEmpty |
| `SbBSPTree` | ✅ | `src/base/SbBSPTree.cpp` | add/find/remove points |
| `SbMatrix` | ✅ | `src/base/SbMatrix.cpp` | construct from SbDPMatrix |
| `SbDPMatrix` | ✅ | `src/base/SbDPMatrix.cpp` | construct from SbMatrix |
| `SbDPRotation` | ✅ | `src/base/SbDPRotation.cpp` | construct from axis/angle |
| `SbDPPlane` | ✅ | `src/base/SbDPPlane.cpp` | plane-plane intersection sign correct |
| `SbRotation` | ✅ | `src/base/SbRotation.cpp` | fromString valid/invalid |
| `SbString` | ✅ | `src/base/SbString.cpp` | operator+ (all three forms) |
| `SbPlane` | ✅ | `src/base/SbPlane.cpp` | plane-plane intersection |
| `SbViewVolume` | ✅ | `src/base/SbViewVolume.cpp` | ortho/perspective intersection |
| `SbImage` | ✅ | `src/base/SbImage.cpp` | copyConstruct |
| `SbColor` | 🔶 | (in test_base.cpp) | HSV conversion |
| `SbColor4f` | ✅ | — | construction, set/get round-trip |
| `SbLine` | ✅ | — | getClosestPoint, getClosestPoints (parallel lines) |
| `SbSphere` | ✅ | — | pointInside, getRadius/setRadius, setCenter |
| `SbCylinder` | ✅ | — | construction, getRadius |
| `SbHeap` | ✅ | `src/base/heap.cpp` | min_heap, max_heap, heap_add, heap_remove, heap_update |

---

## Fields (`tests/fields/`)

### Single-Value Fields (SoSF*)

| Class | Tests | Vanilla Baseline | Notes |
|-------|-------|-----------------|-------|
| `SoSFBool` | ✅ | `src/fields/SoSFBool.cpp` | initialized, textinput (TRUE/FALSE/0/1/invalid) |
| `SoSFFloat` | ✅ | `src/fields/SoSFFloat.cpp` | initialized, set/get round-trip |
| `SoSFDouble` | ✅ | `src/fields/SoSFDouble.cpp` | initialized |
| `SoSFInt32` | ✅ | `src/fields/SoSFInt32.cpp` | initialized, set/get round-trip |
| `SoSFShort` | ✅ | `src/fields/SoSFShort.cpp` | initialized |
| `SoSFUInt32` | ✅ | `src/fields/SoSFUInt32.cpp` | initialized |
| `SoSFUShort` | ✅ | `src/fields/SoSFUShort.cpp` | initialized |
| `SoSFVec2f` | ✅ | `src/fields/SoSFVec2f.cpp` | initialized |
| `SoSFVec3f` | ✅ | `src/fields/SoSFVec3f.cpp` | initialized, set/get round-trip |
| `SoSFVec4f` | ✅ | `src/fields/SoSFVec4f.cpp` | initialized |
| `SoSFColor` | ✅ | `src/fields/SoSFColor.cpp` | initialized, set/get round-trip |
| `SoSFColorRGBA` | ✅ | `src/fields/SoSFColorRGBA.cpp` | initialized |
| `SoSFString` | ✅ | `src/fields/SoSFString.cpp` | initialized, set/get round-trip |
| `SoSFRotation` | ✅ | `src/fields/SoSFRotation.cpp` | initialized |
| `SoSFMatrix` | ✅ | `src/fields/SoSFMatrix.cpp` | initialized |
| `SoSFName` | ✅ | `src/fields/SoSFName.cpp` | initialized |
| `SoSFTime` | ✅ | `src/fields/SoSFTime.cpp` | initialized |
| `SoSFEnum` | ✅ | `src/fields/SoSFEnum.cpp` | initialized |
| `SoSFBitMask` | ✅ | `src/fields/SoSFBitMask.cpp` | initialized |
| `SoSFImage` | ✅ | `src/fields/SoSFImage.cpp` | initialized |
| `SoSFImage3` | ✅ | `src/fields/SoSFImage3.cpp` | initialized |
| `SoSFPlane` | ✅ | `src/fields/SoSFPlane.cpp` | initialized |
| `SoSFNode` | ✅ | `src/fields/SoSFNode.cpp` | initialized |
| `SoSFPath` | ✅ | `src/fields/SoSFPath.cpp` | initialized |
| `SoSFEngine` | ✅ | `src/fields/SoSFEngine.cpp` | initialized |
| `SoSFTrigger` | ✅ | `src/fields/SoSFTrigger.cpp` | initialized |
| `SoSFBox2d/2f/2i32/2s` | ✅ | box SF fields | initialized |
| `SoSFBox3d/3f/3i32/3s` | ✅ | box SF fields | initialized |
| `SoSFVec2d/i32/s` | ✅ | vec SF fields | initialized |
| `SoSFVec3d/i32/s` | ✅ | vec SF fields | initialized |
| `SoSFVec4d/i32/s` | ✅ | vec SF fields | initialized |
| `SoSFVec2b/3b/4b/4ub/4ui32/4us` | ✅ | `src/fields/SoSFVec2b.cpp` et al. | initialized; SoSFVec4ub also set/get |

### Multi-Value Fields (SoMF*)

| Class | Tests | Vanilla Baseline | Notes |
|-------|-------|-----------------|-------|
| `SoMFFloat` | ✅ | `src/fields/SoMFFloat.cpp` | initialized (getNum==0), set/get |
| `SoMFDouble` | ✅ | `src/fields/SoMFDouble.cpp` | initialized |
| `SoMFInt32` | ✅ | `src/fields/SoMFInt32.cpp` | initialized, deleteValues |
| `SoMFShort` | ✅ | `src/fields/SoMFShort.cpp` | initialized |
| `SoMFUInt32` | ✅ | `src/fields/SoMFUInt32.cpp` | initialized |
| `SoMFUShort` | ✅ | `src/fields/SoMFUShort.cpp` | initialized |
| `SoMFVec2f` | ✅ | `src/fields/SoMFVec2f.cpp` | initialized |
| `SoMFVec3f` | ✅ | `src/fields/SoMFVec3f.cpp` | initialized, set/get |
| `SoMFVec4f` | ✅ | `src/fields/SoMFVec4f.cpp` | initialized |
| `SoMFColor` | ✅ | `src/fields/SoMFColor.cpp` | initialized, set/get |
| `SoMFString` | ✅ | `src/fields/SoMFString.cpp` | initialized, set/get |
| `SoMFRotation` | ✅ | `src/fields/SoMFRotation.cpp` | initialized |
| `SoMFBool` | ✅ | `src/fields/SoMFBool.cpp` | initialized |
| `SoMFMatrix` | ✅ | `src/fields/SoMFMatrix.cpp` | initialized |
| `SoMFName` | ✅ | `src/fields/SoMFName.cpp` | initialized |
| `SoMFTime` | ✅ | `src/fields/SoMFTime.cpp` | initialized |
| `SoMFPlane` | ✅ | `src/fields/SoMFPlane.cpp` | initialized |
| `SoMFColorRGBA` | ✅ | `src/fields/SoMFColorRGBA.cpp` | initialized |
| `SoMFEnum` | ✅ | `src/fields/SoMFEnum.cpp` | initialized |
| `SoMFBitMask` | ✅ | `src/fields/SoMFBitMask.cpp` | initialized |
| `SoMFNode` | ✅ | `src/fields/SoMFNode.cpp` | initialized |
| `SoMFPath` | ✅ | `src/fields/SoMFPath.cpp` | initialized |
| `SoMFEngine` | ✅ | `src/fields/SoMFEngine.cpp` | initialized |
| `SoMFVec2d/i32/s` | ✅ | vec MF fields | initialized |
| `SoMFVec3d/i32/s` | ✅ | vec MF fields | initialized |
| `SoMFVec4d/i32/s` | ✅ | vec MF fields | initialized |
| `SoMFVec2b/3b/4b/4ub/4ui32/4us` | ✅ | `src/fields/SoMFVec2b.cpp` et al. | initialized |

---

## Actions (`tests/actions/`)

| Class | Tests | Vanilla Baseline | Notes |
|-------|-------|-----------------|-------|
| `SoCallbackAction` | ✅ | `src/actions/SoCallbackAction.cpp` | callbackAll on/off, switch traversal |
| `SoWriteAction` | ✅ | `src/actions/SoWriteAction.cpp` | DEF/USE naming for multi-ref nodes |
| `SoSearchAction` | ✅ | — | find by name, find by type |
| `SoGetBoundingBoxAction` | ✅ | — | unit cube bounds |
| `SoGLRenderAction` | ✅ | — | visual regression tests in `tests/rendering/`; 7 scenes covering primitives, materials, lighting, transforms, cameras, draw styles, texture mapping |
| `SoGetMatrixAction` | ✅ | — | class initialized, identity for empty scene |
| `SoHandleEventAction` | ✅ | — | class initialized, dispatch on empty scene (not handled), no crash |
| `SoPickAction` | ❌ | — | base class; tested via SoRayPickAction |
| `SoRayPickAction` | ✅ | — | class initialized, no picks on empty scene, picks cube at origin, pick point on cube surface + path |
| `SoGetPrimitiveCountAction` | ✅ | — | class initialized, count 0 for empty scene |
| `SoReorganizeAction` | ✅ | — | class initialized (type registered) |
| `SoAudioRenderAction` | ❌ | — | removed from Obol |

---

## Nodes (`tests/nodes/`)

| Class | Tests | Vanilla Baseline | Notes |
|-------|-------|-----------------|-------|
| `SoAnnotation` | ✅ | `src/nodes/SoAnnotation.cpp` | initialized (typeId, ref/unref) |
| `SoType` | ✅ | `src/misc/SoType.cpp` | createType, removeType |
| `SoNode` (base) | ✅ | — | isOfType, setName/getName, getByName |
| `SoSeparator` | ✅ | — | addChild, removeChild, insertChild, getNumChildren |
| `SoGroup` | ✅ | — | isOfType hierarchy |
| `SoCube` | ✅ | — | default field values (2x2x2) |
| `SoSphere` | ✅ | — | default radius (1.0) |
| `SoCone` | ✅ | — | default fields |
| `SoCylinder` | ✅ | — | default radius (1.0) and height (2.0) |
| `SoFaceSet` | ✅ | — | class initialized |
| `SoIndexedFaceSet` | ✅ | — | class initialized |
| `SoTriangleStripSet` | ✅ | — | class initialized |
| `SoIndexedTriangleStripSet` | ✅ | — | class initialized |
| `SoLineSet` | ✅ | — | class initialized |
| `SoIndexedLineSet` | ✅ | — | class initialized |
| `SoPointSet` | ✅ | — | class initialized |
| `SoMaterial` | ✅ | — | default diffuseColor count |
| `SoMaterialBinding` | ✅ | — | class initialized |
| `SoNormalBinding` | ✅ | — | class initialized |
| `SoVertexProperty` | ✅ | — | class initialized |
| `SoDirectionalLight` | ✅ | — | class initialized |
| `SoPointLight` | ✅ | — | class initialized |
| `SoSpotLight` | ✅ | — | class initialized |
| `SoLightModel` | ✅ | — | class initialized |
| `SoTranslation` | ✅ | — | default translation (0,0,0) |
| `SoRotation` | ✅ | — | default rotation (identity) |
| `SoRotationXYZ` | ✅ | — | class initialized |
| `SoScale` | ✅ | — | default scaleFactor (1,1,1) |
| `SoTransform` | ✅ | — | default translation (0,0,0) |
| `SoMatrixTransform` | ✅ | — | class initialized |
| `SoCamera` (base) | 🔶 | — | default fields tested via SoPerspectiveCamera/SoOrthographicCamera |
| `SoPerspectiveCamera` | ✅ | — | class initialized, nearDistance/farDistance defaults |
| `SoOrthographicCamera` | ✅ | — | class initialized, height default |
| `SoSwitch` | ✅ | — | default whichChild == SO_SWITCH_NONE |
| `SoBlinker` | ✅ | — | class initialized |
| `SoRotor` | ✅ | — | class initialized |
| `SoText2` / `SoText3` | ❌ | — | need rendering context for font metric |
| `SoCoordinate3` | ✅ | — | class initialized |
| `SoNormal` | ✅ | — | class initialized |
| `SoTextureCoordinate2` | ✅ | — | class initialized |
| `SoTextureCoordinateBinding` | ✅ | — | class initialized |
| `SoTexture2` | ✅ | — | class initialized |
| `SoDrawStyle` | ✅ | — | class initialized |
| `SoComplexity` | ✅ | — | class initialized |
| `SoEnvironment` | ✅ | — | class initialized |
| `SoClipPlane` | ✅ | — | class initialized |
| `SoFile` | ✅ | — | class initialized |
| `SoInfo` | ✅ | — | class initialized |
| `SoLOD` | ✅ | — | class initialized |
| Shader nodes | ✅ | `src/shaders/` | SoShaderProgram, SoFragmentShader, SoVertexShader, SoGeometryShader class initialized |
| Shader parameter nodes | ✅ | `src/shaders/SoShaderParameter.cpp` | 1f/1i/2f/2i/3f/3i/4f/4i/Array1f/Array1i/Matrix/MatrixArray/StateMatrix |
| Shadow nodes | ✅ | `src/shadows/` | SoShadowGroup, SoShadowStyle class initialized |
| Geo nodes | ✅ | `src/geo/` | SoGeoOrigin, SoGeoCoordinate class initialized |

---

## I/O and Database (`tests/io/`)

| Class / Feature | Tests | Vanilla Baseline | Notes |
|-----------------|-------|-----------------|-------|
| `SoDB` initialization | ✅ | `src/misc/SoDB.cpp` | realTime field check |
| `SoDB::readAll` (IV 2.1) | ✅ | `src/misc/SoDB.cpp` | valid scene, DEF/USE |
| `SoDB::readAll` (invalid) | ✅ | `src/misc/SoDB.cpp` | empty input returns NULL |
| Write/read round-trip | ✅ | `src/misc/SoDB.cpp` | structure + field values preserved |
| `SoDB::isValidHeader` | ✅ | — | |
| `SoDB::readAll` (VRML 2.0) | ❌ | `src/misc/SoDB.cpp` | readChildList (VRML) – removed in Obol |
| `SoBase` write/read | ✅ | `src/misc/SoBase.cpp` | unnamed multi-ref DEF/USE, same-named node disambiguation (+N) |
| Binary format I/O | ✅ | — | binary write produces `#Inventor V2.1 binary` header; binary output non-empty |

---

## Sensors (`tests/sensors/`)

*No vanilla COIN_TEST_SUITE baselines for sensors.*

| Class | Tests | Notes |
|-------|-------|-------|
| `SoFieldSensor` | 🔶 | fires on change, stops after detach |
| `SoNodeSensor` | 🔶 | fires on node change |
| `SoTimerSensor` | 🔶 | schedule/unschedule |
| `SoAlarmSensor` | 🔶 | schedule/unschedule |
| `SoOneShotSensor` | 🔶 | type check, schedule/unschedule |
| `SoIdleSensor` | 🔶 | schedule/unschedule |
| `SoPathSensor` | 🔶 | attach/detach |
| `SoDataSensor` | ✅ | — | class initialized (via SoFieldSensor); getTriggerField, getTriggerPathFlag, setTriggerPathFlag |

---

## Engines (`tests/engines/`)

*No vanilla COIN_TEST_SUITE baselines for engines.*

| Class | Tests | Notes |
|-------|-------|-------|
| `SoCalculator` | 🔶 | constant expression, input field expression |
| `SoComposeVec3f` | 🔶 | compose from three floats |
| `SoDecomposeVec3f` | 🔶 | decompose to three floats |
| `SoBoolOperation` | 🔶 | class initialized |
| `SoElapsedTime` | 🔶 | class initialized |
| `SoConcatenate` | 🔶 | class initialized |
| `SoComposeMatrix` | ✅ | — | class initialized |
| `SoComposeRotation` | ✅ | — | class initialized |
| `SoComposeVec2f` / `SoComposeVec4f` | ✅ | — | class initialized |
| `SoComputeBoundingBox` | ✅ | — | class initialized |
| `SoGate` | ✅ | — | class initialized |
| `SoInterpolate*` | ✅ | — | SoInterpolateFloat class initialized |
| `SoSelectOne` | ✅ | — | class initialized |
| `SoTimeCounter` | ✅ | — | class initialized |
| `SoCounter` | ✅ | — | class initialized |

---

## Threads (`tests/threads/`)

| Class | Tests | Notes |
|-------|-------|-------|
| `SbMutex` | ✅ | migrated from vanilla testsuite |
| `SbThreadMutex` | ✅ | migrated from vanilla testsuite |
| `SbCondVar` | ✅ | migrated from vanilla testsuite |
| `SbRWMutex` | ✅ | migrated from vanilla testsuite |
| `SbThread` | ✅ | migrated from vanilla testsuite |
| `SbBarrier` | ✅ | migrated from vanilla testsuite |
| `SbFifo` | ✅ | migrated from vanilla testsuite |
| `SbStorage` | ✅ | migrated from vanilla testsuite |
| `SbTypedStorage` | ✅ | migrated from vanilla testsuite |
| `SbThreadAutoLock` | ✅ | migrated from vanilla testsuite |

---

## XML / ScXML

*Not tested – Obol has removed all XML/VRML/ScXML logic.*

---

## Shaders / Shadows / Geo (`tests/nodes/test_nodes_suite.cpp`)

| Module | Tests | Vanilla Baseline |
|--------|-------|-----------------|
| `SoShaderProgram` | ✅ | `src/shaders/SoShaderProgram.cpp` | class initialized |
| `SoFragmentShader` | ✅ | `src/shaders/SoFragmentShader.cpp` | class initialized |
| `SoVertexShader` | ✅ | `src/shaders/SoVertexShader.cpp` | class initialized |
| `SoGeometryShader` | ✅ | `src/shaders/SoGeometryShader.cpp` | class initialized |
| `SoShaderParameter*` | ✅ | `src/shaders/SoShaderParameter.cpp` | 1f/1i/2f/2i/3f/3i/4f/4i/Array1f/Array1i/Matrix/MatrixArray/StateMatrix |
| `SoShadowGroup` | ✅ | `src/shadows/SoShadowGroup.cpp` | class initialized |
| `SoShadowStyle` | ✅ | `src/shadows/SoShadowStyle.cpp` | class initialized |
| `SoGeoCoordinate` | ✅ | `src/geo/SoGeoCoordinate.cpp` | class initialized |
| `SoGeoOrigin` | ✅ | `src/geo/SoGeoOrigin.cpp` | class initialized |

---

## Draggers (`tests/nodes/test_nodes_suite.cpp`)

| Module | Tests | Vanilla Baseline |
|--------|-------|-----------------|
| `SoTransformerDragger` | 🔶 | `src/draggers/SoTransformerDragger.cpp` | class type registered (constructor crashes without rendering context) |
| Other draggers | ❌ | — |

---

## Visual Rendering (`tests/rendering/`)

Visual regression tests that render scenes with `SoOffscreenRenderer` and compare
pixel output against PNG control images.  Control images are generated from Obol + GLX
(treated as equivalent to vanilla upstream for non-text geometry rendering).
Tests run with both OSMesa and GLX backends; per-test RMSE thresholds accommodate
the small rendering differences between backends.

### Image-comparison tests (pixel output vs stored PNG)

| Test | Tests | Backend | Scene / Properties Verified |
|------|-------|---------|------------------------------|
| `render_primitives` | ✅ | GLX + OSMesa | `SoGLRenderAction`: SoSphere, SoCube, SoCone, SoCylinder, all 4 in 2×2 grid with distinct colours |
| `render_materials` | ✅ | GLX + OSMesa | `SoMaterial`: matte → mirror shininess gradient + emissive sphere |
| `render_lighting` | ✅ | GLX + OSMesa | `SoDirectionalLight`, `SoPointLight`, `SoSpotLight` – 3 spheres lit individually |
| `render_transforms` | ✅ | GLX + OSMesa | `SoTranslation`, `SoRotation`, `SoScale` – 3×3 grid showing each transform type |
| `render_cameras` | ✅ | GLX + OSMesa | `SoPerspectiveCamera` vs `SoOrthographicCamera` – same depth scene, two renders |
| `render_drawstyle` | ✅ | GLX + OSMesa | `SoDrawStyle`: FILLED, LINES, POINTS modes of a low-res icosphere |
| `render_texture` | ✅ | GLX + OSMesa | `SoTexture2`: procedural 8×8 checker (red/white) mapped onto a sphere, untextured sphere as reference |
| `render_text2` | ✅ | GLX + OSMesa | `SoText2`: flat text rendering with default ProFont |
| `render_text3` | ✅ | GLX + OSMesa | `SoText3`: extruded text rendering with default ProFont |
| `render_gradient` | ✅ | GLX + OSMesa | `SoCoordinate3` + `SoIndexedFaceSet`: 10-strip red→green colour gradient |
| `render_colored_cube` | ✅ | GLX + OSMesa | Single red `SoCube` with Phong shading |
| `render_coordinates` | ✅ | GLX + OSMesa | `SoVertexProperty` + `SoIndexedFaceSet`: 4-quadrant colour orientation test |
| `render_scene` | ✅ | GLX + OSMesa | 2×2 grid of primitives in a single compound scene |

### Self-validating pixel-analysis tests (return 0/1 directly)

| Test | Tests | What is Validated |
|------|-------|-------------------|
| `render_sphere_position` | ✅ | Pixel-accurate sphere positioning: orthographic + `SoTransform` offset matches predicted pixel centre |
| `render_checker_texture` | ✅ | Checkerboard texture mapping: `SoTexture2` → near-equal black+white pixel counts in centre region |
| `render_face_set` | ✅ | `SoFaceSet` + `SoCoordinate3`: green quad in lower-left, black background in upper-right |
| `render_line_set` | ✅ | `SoLineSet` + `SoCoordinate3` + `SoBaseColor`: red horizontal line visible at expected pixel row |
| `render_switch_visibility` | ✅ | `SoSwitch` (SO_SWITCH_ALL / SO_SWITCH_NONE): spheres appear/disappear across 3 rendered frames |
| `render_vertex_colors` | ✅ | `SoPackedColor` + `SoMaterialBinding::PER_VERTEX_INDEXED` + `SoIndexedFaceSet`: 4-corner colour interpolation |
| `render_clip_plane` | ✅ | `SoClipPlane`: upper hemisphere of sphere visible; lower half clipped to background |

**Infrastructure:**
- `tests/rendering/CMakeLists.txt` – backend-aware build macro (OSMesa or GLX)
- `tests/rendering/generate_controls.sh` – regenerate control images via GLX
- `tests/rendering/rgb_to_png_py.py` – stdlib-only RGB→PNG converter (fallback; real builds use `rgb_to_png` via libpng)
- `tests/run_image_test.cmake` – CTest driver (xvfb-run for GLX; per-test RMSE thresholds)
- `tests/control_images/render_*_control.png` – stored PNG control images

**Notes:**
- Text rendering (`SoText2`, `SoText3`) is intentionally deferred due to font
  differences between vanilla Coin (FreeType) and Obol (Profont).
- OSMesa vs GLX differences are expected (RMSE 1–8); thresholds set at 2× maximum
  observed difference to catch real regressions while tolerating backend variation.
- `SoBaseColor` is used for line/point primitives instead of `SoMaterial::emissiveColor`
  because the emissive path does not colour non-face geometry under the default
  Phong lighting model.

---

## Code Coverage (`COIN_COVERAGE`)

Obol includes built-in support for generating lcov HTML coverage reports.

### Enabling coverage

```bash
cmake -S . -B build_cov \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCOIN3D_USE_OSMESA=ON \
    -DCOIN_COVERAGE=ON

cmake --build build_cov          # compile with --coverage instrumentation
cmake --build build_cov --target coverage   # run tests + generate report
```

The `coverage` target:
1. Resets all gcov counters (`lcov --zerocounters`)
2. Runs the full CTest suite (`ctest --output-on-failure`)
3. Captures per-source coverage data (`lcov --capture`)
4. Strips out system headers (`/usr/*`) and third-party submodules (`external/`, `upstream/`)
5. Generates an HTML report at `<build>/coverage_report/index.html`

### Requirements

- GCC or Clang (tested with GCC 12+)
- `lcov` and `genhtml` installed: `sudo apt-get install lcov`
- Debug build type recommended (avoids inlining that confuses coverage counters)

### Interpreting results

The HTML report breaks coverage down by:
- **Line coverage** – which source lines were executed by at least one test
- **Function coverage** – which functions were called
- **Branch coverage** – which conditional branches (if/else, switch cases) were taken

### Baseline metrics (captured 2026-02-24)

| Metric | Hit | Total | Percentage |
|--------|----:|------:|----------:|
| Lines (`src/` only) | 35,877 | 87,303 | **41.1 %** |
| Functions (`src/` only) | 7,603 | 16,102 | **47.2 %** |
| Lines (all project files) | 46,681 | 99,374 | **47.0 %** |

*Previous baseline (2026-02-23): 31.6 % lines / 40.7 % functions (`src/` only).*

See `COVERAGE_PLAN.md` for per-subsystem breakdown, the top files by uncovered lines,
and a prioritised plan to reach ≥ 70 % line coverage.

Focus areas for improving coverage:
- `src/manips/` (26 %) – manipulator state-machine code; built on draggers
- `src/geo/` (12 %) – geo coordinate nodes
- `src/rendering/` (39 %) – `SoGLRenderAction` and `SoOffscreenRenderer` edge cases
- `src/base/` (34 %) – matrix math and other base types
- `src/fonts/` (34 %) – font rasterizer (SoText2/SoText3 paths)

---

## Summary

| Category | Covered | Total (approx.) |
|----------|---------|-----------------|
| Base types | 28 | ~30 |
| SF Fields | 53 | 53 |
| MF Fields | 41 | 41 |
| Actions | 11 | 11 |
| Nodes | 74 | 75+ |
| I/O / SoDB | 9 | 10 |
| Sensors | 8 | 8 |
| Engines | 16 | 16 |
| Threads | 10 | 10 |
| XML/ScXML | 0 | 0 (removed in Obol) |
| Shaders/Shadows/Geo | 16 | 16 |
| Draggers | 1 (partial) | 20+ |
| Visual rendering (image-comparison) | 13 | ~15 |
| Visual rendering (pixel-validation) | 7 | ~10 |

---

## Next Steps (Priority Order)

1. **SoDB / I/O** – `SoDB::readAll` (VRML 2.0 path removed in Obol); binary read-back round-trip (currently crashes – needs investigation)
2. **Dragger deep-copy test** – needs rendering context for dragger construction; other dragger types still need coverage
3. **SoText2 / SoText3 visual rendering** – defer until font strategy is decided
   (vanilla uses FreeType; Obol uses Profont – direct pixel comparison is impractical
   without a shared font baseline)
4. ~~**lcov baseline**~~ – ✅ Done (see baseline metrics above; re-baselined 2026-02-24)
5. **Increase rendering coverage** – add tests for `SoQuadMesh`, `SoTriangleStripSet`, `SoLOD`, `SoEnvironment` (fog), `SoAnnotation`, `SoArray`/`SoMultipleCopy`

