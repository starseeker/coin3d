/*
 * Modified version of BasicTexture.headless that actually uses textures
 * This will help us identify the exact issue with OSMesa texture rendering
 */

#include <iostream>
#include <memory>

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

// Save RGB buffer to RGB file using built-in SGI RGB format
bool saveRGB(const std::string& filename, SoOffscreenRenderer* renderer) {
    SbBool result = renderer->writeToRGB(filename.c_str());
    if (result) {
        std::cout << "RGB saved to: " << filename << std::endl;
        return true;
    } else {
        std::cerr << "Error: Could not save RGB file " << filename << std::endl;
        return false;
    }
}

#endif // HAVE_OSMESA

// Generate a checkerboard texture pattern in memory
void generateCheckerboardTexture(int width, int height, unsigned char* data) {
    const int checkerSize = 16;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            bool checkX = (x / checkerSize) % 2;
            bool checkY = (y / checkerSize) % 2;
            bool isWhite = checkX ^ checkY;
            
            int idx = (y * width + x) * 3;  // RGB format
            if (isWhite) {
                data[idx] = 220;     // R - brick color
                data[idx + 1] = 180; // G 
                data[idx + 2] = 100; // B
            } else {
                data[idx] = 140;     // R - mortar color
                data[idx + 1] = 60;  // G
                data[idx + 2] = 30;  // B
            }
        }
    }
}

int
main(int argc, char **argv)
{
#ifdef HAVE_OSMESA
    // Initialize Coin3D with OSMesa context management
    std::unique_ptr<OSMesaContextManager> context_manager = std::make_unique<OSMesaContextManager>();
    SoDB::init(context_manager.get());
    SoInteraction::init();
    
    std::cout << "BasicTexture: Testing ACTUAL texture rendering with OSMesa" << std::endl;
    std::cout << "This version attempts real texture rendering to identify the crash" << std::endl;

    SoSeparator *root = new SoSeparator;
    SoPerspectiveCamera *myCamera = new SoPerspectiveCamera;
    root->ref();
    root->addChild(myCamera);
    root->addChild(new SoDirectionalLight);

    // Generate and demonstrate the NEW Coin 4.1 setImageData() API
    const int texWidth = 128;
    const int texHeight = 128;
    unsigned char* textureData = new unsigned char[texWidth * texHeight * 3];
    generateCheckerboardTexture(texWidth, texHeight, textureData);

    // Create texture using NEW Coin 4.1 setImageData() convenience API  
    SoTexture2 *checkerTexture = new SoTexture2;
    checkerTexture->ref();
    
    std::cout << "Creating texture with setImageData()..." << std::endl;
    
    // This is the new API - much simpler than the old image.setValue() approach!
    checkerTexture->setImageData(texWidth, texHeight, 3, textureData);
    
    // Verify the texture was set correctly using new getImageData() API
    int w, h, c;
    const unsigned char* retrievedData = checkerTexture->getImageData(w, h, c);
    if (retrievedData && w == texWidth && h == texHeight && c == 3) {
        std::cout << "✓ Successfully created " << w << "x" << h << " procedural texture using setImageData()" << std::endl;
        std::cout << "✓ Texture data verified using getImageData()" << std::endl;
        std::cout << "  Sample colors: R=" << (int)retrievedData[0] 
                  << " G=" << (int)retrievedData[1] 
                  << " B=" << (int)retrievedData[2] << std::endl;
    } else {
        std::cout << "✗ API test failed" << std::endl;
        return 1;
    }
    
    std::cout << "\n=== TESTING ACTUAL TEXTURE RENDERING ===" << std::endl;
    std::cout << "Adding texture to scene graph..." << std::endl;
    
    // THIS IS THE CRITICAL DIFFERENCE - actually use the texture!
    root->addChild(checkerTexture);
    root->addChild(new SoTextureCoordinateDefault);
    root->addChild(new SoCube);

    // Set up offscreen renderer with reasonable size
    const int width = 512;
    const int height = 512;
    SbViewportRegion viewport(width, height);
    SoOffscreenRenderer renderer(viewport);
    renderer.setBackgroundColor(SbColor(0.2f, 0.3f, 0.4f)); // Blue-gray background

    // Make camera see everything
    myCamera->viewAll(root, viewport);

    std::cout << "Attempting to render scene with texture..." << std::endl;
    std::cout << "(This is where crashes typically occur with OSMesa)" << std::endl;

    // Render the scene
    SbBool success = renderer.render(root);

    if (success) {
        std::cout << "✓ SUCCESS! Texture rendering worked with OSMesa!" << std::endl;
        
        // Determine output filename
        std::string filename = "BasicTextureFixed.rgb";
        if (argc > 1) {
            filename = argv[1];
        }
        
        // Save to RGB file using built-in SGI RGB format
        if (saveRGB(filename, &renderer)) {
            std::cout << "✓ Successfully rendered textured cube to " << filename << std::endl;
            std::cout << "✓ The OSMesa texture issue appears to be resolved!" << std::endl;
        } else {
            std::cerr << "✗ Render succeeded but failed to save RGB file" << std::endl;
            return 1;
        }
    } else {
        std::cout << "✗ FAILURE: Render failed - this confirms the OSMesa texture problem" << std::endl;
        std::cerr << "This indicates the root cause of the texture crash with OSMesa" << std::endl;
        return 1;
    }

    // Clean up texture data (APIs successfully created and managed the texture)
    delete[] textureData;
    checkerTexture->unref();
    
    // Clean up
    root->unref();

    return 0;
    
#else
    std::cerr << "Error: OSMesa support not available. Cannot run headless rendering." << std::endl;
    return 1;
#endif
}