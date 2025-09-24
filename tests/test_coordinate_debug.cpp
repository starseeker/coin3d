/**
 * @file test_coordinate_debug.cpp  
 * @brief Test for coordinate system issues (Y-axis flipping, etc.)
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
#include <Inventor/SoInteraction.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <Inventor/nodes/SoVertexProperty.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoMaterialBinding.h>
#include <Inventor/SbViewportRegion.h>

#include "../../src/glue/svpng.h"

namespace CoordinateDebug {

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
            // This will cause compute_row_addresses() to be called and reorganize the buffer
            OSMesaPixelStore(OSMESA_Y_UP, 0);
            
            // Now clear GL errors and setup extensions
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

// Create a scene with different colored corners to test coordinate system
SoSeparator* createCornerTestScene() {
    SoSeparator* root = new SoSeparator;
    root->ref();
    
    // Use orthographic camera for pixel-perfect rendering
    SoOrthographicCamera* camera = new SoOrthographicCamera;
    camera->position = SbVec3f(0, 0, 1);
    camera->orientation = SbRotation::identity();
    camera->nearDistance = 0.1f;
    camera->farDistance = 10.0f;
    camera->height = 2.0f;
    root->addChild(camera);
    
    // Create vertex property for a quad covering the full screen
    SoVertexProperty* vertexProp = new SoVertexProperty;
    
    // Define corners (-1 to 1 range)
    vertexProp->vertex.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f)); // Bottom left
    vertexProp->vertex.set1Value(1, SbVec3f( 1.0f, -1.0f, 0.0f)); // Bottom right  
    vertexProp->vertex.set1Value(2, SbVec3f( 1.0f,  1.0f, 0.0f)); // Top right
    vertexProp->vertex.set1Value(3, SbVec3f(-1.0f,  1.0f, 0.0f)); // Top left
    
    // Set emissive colors for each corner:
    // Red = Bottom Left, Green = Bottom Right, Blue = Top Right, Yellow = Top Left
    SoMaterial* redMat = new SoMaterial;
    redMat->emissiveColor = SbColor(1.0f, 0.0f, 0.0f); // Red
    
    SoMaterial* greenMat = new SoMaterial;
    greenMat->emissiveColor = SbColor(0.0f, 1.0f, 0.0f); // Green
    
    SoMaterial* blueMat = new SoMaterial;  
    blueMat->emissiveColor = SbColor(0.0f, 0.0f, 1.0f); // Blue
    
    SoMaterial* yellowMat = new SoMaterial;
    yellowMat->emissiveColor = SbColor(1.0f, 1.0f, 0.0f); // Yellow
    
    // Create four triangles to make the colored corners
    
    // Triangle 1: Bottom-left red corner  
    SoSeparator* tri1 = new SoSeparator;
    tri1->addChild(redMat);
    SoVertexProperty* vp1 = new SoVertexProperty;
    vp1->vertex.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f)); // Bottom left
    vp1->vertex.set1Value(1, SbVec3f( 0.0f, -1.0f, 0.0f)); // Bottom center
    vp1->vertex.set1Value(2, SbVec3f(-1.0f,  0.0f, 0.0f)); // Left center
    SoIndexedFaceSet* face1 = new SoIndexedFaceSet;
    face1->vertexProperty = vp1;
    face1->coordIndex.set1Value(0, 0);
    face1->coordIndex.set1Value(1, 1);
    face1->coordIndex.set1Value(2, 2);
    face1->coordIndex.set1Value(3, -1);
    tri1->addChild(face1);
    root->addChild(tri1);
    
    // Triangle 2: Bottom-right green corner
    SoSeparator* tri2 = new SoSeparator;
    tri2->addChild(greenMat);
    SoVertexProperty* vp2 = new SoVertexProperty;
    vp2->vertex.set1Value(0, SbVec3f( 1.0f, -1.0f, 0.0f)); // Bottom right
    vp2->vertex.set1Value(1, SbVec3f( 0.0f, -1.0f, 0.0f)); // Bottom center
    vp2->vertex.set1Value(2, SbVec3f( 1.0f,  0.0f, 0.0f)); // Right center
    SoIndexedFaceSet* face2 = new SoIndexedFaceSet;
    face2->vertexProperty = vp2;
    face2->coordIndex.set1Value(0, 0);
    face2->coordIndex.set1Value(1, 1);
    face2->coordIndex.set1Value(2, 2);
    face2->coordIndex.set1Value(3, -1);
    tri2->addChild(face2);
    root->addChild(tri2);
    
    // Triangle 3: Top-right blue corner
    SoSeparator* tri3 = new SoSeparator;
    tri3->addChild(blueMat);
    SoVertexProperty* vp3 = new SoVertexProperty;
    vp3->vertex.set1Value(0, SbVec3f( 1.0f,  1.0f, 0.0f)); // Top right
    vp3->vertex.set1Value(1, SbVec3f( 0.0f,  1.0f, 0.0f)); // Top center
    vp3->vertex.set1Value(2, SbVec3f( 1.0f,  0.0f, 0.0f)); // Right center
    SoIndexedFaceSet* face3 = new SoIndexedFaceSet;
    face3->vertexProperty = vp3;
    face3->coordIndex.set1Value(0, 0);
    face3->coordIndex.set1Value(1, 1);
    face3->coordIndex.set1Value(2, 2);
    face3->coordIndex.set1Value(3, -1);
    tri3->addChild(face3);
    root->addChild(tri3);
    
    // Triangle 4: Top-left yellow corner
    SoSeparator* tri4 = new SoSeparator;
    tri4->addChild(yellowMat);
    SoVertexProperty* vp4 = new SoVertexProperty;
    vp4->vertex.set1Value(0, SbVec3f(-1.0f,  1.0f, 0.0f)); // Top left
    vp4->vertex.set1Value(1, SbVec3f( 0.0f,  1.0f, 0.0f)); // Top center
    vp4->vertex.set1Value(2, SbVec3f(-1.0f,  0.0f, 0.0f)); // Left center
    SoIndexedFaceSet* face4 = new SoIndexedFaceSet;
    face4->vertexProperty = vp4;
    face4->coordIndex.set1Value(0, 0);
    face4->coordIndex.set1Value(1, 1);
    face4->coordIndex.set1Value(2, 2);
    face4->coordIndex.set1Value(3, -1);
    tri4->addChild(face4);
    root->addChild(tri4);
    
    return root;
}

bool savePNG(const std::string& filename, const unsigned char* buffer, int width, int height, int components) {
    FILE* file = fopen(filename.c_str(), "wb");
    if (!file) return false;
    
    if (components == 3) {
        svpng(file, width, height, buffer, 0);
    } else {
        std::unique_ptr<unsigned char[]> rgb_data(new unsigned char[width * height * 3]);
        for (int i = 0; i < width * height; i++) {
            rgb_data[i * 3 + 0] = buffer[i * components + 0]; // R
            rgb_data[i * 3 + 1] = buffer[i * components + 1]; // G 
            rgb_data[i * 3 + 2] = buffer[i * components + 2]; // B
        }
        svpng(file, width, height, rgb_data.get(), 0);
    }
    
    fclose(file);
    return true;
}

void analyzeCorners(const unsigned char* buffer, int width, int height, int components) {
    std::cout << "\n=== Corner Analysis ===" << std::endl;
    std::cout << "Image size: " << width << "x" << height << std::endl;
    
    // Check corners - if Y is flipped, we'd expect different colors than intended
    std::vector<std::tuple<int, int, std::string, std::string>> corners = {
        {5, 5, "Top-left", "Yellow"},                    // Image top-left (should be yellow)
        {width-5, 5, "Top-right", "Blue"},               // Image top-right (should be blue)  
        {5, height-5, "Bottom-left", "Red"},             // Image bottom-left (should be red)
        {width-5, height-5, "Bottom-right", "Green"}     // Image bottom-right (should be green)
    };
    
    for (auto& corner : corners) {
        int x = std::get<0>(corner);
        int y = std::get<1>(corner);
        std::string pos = std::get<2>(corner);
        std::string expected = std::get<3>(corner);
        
        int idx = (y * width + x) * components;
        
        std::cout << pos << " (" << x << "," << y << ") [expected " << expected << "]: ";
        
        if (components >= 3) {
            int r = buffer[idx];
            int g = buffer[idx+1];  
            int b = buffer[idx+2];
            std::cout << "R=" << r << " G=" << g << " B=" << b;
            
            // Determine dominant color
            std::string actual = "Unknown";
            if (r > g && r > b && r > 50) actual = "Red";
            else if (g > r && g > b && g > 50) actual = "Green";
            else if (b > r && b > g && b > 50) actual = "Blue";  
            else if (r > 50 && g > 50 && b < 50) actual = "Yellow";
            else if (r < 50 && g < 50 && b < 50) actual = "Black/Dark";
            
            std::cout << " -> " << actual;
            
            if (actual != expected) {
                std::cout << " (MISMATCH!)";
            }
        }
        std::cout << std::endl;
    }
    
    // Additional diagnostic
    std::cout << "\nIf corners don't match expected colors, this indicates either:" << std::endl;
    std::cout << "1. Y-axis is flipped (OpenGL bottom-left vs image top-left)" << std::endl;
    std::cout << "2. Coordinate system or viewport issues" << std::endl;
    std::cout << "3. Material/lighting problems" << std::endl;
}

#endif // HAVE_OSMESA

} // namespace CoordinateDebug

int main() {
    using namespace CoordinateDebug;
    
    SimpleTest::TestRunner runner;
    
#ifdef HAVE_OSMESA
    std::unique_ptr<OSMesaContextManager> context_manager = std::make_unique<OSMesaContextManager>();
    SoDB::init(context_manager.get());
    SoInteraction::init();
    
    runner.startTest("Corner coordinate system test");
    try {
        SoSeparator* scene = createCornerTestScene();
        if (!scene) {
            runner.endTest(false, "Failed to create corner test scene");
            return runner.getSummary();
        }
        
        std::string filename = "coordinate_debug_corners.png";
        SbViewportRegion viewport(256, 256);
        SoOffscreenRenderer renderer(viewport);
        renderer.setComponents(SoOffscreenRenderer::RGB);
        renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f)); // Black background
        
        if (!renderer.render(scene)) {
            scene->unref();
            runner.endTest(false, "Failed to render corner test scene");
            return runner.getSummary();
        }
        
        const unsigned char* buffer = renderer.getBuffer();
        if (!buffer) {
            scene->unref();
            runner.endTest(false, "Failed to get rendered buffer");
            return runner.getSummary();
        }
        
        analyzeCorners(buffer, 256, 256, 3);
        
        if (!savePNG(filename, buffer, 256, 256, 3)) {
            scene->unref();
            runner.endTest(false, "Failed to save PNG");
            return runner.getSummary();
        }
        
        std::cout << "\nCorner test saved as: " << filename << std::endl;
        
        scene->unref();
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
    }
    
#else
    runner.startTest("OSMesa availability check");
    runner.endTest(false, "OSMesa headers not found");
    return 1;
#endif
    
    return runner.getSummary();
}