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
 * @file test_shadows_comprehensive.cpp
 * @brief Comprehensive tests for all Coin3D shadow types and user-facing functionality
 *
 * This module provides comprehensive testing of shadow node creation, shadow mapping,
 * shadow styles, light integration, and rendering validation using OSMesa offscreen rendering.
 */

#include "utils/test_common.h"
#include "utils/osmesa_test_context.h"
#include "utils/scene_graph_test_utils.h"

// Shadow-related node classes
#include <Inventor/annex/FXViz/nodes/SoShadowGroup.h>
#include <Inventor/annex/FXViz/nodes/SoShadowStyle.h>
#include <Inventor/annex/FXViz/nodes/SoShadowCulling.h>
#include <Inventor/annex/FXViz/nodes/SoShadowDirectionalLight.h>
#include <Inventor/annex/FXViz/nodes/SoShadowSpotLight.h>

// Shadow-related elements
#include <Inventor/annex/FXViz/elements/SoShadowStyleElement.h>
#include <Inventor/annex/FXViz/elements/SoGLShadowCullingElement.h>

// Standard scene graph nodes for testing
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoSpotLight.h>

using namespace CoinTestUtils;

// ============================================================================
// Basic Shadow Node Tests
// ============================================================================

TEST_CASE("Shadow Nodes - Creation and Basic Properties", "[shadows][creation][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("SoShadowGroup creation and properties") {
        SoShadowGroup* shadowGroup = new SoShadowGroup;
        shadowGroup->ref();
        
        CHECK(shadowGroup->getTypeId() == SoShadowGroup::getClassTypeId());
        CHECK(shadowGroup->getTypeId().getName() == SbName("ShadowGroup"));
        
        // Test default field values
        CHECK(shadowGroup->isActive.getValue() == TRUE);
        CHECK(shadowGroup->intensity.getValue() == 0.5f);
        CHECK(shadowGroup->precision.getValue() == 0.5f);
        
        shadowGroup->unref();
    }
    
    SECTION("SoShadowStyle creation and properties") {
        SoShadowStyle* shadowStyle = new SoShadowStyle;
        shadowStyle->ref();
        
        CHECK(shadowStyle->getTypeId() == SoShadowStyle::getClassTypeId());
        CHECK(shadowStyle->getTypeId().getName() == SbName("ShadowStyle"));
        
        // Test default field values
        CHECK(shadowStyle->style.getValue() == SoShadowStyle::CASTS_SHADOW_AND_SHADOWED);
        
        shadowStyle->unref();
    }
    
    SECTION("SoShadowCulling creation and properties") {
        SoShadowCulling* shadowCulling = new SoShadowCulling;
        shadowCulling->ref();
        
        CHECK(shadowCulling->getTypeId() == SoShadowCulling::getClassTypeId());
        CHECK(shadowCulling->getTypeId().getName() == SbName("ShadowCulling"));
        
        // Test default field values
        CHECK(shadowCulling->mode.getValue() == SoShadowCulling::AS_IS_CULLING);
        
        shadowCulling->unref();
    }
    
    SECTION("SoShadowDirectionalLight creation and properties") {
        SoShadowDirectionalLight* shadowLight = new SoShadowDirectionalLight;
        shadowLight->ref();
        
        CHECK(shadowLight->getTypeId() == SoShadowDirectionalLight::getClassTypeId());
        CHECK(shadowLight->getTypeId().getName() == SbName("ShadowDirectionalLight"));
        
        // Test inherited light properties
        CHECK(shadowLight->on.getValue() == TRUE);
        CHECK(shadowLight->intensity.getValue() == 1.0f);
        
        // Test shadow-specific properties
        CHECK(shadowLight->maxShadowDistance.getValue() == -1.0f);
        
        shadowLight->unref();
    }
    
    SECTION("SoShadowSpotLight creation and properties") {
        SoShadowSpotLight* shadowSpotLight = new SoShadowSpotLight;
        shadowSpotLight->ref();
        
        CHECK(shadowSpotLight->getTypeId() == SoShadowSpotLight::getClassTypeId());
        CHECK(shadowSpotLight->getTypeId().getName() == SbName("ShadowSpotLight"));
        
        // Test inherited light properties
        CHECK(shadowSpotLight->on.getValue() == TRUE);
        CHECK(shadowSpotLight->intensity.getValue() == 1.0f);
        
        // Test spot light specific properties
        CHECK(shadowSpotLight->cutOffAngle.getValue() >= 0.785f); // approximately π/4 radians
        CHECK(shadowSpotLight->dropOffRate.getValue() == 0.0f);
        
        shadowSpotLight->unref();
    }
}

// ============================================================================
// Shadow Style Tests
// ============================================================================

TEST_CASE("Shadow Style - Style Settings and Behavior", "[shadows][style][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("Shadow style enumeration values") {
        SoShadowStyle* shadowStyle = new SoShadowStyle;
        shadowStyle->ref();
        
        // Test all style values
        shadowStyle->style.setValue(SoShadowStyle::CASTS_SHADOW);
        CHECK(shadowStyle->style.getValue() == SoShadowStyle::CASTS_SHADOW);
        
        shadowStyle->style.setValue(SoShadowStyle::SHADOWED);
        CHECK(shadowStyle->style.getValue() == SoShadowStyle::SHADOWED);
        
        shadowStyle->style.setValue(SoShadowStyle::CASTS_SHADOW_AND_SHADOWED);
        CHECK(shadowStyle->style.getValue() == SoShadowStyle::CASTS_SHADOW_AND_SHADOWED);
        
        shadowStyle->style.setValue(SoShadowStyle::NO_SHADOWING);
        CHECK(shadowStyle->style.getValue() == SoShadowStyle::NO_SHADOWING);
        
        shadowStyle->unref();
    }
    
    SECTION("Shadow style state management") {
        SoShadowStyle* shadowStyle1 = new SoShadowStyle;
        SoShadowStyle* shadowStyle2 = new SoShadowStyle;
        shadowStyle1->ref();
        shadowStyle2->ref();
        
        // Set different styles
        shadowStyle1->style.setValue(SoShadowStyle::CASTS_SHADOW);
        shadowStyle2->style.setValue(SoShadowStyle::SHADOWED);
        
        // Verify independent state
        CHECK(shadowStyle1->style.getValue() != shadowStyle2->style.getValue());
        CHECK(shadowStyle1->style.getValue() == SoShadowStyle::CASTS_SHADOW);
        CHECK(shadowStyle2->style.getValue() == SoShadowStyle::SHADOWED);
        
        shadowStyle1->unref();
        shadowStyle2->unref();
    }
}

// ============================================================================
// Shadow Culling Tests
// ============================================================================

TEST_CASE("Shadow Culling - Culling Mode Settings", "[shadows][culling][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("Shadow culling mode enumeration") {
        SoShadowCulling* shadowCulling = new SoShadowCulling;
        shadowCulling->ref();
        
        // Test all culling modes
        shadowCulling->mode.setValue(SoShadowCulling::AS_IS_CULLING);
        CHECK(shadowCulling->mode.getValue() == SoShadowCulling::AS_IS_CULLING);
        
        shadowCulling->mode.setValue(SoShadowCulling::NO_CULLING);
        CHECK(shadowCulling->mode.getValue() == SoShadowCulling::NO_CULLING);
        
        shadowCulling->unref();
    }
}

// ============================================================================
// Shadow Light Tests
// ============================================================================

TEST_CASE("Shadow Lights - Light Configuration and Properties", "[shadows][lights][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("SoShadowDirectionalLight configuration") {
        SoShadowDirectionalLight* light = new SoShadowDirectionalLight;
        light->ref();
        
        // Test light direction setting
        SbVec3f direction(0.0f, -1.0f, -1.0f);
        light->direction.setValue(direction);
        CHECK(light->direction.getValue() == direction);
        
        // Test light color setting
        SbColor lightColor(1.0f, 0.8f, 0.6f);
        light->color.setValue(lightColor);
        CHECK(light->color.getValue() == lightColor);
        
        // Test shadow distance setting
        light->maxShadowDistance.setValue(100.0f);
        CHECK(light->maxShadowDistance.getValue() == 100.0f);
        
        // Test light intensity
        light->intensity.setValue(0.8f);
        CHECK(light->intensity.getValue() == 0.8f);
        
        light->unref();
    }
    
    SECTION("SoShadowSpotLight configuration") {
        SoShadowSpotLight* spotLight = new SoShadowSpotLight;
        spotLight->ref();
        
        // Test spot light position
        SbVec3f position(10.0f, 10.0f, 10.0f);
        spotLight->location.setValue(position);
        CHECK(spotLight->location.getValue() == position);
        
        // Test spot light direction
        SbVec3f direction(0.0f, -1.0f, 0.0f);
        spotLight->direction.setValue(direction);
        CHECK(spotLight->direction.getValue() == direction);
        
        // Test cut-off angle
        spotLight->cutOffAngle.setValue(1.047198f); // 60 degrees in radians
        CHECK(spotLight->cutOffAngle.getValue() == 1.047198f);
        
        // Test drop-off rate
        spotLight->dropOffRate.setValue(0.5f);
        CHECK(spotLight->dropOffRate.getValue() == 0.5f);
        
        spotLight->unref();
    }
}

// ============================================================================
// Shadow Group Configuration Tests
// ============================================================================

TEST_CASE("Shadow Group - Configuration and Scene Management", "[shadows][group][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("SoShadowGroup intensity and precision") {
        SoShadowGroup* shadowGroup = new SoShadowGroup;
        shadowGroup->ref();
        
        // Test intensity setting
        shadowGroup->intensity.setValue(0.8f);
        CHECK(shadowGroup->intensity.getValue() == 0.8f);
        
        shadowGroup->intensity.setValue(0.2f);
        CHECK(shadowGroup->intensity.getValue() == 0.2f);
        
        // Test precision setting
        shadowGroup->precision.setValue(0.9f);
        CHECK(shadowGroup->precision.getValue() == 0.9f);
        
        shadowGroup->precision.setValue(0.1f);
        CHECK(shadowGroup->precision.getValue() == 0.1f);
        
        shadowGroup->unref();
    }
    
    SECTION("SoShadowGroup activation control") {
        SoShadowGroup* shadowGroup = new SoShadowGroup;
        shadowGroup->ref();
        
        // Test activation control
        shadowGroup->isActive.setValue(FALSE);
        CHECK(shadowGroup->isActive.getValue() == FALSE);
        
        shadowGroup->isActive.setValue(TRUE);
        CHECK(shadowGroup->isActive.getValue() == TRUE);
        
        shadowGroup->unref();
    }
    
    SECTION("SoShadowGroup with child geometry") {
        SoShadowGroup* shadowGroup = new SoShadowGroup;
        shadowGroup->ref();
        
        // Add geometry to shadow group
        SoCube* cube = new SoCube;
        SoSphere* sphere = new SoSphere;
        
        shadowGroup->addChild(cube);
        shadowGroup->addChild(sphere);
        
        CHECK(shadowGroup->getNumChildren() == 2);
        CHECK(shadowGroup->getChild(0) == cube);
        CHECK(shadowGroup->getChild(1) == sphere);
        
        shadowGroup->unref();
    }
}

// ============================================================================
// Shadow Scene Integration Tests
// ============================================================================

TEST_CASE("Shadow Scene Integration - Complete Shadow Scenes", "[shadows][scene][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("Basic shadow scene with directional light") {
        COIN_TEST_WITH_OSMESA_CONTEXT(256, 256) {
            RenderingTestUtils::RenderTestFixture render_fixture(256, 256);
            SoSeparator* scene = StandardTestScenes::createMinimalScene();
            
            // Create shadow group
            SoShadowGroup* shadowGroup = new SoShadowGroup;
            shadowGroup->intensity.setValue(0.7f);
            shadowGroup->precision.setValue(0.8f);
            scene->addChild(shadowGroup);
            
            // Add shadow directional light
            SoShadowDirectionalLight* light = new SoShadowDirectionalLight;
            light->direction.setValue(SbVec3f(0.0f, -1.0f, -1.0f));
            light->intensity.setValue(1.0f);
            shadowGroup->addChild(light);
            
            // Add geometry
            SoCube* cube = new SoCube;
            shadowGroup->addChild(cube);
            
            // Test rendering (should not crash)
            CHECK(render_fixture.renderScene(scene));
            auto analysis = render_fixture.analyzeRenderedPixels();
            CHECK(analysis.total_pixels > 0);
            
            scene->unref();
        }
    }
    
    SECTION("Shadow scene with spot light") {
        COIN_TEST_WITH_OSMESA_CONTEXT(256, 256) {
            RenderingTestUtils::RenderTestFixture render_fixture(256, 256);
            SoSeparator* scene = StandardTestScenes::createMinimalScene();
            
            // Create shadow group
            SoShadowGroup* shadowGroup = new SoShadowGroup;
            scene->addChild(shadowGroup);
            
            // Add shadow spot light
            SoShadowSpotLight* spotLight = new SoShadowSpotLight;
            spotLight->location.setValue(SbVec3f(5.0f, 5.0f, 5.0f));
            spotLight->direction.setValue(SbVec3f(0.0f, -1.0f, 0.0f));
            spotLight->cutOffAngle.setValue(0.785398f); // 45 degrees
            shadowGroup->addChild(spotLight);
            
            // Add geometry
            SoSphere* sphere = new SoSphere;
            shadowGroup->addChild(sphere);
            
            // Test rendering
            CHECK(render_fixture.renderScene(scene));
            auto analysis = render_fixture.analyzeRenderedPixels();
            CHECK(analysis.total_pixels > 0);
            
            scene->unref();
        }
    }
    
    SECTION("Complex shadow scene with multiple objects and styles") {
        COIN_TEST_WITH_OSMESA_CONTEXT(256, 256) {
            RenderingTestUtils::RenderTestFixture render_fixture(256, 256);
            SoSeparator* scene = StandardTestScenes::createMinimalScene();
            
            // Create shadow group
            SoShadowGroup* shadowGroup = new SoShadowGroup;
            shadowGroup->intensity.setValue(0.6f);
            scene->addChild(shadowGroup);
            
            // Add light
            SoShadowDirectionalLight* light = new SoShadowDirectionalLight;
            light->direction.setValue(SbVec3f(1.0f, -1.0f, -1.0f));
            shadowGroup->addChild(light);
            
            // Add object that casts shadows but is not shadowed
            SoSeparator* casterSep = new SoSeparator;
            SoShadowStyle* casterStyle = new SoShadowStyle;
            casterStyle->style.setValue(SoShadowStyle::CASTS_SHADOW);
            casterSep->addChild(casterStyle);
            
            SoTransform* transform1 = new SoTransform;
            transform1->translation.setValue(SbVec3f(-2.0f, 2.0f, 0.0f));
            casterSep->addChild(transform1);
            
            SoCube* cube = new SoCube;
            casterSep->addChild(cube);
            shadowGroup->addChild(casterSep);
            
            // Add object that receives shadows but doesn't cast them
            SoSeparator* receiverSep = new SoSeparator;
            SoShadowStyle* receiverStyle = new SoShadowStyle;
            receiverStyle->style.setValue(SoShadowStyle::SHADOWED);
            receiverSep->addChild(receiverStyle);
            
            SoTransform* transform2 = new SoTransform;
            transform2->translation.setValue(SbVec3f(2.0f, -2.0f, 0.0f));
            receiverSep->addChild(transform2);
            
            SoSphere* sphere = new SoSphere;
            receiverSep->addChild(sphere);
            shadowGroup->addChild(receiverSep);
            
            // Test rendering
            CHECK(render_fixture.renderScene(scene));
            auto analysis = render_fixture.analyzeRenderedPixels();
            CHECK(analysis.total_pixels > 0);
            
            scene->unref();
        }
    }
}

// ============================================================================
// Shadow Performance and Edge Cases
// ============================================================================

TEST_CASE("Shadow Edge Cases - Performance and Special Scenarios", "[shadows][edge][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("Shadow group with no lights") {
        SoShadowGroup* shadowGroup = new SoShadowGroup;
        shadowGroup->ref();
        
        // Add geometry without any lights
        SoCube* cube = new SoCube;
        shadowGroup->addChild(cube);
        
        // Should not crash
        CHECK(shadowGroup->getNumChildren() == 1);
        
        shadowGroup->unref();
    }
    
    SECTION("Shadow group with deactivated shadows") {
        COIN_TEST_WITH_OSMESA_CONTEXT(256, 256) {
            RenderingTestUtils::RenderTestFixture render_fixture(256, 256);
            SoSeparator* scene = StandardTestScenes::createMinimalScene();
            
            // Create deactivated shadow group
            SoShadowGroup* shadowGroup = new SoShadowGroup;
            shadowGroup->isActive.setValue(FALSE);
            scene->addChild(shadowGroup);
            
            // Add light and geometry
            SoShadowDirectionalLight* light = new SoShadowDirectionalLight;
            shadowGroup->addChild(light);
            
            SoCube* cube = new SoCube;
            shadowGroup->addChild(cube);
            
            // Test rendering (shadows should be disabled)
            CHECK(render_fixture.renderScene(scene));
            auto analysis = render_fixture.analyzeRenderedPixels();
            CHECK(analysis.total_pixels > 0);
            
            scene->unref();
        }
    }
    
    SECTION("Very low and high precision settings") {
        SoShadowGroup* shadowGroup = new SoShadowGroup;
        shadowGroup->ref();
        
        // Test extreme precision values
        shadowGroup->precision.setValue(0.0f);
        CHECK(shadowGroup->precision.getValue() == 0.0f);
        
        shadowGroup->precision.setValue(1.0f);
        CHECK(shadowGroup->precision.getValue() == 1.0f);
        
        shadowGroup->unref();
    }
    
    SECTION("Zero and maximum intensity settings") {
        SoShadowGroup* shadowGroup = new SoShadowGroup;
        shadowGroup->ref();
        
        // Test extreme intensity values
        shadowGroup->intensity.setValue(0.0f);
        CHECK(shadowGroup->intensity.getValue() == 0.0f);
        
        shadowGroup->intensity.setValue(1.0f);
        CHECK(shadowGroup->intensity.getValue() == 1.0f);
        
        shadowGroup->unref();
    }
}