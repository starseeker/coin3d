/*
 * Test program to reproduce OSMesa + Coin3D texture crash
 * This will help isolate whether it's an OSMesa bug or a Coin3D integration issue
 */

#include <iostream>
#include <memory>
#include <cstring>

// OSMesa headers for headless rendering
#ifdef __has_include
  #if __has_include(<OSMesa/osmesa.h>)
    #include <OSMesa/osmesa.h>
    #include <OSMesa/gl.h>
    #define HAVE_OSMESA
  #endif
#endif

// Coin3D headers
#include <Inventor/SoDB.h>
#include <Inventor/SoInteraction.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTextureCoordinateDefault.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/SbViewportRegion.h>

#ifdef HAVE_OSMESA

// OSMesa context wrapper
struct OSMesaContextData {
    OSMesaContext context;
    std::unique_ptr<unsigned char[]> buffer;
    int width, height;
    
    OSMesaContextData(int w, int h) : width(w), height(h) {
        context = OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, NULL);
        if (context) {
            buffer = std::make_unique<unsigned char[]>(width * height * 4);
        }
    }
    
    ~OSMesaContextData() {
        if (context) OSMesaDestroyContext(context);
    }
    
    bool makeCurrent() {
        if (!context || !buffer) return false;
        
        bool result = OSMesaMakeCurrent(context, buffer.get(), GL_UNSIGNED_BYTE, width, height);
        if (result) {
            // Set Y-axis orientation for proper image output
            OSMesaPixelStore(OSMESA_Y_UP, 0);
            
            // Clear any initial GL errors
            GLenum error;
            while ((error = glGetError()) != GL_NO_ERROR) {
                std::cout << "Clearing initial GL error: 0x" << std::hex << error << std::dec << std::endl;
            }
        }
        return result;
    }
    
    bool isValid() const { return context != nullptr; }
    
    const unsigned char* getBuffer() const { return buffer.get(); }
};

// OSMesa Context Manager for Coin3D
class OSMesaContextManager : public SoDB::ContextManager {
public:
    virtual void* createOffscreenContext(unsigned int width, unsigned int height) override {
        auto* ctx = new OSMesaContextData(width, height);
        return ctx->isValid() ? ctx : (delete ctx, nullptr);
    }
    
    virtual SbBool makeContextCurrent(void* context) override {
        return context && static_cast<OSMesaContextData*>(context)->makeCurrent() ? TRUE : FALSE;
    }
    
    virtual void restorePreviousContext(void* context) override {
        // OSMesa doesn't need context stacking for single-threaded use
        (void)context;
    }
    
    virtual void destroyContext(void* context) override {
        delete static_cast<OSMesaContextData*>(context);
    }
};

// Generate a simple texture pattern 
void generateSimpleTexture(int width, int height, unsigned char* data) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 3;  // RGB format
            data[idx] = (unsigned char)((x * 255) / width);     // R - red gradient
            data[idx + 1] = (unsigned char)((y * 255) / height); // G - green gradient  
            data[idx + 2] = 128;     // B - constant blue
        }
    }
}

void test_coin3d_texture_with_osmesa() {
    std::cout << "=== Testing Coin3D Texture Rendering with OSMesa ===" << std::endl;
    
    // Initialize Coin3D with OSMesa context management
    std::unique_ptr<OSMesaContextManager> context_manager = std::make_unique<OSMesaContextManager>();
    SoDB::init(context_manager.get());
    SoInteraction::init();
    
    std::cout << "✓ Coin3D initialized with OSMesa context manager" << std::endl;
    
    SoSeparator *root = new SoSeparator;
    SoPerspectiveCamera *myCamera = new SoPerspectiveCamera;
    root->ref();
    root->addChild(myCamera);
    root->addChild(new SoDirectionalLight);

    // Generate a simple test texture
    const int texWidth = 64;
    const int texHeight = 64;
    unsigned char* textureData = new unsigned char[texWidth * texHeight * 3];
    generateSimpleTexture(texWidth, texHeight, textureData);
    
    std::cout << "✓ Generated " << texWidth << "x" << texHeight << " test texture" << std::endl;

    // Create texture using Coin3D API  
    SoTexture2 *testTexture = new SoTexture2;
    testTexture->ref();
    
    // Use the new API to set texture data
    testTexture->setImageData(texWidth, texHeight, 3, textureData);
    
    std::cout << "✓ Texture created using setImageData() API" << std::endl;
    
    // Add texture coordinate generator
    SoTextureCoordinateDefault *texCoord = new SoTextureCoordinateDefault;
    
    // CRITICAL: This is where the potential crash occurs - adding texture to scene graph
    std::cout << "Adding texture to scene graph..." << std::endl;
    root->addChild(testTexture);     // This may cause OSMesa texture issues
    root->addChild(texCoord);
    root->addChild(new SoCube);
    
    std::cout << "✓ Texture added to scene graph without immediate crash" << std::endl;

    // Set up offscreen renderer 
    const int width = 256;
    const int height = 256;
    SbViewportRegion viewport(width, height);
    SoOffscreenRenderer renderer(viewport);
    renderer.setBackgroundColor(SbColor(0.1f, 0.2f, 0.3f)); // Dark blue background

    // Make camera see everything
    myCamera->viewAll(root, viewport);
    
    std::cout << "Attempting to render scene with texture..." << std::endl;

    // CRITICAL: This is where the crash typically occurs during rendering
    SbBool success = FALSE;
    try {
        success = renderer.render(root);
        std::cout << "✓ Render call completed without crash" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "✗ C++ exception during render: " << e.what() << std::endl;
    } catch (...) {
        std::cout << "✗ Unknown exception during render" << std::endl;
    }

    if (success) {
        std::cout << "✓ Render successful - attempting to save output" << std::endl;
        
        // Try to save output
        SbBool saveResult = renderer.writeToRGB("osmesa_texture_test.rgb");
        if (saveResult) {
            std::cout << "✓ Successfully saved rendered texture to osmesa_texture_test.rgb" << std::endl;
        } else {
            std::cout << "✗ Failed to save rendered output" << std::endl;
        }
    } else {
        std::cout << "✗ Render failed - this indicates the OSMesa texture problem" << std::endl;
        
        // Check for OpenGL errors
        GLenum glError = glGetError();
        if (glError != GL_NO_ERROR) {
            std::cout << "OpenGL error detected: 0x" << std::hex << glError << std::dec << std::endl;
            switch (glError) {
                case GL_INVALID_OPERATION:
                    std::cout << "  GL_INVALID_OPERATION - possibly texture-related" << std::endl;
                    break;
                case GL_INVALID_VALUE:
                    std::cout << "  GL_INVALID_VALUE - parameter error" << std::endl;
                    break;
                case GL_OUT_OF_MEMORY:
                    std::cout << "  GL_OUT_OF_MEMORY - memory allocation failed" << std::endl;
                    break;
                default:
                    std::cout << "  Unknown OpenGL error" << std::endl;
            }
        }
    }

    // Clean up
    delete[] textureData;
    testTexture->unref();
    root->unref();
    
    std::cout << "✓ Cleanup completed" << std::endl;
}

void test_direct_osmesa_texture() {
    std::cout << "\n=== Testing Direct OSMesa Texture Operations ===" << std::endl;
    
    // Create OSMesa context directly
    OSMesaContext ctx = OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, NULL);
    if (!ctx) {
        std::cout << "✗ Failed to create OSMesa context" << std::endl;
        return;
    }
    
    const int width = 128;
    const int height = 128;
    std::unique_ptr<unsigned char[]> buffer = std::make_unique<unsigned char[]>(width * height * 4);
    
    if (!OSMesaMakeCurrent(ctx, buffer.get(), GL_UNSIGNED_BYTE, width, height)) {
        std::cout << "✗ Failed to make OSMesa context current" << std::endl;
        OSMesaDestroyContext(ctx);
        return;
    }
    
    std::cout << "✓ OSMesa context created and made current" << std::endl;
    
    // Clear any existing errors
    GLenum error;
    while ((error = glGetError()) != GL_NO_ERROR) {
        std::cout << "Clearing initial error: 0x" << std::hex << error << std::dec << std::endl;
    }
    
    // Test basic OpenGL texture operations
    GLuint textureId;
    glGenTextures(1, &textureId);
    error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cout << "✗ glGenTextures failed with error 0x" << std::hex << error << std::dec << std::endl;
    } else {
        std::cout << "✓ glGenTextures successful, texture ID: " << textureId << std::endl;
    }
    
    glBindTexture(GL_TEXTURE_2D, textureId);
    error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cout << "✗ glBindTexture failed with error 0x" << std::hex << error << std::dec << std::endl;
    } else {
        std::cout << "✓ glBindTexture successful" << std::endl;
    }
    
    // Create simple texture data
    const int texSize = 32;
    std::unique_ptr<unsigned char[]> texData = std::make_unique<unsigned char[]>(texSize * texSize * 3);
    generateSimpleTexture(texSize, texSize, texData.get());
    
    // Upload texture data
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texSize, texSize, 0, GL_RGB, GL_UNSIGNED_BYTE, texData.get());
    error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cout << "✗ glTexImage2D failed with error 0x" << std::hex << error << std::dec << std::endl;
        
        // Additional diagnostics
        if (error == GL_INVALID_OPERATION) {
            std::cout << "  This suggests OSMesa may have texture format/size limitations" << std::endl;
        }
    } else {
        std::cout << "✓ glTexImage2D successful - OSMesa texture upload works!" << std::endl;
    }
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cout << "✗ Texture parameter setup failed with error 0x" << std::hex << error << std::dec << std::endl;
    } else {
        std::cout << "✓ Texture parameters set successfully" << std::endl;
    }
    
    // Test texture enable/disable
    glEnable(GL_TEXTURE_2D);
    error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cout << "✗ glEnable(GL_TEXTURE_2D) failed with error 0x" << std::hex << error << std::dec << std::endl;
    } else {
        std::cout << "✓ Texture 2D enabled successfully" << std::endl;
    }
    
    // Cleanup
    glDeleteTextures(1, &textureId);
    OSMesaDestroyContext(ctx);
    
    std::cout << "✓ Direct OSMesa texture test completed" << std::endl;
}

#endif // HAVE_OSMESA

int main() {
    std::cout << "OSMesa + Coin3D Texture Debugging Tool" << std::endl;
    std::cout << "======================================" << std::endl;
    
#ifdef HAVE_OSMESA
    // Test 1: Direct OSMesa texture operations (isolate OSMesa issues)
    test_direct_osmesa_texture();
    
    // Test 2: Coin3D texture operations with OSMesa (isolate integration issues)
    test_coin3d_texture_with_osmesa();
    
    std::cout << "\n=== SUMMARY ===" << std::endl;
    std::cout << "If direct OSMesa texture test passes but Coin3D test fails," << std::endl;
    std::cout << "then the issue is in Coin3D's OSMesa integration." << std::endl;
    std::cout << "If both tests fail, then it's an OSMesa limitation." << std::endl;
    
#else
    std::cout << "OSMesa not available - cannot run texture debugging tests" << std::endl;
    return 1;
#endif
    
    return 0;
}