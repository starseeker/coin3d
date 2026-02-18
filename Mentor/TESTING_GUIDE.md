# Testing Guide for Headless Examples

This guide explains how to use the image comparison testing framework for the headless Inventor Mentor examples.

## Overview

The testing framework provides:
- **Automated regression testing** - Verify rendering output hasn't changed
- **Configurable comparison** - Adjust sensitivity for different platforms
- **CTest integration** - Standard CMake testing workflow
- **Detailed reporting** - Know exactly what differs when tests fail

## Quick Start

### 1. Build Everything

```bash
cd ivexamples/Mentor-headless
mkdir build
cd build
cmake ..
make
```

### 2. Generate Control Images

See [CONTROL_IMAGE_GENERATION.md](CONTROL_IMAGE_GENERATION.md) for detailed instructions.

```bash
cd ..
BUILD_DIR=build ./generate_control_images.sh
```

Or use CMake:
```bash
cd build
make generate_controls
```

### 3. Run Tests

```bash
cd build
ctest
```

## Configuration

### Comparison Thresholds

Control how strict the image comparison is at CMake configure time:

```bash
cmake -DIMAGE_COMPARISON_HASH_THRESHOLD=10 \
      -DIMAGE_COMPARISON_RMSE_THRESHOLD=8.0 ..
```

**IMAGE_COMPARISON_HASH_THRESHOLD** (default: 5, range: 0-64):
- **0-2**: Extremely strict - nearly pixel-perfect required
- **3-5**: Strict - minor rendering differences only (default)
- **6-10**: Moderate - handles font/anti-aliasing differences
- **11-20**: Tolerant - cross-platform testing
- **20+**: Very tolerant - major differences acceptable

**IMAGE_COMPARISON_RMSE_THRESHOLD** (default: 5.0, range: 0-255):
- **0-2**: Extremely strict - minimal color variation
- **3-5**: Strict - small rendering variations (default)
- **6-10**: Moderate - noticeable but acceptable differences
- **11-20**: Tolerant - significant differences acceptable
- **20+**: Very tolerant - large differences acceptable

### When to Adjust Thresholds

**Stricter thresholds** (lower values):
- Development on a single platform
- Verifying exact rendering output
- Detecting subtle regressions

**Tolerant thresholds** (higher values):
- Cross-platform testing (Windows/Linux/macOS)
- Different OpenGL implementations
- Different GPU/driver combinations
- Font rendering differences

## Running Tests

### Run All Tests

```bash
cd build
ctest
```

### Run Specific Tests

```bash
# By name pattern
ctest -R "HelloCone"
ctest -R "Molecule"

# By chapter
ctest -R "^02\."  # All chapter 2 examples
ctest -R "^15\."  # All chapter 15 examples

# Run a specific test
ctest -R "02.1.HelloCone_test"
```

### Verbose Output

```bash
# Standard verbose
ctest -V

# Extra verbose
ctest -VV

# Show test output on failure only
ctest --output-on-failure

# Rerun failed tests
ctest --rerun-failed --output-on-failure
```

### Parallel Execution

```bash
# Run up to 4 tests in parallel
ctest -j4
```

## Understanding Test Results

### Test Pass

```
Test #1: 02.1.HelloCone_test .......   Passed    0.15 sec
```

The test passed - generated image matches control within thresholds.

### Test Fail - Image Difference

```
Test #1: 02.1.HelloCone_test .......***Failed    0.15 sec
```

Run with `-V` to see comparison details:
```
Perceptual hash distance: 12 (threshold: 5)
RMSE: 8.50 (threshold: 5.00)
Images differ beyond threshold
```

This means the generated image differs from control. Either:
1. A rendering regression occurred
2. Thresholds are too strict for your platform
3. Control images need updating

### Test Fail - Execution Error

```
Example execution failed with code 1
Error: Failed to render scene
```

The example itself failed to run. Common causes:
- OpenGL context creation failed
- Missing data files
- Library linking issues

### Test Skip

If no control image exists for an example, the test is automatically skipped.

## Manual Image Comparison

For detailed investigation, use the comparator directly:

```bash
./bin/image_comparator --verbose \
    ../control_images/02.1.HelloCone_control.rgb \
    test_output/02.1.HelloCone_test.rgb
```

Options:
```bash
# Require pixel-perfect match
./bin/image_comparator --strict control.rgb test.rgb

# Custom thresholds
./bin/image_comparator --threshold 10 --rmse 8.0 control.rgb test.rgb

# Show detailed metrics
./bin/image_comparator --verbose control.rgb test.rgb
```

## Troubleshooting

### No Tests Found

**Symptom**: `ctest` reports `Total Tests: 0`

**Cause**: No control images exist

**Solution**: Generate control images first (see [CONTROL_IMAGE_GENERATION.md](CONTROL_IMAGE_GENERATION.md))

### All Tests Failing

**Symptom**: Every test fails with "Images differ beyond threshold"

**Possible Causes**:
1. **Wrong platform** - Control images generated on different platform
2. **Too strict** - Thresholds too low for your environment
3. **Rendering changes** - Intentional changes made, controls need updating

**Solutions**:
1. Increase thresholds: `-DIMAGE_COMPARISON_HASH_THRESHOLD=15`
2. Generate new platform-specific controls
3. Update control images if changes are intentional

### Execution Failures

**Symptom**: Tests fail with "Example execution failed"

**Common Causes**:
1. **No OpenGL context** - Missing Xvfb or display
2. **Library not found** - LD_LIBRARY_PATH not set
3. **Missing data files** - IVEXAMPLES_DATA_DIR not set

**Solutions**:
```bash
# Start Xvfb (Linux)
Xvfb :99 -screen 0 1024x768x24 &
export DISPLAY=:99

# Set library path
export LD_LIBRARY_PATH=/path/to/coin/build/lib:$LD_LIBRARY_PATH

# Set data directory (for examples that load external files)
export IVEXAMPLES_DATA_DIR=/path/to/ivexamples/Mentor-headless/data
```

## CI/CD Integration

### GitHub Actions Example

```yaml
name: Test Headless Examples

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      
      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y cmake build-essential
          sudo apt-get install -y libx11-dev libgl1-mesa-dev xvfb
      
      - name: Build Coin
        run: |
          mkdir build && cd build
          cmake .. -DCOIN_BUILD_TESTS=OFF
          make -j$(nproc)
      
      - name: Build Examples
        run: |
          cd ivexamples/Mentor-headless
          mkdir build && cd build
          cmake .. -DIMAGE_COMPARISON_HASH_THRESHOLD=10
          make -j$(nproc)
      
      - name: Run Tests
        run: |
          cd ivexamples/Mentor-headless/build
          Xvfb :99 -screen 0 1024x768x24 &
          export DISPLAY=:99
          export LD_LIBRARY_PATH=../../../build/lib:$LD_LIBRARY_PATH
          ctest --output-on-failure
```

### Important Notes for CI

1. **Commit control images** - Generate locally and commit to repository
2. **Use tolerant thresholds** - CI environments vary, use higher thresholds
3. **Software rendering** - May need `LIBGL_ALWAYS_SOFTWARE=1`
4. **Xvfb required** - Linux CI needs virtual framebuffer

## Best Practices

### Development Workflow

1. **Make changes** to Coin or examples
2. **Build** everything
3. **Run tests** to check for regressions
4. **Investigate failures** - are they expected?
5. **Update controls** if changes are intentional
6. **Commit** updated controls with your changes

### Control Image Management

- **Version control** - Always commit control images
- **Platform variants** - Consider separate controls per platform if needed
- **Document changes** - Note why controls were updated in commits
- **Regular updates** - Regenerate periodically to catch drift

### Platform Differences

Different platforms may produce slightly different rendering:

| Platform | Typical Differences | Recommended Thresholds |
|----------|-------------------|------------------------|
| Same machine | Minimal | Hash: 0-4, RMSE: 0-4.9 |
| Same OS, different GPU | Minor anti-aliasing | Hash: 5-9, RMSE: 5.0-7.9 |
| Different OS | Font rendering, colors | Hash: 10-19, RMSE: 8.0-14.9 |
| Different drivers | OpenGL implementations | Hash: 15-25, RMSE: 10.0-20.0 |

## Example Workflows

### Workflow 1: Initial Setup

```bash
# Build everything
cd ivexamples/Mentor-headless
mkdir build && cd build
cmake ..
make -j$(nproc)

# Generate controls
cd ..
BUILD_DIR=build ./generate_control_images.sh

# Verify
cd build
ctest
```

### Workflow 2: Cross-Platform Testing

```bash
# Configure with tolerant thresholds
cmake -DIMAGE_COMPARISON_HASH_THRESHOLD=15 \
      -DIMAGE_COMPARISON_RMSE_THRESHOLD=10.0 ..

# Build and test
make -j$(nproc)
ctest --output-on-failure
```

### Workflow 3: Investigating Failures

```bash
# Run single test verbosely
ctest -R "02.1.HelloCone_test" -V

# Compare images manually
./bin/image_comparator --verbose \
    ../control_images/02.1.HelloCone_control.rgb \
    test_output/02.1.HelloCone_test.rgb

# Convert to viewable format
convert test_output/02.1.HelloCone_test.rgb test.png
convert ../control_images/02.1.HelloCone_control.rgb control.png

# View side by side
display test.png control.png
```

### Workflow 4: Updating Controls After Changes

```bash
# Make your changes to Coin or examples
# ...

# Rebuild
cd build
make -j$(nproc)

# Regenerate ALL control images
cd ..
BUILD_DIR=build ./generate_control_images.sh

# Verify tests pass
cd build
ctest

# Commit updated controls
cd ..
git add control_images/*.rgb
git commit -m "Update control images after rendering improvement"
```

## Advanced Topics

### Custom Test Scripts

You can create custom test scripts using `run_image_test.cmake` as a template:

```cmake
add_test(
    NAME custom_test
    COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:my_example>
        -DTEST_IMAGE=${CMAKE_CURRENT_BINARY_DIR}/my_output.rgb
        -DCONTROL_IMAGE=${CMAKE_CURRENT_SOURCE_DIR}/my_control.rgb
        -DCOMPARATOR=$<TARGET_FILE:image_comparator>
        -DHASH_THRESHOLD=10
        -DRMSE_THRESHOLD=8.0
        -P ${CMAKE_CURRENT_SOURCE_DIR}/run_image_test.cmake
)
```

### Multiple Control Sets

For truly platform-specific testing, you could maintain separate control directories:

```bash
control_images/
  linux/
    02.1.HelloCone_control.rgb
    ...
  macos/
    02.1.HelloCone_control.rgb
    ...
  windows/
    02.1.HelloCone_control.rgb
    ...
```

Then select the appropriate set in CMakeLists.txt based on platform.

## Getting Help

If tests are failing unexpectedly:

1. **Check the logs**: `Testing/Temporary/LastTest.log`
2. **Run with verbose output**: `ctest -VV`
3. **Try manual comparison**: Use `image_comparator` directly
4. **Adjust thresholds**: Try more tolerant values
5. **Verify environment**: Check OpenGL context, library paths, etc.

For issues with the testing framework itself, check:
- CMakeLists.txt configuration
- run_image_test.cmake script
- image_comparator.cpp implementation
