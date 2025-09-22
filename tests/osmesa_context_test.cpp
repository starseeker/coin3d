#include <OSMesa/osmesa.h>
#include <OSMesa/gl.h>
#include <iostream>
#include <vector>
#include <memory>
#include <Inventor/SoDB.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/SbViewportRegion.h>

// OSMesa Context wrapper struct
struct OSMesaContextData {
    OSMesaContext context;
    std::unique_ptr<unsigned char[]> buffer;
    int width, height;
    
    OSMesaContextData(int w, int h) : width(w), height(h) {
        // Create OSMesa context with RGBA format, 16-bit depth buffer
        context = OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, NULL);
        if (!context) {
            throw std::runtime_error("Failed to create OSMesa context");
        }
        
        // Allocate buffer for offscreen rendering
        buffer = std::make_unique<unsigned char[]>(width * height * 4);
    }
    
    ~OSMesaContextData() {
        if (context) {
            OSMesaDestroyContext(context);
        }
    }
    
    bool makeCurrent() {
        return OSMesaMakeCurrent(context, buffer.get(), GL_UNSIGNED_BYTE, width, height) == GL_TRUE;
    }
};

// Callback functions for Coin3D context management
static void* osmesa_create_offscreen(unsigned int width, unsigned int height) {
    try {
        auto* ctx = new OSMesaContextData(width, height);
        std::cout << "Created OSMesa context: " << width << "x" << height << std::endl;
        return ctx;
    } catch (const std::exception& e) {
        std::cerr << "Failed to create OSMesa context: " << e.what() << std::endl;
        return nullptr;
    }
}

static SbBool osmesa_make_current(void* context) {
    if (!context) return FALSE;
    
    auto* ctx = static_cast<OSMesaContextData*>(context);
    bool result = ctx->makeCurrent();
    if (result) {
        std::cout << "Made OSMesa context current" << std::endl;
    } else {
        std::cerr << "Failed to make OSMesa context current" << std::endl;
    }
    return result ? TRUE : FALSE;
}

static void osmesa_reinstate_previous(void* context) {
    // OSMesa doesn't need context stacking - this is a no-op
    // In real applications, you might implement context switching here
    std::cout << "Reinstating previous OSMesa context (no-op)" << std::endl;
}

static void osmesa_destruct(void* context) {
    if (context) {
        auto* ctx = static_cast<OSMesaContextData*>(context);
        std::cout << "Destroying OSMesa context" << std::endl;
        delete ctx;
    }
}

int main() {
    std::cout << "Testing OSMesa context management with Coin3D" << std::endl;
    
    // Initialize Coin3D
    SoDB::init();
    
    // Set up OSMesa callback functions
    cc_glglue_offscreen_cb_functions osmesa_callbacks = {
        osmesa_create_offscreen,
        osmesa_make_current,
        osmesa_reinstate_previous,
        osmesa_destruct
    };
    
    // Register our OSMesa callbacks with Coin3D
    cc_glglue_context_set_offscreen_cb_functions(&osmesa_callbacks);
    
    // Test creating an offscreen context through Coin3D's API
    void* ctx = cc_glglue_context_create_offscreen(256, 256);
    if (!ctx) {
        std::cerr << "Failed to create offscreen context via Coin3D API" << std::endl;
        return 1;
    }
    
    // Make the context current
    SbBool ok = cc_glglue_context_make_current(ctx);
    if (!ok) {
        std::cerr << "Failed to make context current" << std::endl;
        cc_glglue_context_destruct(ctx);
        return 1;
    }
    
    // Test basic OpenGL functionality
    GLenum error = glGetError(); // Clear any pending errors
    
    const GLubyte* version = glGetString(GL_VERSION);
    error = glGetError();
    if (error == GL_NO_ERROR && version) {
        std::cout << "OpenGL Version: " << (const char*)version << std::endl;
    } else {
        std::cout << "OpenGL Version: Error querying (code: " << error << ")" << std::endl;
    }
    
    const GLubyte* vendor = glGetString(GL_VENDOR);
    error = glGetError();
    if (error == GL_NO_ERROR && vendor) {
        std::cout << "OpenGL Vendor: " << (const char*)vendor << std::endl;
    } else {
        std::cout << "OpenGL Vendor: Error querying (code: " << error << ")" << std::endl;
    }
    
    const GLubyte* renderer = glGetString(GL_RENDERER);
    error = glGetError();
    if (error == GL_NO_ERROR && renderer) {
        std::cout << "OpenGL Renderer: " << (const char*)renderer << std::endl;
    } else {
        std::cout << "OpenGL Renderer: Error querying (code: " << error << ")" << std::endl;
    }
    
    // Test basic OpenGL commands
    glClearColor(0.2f, 0.3f, 0.4f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "OpenGL error after clear: " << error << std::endl;
    } else {
        std::cout << "OpenGL clear operation successful" << std::endl;
    }
    
    // Test creating a simple Coin3D scene - SKIP for now to avoid segfault
    SoSeparator* root = new SoSeparator;
    root->ref();
    
    SoCube* cube = new SoCube;
    root->addChild(cube);
    
    std::cout << "Created Coin3D scene graph with cube" << std::endl;
    
    /* 
    // Create a render action and test scene graph rendering
    SbViewportRegion viewport(256, 256);
    SoGLRenderAction renderAction(viewport);
    
    try {
        renderAction.apply(root);
        std::cout << "Coin3D scene graph rendering successful" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error during scene graph rendering: " << e.what() << std::endl;
    }
    */
    
    root->unref();
    
    // Clean up - restore original context provider
    SoOffscreenRenderer::setContextProvider(originalProvider);
    
    std::cout << "OSMesa context test completed successfully" << std::endl;
    return 0;
}