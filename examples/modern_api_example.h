/* Modern C++ example demonstrating per-context manager API usage.
 *
 * The preferred pattern for multi-backend scenarios is to pass the context
 * manager explicitly to SoOffscreenRenderer so that different renderers in
 * the same process can each use a different backend without affecting each
 * other through any global state.
 */

#ifdef OBOL_SWRAST_BUILD
/* OSMesa-specific context manager example */
#include <Inventor/SoDB.h>
#include <Inventor/SoOffscreenRenderer.h>

// Example: create a context manager and pass it explicitly to the renderer.
// This avoids all global-singleton dependencies.
inline void demonstrateOSMesaUsage(SoDB::ContextManager * mgr) {
    SbViewportRegion viewport(256, 256);
    SoOffscreenRenderer renderer(mgr, viewport);

    // Query OpenGL capabilities through the renderer instance.
    int major, minor, release;
    renderer.getOpenGLVersion(major, minor, release);

    SbBool hasFBO = renderer.hasFramebufferObjectSupport();
    SbBool hasExtension = renderer.isOpenGLExtensionSupported("GL_ARB_vertex_buffer_object");
    SbBool hasGL3 = renderer.isVersionAtLeast(3, 0);
    (void)hasFBO; (void)hasExtension; (void)hasGL3;
}

#else
/* System OpenGL context manager example */
#include <Inventor/SoDB.h>
#include <Inventor/SoOffscreenRenderer.h>

// Example: pass the context manager explicitly.
// Multiple panels with different backends can coexist because each renderer
// uses its own manager instead of a global singleton.
inline void demonstrateSystemGLUsage(SoDB::ContextManager * mgr) {
    SbViewportRegion viewport(800, 600);
    SoOffscreenRenderer renderer(mgr, viewport);

    int major, minor, release;
    renderer.getOpenGLVersion(major, minor, release);

    bool hasModernOpenGL = renderer.isVersionAtLeast(3, 0);
    bool hasFBOSupport = renderer.hasFramebufferObjectSupport();

    printf("OpenGL Version: %d.%d.%d\n", major, minor, release);
    printf("Modern OpenGL (3.0+): %s\n", hasModernOpenGL ? "Yes" : "No");
    printf("FBO Support: %s\n", hasFBOSupport ? "Yes" : "No");
}

#endif /* OBOL_SWRAST_BUILD */

/*
 * Preferred API usage summary
 * ---------------------------
 *
 * 1. Implement SoDB::ContextManager for your backend (or use a built-in one
 *    such as the OSMesa manager returned by SoDB::createOSMesaContextManager()).
 *
 * 2. Pass it to SoDB::init() at application start:
 *      MyContextManager mgr;
 *      SoDB::init(&mgr);
 *
 * 3. Pass it explicitly to each SoOffscreenRenderer:
 *      SoOffscreenRenderer renderer(&mgr, viewport);
 *
 *    This allows multiple renderers in the same process to use different
 *    backends independently, with no global state dependencies between them.
 */
