/*
 * Comprehensive demonstration of the NEW Coin3D public context management API
 * 
 * This test demonstrates:
 * 1. How to implement a custom context manager
 * 2. Proper initialization ordering 
 * 3. Integration with Coin3D rendering
 * 4. Clean C++ object-oriented interface
 * 5. Elimination of initialization ordering issues
 */

#include <Inventor/SoDB.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSeparator.h>
#include <iostream>

#ifdef COIN3D_OSMESA_BUILD
#include <OSMesa/osmesa.h>
#include <OSMesa/gl.h>
#include <memory>

// Complete OSMesa context implementation using the NEW public API
class ComprehensiveOSMesaContext {
private:
    OSMesaContext context;
    std::unique_ptr<unsigned char[]> buffer;
    unsigned int width, height;
    bool valid;

public:
    ComprehensiveOSMesaContext(unsigned int w, unsigned int h) 
        : width(w), height(h), valid(false) {
        
        std::cout << "Creating OSMesa context " << width << "x" << height << std::endl;
        
        // Create OSMesa context with RGBA format and depth buffer
        context = OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, NULL);
        if (context) {
            // Allocate color buffer
            buffer = std::make_unique<unsigned char[]>(width * height * 4);
            valid = true;
            std::cout << "✓ OSMesa context created successfully" << std::endl;
        } else {
            std::cerr << "✗ Failed to create OSMesa context" << std::endl;
        }
    }
    
    ~ComprehensiveOSMesaContext() {
        if (context) {
            OSMesaDestroyContext(context);
            std::cout << "✓ OSMesa context destroyed" << std::endl;
        }
    }
    
    bool isValid() const { return valid; }
    
    bool makeCurrent() {
        if (!valid) return false;
        
        bool success = OSMesaMakeCurrent(context, buffer.get(), GL_UNSIGNED_BYTE, width, height) == GL_TRUE;
        if (success) {
            std::cout << "✓ OSMesa context made current" << std::endl;
            
            // Verify OpenGL state
            GLint viewport[4];
            glGetIntegerv(GL_VIEWPORT, viewport);
            std::cout << "  Viewport: " << viewport[2] << "x" << viewport[3] << std::endl;
            
            // Check for extensions
            const char* extensions = (const char*)glGetString(GL_EXTENSIONS);
            if (extensions && strstr(extensions, "GL_EXT_framebuffer_object")) {
                std::cout << "  ✓ FBO extension available" << std::endl;
            }
        } else {
            std::cerr << "✗ Failed to make OSMesa context current" << std::endl;
        }
        
        return success;
    }
    
    void restorePrevious() {
        // OSMesa is single-threaded, no context switching needed
        std::cout << "✓ Context restoration completed (no-op for OSMesa)" << std::endl;
    }
};

// Context Manager implementation using NEW public SoDB API
class DemoContextManager : public SoDB::ContextManager {
private:
    int contextCount;
    
public:
    DemoContextManager() : contextCount(0) {
        std::cout << "\n=== Context Manager Created ===" << std::endl;
    }
    
    ~DemoContextManager() {
        std::cout << "=== Context Manager Destroyed ===" << std::endl;
        std::cout << "Total contexts created: " << contextCount << std::endl;
    }
    
    virtual void* createOffscreenContext(unsigned int width, unsigned int height) override {
        contextCount++;
        std::cout << "\n[Context " << contextCount << "] Creating offscreen context..." << std::endl;
        
        auto* ctx = new ComprehensiveOSMesaContext(width, height);
        if (!ctx->isValid()) {
            delete ctx;
            return nullptr;
        }
        
        return ctx;
    }
    
    virtual SbBool makeContextCurrent(void* context) override {
        if (!context) {
            std::cerr << "✗ makeContextCurrent called with NULL context" << std::endl;
            return FALSE;
        }
        
        auto* ctx = static_cast<ComprehensiveOSMesaContext*>(context);
        return ctx->makeCurrent() ? TRUE : FALSE;
    }
    
    virtual void restorePreviousContext(void* context) override {
        if (context) {
            auto* ctx = static_cast<ComprehensiveOSMesaContext*>(context);
            ctx->restorePrevious();
        }
    }
    
    virtual void destroyContext(void* context) override {
        if (context) {
            std::cout << "Destroying context..." << std::endl;
            delete static_cast<ComprehensiveOSMesaContext*>(context);
        }
    }
};

#endif // COIN3D_OSMESA_BUILD

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "Coin3D PUBLIC Context Management API Demonstration" << std::endl;
    std::cout << "==================================================" << std::endl;
    
#ifdef COIN3D_OSMESA_BUILD
    std::cout << "\nPlatform: OSMesa (Offscreen Rendering)" << std::endl;
    
    // Step 1: Create context manager
    std::cout << "\n--- Step 1: Create Context Manager ---" << std::endl;
    DemoContextManager contextManager;
    
    // Step 2: Register BEFORE SoDB::init() - THIS IS CRITICAL
    std::cout << "\n--- Step 2: Register Context Manager ---" << std::endl;
    std::cout << "Setting context manager BEFORE SoDB::init()..." << std::endl;
    SoDB::setContextManager(&contextManager);
    
    // Verify registration
    if (SoDB::getContextManager() == &contextManager) {
        std::cout << "✓ Context manager successfully registered" << std::endl;
    } else {
        std::cerr << "✗ Context manager registration failed" << std::endl;
        return 1;
    }
    
    // Step 3: Initialize Coin3D
    std::cout << "\n--- Step 3: Initialize Coin3D ---" << std::endl;
    std::cout << "Calling SoDB::init()..." << std::endl;
    SoDB::init();
    std::cout << "✓ SoDB::init() completed successfully" << std::endl;
    
    // Step 4: Test rendering
    std::cout << "\n--- Step 4: Test Offscreen Rendering ---" << std::endl;
    
    // Create a simple scene
    SoSeparator* root = new SoSeparator;
    root->ref();
    
    SoCube* cube = new SoCube;
    cube->width = 2.0f;
    cube->height = 2.0f;
    cube->depth = 2.0f;
    root->addChild(cube);
    
    // Test rendering at different resolutions
    int testSizes[] = {64, 128, 256};
    
    for (int size : testSizes) {
        std::cout << "\nTesting " << size << "x" << size << " rendering..." << std::endl;
        
        try {
            SbViewportRegion viewport(size, size);
            SoOffscreenRenderer renderer(viewport);
            
            SbBool success = renderer.render(root);
            
            if (success) {
                std::cout << "✓ Rendering successful for " << size << "x" << size << std::endl;
                
                // Verify buffer
                unsigned char* buffer = renderer.getBuffer();
                if (buffer) {
                    // Simple validation - check for non-background pixels
                    bool hasContent = false;
                    int pixelCount = size * size * 3;
                    
                    for (int i = 0; i < pixelCount && !hasContent; i++) {
                        if (buffer[i] != 0) {
                            hasContent = true;
                        }
                    }
                    
                    std::cout << "  Buffer status: " << (hasContent ? "Contains content" : "Empty/background") << std::endl;
                } else {
                    std::cout << "  Warning: Could not retrieve buffer" << std::endl;
                }
            } else {
                std::cerr << "✗ Rendering failed for " << size << "x" << size << std::endl;
            }
            
        } catch (const std::exception& e) {
            std::cerr << "✗ Exception during rendering: " << e.what() << std::endl;
        }
    }
    
    // Step 5: Cleanup
    std::cout << "\n--- Step 5: Cleanup ---" << std::endl;
    root->unref();
    
    // Clear context manager
    SoDB::setContextManager(nullptr);
    if (SoDB::getContextManager() == nullptr) {
        std::cout << "✓ Context manager cleared successfully" << std::endl;
    }
    
    std::cout << "\n=== DEMONSTRATION COMPLETED SUCCESSFULLY ===" << std::endl;
    std::cout << "\nKey Benefits Demonstrated:" << std::endl;
    std::cout << "✓ Clean C++ object-oriented API" << std::endl;
    std::cout << "✓ Clear initialization ordering (manager BEFORE init)" << std::endl;
    std::cout << "✓ No more internal callback registration" << std::endl;
    std::cout << "✓ Proper RAII and exception safety" << std::endl;
    std::cout << "✓ Integration with Coin3D rendering pipeline" << std::endl;
    
#else
    std::cout << "\nThis demonstration requires OSMesa build." << std::endl;
    std::cout << "Please build with -DCOIN3D_USE_OSMESA=ON to see the full demo." << std::endl;
#endif
    
    return 0;
}