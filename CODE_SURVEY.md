# Obol Code Survey

*Generated: 2026-02-24.  Based on lcov coverage build (Debug + OSMesa,
`COIN_COVERAGE=ON`) plus manual inspection of every uncovered subsystem.*

---

## 1. Coverage Metrics (this build)

| Scope | Lines hit | Total | % |
|-------|----------:|------:|--:|
| All project files | 72,795 | 115,551 | **63.0 %** |
| `src/` only | 44,834 | 85,922 | **52.2 %** |
| Functions (all) | 11,172 | 18,753 | **59.6 %** |

Per-subsystem (`src/` only, sorted by line coverage):

| Subsystem | Line % | Uncovered lines | Assessment |
|-----------|-------:|----------------:|------------|
| `hardcopy` | 1.4 % | 1,198 | **Dead** – PostScript output; no callers in Obol |
| `profiler` | 5.3 % | 2,612 | **Dead paths** – `isEnabled()` always FALSE |
| `fonts` | 34.4 % | 2,063 | Untested – glyph rasteriser for SoText2/SoText3 |
| `fields` | 37.3 % | 3,334 | Untested – many field-conversion paths |
| `caches` | 39.1 % | 1,271 | Partially untested – earcut tessellator never reached |
| `base` | 41.8 % | 5,712 | Mixed – `cc_string`, `utf8/ini.cpp` dead; most live but untested |
| `threads` | 41.1 % | 319 | Untested – edge-case thread primitives |
| `events` | 43.1 % | 333 | Untested – many event-type subclasses |
| `glue` | 47.8 % | 1,208 | Untested – extension GL paths (geometry shaders etc.) |
| `actions` | 53.3 % | 1,452 | Untested – transparency sort, pass callbacks |
| `misc` | 53.3 % | 2,243 | Partially untested – SoProto, SoSceneManager paths |
| `engines` | 53.7 % | 1,643 | Untested – SoConvertAll type-conversion paths |
| `io` | 55.8 % | 939 | Untested – binary edge cases, corrupt files |
| `nodes` | 55.9 % | 4,572 | Untested – SoExtSelection, various callbacks |
| `bundles` | 50.4 % | 135 | Untested – normal-bundle generation paths |
| `rendering` | 57.6 % | 1,840 | Partially untested – SoRenderManager, big-image tiling |
| `shapenodes` | 59.3 % | 3,409 | Partially untested – SoAsciiText, SoImage |
| `hud` | 62.2 % | 73 | Mostly tested |
| `nodekits` | 62.1 % | 765 | Partially untested |
| `projectors` | 62.2 % | 276 | Partially untested |
| `shaders` | 63.4 % | 624 | Partially untested |
| `elements` | 68.1 % | 2,051 | Partially untested |
| `draggers` | 68.4 % | 1,738 | Partially untested – deeper drag sequences |
| `manips` | 72.2 % | 322 | Mostly tested |
| `shadows` | 74.6 % | 319 | Mostly tested |
| `sensors` | 79.6 % | 140 | Mostly tested |
| `lists` | 80.5 % | 94 | Mostly tested |
| `tools` | 94.4 % | 2 | Essentially complete |
| `details` | 93.8 % | 17 | Essentially complete |

---

## 2. Dead Code (Confirmed Unreachable)

### 2a. Already Removed: `SoGLImage` viewer-loop methods

The following 9 public methods were removed from `src/rendering/SoGLImage.cpp` and
`include/Inventor/misc/SoGLImage.h` (see `COVERAGE.md` for details):

| Method | Root cause |
|--------|------------|
| `beginFrame(SoState*)` | Viewer-loop texture management; Obol has no viewer |
| `endFrame(SoState*)` | Same |
| `freeAllImages(SoState*)` | Same |
| `setDisplayListMaxAge(uint32_t)` | Same |
| `setEndFrameCallback(cb, closure)` | VRML97 caller (`vrml97/ImageTexture.cpp`) removed |
| `getNumFramesSinceUsed()` | VRML97 caller removed |
| `setResizeCallback(cb, closure)` | No callers anywhere in Obol |
| `useAlphaTest()` | No callers; alpha-test managed via `SoLazyElement` |
| `getGLImageId()` | No callers anywhere in Obol |

Also removed: `endframecb`/`endframeclosure` private fields, `resizecb`/`resizeclosure`
static members, `glimage_maxage` static, and the `SoGLImageResizeCB` typedef.
`tagImage()` was retained – it has callers in `SoGLBigImage.cpp` and
`SoGLMultiTextureImageElement.cpp`.

---

### 2b. `src/base/string.cpp` — legacy `cc_string` C ADT (264 lines, 0 % coverage)

`cc_string` is a C-style string type from the original Coin implementation.
The header (`src/C/base/string.h`) is still included by `src/glue/glp.h` and
`src/fonts/freetype.cpp`, but **no `cc_string_*` functions are called** anywhere
in the current Obol codebase – all active string management uses `SbString` or
`std::string`.

**Status:** ✅ **Removed** – `string.cpp` excluded from build (`src/base/CMakeLists.txt`).
Stale `#include "C/base/string.h"` removed from `freetype.cpp`, `glp.h`, and
`CoinTidbits.cpp`.  `CoinInternalError.h` made self-sufficient with a direct
`#include "Inventor/basic.h"` (fixes pre-existing `COIN_DLL_API` build error).

---

### 2c. `src/misc/SoGlyph.cpp` — obsolete glyph cache (288 lines, 0 % coverage)

The class comment reads:
> *"This class is now obsolete, and will be removed from a later version of Coin."*

Text rendering in Obol uses `SbFont` / `SbGlyph2D` / `SbGlyph3D` directly.
`SoGlyph` has no callers in `src/` or `tests/`.

**Status:** ✅ **Removed** – `SoGlyph.cpp` excluded from build (`src/misc/CMakeLists.txt`)
and `include/Inventor/misc/SoGlyph.h` deleted.

---

### 2d. `src/base/utf8/ini.cpp` — INI file library (353 lines, 0 % coverage)

This file is part of the bundled [neacsum/utf8](https://github.com/neacsum/utf8)
library.  `utf8::IniFile` is never instantiated anywhere in Obol.  The class is
pulled in because the main `utf8.h` header unconditionally `#include`s `utf8/ini.h`.

**Status:** ✅ **Removed** – `utf8/ini.cpp` excluded from build (`src/base/CMakeLists.txt`).

---

### 2e. `src/hardcopy/` — PostScript output subsystem (1,215 lines, 1.4 % coverage)

Only `SoHardCopy::init()` / `getProductName()` / `getVersion()` execute (called from
`SoDB::init()`).  The entire implementation:
- `VectorizeActionP.cpp` – 574 lines (0 %)
- `VectorizePSAction.cpp` – 811 lines (~1 %)
- `PSVectorOutput.cpp` / `VectorOutput.cpp` / `VectorizeAction.cpp` – remainder

is never exercised because Obol has no viewer and the PostScript-output workflow
requires a running render loop.  The PostScript output capability is real, but
requires integration with a viewer that Obol does not provide.

**Status:** ✅ **Made optional** – new `COIN_HARDCOPY` CMake option (default `OFF`)
gates the entire `src/hardcopy/` subdirectory and the `SoHardCopy::init()` call
in `SoDB.cpp`.  Enable with `-DCOIN_HARDCOPY=ON` for applications that provide
their own viewer/render loop.

---

### 2f. `src/profiler/` — profiling subsystem (2,758 lines, 5.3 % coverage)

`SoProfiler::isEnabled()` is stubbed in `src/misc/SoProfiler_stubs.cpp` to always
return `FALSE`.  As a result every `if (SoProfiler::isEnabled())` guard in
`SoSeparator`, `SoGroup`, `SoMaterial`, and `SoDB` is an **always-false conditional**:
the profiling code paths are never taken.

- `SoProfilingReportGenerator.cpp` – 1,475 lines (0 %)
- `SbProfilingData.cpp` – 522 lines (11 %)
- All other profiler files – varying low coverage

**Recommendation:**
- **Short term:** Add a `COIN_PROFILING` CMake option; when OFF (the default),
  compile only `SoProfilerMinimal.cpp` and exclude the rest of `src/profiler/`.
  This would also eliminate the dead `if (SoProfiler::isEnabled())` blocks via
  `#if COIN_PROFILING`.
- **Long term:** If profiling support is not a goal for Obol, remove the entire
  `src/profiler/` tree.

---

## 3. Always-False Conditionals

### 3a. `SoProfiler::isEnabled()` guards

Four call sites in active node-traversal code:

| File | Line | Block |
|------|------|-------|
| `src/nodes/SoSeparator.cpp` | 678 | Profiler element push/pop around child traversal |
| `src/nodes/SoGroup.cpp` | 537 | Switch to `childGLRenderProfiler` function pointer |
| `src/nodes/SoMaterial.cpp` | 376 | Record material stats in profiler element |
| `src/misc/SoDB.cpp` | 443 | Enable profiler realtime sensor |

All four blocks are unreachable because `SoProfiler::isEnabled()` is hardcoded to
return `FALSE` in the Obol stub.  They add dead code to hot traversal paths.

**Recommendation:** Wrap with `#if COIN_PROFILING` once the CMake option exists.

---

### 3b. `SoGLImageP::resizecb` check in `resizeImage()`

Before this survey the code tested `if (SoGLImageP::resizecb)` to call a custom
resize callback.  `resizecb` was always `NULL` because `setResizeCallback()` (the
public setter) had no callers.  The dead setter and the unreachable check have been
removed as part of this survey (see §2a).

---

## 4. Poor Coverage — Untested But Live Code

These subsystems have significant uncovered lines that represent **real, reachable
functionality** that is simply not yet exercised by the test suite.  They are not
removal candidates; they are coverage expansion targets.

### 4a. `src/fonts/` — glyph rasteriser (34.4 %, 2,063 uncovered)

The transition to `struetype.h` as the sole font backend is complete:
`freetype.cpp` uses `stt_*` functions for bitmap glyph rendering (`SoText2`), and
`SoText3` / `SoAsciiText` use `SbFont` / `SbGlyph3D` directly for vector outlines.
The stale `#include "C/base/string.h"` has been removed from `freetype.cpp`.

The remaining uncovered lines (1,648) in `struetype.h` are reached only when
`SoText2` or `SoText3` renders characters.  Text rendering tests
(`render_text2`, `render_text3`, `render_ascii_text`) execute, but the font coverage
depends heavily on which glyphs are requested.  Adding tests that cover more Unicode
ranges and multi-line/kerned text would improve this subsystem.

### 4b. `src/base/` — math & containers (41.8 %, 5,712 uncovered)

Key uncovered clusters:
- `src/base/SbImageResize.cpp` (326 lines, 0 %) – bilinear/bicubic image scaling.
  Callers exist inside `SbImageFormatHandler`, but no test triggers a texture that
  needs power-of-2 scaling.
- `src/base/utf8/utf8.cpp` (293 lines, 0 %) – UTF-8 utility functions from the
  neacsum library; called only on non-ASCII paths.
- `src/base/string.cpp` (264 lines, 0 %) – `cc_string` C ADT; see §2b.
- `src/base/SbDPMatrix.cpp` (322 uncovered) – double-precision matrix operations;
  partially tested but `factor()`, `LU decompose`, and some `setTransform` paths
  remain uncovered.

### 4c. `src/rendering/SoRenderManager.cpp` (31 %, 395 uncovered)

Camera-auto-clipping, superimposition management, stereo rendering, and the
`scheduleRedraw` callback path are untested.  These require a more complete
rendering-loop driver than the existing OSMesa tests provide.

### 4d. `src/misc/SoProto.cpp` (11 %, 450 uncovered)

PROTO definition parsing and instantiation.  The existing `test_proto.cpp` test
covers the happy path; error recovery, nested PROTOs, and IS-field connections
are not covered.

### 4e. `src/engines/SoConvertAll.cpp` (27 %, 285 uncovered)

Automatic type-conversion engine paths.  Most conversion pairs are never triggered
by the current test suite.  A focused `SoFieldConverter` / `connectFrom` test
driving many type pairs would fill this in.

### 4f. `src/actions/SoGLRenderAction.cpp` (41 %, 507 uncovered)

Transparency sorting (`SORTED_OBJECT`, `SORTED_LAYERS_BLEND`), multi-pass
rendering, delayed-path handling, and cache-policy edge cases.  All require a full
rendering context with alpha-blended geometry.

### 4g. `src/nodes/SoExtSelection.cpp` (51 %, 531 uncovered)

Lasso and rubber-band selection logic.  Requires a full event-dispatch loop with
mouse-click and mouse-move events synthesised against a rendered scene.

### 4h. `src/caches/earcut.hpp` (0 %, 406 uncovered)

The earcut polygon tessellator is used by `SoConvexDataCache` when a non-convex
polygon is encountered.  All current test geometry happens to be convex, so this
path is never reached.  A concave `SoFaceSet` polygon would exercise it.

---

## 5. Consolidation Opportunities

### 5a. Dual profiler implementations

`src/profiler/SoProfilerMinimal.cpp` and `src/misc/SoProfiler_stubs.cpp` both
provide stub implementations of `SoProfiler::isEnabled()` and related functions.
Having two files with overlapping stubs is fragile.

**Recommendation:** Keep only `SoProfiler_stubs.cpp` in the Obol-specific `src/misc/`
directory.  Remove the corresponding functions from `src/profiler/SoProfilerMinimal.cpp`
(or exclude the entire `src/profiler/` directory when `COIN_PROFILING=OFF`).

### 5b. `glimage_reglist` maintenance without iteration

After removing `endFrame()`/`beginFrame()`/`freeAllImages()`, the `glimage_reglist`
global is still populated by `SoGLImage::registerImage()` and depopulated by
`SoGLImage::unregisterImage()` on every texture create/destroy.  The list is now
only used for cleanup at exit via `regimage_cleanup()`.  Maintaining the full list
has O(n) cost on texture creation and destruction.

**Recommendation:** Replace `glimage_reglist` with a simple reference-count or remove
it entirely if `regimage_cleanup` is no longer needed for resource tracking.

### 5c. `ResizeReason` enum with only one live value

`SoGLImage::ResizeReason` has three values (`IMAGE`, `SUBIMAGE`, `MIPMAP`) but the
only value ever passed is `IMAGE`.  An old `FIXME` comment notes this:
> *"Support other reason values than IMAGE (kintel 20050531)"*

Since the callback that consumed `ResizeReason` (`setResizeCallback`) has been
removed, the enum is now unused externally.  It is still part of the public header;
consider removing or deprecating it.

### 5d. `SoGLImage::incAge()` / `resetAge()` without a reader

`incAge()` and `resetAge()` are `protected` virtual methods called from
`SoGLBigImage` and `tagImage()`.  They maintain `PRIVATE(this)->imageage`.
The only reader was `getNumFramesSinceUsed()`, which has been removed (no external
callers).  `imageage` is now incremented and reset but never read.

**Recommendation:** Remove `incAge()`, `resetAge()`, and the `imageage` field.
This also requires updating `SoGLBigImage.cpp` (two call sites).

---

## 6. Summary Table

| Category | Finding | Action |
|----------|---------|--------|
| Dead – no callers | `SoGLImage` viewer-loop methods (9) | ✅ **Removed** (#185) |
| Dead – no callers | `src/base/string.cpp` `cc_string` ADT | ✅ **Removed from build** |
| Dead – no callers | `src/misc/SoGlyph.cpp` obsolete glyph class | ✅ **Removed from build** |
| Dead – no callers | `src/base/utf8/ini.cpp` INI file library | ✅ **Excluded from build** |
| Dead – no callers | `src/hardcopy/` PostScript output | ✅ **Made optional** (`COIN_HARDCOPY=OFF`) |
| Dead paths | `src/profiler/` – `isEnabled()` always FALSE | 📋 Add `COIN_PROFILING` option |
| Always-false | `SoProfiler::isEnabled()` guards in 4 nodes | 📋 Wrap with `#if COIN_PROFILING` |
| Always-false (fixed) | `SoGLImageP::resizecb` check | ✅ **Removed** (#185) |
| Consolidation | Dual profiler stubs | 📋 Dedup |
| Consolidation | `glimage_reglist` without iterators | 📋 Simplify or remove |
| Consolidation | `incAge()`/`resetAge()` without reader | 📋 Remove + update SoGLBigImage |
| Untested – live | `src/fonts/` glyph rasteriser | 🔬 Test expansion target |
| Untested – live | `src/base/SbImageResize.cpp` | 🔬 Test expansion target |
| Untested – live | `src/rendering/SoRenderManager.cpp` | 🔬 Test expansion target |
| Untested – live | `src/engines/SoConvertAll.cpp` | 🔬 Test expansion target |
| Untested – live | `src/caches/earcut.hpp` | 🔬 Test expansion target |
| Untested – live | `src/misc/SoProto.cpp` (error paths) | 🔬 Test expansion target |
| Untested – live | `src/actions/SoGLRenderAction.cpp` | 🔬 Test expansion target |

---

*See `COVERAGE.md` for full per-subsystem line-coverage tables and
`COVERAGE_PLAN.md` for a prioritised plan to expand test coverage further.*
