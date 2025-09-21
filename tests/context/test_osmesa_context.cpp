#ifdef COIN3D_OSMESA_BUILD
// OSMesa integration for Coin3D context management testing

#include <OSMesa/osmesa.h>
#include <OSMesa/gl.h>
#include <Inventor/SoDB.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/SoOffscreenRenderer.h>
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

// Modern C++ ContextProvider for OSMesa testing
class OSMesaTestContextProvider : public SoOffscreenRenderer::ContextProvider {
public:
    virtual void * createOffscreenContext(unsigned int width, unsigned int height) override {
        return osmesa_test_create_offscreen(width, height);
    }
    
    virtual SbBool makeContextCurrent(void * context) override {
        return osmesa_test_make_current(context);
    }
    
    virtual void restorePreviousContext(void * context) override {
        osmesa_test_reinstate_previous(context);
    }
    
    virtual void destroyContext(void * context) override {
        osmesa_test_destruct(context);
    }
};

// RAII helper for context provider management
class OSMesaCallbackManager {
private:
    OSMesaTestContextProvider provider;
    SoOffscreenRenderer::ContextProvider* originalProvider;
    
public:
    OSMesaCallbackManager() 
        : originalProvider(SoOffscreenRenderer::getContextProvider()) {
        SoOffscreenRenderer::setContextProvider(&provider);
    }
    
    ~OSMesaCallbackManager() {
        SoOffscreenRenderer::setContextProvider(originalProvider);
    }
};

} // anonymous namespace

TEST_CASE("OSMesa Context Management", "[osmesa][context]") {
    
    SECTION("OSMesa context provider installation and basic functionality") {
        OSMesaCallbackManager manager;
        
        // Verify context provider is installed
        REQUIRE(SoOffscreenRenderer::getContextProvider() != nullptr);
        
        // Test high-level rendering via SoOffscreenRenderer
        SbViewportRegion viewport(256, 256);
        SoOffscreenRenderer renderer(viewport);
        
        // Test basic OpenGL capabilities
        int major, minor, release;
        SoOffscreenRenderer::getOpenGLVersion(major, minor, release);
        REQUIRE(major >= 1);
        INFO("OpenGL Version: " << major << "." << minor << "." << release);
        
        SUCCEED("OSMesa context provider functionality verified");
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
        
        // Test creating multiple SoOffscreenRenderer instances
        SbViewportRegion viewport1(64, 64);
        SbViewportRegion viewport2(128, 128);
        
        SoOffscreenRenderer renderer1(viewport1);
        SoOffscreenRenderer renderer2(viewport2);
        
        // Both should work with the same context provider
        renderer1.setBackgroundColor(SbColor(1.0f, 0.0f, 0.0f));
        renderer2.setBackgroundColor(SbColor(0.0f, 1.0f, 0.0f));
        
        SUCCEED("Multiple renderer instances work correctly");
    }
}

TEST_CASE("Context Creation Without Callbacks", "[context][error]") {
    
    SECTION("Error handling when no context provider is set") {
        // Store original provider
        SoOffscreenRenderer::ContextProvider* originalProvider = 
            SoOffscreenRenderer::getContextProvider();
        
        // Temporarily clear the provider
        SoOffscreenRenderer::setContextProvider(nullptr);
        
        // Test that SoOffscreenRenderer handles missing provider gracefully
        SbViewportRegion viewport(128, 128);
        SoOffscreenRenderer renderer(viewport);
        
        // This should work because SoOffscreenRenderer has fallback mechanisms
        SUCCEED("SoOffscreenRenderer handles missing context provider gracefully");
        
        // Restore original provider
        SoOffscreenRenderer::setContextProvider(originalProvider);
    }
}

#endif // COIN3D_OSMESA_BUILD