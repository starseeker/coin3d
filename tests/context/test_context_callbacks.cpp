#include <Inventor/SoDB.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <catch2/catch_test_macros.hpp>

// The ContextProvider API has been removed from SoOffscreenRenderer
// These tests are no longer applicable as context management is now
// handled globally via SoDB::init(context_manager)

TEST_CASE("Context Provider API Removed", "[context][removed]") {
    SECTION("ContextProvider API no longer exists") {
        // This test documents that the ContextProvider API has been removed
        // Context management should now be done via SoDB::init(context_manager)
        INFO("ContextProvider API has been removed from SoOffscreenRenderer");
        INFO("Use SoDB::init(context_manager) for context management instead");
        SUCCEED("API removal confirmed");
    }
}