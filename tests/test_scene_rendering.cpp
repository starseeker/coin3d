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
            buffer = std::make_unique<unsigned char[]>(width * height * 4);
        }
    }
    
    ~OSMesaContextData() {
        if (context) OSMesaDestroyContext(context);
    }
    
    bool makeCurrent() {
        return context && OSMesaMakeCurrent(context, buffer.get(), GL_UNSIGNED_BYTE, width, height);
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