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

// Simple PNG writer using built-in functionality
void writePNG(const std::string& filename, const unsigned char* pixels, int width, int height) {
    std::ofstream file(filename, std::ios::binary);
    if (!file) return;
    
    // For testing purposes, write a simple PGM format instead of PNG
    // This is easier and doesn't require external dependencies
    file << "P6\n" << width << " " << height << "\n255\n";
    
    // Convert RGBA to RGB and flip vertically (OpenGL is bottom-up)
    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 4; // Assuming RGBA format
            file.put(pixels[idx]);     // R
            file.put(pixels[idx + 1]); // G  
            file.put(pixels[idx + 2]); // B
            // Skip alpha channel
        }
    }
}

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
        
        // Write test image to verify FBO rendering
        writePNG("/tmp/fbo_test_basic.ppm", image, size[0], size[1]);
        
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
            
            // Write test images for different sizes
            std::string filename = "/tmp/fbo_test_" + std::to_string(size[0]) + "x" + std::to_string(size[1]) + ".ppm";
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

#endif // COIN3D_OSMESA_BUILD
