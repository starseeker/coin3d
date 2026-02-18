# Generating Control Images - Setup Guide

## Overview

Control images serve as reference baselines for image comparison testing. They must be generated on a system with working OpenGL rendering capabilities.

## Prerequisites

### Linux

1. **X11 Server** - For headless operation, use Xvfb:
   ```bash
   sudo apt-get install xvfb
   ```

2. **OpenGL Libraries**:
   ```bash
   sudo apt-get install libgl1-mesa-dev libglu1-mesa-dev
   sudo apt-get install libgl1-mesa-dri
   ```

3. **Coin Library** - Built with offscreen rendering support

### macOS

OpenGL should work out of the box. No special setup needed.

### Windows

Ensure OpenGL drivers are installed and up to date.

## Generation Process

### Step 1: Build Examples

```bash
cd ivexamples/Mentor-headless
mkdir build
cd build
cmake ..
make
```

### Step 2: Start Xvfb (Linux only)

```bash
# Start Xvfb on display :99
Xvfb :99 -screen 0 1024x768x24 &
export DISPLAY=:99

# Verify GLX is working
glxinfo | head -10
```

### Step 3: Set Library Path

```bash
# Ensure Coin library can be found
export LD_LIBRARY_PATH=/path/to/coin/build/lib:$LD_LIBRARY_PATH
```

### Step 4: Generate Control Images

```bash
cd ivexamples/Mentor-headless
BUILD_DIR=build CONTROL_DIR=control_images ./generate_control_images.sh
```

Or use the CMake target:

```bash
cd build
make generate_controls
```

### Step 5: Verify Control Images

Check that images were created:

```bash
ls -lh control_images/*.rgb
```

You should see 53 `.rgb` files, one for each example.

## Troubleshooting

### GLX Context Creation Failed

**Symptom**: `Coin warning in glxglue_context_create_software(): Couldn't create GLX context.`

**Solutions**:

1. **Check Xvfb is running**:
   ```bash
   ps aux | grep Xvfb
   ```

2. **Verify DISPLAY is set**:
   ```bash
   echo $DISPLAY
   ```

3. **Test GLX**:
   ```bash
   glxinfo
   ```

4. **Try software rendering**:
   ```bash
   export LIBGL_ALWAYS_SOFTWARE=1
   ```

5. **Install Mesa DRI drivers**:
   ```bash
   sudo apt-get install libgl1-mesa-dri
   ```

### Segmentation Faults

Some examples may fail if:
- Coin library is not properly linked
- Data files are missing (for examples that load external models)
- OpenGL context creation fails

Check the error messages for specific issues.

### Missing Data Files

Examples like `11.1.ReadFile` need data files from the `data/` directory. Ensure:
```bash
export IVEXAMPLES_DATA_DIR=/path/to/ivexamples/Mentor-headless/data
```

## CI/CD Environments

Generating control images in CI/CD environments (like GitHub Actions) can be challenging due to:
- Limited or no GPU access
- Software rendering limitations
- Docker container restrictions

### Recommended Approach

1. **Generate locally** - Create control images on a development machine with proper OpenGL support
2. **Commit to repository** - Add control images to version control
3. **Test in CI** - Use committed control images for regression testing

### Alternative: Use EGL

If GLX doesn't work, you may try EGL (if Coin was built with EGL support):

```bash
export COIN_USE_EGL=1
```

## Image Format

Control images are in SGI RGB format:
- Uncompressed, simple format
- Natively supported by Coin
- Can be converted using ImageMagick:
  ```bash
  convert input.rgb output.png
  ```

## Updating Control Images

Control images should be updated when:

1. **Intentional visual changes** - After verified rendering improvements
2. **Platform updates** - When baseline rendering behavior changes
3. **Resolution changes** - If output dimensions are modified

To update:
```bash
# Regenerate all control images
cd ivexamples/Mentor-headless
BUILD_DIR=build CONTROL_DIR=control_images ./generate_control_images.sh

# Commit updated images
git add control_images/*.rgb
git commit -m "Update control images after rendering improvement"
```

## Best Practices

1. **Consistent Environment** - Generate all control images on the same system
2. **Document Changes** - Note any rendering changes when updating controls
3. **Version Control** - Commit control images to track rendering evolution
4. **Platform Baselines** - Consider separate controls for different platforms if needed

## Example: Manual Control Generation

If automated generation fails, you can generate controls manually:

```bash
cd build/bin

# Set up environment
export DISPLAY=:99
export LD_LIBRARY_PATH=/path/to/coin/lib:$LD_LIBRARY_PATH

# Generate individual control images
./02.1.HelloCone ../../control_images/02.1.HelloCone_control.rgb
./02.2.EngineSpin ../../control_images/02.2.EngineSpin_control.rgb
./03.1.Molecule ../../control_images/03.1.Molecule_control.rgb
# ... repeat for all examples
```

## Verification

After generating control images, verify they're usable:

```bash
# Check file sizes (should be non-zero)
ls -lh control_images/*.rgb

# Try viewing one (convert to PNG first)
convert control_images/02.1.HelloCone_control.rgb /tmp/test.png
display /tmp/test.png  # or open with image viewer

# Test the comparator
./bin/image_comparator --verbose \
    control_images/02.1.HelloCone_control.rgb \
    control_images/02.1.HelloCone_control.rgb
# Should report: Images are pixel-perfect match
```
