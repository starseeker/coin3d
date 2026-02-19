# OpenGL Backend Selection Guide for Mentor Examples

## Overview

The Mentor examples can now be built to work with either **OSMesa** (headless offscreen rendering) or **system OpenGL** (platform-specific rendering), depending on how the parent Coin library is compiled. This flexibility allows for:

1. **Headless CI/CD environments** - Generate test images without display servers using OSMesa
2. **Interactive development** - Use system OpenGL with display servers for visual debugging
3. **Image comparison testing** - Generate reference images with both backends for validation

## Build Configuration

### Parent Coin Library Configuration

The Mentor examples automatically detect and adapt to how Coin was compiled:

```bash
# Option 1: Build Coin with OSMesa (headless rendering)
cmake -DCOIN3D_USE_OSMESA=ON -DCOIN_BUILD_TESTS=ON ..
make

# Option 2: Build Coin with system OpenGL (requires display)
cmake -DCOIN3D_USE_OSMESA=OFF -DCOIN_BUILD_TESTS=ON ..
make
```

**Note:** The `COIN_BUILD_TESTS=ON` option is required to build the Mentor examples.

### Standalone Mentor Examples Build

If building the Mentor examples standalone (without building Coin from source):

```bash
cd Mentor
mkdir build && cd build

# Option 1: For OSMesa-built Coin installation
cmake -DEXAMPLES_USE_OSMESA=ON ..
make

# Option 2: For system OpenGL-built Coin installation
cmake -DEXAMPLES_USE_OSMESA=OFF ..
make
```

## Backend Differences

| Feature | OSMesa Backend | System OpenGL Backend |
|---------|----------------|----------------------|
| **Display Server** | Not required | Required (X11, Wayland, etc.) |
| **Use Case** | Headless servers, CI/CD | Interactive applications |
| **GL Headers** | `<OSMesa/gl.h>` | `<GL/gl.h>` |
| **Context Creation** | Software buffer-based | Platform-specific (GLX/WGL/CGL) |
| **Preprocessor Define** | `COIN3D_OSMESA_BUILD` | Not defined |
| **Performance** | CPU rendering | GPU-accelerated (if available) |
| **Image Quality** | Consistent across platforms | May vary by driver/hardware |

## How It Works

### Conditional Compilation in `headless_utils.h`

The header file uses preprocessor directives to adapt:

```cpp
#ifdef COIN3D_OSMESA_BUILD
    // OSMesa-specific context management
    #include <OSMesa/osmesa.h>
    class CoinHeadlessContextManager : public SoDB::ContextManager {
        // OSMesa implementation
    };
#else
    // System OpenGL stub (warns that display server is required)
    class CoinHeadlessContextManager : public SoDB::ContextManager {
        // Stub implementation with warning
    };
#endif
```

### CMake Configuration Detection

The `CMakeLists.txt` automatically detects the parent configuration:

```cmake
if(DEFINED COIN3D_USE_OSMESA)
    # Building as subdirectory - use parent's configuration
    set(EXAMPLES_USE_OSMESA ${COIN3D_USE_OSMESA})
else()
    # Building standalone - use cache variable
    set(EXAMPLES_USE_OSMESA ON CACHE BOOL "...")
endif()
```

## Running Examples

### With OSMesa Backend

```bash
cd build/Mentor
./bin/02.1.HelloCone output.rgb
# Output: "Coin examples initialized with OSMesa backend for headless rendering"
```

No display server required - works in CI/CD, Docker containers, headless servers.

### With System OpenGL Backend

```bash
# Ensure display server is running
export DISPLAY=:0  # or use Xvfb for virtual display

cd build/Mentor
./bin/02.1.HelloCone output.rgb
# Output: "Coin examples initialized with system OpenGL backend (requires display)"
```

**Note:** System OpenGL builds require an active display server. For headless environments, use Xvfb:

```bash
Xvfb :99 -screen 0 1024x768x24 &
export DISPLAY=:99
./bin/02.1.HelloCone output.rgb
```

## Image Comparison Testing

### Generating Reference Images with Both Backends

To compare rendering quality between OSMesa and system OpenGL:

```bash
# 1. Build and test with OSMesa
cd /path/to/coin
rm -rf build_osmesa && mkdir build_osmesa && cd build_osmesa
cmake -DCOIN3D_USE_OSMESA=ON -DCOIN_BUILD_TESTS=ON ..
make
cd Mentor
./bin/02.1.HelloCone osmesa_output.rgb

# 2. Build and test with system OpenGL (on a system with display)
cd /path/to/coin
rm -rf build_opengl && mkdir build_opengl && cd build_opengl
cmake -DCOIN3D_USE_OSMESA=OFF -DCOIN_BUILD_TESTS=ON ..
make
cd Mentor
./bin/02.1.HelloCone opengl_output.rgb

# 3. Compare images
cd Mentor
./bin/image_comparator --verbose \
    ../../build_osmesa/Mentor/osmesa_output.rgb \
    ../../build_opengl/Mentor/opengl_output.rgb
```

### Expected Differences

When comparing OSMesa vs system OpenGL renderings, expect minor differences:

- **Anti-aliasing** - System OpenGL may have different MSAA settings
- **Line width** - May vary slightly between implementations
- **Texture filtering** - Different filtering algorithms
- **Floating-point precision** - Minor numerical differences

Use the image comparator's threshold settings to account for these:

```bash
./bin/image_comparator --threshold 10 --rmse 5.0 image1.rgb image2.rgb
```

## Troubleshooting

### "Warning: Examples compiled with system OpenGL build"

**Problem:** Examples built with system OpenGL are trying to run in headless environment.

**Solution:** Rebuild Coin with OSMesa support:
```bash
cmake -DCOIN3D_USE_OSMESA=ON -DCOIN_BUILD_TESTS=ON ..
```

### "Could NOT find OpenGL" during CMake configuration

**Problem:** System OpenGL development files not installed.

**Solutions:**
- Install OpenGL dev packages: `apt-get install libgl1-mesa-dev libglu1-mesa-dev`
- Or build with OSMesa instead: `-DCOIN3D_USE_OSMESA=ON`

### Examples fail with "Failed to create OpenGL context"

**Problem:** No display server available for system OpenGL build.

**Solutions:**
- Use Xvfb for virtual display: `Xvfb :99 & export DISPLAY=:99`
- Or rebuild with OSMesa for true headless operation

### OSMesa examples show "GL_INVALID_OPERATION" warning

This is a benign warning during context initialization and can be safely ignored. It occurs during normal OSMesa context cleanup.

## CI/CD Integration

### GitHub Actions Example

```yaml
name: Test Examples
on: [push, pull_request]

jobs:
  test-osmesa:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
        with:
          submodules: recursive
      
      - name: Build Coin with OSMesa
        run: |
          mkdir build && cd build
          cmake -DCOIN3D_USE_OSMESA=ON -DCOIN_BUILD_TESTS=ON ..
          make -j$(nproc)
      
      - name: Run Mentor Examples
        run: |
          cd build/Mentor
          ./bin/02.1.HelloCone test.rgb
          ./bin/02.2.EngineSpin test.rgb
      
      - name: Upload Images
        uses: actions/upload-artifact@v3
        with:
          name: rendered-images
          path: build/Mentor/*.rgb
```

## Implementation Details

### Context Manager Pattern

Both backends implement the `SoDB::ContextManager` interface:

```cpp
class SoDB::ContextManager {
public:
    virtual void* createOffscreenContext(unsigned int width, unsigned int height) = 0;
    virtual SbBool makeContextCurrent(void* context) = 0;
    virtual void restorePreviousContext(void* context) = 0;
    virtual void destroyContext(void* context) = 0;
};
```

This abstraction allows Coin to work with any rendering backend that implements these four methods.

### OSMesa Implementation

```cpp
struct CoinOSMesaContext {
    OSMesaContext context;
    std::unique_ptr<unsigned char[]> buffer;  // RGBA buffer
    int width, height;
};

// Creates offscreen buffer, no display needed
OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, NULL);
```

### System OpenGL Stub

The system OpenGL implementation is a stub that warns users - this is intentional because:

1. System OpenGL context creation is platform-specific (GLX/WGL/CGL)
2. Examples are designed for headless operation
3. Users wanting system OpenGL should use Coin's built-in context management

## Best Practices

1. **Use OSMesa for CI/CD** - Consistent, headless, cross-platform
2. **Use system OpenGL for debugging** - Visual feedback, GPU acceleration
3. **Compare both backends** - Validate rendering correctness
4. **Set appropriate thresholds** - Account for implementation differences
5. **Document your choice** - Make it clear which backend your tests require

## Future Enhancements

Possible future improvements:

- Full GLX/WGL/CGL context managers for system OpenGL examples
- Automatic backend detection from installed Coin
- Runtime backend switching
- Performance profiling between backends
- Extended image comparison metrics

## Related Documentation

- [README.md](README.md) - Main Mentor examples documentation
- [TESTING_GUIDE.md](TESTING_GUIDE.md) - Testing and validation procedures
- [STATUS.md](STATUS.md) - Status of all converted examples
- Main Coin README - Build configuration options
