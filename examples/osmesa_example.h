/* Example demonstrating OSMesa vs System OpenGL conditional compilation */

#ifdef COIN3D_OSMESA_BUILD
/* OSMesa-specific code - Full context management example */
#include <OSMesa/osmesa.h>
#include <OSMesa/gl.h>
#include "internal_glue.h"
#include <Inventor/SoDB.h> // For new public context management API
#include <memory>

struct CoinOSMesaContext {
    OSMesaContext context;
    std::unique_ptr<unsigned char[]> buffer;
    int width, height;
    
    CoinOSMesaContext(int w, int h) : width(w), height(h) {
        context = OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, NULL);
        if (context) {
            buffer = std::make_unique<unsigned char[]>(width * height * 4);
        }
    }
    
    ~CoinOSMesaContext() {
        if (context) OSMesaDestroyContext(context);
    }
    
    bool makeCurrent() {
        return context && OSMesaMakeCurrent(context, buffer.get(), GL_UNSIGNED_BYTE, width, height);
    }
    
    bool isValid() const { return context != nullptr; }
};

// OSMesa callback implementations for Coin3D
static void* coin_osmesa_create_offscreen(unsigned int width, unsigned int height) {
    auto* ctx = new CoinOSMesaContext(width, height);
    return ctx->isValid() ? ctx : (delete ctx, nullptr);
}

static SbBool coin_osmesa_make_current(void* context) {
    return context && static_cast<CoinOSMesaContext*>(context)->makeCurrent() ? TRUE : FALSE;
}

static void coin_osmesa_reinstate_previous(void* context) {
    // OSMesa doesn't require explicit context switching for single-threaded use
    (void)context;
}

static void coin_osmesa_destruct(void* context) {
    delete static_cast<CoinOSMesaContext*>(context);
}

// Initialize OSMesa context management for Coin3D
inline void initializeCoinOSMesaContext() {
    static cc_glglue_offscreen_cb_functions osmesa_callbacks = {
        coin_osmesa_create_offscreen,
        coin_osmesa_make_current, 
        coin_osmesa_reinstate_previous,
        coin_osmesa_destruct
    };
    cc_glglue_context_set_offscreen_cb_functions(&osmesa_callbacks);
}

// NEW: Alternative using the PUBLIC SoDB API (recommended approach)
class CoinOSMesaContextManager : public SoDB::ContextManager {
public:
    virtual void* createOffscreenContext(unsigned int width, unsigned int height) override {
        auto* ctx = new CoinOSMesaContext(width, height);
        return ctx->isValid() ? ctx : (delete ctx, nullptr);
    }
    
    virtual SbBool makeContextCurrent(void* context) override {
        return context && static_cast<CoinOSMesaContext*>(context)->makeCurrent() ? TRUE : FALSE;
    }
    
    virtual void restorePreviousContext(void* context) override {
        // OSMesa doesn't require explicit context switching for single-threaded use
        (void)context;
    }
    
    virtual void destroyContext(void* context) override {
        delete static_cast<CoinOSMesaContext*>(context);
    }
};

// NEW: Initialize OSMesa context management using PUBLIC API
inline void initializeCoinOSMesaContextNew() {
    static CoinOSMesaContextManager osmesa_manager;
    SoDB::setContextManager(&osmesa_manager);
}

#else
/* System OpenGL code - Platform-specific context creation */
#include <GL/gl.h>
#include <GL/glu.h>
#include "internal_glue.h"

// NOTE: With the new Coin3D context management, applications must provide
// context creation callbacks even for system OpenGL. The library no longer
// creates contexts automatically.

// Example: GLX-based context creation (Linux)
#ifdef HAVE_GLX
#include <GL/glx.h>

struct CoinGLXContext {
    Display* display;
    GLXContext context;
    GLXPbuffer pbuffer;
    // ... implementation details
};

// Implement GLX callback functions similar to OSMesa example above
// ... 
#endif

// Example: WGL-based context creation (Windows)  
#ifdef HAVE_WGL
#include <windows.h>
#include <GL/wgl.h>

struct CoinWGLContext {
    HDC hdc;
    HGLRC context;
    // ... implementation details  
};

// Implement WGL callback functions
// ...
#endif

// Initialize platform-specific context management
inline void initializeCoinSystemContext() {
    // Applications must implement and register their platform-specific callbacks
    // The library no longer provides automatic context creation
    #error "Applications must provide context creation callbacks - see OSMesa example"
}

#endif

/* Common OpenGL code that works with both backends */
inline void setupBasicRendering() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

/* Example usage: */
/*
int main() {
    // Initialize Coin3D
    SoDB::init();
    
    // Set up context management - REQUIRED with new architecture
    #ifdef COIN3D_OSMESA_BUILD
        initializeCoinOSMesaContext();
    #else
        initializeCoinSystemContext();  // Must be implemented by application
    #endif
    
    // Now Coin3D offscreen rendering will work
    SoOffscreenRenderer renderer(SbViewportRegion(512, 512));
    // ... use renderer normally
    
    return 0;
}
*/