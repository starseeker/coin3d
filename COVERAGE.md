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
| `SbVec3d` | ❌ | `src/base/SbVec3d.cpp` | |
| `SbVec3s` | ❌ | `src/base/SbVec3s.cpp` | |
| `SbVec3us` | ❌ | `src/base/SbVec3us.cpp` | |
| `SbVec4f` | ❌ | `src/base/SbVec4f.cpp` | |
| `SbBox2f` | ✅ | `src/base/SbBox2f.cpp` | getSize, getClosestPoint (outside, center) |
| `SbBox2d` | ❌ | `src/base/SbBox2d.cpp` | |
| `SbBox2i32` | ❌ | `src/base/SbBox2i32.cpp` | |
| `SbBox2s` | ❌ | `src/base/SbBox2s.cpp` | |
| `SbBox3f` | ✅ | `src/base/SbBox3f.cpp` | getClosestPoint (outside, center) |
| `SbBox3d` | ❌ | `src/base/SbBox3d.cpp` | |
| `SbBox3i32` | ✅ | `src/base/SbBox3i32.cpp` | getSize, getClosestPoint |
| `SbBox3s` | ❌ | `src/base/SbBox3s.cpp` | |
| `SbByteBuffer` | ✅ | `src/base/SbByteBuffer.cpp` | pushUnique, pushOnEmpty |
| `SbBSPTree` | ✅ | `src/base/SbBSPTree.cpp` | add/find/remove points |
| `SbMatrix` | ✅ | `src/base/SbMatrix.cpp` | construct from SbDPMatrix |
| `SbDPMatrix` | ✅ | `src/base/SbDPMatrix.cpp` | construct from SbMatrix |
| `SbDPRotation` | ❌ | `src/base/SbDPRotation.cpp` | |
| `SbDPPlane` | ❌ | `src/base/SbDPPlane.cpp` | |
| `SbRotation` | ✅ | `src/base/SbRotation.cpp` | fromString valid/invalid |
| `SbString` | ✅ | `src/base/SbString.cpp` | operator+ (all three forms) |
| `SbPlane` | ✅ | `src/base/SbPlane.cpp` | plane-plane intersection |
| `SbViewVolume` | ✅ | `src/base/SbViewVolume.cpp` | ortho/perspective intersection |
| `SbImage` | ❌ | `src/base/SbImage.cpp` | |
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
| `SoSFColorRGBA` | ❌ | `src/fields/SoSFColorRGBA.cpp` | |
| `SoSFString` | ✅ | `src/fields/SoSFString.cpp` | initialized, set/get round-trip |
| `SoSFRotation` | ✅ | `src/fields/SoSFRotation.cpp` | initialized |
| `SoSFMatrix` | ✅ | `src/fields/SoSFMatrix.cpp` | initialized |
| `SoSFName` | ✅ | `src/fields/SoSFName.cpp` | initialized |
| `SoSFTime` | ✅ | `src/fields/SoSFTime.cpp` | initialized |
| `SoSFEnum` | ❌ | `src/fields/SoSFEnum.cpp` | |
| `SoSFBitMask` | ❌ | `src/fields/SoSFBitMask.cpp` | |
| `SoSFImage` | ❌ | `src/fields/SoSFImage.cpp` | |
| `SoSFImage3` | ❌ | `src/fields/SoSFImage3.cpp` | |
| `SoSFPlane` | ❌ | `src/fields/SoSFPlane.cpp` | |
| `SoSFNode` | ❌ | `src/fields/SoSFNode.cpp` | |
| `SoSFPath` | ❌ | `src/fields/SoSFPath.cpp` | |
| `SoSFEngine` | ❌ | `src/fields/SoSFEngine.cpp` | |
| `SoSFTrigger` | ❌ | `src/fields/SoSFTrigger.cpp` | |
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
| `SoMFColorRGBA` | ❌ | `src/fields/SoMFColorRGBA.cpp` | |
| `SoMFEnum` | ❌ | `src/fields/SoMFEnum.cpp` | |
| `SoMFBitMask` | ❌ | `src/fields/SoMFBitMask.cpp` | |
| `SoMFNode` | ❌ | `src/fields/SoMFNode.cpp` | |
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
| `SoGetMatrixAction` | ❌ | — | |
| `SoHandleEventAction` | ❌ | — | |
| `SoPickAction` | ❌ | — | |
| `SoRayPickAction` | ❌ | — | |
| `SoGetPrimitiveCountAction` | ❌ | — | |
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
| `SoCylinder` | ❌ | — | |
| `SoMaterial` | ✅ | — | default diffuseColor count |
| `SoDirectionalLight` | ❌ | — | |
| `SoPointLight` | ❌ | — | |
| `SoSpotLight` | ❌ | — | |
| `SoTranslation` | ❌ | — | |
| `SoRotation` | ❌ | — | |
| `SoScale` | ❌ | — | |
| `SoTransform` | ❌ | — | |
| `SoCamera` (base) | ❌ | — | |
| `SoPerspectiveCamera` | ❌ | — | |
| `SoOrthographicCamera` | ❌ | — | |
| `SoSwitch` | ❌ | — | (covered indirectly via actions tests) |
| `SoText2` / `SoText3` | ❌ | — | |
| Geometry nodes (Face/IndexedFace/Strip/Quad sets) | ❌ | — | |
| `SoCoordinate3` | ❌ | — | |
| `SoNormal` | ❌ | — | |
| `SoTextureCoordinate2` | ❌ | — | |
| `SoTexture2` | ❌ | — | |
| Shader nodes | ❌ | `src/shaders/` | vanilla has tests |
| Shadow nodes | ❌ | `src/shadows/` | vanilla has tests |
| Geo nodes | ❌ | `src/geo/` | vanilla has tests |

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
| `SoIdleSensor` | ❌ | |
| `SoPathSensor` | ❌ | |
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
| `SoComposeMatrix` | ❌ | |
| `SoComposeRotation` | ❌ | |
| `SoComposeVec2f` / `SoComposeVec4f` | ❌ | |
| `SoComputeBoundingBox` | ❌ | |
| `SoGate` | ❌ | |
| `SoInterpolate*` | ❌ | |
| `SoSelectOne` | ❌ | |
| `SoTimeCounter` | ❌ | |
| `SoCounter` | ❌ | |

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

## XML / ScXML (`tests/` – not yet organised)

| Module | Tests | Vanilla Baseline |
|--------|-------|-----------------|
| `xml/document.cpp` | ❌ | `src/xml/document.cpp` |
| `scxml/SbStringConvert` | ❌ | `src/scxml/SbStringConvert.cpp` |
| `scxml/ScXMLMinimumEvaluator` | ❌ | `src/scxml/ScXMLMinimumEvaluator.cpp` |
| `soscxml/ScXMLCoinEvaluator` | ❌ | `src/soscxml/ScXMLCoinEvaluator.cpp` |

---

## Shaders / Shadows / Geo (`tests/` – not yet organised)

| Module | Tests | Vanilla Baseline |
|--------|-------|-----------------|
| `SoShaderProgram` | ❌ | `src/shaders/SoShaderProgram.cpp` |
| `SoFragmentShader` | ❌ | `src/shaders/SoFragmentShader.cpp` |
| `SoVertexShader` | ❌ | `src/shaders/SoVertexShader.cpp` |
| `SoGeometryShader` | ❌ | `src/shaders/SoGeometryShader.cpp` |
| `SoShaderParameter*` | ❌ | `src/shaders/SoShaderParameter.cpp` |
| `SoShadowGroup` | ❌ | `src/shadows/SoShadowGroup.cpp` |
| `SoShadowStyle` | ❌ | `src/shadows/SoShadowStyle.cpp` |
| `SoGeoCoordinate` | ❌ | `src/geo/SoGeoCoordinate.cpp` |
| `SoGeoOrigin` | ❌ | `src/geo/SoGeoOrigin.cpp` |

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
| Base types | 12 | ~30 |
| SF Fields | 16 | 47 |
| MF Fields | 17 | 40 |
| Actions | 4 | 11 |
| Nodes | 9 | 50+ |
| I/O / SoDB | 5 | 10 |
| Sensors | 5 | 8 |
| Engines | 6 | 15+ |
| Threads | 10 | 10 |
| XML/ScXML | 0 | 4 |
| Shaders/Shadows/Geo | 0 | 15 |

---

## Next Steps (Priority Order)

1. **SoBase write/read tests** – vanilla has tests in `src/misc/SoBase.cpp`
2. **Remaining SbVec/SbBox variants** – `SbVec3d`, `SbVec4f`, `SbBox2d`, `SbBox3d`, etc.
3. **SoSFBool extended** – more text-input edge cases from vanilla
4. **VRML 2.0 read tests** – `SoDB::readAll` with VRML content
5. **Shader node initialization** – vanilla has COIN_TEST_SUITE blocks
6. **Shadow node initialization** – vanilla has COIN_TEST_SUITE blocks
7. **Geo node initialization** – vanilla has COIN_TEST_SUITE blocks
8. **Visual/rendering tests** – require rendering context (OSMesa/GLX)
