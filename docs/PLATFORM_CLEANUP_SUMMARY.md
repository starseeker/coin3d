# Obol Platform-Specific Code Cleanup Summary

## Overview
This document summarizes the removal of platform-specific code from Obol,
achieving a clean, portable codebase using only FBO-based offscreen rendering
and callback-based context management.

## Files Removed (18 files, ~8000+ lines)

### Platform-Specific Offscreen Data Files
- `src/rendering/SoOffscreenWGLData.cpp/h` - Windows resolution detection
- `src/rendering/SoOffscreenGLXData.cpp/h` - Linux resolution detection  
- `src/rendering/SoOffscreenCGData.cpp/h` - macOS resolution detection

### Platform-Specific Font Files
- `src/fonts/win32.cpp/h` - Windows font handling

### Platform-Specific Headers
- `include/Inventor/oivwin32.h` - Windows compatibility
- `src/glue/win32api.h` - Windows API wrapper

### Platform-Specific GL Glue Files
- `src/glue/gl_wgl.cpp/h` - Windows WGL implementation
- `src/glue/gl_glx.cpp/h` - Linux GLX implementation
- `src/glue/gl_agl.cpp/h` - macOS AGL implementation
- `src/glue/gl_cgl.cpp/h` - macOS CGL implementation
- `src/glue/gl_egl.cpp/h` - EGL implementation

## Functions Simplified/Removed

### Context Management Functions (now portable)
- `cc_glglue_context_create_offscreen()` - Uses callbacks only
- `cc_glglue_context_make_current()` - Uses callbacks only
- `cc_glglue_context_reinstate_previous()` - Uses callbacks only
- `cc_glglue_context_destruct()` - Uses callbacks only

### Obsolete PBuffer Functions (simplified to no-ops)
- `cc_glglue_context_pbuffer_is_bound()` - Always returns FALSE
- `cc_glglue_context_bind_pbuffer()` - No-op
- `cc_glglue_context_release_pbuffer()` - No-op

### Render-to-Texture Functions (simplified)
- `cc_glglue_context_can_render_to_texture()` - Always returns FALSE

### Windows HDC Functions (simplified)
- `cc_glglue_win32_HDC()` - Always returns NULL
- `cc_glglue_win32_updateHDCBitmap()` - No-op

### Function Loading (simplified)
- `cc_glglue_getprocaddress()` - Uses only portable `cc_dl_sym()`

### Context Information (simplified)
- `coin_gl_current_context()` - Always returns NULL

## Variables/Detection Removed

### Platform Detection Variables
- `COIN_USE_AGL` - AGL vs CGL selection (macOS)
- `COIN_USE_EGL` - EGL selection (embedded systems)

### Platform Detection Functions
- `check_force_agl()` - AGL detection
- `check_egl()` - EGL detection

## Resolution Detection Simplified

### Before (platform-specific)
```cpp
#ifdef HAVE_GLX
  pixmmres = SoOffscreenGLXData::getResolution();
#elif defined(HAVE_WGL)
  pixmmres = SoOffscreenWGLData::getResolution();
#elif defined(COIN_MACOS_10)
  pixmmres = SoOffscreenCGData::getResolution();
#endif
```

### After (portable)
```cpp
// Use standard 72 DPI for offscreen rendering
SbVec2f pixmmres(72.0f / 25.4f, 72.0f / 25.4f);
```

## Font Handling Simplified

### Before (platform-specific)
- Win32 font API (`cc_flww32_*`) for Windows
- FreeType (`cc_flwft_*`) for other platforms

### After (portable)
- Embedded `struetype` TrueType rasterizer on all platforms; `libfreetype` is not required

## Current Architecture (100% Portable)

### Context Management
- **Method**: Callback-based only via `SoDB::ContextManager`
- **Files**: `src/glue/gl.cpp` (callback dispatch)
- **Application Responsibility**: Implement `SoDB::ContextManager` and pass it to `SoDB::init()`

### Offscreen Rendering
- **Method**: FBO-based only
- **Files**: `src/rendering/CoinOffscreenGLCanvas.cpp`
- **Features**: Portable OpenGL framebuffer objects

### Font Handling
- **Method**: Embedded `struetype` TrueType rasterizer (no external dependency)
- **Files**: `src/fonts/fontlib_wrapper.cpp`
- **Platforms**: All platforms use the bundled `struetype` rasterizer; `libfreetype` is not required

### Resolution Detection
- **Method**: Fixed 72 DPI default
- **Files**: `src/rendering/SoOffscreenRenderer.cpp`
- **Rationale**: Appropriate for offscreen rendering, applications can scale as needed

### OpenGL Function Loading
- **Method**: Portable shared library symbol lookup
- **Files**: `src/glue/gl.cpp` (`cc_dl_sym()`)
- **Fallback**: Applications provide complete function loading via callbacks

## Migration Guide for Applications

### Required Changes
1. **Implement `SoDB::ContextManager`**: Replace any platform-specific context
   creation with a `ContextManager` subclass passed to `SoDB::init()`.  See
   `docs/CONTEXT_MANAGEMENT_API.md` for examples.

2. **DPI Handling**: Obol returns a fixed 72 DPI from the offscreen renderer.
   Applications that need device-specific DPI should scale their rendering
   independently.

## Benefits Achieved

### Code Simplification
- **8000+ lines removed**: Massive reduction in codebase complexity
- **Zero platform conditionals**: No more `#ifdef HAVE_WGL/GLX/AGL/CGL/EGL`
- **Single code path**: One implementation for all platforms

### Maintenance Benefits
- **Reduced complexity**: No platform-specific testing needed
- **Better reliability**: Fewer code paths = fewer bugs
- **Easier porting**: No platform-specific dependencies

### Architecture Benefits
- **Clean separation**: Clear boundary between library and platform
- **Flexible**: Applications control context management completely
- **Future-proof**: No dependency on legacy platform APIs

## Testing Status
- ✅ All tests passing (see `docs/TESTING.md`)
- ✅ Offscreen rendering working (FBO-based)
- ✅ Font rendering working (embedded `struetype` rasterizer)
- ✅ Context management working via `SoDB::ContextManager`

This cleanup represents a major milestone in achieving a clean, portable, and maintainable Obol codebase.