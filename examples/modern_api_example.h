/* Modern C++ example demonstrating context management API changes */

// NOTE: The ContextProvider API has been removed from SoOffscreenRenderer
// Context management should now be done via SoDB::init(context_manager) before library initialization

#ifdef COIN3D_OSMESA_BUILD
/* OSMesa-specific code - Context management now handled globally */
#include <OSMesa/osmesa.h>
#include <OSMesa/gl.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <memory>

// Example: Modern usage without ContextProvider API
inline void demonstrateModernOSMesaUsage() {
    // Context management is now handled globally via SoDB::init(context_manager)
    // No need for per-renderer context providers
    
    SbViewportRegion viewport(256, 256);
    SoOffscreenRenderer renderer(viewport);
    
    // OpenGL capabilities can be queried directly
    int major, minor, release;
    SoOffscreenRenderer::getOpenGLVersion(major, minor, release);
    
    SbBool hasFBO = SoOffscreenRenderer::hasFramebufferObjectSupport();
    SbBool hasExtension = SoOffscreenRenderer::isOpenGLExtensionSupported("GL_ARB_vertex_buffer_object");
    SbBool hasGL3 = SoOffscreenRenderer::isVersionAtLeast(3, 0);
}

#else
/* System OpenGL code - Uses standard context creation */
#include <GL/gl.h>
#include <GL/glu.h>
#include <Inventor/SoOffscreenRenderer.h>

// NOTE: With modern Coin3D APIs, applications can use the high-level
// SoOffscreenRenderer directly without needing custom context providers
// for most standard use cases.

// Example: Standard usage without custom context provider
inline void demonstrateModernStandardUsage() {
    // Check OpenGL capabilities using modern API
    int major, minor, release;
    SoOffscreenRenderer::getOpenGLVersion(major, minor, release);
    
    bool hasModernOpenGL = SoOffscreenRenderer::isVersionAtLeast(3, 0);
    bool hasFBOSupport = SoOffscreenRenderer::hasFramebufferObjectSupport();
    
    printf("OpenGL Version: %d.%d.%d\n", major, minor, release);
    printf("Modern OpenGL (3.0+): %s\n", hasModernOpenGL ? "Yes" : "No");
    printf("FBO Support: %s\n", hasFBOSupport ? "Yes" : "No");
    
    // For standard rendering, just use SoOffscreenRenderer directly
    SbViewportRegion viewport(800, 600);
    SoOffscreenRenderer renderer(viewport);
    
    // The renderer will handle context creation automatically
    // No need for manual context management in most cases
}

#endif

// ============================================================================
// Modern usage example - replacing old cc_glglue style code
// ============================================================================

/*
 * OLD WAY (no longer available):
 * 
 * #include <Inventor/SoOffscreenRenderer.h>
 * class MyProvider : public SoOffscreenRenderer::ContextProvider { ... };
 * SoOffscreenRenderer::setContextProvider(&provider);
 * SoOffscreenRenderer::hasFramebufferObjectSupport();
 * 
 * NEW WAY (current approach):
 * 
 * Context management should be done via SoDB::init(context_manager) at initialization.
 * OpenGL capabilities can be queried directly:
 * 
 * #include <Inventor/SoOffscreenRenderer.h>
 * SoOffscreenRenderer::hasFramebufferObjectSupport();
 * SoOffscreenRenderer::getOpenGLVersion(major, minor, release);
 * SoOffscreenRenderer::isVersionAtLeast(3, 0);
 */

#endif // COIN3D_OSMESA_BUILD