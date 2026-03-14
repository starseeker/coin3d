# Inventor Mentor Headless Examples

Headless, offscreen-rendering adaptations of the examples from *The Inventor
Mentor* (Wernecke, 1994).  The examples are adapted from the original SGI
source code (LGPL 2.1) and demonstrate Obol (Coin) scene-graph features
without requiring a GUI toolkit.

## Purpose

- Verify that core Obol features work correctly and produce stable images.
- Serve as visual regression tests via CTest image-comparison.
- Illustrate how to use `SoOffscreenRenderer` and the headless context-manager
  infrastructure found in `tests/utils/`.

## Coverage

58 of the 66 Mentor examples are converted (88%).  The remaining 8 examples
test Motif/Xt/GLX toolkit-specific integration and cannot be converted
meaningfully.

| Chapters | Topic | Status |
|----------|-------|--------|
| 2–7 | Scene graphs, geometry, materials, cameras, lights, text, textures | ✅ complete |
| 9 | Actions (print, search, pick, callback) | ✅ complete |
| 10 | Events and selection | ✅ 6 of 8 (2 Motif-only) |
| 11 | File I/O | ✅ complete |
| 12–13 | Sensors and engines | ✅ complete |
| 14 | NodeKits | ✅ complete |
| 15 | Draggers / manipulators | ✅ complete |
| 16 | Component integration | ✅ 2 of 5 (3 Motif/GLX-only) |
| 17 | OpenGL callback integration | ✅ 1 of 3 (2 Xt-only) |
| 8 | NURBS curves and surfaces | ❌ skipped (SoNurbs* not in this fork) |

## Building

The Mentor examples are built automatically when Obol is configured with
`-DOBOL_BUILD_TESTS=ON`.  They inherit the parent build's GL backend
(OSMesa or system OpenGL).

```bash
# From the Obol top-level source tree:
cmake -S . -B build -DOBOL_BUILD_TESTS=ON [-DOBOL_USE_OSMESA=ON]
cmake --build build -- -j$(nproc)
```

## Running the Tests

```bash
cd build
ctest -R "Mentor"          # run Mentor image-comparison tests only
ctest --output-on-failure  # run all tests
```

On a headless system without a real display the build system automatically
wraps each test with `xvfb-run` when the `DISPLAY` environment variable is
not set.

## Generating Control Images

Control (reference) PNG images live in `control_images/`.  To regenerate
them from the current build:

```bash
cmake --build build --target generate_mentor_controls
```

Or directly from the script (requires a display or Xvfb):

```bash
BUILD_DIR=build examples/Mentor/generate_control_images.sh
```

## Shared Utilities

These examples use the shared utility code from `tests/utils/`:

| File | Purpose |
|------|---------|
| `headless_utils.h` | `initCoinHeadless()`, `renderToFile()`, camera/light helpers |
| `image_comparator.cpp` | Perceptual-hash + RMSE image comparison tool |
| `rgb_to_png.cpp` | Convert SGI RGB files to PNG for repository storage |

`mock_gui_toolkit.h` (local to this directory) provides lightweight stubs
for the Xt/Motif concepts used in a handful of examples (Chapters 10, 14, 16)
that demonstrate toolkit-agnostic event-handling patterns.

## Image Comparison Thresholds

The default RMSE threshold for Mentor tests is 25.0 (versus 5.0 for the core
rendering regression suite) to accommodate minor rendering differences that
arise across driver and platform variants.  Per-test overrides are listed in
`CMakeLists.txt`.

## License

The original Inventor Mentor example code is copyright © 2000 Silicon
Graphics, Inc. and distributed under the GNU Lesser General Public License
v2.1 or later.  See `LICENSE` in this directory.

Headless-adaptation changes and new test infrastructure are copyright ©
Kongsberg Oil & Gas Technologies AS and are also licensed under LGPL 2.1+.
