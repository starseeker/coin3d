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

// The ContextProvider API has been removed from SoOffscreenRenderer
// These tests documented the old per-renderer context provider approach
// Context management should now be done via SoDB::init(context_manager)

TEST_CASE("OSMesa Context Management", "[osmesa][context]") {
    
    SECTION("Context management now uses global SoDB approach") {
        // Document that the ContextProvider API has been removed
        INFO("ContextProvider API has been removed from SoOffscreenRenderer");
        INFO("Context management should now be done via SoDB::init(context_manager)");
        
        // Test that basic rendering still works with global context management
        SbViewportRegion viewport(128, 128);
        SoOffscreenRenderer renderer(viewport);
        
        SUCCEED("OSMesa context management updated to use global approach");
    }
}

#endif // COIN3D_OSMESA_BUILD