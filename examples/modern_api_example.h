/* Modern C++ example demonstrating the Obol v2 render API.
 *
 * Applications still provide the platform/backend context implementation, but
 * v2 code should wrap it in obol::ContextManagerBackend and render through
 * obol::OffscreenRenderer instead of constructing Inventor nodes directly.
 */

#ifndef OBOL_EXAMPLES_MODERN_API_EXAMPLE_H
#define OBOL_EXAMPLES_MODERN_API_EXAMPLE_H

#include <Inventor/SoDB.h>
#include <Obol/Obol.h>

#include <cstdio>

inline void demonstrateObolV2Usage(SoDB::ContextManager * mgr)
{
    obol::ContextManagerBackend backend(mgr,
#ifdef OBOL_SWRAST_BUILD
        obol::RenderBackendKind::OpenGL2SWRast,
        "osmesa-swrast"
#else
        obol::RenderBackendKind::OpenGL,
        "system-opengl"
#endif
    );

    obol::Scene scene;
    obol::PerspectiveCamera camera;
    camera.position = {0.0f, 0.0f, 5.0f};
    camera.target = {0.0f, 0.0f, 0.0f};
    scene.setCamera(camera);
    scene.addDirectionalLight(obol::DirectionalLight{});

    obol::Material material;
    material.baseColor = {0.8f, 0.1f, 0.1f, 1.0f};
    scene.addPrimitive(obol::Primitive::Cone, material);

    obol::RenderTarget target;
    target.width = 256;
    target.height = 256;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::OffscreenRenderer renderer(backend, target);
    obol::FrameResult result = renderer.render(scene);
    if (!result.success) {
        std::printf("Obol v2 render failed\n");
        return;
    }

    const obol::RenderCapabilities & caps = result.capabilities;
    std::printf("Backend: %s\n", caps.backendName.c_str());
    if (caps.known) {
        std::printf("OpenGL Version: %d.%d.%d\n",
                    caps.glMajor, caps.glMinor, caps.glRelease);
    }
    std::printf("FBO Support: %s\n",
                caps.framebufferObjects ? "Yes" : "No or unknown");
}

/*
 * Preferred API usage summary
 * ---------------------------
 *
 * 1. Implement SoDB::ContextManager for the platform/backend, or use a built-in
 *    manager such as SoDB::createOSMesaContextManager().
 *
 * 2. Pass that manager to SoDB::init() at application start.
 *
 * 3. Wrap the manager in obol::ContextManagerBackend and pass that backend to
 *    each obol::OffscreenRenderer.
 *
 * This keeps application code on the v2 scene/render API while preserving the
 * existing tested OpenGL2, system OpenGL, OSMesa, and non-GL render paths.
 */

#endif // OBOL_EXAMPLES_MODERN_API_EXAMPLE_H
