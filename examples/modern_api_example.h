/* Modern C++ example demonstrating the new idiomatic API for context management */

#ifdef COIN3D_OSMESA_BUILD
/* OSMesa-specific code - Uses modern C++ context provider API */
#include <OSMesa/osmesa.h>
#include <OSMesa/gl.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <memory>

struct CoinOSMesaContext {
    OSMesaContext context;
    std::unique_ptr<unsigned char[]> buffer;
    int width, height;
    
    CoinOSMesaContext(int w, int h) : width(w), height(h) {
        buffer = std::make_unique<unsigned char[]>(width * height * 4);
        context = OSMesaCreateContext(OSMESA_RGBA, nullptr);
    }
    
    ~CoinOSMesaContext() {
        if (context) {
            OSMesaDestroyContext(context);
        }
    }
    
    bool makeCurrent() {
        return OSMesaMakeCurrent(context, buffer.get(), GL_UNSIGNED_BYTE, width, height) == GL_TRUE;
    }
    
    bool isValid() const {
        return context != nullptr;
    }
};

// Modern C++ context provider implementation
class CoinOSMesaContextProvider : public SoOffscreenRenderer::ContextProvider {
public:
    virtual void * createOffscreenContext(unsigned int width, unsigned int height) override {
        auto* context = new CoinOSMesaContext(width, height);
        if (!context->isValid()) {
            delete context;
            return nullptr;
        }
        return context;
    }
    
    virtual SbBool makeContextCurrent(void * context) override {
        if (!context) return FALSE;
        auto* osmesa_ctx = static_cast<CoinOSMesaContext*>(context);
        return osmesa_ctx->makeCurrent() ? TRUE : FALSE;
    }
    
    virtual void restorePreviousContext(void * context) override {
        // OSMesa doesn't need explicit context restoration
        (void)context;
    }
    
    virtual void destroyContext(void * context) override {
        if (context) {
            delete static_cast<CoinOSMesaContext*>(context);
        }
    }
};

// Initialize modern OSMesa context management for Coin3D
inline void initializeModernCoinOSMesaContext() {
    static CoinOSMesaContextProvider provider;
    SoOffscreenRenderer::setContextProvider(&provider);
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

// Example: GLX-based custom context provider (advanced usage)
#ifdef HAVE_GLX
#include <GL/glx.h>

class GLXContextProvider : public SoOffscreenRenderer::ContextProvider {
    // Implementation would go here for advanced GLX context management
    // This is only needed for specialized rendering scenarios
public:
    virtual void * createOffscreenContext(unsigned int width, unsigned int height) override {
        // Custom GLX pbuffer creation would go here
        return nullptr; // Simplified for example
    }
    
    virtual SbBool makeContextCurrent(void * context) override {
        return FALSE; // Simplified for example
    }
    
    virtual void restorePreviousContext(void * context) override {
        // Custom context restoration
    }
    
    virtual void destroyContext(void * context) override {
        // Custom context destruction
    }
};
#endif

#endif

// ============================================================================
// Modern usage example - replacing old cc_glglue style code
// ============================================================================

/*
 * OLD WAY (discouraged):
 * 
 * #include "internal_glue.h"
 * cc_glglue_offscreen_cb_functions callbacks = { ... };
 * cc_glglue_context_set_offscreen_cb_functions(&callbacks);
 * const cc_glglue* glue = cc_glglue_instance(1);
 * cc_glglue_has_framebuffer_objects(glue);
 * 
 * NEW WAY (recommended):
 * 
 * #include <Inventor/SoOffscreenRenderer.h>
 * class MyProvider : public SoOffscreenRenderer::ContextProvider { ... };
 * SoOffscreenRenderer::setContextProvider(&provider);
 * SoOffscreenRenderer::hasFramebufferObjectSupport();
 */