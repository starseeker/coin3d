# OSMesa Integration for Guaranteed Context Creation

This implementation adds OSMesa support as requested by @starseeker to provide guaranteed context creation capability, especially for testing environments.

## What Was Implemented

### 1. **OSMesa Glue Layer** (`gl_osmesa.h` / `gl_osmesa.cpp`)
- Complete OSMesa implementation following the same pattern as `gl_glx.cpp`, `gl_wgl.cpp`, etc.
- Provides software-based OpenGL rendering that works in headless environments
- Supports RGBA rendering with configurable depth buffer
- Memory management with proper cleanup

### 2. **Environment Variable Control**
- **`COIN_FORCE_OSMESA=1`**: Forces OSMesa usage for guaranteed context creation (perfect for testing)
- **`COIN_USE_OSMESA_FALLBACK=1`** (default): Uses OSMesa as fallback when platform-specific methods fail
- **`COIN_DEBUG_OSMESA=1`**: Enables OSMesa debug output

### 3. **Integration into Offscreen Context Creation**
Modified `cc_glglue_context_create_offscreen()` to:
1. Try OSMesa first if `COIN_FORCE_OSMESA=1` (guaranteed context creation)
2. Try platform-specific implementations (GLX, WGL, CGL, etc.)
3. Fall back to OSMesa if platform methods fail and `COIN_USE_OSMESA_FALLBACK=1`

### 4. **Testing Support**
- Added OSMesa test functions to verify functionality
- Unit tests for environment variable handling
- Integration with existing offscreen rendering test framework

## Key Benefits

1. **Guaranteed Context Creation**: OSMesa provides software rendering that always works
2. **Perfect for Testing**: Eliminates the "no display" issues in headless CI environments
3. **Cross-Platform**: Works on all platforms where OSMesa is available
4. **Seamless Integration**: Uses existing offscreen rendering APIs transparently
5. **FBO Compatible**: OSMesa supports modern OpenGL features including FBOs

## Usage Examples

```bash
# Force OSMesa for guaranteed testing
export COIN_FORCE_OSMESA=1
./my_test_application

# Use OSMesa as fallback (default behavior)
export COIN_USE_OSMESA_FALLBACK=1
./my_application

# Debug OSMesa usage
export COIN_DEBUG_OSMESA=1
```

## Integration with FBO Implementation

The OSMesa layer works seamlessly with the FBO implementation:
- When FBO is available in OSMesa context → Uses FBO for efficiency
- When FBO not available → Falls back to OSMesa's native offscreen rendering
- This provides a robust, portable solution for all scenarios

## CMake Configuration

OSMesa support is automatically included when `HAVE_OSMESA` is defined:
```cmake
option(COIN3D_USE_OSMESA "Build against OSMesa for offscreen/headless rendering" OFF)
```

This addresses @starseeker's request for guaranteed context creation capability and provides a solid foundation for reliable testing in any environment.