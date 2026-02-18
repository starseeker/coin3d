# Control Images

This directory contains reference images for testing the headless Mentor examples.

## Purpose

These control images are used for regression testing to ensure that:
1. The rendering output remains consistent across builds
2. Changes to the Coin library don't introduce visual regressions
3. Platform differences in rendering are acceptable within defined thresholds

## Generating Control Images

To generate control images, first build the examples:

```bash
cd ivexamples/Mentor-headless
mkdir build
cd build
cmake ..
make
```

Then run the generation script:

```bash
cd ..
BUILD_DIR=build ./generate_control_images.sh
```

Or use the CMake target:

```bash
cd build
make generate_controls
```

This will create `*_control.rgb` files in this directory.

## Image Format

Control images are in SGI RGB format (`.rgb`), which is natively supported by Coin's 
`SoOffscreenRenderer::writeToRGB()` method. This format:

- Does not require external image libraries
- Is simple and well-defined
- Can be converted to other formats using ImageMagick if needed:
  ```bash
  convert input.rgb output.png
  ```

## Naming Convention

Control images follow the naming pattern: `<example_name>_control.rgb`

Examples:
- `02.1.HelloCone_control.rgb` - Control image for HelloCone example
- `03.1.Molecule_control.rgb` - Control image for Molecule example
- `15.3.AttachManip_control.rgb` - Control image for AttachManip example

## Image Comparison

The images are compared using the `image_comparator` utility, which provides:

1. **Pixel-perfect comparison** - Exact byte-for-byte match
2. **Perceptual hash comparison** - Approximate match using perceptual hashing
3. **RMSE comparison** - Root Mean Square Error between images

The comparison thresholds can be configured at CMake configure time:

```bash
cmake -DIMAGE_COMPARISON_HASH_THRESHOLD=10 \
      -DIMAGE_COMPARISON_RMSE_THRESHOLD=8.0 ..
```

### Threshold Guidelines

**Hash Threshold** (0-64):
- 0 = Pixel-perfect match required
- 1-4 = Very strict (minor rendering differences, anti-aliasing)
- 5-9 = Moderate (font rendering variations, OpenGL driver differences)
- 10-19 = Tolerant (acceptable for cross-platform testing)
- 20+ = Very tolerant (major differences acceptable)

**RMSE Threshold** (0-255):
- 0.0 = Pixel-perfect match required
- 0.1-4.9 = Very strict (minimal color/brightness variations)
- 5.0-9.9 = Moderate (acceptable rendering differences)
- 10.0-19.9 = Tolerant (noticeable but acceptable differences)
- 20.0+ = Very tolerant (significant differences acceptable)

## Updating Control Images

Control images should be updated when:

1. **Intentional visual changes** - When rendering improvements or fixes change the expected output
2. **Platform changes** - When baseline rendering behavior changes (e.g., OpenGL driver updates)
3. **Resolution changes** - When output image size is modified

To update control images, regenerate them using the script above.

## Testing

To run the image comparison tests:

```bash
cd build
ctest
```

Or to run specific tests:

```bash
ctest -R "02.1.HelloCone"
ctest -R "Molecule"
```

To see verbose output:

```bash
ctest -V
```

## Version Control

Control images should be committed to version control when:
- Initial generation for new examples
- Intentional updates after verified visual changes
- Platform-specific baselines (if needed)

Large binary files may benefit from Git LFS if the repository uses it.
