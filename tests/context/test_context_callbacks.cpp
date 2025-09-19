#include <Inventor/C/glue/gl.h>
#include <Inventor/SoDB.h>
#include <catch2/catch_test_macros.hpp>

// Mock context implementation for testing callback interface
namespace {

struct MockContext {
    int width, height;
    bool is_current;
    
    MockContext(int w, int h) : width(w), height(h), is_current(false) {}
};

static void* mock_create_offscreen(unsigned int width, unsigned int height) {
    return new MockContext(width, height);
}

static SbBool mock_make_current(void* context) {
    if (!context) return FALSE;
    auto* ctx = static_cast<MockContext*>(context);
    ctx->is_current = true;
    return TRUE;
}

static void mock_reinstate_previous(void* context) {
    if (context) {
        auto* ctx = static_cast<MockContext*>(context);
        ctx->is_current = false;
    }
}

static void mock_destruct(void* context) {
    if (context) {
        delete static_cast<MockContext*>(context);
    }
}

class MockCallbackManager {
private:
    cc_glglue_offscreen_cb_functions callbacks;
    
public:
    MockCallbackManager() {
        callbacks.create_offscreen = mock_create_offscreen;
        callbacks.make_current = mock_make_current;
        callbacks.reinstate_previous = mock_reinstate_previous;
        callbacks.destruct = mock_destruct;
        
        cc_glglue_context_set_offscreen_cb_functions(&callbacks);
    }
    
    ~MockCallbackManager() {
        cc_glglue_context_set_offscreen_cb_functions(nullptr);
    }
};

} // anonymous namespace

TEST_CASE("Context Callback Interface", "[context][callbacks]") {
    
    SECTION("Mock context callbacks work correctly") {
        MockCallbackManager manager;
        
        // Test context creation
        void* ctx = cc_glglue_context_create_offscreen(100, 100);
        REQUIRE(ctx != nullptr);
        
        auto* mock_ctx = static_cast<MockContext*>(ctx);
        REQUIRE(mock_ctx->width == 100);
        REQUIRE(mock_ctx->height == 100);
        REQUIRE(mock_ctx->is_current == false);
        
        // Test make current
        SbBool result = cc_glglue_context_make_current(ctx);
        REQUIRE(result == TRUE);
        REQUIRE(mock_ctx->is_current == true);
        
        // Test reinstate previous
        cc_glglue_context_reinstate_previous(ctx);
        REQUIRE(mock_ctx->is_current == false);
        
        // Test destruction
        cc_glglue_context_destruct(ctx);
        // Context is now destroyed, should not access mock_ctx
    }
    
    SECTION("Callback lifecycle management") {
        {
            MockCallbackManager manager;
            void* ctx = cc_glglue_context_create_offscreen(50, 50);
            REQUIRE(ctx != nullptr);
            cc_glglue_context_destruct(ctx);
        }
        
        // After manager destruction, callbacks should be reset
        void* ctx = cc_glglue_context_create_offscreen(50, 50);
        REQUIRE(ctx == nullptr); // Should fail without callbacks
    }
    
    SECTION("Null pointer handling") {
        MockCallbackManager manager;
        
        // Test functions with null context
        SbBool result = cc_glglue_context_make_current(nullptr);
        REQUIRE(result == FALSE);
        
        // These should not crash
        cc_glglue_context_reinstate_previous(nullptr);
        cc_glglue_context_destruct(nullptr);
    }
    
    SECTION("Multiple callback installations") {
        MockCallbackManager manager1;
        
        void* ctx1 = cc_glglue_context_create_offscreen(64, 64);
        REQUIRE(ctx1 != nullptr);
        
        {
            MockCallbackManager manager2; // This should replace manager1's callbacks
            
            void* ctx2 = cc_glglue_context_create_offscreen(128, 128);
            REQUIRE(ctx2 != nullptr);
            
            cc_glglue_context_destruct(ctx2);
        } // manager2 destroyed, callbacks should be reset
        
        // Try to use the first context - this might fail if callbacks are gone
        // This demonstrates the importance of callback lifetime management
        cc_glglue_context_destruct(ctx1);
    }
}

TEST_CASE("Context Error Conditions", "[context][error]") {
    
    SECTION("No callbacks installed") {
        // Ensure no callbacks
        cc_glglue_context_set_offscreen_cb_functions(nullptr);
        
        void* ctx = cc_glglue_context_create_offscreen(128, 128);
        REQUIRE(ctx == nullptr);
        
        SbBool result = cc_glglue_context_make_current(nullptr);
        REQUIRE(result == FALSE);
    }
    
    SECTION("Partial callback structure") {
        cc_glglue_offscreen_cb_functions partial_callbacks = {0};
        partial_callbacks.create_offscreen = mock_create_offscreen;
        // Leave other callbacks as null
        
        cc_glglue_context_set_offscreen_cb_functions(&partial_callbacks);
        
        void* ctx = cc_glglue_context_create_offscreen(64, 64);
        REQUIRE(ctx != nullptr);
        
        // make_current should fail gracefully with null callback
        SbBool result = cc_glglue_context_make_current(ctx);
        REQUIRE(result == FALSE);
        
        // Clean up - this might show error message due to null destruct callback
        cc_glglue_context_destruct(ctx);
        
        // Reset callbacks
        cc_glglue_context_set_offscreen_cb_functions(nullptr);
    }
}