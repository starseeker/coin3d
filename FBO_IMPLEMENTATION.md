# FBO-based Offscreen Rendering Implementation

This implementation replaces platform-specific offscreen rendering code with a portable GL_EXT_framebuffer_object based solution.

## Files Modified/Added

### New Files:
- `src/glue/gl_fbo.h` - FBO offscreen rendering header
- `src/glue/gl_fbo.cpp` - FBO offscreen rendering implementation  
- `tests/rendering/test_fbo.cpp` - Unit tests for FBO functionality
- `tests/offscreen_rendering_test.cpp` - Integration test (requires OpenGL context)

### Modified Files:
- `src/glue/gl.cpp` - Added FBO support and environment variable control
- `src/rendering/CoinOffscreenGLCanvas.cpp` - Try FBO first when available
- `include/Inventor/C/glue/gl.h` - Added public API function
- `src/C/glue/gl.h` - Added public API function  
- `src/glue/CMakeLists.txt` - Added FBO source files
- `tests/CMakeLists.txt` - Added FBO tests

## Key Features

### 1. Portable FBO Implementation
- Uses GL_EXT_framebuffer_object for cross-platform compatibility
- Supports RGBA8 color buffer and 24-bit depth buffer
- Proper framebuffer completeness validation
- Memory management with automatic cleanup

### 2. Environment Variable Control
- `COIN_USE_FBO_OFFSCREEN`: Enable/disable FBO offscreen rendering (default: enabled)
- `COIN_DEBUG_FBO`: Enable debug output for troubleshooting

### 3. Graceful Fallback
- Falls back to platform-specific implementations when FBO unavailable
- No performance impact when FBO is not supported
- Maintains compatibility with existing code

### 4. API Integration
- `cc_glglue_setup_fbo_offscreen_if_available()` - Setup FBO when available
- Integrates with existing offscreen callback system
- Works with `CoinOffscreenGLCanvas` and `SoOffscreenRenderer`

## How It Works

1. **Initialization**: `CoinOffscreenGLCanvas::tryActivateGLContext()` calls `cc_glglue_setup_fbo_offscreen_if_available()`

2. **FBO Detection**: Checks for GL_EXT_framebuffer_object support in current context

3. **Callback Setup**: If FBO available and enabled, sets FBO callbacks as offscreen provider

4. **Context Creation**: When offscreen context needed:
   - If FBO callbacks set, uses `fbo_context_create_offscreen()`
   - Otherwise falls back to platform-specific implementations

5. **Rendering**: FBO provides render target that works with existing rendering pipeline

## Platform Support

### Replaced Platform-Specific Code:
- **GLX (Linux)**: `src/glue/gl_glx.cpp` - P-buffer and software contexts
- **WGL (Windows)**: `src/glue/gl_wgl.cpp` - P-buffer and device contexts  
- **CGL (macOS)**: `src/glue/gl_cgl.cpp` - P-buffer contexts
- **AGL (macOS legacy)**: `src/glue/gl_agl.cpp` - Legacy offscreen contexts

### FBO Advantages:
- **Portable**: Works on any platform with GL_EXT_framebuffer_object
- **Modern**: Uses standard OpenGL extension (core in OpenGL 3.0+)
- **Efficient**: Direct GPU rendering without CPU copies
- **Flexible**: Easy to extend for different formats/attachments

## Testing

### Unit Tests (tests/rendering/test_fbo.cpp):
- API availability and safety
- Environment variable handling  
- Graceful failure without OpenGL context
- Memory safety and error handling

### Integration Test (tests/offscreen_rendering_test.cpp):
- Renders complex scene (sphere + cone) 
- Compares FBO vs traditional rendering
- Validates output image quality
- Requires actual OpenGL context to run

## Environment Variables

```bash
# Enable FBO offscreen rendering (default)
export COIN_USE_FBO_OFFSCREEN=1

# Disable FBO, use platform-specific fallback  
export COIN_USE_FBO_OFFSCREEN=0

# Enable FBO debug output
export COIN_DEBUG_FBO=1
```

## Usage Example

```cpp
#include <Inventor/SoOffscreenRenderer.h>

// FBO will be used automatically if available
SoOffscreenRenderer renderer(SbViewportRegion(512, 512));
SbBool success = renderer.render(scene);
unsigned char * buffer = renderer.getBuffer();
```

The implementation maintains full backward compatibility while providing a modern, portable alternative to platform-specific offscreen rendering.