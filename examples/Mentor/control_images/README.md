# Control Images

This directory contains reference (control) images for regression testing
of the headless Mentor examples.

## Image Format

Control images are stored as **PNG** (`.png`) for compact repository storage.
PNG encoding is **lossless**: the exact RGB pixel values from the original
SGI RGB rendering are preserved and can be recovered bitwise.

- Each PNG contains 8-bit RGB pixel data (no alpha channel)
- Rows are stored top-to-bottom (standard PNG convention)
- Original SGI RGB data was rendered at **800×600 pixels**

The runtime test generates SGI RGB images (Coin's native offscreen format).
The `image_comparator` utility can read both `.rgb` and `.png` files and
decodes them to the same interleaved RGB representation for comparison.

## Naming Convention

Images follow a two-part naming scheme:

| Pattern | Meaning |
|---------|---------|
| `{example_name}_control.png` | Primary (canonical) view for the example |
| `{example_name}_{view_suffix}_control.png` | Additional view for multi-view examples |

Examples:
- `02.1.HelloCone_control.png` — single view for HelloCone
- `03.1.Molecule_front_control.png` — front view for Molecule
- `03.1.Molecule_side_control.png` — side view for Molecule
- `02.2.EngineSpin_frame00_control.png` — frame 0 of the spinning animation

For **regression tests**, only the primary `{example_name}_control.png` is
used. Multi-view images provide a complete reference series.

## Generation

Control images are generated using `generate_control_images.sh`:

```bash
# Build Coin and the examples first:
cd <repo_root>
mkdir build && cd build && cmake .. && make
cd ../ivexamples/Mentor-headless
mkdir build && cd build && cmake .. && make
cd ..

# Generate control images (requires Xvfb for headless rendering):
BUILD_DIR=build DISPLAY=:99 ./generate_control_images.sh
```

The script:
1. Starts Xvfb if `DISPLAY` is not set
2. Runs each example with `COIN_GLX_PIXMAP_DIRECT_RENDERING=1` and
   `COIN_GLXGLUE_NO_PBUFFERS=1` for Xvfb compatibility
3. Converts each SGI RGB output to PNG via `rgb_to_png`
4. Copies the first generated view as the canonical `_control.png`

## Round-Trip Fidelity

The PNG files can be converted back to SGI RGB for bit-exact comparison:
- PNG stores the same interleaved RGB bytes that Coin renders
- `image_comparator` reads PNG and SGI RGB to the same internal format
- A re-rendered SGI RGB image compares as **pixel-perfect** against the PNG

## Coverage

- **49 of 55** examples generate control images
- **6 examples** (chapters 14–15 manipulator examples) currently segfault and
  have no control images
- Examples generating multiple views also have a canonical `_control.png`

## Updating Control Images

Update when there is an **intentional** rendering change or new example:

```bash
# Remove old images and regenerate:
rm control_images/*.png
BUILD_DIR=build ./generate_control_images.sh
```

## Testing

Run regression tests (requires control images to exist):

```bash
cd ivexamples/Mentor-headless/build
DISPLAY=:99 \
  COIN_GLX_PIXMAP_DIRECT_RENDERING=1 \
  COIN_GLXGLUE_NO_PBUFFERS=1 \
  ctest -V
```
