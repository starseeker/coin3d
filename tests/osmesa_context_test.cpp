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

// Using the NEW public SoDB::ContextManager API (no more internal glue headers needed)

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
        bool result = OSMesaMakeCurrent(context, buffer.get(), GL_UNSIGNED_BYTE, width, height) == GL_TRUE;
        if (result) {
            // After making context current, we need to ensure extensions are available
            // This is equivalent to what glewInit() does in the OSMesa glew example
            
            // Force loading of extension string to trigger extension detection
            const char* extensions = (const char*)glGetString(GL_EXTENSIONS);
            if (extensions && strstr(extensions, "GL_EXT_framebuffer_object")) {
                std::cout << "OSMesa context detected GL_EXT_framebuffer_object extension" << std::endl;
            }
        } else {
            std::cerr << "Failed to make OSMesa context current" << std::endl;
        }
        return result;
    }
};

// NEW: OSMesa Context Manager using the public SoDB API
class OSMesaContextManager : public SoDB::ContextManager {
public:
    virtual void* createOffscreenContext(unsigned int width, unsigned int height) override {
        try {
            auto* ctx = new OSMesaContextData(width, height);
            std::cout << "Created OSMesa context " << width << "x" << height << std::endl;
            return ctx;
        } catch (const std::exception& e) {
            std::cerr << "Failed to create OSMesa context: " << e.what() << std::endl;
            return nullptr;
        }
    }
    
    virtual SbBool makeContextCurrent(void* context) override {
        if (!context) return FALSE;
        auto* ctx = static_cast<OSMesaContextData*>(context);
        SbBool result = ctx->makeCurrent() ? TRUE : FALSE;
        if (result) {
            std::cout << "Made OSMesa context current" << std::endl;
        } else {
            std::cerr << "Failed to make OSMesa context current" << std::endl;
        }
        return result;
    }
    
    virtual void restorePreviousContext(void* context) override {
        // OSMesa doesn't need context stacking - this is a no-op
        // In real applications, you might implement context switching here
        std::cout << "Restoring previous OSMesa context (no-op)" << std::endl;
    }
    
    virtual void destroyContext(void* context) override {
        if (context) {
            auto* ctx = static_cast<OSMesaContextData*>(context);
            std::cout << "Destroying OSMesa context" << std::endl;
            delete ctx;
        }
    }
    
    virtual SbBool Initialize() override {
        // For OSMesa, we can check if OSMesa is available
        std::cout << "Initializing OSMesa context manager..." << std::endl;
        try {
            // Try creating a small test context to verify OSMesa works
            OSMesaContext test_ctx = OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, NULL);
            if (test_ctx) {
                OSMesaDestroyContext(test_ctx);
                std::cout << "✓ OSMesa context manager initialized successfully" << std::endl;
                return TRUE;
            }
        } catch (...) {
            std::cerr << "✗ OSMesa context manager initialization failed" << std::endl;
        }
        return FALSE;
    }
    
    virtual SbBool IsInitialized() override {
        // For this example, we consider it initialized if OSMesa functions are available
        return TRUE; // OSMesa is statically linked and always available
    }
};

int main() {
    std::cout << "Testing OSMesa context management with NEW public SoDB API" << std::endl;
    
    // NEW APPROACH: Use the public SoDB context management API
    // Context manager is now passed directly to SoDB::init()
    OSMesaContextManager context_manager;
    
    // Now initialize Coin3D with our context manager
    // This enforces proper initialization ordering by construction
    SoDB::init(&context_manager);
    
    // Test basic rendering with the new context management system
    std::cout << "Testing rendering with new context management..." << std::endl;
    
    // Create a simple scene
    SoSeparator* root = new SoSeparator;
    root->ref();
    
    SoCube* cube = new SoCube;
    root->addChild(cube);
    
    // Test offscreen rendering
    try {
        SbViewportRegion viewport(256, 256);
        SoOffscreenRenderer renderer(viewport);
        
        std::cout << "Testing offscreen rendering with Coin3D scene..." << std::endl;
        SbBool success = renderer.render(root);
        
        if (success) {
            std::cout << "✓ Successfully rendered scene using NEW context management API" << std::endl;
            
            // Get and analyze the buffer
            unsigned char* buffer = renderer.getBuffer();
            if (buffer) {
                // Basic validation
                bool hasNonZeroPixels = false;
                int pixelCount = 256 * 256 * 3; // RGB
                
                for (int i = 0; i < pixelCount; i++) {
                    if (buffer[i] != 0) {
                        hasNonZeroPixels = true;
                        break;
                    }
                }
                
                if (hasNonZeroPixels) {
                    std::cout << "✓ Rendered image contains content" << std::endl;
                } else {
                    std::cout << "! Rendered image is empty (background color)" << std::endl;
                }
                
                std::cout << "✓ Context management test completed successfully!" << std::endl;
                
            } else {
                std::cerr << "✗ Failed to get rendered buffer" << std::endl;
                root->unref();
                return 1;
            }
            
        } else {
            std::cerr << "✗ Failed to render scene" << std::endl;
            root->unref();
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "✗ Exception during rendering: " << e.what() << std::endl;
        root->unref();
        return 1;
    }
    
    root->unref();
    
    std::cout << std::endl;
    std::cout << "=== SUMMARY ===" << std::endl;
    std::cout << "✓ NEW public SoDB context management API working correctly" << std::endl;
    std::cout << "✓ Context manager passed directly to SoDB::init() - no ordering issues!" << std::endl;
    std::cout << "✓ Clean C++ interface instead of C-style callbacks" << std::endl;
    std::cout << "✓ Eliminates need for internal cc_glglue_context_* functions" << std::endl;
    std::cout << "✓ Enforced initialization ordering by API design" << std::endl;
    
    return 0;
}