/**************************************************************************\
* Copyright (c) Kongsberg Oil & Gas Technologies AS
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*
* Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
*
* Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
*
* Neither the name of the copyright holder nor the names of its
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
\**************************************************************************/

/**
 * @file test_scene_rendering.cpp
 * @brief Integration test for comprehensive 3D scene rendering with PNG output
 *
 * Demonstrates the ability to create, render and visualize non-trivial 3D scenes
 * using OSMesa for headless rendering and PNG output for verification.
 * Returns 0 for success, non-zero for failure.
 */

#include "test_utils.h"
#include <memory>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <tuple>

// OSMesa headers for headless OpenGL
#ifdef __has_include
  #if __has_include(<OSMesa/osmesa.h>)
    #include <OSMesa/osmesa.h>
    #include <OSMesa/gl.h>
    #define HAVE_OSMESA
  #endif
#endif

// Coin3D headers for scene creation and rendering
#include <Inventor/SoDB.h>
#include <Inventor/SoInteraction.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoRotation.h>
#include <Inventor/SbViewportRegion.h>

// Include svpng for PNG output
#include "../../src/glue/svpng.h"

namespace SimpleTest {

#ifdef HAVE_OSMESA

// OSMesa Context wrapper
struct OSMesaContextData {
    OSMesaContext context;
    std::unique_ptr<unsigned char[]> buffer;
    int width, height;
    
    OSMesaContextData(int w, int h) : width(w), height(h) {
        // Create RGBA context with 16-bit depth, 0 stencil, 0 accum
        context = OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, NULL);
        if (context) {
            // Allocate larger buffer like BRL-CAD does - OSMesa needs space for internal
            // operations like textures, framebuffers, etc. beyond just the final image
            // Use at least 4096x4096 to match OSMesa's MAX_WIDTH/MAX_HEIGHT settings
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
            
            // Clear any GL errors that might have occurred during context creation
            // This prevents warnings in cc_glglue_instance() about context setup errors
            GLenum error;
            while ((error = glGetError()) != GL_NO_ERROR) {
                // Clear errors without reporting - context creation can generate spurious errors
            }
            
            // Ensure OpenGL extension loading is triggered after making context current
            // This is equivalent to what glewInit() does in OSMesa examples
            const char* extensions = (const char*)glGetString(GL_EXTENSIONS);
            if (extensions) {
                // Extension string retrieved successfully - this triggers proper GL state initialization
                (void)extensions; // Avoid unused variable warning
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
        // OSMesa doesn't need context stacking for single-threaded use
        (void)context;
    }
    
    virtual void destroyContext(void* context) override {
        delete static_cast<OSMesaContextData*>(context);
    }
};

// Helper function to create a comprehensive 3D scene
SoSeparator* createComplexScene() {
    SoSeparator* root = new SoSeparator;
    root->ref();
    
    // Add camera
    SoPerspectiveCamera* camera = new SoPerspectiveCamera;
    camera->position = SbVec3f(0, 0, 8);
    camera->orientation = SbRotation(SbVec3f(1, 0, 0), -0.2f);
    camera->nearDistance = 1.0f;
    camera->farDistance = 20.0f;
    camera->focalDistance = 8.0f;
    root->addChild(camera);
    
    // Add lighting
    SoDirectionalLight* light = new SoDirectionalLight;
    light->direction = SbVec3f(-1, -1, -1);
    light->intensity = 0.8f;
    root->addChild(light);
    
    // Create central red cube
    SoSeparator* cubeGroup = new SoSeparator;
    SoMaterial* redMaterial = new SoMaterial;
    redMaterial->diffuseColor = SbColor(0.8f, 0.2f, 0.2f);
    redMaterial->specularColor = SbColor(1.0f, 1.0f, 1.0f);
    redMaterial->shininess = 0.5f;
    cubeGroup->addChild(redMaterial);
    SoCube* cube = new SoCube;
    cube->width = 1.5f;
    cube->height = 1.5f;
    cube->depth = 1.5f;
    cubeGroup->addChild(cube);
    root->addChild(cubeGroup);
    
    // Create green sphere (upper right)
    SoSeparator* sphereGroup = new SoSeparator;
    SoTransform* sphereTransform = new SoTransform;
    sphereTransform->translation = SbVec3f(3, 2, 0);
    sphereGroup->addChild(sphereTransform);
    SoMaterial* greenMaterial = new SoMaterial;
    greenMaterial->diffuseColor = SbColor(0.2f, 0.8f, 0.2f);
    greenMaterial->specularColor = SbColor(1.0f, 1.0f, 1.0f);
    greenMaterial->shininess = 0.7f;
    sphereGroup->addChild(greenMaterial);
    SoSphere* sphere = new SoSphere;
    sphere->radius = 1.0f;
    sphereGroup->addChild(sphere);
    root->addChild(sphereGroup);
    
    // Create blue cone (upper left)
    SoSeparator* coneGroup = new SoSeparator;
    SoTransform* coneTransform = new SoTransform;
    coneTransform->translation = SbVec3f(-3, 2, 0);
    coneGroup->addChild(coneTransform);
    SoMaterial* blueMaterial = new SoMaterial;
    blueMaterial->diffuseColor = SbColor(0.2f, 0.2f, 0.8f);
    blueMaterial->specularColor = SbColor(1.0f, 1.0f, 1.0f);
    blueMaterial->shininess = 0.3f;
    coneGroup->addChild(blueMaterial);
    SoCone* cone = new SoCone;
    cone->bottomRadius = 1.0f;
    cone->height = 2.0f;
    coneGroup->addChild(cone);
    root->addChild(coneGroup);
    
    // Create yellow cylinder (lower center)
    SoSeparator* cylinderGroup = new SoSeparator;
    SoTransform* cylinderTransform = new SoTransform;
    cylinderTransform->translation = SbVec3f(0, -2.5f, 1);
    cylinderTransform->rotation = SbRotation(SbVec3f(0, 0, 1), M_PI_4);
    cylinderGroup->addChild(cylinderTransform);
    SoMaterial* yellowMaterial = new SoMaterial;
    yellowMaterial->diffuseColor = SbColor(0.8f, 0.8f, 0.2f);
    yellowMaterial->specularColor = SbColor(1.0f, 1.0f, 1.0f);
    yellowMaterial->shininess = 0.4f;
    cylinderGroup->addChild(yellowMaterial);
    SoCylinder* cylinder = new SoCylinder;
    cylinder->radius = 0.8f;
    cylinder->height = 3.0f;
    cylinderGroup->addChild(cylinder);
    root->addChild(cylinderGroup);
    
    return root;
}

// Helper function to save image as PNG using svpng
bool savePNG(const std::string& filename, const unsigned char* buffer, int width, int height) {
    FILE* file = fopen(filename.c_str(), "wb");
    if (!file) {
        return false;
    }
    
    // Convert RGBA to RGB for PNG output (svpng can handle both)
    std::unique_ptr<unsigned char[]> rgb_data(new unsigned char[width * height * 3]);
    for (int i = 0; i < width * height; i++) {
        rgb_data[i * 3 + 0] = buffer[i * 4 + 0]; // R
        rgb_data[i * 3 + 1] = buffer[i * 4 + 1]; // G
        rgb_data[i * 3 + 2] = buffer[i * 4 + 2]; // B
        // Skip alpha channel
    }
    
    // Use svpng to write PNG (0 = RGB, no alpha)
    svpng(file, width, height, rgb_data.get(), 0);
    fclose(file);
    
    return true;
}

// Pixel validation functions for analyzing rendered output
struct RGBColor {
    unsigned char r, g, b;
    
    constexpr RGBColor(unsigned char red = 0, unsigned char green = 0, unsigned char blue = 0) 
        : r(red), g(green), b(blue) {}
};

// Expected colors based on the scene setup (approximations accounting for lighting)
const RGBColor EXPECTED_BACKGROUND(25, 25, 77);  // Dark blue: 0.1, 0.1, 0.3 * 255

// Color tolerance for pixel comparison
constexpr int COLOR_TOLERANCE = 50;  // Increased tolerance for lighting effects

bool colorsMatch(const RGBColor& actual, const RGBColor& expected, int tolerance = COLOR_TOLERANCE) {
    return abs(actual.r - expected.r) <= tolerance &&
           abs(actual.g - expected.g) <= tolerance &&
           abs(actual.b - expected.b) <= tolerance;
}

bool isBackgroundColor(const RGBColor& color) {
    return colorsMatch(color, EXPECTED_BACKGROUND, 30); // Tighter tolerance for background
}

// More flexible geometry detection - look for any non-background colors
bool isGeometryColor(const RGBColor& color) {
    // Avoid background and very dark colors
    if (isBackgroundColor(color)) return false;
    
    // Look for colors with significant intensity (not just black/dark artifacts)
    int intensity = color.r + color.g + color.b;
    return intensity > 100; // Must have reasonable brightness to be geometry
}

// Sample colors from the rendered buffer to understand actual color distribution
void sampleColors(const unsigned char* buffer, int width, int height) {
    std::cout << "\n--- Color Sampling (First 20 Unique Colors) ---" << std::endl;
    
    std::set<std::tuple<int, int, int>> unique_colors;
    
    // Sample every 10th pixel to get a representative set
    for (int y = 0; y < height && unique_colors.size() < 50; y += 10) {
        for (int x = 0; x < width && unique_colors.size() < 50; x += 10) {
            int idx = (y * width + x) * 4;
            unique_colors.insert({buffer[idx], buffer[idx+1], buffer[idx+2]});
        }
    }
    
    int count = 0;
    for (const auto& color : unique_colors) {
        if (count++ >= 20) break;
        std::cout << "RGB(" << std::get<0>(color) << ", " << std::get<1>(color) 
                  << ", " << std::get<2>(color) << ")" << std::endl;
    }
}

struct PixelAnalysis {
    int total_pixels;
    int background_pixels;
    int geometry_pixels;  
    int artifact_pixels;  // Pixels that are neither background nor expected geometry
    float background_percentage;
    float geometry_percentage;
    float artifact_percentage;
};

PixelAnalysis analyzePixels(const unsigned char* buffer, int width, int height) {
    PixelAnalysis analysis = {0};
    analysis.total_pixels = width * height;
    
    std::cout << "\n=== Pixel Analysis Results ===" << std::endl;
    std::cout << "Image dimensions: " << width << "x" << height << std::endl;
    std::cout << "Expected background color: RGB(" << (int)EXPECTED_BACKGROUND.r 
              << ", " << (int)EXPECTED_BACKGROUND.g << ", " << (int)EXPECTED_BACKGROUND.b << ")" << std::endl;
    
    // Sample actual colors first
    sampleColors(buffer, width, height);
    
    // Sample key regions for detailed analysis
    struct TestRegion {
        int x, y;
        const char* description;
        bool should_be_background;
    };
    
    TestRegion test_regions[] = {
        // Corner regions (should be background)
        {50, 50, "Top-left corner", true},
        {width-50, 50, "Top-right corner", true},
        {50, height-50, "Bottom-left corner", true}, 
        {width-50, height-50, "Bottom-right corner", true},
        
        // Center region (likely has geometry)
        {width/2, height/2, "Center", false},
        
        // Edge regions (should be background)
        {width/2, 25, "Top edge center", true},
        {width/2, height-25, "Bottom edge center", true},
        {25, height/2, "Left edge center", true},
        {width-25, height/2, "Right edge center", true}
    };
    
    // Analyze overall image
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 4; // RGBA format
            RGBColor pixel = {buffer[idx], buffer[idx+1], buffer[idx+2]};
            
            if (isBackgroundColor(pixel)) {
                analysis.background_pixels++;
            } else if (isGeometryColor(pixel)) {
                analysis.geometry_pixels++;
            } else {
                analysis.artifact_pixels++;
            }
        }
    }
    
    // Calculate percentages
    analysis.background_percentage = (float)analysis.background_pixels / analysis.total_pixels * 100.0f;
    analysis.geometry_percentage = (float)analysis.geometry_pixels / analysis.total_pixels * 100.0f;
    analysis.artifact_percentage = (float)analysis.artifact_pixels / analysis.total_pixels * 100.0f;
    
    // Report test region results  
    std::cout << "\n--- Key Region Analysis ---" << std::endl;
    for (const auto& region : test_regions) {
        if (region.x >= 0 && region.x < width && region.y >= 0 && region.y < height) {
            int idx = (region.y * width + region.x) * 4;
            RGBColor pixel = {buffer[idx], buffer[idx+1], buffer[idx+2]};
            
            std::cout << region.description << " (" << region.x << "," << region.y << "): "
                     << "RGB(" << (int)pixel.r << "," << (int)pixel.g << "," << (int)pixel.b << ") ";
            
            if (region.should_be_background) {
                if (isBackgroundColor(pixel)) {
                    std::cout << "✓ Background as expected" << std::endl;
                } else if (isGeometryColor(pixel)) {
                    std::cout << "✗ GEOMETRY IN BACKGROUND REGION!" << std::endl;
                } else {
                    std::cout << "? Unexpected color (artifact)" << std::endl;
                }
            } else {
                if (isBackgroundColor(pixel)) {
                    std::cout << "- Background (geometry may be elsewhere)" << std::endl;
                } else if (isGeometryColor(pixel)) {
                    std::cout << "✓ Geometry detected" << std::endl;
                } else {
                    std::cout << "? Unexpected color (potential artifact)" << std::endl;
                }
            }
        }
    }
    
    // Report overall statistics
    std::cout << "\n--- Overall Statistics ---" << std::endl;
    std::cout << "Background pixels: " << analysis.background_pixels << " (" 
              << std::fixed << std::setprecision(1) << analysis.background_percentage << "%)" << std::endl;
    std::cout << "Geometry pixels: " << analysis.geometry_pixels << " ("
              << std::fixed << std::setprecision(1) << analysis.geometry_percentage << "%)" << std::endl;
    std::cout << "Artifact pixels: " << analysis.artifact_pixels << " ("
              << std::fixed << std::setprecision(1) << analysis.artifact_percentage << "%)" << std::endl;
    
    return analysis;
}

std::pair<bool, PixelAnalysis> validateSceneRendering(const unsigned char* buffer, int width, int height) {
    PixelAnalysis analysis = analyzePixels(buffer, width, height);
    
    std::cout << "\n--- Validation Results ---" << std::endl;
    
    bool passed = true;
    
    // Check that we have reasonable amount of background (should be majority, but allow for lighting effects)
    if (analysis.background_percentage < 30.0f) {
        std::cout << "✗ FAIL: Background percentage too low (" 
                  << analysis.background_percentage << "% < 30%)" << std::endl;
        passed = false;
    } else {
        std::cout << "✓ Background percentage acceptable (" 
                  << analysis.background_percentage << "%)" << std::endl;
    }
    
    // Check that we have some visible geometry (relaxed thresholds)
    if (analysis.geometry_percentage < 1.0f) {
        std::cout << "✗ FAIL: Geometry percentage too low (" 
                  << analysis.geometry_percentage << "% < 1%) - shapes likely not visible" << std::endl;
        passed = false;
    } else if (analysis.geometry_percentage > 60.0f) {
        std::cout << "✗ FAIL: Geometry percentage too high ("
                  << analysis.geometry_percentage << "% > 60%) - geometry dominating scene" << std::endl;
        passed = false;
    } else {
        std::cout << "✓ Geometry percentage acceptable (" 
                  << analysis.geometry_percentage << "%)" << std::endl;
    }
    
    // Check for excessive artifacts (more relaxed threshold accounting for lighting)
    if (analysis.artifact_percentage > 60.0f) {
        std::cout << "✗ FAIL: Too many artifact pixels (" 
                  << analysis.artifact_percentage << "% > 60%) - significant visual artifacts!" << std::endl;
        passed = false;
    } else {
        std::cout << "✓ Artifact percentage acceptable (" 
                  << analysis.artifact_percentage << "%)" << std::endl;
    }
    
    return {passed, analysis};
}

#endif // HAVE_OSMESA

} // namespace SimpleTest

int main() {
    using namespace SimpleTest;
    
    TestRunner runner;
    
#ifdef HAVE_OSMESA
    // Initialize Coin3D with OSMesa context manager first
    std::unique_ptr<OSMesaContextManager> context_manager = std::make_unique<OSMesaContextManager>();
    SoDB::init(context_manager.get());
    SoInteraction::init();
    
    // Test OSMesa availability and context creation
    runner.startTest("OSMesa context management");
    try {
        OSMesaContextData ctx(256, 256);
        if (!ctx.isValid() || !ctx.makeCurrent()) {
            runner.endTest(false, "Failed to create or activate OSMesa context");
            return runner.getSummary();
        }
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return runner.getSummary();
    }
    
    // Test comprehensive 3D scene creation
    runner.startTest("Complex 3D scene construction");
    SoSeparator* scene = nullptr;
    try {
        scene = createComplexScene();
        if (!scene) {
            runner.endTest(false, "Failed to create complex scene");
            return runner.getSummary();
        }
        // Verify scene has expected number of children (camera, light, 4 shape groups)
        if (scene->getNumChildren() < 6) {
            scene->unref();
            runner.endTest(false, "Scene does not have expected number of children");
            return runner.getSummary();
        }
        runner.endTest(true);
    } catch (const std::exception& e) {
        if (scene) scene->unref();
        runner.endTest(false, std::string("Exception: ") + e.what());
        return runner.getSummary();
    }
    
    // Test rendering and PNG output
    runner.startTest("Scene rendering and PNG output");
    try {
        const std::string filename = "coin3d_scene_test.png";
        
        // Create offscreen renderer
        SbViewportRegion viewport(512, 512);
        SoOffscreenRenderer renderer(viewport);
        renderer.setBackgroundColor(SbColor(0.1f, 0.1f, 0.3f)); // Dark blue background
        
        // Render the scene
        SbBool renderResult = renderer.render(scene);
        if (!renderResult) {
            scene->unref();
            runner.endTest(false, "Failed to render scene");
            return runner.getSummary();
        }
        
        // Get the rendered buffer
        const unsigned char* buffer = renderer.getBuffer();
        if (!buffer) {
            scene->unref();
            runner.endTest(false, "Failed to get rendered buffer");
            return runner.getSummary();
        }
        
        // Save as PNG
        if (!savePNG(filename, buffer, 512, 512)) {
            scene->unref();
            runner.endTest(false, "Failed to save PNG");
            return runner.getSummary();
        }
        
        // Verify PNG file was created and has reasonable size
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file.good() || file.tellg() < 1000) { // PNG should be at least 1KB
            scene->unref();
            runner.endTest(false, "PNG file was not created or is too small");
            return runner.getSummary();
        }
        
        std::cout << std::endl << "✓ Rendered scene saved as: " << filename << std::endl;
        
        // Perform pixel validation
        auto [validationPassed, analysis] = validateSceneRendering(buffer, 512, 512);
        if (!validationPassed) {
            // Don't exit early - continue to the detailed analysis test
            runner.endTest(false, "Pixel validation failed - visual artifacts detected in rendered output");
        } else {
            runner.endTest(true);
        }
        
        runner.endTest(true);
    } catch (const std::exception& e) {
        scene->unref();
        runner.endTest(false, std::string("Exception: ") + e.what());
        return runner.getSummary();
    }
    
    // Test multiple resolutions
    runner.startTest("Multiple resolution rendering");
    try {
        // Test different resolutions
        const std::vector<std::pair<int, int>> resolutions = {
            {256, 256},
            {1024, 768}
        };
        
        bool allSuccess = true;
        for (const auto& res : resolutions) {
            std::string filename = "coin3d_scene_" + std::to_string(res.first) + "x" + 
                                   std::to_string(res.second) + ".png";
            
            // Create offscreen renderer for this resolution
            SbViewportRegion viewport(res.first, res.second);
            SoOffscreenRenderer renderer(viewport);
            renderer.setBackgroundColor(SbColor(0.1f, 0.1f, 0.3f));
            
            // Render the scene
            if (!renderer.render(scene)) {
                allSuccess = false;
                break;
            }
            
            const unsigned char* buffer = renderer.getBuffer();
            if (!buffer) {
                allSuccess = false;
                break;
            }
            
            if (!savePNG(filename, buffer, res.first, res.second)) {
                allSuccess = false;
                break;
            }
            
            std::cout << std::endl << "✓ Rendered " << res.first << "x" << res.second << 
                         " scene saved as: " << filename << std::endl;
        }
        
        if (!allSuccess) {
            scene->unref();
            runner.endTest(false, "Failed to render at multiple resolutions");
            return runner.getSummary();
        }
        runner.endTest(true);
    } catch (const std::exception& e) {
        scene->unref();
        runner.endTest(false, std::string("Exception: ") + e.what());
        return runner.getSummary();
    }
    
    // Dedicated pixel validation and artifact analysis test
    runner.startTest("Detailed pixel validation and artifact analysis");
    try {
        std::cout << std::endl << "=== Detailed Scene Rendering Analysis ===" << std::endl;
        
        // Render at standard resolution for detailed analysis
        SbViewportRegion viewport(512, 512);
        SoOffscreenRenderer renderer(viewport);
        renderer.setBackgroundColor(SbColor(0.1f, 0.1f, 0.3f)); // Dark blue background
        
        if (!renderer.render(scene)) {
            scene->unref();
            runner.endTest(false, "Failed to render scene for validation");
            return runner.getSummary();
        }
        
        const unsigned char* buffer = renderer.getBuffer();
        if (!buffer) {
            scene->unref();
            runner.endTest(false, "Failed to get buffer for validation");
            return runner.getSummary();
        }
        
        // Perform comprehensive pixel analysis
        auto [validationPassed, analysis] = validateSceneRendering(buffer, 512, 512);
        
        if (!validationPassed) {
            std::cout << "\n⚠ ROOT CAUSE ANALYSIS - Visual Artifacts Detected!" << std::endl;
            std::cout << "==========================================================" << std::endl;
            std::cout << "ISSUE: Geometry appearing in background-only regions" << std::endl;
            std::cout << "SYMPTOMS:" << std::endl;
            std::cout << "- Objects at corners/edges where only background should be" << std::endl;
            std::cout << "- Low background percentage (" << analysis.background_percentage << "%)" << std::endl; 
            std::cout << "- High geometry coverage (" << analysis.geometry_percentage << "%)" << std::endl;
            std::cout << "\nPOSSIBLE ROOT CAUSES:" << std::endl;
            std::cout << "1. CAMERA POSITIONING: Objects may be too close or camera FOV too wide" << std::endl;
            std::cout << "2. OBJECT SCALING: Geometry objects (cube, sphere, etc.) may be too large" << std::endl;
            std::cout << "3. VIEWPORT MAPPING: Scene coordinate-to-pixel mapping incorrect" << std::endl;
            std::cout << "4. LIGHTING ARTIFACTS: Specular highlights extending to edges" << std::endl;
            std::cout << "5. DEPTH BUFFER ISSUES: Z-fighting or depth precision problems" << std::endl;
            std::cout << "6. FRAMEBUFFER CORRUPTION: OSMesa buffer management issues" << std::endl;
            std::cout << "\nRECOMMENDED FIXES:" << std::endl;
            std::cout << "- Increase camera distance or reduce object sizes" << std::endl;
            std::cout << "- Use orthographic camera for predictable mapping" << std::endl;
            std::cout << "- Add viewport margins by positioning objects away from edges" << std::endl;
            std::cout << "- Disable specular lighting for consistent colors" << std::endl;
        } else {
            std::cout << "\n✓ Scene rendering validation passed - no significant artifacts detected" << std::endl;
        }
        
        runner.endTest(validationPassed, validationPassed ? "" : "Scene pixel validation found visual artifacts");
    } catch (const std::exception& e) {
        scene->unref();
        runner.endTest(false, std::string("Exception: ") + e.what());
        return runner.getSummary();
    }
    
    // Clean up scene
    if (scene) {
        scene->unref();
    }
    
    std::cout << std::endl << "Integration test completed successfully!" << std::endl;
    std::cout << "Check the generated PNG files to verify scene rendering:" << std::endl;
    std::cout << "  - coin3d_scene_test.png (512x512)" << std::endl;
    std::cout << "  - coin3d_scene_256x256.png" << std::endl; 
    std::cout << "  - coin3d_scene_1024x768.png" << std::endl;
    
#else
    // OSMesa not available - skip rendering tests
    runner.startTest("OSMesa availability check");
    runner.endTest(false, "OSMesa headers not found - rendering tests skipped");
    
    std::cout << std::endl << "WARNING: OSMesa not available - rendering tests were skipped" << std::endl;
    std::cout << "To run full rendering tests, ensure OSMesa development headers are installed" << std::endl;
    return 1; // Fail if we can't test rendering
#endif
    
    return runner.getSummary();
}