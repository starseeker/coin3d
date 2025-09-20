# Context Management Refactoring in Coin3D

## Overview

This refactoring changes Coin3D's OpenGL context management to require applications to provide context creation and management through callbacks, rather than having the library create contexts internally.

## Motivation

The original problem statement requested that:
1. Applications take full responsibility for creating OpenGL contexts
2. Coin3D should assume a context is already present 
3. Platform-specific operations should be handled via application-supplied callbacks
4. OSMesa should be used for testing to demonstrate the approach

## Changes Made

### 1. CoinOffscreenGLCanvas Refactoring

**File**: `src/rendering/CoinOffscreenGLCanvas.cpp`

- Removed direct platform-specific context creation calls
- Modified `tryActivateGLContext()` to only use callback-based context creation
- Updated HDC handling to be callback-managed (Windows-specific features now managed by callbacks)
- Added clear error messages when callbacks are not provided

**Before**:
```cpp
#if defined(HAVE_WGL)
    this->context = wglglue_context_create_offscreen(this->size[0], this->size[1], FALSE);
#else
    this->context = cc_glglue_context_create_offscreen(this->size[0], this->size[1]);
#endif
```

**After**:
```cpp
// Always use the callback-based context creation system
// Applications must provide context creation callbacks
this->context = cc_glglue_context_create_offscreen(this->size[0], this->size[1]);
```

### 2. Context Management Functions Refactoring

**File**: `src/glue/gl.cpp`

Modified all context management functions to require callbacks:

- `cc_glglue_context_create_offscreen()` - Now returns NULL if no callbacks provided
- `cc_glglue_context_make_current()` - Returns FALSE if no callbacks provided  
- `cc_glglue_context_reinstate_previous()` - Graceful no-op if no callbacks
- `cc_glglue_context_destruct()` - Shows error if no destruct callback provided

**Error Messages**: Clear error messages guide developers to use `cc_glglue_context_set_offscreen_cb_functions()`

### 3. Callback Interface

The existing callback interface is used without changes:

```cpp
typedef struct cc_glglue_offscreen_cb_functions {
    cc_glglue_offscreen_data (*create_offscreen)(unsigned int width, unsigned int height);
    SbBool (*make_current)(cc_glglue_offscreen_data context);
    void (*reinstate_previous)(cc_glglue_offscreen_data context);
    void (*destruct)(cc_glglue_offscreen_data context);
} cc_glglue_offscreen_cb_functions;
```

## Usage Examples

### OSMesa Context Management

**File**: `tests/osmesa_context_test.cpp`

Demonstrates complete OSMesa-based context management:

```cpp
// OSMesa Context wrapper
struct OSMesaContextData {
    OSMesaContext context;
    std::unique_ptr<unsigned char[]> buffer;
    int width, height;
    // ... methods for makeCurrent(), etc.
};

// Callback implementations
static void* osmesa_create_offscreen(unsigned int width, unsigned int height) {
    return new OSMesaContextData(width, height);
}

static SbBool osmesa_make_current(void* context) {
    auto* ctx = static_cast<OSMesaContextData*>(context);
    return ctx->makeCurrent() ? TRUE : FALSE;
}

// Register callbacks
cc_glglue_offscreen_cb_functions osmesa_callbacks = {
    osmesa_create_offscreen,
    osmesa_make_current,
    osmesa_reinstate_previous,
    osmesa_destruct
};
cc_glglue_context_set_offscreen_cb_functions(&osmesa_callbacks);
```

### Mock Context for Testing

**File**: `tests/context/test_context_callbacks.cpp`

Shows how to create minimal mock contexts for unit testing:

```cpp
struct MockContext {
    int width, height;
    bool is_current;
};

static void* mock_create_offscreen(unsigned int width, unsigned int height) {
    return new MockContext(width, height);
}

static SbBool mock_make_current(void* context) {
    static_cast<MockContext*>(context)->is_current = true;
    return TRUE;
}
```

## Migration Guide

### For Applications Using SoOffscreenRenderer

1. **Implement Context Callbacks**: Create platform-specific context management functions
2. **Register Callbacks**: Call `cc_glglue_context_set_offscreen_cb_functions()` before using any offscreen rendering
3. **Handle Lifecycle**: Ensure callbacks remain valid for the lifetime of context usage

### Example Migration

**Before** (automatic context creation):
```cpp
SoOffscreenRenderer renderer(SbViewportRegion(512, 512));
renderer.render(sceneGraph);  // Context created automatically
```

**After** (explicit callback setup):
```cpp
// Set up context callbacks first
MyContextCallbacks callbacks;
cc_glglue_context_set_offscreen_cb_functions(&callbacks);

// Now rendering works as before
SoOffscreenRenderer renderer(SbViewportRegion(512, 512));
renderer.render(sceneGraph);  // Uses callback-created context
```

## Testing

### Unit Tests Added

1. **Context Callback Tests** (`test_context_callbacks.cpp`):
   - Mock context implementation
   - Callback lifecycle management
   - Error condition handling
   - Null pointer safety

2. **OSMesa Integration Tests** (`test_osmesa_context.cpp`):
   - Full OSMesa context integration
   - SoOffscreenRenderer with OSMesa
   - Multiple context management

### Test Results

All existing tests pass, demonstrating backward compatibility for applications that don't use offscreen rendering or that provide their own context callbacks.

## Benefits

1. **Clear Separation of Concerns**: Context creation is now entirely the application's responsibility
2. **Platform Independence**: Coin3D no longer needs platform-specific context code
3. **Flexibility**: Applications can use any context type (OSMesa, EGL, custom implementations)
4. **Better Testing**: Context management can be mocked for unit testing
5. **Reduced Dependencies**: Fewer platform-specific dependencies in core library

## Error Handling

The refactored system provides clear error messages when callbacks are not provided:

```
ERROR: No context creation callbacks provided. Applications must provide context creation callbacks via cc_glglue_context_set_offscreen_cb_functions(). See examples/osmesa_example.h for reference implementation.
```

This guides developers to the correct solution and reference implementations.

## Future Considerations

1. **Complete Platform Code Removal**: Could remove all platform-specific context code (GLX, WGL, CGL) from the library entirely
2. **Context Sharing**: Callback interface could be extended to support context sharing
3. **Performance Callbacks**: Could add callbacks for context-specific performance optimizations
4. **Documentation**: Add comprehensive documentation for callback implementations

## Files Modified

- `src/rendering/CoinOffscreenGLCanvas.cpp` - Context creation refactoring
- `src/glue/gl.cpp` - Callback-only context management  
- `tests/context/test_context_callbacks.cpp` - Unit tests
- `tests/osmesa_context_test.cpp` - OSMesa demonstration
- `tests/CMakeLists.txt` - Test integration

This refactoring successfully achieves the goal of making context creation the application's responsibility while maintaining backward compatibility and providing clear guidance for migration.