# Obol Code Review Progress

This file tracks ongoing code review work.  Each session should update the
checklist below and add entries to the **Session Log**.

---

## Overall Goals

- General quality pass: cleanup, best practices, comment clarity/completeness
- Migrate inline block comments on public API to Doxygen `/*!` / `/**` style
  so that `make obol_docs` generates usable API reference documentation
- Fix any functional issues (bugs, UB, resource leaks) found along the way

---

## Status Legend

- ✅ Done
- 🔄 In progress / partial
- ⬜ Not started

---

## Infrastructure

| Item | Status | Notes |
|------|--------|-------|
| Add `docs/Doxyfile.in` (Doxygen config template) | ✅ | Created in session 1 |
| CMake `OBOL_BUILD_DOCS` option + `obol_docs` target | ✅ | `cmake -DOBOL_BUILD_DOCS=ON` then `make obol_docs` |
| Verify `make obol_docs` runs without errors | ✅ | 2318 HTML pages generated; warnings are pre-existing |

---

## Public Header Documentation  (`include/Inventor/`)

Priority: headers used directly by application code.

| Header | Class Doc | Notes |
|--------|-----------|-------|
| `SbBasic.h` | ✅ | Added `\defgroup coin_math_utilities` + `\fn` Doxygen for all template utilities |
| `SbMatrix.h` | ✅ | Added `\class SbMatrix` Doxygen block |
| `SbVec3f.h` | ✅ | Added `\class SbVec3f` Doxygen block |
| `SoDB.h` | ✅ | Added `\class SoDB` block; converted `ContextManager` block comment to `\class`; added `\brief` docs for pure-virtual GL lifecycle methods |
| `SoInput.h` | ✅ | Added `\class SoInput` Doxygen block |
| `SoOutput.h` | ✅ | Added `\class SoOutput` Doxygen block |
| `SoOffscreenRenderer.h` | ✅ | Added `\class SoOffscreenRenderer` Doxygen block |
| `SoRenderManager.h` | ✅ | Added `\class SoRenderManager` Doxygen block |
| `SbVec2f.h` | ⬜ | |
| `SbVec3d.h` | ⬜ | |
| `SbVec4f.h` | ⬜ | |
| `SbRotation.h` | ⬜ | |
| `SbLine.h` | ⬜ | |
| `SbPlane.h` | ⬜ | |
| `SbViewVolume.h` | ⬜ | |
| `SbViewportRegion.h` | ⬜ | |
| `SbColor.h` | ⬜ | |
| `SbString.h` | ⬜ | |
| `SbName.h` | ⬜ | |
| `SbTime.h` | ⬜ | |
| `SbImage.h` | ⬜ | |
| `SoType.h` | ⬜ | |
| `SoPath.h` | ⬜ | |
| `SoSceneManager.h` | ⬜ | |
| `SoEventManager.h` | ⬜ | |
| `SoPickedPoint.h` | ⬜ | |
| `SoPrimitiveVertex.h` | ⬜ | |
| `SoFullPath.h` | ⬜ | |
| All `actions/` headers | ⬜ | ~15 headers |
| All `nodes/` headers | ⬜ | ~100 headers |
| All `fields/` headers | ⬜ | ~60 headers |
| All `elements/` headers | ⬜ | ~70 headers |
| All `engines/` headers | ⬜ | ~30 headers |
| All `sensors/` headers | ⬜ | ~15 headers |
| All `events/` headers | ⬜ | ~15 headers |

---

## Source File Documentation  (`src/`)

The `.cpp` files already carry the bulk of the Doxygen documentation (Coin
convention: `\fn` / `\class` blocks live in the implementation file).  The
following items remain:

| Item | Status | Notes |
|------|--------|-------|
| Fix duplicate `/*!` opening in `src/engines/SoNodeEngine.cpp:317` | ✅ | Removed stray extra `/*!` (Doxygen reported "nested comment" error) |
| Fix broken `/// * !` comment in `src/misc/SoDB.cpp:33` | ✅ | Converted to proper `/*!` block, updated text for `SbBool = bool` reality |
| `OBOL_UNUSED_ARG` macro in Doxygen `\fn` signatures (SoReorganizeAction.cpp) | ⬜ | Doxygen cannot resolve the macro-expanded signature; consider adding `PREDEFINED` entry in Doxyfile or reformatting `\fn` lines |
| `src/errors/error.cpp` – `\class` references missing header `error.h` | ⬜ | Internal header not in include search path; low priority |
| `src/misc/SoGlyph.cpp` – `\class` references missing header `SoGlyph.h` | ⬜ | Same; low priority |

---

## Code Quality / Best Practices

| Item | Status | Notes |
|------|--------|-------|
| `FIXME` comments in `src/actions/` (SoCallbackAction, SoReorganizeAction, SoGLRenderAction, etc.) | ⬜ | Many date from Coin era; evaluate which are still relevant |
| `FIXME` in `src/fields/` (SoField, SoFieldContainer, SoFieldData) | ⬜ | Several are still actionable |
| `HACK` comment in `SoFieldContainer.cpp:131` – bitmask misuse of `donotify` | ⬜ | Possibly clean up with a dedicated bitfield struct |
| `SoDB.cpp` SbBool typedef doc (was broken `/// * !`) | ✅ | Fixed session 1 |
| Remove/update stale Coin-era comments referencing removed subsystems (VRML, ScXML, audio) | ⬜ | Many instances across `src/` |
| Audit `const_cast` usage | ⬜ | `SoFieldContainer.cpp:885` is flagged with "ugly constness cast" FIXME |
| Audit raw pointer ownership across public API | ⬜ | Several `void *` context handles could be typed |
| `add_definitions(-g)` in CMakeLists.txt – debug info in Release builds | ⬜ | Likely unintentional; should be gated on `CMAKE_BUILD_TYPE STREQUAL Debug` |
| `src/base/utf8/include/` not on Doxygen INCLUDE_PATH | ✅ | Fixed in Doxyfile.in session 1 |

---

## Session Log

### Session 1 (2026-03-03)

**Files changed:**

- `docs/Doxyfile.in` — new file; Doxygen configuration template for the
  `obol_docs` CMake target
- `CMakeLists.txt` — replaced the "Documentation options removed" stub with a
  proper `OBOL_BUILD_DOCS` option and `find_package(Doxygen)` + custom target
- `src/misc/SoDB.cpp` — fixed broken `/// * !` comment block (was not valid
  Doxygen; converted to proper `/*!` block; updated text to reflect that
  `SbBool` is now a `bool` alias rather than `int`)
- `src/engines/SoNodeEngine.cpp` — removed duplicate `/*!` opening that Doxygen
  flagged as an unclosed nested comment
- `include/Inventor/SbBasic.h` — replaced plain block comment for the utility
  template functions with proper `/*!  \defgroup` / `\fn` Doxygen markup
- `include/Inventor/SbMatrix.h` — added `\class SbMatrix` Doxygen block
- `include/Inventor/SbVec3f.h` — added `\class SbVec3f` Doxygen block
- `include/Inventor/SoDB.h` — added `\class SoDB` block; converted
  `ContextManager` plain comment to `\class` Doxygen block; added `\brief`
  annotations to the four pure-virtual GL lifecycle methods
- `include/Inventor/SoInput.h` — added `\class SoInput` Doxygen block
- `include/Inventor/SoOutput.h` — added `\class SoOutput` Doxygen block
- `include/Inventor/SoOffscreenRenderer.h` — added `\class SoOffscreenRenderer`
  Doxygen block with usage example
- `include/Inventor/SoRenderManager.h` — added `\class SoRenderManager`
  Doxygen block

**Build verification:**

`cmake -DOBOL_BUILD_DOCS=ON ... && make && make obol_docs` — clean build,
2318 HTML pages generated.  All Doxygen warnings in the output are pre-existing
(unclosed comment in `SoNodeEngine.cpp` is now fixed; remaining warnings are
macro-expansion and missing-include issues from legacy Coin code).

**Next session priorities:**

1. Continue adding `\class` blocks to the remaining ~70 undocumented public
   headers, starting with the math types (`SbVec2f`, `SbVec3d`, `SbRotation`,
   `SbLine`, `SbPlane`, `SbViewVolume`, …) and then the core action/node/field
   headers.
2. Evaluate the `OBOL_UNUSED_ARG` macro Doxygen warning in
   `SoReorganizeAction.cpp` and add a `PREDEFINED` expansion if needed.
3. Address the `add_definitions(-g)` always-on debug-info issue in
   `CMakeLists.txt`.
4. Review the still-relevant `FIXME` items in `SoGLRenderAction.cpp`.
