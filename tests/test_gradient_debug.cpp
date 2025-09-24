/**
 * @file test_gradient_debug.cpp
 * @brief Debug test to analyze color artifacts by creating a simple gradient scene
 *
 * Creates a flat, view-plane aligned surface with a left-to-right color gradient
 * to help track RGB color values and identify rendering/pixel conversion issues.
 */

#include "test_utils.h"
#include <memory>
#include <fstream>
#include <iomanip>

// OSMesa headers for headless OpenGL
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
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <Inventor/nodes/SoVertexProperty.h>
#include <Inventor/nodes/SoShapeHints.h>
#include <Inventor/nodes/SoMaterialBinding.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/SbViewportRegion.h>

// Include svpng for PNG output
#include "../../src/glue/svpng.h"

namespace GradientDebug {

#ifdef HAVE_OSMESA

// OSMesa Context wrapper
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
            GLenum error;
            while ((error = glGetError()) != GL_NO_ERROR) {
                // Clear errors
            }
            
            const char* extensions = (const char*)glGetString(GL_EXTENSIONS);
            if (extensions) {
                (void)extensions;
            }
        }
        return result;
    }
    
    bool isValid() const { return context != nullptr; }
    const unsigned char* getBuffer() const { return buffer.get(); }
};

// OSMesa Context Manager implementation
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

// Create a simple horizontal gradient using separate geometry with emissive materials
SoSeparator* createGradientScene() {
    SoSeparator* root = new SoSeparator;
    root->ref();
    
    // Use orthographic camera for flat, pixel-aligned rendering
    SoOrthographicCamera* camera = new SoOrthographicCamera;
    camera->position = SbVec3f(0, 0, 1);
    camera->orientation = SbRotation::identity();
    camera->nearDistance = 0.1f;
    camera->farDistance = 10.0f;
    camera->height = 2.0f; // -1 to 1 range
    root->addChild(camera);
    
    // Create a simple horizontal gradient using multiple colored rectangles
    const int numStrips = 10; // Number of vertical strips for gradient
    const float stripWidth = 2.0f / numStrips; // Width of each strip
    
    for (int i = 0; i < numStrips; i++) {
        SoSeparator* stripGroup = new SoSeparator;
        
        // Calculate color: interpolate from red (left) to green (right)
        float t = (float)i / (numStrips - 1); // 0.0 to 1.0
        float red = 1.0f - t;   // 1.0 to 0.0
        float green = t;        // 0.0 to 1.0
        
        // Create emissive material for this strip
        SoMaterial* material = new SoMaterial;
        material->emissiveColor = SbColor(red, green, 0.0f); // Red to Green gradient
        material->diffuseColor = SbColor(0.0f, 0.0f, 0.0f);  // No diffuse
        material->ambientColor = SbColor(0.0f, 0.0f, 0.0f);  // No ambient
        stripGroup->addChild(material);
        
        // Create coordinates for this strip
        SoCoordinate3* coords = new SoCoordinate3;
        float leftX = -1.0f + i * stripWidth;
        float rightX = leftX + stripWidth;
        
        coords->point.set1Value(0, SbVec3f(leftX, -1.0f, 0.0f));   // Bottom left
        coords->point.set1Value(1, SbVec3f(rightX, -1.0f, 0.0f));  // Bottom right
        coords->point.set1Value(2, SbVec3f(rightX, 1.0f, 0.0f));   // Top right
        coords->point.set1Value(3, SbVec3f(leftX, 1.0f, 0.0f));    // Top left
        stripGroup->addChild(coords);
        
        // Create face set for this strip
        SoIndexedFaceSet* faceSet = new SoIndexedFaceSet;
        faceSet->coordIndex.set1Value(0, 0);
        faceSet->coordIndex.set1Value(1, 1);
        faceSet->coordIndex.set1Value(2, 2);
        faceSet->coordIndex.set1Value(3, 3);
        faceSet->coordIndex.set1Value(4, -1); // End face
        stripGroup->addChild(faceSet);
        
        root->addChild(stripGroup);
    }
    
    return root;
}

// Helper to save PNG
bool savePNG(const std::string& filename, const unsigned char* buffer, int width, int height, int components) {
    FILE* file = fopen(filename.c_str(), "wb");
    if (!file) return false;
    
    if (components == 3) {
        // Buffer is already RGB, save directly
        svpng(file, width, height, buffer, 0);
    } else if (components == 4) {
        // Buffer is RGBA, extract RGB for PNG output
        std::unique_ptr<unsigned char[]> rgb_data(new unsigned char[width * height * 3]);
        for (int i = 0; i < width * height; i++) {
            rgb_data[i * 3 + 0] = buffer[i * 4 + 0]; // R
            rgb_data[i * 3 + 1] = buffer[i * 4 + 1]; // G
            rgb_data[i * 3 + 2] = buffer[i * 4 + 2]; // B
        }
        svpng(file, width, height, rgb_data.get(), 0);
    } else {
        // For 1 or 2 component buffers, expand to RGB
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

// Helper to analyze specific pixel values
void analyzePixels(const unsigned char* buffer, int width, int height, SoOffscreenRenderer::Components components) {
    std::cout << "\n=== Pixel Analysis ===" << std::endl;
    std::cout << "Image size: " << width << "x" << height << std::endl;
    std::cout << "Components: " << components << std::endl;
    
    int bytesPerPixel = (int)components;
    
    // Check key pixels across the gradient
    std::vector<std::pair<int, int>> testPixels = {
        {0, height/2},           // Far left (should be red)
        {width/4, height/2},     // Quarter way (should be red-green mix)
        {width/2, height/2},     // Middle (should be 50-50 mix)
        {3*width/4, height/2},   // Three quarters (should be green-red mix)
        {width-1, height/2}      // Far right (should be green)
    };
    
    for (auto& pixel : testPixels) {
        int x = pixel.first;
        int y = pixel.second;
        int idx = (y * width + x) * bytesPerPixel;
        
        std::cout << "Pixel (" << x << "," << y << "): ";
        if (components == SoOffscreenRenderer::RGB || components == SoOffscreenRenderer::RGB_TRANSPARENCY) {
            std::cout << "R=" << (int)buffer[idx] << " G=" << (int)buffer[idx+1] << " B=" << (int)buffer[idx+2];
            if (components == SoOffscreenRenderer::RGB_TRANSPARENCY) {
                std::cout << " A=" << (int)buffer[idx+3];
            }
        } else {
            std::cout << "L=" << (int)buffer[idx];
            if (components == SoOffscreenRenderer::LUMINANCE_TRANSPARENCY) {
                std::cout << " A=" << (int)buffer[idx+1];
            }
        }
        std::cout << std::endl;
    }
}

#endif // HAVE_OSMESA

} // namespace GradientDebug

int main() {
    using namespace GradientDebug;
    
    SimpleTest::TestRunner runner;
    
#ifdef HAVE_OSMESA
    // Initialize Coin3D with OSMesa context manager
    std::unique_ptr<OSMesaContextManager> context_manager = std::make_unique<OSMesaContextManager>();
    SoDB::init(context_manager.get());
    SoInteraction::init();
    
    // Test simple gradient rendering
    runner.startTest("Gradient scene rendering (RGB)");
    try {
        SoSeparator* scene = createGradientScene();
        if (!scene) {
            runner.endTest(false, "Failed to create gradient scene");
            return runner.getSummary();
        }
        
        // Test RGB rendering
        const std::string filename = "gradient_debug_rgb.png";
        SbViewportRegion viewport(256, 256);
        SoOffscreenRenderer renderer(viewport);
        renderer.setComponents(SoOffscreenRenderer::RGB);
        renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f)); // Black background
        
        if (!renderer.render(scene)) {
            scene->unref();
            runner.endTest(false, "Failed to render gradient scene");
            return runner.getSummary();
        }
        
        const unsigned char* buffer = renderer.getBuffer();
        if (!buffer) {
            scene->unref();
            runner.endTest(false, "Failed to get rendered buffer");
            return runner.getSummary();
        }
        
        // Analyze the pixel values
        analyzePixels(buffer, 256, 256, renderer.getComponents());
        
        // Save as PNG for visual inspection
        if (!savePNG(filename, buffer, 256, 256, (int)renderer.getComponents())) {
            scene->unref();
            runner.endTest(false, "Failed to save PNG");
            return runner.getSummary();
        }
        
        std::cout << "Gradient test saved as: " << filename << std::endl;
        
        scene->unref();
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return runner.getSummary();
    }
    
    // Test RGBA rendering
    runner.startTest("Gradient scene rendering (RGBA)");
    try {
        SoSeparator* scene = createGradientScene();
        if (!scene) {
            runner.endTest(false, "Failed to create gradient scene");
            return runner.getSummary();
        }
        
        const std::string filename = "gradient_debug_rgba.png";
        SbViewportRegion viewport(256, 256);
        SoOffscreenRenderer renderer(viewport);
        renderer.setComponents(SoOffscreenRenderer::RGB_TRANSPARENCY);
        renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f)); // Black background
        
        if (!renderer.render(scene)) {
            scene->unref();
            runner.endTest(false, "Failed to render gradient scene");
            return runner.getSummary();
        }
        
        const unsigned char* buffer = renderer.getBuffer();
        if (!buffer) {
            scene->unref();
            runner.endTest(false, "Failed to get rendered buffer");
            return runner.getSummary();
        }
        
        // Analyze the pixel values
        analyzePixels(buffer, 256, 256, renderer.getComponents());
        
        // Save as PNG for visual inspection
        if (!savePNG(filename, buffer, 256, 256, (int)renderer.getComponents())) {
            scene->unref();
            runner.endTest(false, "Failed to save PNG");
            return runner.getSummary();
        }
        
        std::cout << "RGBA gradient test saved as: " << filename << std::endl;
        
        scene->unref();
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return runner.getSummary();
    }
    
#else
    runner.startTest("OSMesa availability check");
    runner.endTest(false, "OSMesa headers not found - gradient tests skipped");
    return 1;
#endif
    
    return runner.getSummary();
}