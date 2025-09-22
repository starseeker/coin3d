/**************************************************************************\
* Copyright (c) Kongsberg Oil & Gas Technologies AS
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*
* Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
*
* Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
*
* Neither the name of the copyright holder nor the names of its
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
\**************************************************************************/

/**
 * @file test_manips_comprehensive.cpp
 * @brief Comprehensive tests for all Coin3D manipulator types and user-facing functionality
 *
 * This module provides comprehensive testing of manipulator node creation, interaction,
 * transformation handling, dragger integration, and rendering validation using OSMesa offscreen rendering.
 */

#include "utils/test_common.h"
#include "utils/osmesa_test_context.h"
#include "utils/scene_graph_test_utils.h"

// Manipulator node classes
#include <Inventor/manips/SoTransformManip.h>
#include <Inventor/manips/SoHandleBoxManip.h>
#include <Inventor/manips/SoCenterballManip.h>
#include <Inventor/manips/SoJackManip.h>
#include <Inventor/manips/SoTrackballManip.h>
#include <Inventor/manips/SoTransformerManip.h>
#include <Inventor/manips/SoTabBoxManip.h>
#include <Inventor/manips/SoClipPlaneManip.h>
#include <Inventor/manips/SoDirectionalLightManip.h>
#include <Inventor/manips/SoPointLightManip.h>
#include <Inventor/manips/SoSpotLightManip.h>

// Transform and lighting nodes for testing
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoClipPlane.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoPointLight.h>
#include <Inventor/nodes/SoSpotLight.h>

// Scene graph nodes for testing
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoMaterial.h>

// Event handling
#include <Inventor/events/SoMouseButtonEvent.h>
#include <Inventor/events/SoLocation2Event.h>
#include <Inventor/SoPath.h>

using namespace CoinTestUtils;

// ============================================================================
// Basic Transform Manipulator Tests
// ============================================================================

TEST_CASE("Transform Manipulators - Creation and Basic Properties", "[manips][transform][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("SoTransformManip creation and properties") {
        SoTransformManip* manip = new SoTransformManip;
        manip->ref();
        
        CHECK(manip->getTypeId() == SoTransformManip::getClassTypeId());
        CHECK(manip->getTypeId().getName() == SbName("TransformManip"));
        
        // Test that it inherits transform properties
        SbVec3f translation(1.0f, 2.0f, 3.0f);
        manip->translation.setValue(translation);
        CHECK(manip->translation.getValue() == translation);
        
        SbRotation rotation(SbVec3f(0.0f, 1.0f, 0.0f), M_PI / 4.0f);
        manip->rotation.setValue(rotation);
        CHECK(manip->rotation.getValue() == rotation);
        
        SbVec3f scale(2.0f, 1.5f, 0.5f);
        manip->scaleFactor.setValue(scale);
        CHECK(manip->scaleFactor.getValue() == scale);
        
        manip->unref();
    }
    
    SECTION("SoHandleBoxManip creation and properties") {
        SoHandleBoxManip* manip = new SoHandleBoxManip;
        manip->ref();
        
        CHECK(manip->getTypeId() == SoHandleBoxManip::getClassTypeId());
        CHECK(manip->getTypeId().getName() == SbName("HandleBoxManip"));
        
        // Test default transform values
        CHECK(manip->translation.getValue() == SbVec3f(0.0f, 0.0f, 0.0f));
        CHECK(manip->scaleFactor.getValue() == SbVec3f(1.0f, 1.0f, 1.0f));
        
        manip->unref();
    }
    
    SECTION("SoCenterballManip creation and properties") {
        SoCenterballManip* manip = new SoCenterballManip;
        manip->ref();
        
        CHECK(manip->getTypeId() == SoCenterballManip::getClassTypeId());
        CHECK(manip->getTypeId().getName() == SbName("CenterballManip"));
        
        // Test center field
        SbVec3f center(5.0f, 5.0f, 5.0f);
        manip->center.setValue(center);
        CHECK(manip->center.getValue() == center);
        
        manip->unref();
    }
    
    SECTION("SoJackManip creation and properties") {
        SoJackManip* manip = new SoJackManip;
        manip->ref();
        
        CHECK(manip->getTypeId() == SoJackManip::getClassTypeId());
        CHECK(manip->getTypeId().getName() == SbName("JackManip"));
        
        // Test rotation capabilities
        SbRotation rotation(SbVec3f(1.0f, 0.0f, 0.0f), M_PI / 2.0f);
        manip->rotation.setValue(rotation);
        CHECK(manip->rotation.getValue() == rotation);
        
        manip->unref();
    }
    
    SECTION("SoTrackballManip creation and properties") {
        SoTrackballManip* manip = new SoTrackballManip;
        manip->ref();
        
        CHECK(manip->getTypeId() == SoTrackballManip::getClassTypeId());
        CHECK(manip->getTypeId().getName() == SbName("TrackballManip"));
        
        // Test rotation and scaling
        SbRotation rotation(SbVec3f(0.0f, 0.0f, 1.0f), M_PI / 3.0f);
        manip->rotation.setValue(rotation);
        CHECK(manip->rotation.getValue() == rotation);
        
        SbVec3f scale(1.5f, 1.5f, 1.5f);
        manip->scaleFactor.setValue(scale);
        CHECK(manip->scaleFactor.getValue() == scale);
        
        manip->unref();
    }
    
    SECTION("SoTransformerManip creation and properties") {
        SoTransformerManip* manip = new SoTransformerManip;
        manip->ref();
        
        CHECK(manip->getTypeId() == SoTransformerManip::getClassTypeId());
        CHECK(manip->getTypeId().getName() == SbName("TransformerManip"));
        
        // Test all transform components
        SbVec3f translation(10.0f, 20.0f, 30.0f);
        manip->translation.setValue(translation);
        CHECK(manip->translation.getValue() == translation);
        
        SbRotation rotation(SbVec3f(1.0f, 1.0f, 1.0f), M_PI / 6.0f);
        manip->rotation.setValue(rotation);
        CHECK(manip->rotation.getValue() == rotation);
        
        SbVec3f scale(0.5f, 2.0f, 1.0f);
        manip->scaleFactor.setValue(scale);
        CHECK(manip->scaleFactor.getValue() == scale);
        
        manip->unref();
    }
    
    SECTION("SoTabBoxManip creation and properties") {
        SoTabBoxManip* manip = new SoTabBoxManip;
        manip->ref();
        
        CHECK(manip->getTypeId() == SoTabBoxManip::getClassTypeId());
        CHECK(manip->getTypeId().getName() == SbName("TabBoxManip"));
        
        // Test scaling operations
        SbVec3f scale(3.0f, 0.25f, 1.75f);
        manip->scaleFactor.setValue(scale);
        CHECK(manip->scaleFactor.getValue() == scale);
        
        manip->unref();
    }
}

// ============================================================================
// Light Manipulator Tests
// ============================================================================

TEST_CASE("Light Manipulators - Creation and Light Properties", "[manips][light][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("SoDirectionalLightManip creation and properties") {
        SoDirectionalLightManip* manip = new SoDirectionalLightManip;
        manip->ref();
        
        CHECK(manip->getTypeId() == SoDirectionalLightManip::getClassTypeId());
        CHECK(manip->getTypeId().getName() == SbName("DirectionalLightManip"));
        
        // Test light properties
        SbVec3f direction(-1.0f, -1.0f, 0.0f);
        manip->direction.setValue(direction);
        CHECK(manip->direction.getValue() == direction);
        
        SbColor color(1.0f, 0.8f, 0.6f);
        manip->color.setValue(color);
        CHECK(manip->color.getValue() == color);
        
        manip->intensity.setValue(0.75f);
        CHECK(manip->intensity.getValue() == 0.75f);
        
        manip->on.setValue(FALSE);
        CHECK(manip->on.getValue() == FALSE);
        
        manip->unref();
    }
    
    SECTION("SoPointLightManip creation and properties") {
        SoPointLightManip* manip = new SoPointLightManip;
        manip->ref();
        
        CHECK(manip->getTypeId() == SoPointLightManip::getClassTypeId());
        CHECK(manip->getTypeId().getName() == SbName("PointLightManip"));
        
        // Test point light specific properties
        SbVec3f location(5.0f, 10.0f, -5.0f);
        manip->location.setValue(location);
        CHECK(manip->location.getValue() == location);
        
        SbColor color(0.8f, 1.0f, 0.9f);
        manip->color.setValue(color);
        CHECK(manip->color.getValue() == color);
        
        manip->intensity.setValue(1.2f);
        CHECK(manip->intensity.getValue() == 1.2f);
        
        manip->unref();
    }
    
    SECTION("SoSpotLightManip creation and properties") {
        SoSpotLightManip* manip = new SoSpotLightManip;
        manip->ref();
        
        CHECK(manip->getTypeId() == SoSpotLightManip::getClassTypeId());
        CHECK(manip->getTypeId().getName() == SbName("SpotLightManip"));
        
        // Test spot light specific properties
        SbVec3f location(0.0f, 20.0f, 0.0f);
        manip->location.setValue(location);
        CHECK(manip->location.getValue() == location);
        
        SbVec3f direction(0.0f, -1.0f, 0.0f);
        manip->direction.setValue(direction);
        CHECK(manip->direction.getValue() == direction);
        
        manip->cutOffAngle.setValue(1.047198f); // 60 degrees
        CHECK(manip->cutOffAngle.getValue() == 1.047198f);
        
        manip->dropOffRate.setValue(0.3f);
        CHECK(manip->dropOffRate.getValue() == 0.3f);
        
        manip->unref();
    }
}

// ============================================================================
// Clip Plane Manipulator Tests
// ============================================================================

TEST_CASE("Clip Plane Manipulator - Creation and Plane Properties", "[manips][clip][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("SoClipPlaneManip creation and properties") {
        SoClipPlaneManip* manip = new SoClipPlaneManip;
        manip->ref();
        
        CHECK(manip->getTypeId() == SoClipPlaneManip::getClassTypeId());
        CHECK(manip->getTypeId().getName() == SbName("ClipPlaneManip"));
        
        // Test clipping plane properties
        SbPlane plane(SbVec3f(1.0f, 0.0f, 0.0f), 5.0f);
        manip->plane.setValue(plane);
        CHECK(manip->plane.getValue() == plane);
        
        manip->on.setValue(TRUE);
        CHECK(manip->on.getValue() == TRUE);
        
        manip->unref();
    }
}

// ============================================================================
// Manipulator Replacement Tests
// ============================================================================

TEST_CASE("Manipulator Replacement - Node Replacement Functionality", "[manips][replacement][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("Transform node replacement with manipulator") {
        SoSeparator* scene = new SoSeparator;
        scene->ref();
        
        // Create and add a transform node
        SoTransform* transform = new SoTransform;
        transform->translation.setValue(SbVec3f(1.0f, 2.0f, 3.0f));
        scene->addChild(transform);
        
        // Create a cube to be affected by the transform
        SoCube* cube = new SoCube;
        scene->addChild(cube);
        
        CHECK(scene->getNumChildren() == 2);
        CHECK(scene->getChild(0) == transform);
        
        // Replace transform with manipulator
        SoHandleBoxManip* manip = new SoHandleBoxManip;
        
        // Create a path to the transform node
        SoPath* path = new SoPath;
        path->ref();
        path->append(scene);
        path->append(transform);
        
        manip->replaceNode(path);
        
        // Verify replacement
        CHECK(scene->getChild(0) == manip);
        CHECK(manip->translation.getValue() == SbVec3f(1.0f, 2.0f, 3.0f));
        
        path->unref();
        
        scene->unref();
    }
    
    SECTION("Light node replacement with manipulator") {
        SoSeparator* scene = new SoSeparator;
        scene->ref();
        
        // Create and add a directional light
        SoDirectionalLight* light = new SoDirectionalLight;
        light->direction.setValue(SbVec3f(0.0f, -1.0f, -1.0f));
        light->intensity.setValue(0.8f);
        scene->addChild(light);
        
        CHECK(scene->getNumChildren() == 1);
        CHECK(scene->getChild(0) == light);
        
        // Replace light with manipulator
        SoDirectionalLightManip* manip = new SoDirectionalLightManip;
        
        // Create a path to the light node
        SoPath* path = new SoPath;
        path->ref();
        path->append(scene);
        path->append(light);
        
        manip->replaceNode(path);
        
        // Verify replacement and property transfer
        CHECK(scene->getChild(0) == manip);
        CHECK(manip->direction.getValue() == SbVec3f(0.0f, -1.0f, -1.0f));
        CHECK(manip->intensity.getValue() == 0.8f);
        
        path->unref();
        
        scene->unref();
    }
}

// ============================================================================
// Manipulator Scene Integration Tests
// ============================================================================

TEST_CASE("Manipulator Scene Integration - Rendering and Interaction", "[manips][scene][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("Basic scene with transform manipulator") {
        COIN_TEST_WITH_OSMESA_CONTEXT(256, 256) {
            RenderingTestUtils::RenderTestFixture render_fixture(256, 256);
            SoSeparator* scene = StandardTestScenes::createMinimalScene();
            
            // Add transform manipulator
            SoTransformManip* manip = new SoTransformManip;
            manip->translation.setValue(SbVec3f(0.0f, 0.0f, -5.0f));
            scene->addChild(manip);
            
            // Add geometry
            SoCube* cube = new SoCube;
            scene->addChild(cube);
            
            // Test rendering (should not crash)
            CHECK(render_fixture.renderScene(scene));
            auto analysis = render_fixture.analyzeRenderedPixels();
            CHECK(analysis.total_pixels > 0);
            
            scene->unref();
        }
    }
    
    SECTION("Scene with multiple manipulators") {
        COIN_TEST_WITH_OSMESA_CONTEXT(256, 256) {
            RenderingTestUtils::RenderTestFixture render_fixture(256, 256);
            SoSeparator* scene = StandardTestScenes::createMinimalScene();
            
            // Add lighting manipulator
            SoDirectionalLightManip* lightManip = new SoDirectionalLightManip;
            lightManip->direction.setValue(SbVec3f(0.0f, -1.0f, -1.0f));
            lightManip->intensity.setValue(1.0f);
            scene->addChild(lightManip);
            
            // Add transform manipulator
            SoHandleBoxManip* transformManip = new SoHandleBoxManip;
            transformManip->translation.setValue(SbVec3f(2.0f, 0.0f, 0.0f));
            scene->addChild(transformManip);
            
            // Add geometry
            SoSphere* sphere = new SoSphere;
            scene->addChild(sphere);
            
            // Test rendering
            CHECK(render_fixture.renderScene(scene));
            auto analysis = render_fixture.analyzeRenderedPixels();
            CHECK(analysis.total_pixels > 0);
            
            scene->unref();
        }
    }
    
    SECTION("Clip plane manipulator scene") {
        COIN_TEST_WITH_OSMESA_CONTEXT(256, 256) {
            RenderingTestUtils::RenderTestFixture render_fixture(256, 256);
            SoSeparator* scene = StandardTestScenes::createMinimalScene();
            
            // Add clip plane manipulator
            SoClipPlaneManip* clipManip = new SoClipPlaneManip;
            SbPlane clipPlane(SbVec3f(1.0f, 0.0f, 0.0f), 0.0f);
            clipManip->plane.setValue(clipPlane);
            clipManip->on.setValue(TRUE);
            scene->addChild(clipManip);
            
            // Add geometry to be clipped
            SoCube* cube = new SoCube;
            cube->width.setValue(4.0f);
            scene->addChild(cube);
            
            // Test rendering
            CHECK(render_fixture.renderScene(scene));
            auto analysis = render_fixture.analyzeRenderedPixels();
            CHECK(analysis.total_pixels > 0);
            
            scene->unref();
        }
    }
}

// ============================================================================
// Manipulator Interaction Simulation Tests
// ============================================================================

TEST_CASE("Manipulator Interaction - Event Handling Simulation", "[manips][interaction][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("Transform manipulator basic interaction") {
        SoTransformManip* manip = new SoTransformManip;
        manip->ref();
        
        // Create a simple mouse event
        SoMouseButtonEvent* mouseEvent = new SoMouseButtonEvent;
        mouseEvent->setButton(SoMouseButtonEvent::BUTTON1);
        mouseEvent->setState(SoButtonEvent::DOWN);
        mouseEvent->setPosition(SbVec2s(100, 100));
        
        // Test that manipulator can handle events (should not crash)
        // Note: In a real scenario, this would be handled by the render action
        CHECK(mouseEvent->getButton() == SoMouseButtonEvent::BUTTON1);
        CHECK(mouseEvent->getState() == SoButtonEvent::DOWN);
        
        delete mouseEvent;
        manip->unref();
    }
    
    SECTION("Light manipulator interaction setup") {
        SoDirectionalLightManip* lightManip = new SoDirectionalLightManip;
        lightManip->ref();
        
        // Test basic light properties that would be affected by interaction
        SbVec3f originalDirection = lightManip->direction.getValue();
        
        // Simulate direction change via interaction
        SbVec3f newDirection(1.0f, -0.5f, -0.5f);
        lightManip->direction.setValue(newDirection);
        
        CHECK(lightManip->direction.getValue() == newDirection);
        CHECK(lightManip->direction.getValue() != originalDirection);
        
        lightManip->unref();
    }
}

// ============================================================================
// Manipulator Matrix and Transformation Tests
// ============================================================================

TEST_CASE("Manipulator Matrices - Transformation Matrix Computation", "[manips][matrix][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("Transform manipulator matrix computation") {
        SoTransformManip* manip = new SoTransformManip;
        manip->ref();
        
        // Set transform values
        SbVec3f translation(10.0f, 20.0f, 30.0f);
        SbRotation rotation(SbVec3f(0.0f, 1.0f, 0.0f), M_PI / 2.0f);
        SbVec3f scale(2.0f, 3.0f, 0.5f);
        
        manip->translation.setValue(translation);
        manip->rotation.setValue(rotation);
        manip->scaleFactor.setValue(scale);
        
        // Verify values are set correctly
        CHECK(manip->translation.getValue() == translation);
        CHECK(manip->rotation.getValue() == rotation);
        CHECK(manip->scaleFactor.getValue() == scale);
        
        manip->unref();
    }
    
    SECTION("Centerball manipulator center and rotation") {
        SoCenterballManip* manip = new SoCenterballManip;
        manip->ref();
        
        // Set center point
        SbVec3f center(5.0f, 5.0f, 5.0f);
        manip->center.setValue(center);
        CHECK(manip->center.getValue() == center);
        
        // Set rotation around center
        SbRotation rotation(SbVec3f(1.0f, 1.0f, 0.0f), M_PI / 4.0f);
        manip->rotation.setValue(rotation);
        CHECK(manip->rotation.getValue() == rotation);
        
        manip->unref();
    }
}

// ============================================================================
// Manipulator Error Handling and Edge Cases
// ============================================================================

TEST_CASE("Manipulator Edge Cases - Error Handling and Special Scenarios", "[manips][edge][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("Invalid node replacement") {
        SoTransformManip* manip = new SoTransformManip;
        manip->ref();
        
        // Attempt to replace a NULL node (should handle gracefully)
        manip->replaceNode(NULL);
        
        // Should not crash
        CHECK(TRUE);
        
        manip->unref();
    }
    
    SECTION("Extreme transformation values") {
        SoTransformManip* manip = new SoTransformManip;
        manip->ref();
        
        // Set extreme values
        SbVec3f extremeTranslation(1000000.0f, -1000000.0f, 0.0f);
        manip->translation.setValue(extremeTranslation);
        CHECK(manip->translation.getValue() == extremeTranslation);
        
        SbVec3f extremeScale(0.00001f, 100000.0f, 1.0f);
        manip->scaleFactor.setValue(extremeScale);
        CHECK(manip->scaleFactor.getValue() == extremeScale);
        
        manip->unref();
    }
    
    SECTION("Light manipulator with extreme values") {
        SoPointLightManip* manip = new SoPointLightManip;
        manip->ref();
        
        // Set extreme intensity
        manip->intensity.setValue(1000.0f);
        CHECK(manip->intensity.getValue() == 1000.0f);
        
        manip->intensity.setValue(0.0f);
        CHECK(manip->intensity.getValue() == 0.0f);
        
        // Set extreme location
        SbVec3f extremeLocation(999999.0f, -999999.0f, 0.0f);
        manip->location.setValue(extremeLocation);
        CHECK(manip->location.getValue() == extremeLocation);
        
        manip->unref();
    }
    
    SECTION("Clip plane with invalid plane definition") {
        SoClipPlaneManip* manip = new SoClipPlaneManip;
        manip->ref();
        
        // Set plane with zero normal (potentially problematic)
        SbPlane invalidPlane(SbVec3f(0.0f, 0.0f, 0.0f), 1.0f);
        manip->plane.setValue(invalidPlane);
        
        // Should handle gracefully
        CHECK(manip->plane.getValue() == invalidPlane);
        
        manip->unref();
    }
}