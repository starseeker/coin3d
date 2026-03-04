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

Obol ships two reference implementations:

| Class | Backend | Header | Notes |
|-------|---------|--------|-------|
| `SoNanoRTContextManager` | [nanort](https://github.com/lighttransport/nanort) (bundled) | `tests/utils/nanort_context_manager.h` | Always available |
| `SoEmbreeContextManager` | [Intel Embree 4](https://www.embree.org/) (system library) | `tests/utils/embree_context_manager.h` | Requires `libembree-dev` |

Both managers delegate **all** scene collection to the generic
`SoRaytracerSceneCollector` library class (see below) and differ only in their
BVH build and ray-intersection code.

### SoRaytracerSceneCollector

`SoRaytracerSceneCollector` (`include/Inventor/SoRaytracerSceneCollector.h`) is
a public Obol library class that encapsulates the full scene-collection
pipeline that any CPU raytrace backend needs:

| Capability | Description |
|-----------|-------------|
| Triangle collection | All `SoShape` subclasses → world-space `SoRtTriangle` list |
| Light collection | `SoDirectionalLight`, `SoPointLight`, `SoSpotLight` → world-space `SoRtLightInfo` list |
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
SoRaytracerSceneCollector collector;
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
2. Include `<Inventor/SoRaytracerSceneCollector.h>`.
3. Add a `SoRaytracerSceneCollector` member.
4. In `renderScene()`:
   - Call `collector_.needsRebuild()` / `collector_.collect()` as above.
   - Build your backend's acceleration structure from `collector_.getTriangles()`.
   - Raytrace using `collector_.getLights()` for shading.
   - Call `collector_.compositeOverlays()` for text/HUD.
5. Pass the manager to `SoDB::init()`.

See `tests/utils/embree_context_manager.h` for a complete worked example (~450 lines).

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
