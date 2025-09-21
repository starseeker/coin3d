#ifdef COIN3D_OSMESA_BUILD
// OSMesa integration for Coin3D context management testing

#include <OSMesa/osmesa.h>
#include <OSMesa/gl.h>
#include "../utils/internal_glue.h"
#include <Inventor/SoDB.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/rendering/SoOffscreenRenderer.h>
#include <Inventor/SbViewportRegion.h>
#include <catch2/catch_test_macros.hpp>
#include <memory>

namespace {

// OSMesa Context wrapper for testing
struct OSMesaTestContext {
    OSMesaContext context;
    std::unique_ptr<unsigned char[]> buffer;
    int width, height;
    
    OSMesaTestContext(int w, int h) : width(w), height(h) {
        context = OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, NULL);
        if (context) {
            buffer = std::make_unique<unsigned char[]>(width * height * 4);
        }
    }
    
    ~OSMesaTestContext() {
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
static void* osmesa_test_create_offscreen(unsigned int width, unsigned int height) {
    auto* ctx = new OSMesaTestContext(width, height);
    if (!ctx->isValid()) {
        delete ctx;
        return nullptr;
    }
    return ctx;
}

static SbBool osmesa_test_make_current(void* context) {
    if (!context) return FALSE;
    auto* ctx = static_cast<OSMesaTestContext*>(context);
    return ctx->makeCurrent() ? TRUE : FALSE;
}

static void osmesa_test_reinstate_previous(void* context) {
    // OSMesa doesn't need explicit context switching in our test setup
    (void)context;
}

static void osmesa_test_destruct(void* context) {
    if (context) {
        auto* ctx = static_cast<OSMesaTestContext*>(context);
        delete ctx;
    }
}

// RAII helper for callback management
class OSMesaCallbackManager {
private:
    cc_glglue_offscreen_cb_functions callbacks;
    cc_glglue_offscreen_cb_functions* previous_callbacks;
    
public:
    OSMesaCallbackManager() {
        callbacks.create_offscreen = osmesa_test_create_offscreen;
        callbacks.make_current = osmesa_test_make_current;
        callbacks.reinstate_previous = osmesa_test_reinstate_previous;
        callbacks.destruct = osmesa_test_destruct;
        
        // Install our callbacks
        cc_glglue_context_set_offscreen_cb_functions(&callbacks);
    }
    
    ~OSMesaCallbackManager() {
        // Restore previous callbacks (likely null)
        cc_glglue_context_set_offscreen_cb_functions(nullptr);
    }
};

} // anonymous namespace

TEST_CASE("OSMesa Context Management", "[osmesa][context]") {
    
    SECTION("OSMesa callback installation and basic functionality") {
        OSMesaCallbackManager manager;
        
        // Test context creation via Coin3D API
        void* ctx = cc_glglue_context_create_offscreen(256, 256);
        REQUIRE(ctx != nullptr);
        
        // Test context activation
        SbBool result = cc_glglue_context_make_current(ctx);
        REQUIRE(result == TRUE);
        
        // Test basic OpenGL operations
        glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        GLenum error = glGetError();
        REQUIRE(error == GL_NO_ERROR);
        
        // Test context deactivation and destruction
        cc_glglue_context_reinstate_previous(ctx);
        cc_glglue_context_destruct(ctx);
    }
    
    SECTION("SoOffscreenRenderer with OSMesa context") {
        OSMesaCallbackManager manager;
        
        // Create a simple scene
        SoSeparator* root = new SoSeparator;
        root->ref();
        
        SoCube* cube = new SoCube;
        root->addChild(cube);
        
        // Test offscreen rendering
        SbViewportRegion viewport(128, 128);
        SoOffscreenRenderer renderer(viewport);
        renderer.setBackgroundColor(SbColor(0.2f, 0.3f, 0.4f));
        
        // This should work with our OSMesa context callbacks
        SbBool renderResult = renderer.render(root);
        REQUIRE(renderResult == TRUE);
        
        // Test getting the rendered image
        const unsigned char* image = renderer.getBuffer();
        REQUIRE(image != nullptr);
        
        root->unref();
    }
    
    SECTION("Multiple context creation and management") {
        OSMesaCallbackManager manager;
        
        // Test creating multiple contexts
        void* ctx1 = cc_glglue_context_create_offscreen(64, 64);
        void* ctx2 = cc_glglue_context_create_offscreen(128, 128);
        
        REQUIRE(ctx1 != nullptr);
        REQUIRE(ctx2 != nullptr);
        REQUIRE(ctx1 != ctx2);
        
        // Test switching between contexts
        REQUIRE(cc_glglue_context_make_current(ctx1) == TRUE);
        glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
        
        REQUIRE(cc_glglue_context_make_current(ctx2) == TRUE);
        glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
        
        // Clean up
        cc_glglue_context_destruct(ctx1);
        cc_glglue_context_destruct(ctx2);
    }
}

TEST_CASE("Context Creation Without Callbacks", "[context][error]") {
    
    SECTION("Error handling when no callbacks are provided") {
        // Ensure no callbacks are set
        cc_glglue_context_set_offscreen_cb_functions(nullptr);
        
        // This should fail gracefully
        void* ctx = cc_glglue_context_create_offscreen(128, 128);
        REQUIRE(ctx == nullptr);
        
        // Test other functions with null context should not crash
        SbBool result = cc_glglue_context_make_current(nullptr);
        REQUIRE(result == FALSE);
        
        // These should not crash
        cc_glglue_context_reinstate_previous(nullptr);
        cc_glglue_context_destruct(nullptr);
    }
}

#endif // COIN3D_OSMESA_BUILD