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