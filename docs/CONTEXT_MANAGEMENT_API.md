# Obol Context Management API

## Overview

Obol requires applications to supply an OpenGL context manager before library
initialization.  This cleanly separates all platform-specific context logic
from the library itself and allows any context back end — OSMesa, EGL, WGL,
GLX, or a no-op stub for non-rendering scenarios — to be substituted without
touching Obol code.

## The ContextManager Interface

`SoDB::ContextManager` is an abstract base class declared inside `SoDB`
(`include/Inventor/SoDB.h`):

```cpp
class SoDB::ContextManager {
public:
    virtual ~ContextManager() {}
    virtual void * createOffscreenContext(unsigned int width,
                                          unsigned int height) = 0;
    virtual SbBool makeContextCurrent(void * context) = 0;
    virtual void restorePreviousContext(void * context) = 0;
    virtual void destroyContext(void * context) = 0;
};
```

Pass an instance to `SoDB::init()`:

```cpp
MyContextManager cm;
SoDB::init(&cm);          // preferred: pass manager directly to init()
```

`SoDB::getContextManager()` returns the registered instance at any time after
initialization.

## NullContextManager — Non-rendering / Testing

For unit tests or applications that never call `SoOffscreenRenderer`, a no-op
implementation is sufficient:

```cpp
class NullContextManager : public SoDB::ContextManager {
public:
    void * createOffscreenContext(unsigned int, unsigned int) override
        { return nullptr; }
    SbBool makeContextCurrent(void *) override { return FALSE; }
    void restorePreviousContext(void *) override {}
    void destroyContext(void *) override {}
};

int main() {
    NullContextManager cm;
    SoDB::init(&cm);
    // ...
}
```

## OSMesa Example — Full Offscreen Rendering

```cpp
#include <Inventor/SoDB.h>
#include <GL/osmesa.h>

class OSMesaContextManager : public SoDB::ContextManager {
    struct Ctx {
        OSMesaContext ctx;
        std::unique_ptr<unsigned char[]> buf;
        unsigned int w, h;

        Ctx(unsigned int width, unsigned int height)
            : ctx(OSMesaCreateContext(OSMESA_RGBA, nullptr))
            , buf(new unsigned char[width * height * 4])
            , w(width), h(height)
        {}

        ~Ctx() { if (ctx) OSMesaDestroyContext(ctx); }

        bool makeCurrent() {
            return OSMesaMakeCurrent(ctx, buf.get(),
                                     GL_UNSIGNED_BYTE, w, h) == GL_TRUE;
        }
    };

public:
    void * createOffscreenContext(unsigned int w, unsigned int h) override {
        return new Ctx(w, h);
    }

    SbBool makeContextCurrent(void * context) override {
        return static_cast<Ctx *>(context)->makeCurrent() ? TRUE : FALSE;
    }

    void restorePreviousContext(void *) override {}

    void destroyContext(void * context) override {
        delete static_cast<Ctx *>(context);
    }
};

int main() {
    OSMesaContextManager cm;
    SoDB::init(&cm);
    // Use SoOffscreenRenderer, SoRenderManager, etc. normally.
}
```

See the test suite (e.g. `tests/rendering/`) for complete worked examples
used in CI.

## Platform-Specific Managers

| Platform | Context API | Notes |
|----------|-------------|-------|
| Linux    | GLX or EGL  | Create a Pbuffer or FBO-only context |
| Windows  | WGL         | Use `wglCreateContext` / `wglMakeCurrent` |
| macOS    | CGL         | Use `CGLCreateContext` / `CGLSetCurrentContext` |
| Headless | OSMesa      | Works everywhere; software rasterizer |

Implement all four methods, then pass an instance to `SoDB::init()`.

## Key Points

* The manager must remain valid for the entire lifetime of the Obol library
  (i.e., until `SoDB::finish()` is called or the program exits).
* `createOffscreenContext` is called by `SoOffscreenRenderer` when it needs
  an offscreen buffer; for on-screen rendering only a `NullContextManager` is
  acceptable.
* The `restorePreviousContext` hook exists for context-sharing scenarios where
  a previous context must be reinstated after offscreen work; it is a no-op in
  most single-context setups.
* Obol itself has **no** WGL, GLX, AGL, CGL, or EGL code.  All
  platform-specific context logic lives in the application-supplied manager.
