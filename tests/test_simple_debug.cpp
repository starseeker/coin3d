/**
 * @file test_simple_debug.cpp
 * @brief Simple debug test to isolate color issues with basic colored geometry
 */

#include "test_utils.h"
#include <memory>
#include <fstream>
#include <iomanip>

// OSMesa headers
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
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/SbViewportRegion.h>

#include "../../src/glue/svpng.h"

namespace SimpleDebug {

#ifdef HAVE_OSMESA

struct OSMesaContextData {
    OSMesaContext context;
    std::unique_ptr<unsigned char[]> buffer;
    int width, height;
    
    OSMesaContextData(int w, int h) : width(w), height(h) {
        context = OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, NULL);
        if (context) {
            size_t buffer_size = std::max(static_cast<size_t>(4096 * 4096 * 4), 
                                         static_cast<size_t>(width * height * 4));
            buffer = std::make_unique<unsigned char[]>(buffer_size);
        }
    }
    
    ~OSMesaContextData() {
        if (context) OSMesaDestroyContext(context);
    }
    
    bool makeCurrent() {
        if (!context || !buffer) return false;
        
        bool result = OSMesaMakeCurrent(context, buffer.get(), GL_UNSIGNED_BYTE, width, height);
        if (result) {
            // IMPORTANT: Set Y_UP immediately after making context current
            OSMesaPixelStore(OSMESA_Y_UP, 0);
            
            GLenum error;
            while ((error = glGetError()) != GL_NO_ERROR) {}
            const char* extensions = (const char*)glGetString(GL_EXTENSIONS);
            if (extensions) { (void)extensions; }
        }
        return result;
    }
    
    bool isValid() const { return context != nullptr; }
    const unsigned char* getBuffer() const { return buffer.get(); }
};

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

// Create simple red cube scene
SoSeparator* createRedCubeScene() {
    SoSeparator* root = new SoSeparator;
    root->ref();
    
    // Add camera
    SoPerspectiveCamera* camera = new SoPerspectiveCamera;
    camera->position = SbVec3f(0, 0, 5);
    camera->orientation = SbRotation::identity();
    root->addChild(camera);
    
    // Add red material with high emission to bypass lighting
    SoMaterial* material = new SoMaterial;
    material->diffuseColor = SbColor(1.0f, 0.0f, 0.0f); // Bright red
    material->ambientColor = SbColor(0.2f, 0.0f, 0.0f); // Dark red ambient  
    material->emissiveColor = SbColor(0.8f, 0.0f, 0.0f); // High red emission to bypass lighting
    material->specularColor = SbColor(0.0f, 0.0f, 0.0f); // No specular
    material->shininess = 0.0f;
    root->addChild(material);
    
    // Add cube
    SoCube* cube = new SoCube;
    cube->width = 1.0f;
    cube->height = 1.0f;  
    cube->depth = 1.0f;
    root->addChild(cube);
    
    return root;
}

bool savePNG(const std::string& filename, const unsigned char* buffer, int width, int height, int components) {
    FILE* file = fopen(filename.c_str(), "wb");
    if (!file) return false;
    
    if (components == 3) {
        // RGB - save directly
        svpng(file, width, height, buffer, 0);
    } else if (components == 4) {
        // RGBA - convert to RGB
        std::unique_ptr<unsigned char[]> rgb_data(new unsigned char[width * height * 3]);
        for (int i = 0; i < width * height; i++) {
            rgb_data[i * 3 + 0] = buffer[i * 4 + 0]; // R
            rgb_data[i * 3 + 1] = buffer[i * 4 + 1]; // G
            rgb_data[i * 3 + 2] = buffer[i * 4 + 2]; // B
        }
        svpng(file, width, height, rgb_data.get(), 0);
    } else {
        // Grayscale - expand to RGB
        std::unique_ptr<unsigned char[]> rgb_data(new unsigned char[width * height * 3]);
        for (int i = 0; i < width * height; i++) {
            unsigned char gray = buffer[i * components];
            rgb_data[i * 3 + 0] = gray; // R
            rgb_data[i * 3 + 1] = gray; // G
            rgb_data[i * 3 + 2] = gray; // B
        }
        svpng(file, width, height, rgb_data.get(), 0);
    }
    
    fclose(file);
    return true;
}

void analyzePixels(const unsigned char* buffer, int width, int height, int components) {
    std::cout << "\n=== Pixel Analysis ===" << std::endl;
    std::cout << "Image size: " << width << "x" << height << std::endl;
    std::cout << "Components: " << components << std::endl;
    
    // Check center and corners
    std::vector<std::pair<int, int>> testPixels = {
        {0, 0},              // Top-left corner  
        {width-1, 0},        // Top-right corner
        {0, height-1},       // Bottom-left corner
        {width-1, height-1}, // Bottom-right corner
        {width/2, height/2}  // Center
    };
    
    for (auto& pixel : testPixels) {
        int x = pixel.first;
        int y = pixel.second;
        int idx = (y * width + x) * components;
        
        std::cout << "Pixel (" << x << "," << y << "): ";
        
        if (components >= 3) {
            std::cout << "R=" << (int)buffer[idx] << " G=" << (int)buffer[idx+1] << " B=" << (int)buffer[idx+2];
            if (components == 4) {
                std::cout << " A=" << (int)buffer[idx+3];
            }
        } else {
            std::cout << "L=" << (int)buffer[idx];
            if (components == 2) {
                std::cout << " A=" << (int)buffer[idx+1];
            }
        }
        std::cout << std::endl;
    }
}

#endif // HAVE_OSMESA

} // namespace SimpleDebug

int main() {
    using namespace SimpleDebug;
    
    SimpleTest::TestRunner runner;
    
#ifdef HAVE_OSMESA
    // Initialize Coin3D
    std::unique_ptr<OSMesaContextManager> context_manager = std::make_unique<OSMesaContextManager>();
    SoDB::init(context_manager.get());
    SoInteraction::init();
    
    // Test simple red cube rendering with different component counts
    std::vector<SoOffscreenRenderer::Components> componentTypes = {
        SoOffscreenRenderer::RGB,
        SoOffscreenRenderer::RGB_TRANSPARENCY,
        SoOffscreenRenderer::LUMINANCE,
        SoOffscreenRenderer::LUMINANCE_TRANSPARENCY
    };
    
    std::vector<std::string> componentNames = {
        "RGB", "RGBA", "Luminance", "Luminance+Alpha"
    };
    
    for (size_t i = 0; i < componentTypes.size(); i++) {
        std::string testName = "Red cube rendering (" + componentNames[i] + ")";
        runner.startTest(testName);
        
        try {
            SoSeparator* scene = createRedCubeScene();
            if (!scene) {
                runner.endTest(false, "Failed to create red cube scene");
                continue;
            }
            
            std::string filename = "simple_debug_" + componentNames[i] + ".png";
            SbViewportRegion viewport(128, 128); // Small size for easier debugging
            SoOffscreenRenderer renderer(viewport);
            renderer.setComponents(componentTypes[i]);
            renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.1f)); // Dark blue background
            
            if (!renderer.render(scene)) {
                scene->unref();
                runner.endTest(false, "Failed to render scene");
                continue;
            }
            
            const unsigned char* buffer = renderer.getBuffer();
            if (!buffer) {
                scene->unref();
                runner.endTest(false, "Failed to get rendered buffer");
                continue;
            }
            
            analyzePixels(buffer, 128, 128, (int)componentTypes[i]);
            
            if (!savePNG(filename, buffer, 128, 128, (int)componentTypes[i])) {
                scene->unref();
                runner.endTest(false, "Failed to save PNG");
                continue;
            }
            
            std::cout << "Simple debug test saved as: " << filename << std::endl;
            
            scene->unref();
            runner.endTest(true);
        } catch (const std::exception& e) {
            runner.endTest(false, std::string("Exception: ") + e.what());
        }
    }
    
#else
    runner.startTest("OSMesa availability check");
    runner.endTest(false, "OSMesa headers not found");
    return 1;
#endif
    
    return runner.getSummary();
}