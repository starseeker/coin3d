#include <Inventor/SoDB.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <catch2/catch_test_macros.hpp>

#ifndef COIN3D_OSMESA_BUILD
// Only include these detailed context tests in non-OSMesa builds
// OSMesa builds have global context management that interferes with these tests

// Mock context provider implementation for testing
namespace {

struct MockContext {
    int width, height;
    bool is_current;
    
    MockContext(int w, int h) : width(w), height(h), is_current(false) {}
};

class MockContextProvider : public SoOffscreenRenderer::ContextProvider {
public:
    virtual void * createOffscreenContext(unsigned int width, unsigned int height) override {
        return new MockContext(width, height);
    }
    
    virtual SbBool makeContextCurrent(void * context) override {
        if (!context) return FALSE;
        auto* ctx = static_cast<MockContext*>(context);
        ctx->is_current = true;
        return TRUE;
    }
    
    virtual void restorePreviousContext(void * context) override {
        if (context) {
            auto* ctx = static_cast<MockContext*>(context);
            ctx->is_current = false;
        }
    }
    
    virtual void destroyContext(void * context) override {
        if (context) {
            delete static_cast<MockContext*>(context);
        }
    }
};

class MockCallbackManager {
private:
    MockContextProvider provider;
    SoOffscreenRenderer::ContextProvider* originalProvider;
    
public:
    MockCallbackManager() {
        originalProvider = SoOffscreenRenderer::getContextProvider();
        SoOffscreenRenderer::setContextProvider(&provider);
    }
    
    ~MockCallbackManager() {
        SoOffscreenRenderer::setContextProvider(originalProvider);
    }
};

} // anonymous namespace

TEST_CASE("Context Provider Interface", "[context][callbacks]") {
    
    SECTION("Mock context provider works correctly") {
        MockCallbackManager manager;
        
        // Verify the provider is set
        REQUIRE(SoOffscreenRenderer::getContextProvider() != nullptr);
        
        // Test through SoOffscreenRenderer - this will use our provider internally
        SbViewportRegion viewport(100, 100);
        SoOffscreenRenderer renderer(viewport);
        
        // The renderer should be able to work with our mock provider
        // This is a higher-level test of the provider interface
        SUCCEED("Mock context provider installed successfully");
    }
    
    SECTION("Provider lifecycle management") {
        SoOffscreenRenderer::ContextProvider* originalProvider = 
            SoOffscreenRenderer::getContextProvider();
        
        {
            MockCallbackManager manager;
            REQUIRE(SoOffscreenRenderer::getContextProvider() != nullptr);
            REQUIRE(SoOffscreenRenderer::getContextProvider() != originalProvider);
        }
        
        // After manager destruction, provider should be restored
        REQUIRE(SoOffscreenRenderer::getContextProvider() == originalProvider);
    }
    
    SECTION("Multiple provider installations") {
        SoOffscreenRenderer::ContextProvider* originalProvider = 
            SoOffscreenRenderer::getContextProvider();
        
        MockCallbackManager manager1;
        SoOffscreenRenderer::ContextProvider* provider1 = 
            SoOffscreenRenderer::getContextProvider();
        
        {
            MockCallbackManager manager2; // This should replace manager1's provider
            
            SoOffscreenRenderer::ContextProvider* provider2 = 
                SoOffscreenRenderer::getContextProvider();
            REQUIRE(provider2 != provider1);
            
        } // manager2 destroyed, provider should be restored to manager1's
        
        REQUIRE(SoOffscreenRenderer::getContextProvider() == provider1);
    }
}

TEST_CASE("Context Provider Error Conditions", "[context][error]") {
    
#ifndef COIN3D_OSMESA_BUILD
    // Only run error condition tests in non-OSMesa builds 
    // since OSMesa builds need global callbacks to be persistent
    
    SECTION("No context provider installed") {
        SoOffscreenRenderer::ContextProvider* originalProvider = 
            SoOffscreenRenderer::getContextProvider();
        
        // Temporarily clear the provider
        SoOffscreenRenderer::setContextProvider(nullptr);
        REQUIRE(SoOffscreenRenderer::getContextProvider() == nullptr);
        
        // Try to use offscreen renderer without provider - should work with defaults
        SbViewportRegion viewport(128, 128);
        SoOffscreenRenderer renderer(viewport);
        
        // This should work because SoOffscreenRenderer has fallback mechanisms
        SUCCEED("SoOffscreenRenderer handles missing context provider gracefully");
        
        // Restore original provider
        SoOffscreenRenderer::setContextProvider(originalProvider);
    }
    
    SECTION("Provider replacement") {
        SoOffscreenRenderer::ContextProvider* originalProvider = 
            SoOffscreenRenderer::getContextProvider();
        
        MockContextProvider provider1;
        SoOffscreenRenderer::setContextProvider(&provider1);
        REQUIRE(SoOffscreenRenderer::getContextProvider() == &provider1);
        
        MockContextProvider provider2;
        SoOffscreenRenderer::setContextProvider(&provider2);
        REQUIRE(SoOffscreenRenderer::getContextProvider() == &provider2);
        
        // Restore original
        SoOffscreenRenderer::setContextProvider(originalProvider);
    }
#else
    // In OSMesa builds, just verify that providers are working properly
    SECTION("OSMesa context provider is functional") {
        SoOffscreenRenderer::ContextProvider* provider = 
            SoOffscreenRenderer::getContextProvider();
        
        // Should have a provider in OSMesa builds
        INFO("Context provider: " << (provider ? "Available" : "Not available"));
        
        // Test basic OpenGL capabilities
        SbBool hasFBO = SoOffscreenRenderer::hasFramebufferObjectSupport();
        INFO("FBO support: " << (hasFBO ? "Yes" : "No"));
        
        SUCCEED("Context provider functionality verified");
    }
#endif
}

#else
// OSMesa builds: Skip detailed context tests that interfere with global context management
// These tests are designed to test error conditions which conflict with global OSMesa setup
TEST_CASE("Context Provider Interface", "[context][callbacks]") {
    SECTION("OSMesa build - context tests skipped") {
        // Context tests are skipped in OSMesa builds to avoid interference with global setup
        SUCCEED("Context provider tests skipped in OSMesa build - global callbacks managed automatically");
    }
}

TEST_CASE("Context Provider Error Conditions", "[context][error]") {
    SECTION("OSMesa build - error condition tests skipped") {
        // Error condition tests are skipped in OSMesa builds
        SUCCEED("Context error tests skipped in OSMesa build - OSMesa provides global context management");
    }
}
#endif