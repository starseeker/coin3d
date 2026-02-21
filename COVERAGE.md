# Coin API Test Coverage

This file tracks which Coin APIs have tests and which still need coverage.
Tests in `tests/` subdirectories are baselined against the
`coin_vanilla` reference implementation (`COIN_TEST_SUITE` blocks).

---

## Legend

| Symbol | Meaning |
|--------|---------|
| ✅ | Tests written and baselined against coin_vanilla |
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
| `SbVec3us` | ❌ | `src/base/SbVec3us.cpp` | |
| `SbVec4f` | ✅ | `src/base/SbVec4f.cpp` | normalize already-normalized |
| `SbBox2f` | ✅ | `src/base/SbBox2f.cpp` | getSize, getClosestPoint (outside, center) |
| `SbBox2d` | ✅ | `src/base/SbBox2d.cpp` | getSize, getClosestPoint |
| `SbBox2i32` | ❌ | `src/base/SbBox2i32.cpp` | |
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
| `SbColor4f` | ❌ | — | |
| `SbLine` | ❌ | — | |
| `SbSphere` | ❌ | — | |
| `SbCylinder` | ❌ | — | |
| `SbHeap` | ❌ | `src/base/heap.cpp` | |

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
| `SoSFImage` | ❌ | `src/fields/SoSFImage.cpp` | |
| `SoSFImage3` | ❌ | `src/fields/SoSFImage3.cpp` | |
| `SoSFPlane` | ✅ | `src/fields/SoSFPlane.cpp` | initialized |
| `SoSFNode` | ✅ | `src/fields/SoSFNode.cpp` | initialized |
| `SoSFPath` | ❌ | `src/fields/SoSFPath.cpp` | |
| `SoSFEngine` | ❌ | `src/fields/SoSFEngine.cpp` | |
| `SoSFTrigger` | ✅ | `src/fields/SoSFTrigger.cpp` | initialized |
| `SoSFBox2d/2f/2i32/2s` | ❌ | box SF fields | |
| `SoSFBox3d/3f/3i32/3s` | ❌ | box SF fields | |
| `SoSFVec2b/d/i32/s` | ❌ | vec SF fields | |
| `SoSFVec3b/d/i32/s` | ❌ | vec SF fields | |
| `SoSFVec4b/d/i32/s/ub/ui32/us` | ❌ | vec SF fields | |

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
| `SoMFPath` | ❌ | `src/fields/SoMFPath.cpp` | |
| `SoMFEngine` | ❌ | `src/fields/SoMFEngine.cpp` | |
| `SoMFVec2b/d/i32/s` | ❌ | vec MF fields | |
| `SoMFVec3b/d/i32/s` | ❌ | vec MF fields | |
| `SoMFVec4b/d/i32/s/ub/ui32/us` | ❌ | vec MF fields | |

---

## Actions (`tests/actions/`)

| Class | Tests | Vanilla Baseline | Notes |
|-------|-------|-----------------|-------|
| `SoCallbackAction` | ✅ | `src/actions/SoCallbackAction.cpp` | callbackAll on/off, switch traversal |
| `SoWriteAction` | ✅ | `src/actions/SoWriteAction.cpp` | DEF/USE naming for multi-ref nodes |
| `SoSearchAction` | ✅ | — | find by name, find by type |
| `SoGetBoundingBoxAction` | ✅ | — | unit cube bounds |
| `SoGLRenderAction` | ❌ | — | needs rendering context |
| `SoGetMatrixAction` | ✅ | — | class initialized, identity for empty scene |
| `SoHandleEventAction` | ❌ | — | |
| `SoPickAction` | ❌ | — | |
| `SoRayPickAction` | ❌ | — | |
| `SoGetPrimitiveCountAction` | ✅ | — | class initialized, count 0 for empty scene |
| `SoReorganizeAction` | ❌ | — | |
| `SoAudioRenderAction` | ❌ | — | |

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
| `SoMaterial` | ✅ | — | default diffuseColor count |
| `SoDirectionalLight` | ✅ | — | class initialized |
| `SoPointLight` | ✅ | — | class initialized |
| `SoSpotLight` | ✅ | — | class initialized |
| `SoTranslation` | ✅ | — | default translation (0,0,0) |
| `SoRotation` | ✅ | — | default rotation (identity) |
| `SoScale` | ✅ | — | default scaleFactor (1,1,1) |
| `SoTransform` | ✅ | — | default translation (0,0,0) |
| `SoCamera` (base) | ❌ | — | |
| `SoPerspectiveCamera` | ✅ | — | class initialized |
| `SoOrthographicCamera` | ✅ | — | class initialized |
| `SoSwitch` | ✅ | — | default whichChild == SO_SWITCH_NONE |
| `SoText2` / `SoText3` | ❌ | — | |
| Geometry nodes (Face/IndexedFace/Strip/Quad sets) | ❌ | — | |
| `SoCoordinate3` | ✅ | — | class initialized |
| `SoNormal` | ✅ | — | class initialized |
| `SoTextureCoordinate2` | ❌ | — | |
| `SoTexture2` | ❌ | — | |
| Shader nodes | ✅ | `src/shaders/` | SoShaderProgram, SoFragmentShader, SoVertexShader, SoGeometryShader class initialized |
| Shadow nodes | ❌ | `src/shadows/` | vanilla has tests |
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
| `SoDB::readAll` (VRML 2.0) | ❌ | `src/misc/SoDB.cpp` | readChildList (VRML) |
| `SoBase` write/read | ❌ | `src/misc/SoBase.cpp` | vanilla has tests |
| Binary format I/O | ❌ | — | |

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
| `SoDataSensor` | ❌ | |

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
| `SoComputeBoundingBox` | ❌ | |
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
| `SoShaderParameter*` | ❌ | `src/shaders/SoShaderParameter.cpp` | |
| `SoShadowGroup` | ❌ | `src/shadows/SoShadowGroup.cpp` | |
| `SoShadowStyle` | ❌ | `src/shadows/SoShadowStyle.cpp` | |
| `SoGeoCoordinate` | ✅ | `src/geo/SoGeoCoordinate.cpp` | class initialized |
| `SoGeoOrigin` | ✅ | `src/geo/SoGeoOrigin.cpp` | class initialized |

---

## Draggers

| Module | Tests | Vanilla Baseline |
|--------|-------|-----------------|
| `SoTransformerDragger` | ❌ | `src/draggers/SoTransformerDragger.cpp` |
| Other draggers | ❌ | — |

---

## Summary

| Category | Covered | Total (approx.) |
|----------|---------|-----------------|
| Base types | 22 | ~30 |
| SF Fields | 22 | 47 |
| MF Fields | 21 | 40 |
| Actions | 6 | 11 |
| Nodes | 25 | 50+ |
| I/O / SoDB | 5 | 10 |
| Sensors | 7 | 8 |
| Engines | 15 | 15+ |
| Threads | 10 | 10 |
| XML/ScXML | 0 | 0 (removed in Obol) |
| Shaders/Shadows/Geo | 5 | 15 |

---

## Next Steps (Priority Order)

1. **SoBase write/read tests** – vanilla has tests in `src/misc/SoBase.cpp`
2. **SoSFImage / SoSFImage3** – field initialized tests
3. **Shadow node initialization** – `SoShadowGroup`, `SoShadowStyle`
4. **SoComputeBoundingBox engine** – class initialized
5. **SoRayPickAction** – pick action with scene graph
6. **Visual/rendering tests** – require rendering context (OSMesa/GLX)
