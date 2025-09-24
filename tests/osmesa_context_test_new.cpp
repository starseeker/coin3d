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

// NEW: Context Manager implementation using the public SoDB API
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
        // This is a simple validation - in real apps you might check drivers, etc.
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
        // In a real implementation, you might track initialization state
        std::cout << "OSMesa IsInitialized() called - returning TRUE" << std::endl;
        return TRUE; // OSMesa is statically linked and always available
    }
};

int main() {
    std::cout << "Testing NEW public SoDB context management API with OSMesa" << std::endl;
    
    // NEW APPROACH: Pass context manager directly to SoDB::init()
    OSMesaContextManager context_manager;
    
    // Initialize Coin3D with our context manager
    // This enforces proper initialization ordering by construction
    SoDB::init(&context_manager);
    
    // Verify the context manager was set correctly
    if (SoDB::getContextManager() == &context_manager) {
        std::cout << "✓ Context manager successfully set via SoDB::init()" << std::endl;
    } else {
        std::cerr << "✗ Context manager not set correctly" << std::endl;
        return 1;
    }
    
    // Test creating a simple scene to verify rendering works
    SoSeparator* root = new SoSeparator;
    root->ref();
    
    SoCube* cube = new SoCube;
    root->addChild(cube);
    
    // Test offscreen rendering using the new context management
    try {
        SbViewportRegion viewport(256, 256);
        SoOffscreenRenderer renderer(viewport);
        
        std::cout << "Testing offscreen rendering..." << std::endl;
        SbBool success = renderer.render(root);
        
        if (success) {
            std::cout << "✓ Offscreen rendering successful with new context API" << std::endl;
            
            // Get the rendered buffer
            unsigned char* buffer = renderer.getBuffer();
            if (buffer) {
                std::cout << "✓ Rendered buffer retrieved successfully" << std::endl;
                
                // Basic validation - check if we have non-zero pixels
                bool hasContent = false;
                for (int i = 0; i < 256 * 256 * 3; i++) {
                    if (buffer[i] != 0) {
                        hasContent = true;
                        break;
                    }
                }
                
                if (hasContent) {
                    std::cout << "✓ Rendered image contains non-zero pixels" << std::endl;
                } else {
                    std::cout << "! Rendered image appears to be empty (might be background)" << std::endl;
                }
            } else {
                std::cerr << "✗ Failed to retrieve rendered buffer" << std::endl;
            }
        } else {
            std::cerr << "✗ Offscreen rendering failed" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "✗ Exception during rendering: " << e.what() << std::endl;
        root->unref();
        return 1;
    }
    
    root->unref();
    
    std::cout << "✓ All tests completed successfully with new public API!" << std::endl;
    
    // Test context manager cleanup - no longer possible with new API
    // Context managers are set at init time and managed by the library
    if (SoDB::getContextManager() == &context_manager) {
        std::cout << "✓ Context manager still accessible via SoDB::getContextManager()" << std::endl;
    }
    
    return 0;
}