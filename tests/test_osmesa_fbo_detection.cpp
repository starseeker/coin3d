/**
 * Minimal test to demonstrate OSMesa FBO extension detection issue
 * Based on OSMesa glew example pattern from https://github.com/starseeker/osmesa
 */

#ifdef COIN3D_OSMESA_BUILD

#include <OSMesa/osmesa.h>
#include <OSMesa/gl.h>
#include <iostream>
#include <cstring>
#include <Inventor/SoDB.h>

// Include glue system for context management and extension detection
#include "glue/glp.h"

// Simple OSMesa context manager following glew example pattern
struct SimpleOSMesaContext {
    OSMesaContext context;
    unsigned char* buffer;
    int width, height;
    
    SimpleOSMesaContext(int w, int h) : width(w), height(h) {
        // Create context like OSMesa glew example
        context = OSMesaCreateContext(OSMESA_RGBA, NULL);
        if (context) {
            buffer = new unsigned char[width * height * 4];
        } else {
            buffer = nullptr;
        }
    }
    
    ~SimpleOSMesaContext() {
        if (context) OSMesaDestroyContext(context);
        delete[] buffer;
    }
    
    bool makeCurrent() {
        if (!context || !buffer) return false;
        
        bool result = OSMesaMakeCurrent(context, buffer, GL_UNSIGNED_BYTE, width, height) == GL_TRUE;
        if (result) {
            std::cout << "OSMesa context made current" << std::endl;
            
            // Verify basic OpenGL state like glew example does
            std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
            std::cout << "OpenGL Vendor: " << glGetString(GL_VENDOR) << std::endl;
            std::cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << std::endl;
            
            // Test extension string like glew example
            const char* extensions = (const char*)glGetString(GL_EXTENSIONS);
            if (extensions) {
                std::cout << "Extension string length: " << strlen(extensions) << std::endl;
                
                if (strstr(extensions, "GL_EXT_framebuffer_object")) {
                    std::cout << "✓ GL_EXT_framebuffer_object found in extension string" << std::endl;
                    
                    // Test function loading like glew example does
                    void* genFBO = OSMesaGetProcAddress("glGenFramebuffersEXT");
                    void* bindFBO = OSMesaGetProcAddress("glBindFramebufferEXT");
                    void* checkFBO = OSMesaGetProcAddress("glCheckFramebufferStatusEXT");
                    
                    std::cout << "FBO function pointers:" << std::endl;
                    std::cout << "  glGenFramebuffersEXT: " << genFBO << std::endl;
                    std::cout << "  glBindFramebufferEXT: " << bindFBO << std::endl;
                    std::cout << "  glCheckFramebufferStatusEXT: " << checkFBO << std::endl;
                    
                    if (genFBO && bindFBO && checkFBO) {
                        std::cout << "✓ All FBO functions available via OSMesaGetProcAddress" << std::endl;
                    } else {
                        std::cout << "✗ Some FBO functions not available" << std::endl;
                    }
                } else {
                    std::cout << "✗ GL_EXT_framebuffer_object NOT found in extension string" << std::endl;
                }
            } else {
                std::cout << "✗ Extension string is NULL" << std::endl;
            }
        }
        return result;
    }
    
    bool isValid() const { return context != nullptr && buffer != nullptr; }
};

// Callback functions for Coin3D (following the pattern from osmesa_context_test.cpp)
static void* test_osmesa_create_offscreen(unsigned int width, unsigned int height) {
    std::cout << "Creating OSMesa context: " << width << "x" << height << std::endl;
    auto* ctx = new SimpleOSMesaContext(width, height);
    if (!ctx->isValid()) {
        std::cout << "✗ Failed to create valid OSMesa context" << std::endl;
        delete ctx;
        return nullptr;
    }
    std::cout << "✓ OSMesa context created successfully" << std::endl;
    return ctx;
}

static SbBool test_osmesa_make_current(void* context) {
    if (!context) return FALSE;
    
    std::cout << "Making OSMesa context current..." << std::endl;
    auto* ctx = static_cast<SimpleOSMesaContext*>(context);
    return ctx->makeCurrent() ? TRUE : FALSE;
}

static void test_osmesa_reinstate_previous(void* context) {
    // No-op for OSMesa
    (void)context;
}

static void test_osmesa_destruct(void* context) {
    if (context) {
        std::cout << "Destroying OSMesa context" << std::endl;
        delete static_cast<SimpleOSMesaContext*>(context);
    }
}

int main() {
    std::cout << "=== OSMesa FBO Extension Detection Test ===" << std::endl;
    std::cout << "Following pattern from OSMesa glew examples" << std::endl;
    
    // Set up callback functions BEFORE SoDB::init()
    cc_glglue_offscreen_cb_functions callbacks = {
        test_osmesa_create_offscreen,
        test_osmesa_make_current,
        test_osmesa_reinstate_previous,
        test_osmesa_destruct
    };
    
    std::cout << "Registering OSMesa callbacks..." << std::endl;
    cc_glglue_context_set_offscreen_cb_functions(&callbacks);
    
    std::cout << "Initializing Coin3D..." << std::endl;
    SoDB::init();
    
    std::cout << "Creating offscreen context via Coin3D API..." << std::endl;
    void* ctx = cc_glglue_context_create_offscreen(256, 256);
    if (!ctx) {
        std::cerr << "✗ Failed to create offscreen context" << std::endl;
        return 1;
    }
    
    std::cout << "Making context current via Coin3D API..." << std::endl;
    if (!cc_glglue_context_make_current(ctx)) {
        std::cerr << "✗ Failed to make context current" << std::endl;
        cc_glglue_context_destruct(ctx);
        return 1;
    }
    
    std::cout << "Getting cc_glglue instance for extension detection..." << std::endl;
    const cc_glglue* glue = cc_glglue_instance(1);
    if (!glue) {
        std::cerr << "✗ Failed to get cc_glglue instance" << std::endl;
        cc_glglue_context_destruct(ctx);
        return 1;
    }
    
    std::cout << "Testing Coin3D FBO extension detection..." << std::endl;
    if (cc_glglue_has_framebuffer_objects(glue)) {
        std::cout << "✓ SUCCESS: cc_glglue_has_framebuffer_objects() returns TRUE" << std::endl;
    } else {
        std::cout << "✗ ISSUE: cc_glglue_has_framebuffer_objects() returns FALSE" << std::endl;
        std::cout << "This is the core issue - OSMesa supports FBOs but Coin3D doesn't detect them" << std::endl;
    }
    
    // Test Coin3D's function loading
    void* coinGenFBO = cc_glglue_getprocaddress(glue, "glGenFramebuffersEXT");
    void* coinBindFBO = cc_glglue_getprocaddress(glue, "glBindFramebufferEXT");
    
    std::cout << "Coin3D function loading:" << std::endl;
    std::cout << "  cc_glglue_getprocaddress('glGenFramebuffersEXT'): " << coinGenFBO << std::endl;
    std::cout << "  cc_glglue_getprocaddress('glBindFramebufferEXT'): " << coinBindFBO << std::endl;
    
    // Cleanup
    cc_glglue_context_destruct(ctx);
    
    std::cout << "=== Test Complete ===" << std::endl;
    return 0;
}

#else
#include <iostream>
int main() {
    std::cout << "This test requires COIN3D_OSMESA_BUILD" << std::endl;
    return 0;
}
#endif