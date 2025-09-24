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
 * @file test_sphere_positioning.cpp
 * @brief Test for precise sphere positioning to verify pixel-accurate rendering
 *
 * Creates a simple scene with uniform background and a small colored sphere
 * at a known position to verify that sphere pixels appear only where expected
 * in the rendered output grid.
 */

#include "test_utils.h"
#include <memory>

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <Inventor/SoDB.h>
#include <Inventor/SoInteraction.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbColor.h>

// OSMesa headers for headless OpenGL
#ifdef __has_include
  #if __has_include(<OSMesa/osmesa.h>)
    #include <OSMesa/osmesa.h>
    #include <OSMesa/gl.h>
  #endif
#endif

// Fallback to system OpenGL headers if OSMesa not available
#ifndef OSMESA_MAJOR_VERSION
  #ifdef __APPLE__
    #include <OpenGL/gl.h>
  #else
    #include <GL/gl.h>
  #endif
#endif

// Small PNG encoder from single header library
#include "../src/glue/svpng.h"

using namespace std;

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

// Test configuration
constexpr int IMAGE_WIDTH = 256;
constexpr int IMAGE_HEIGHT = 256;
constexpr float SPHERE_RADIUS = 0.2f;
constexpr float SPHERE_X = 0.3f;  // Offset right from center
constexpr float SPHERE_Y = 0.2f;  // Offset up from center

// Expected sphere center in pixel coordinates (accounting for viewport transform)
constexpr int EXPECTED_SPHERE_CENTER_X = static_cast<int>(IMAGE_WIDTH * (0.5f + SPHERE_X * 0.25f));
constexpr int EXPECTED_SPHERE_CENTER_Y = static_cast<int>(IMAGE_HEIGHT * (0.5f + SPHERE_Y * 0.25f));
constexpr int EXPECTED_SPHERE_RADIUS_PX = static_cast<int>(SPHERE_RADIUS * IMAGE_WIDTH * 0.25f);

// Colors
struct RGB { unsigned char r, g, b; };
constexpr RGB BACKGROUND_COLOR = {50, 50, 50};    // Dark gray background
constexpr RGB SPHERE_COLOR = {255, 100, 100};     // Red sphere

void savePNG(const char* filename, const unsigned char* buffer, int width, int height, int components) {
    FILE* fp = fopen(filename, "wb");
    if (!fp) return;
    
    if (components == 3) {
        // RGB buffer - save directly
        svpng(fp, width, height, buffer, 0);
    } else if (components == 4) {
        // RGBA buffer - extract RGB channels
        unsigned char* rgb_buffer = new unsigned char[width * height * 3];
        for (int i = 0; i < width * height; i++) {
            rgb_buffer[i*3 + 0] = buffer[i*4 + 0]; // R
            rgb_buffer[i*3 + 1] = buffer[i*4 + 1]; // G
            rgb_buffer[i*3 + 2] = buffer[i*4 + 2]; // B
        }
        svpng(fp, width, height, rgb_buffer, 0);
        delete[] rgb_buffer;
    }
    fclose(fp);
}

SoSeparator* createSphereScene() {
    SoSeparator* root = new SoSeparator;
    
    // Add camera
    SoPerspectiveCamera* camera = new SoPerspectiveCamera;
    camera->position.setValue(0.0f, 0.0f, 3.0f);
    camera->orientation.setValue(SbVec3f(0, 1, 0), 0);
    camera->nearDistance = 1.0f;
    camera->farDistance = 10.0f;
    root->addChild(camera);
    
    // Add uniform background lighting
    SoDirectionalLight* light = new SoDirectionalLight;
    light->direction.setValue(0, 0, -1);
    light->intensity = 1.0f;
    root->addChild(light);
    
    // Create sphere with emissive material for consistent color
    SoSeparator* sphereGroup = new SoSeparator;
    
    // Material with emissive color (ignores lighting)
    SoMaterial* material = new SoMaterial;
    material->emissiveColor.setValue(SPHERE_COLOR.r/255.0f, SPHERE_COLOR.g/255.0f, SPHERE_COLOR.b/255.0f);
    material->diffuseColor.setValue(0.0f, 0.0f, 0.0f);  // No diffuse contribution
    sphereGroup->addChild(material);
    
    // Position the sphere
    SoTransform* transform = new SoTransform;
    transform->translation.setValue(SPHERE_X, SPHERE_Y, 0.0f);
    sphereGroup->addChild(transform);
    
    // Create the sphere
    SoSphere* sphere = new SoSphere;
    sphere->radius = SPHERE_RADIUS;
    sphereGroup->addChild(sphere);
    
    root->addChild(sphereGroup);
    
    return root;
}

bool analyzeSpherePositioning(const unsigned char* buffer, int width, int height, int components) {
    cout << "=== Sphere Position Analysis ===" << endl;
    cout << "Image size: " << width << "x" << height << endl;
    cout << "Expected sphere center: (" << EXPECTED_SPHERE_CENTER_X << ", " << EXPECTED_SPHERE_CENTER_Y << ")" << endl;
    cout << "Expected sphere radius: ~" << EXPECTED_SPHERE_RADIUS_PX << " pixels" << endl;
    
    // Sample key positions
    struct TestPoint {
        int x, y;
        const char* description;
        bool should_be_sphere;
    };
    
    TestPoint test_points[] = {
        {EXPECTED_SPHERE_CENTER_X, EXPECTED_SPHERE_CENTER_Y, "Sphere center", true},
        {EXPECTED_SPHERE_CENTER_X - EXPECTED_SPHERE_RADIUS_PX/2, EXPECTED_SPHERE_CENTER_Y, "Left of center", true},
        {EXPECTED_SPHERE_CENTER_X + EXPECTED_SPHERE_RADIUS_PX/2, EXPECTED_SPHERE_CENTER_Y, "Right of center", true},
        {EXPECTED_SPHERE_CENTER_X, EXPECTED_SPHERE_CENTER_Y - EXPECTED_SPHERE_RADIUS_PX/2, "Below center", true},
        {EXPECTED_SPHERE_CENTER_X, EXPECTED_SPHERE_CENTER_Y + EXPECTED_SPHERE_RADIUS_PX/2, "Above center", true},
        {50, 50, "Top-left corner", false},
        {width-50, 50, "Top-right corner", false},
        {50, height-50, "Bottom-left corner", false},
        {width-50, height-50, "Bottom-right corner", false},
    };
    
    bool all_correct = true;
    
    for (const auto& point : test_points) {
        if (point.x >= 0 && point.x < width && point.y >= 0 && point.y < height) {
            int pixel_idx = (point.y * width + point.x) * components;
            RGB pixel_color = {buffer[pixel_idx], buffer[pixel_idx + 1], buffer[pixel_idx + 2]};
            
            // Determine if this pixel looks like sphere or background
            bool is_sphere_color = (abs(pixel_color.r - SPHERE_COLOR.r) < 30 && 
                                   abs(pixel_color.g - SPHERE_COLOR.g) < 30 && 
                                   abs(pixel_color.b - SPHERE_COLOR.b) < 30);
            
            bool is_background_color = (abs(pixel_color.r - BACKGROUND_COLOR.r) < 30 && 
                                       abs(pixel_color.g - BACKGROUND_COLOR.g) < 30 && 
                                       abs(pixel_color.b - BACKGROUND_COLOR.b) < 30);
            
            cout << point.description << " (" << point.x << "," << point.y << "): "
                 << "RGB(" << (int)pixel_color.r << "," << (int)pixel_color.g << "," << (int)pixel_color.b << ") ";
            
            if (point.should_be_sphere) {
                if (is_sphere_color) {
                    cout << "✓ Sphere color as expected" << endl;
                } else {
                    cout << "✗ Expected sphere color, got ";
                    if (is_background_color) {
                        cout << "background color";
                    } else {
                        cout << "unexpected color";
                    }
                    cout << endl;
                    all_correct = false;
                }
            } else {
                if (is_background_color) {
                    cout << "✓ Background color as expected" << endl;
                } else if (is_sphere_color) {
                    cout << "✗ Expected background, got sphere color - POSITIONING ERROR!" << endl;
                    all_correct = false;
                } else {
                    cout << "? Unexpected color (neither sphere nor background)" << endl;
                }
            }
        }
    }
    
    // Count sphere pixels
    int sphere_pixel_count = 0;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int pixel_idx = (y * width + x) * components;
            RGB pixel_color = {buffer[pixel_idx], buffer[pixel_idx + 1], buffer[pixel_idx + 2]};
            
            if (abs(pixel_color.r - SPHERE_COLOR.r) < 30 && 
                abs(pixel_color.g - SPHERE_COLOR.g) < 30 && 
                abs(pixel_color.b - SPHERE_COLOR.b) < 30) {
                sphere_pixel_count++;
            }
        }
    }
    
    cout << "Total sphere pixels found: " << sphere_pixel_count << endl;
    cout << "Expected sphere area: ~" << (3.14159f * EXPECTED_SPHERE_RADIUS_PX * EXPECTED_SPHERE_RADIUS_PX) << " pixels" << endl;
    
    return all_correct;
}

int main() {
    cout << "Running: Sphere positioning test..." << endl;
    
    // Initialize Coin with OSMesa context manager
    try {
        std::unique_ptr<OSMesaContextManager> context_manager = std::make_unique<OSMesaContextManager>();
        SoDB::init(context_manager.get());
        SoInteraction::init();
    
    // Create test scene
    SoSeparator* root = createSphereScene();
    
    // Set up viewport and render
    SbViewportRegion viewport(IMAGE_WIDTH, IMAGE_HEIGHT);
    SoOffscreenRenderer renderer(viewport);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(BACKGROUND_COLOR.r/255.0f, BACKGROUND_COLOR.g/255.0f, BACKGROUND_COLOR.b/255.0f));
    
    if (!renderer.render(root)) {
        cout << "Failed to render scene" << endl;
        root->unref();
        return 1;
    }
    
    // Get rendered buffer
    const unsigned char* buffer = renderer.getBuffer();
    
    // Save image for visual inspection
    savePNG("sphere_positioning_test.png", buffer, IMAGE_WIDTH, IMAGE_HEIGHT, 3);
    cout << "Test image saved as: sphere_positioning_test.png" << endl;
    
    // Analyze positioning
    bool positioning_correct = analyzeSpherePositioning(buffer, IMAGE_WIDTH, IMAGE_HEIGHT, 3);
    
        // Cleanup
        root->unref();
        SoDB::finish();
        
        if (positioning_correct) {
            cout << " PASSED" << endl;
            return 0;
        } else {
            cout << " FAILED - Sphere positioning issues detected" << endl;
            return 1;
        }
    } catch (const std::exception& e) {
        cout << "Error: " << e.what() << endl;
        return 1;
    } catch (...) {
        cout << "Unknown error occurred" << endl;
        return 1;
    }
}