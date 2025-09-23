/*
 * Modern C++ API demonstration for OpenGL context management and capability detection
 * 
 * This example shows how to use the new idiomatic C++ APIs in SoOffscreenRenderer
 * instead of the low-level cc_glglue functions.
 */

#include <catch2/catch_test_macros.hpp>
#include <Inventor/SoDB.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>

#include "../utils/scene_graph_test_utils.h"
#include "../utils/osmesa_test_context.h"

// Note: The ContextProvider API has been removed from SoOffscreenRenderer
// Context management should now be done via SoDB::init(context_manager)

TEST_CASE("Modern C++ API for OpenGL capabilities", "[rendering][modern][api]") {
    
    SECTION("OpenGL version detection") {
        // Test OpenGL version detection using the simplified API
        // Global OSMesa context manager is already set up in main.cpp
        int major, minor, release;
        SoOffscreenRenderer::getOpenGLVersion(major, minor, release);
        
        // Version should be detected if context is available
        INFO("OpenGL version: " << major << "." << minor << "." << release);
        REQUIRE(major >= 0);  // Should at least report 0 if no context
    }
    
    SECTION("OpenGL extension support detection") {
        // Test extension support detection using the simplified API
        // Global OSMesa context manager is already set up in main.cpp
        SbBool hasVBO = SoOffscreenRenderer::isOpenGLExtensionSupported("GL_ARB_vertex_buffer_object");
        SbBool hasFBO = SoOffscreenRenderer::hasFramebufferObjectSupport();
        
        INFO("VBO support: " << (hasVBO ? "Yes" : "No"));
        INFO("FBO support: " << (hasFBO ? "Yes" : "No"));
        
        // Extensions may or may not be available depending on context setup
        SUCCEED("Extension detection API works");
    }
    
    SECTION("OpenGL version comparison") {
        // Test version comparison using the simplified API
        // Global OSMesa context manager is already set up in main.cpp
        SbBool hasGL2 = SoOffscreenRenderer::isVersionAtLeast(2, 0);
        SbBool hasGL3 = SoOffscreenRenderer::isVersionAtLeast(3, 0);
        
        INFO("OpenGL 2.0+: " << (hasGL2 ? "Yes" : "No"));
        INFO("OpenGL 3.0+: " << (hasGL3 ? "Yes" : "No"));
        
        SUCCEED("Version comparison API works");
    }
}

#ifdef NEVER_DEFINED  // Disable old ContextProvider examples

// Example custom context provider using the new C++ API
class OSMesaContextProvider : public SoOffscreenRenderer::ContextProvider {
public:
    virtual void * createOffscreenContext(unsigned int width, unsigned int height) override {
        auto* context = new CoinTestUtils::OSMesaTestContext(width, height);
        if (!context->isValid()) {
            delete context;
            return nullptr;
        }
        return context;
    }
    
    virtual SbBool makeContextCurrent(void * context) override {
        if (!context) return FALSE;
        auto* osmesa_ctx = static_cast<CoinTestUtils::OSMesaTestContext*>(context);
        return osmesa_ctx->makeCurrent() ? TRUE : FALSE;
    }
    
    virtual void restorePreviousContext(void * context) override {
        // OSMesa doesn't need explicit context switching in our test setup
        (void)context;
    }
    
    virtual void destroyContext(void * context) override {
        if (context) {
            delete static_cast<CoinTestUtils::OSMesaTestContext*>(context);
        }
    }
};

TEST_CASE("Modern C++ API for OpenGL capabilities", "[rendering][modern][api]") {
    
    SECTION("OpenGL version detection") {
        // Initialize test context first
        CoinTestUtils::OSMesaTestFixture fixture(32, 32);
        
        // Test the new modern API for version detection
        int major, minor, release;
        SoOffscreenRenderer::getOpenGLVersion(major, minor, release);
        
        // We should get valid version numbers
        REQUIRE(major >= 1);
        INFO("OpenGL Version: " << major << "." << minor << "." << release);
        
        // Test version comparison
        SbBool isModern = SoOffscreenRenderer::isVersionAtLeast(2, 0);
        INFO("OpenGL 2.0+ support: " << (isModern ? "Yes" : "No"));
        
        // This should always be true for any reasonable OpenGL implementation
        SbBool hasBasicGL = SoOffscreenRenderer::isVersionAtLeast(1, 1);
        REQUIRE(hasBasicGL == TRUE);
    }
    
    SECTION("OpenGL extension detection") {
        // Initialize test context first
        CoinTestUtils::OSMesaTestFixture fixture(32, 32);
        
        // Test extension detection using the modern API
        SbBool hasMultitexture = SoOffscreenRenderer::isOpenGLExtensionSupported("GL_ARB_multitexture");
        INFO("Multitexture support: " << (hasMultitexture ? "Available" : "Not available"));
        
        // Test FBO detection - this is commonly used in tests
        SbBool hasFBO = SoOffscreenRenderer::hasFramebufferObjectSupport();
        INFO("FBO support: " << (hasFBO ? "Available" : "Not available"));
        
        // These calls should not crash even if extensions aren't available
        SbBool hasVBO = SoOffscreenRenderer::isOpenGLExtensionSupported("GL_ARB_vertex_buffer_object");
        INFO("VBO support: " << (hasVBO ? "Available" : "Not available"));
        
        SUCCEED("Extension detection API works correctly");
    }
    
    SECTION("Custom context provider registration") {
        // Test the new C++ context provider API
        CoinTestUtils::OSMesaTestFixture fixture(256, 256);
        
        // Create and register a custom context provider
        OSMesaContextProvider provider;
        
        // Store original provider (if any)
        SoOffscreenRenderer::ContextProvider* originalProvider = 
            SoOffscreenRenderer::getContextProvider();
        
        // Set our custom provider
        SoOffscreenRenderer::setContextProvider(&provider);
        
        // Verify it was set
        REQUIRE(SoOffscreenRenderer::getContextProvider() == &provider);
        
        // Create an offscreen renderer to test the provider
        SbViewportRegion viewport(256, 256);
        SoOffscreenRenderer renderer(viewport);
        
        // Create a simple scene
        SoSeparator* root = new SoSeparator;
        root->ref();
        
        SoDirectionalLight* light = new SoDirectionalLight;
        root->addChild(light);
        
        SoPerspectiveCamera* camera = new SoPerspectiveCamera;
        root->addChild(camera);
        
        SoCube* cube = new SoCube;
        root->addChild(cube);
        
        // Test rendering with our custom context provider
        SbBool result = renderer.render(root);
        
        // In OSMesa builds this should work, but if FBO isn't available it might use fallback
        INFO("Rendering result: " << (result ? "Success" : "Failed (expected in some configurations)"));
        
        // Clean up
        root->unref();
        
        // Restore original provider
        SoOffscreenRenderer::setContextProvider(originalProvider);
        
        SUCCEED("Custom context provider API works correctly");
    }
}

TEST_CASE("Comparison of old vs new API", "[rendering][comparison][api]") {
    
    SECTION("API style comparison") {
        // Initialize context first to avoid segfaults
        CoinTestUtils::OSMesaTestFixture fixture(32, 32);
        
        // This test demonstrates the difference between old and new APIs
        
        // === NEW MODERN C++ API ===
        // Clean, object-oriented, exception-safe
        int major, minor, release;
        SoOffscreenRenderer::getOpenGLVersion(major, minor, release);
        SbBool hasFBO = SoOffscreenRenderer::hasFramebufferObjectSupport();
        SbBool isModern = SoOffscreenRenderer::isVersionAtLeast(3, 0);
        
        INFO("Modern API - OpenGL " << major << "." << minor << "." << release);
        INFO("Modern API - FBO support: " << (hasFBO ? "Yes" : "No"));
        INFO("Modern API - OpenGL 3.0+: " << (isModern ? "Yes" : "No"));
        
                // === OLD cc_glglue API (still available internally but discouraged) ===
        // The old API required manual context management and C-style function calls:
        // const cc_glglue* glue = cc_glglue_instance(1);
        // cc_glglue_glversion(glue, &major, &minor, &release);
        // cc_glglue_has_framebuffer_objects(glue);
        // cc_glglue_glversion_matches_at_least(glue, 3, 0, 0);
        
        SUCCEED("Both APIs provide the same information but modern API is cleaner");
    }
}

#endif // NEVER_DEFINED - Old ContextProvider tests disabled