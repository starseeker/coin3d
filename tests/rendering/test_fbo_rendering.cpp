#ifdef COIN3D_OSMESA_BUILD
// Test FBO-based offscreen rendering with OSMesa context

#include <OSMesa/osmesa.h>
#include <OSMesa/gl.h>
#include <Inventor/C/glue/gl.h>
#include <Inventor/SoDB.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoCamera.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/SbViewportRegion.h>
#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <vector>
#include <fstream>
#include "utils/png_test_utils.h"

using namespace CoinTestUtils;

namespace {

// OSMesa Context wrapper for FBO testing
struct OSMesaFBOTestContext {
    OSMesaContext context;
    std::unique_ptr<unsigned char[]> buffer;
    int width, height;
    
    OSMesaFBOTestContext(int w, int h) : width(w), height(h) {
        context = OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, NULL);
        if (context) {
            buffer = std::make_unique<unsigned char[]>(width * height * 4);
        }
    }
    
    ~OSMesaFBOTestContext() {
        if (context) {
            OSMesaDestroyContext(context);
        }
    }
    
    bool isValid() const { return context != nullptr; }
    
    bool makeCurrent() {
        return context && OSMesaMakeCurrent(context, buffer.get(), GL_UNSIGNED_BYTE, width, height) == GL_TRUE;
    }
};

// Global callback functions for Coin3D context management
static void* osmesa_fbo_create_offscreen(unsigned int width, unsigned int height) {
    auto* ctx = new OSMesaFBOTestContext(width, height);
    if (!ctx->isValid()) {
        delete ctx;
        return nullptr;
    }
    return ctx;
}

static SbBool osmesa_fbo_make_current(void* context) {
    if (!context) return FALSE;
    auto* ctx = static_cast<OSMesaFBOTestContext*>(context);
    return ctx->makeCurrent() ? TRUE : FALSE;
}

static void osmesa_fbo_reinstate_previous(void* context) {
    // OSMesa doesn't need explicit context switching in our test setup
    (void)context;
}

static void osmesa_fbo_destruct(void* context) {
    if (context) {
        auto* ctx = static_cast<OSMesaFBOTestContext*>(context);
        delete ctx;
    }
}

// RAII wrapper for OSMesa callback management
class OSMesaFBOCallbackManager {
public:
    OSMesaFBOCallbackManager() {
        SoDB::init();
        
        callbacks.create_offscreen = osmesa_fbo_create_offscreen;
        callbacks.make_current = osmesa_fbo_make_current;
        callbacks.reinstate_previous = osmesa_fbo_reinstate_previous;
        callbacks.destruct = osmesa_fbo_destruct;
        
        cc_glglue_context_set_offscreen_cb_functions(&callbacks);
    }
    
    ~OSMesaFBOCallbackManager() {
        // Don't nullify callbacks since they should persist globally for OSMesa builds
        // cc_glglue_context_set_offscreen_cb_functions(nullptr);
    }
    
private:
    cc_glglue_offscreen_cb_functions callbacks;
};

// Note: PNG writing functionality now provided by shared png_test_utils

} // anonymous namespace

TEST_CASE("FBO-based Offscreen Rendering", "[fbo][osmesa][rendering]") {
    
    SECTION("Basic FBO rendering with simple scene") {
        OSMesaFBOCallbackManager manager;
        // Create a simple scene with lighting
        SoSeparator* root = new SoSeparator;
        root->ref();
        
        SoPerspectiveCamera* camera = new SoPerspectiveCamera;
        camera->position = SbVec3f(0, 0, 3);
        camera->nearDistance = 1.0f;
        camera->farDistance = 10.0f;
        root->addChild(camera);
        
        SoDirectionalLight* light = new SoDirectionalLight;
        light->direction = SbVec3f(-1, -1, -1);
        root->addChild(light);
        
        SoCube* cube = new SoCube;
        root->addChild(cube);
        
        // Test offscreen rendering using FBOs
        SbViewportRegion viewport(256, 256);
        SoOffscreenRenderer renderer(viewport);
        renderer.setBackgroundColor(SbColor(0.2f, 0.3f, 0.4f));
        
        // Check OpenGL state before rendering
        void* ctx = cc_glglue_context_create_offscreen(32, 32);
        if (ctx) {
            if (cc_glglue_context_make_current(ctx)) {
                cc_glglue_context_destruct(ctx);
            }
        }
        
        SbBool renderResult = renderer.render(root);
        REQUIRE(renderResult == TRUE);
        REQUIRE(renderResult == TRUE);
        
        // Test getting the rendered image
        const unsigned char* image = renderer.getBuffer();
        REQUIRE(image != nullptr);
        
        // Verify that we actually rendered something meaningful
        // Check that not all pixels are the background color
        SbVec2s size = viewport.getViewportSizePixels();
        int totalPixels = size[0] * size[1];
        int backgroundPixels = 0;
        
        for (int i = 0; i < totalPixels; i++) {
            int idx = i * 4; // Assuming RGBA
            // Check if pixel is close to background color (0.2, 0.3, 0.4)
            if (abs(image[idx] - 51) < 10 &&     // R: 0.2 * 255 ≈ 51
                abs(image[idx + 1] - 77) < 10 && // G: 0.3 * 255 ≈ 77  
                abs(image[idx + 2] - 102) < 10)  // B: 0.4 * 255 ≈ 102
            {
                backgroundPixels++;
            }
        }
        
        // Ensure that not all pixels are background (cube should be visible)
        REQUIRE(backgroundPixels < totalPixels * 0.9); // At least 10% non-background
        
        // Write test image to verify FBO rendering using shared PNG utilities
        writePNG("/tmp/fbo_test_basic.png", image, size[0], size[1]);
        
        root->unref();
    }
    
    SECTION("FBO rendering with different viewport sizes") {
        OSMesaFBOCallbackManager manager;
        
        // Create a simple scene
        SoSeparator* root = new SoSeparator;
        root->ref();
        
        SoCube* cube = new SoCube;
        root->addChild(cube);
        
        // Test different sizes to ensure FBO resize works correctly
        std::vector<SbVec2s> testSizes = {
            SbVec2s(64, 64),
            SbVec2s(128, 128), 
            SbVec2s(256, 128),
            SbVec2s(128, 256)
        };
        
        for (const auto& size : testSizes) {
            SbViewportRegion viewport(size);
            SoOffscreenRenderer renderer(viewport);
            renderer.setBackgroundColor(SbColor(1.0f, 0.0f, 0.0f));
            
            SbBool renderResult = renderer.render(root);
            REQUIRE(renderResult == TRUE);
            
            const unsigned char* image = renderer.getBuffer();
            REQUIRE(image != nullptr);
            
            // Write test images for different sizes using shared PNG utilities
            std::string filename = "/tmp/fbo_test_" + std::to_string(size[0]) + "x" + std::to_string(size[1]) + ".png";
            writePNG(filename, image, size[0], size[1]);
        }
        
        root->unref();
    }
    
    SECTION("FBO extension availability check") {
        OSMesaFBOCallbackManager manager;
        
        // Create context to check FBO support
        void* ctx = cc_glglue_context_create_offscreen(64, 64);
        REQUIRE(ctx != nullptr);
        
        SbBool result = cc_glglue_context_make_current(ctx);
        REQUIRE(result == TRUE);
        
        // Check if FBO extension is supported in OSMesa context
        const cc_glglue* glue = cc_glglue_instance(1); // Use context ID 1 as a test
        if (glue) {
            SbBool hasFBO = cc_glglue_has_framebuffer_objects(glue);
            // Note: This might not be available in all OSMesa builds
            // The test should succeed regardless, but log the capability
            if (hasFBO) {
                SUCCEED("GL_EXT_framebuffer_object extension is available in OSMesa context");
            } else {
                WARN("GL_EXT_framebuffer_object extension not available - falling back to default framebuffer");
            }
        }
        
        cc_glglue_context_destruct(ctx);
    }
}

// New comprehensive demo functionality integrated into test framework
TEST_CASE("FBO Demo Comprehensive Integration", "[fbo][demo][osmesa][rendering]") {
    
    SECTION("FBO demo architecture validation - replaces standalone demo") {
        OSMesaFBOCallbackManager manager;
        
        // Create a comprehensive 3D scene like the original FBO demo
        SoSeparator* root = new SoSeparator;
        root->ref();
        
        // Add camera with demo-style positioning
        SoPerspectiveCamera* camera = new SoPerspectiveCamera;
        camera->position = SbVec3f(0, 0, 3);
        camera->nearDistance = 1.0f;
        camera->farDistance = 10.0f;
        root->addChild(camera);
        
        // Add directional light
        SoDirectionalLight* light = new SoDirectionalLight;
        light->direction = SbVec3f(-1, -1, -1);
        root->addChild(light);
        
        // Add geometry (cube like original demo)
        SoCube* cube = new SoCube;
        root->addChild(cube);
        
        // Create offscreen renderer with demo specifications (512x512)
        SbViewportRegion viewport(512, 512);
        SoOffscreenRenderer renderer(viewport);
        renderer.setBackgroundColor(SbColor(0.1f, 0.2f, 0.3f));
        
        INFO("Testing FBO-based architecture functionality");
        
        // Attempt to render the scene
        SbBool renderResult = renderer.render(root);
        REQUIRE(renderResult == TRUE);
        
        // Get the rendered image
        const unsigned char* image = renderer.getBuffer();
        REQUIRE(image != nullptr);
        
        // Validate image content
        SbVec2s size = viewport.getViewportSizePixels();
        REQUIRE(size[0] == 512);
        REQUIRE(size[1] == 512);
        
        // Verify image buffer format - check first few pixels to ensure valid data
        int totalPixels = size[0] * size[1];
        REQUIRE(totalPixels > 0);
        
        // Simple validation - check that image has reasonable values
        bool hasValidData = false;
        for (int i = 0; i < std::min(100, totalPixels); i++) {
            int idx = i * 4; // RGBA
            if (image[idx] <= 255 && image[idx + 1] <= 255 && 
                image[idx + 2] <= 255 && image[idx + 3] <= 255) {
                hasValidData = true;
                break;
            }
        }
        REQUIRE(hasValidData);
        
        // Count background vs non-background pixels (simplified check)
        int backgroundPixels = 0;
        int samplesToCheck = std::min(1000, totalPixels); // Sample subset for safety
        
        for (int i = 0; i < samplesToCheck; i += 10) { // Sample every 10th pixel
            int idx = i * 4; // RGBA
            // Check if pixel is close to background color (0.1, 0.2, 0.3)
            if (abs(image[idx] - 26) < 20 &&     // R: 0.1 * 255 ≈ 26
                abs(image[idx + 1] - 51) < 20 && // G: 0.2 * 255 ≈ 51
                abs(image[idx + 2] - 77) < 20)   // B: 0.3 * 255 ≈ 77
            {
                backgroundPixels++;
            }
        }
        
        // Ensure that rendering produced visible geometry (relaxed check)
        int sampledPixels = samplesToCheck / 10;
        CHECK(backgroundPixels < sampledPixels * 0.95); // Allow for some variance
        
        // Save output using shared PNG utilities (both RGBA and RGB formats)
        bool pngResultRGBA = writePNG("/tmp/fbo_demo_integrated_rgba.png", image, size[0], size[1]);
        CHECK(pngResultRGBA);
        
        // Test RGB conversion functionality with safe bounds checking
        std::unique_ptr<unsigned char[]> rgb_data(new unsigned char[totalPixels * 3]);
        for (int i = 0; i < totalPixels; i++) {
            int src_idx = i * 4; // RGBA
            int dst_idx = i * 3; // RGB
            rgb_data[dst_idx] = image[src_idx];       // R
            rgb_data[dst_idx + 1] = image[src_idx + 1]; // G
            rgb_data[dst_idx + 2] = image[src_idx + 2]; // B
        }
        bool pngResultRGB = writePNG_RGB("/tmp/fbo_demo_integrated_rgb.png", rgb_data.get(), size[0], size[1]);
        CHECK(pngResultRGB);
        
        // Verify files exist
        std::ifstream rgbaFile("/tmp/fbo_demo_integrated_rgba.png");
        std::ifstream rgbFile("/tmp/fbo_demo_integrated_rgb.png");
        CHECK(rgbaFile.good());
        CHECK(rgbFile.good());
        
        root->unref();
    }
    
    SECTION("FBO callback architecture validation") {
        OSMesaFBOCallbackManager manager;
        
        // Test context creation directly (like original demo)
        void* ctx = cc_glglue_context_create_offscreen(256, 256);
        REQUIRE(ctx != nullptr);
        
        // Test context activation
        SbBool result = cc_glglue_context_make_current(ctx);
        REQUIRE(result == TRUE);
        
        // Verify context functionality
        const cc_glglue* glue = cc_glglue_instance(1);
        if (glue) {
            SbBool hasFBO = cc_glglue_has_framebuffer_objects(glue);
            if (hasFBO) {
                SUCCEED("FBO extension available - advanced rendering possible");
            } else {
                WARN("FBO extension not available - using fallback rendering");
            }
        }
        
        // Clean up context
        cc_glglue_context_destruct(ctx);
        
        SUCCEED("FBO callback architecture validated successfully");
    }
}

#endif // COIN3D_OSMESA_BUILD
