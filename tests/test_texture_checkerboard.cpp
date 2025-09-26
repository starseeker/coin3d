/**
 * @file test_texture_checkerboard.cpp
 * @brief Test that validates proper black and white checkerboard texture rendering
 * 
 * This test was created to verify that the OSMesa texture fixes work correctly.
 * It creates a pure black and white checkerboard texture and validates that
 * the rendered output contains the expected pixel values.
 */

#include "test_utils.h"
#include <memory>

#ifdef __has_include
  #if __has_include(<OSMesa/osmesa.h>)
    #include <OSMesa/osmesa.h>
    #include <OSMesa/gl.h>
    #define HAVE_OSMESA
  #endif
#endif

#include <Inventor/SoDB.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoTextureCoordinateDefault.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/SbViewportRegion.h>

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
        (void)context;
    }
    
    virtual void destroyContext(void* context) override {
        delete static_cast<OSMesaContextData*>(context);
    }
};

// Generate a pure black and white checkerboard texture pattern
void generateCheckerboardTexture(int width, int height, unsigned char* data) {
    const int checkerSize = 32;  // Large squares for clear validation
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            bool checkX = (x / checkerSize) % 2;
            bool checkY = (y / checkerSize) % 2;
            bool isWhite = checkX ^ checkY;
            
            int idx = (y * width + x) * 3;  // RGB format
            if (isWhite) {
                data[idx] = 255;     // R - pure white
                data[idx + 1] = 255; // G 
                data[idx + 2] = 255; // B
            } else {
                data[idx] = 0;       // R - pure black
                data[idx + 1] = 0;   // G
                data[idx + 2] = 0;   // B
            }
        }
    }
}

// Validate that rendered pixels contain proper black and white values
bool validateCheckerboardPixels(SoOffscreenRenderer* renderer, int width, int height) {
    const unsigned char* buffer = renderer->getBuffer();
    if (!buffer) {
        return false;
    }
    
    int blackPixels = 0;
    int whitePixels = 0;
    int otherPixels = 0;
    
    // Sample pixels from the center area where the textured cube should be
    int startX = width / 4;
    int endX = 3 * width / 4;
    int startY = height / 4;
    int endY = 3 * height / 4;
    
    for (int y = startY; y < endY; y += 8) {  // Sample every 8th pixel
        for (int x = startX; x < endX; x += 8) {
            int idx = (y * width + x) * 4;  // RGBA format
            unsigned char r = buffer[idx];
            unsigned char g = buffer[idx + 1];
            unsigned char b = buffer[idx + 2];
            
            // Check if it's black (all values close to 0)
            if (r < 32 && g < 32 && b < 32) {
                blackPixels++;
            }
            // Check if it's white (all values close to 255)
            else if (r > 220 && g > 220 && b > 220) {
                whitePixels++;
            }
            else {
                otherPixels++;
            }
        }
    }
    
    int totalSampled = blackPixels + whitePixels + otherPixels;
    
    // For a proper checkerboard, we should have roughly equal black and white pixels
    // Allow some tolerance for edge effects and sampling
    bool hasBlackPixels = blackPixels > totalSampled / 10;
    bool hasWhitePixels = whitePixels > totalSampled / 10;
    bool fewOtherPixels = otherPixels < totalSampled / 2;
    
    return hasBlackPixels && hasWhitePixels && fewOtherPixels;
}

namespace TestUtils {

bool testCheckerboardTexture() {
    // Initialize Coin3D with OSMesa context management
    std::unique_ptr<OSMesaContextManager> context_manager = std::make_unique<OSMesaContextManager>();
    SoDB::init(context_manager.get());
    
    SoSeparator *root = new SoSeparator;
    SoPerspectiveCamera *camera = new SoPerspectiveCamera;
    root->ref();
    root->addChild(camera);
    
    // Add lighting - important for texture rendering
    root->addChild(new SoDirectionalLight);

    // Generate pure black and white checkerboard texture
    const int texWidth = 128;
    const int texHeight = 128;
    unsigned char* textureData = new unsigned char[texWidth * texHeight * 3];
    generateCheckerboardTexture(texWidth, texHeight, textureData);

    // Create texture using setImageData API  
    SoTexture2 *checkerTexture = new SoTexture2;
    checkerTexture->ref();
    checkerTexture->setImageData(texWidth, texHeight, 3, textureData);
    
    // Add material first - white base material to let texture show through
    SoMaterial* material = new SoMaterial;
    material->diffuseColor = SbColor(1.0f, 1.0f, 1.0f);  // White
    material->ambientColor = SbColor(0.2f, 0.2f, 0.2f);  // Light gray ambient
    root->addChild(material);
    
    // Add texture to scene graph
    root->addChild(checkerTexture);
    root->addChild(new SoTextureCoordinateDefault);
    
    // Clean up texture data after setting it
    delete[] textureData;

    // Make a cube
    root->addChild(new SoCube);

    // Set up offscreen renderer
    const int width = 512;
    const int height = 512;
    SbViewportRegion viewport(width, height);
    SoOffscreenRenderer renderer(viewport);
    renderer.setBackgroundColor(SbColor(0.2f, 0.3f, 0.4f));

    // Make camera see everything
    camera->viewAll(root, viewport);

    // Render the scene
    SbBool success = renderer.render(root);
    
    bool pixelsValid = false;
    if (success) {
        // Validate the rendered pixels
        pixelsValid = validateCheckerboardPixels(&renderer, width, height);
    }

    // Clean up
    checkerTexture->unref();
    root->unref();
    
    return success && pixelsValid;
}

} // namespace TestUtils

int main() {
#ifdef HAVE_OSMESA
    std::cout << "Running: Checkerboard texture rendering test..." << std::endl;
    
    bool result = TestUtils::testCheckerboardTexture();
    
    if (result) {
        std::cout << " PASSED" << std::endl;
        std::cout << "\nTest Summary: 1 passed, 0 failed (total: 1)" << std::endl;
        return 0;
    } else {
        std::cout << " FAILED" << std::endl;
        std::cout << "\nTest Summary: 0 passed, 1 failed (total: 1)" << std::endl;
        return 1;
    }
#else
    std::cout << "SKIPPED - OSMesa not available" << std::endl;
    return 0;
#endif
}