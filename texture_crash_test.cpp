/*
 * Test program to demonstrate the actual OSMesa + Coin3D texture crash
 * This version actually tries to render textures to reproduce the issue
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
void generateCheckerboardTexture(int width, int height, unsigned char* data) {
    const int checkerSize = 8;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            bool checkX = (x / checkerSize) % 2;
            bool checkY = (y / checkerSize) % 2;
            bool isWhite = checkX ^ checkY;
            
            int idx = (y * width + x) * 3;  // RGB format
            if (isWhite) {
                data[idx] = 255;     // R - white
                data[idx + 1] = 255; // G 
                data[idx + 2] = 255; // B
            } else {
                data[idx] = 255;     // R - red
                data[idx + 1] = 0;   // G
                data[idx + 2] = 0;   // B
            }
        }
    }
}

#endif // HAVE_OSMESA

int main() {
    std::cout << "OSMesa + Coin3D Texture Crash Reproduction Test" << std::endl;
    std::cout << "===============================================" << std::endl;
    
#ifdef HAVE_OSMESA
    std::cout << "Testing actual texture rendering that may crash..." << std::endl;
    
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

    // Generate texture
    const int texWidth = 64;
    const int texHeight = 64;
    unsigned char* textureData = new unsigned char[texWidth * texHeight * 3];
    generateCheckerboardTexture(texWidth, texHeight, textureData);
    
    std::cout << "✓ Generated " << texWidth << "x" << texHeight << " checkerboard texture" << std::endl;

    // Create texture using Coin3D API  
    SoTexture2 *testTexture = new SoTexture2;
    testTexture->ref();
    
    // Use the new API to set texture data
    testTexture->setImageData(texWidth, texHeight, 3, textureData);
    
    std::cout << "✓ Texture created using setImageData() API" << std::endl;
    
    // Add texture coordinate generator
    SoTextureCoordinateDefault *texCoord = new SoTextureCoordinateDefault;
    
    // CRITICAL: This is where we actually use the texture in the scene graph
    // Unlike the current BasicTexture.headless example which avoids this
    std::cout << "Adding texture to scene graph (this may cause issues with OSMesa)..." << std::endl;
    root->addChild(testTexture);     // The problematic step
    root->addChild(texCoord);
    root->addChild(new SoCube);
    
    std::cout << "✓ Texture added to scene graph" << std::endl;

    // Set up offscreen renderer 
    const int width = 512;
    const int height = 512;
    SbViewportRegion viewport(width, height);
    SoOffscreenRenderer renderer(viewport);
    renderer.setBackgroundColor(SbColor(0.1f, 0.2f, 0.3f)); // Dark blue background

    // Make camera see everything
    myCamera->viewAll(root, viewport);
    
    std::cout << "Attempting to render scene with texture (potential crash point)..." << std::endl;

    // CRITICAL: This render call may crash or fail with OSMesa texture issues
    SbBool success = FALSE;
    try {
        success = renderer.render(root);
        std::cout << "✓ Render call completed successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "✗ C++ exception during render: " << e.what() << std::endl;
    } catch (...) {
        std::cout << "✗ Unknown exception during render" << std::endl;
    }

    if (success) {
        std::cout << "✓ Render successful - texture rendering worked with OSMesa!" << std::endl;
        
        // Try to save output
        SbBool saveResult = renderer.writeToRGB("successful_texture_render.rgb");
        if (saveResult) {
            std::cout << "✓ Successfully saved textured cube to successful_texture_render.rgb" << std::endl;
            std::cout << "✓ THIS MEANS THE TEXTURE + OSMESA ISSUE IS RESOLVED!" << std::endl;
        } else {
            std::cout << "✗ Render succeeded but failed to save output" << std::endl;
        }
    } else {
        std::cout << "✗ Render failed - this confirms the OSMesa texture problem" << std::endl;
        
        // Check for OpenGL errors
        GLenum glError = glGetError();
        if (glError != GL_NO_ERROR) {
            std::cout << "OpenGL error detected: 0x" << std::hex << glError << std::dec << " (";
            switch (glError) {
                case GL_INVALID_OPERATION:
                    std::cout << "GL_INVALID_OPERATION";
                    break;
                case GL_INVALID_VALUE:
                    std::cout << "GL_INVALID_VALUE";
                    break;
                case GL_OUT_OF_MEMORY:
                    std::cout << "GL_OUT_OF_MEMORY";
                    break;
                default:
                    std::cout << "Unknown error";
            }
            std::cout << ")" << std::endl;
        }
    }

    // Clean up
    delete[] textureData;
    testTexture->unref();
    root->unref();
    
    std::cout << "✓ Test completed" << std::endl;
    
#else
    std::cout << "OSMesa not available - cannot run texture debugging tests" << std::endl;
    return 1;
#endif
    
    return 0;
}