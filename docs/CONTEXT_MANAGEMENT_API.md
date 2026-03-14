# Obol Context Management API

## Overview

Obol requires applications to supply an OpenGL context manager before library
initialization.  This cleanly separates all platform-specific context logic
from the library itself and allows any context back end — OSMesa, EGL, WGL,
GLX, or a no-op stub for non-rendering scenarios — to be substituted without
touching Obol code.

## The ContextManager Interface

`SoDB::ContextManager` is an abstract base class declared inside `SoDB`
(`include/Inventor/SoDB.h`).

### Pure-virtual methods (must be implemented)

```cpp
class SoDB::ContextManager {
public:
    virtual ~ContextManager() {}

    // Create an offscreen GL context of the given dimensions.
    // Returns an opaque context handle, or NULL on failure.
    virtual void * createOffscreenContext(unsigned int width,
                                          unsigned int height) = 0;

    // Make 'context' current for subsequent GL calls.
    // Returns TRUE on success.
    virtual SbBool makeContextCurrent(void * context) = 0;

    // Restore the context that was current before the last makeContextCurrent().
    virtual void restorePreviousContext(void * context) = 0;

    // Release all resources associated with 'context'.
    virtual void destroyContext(void * context) = 0;
};
```

### Optional virtual methods (default implementations provided)

These methods have safe no-op defaults and only need to be overridden by
implementations that specifically require the feature.

```cpp
    // Return TRUE if 'context' was created against OSMesa rather than
    // the system OpenGL/GLX/WGL backend.  Only needs to be overridden in
    // dual-GL builds (OBOL_BUILD_DUAL_GL) where both backends coexist in
    // the same library.  Default: returns FALSE.
    virtual SbBool isOSMesaContext(void * context);

    // Report the maximum offscreen dimensions this backend supports.
    // CoinOffscreenGLCanvas calls this to determine the render target ceiling.
    // An OSMesa manager should return a large value (e.g. 16384×16384) since
    // it is only RAM-limited.  Default: returns {0,0}, which tells
    // CoinOffscreenGLCanvas to probe via the global GL pipeline instead.
    virtual void maxOffscreenDimensions(unsigned int & width,
                                        unsigned int & height) const;

    // Return the actual pixel dimensions of the backing surface (Pbuffer,
    // renderbuffer, or window) for the given context handle.
    //
    // **This method is important for safety.**  Obol calls it before
    // issuing a raw glReadPixels() to confirm the surface is large enough
    // to hold the requested pixels.  If the surface is smaller than the
    // render target *and* FBO support is unavailable, Obol will skip the
    // readback and emit a diagnostic warning — preventing the out-of-bounds
    // memory write that would otherwise occur when reading beyond a tiny
    // framebuffer.
    //
    // Default: returns {0,0} ("unknown"), which causes Obol to rely solely
    // on FBO creation success/failure as its safety guard.  Override this
    // whenever the backing surface can be smaller than the largest texture
    // the application may request.
    virtual void getActualSurfaceSize(void * context,
                                      unsigned int & width,
                                      unsigned int & height) const;

    // Look up a GL extension function pointer by name.
    //
    // Called by Obol's GL glue layer when dlsym() cannot locate a function.
    // Platform implementations should delegate to the appropriate resolver:
    //   - X11/GLX:  glXGetProcAddress()
    //   - Windows:  wglGetProcAddress()
    //   - EGL:      eglGetProcAddress()
    //   - OSMesa:   OSMesaGetProcAddress()
    //
    // Keeping each backend's function pointers routed through its own
    // resolver prevents cross-contamination when system GL and OSMesa share
    // a process (see docs/DUAL_GL_ARCHITECTURE.md).
    // Default: returns nullptr; falls back to the existing dlsym lookup.
    virtual void * getProcAddress(const char * funcName);

    // Optional alternative rendering path (no OpenGL required).
    // If this returns TRUE, SoOffscreenRenderer uses the supplied 'pixels'
    // buffer directly and skips the entire GL pipeline.
    //
    // 'pixels' is pre-allocated: width*height*nrcomponents bytes, row-major,
    // top-to-bottom (matching SoOffscreenRenderer::getBuffer()).
    // 'background_rgb' is [R,G,B] in [0,1].
    //
    // Implement this to provide a CPU raytracing or Vulkan rasterization
    // backend.  See SoNanoRTContextManager and SoVulkanContextManager for
    // reference implementations.  Default: returns FALSE (GL path is used).
    virtual SbBool renderScene(SoNode * scene,
                               unsigned int width, unsigned int height,
                               unsigned char * pixels,
                               unsigned int nrcomponents,
                               const float background_rgb[3]);
```

Pass an instance to `SoDB::init()`:

```cpp
MyContextManager cm;
SoDB::init(&cm);          // preferred: pass manager directly to init()
```

`SoDB::getContextManager()` returns the registered global instance at any time
after initialization.  The global instance is used by legacy
`SoOffscreenRenderer` constructors that take no `ContextManager` argument.
Per-renderer managers (see below) take precedence over the global singleton.

## NullContextManager — Non-rendering / Testing

For unit tests or applications that never call `SoOffscreenRenderer`, a no-op
implementation is sufficient.  Only the four pure-virtual methods need to be
implemented:

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

For headless offscreen rendering with OSMesa, also override
`isOSMesaContext()`, `maxOffscreenDimensions()`, `getActualSurfaceSize()`, and
`getProcAddress()` so the library can make correct dispatch and safety
decisions:

```cpp
#include <Inventor/SoDB.h>
#include <OSMesa/osmesa.h>
#include <OSMesa/gl.h>

class OSMesaContextManager : public SoDB::ContextManager {
    struct Ctx {
        OSMesaContext ctx;
        std::unique_ptr<unsigned char[]> buf;
        unsigned int w, h;

        Ctx(unsigned int width, unsigned int height)
            // OSMesaCreateContextExt(format, depthBits, stencilBits, accumBits, sharelist)
            : ctx(OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, nullptr))
            , buf(ctx ? new unsigned char[width * height * 4] : nullptr)
            , w(width), h(height)
        {}

        ~Ctx() { if (ctx) OSMesaDestroyContext(ctx); }

        bool makeCurrent() {
            return ctx && OSMesaMakeCurrent(ctx, buf.get(),
                                            GL_UNSIGNED_BYTE, w, h) == GL_TRUE;
        }
        bool isValid() const { return ctx != nullptr; }
    };

public:
    void * createOffscreenContext(unsigned int w, unsigned int h) override {
        auto * ctx = new Ctx(w, h);
        return ctx->isValid() ? ctx : (delete ctx, nullptr);
    }

    SbBool makeContextCurrent(void * context) override {
        return static_cast<Ctx *>(context)->makeCurrent() ? TRUE : FALSE;
    }

    void restorePreviousContext(void *) override {
        OSMesaMakeCurrent(nullptr, nullptr, 0, 0, 0);
    }

    void destroyContext(void * context) override {
        delete static_cast<Ctx *>(context);
    }

    // --- optional overrides for correctness and safety ---

    // All contexts from this manager are OSMesa; tell the dispatch layer.
    SbBool isOSMesaContext(void *) override { return TRUE; }

    // OSMesa is only RAM-limited; report a large ceiling.
    void maxOffscreenDimensions(unsigned int & width,
                                unsigned int & height) const override {
        width = height = 16384;
    }

    // Report the exact surface size so Obol can guard against
    // glReadPixels reading beyond the allocation.
    void getActualSurfaceSize(void * context,
                              unsigned int & width,
                              unsigned int & height) const override {
        auto * ctx = static_cast<Ctx *>(context);
        width  = ctx ? ctx->w : 0;
        height = ctx ? ctx->h : 0;
    }

    // Route extension-function lookups through OSMesa, not the system GL.
    void * getProcAddress(const char * name) override {
        return reinterpret_cast<void *>(OSMesaGetProcAddress(name));
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

Implement all four pure-virtual methods, then pass an instance to
`SoDB::init()`.  Override `getActualSurfaceSize()` and `getProcAddress()` for
any production manager where the backing surface has a fixed size or where
extension pointers need platform-specific resolution.

## Per-Renderer Context Managers

`SoOffscreenRenderer` accepts a `ContextManager` pointer in its constructor,
enabling each renderer instance to use a *different* backend independently of
the global singleton:

```cpp
// Global context manager (for on-screen rendering, etc.)
FLTKContextManager global_cm;
SoDB::init(&global_cm);

// Dedicated OSMesa manager for a specific renderer
CoinOSMesaContextManager osmesa_cm;
SoOffscreenRenderer renderer(&osmesa_cm, viewport);  // uses osmesa_cm
renderer.render(root);

// Another renderer using the global manager (system GL)
SoOffscreenRenderer other_renderer(viewport);         // uses global_cm
```

Use `renderer.setContextManager(&other_cm)` at any time to switch backends;
pass `nullptr` to revert to the global singleton.

This design makes it straightforward to run, for example, a Vulkan rasterization
backend and an OSMesa backend in the same process without any global state
switching.

## Built-in OSMesa Factory

When the library is built with OSMesa support
(`OBOL_USE_OSMESA=ON` or `OBOL_BUILD_DUAL_GL=ON`), the factory function
`SoDB::createOSMesaContextManager()` creates a ready-to-use OSMesa context
manager without requiring the application to include OSMesa headers:

```cpp
// Application does not need to include <OSMesa/osmesa.h>
auto mgr = std::unique_ptr<SoDB::ContextManager>(
               SoDB::createOSMesaContextManager());
if (mgr) {
    SoOffscreenRenderer renderer(mgr.get(), viewport);
    renderer.render(root);
}
```

This is the recommended way to use the OSMesa backend in dual-GL builds, where
both system GL and OSMesa are available in the same library.  The returned
manager correctly implements all optional virtual overrides, including
`isOSMesaContext()`, `maxOffscreenDimensions()`, `getActualSurfaceSize()`, and
`getProcAddress()`.  Returns `nullptr` if the library was built without OSMesa
support.

## Raytracing Context Managers (No OpenGL Required)

A `ContextManager` may override the optional `renderScene()` virtual to
replace the entire GL render pipeline with a CPU raytracing backend:

```cpp
class SoDB::ContextManager {
public:
    // ... (GL context lifecycle, unchanged) ...

    // Optional override: render the scene without OpenGL.
    // Called by SoOffscreenRenderer when a custom backend is active.
    // Returns FALSE to fall back to the GL path.
    virtual SbBool renderScene(SoNode * scene,
                               unsigned int width, unsigned int height,
                               unsigned char * pixels,
                               unsigned int nrcomponents,
                               const float background_rgb[3]);
};
```

Obol ships three reference implementations:

| Class | Backend | Header | Notes |
|-------|---------|--------|-------|
| `SoNanoRTContextManager` | [nanort](https://github.com/lighttransport/nanort) (bundled) | `tests/utils/nanort_context_manager.h` | Always available |
| `SoEmbreeContextManager` | [Intel Embree 4](https://www.embree.org/) (system library) | `tests/utils/embree_context_manager.h` | Requires `libembree-dev` |
| `SoVulkanContextManager` | Vulkan rasterization (CPU Phong pre-baking) | `tests/utils/vulkan_context_manager.h` | Requires Vulkan SDK; uses `SoRaytracerSceneCollector` for geometry |

The raytracing managers delegate **all** scene collection to the generic
`SoSceneCollector` library class (see below) and differ only in their
BVH build and ray-intersection code.  `SoVulkanContextManager` implements
`renderScene()` using Vulkan rasterization rather than ray tracing.

### SoSceneCollector

`SoSceneCollector` (`include/Inventor/SoSceneCollector.h`) is
a public Obol library class that encapsulates the full scene-collection
pipeline that any CPU raytrace backend needs:

| Capability | Description |
|-----------|-------------|
| Triangle collection | All `SoShape` subclasses → world-space `SoScTriangle` list |
| Light collection | `SoDirectionalLight`, `SoPointLight`, `SoSpotLight` → world-space `SoScLightInfo` list |
| Invisible shape pruning | `DrawStyle INVISIBLE` shapes pruned before geometry extraction |
| Line proxy geometry | `SoLineSet` / `SoIndexedLineSet` → thin cylinder proxies (via `createCylinderProxy()`) |
| Point proxy geometry | `SoPointSet` → small sphere proxies (via `createSphereProxy()`) |
| Wireframe cylinder rings | `SoCylinder` with `DrawStyle LINES` → ring of tube segments |
| Text overlays | `SoText2` pixel buffers (via `buildPixelBuffer()`) |
| HUD label overlays | `SoHUDLabel` text rasterized via `SbFont` |
| HUD button overlays | `SoHUDButton` border + centered label |
| Overlay compositing | `compositeOverlays()` alpha-blends all overlays onto the framebuffer |
| Scene change detection | `needsRebuild()` uses `SoNode::getNodeId()` to avoid unnecessary BVH rebuilds |

Usage pattern:

```cpp
SoSceneCollector collector;
SbViewportRegion vp(800, 600);

// Full scene rebuild:
SoCamera * cam = findCamera(root);
if (collector.needsRebuild(root, cam)) {
    collector.reset();
    collector.collect(root, vp);          // fills tris, lights, overlays
    buildBVH(collector.getTriangles());    // backend-specific
    collector.updateCacheKeysAfterRebuild(root, cam);
} else {
    collector.collectOverlaysOnly(root, vp); // cheap re-collect of text/HUD
}

// ... raytrace using cached BVH and collector.getLights() ...

collector.compositeOverlays(pixels, width, height, 3);
collector.updateCameraId(cam);
```

### Adding a New Raytracing Backend

To integrate a new raytracing engine (e.g. OptiX, Embree, BRL-CAD librt):

1. Create a `ContextManager` subclass (a single `.h` file is sufficient).
2. Include `<Inventor/SoSceneCollector.h>`.
3. Add a `SoSceneCollector` member.
4. In `renderScene()`:
   - Call `collector_.needsRebuild()` / `collector_.collect()` as above.
   - Build your backend's acceleration structure from `collector_.getTriangles()`.
   - Raytrace using `collector_.getLights()` for shading.
   - Call `collector_.compositeOverlays()` for text/HUD.
5. Pass the manager to `SoDB::init()`.

See `tests/utils/embree_context_manager.h` for a complete worked example (~450 lines).

## Key Points

* The global manager passed to `SoDB::init()` must remain valid for the entire
  lifetime of the Obol library (i.e., until `SoDB::finish()` is called or the
  program exits).
* Per-renderer managers (passed to `SoOffscreenRenderer` constructors) must
  remain valid for the lifetime of that renderer instance.
* `createOffscreenContext` is called by `SoOffscreenRenderer` when it needs
  an offscreen buffer; for on-screen rendering only a `NullContextManager` is
  acceptable.
* The `restorePreviousContext` hook exists for context-sharing scenarios where
  a previous context must be reinstated after offscreen work; it is a no-op in
  most single-context setups.
* **Override `getActualSurfaceSize()`** for any manager whose backing surface
  can be smaller than the largest texture an application might request.
  Implementing this correctly prevents out-of-bounds reads in `glReadPixels()`
  when FBO support is unavailable.
* Obol itself has **no** WGL, GLX, AGL, CGL, or EGL code.  All
  platform-specific context logic lives in the application-supplied manager.
* For dual-GL builds (system GL + OSMesa in one library) see
  `docs/DUAL_GL_ARCHITECTURE.md` for details on how both backends safely
  coexist.
