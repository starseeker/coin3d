# Coin3D Context Management API

## Overview

Coin3D now provides a public API for OpenGL context management that allows applications to provide custom context creation and management before library initialization. This addresses initialization ordering issues and provides a clean, idiomatic C++ interface.

## Problem Solved

Previously, applications needed to use internal `cc_glglue_context_*` functions to provide context management callbacks. This had several issues:

1. **Private API**: These functions were internal-only and not part of the public interface
2. **Initialization Ordering**: Context callbacks needed to be set before `SoDB::init()` but this wasn't clear
3. **Non-deterministic Behavior**: Improper ordering could cause hanging or infinite loops, especially with OSMesa FBO support
4. **C-style Interface**: Required C-style callbacks instead of clean C++ interfaces

## New Public API

### SoDB::ContextManager

A new abstract base class in the `SoDB` class provides the public interface:

```cpp
class SoDB::ContextManager {
public:
    virtual ~ContextManager() {}
    virtual void * createOffscreenContext(unsigned int width, unsigned int height) = 0;
    virtual SbBool makeContextCurrent(void * context) = 0;
    virtual void restorePreviousContext(void * context) = 0;
    virtual void destroyContext(void * context) = 0;
};
```

### Registration Methods

```cpp
// Set the global context manager (must be called BEFORE SoDB::init())
static void SoDB::setContextManager(ContextManager * manager);

// Get the currently registered context manager
static SoDB::ContextManager * SoDB::getContextManager(void);
```

## Usage Example: OSMesa

```cpp
#include <Inventor/SoDB.h>
#include <OSMesa/osmesa.h>

class OSMesaContextManager : public SoDB::ContextManager {
private:
    struct OSMesaContext {
        OSMesaContext context;
        std::unique_ptr<unsigned char[]> buffer;
        int width, height;
        // ... implementation details
    };

public:
    virtual void* createOffscreenContext(unsigned int width, unsigned int height) override {
        // Create OSMesa context
        return new OSMesaContext(width, height);
    }
    
    virtual SbBool makeContextCurrent(void* context) override {
        // Make OSMesa context current
        auto* ctx = static_cast<OSMesaContext*>(context);
        return ctx->makeCurrent() ? TRUE : FALSE;
    }
    
    virtual void restorePreviousContext(void* context) override {
        // Restore previous context (OSMesa may not need this)
    }
    
    virtual void destroyContext(void* context) override {
        // Clean up context
        delete static_cast<OSMesaContext*>(context);
    }
};

int main() {
    // Create context manager
    OSMesaContextManager contextManager;
    
    // CRITICAL: Set context manager BEFORE SoDB::init()
    SoDB::setContextManager(&contextManager);
    
    // Now initialize Coin3D
    SoDB::init();
    
    // ... rest of application code ...
    
    return 0;
}
```

## Migration from Old API

### Before (Internal API - Not Recommended)

```cpp
#include "glue/glp.h"  // Internal header

// C-style callbacks
static void* create_context(unsigned int w, unsigned int h) { /* ... */ }
static SbBool make_current(void* ctx) { /* ... */ }
static void reinstate_previous(void* ctx) { /* ... */ }
static void destruct_context(void* ctx) { /* ... */ }

int main() {
    // Register C-style callbacks
    cc_glglue_offscreen_cb_functions callbacks = {
        create_context, make_current, reinstate_previous, destruct_context
    };
    cc_glglue_context_set_offscreen_cb_functions(&callbacks);
    
    SoDB::init();
    // ...
}
```

### After (Public API - Recommended)

```cpp
#include <Inventor/SoDB.h>  // Public header

class MyContextManager : public SoDB::ContextManager {
    virtual void* createOffscreenContext(unsigned int w, unsigned int h) override { /* ... */ }
    virtual SbBool makeContextCurrent(void* ctx) override { /* ... */ }
    virtual void restorePreviousContext(void* ctx) override { /* ... */ }
    virtual void destroyContext(void* ctx) override { /* ... */ }
};

int main() {
    MyContextManager manager;
    SoDB::setContextManager(&manager);  // BEFORE SoDB::init()
    
    SoDB::init();
    // ...
}
```

## Key Benefits

1. **Public API**: No longer need to include internal headers
2. **Clear Ordering**: Explicit requirement to set manager before `SoDB::init()`
3. **C++ Interface**: Clean object-oriented design instead of C callbacks
4. **Type Safety**: Better type checking and RAII support
5. **Documentation**: Proper API documentation and examples

## Initialization Ordering

The key requirement is that `SoDB::setContextManager()` **must** be called before `SoDB::init()`:

```cpp
// ✓ CORRECT ORDER
SoDB::setContextManager(&myManager);
SoDB::init();

// ✗ WRONG ORDER - May cause issues
SoDB::init();
SoDB::setContextManager(&myManager);  // Too late!
```

## Backwards Compatibility

The old internal callback API still works but is not recommended. The new public API is the preferred approach for all new code.

## Platform Support

- **OSMesa**: Full support with included examples
- **GLX**: Implement context manager for X11/Linux platforms
- **WGL**: Implement context manager for Windows platforms  
- **Custom**: Any OpenGL context management can be supported

## Testing

The new API includes comprehensive tests demonstrating:

- Context manager registration before initialization
- Successful rendering with custom contexts
- Proper cleanup and lifecycle management
- Integration with existing Coin3D rendering pipeline

See `tests/osmesa_context_test_new.cpp` for a complete working example.