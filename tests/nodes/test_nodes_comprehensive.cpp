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
 * @file test_nodes_comprehensive.cpp
 * @brief Comprehensive tests for all Coin3D node types and user-facing functionality
 *
 * This module provides comprehensive testing of node creation, field manipulation,
 * scene graph integration, and rendering validation using OSMesa offscreen rendering.
 */

#include "utils/test_common.h"
#include "utils/osmesa_test_context.h"
#include "utils/scene_graph_test_utils.h"

// Core nodes
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/nodes/SoSwitch.h>
#include <Inventor/nodes/SoLevelOfDetail.h>

// Geometry nodes
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <Inventor/nodes/SoFaceSet.h>
#include <Inventor/nodes/SoLineSet.h>
#include <Inventor/nodes/SoPointSet.h>

// Property nodes
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/nodes/SoComplexity.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/nodes/SoLightModel.h>

// Camera nodes
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoOrthographicCamera.h>

// Light nodes
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoPointLight.h>
#include <Inventor/nodes/SoSpotLight.h>

// Coordinate and normal nodes
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoNormal.h>
#include <Inventor/nodes/SoTextureCoordinate2.h>

using namespace CoinTestUtils;

// ============================================================================
// Core Node Group Tests
// ============================================================================

TEST_CASE("Core Nodes - Group and Organization", "[nodes][core][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("SoSeparator functionality") {
        SoSeparator* sep = new SoSeparator;
        sep->ref();
        
        // Test basic properties
        CHECK(sep->getTypeId() == SoSeparator::getClassTypeId());
        CHECK(sep->getNumChildren() == 0);
        
        // Test child management
        SoCube* cube = new SoCube;
        sep->addChild(cube);
        CHECK(sep->getNumChildren() == 1);
        CHECK(sep->getChild(0) == cube);
        
        // Test state isolation
        CHECK(sep->boundingBoxCaching.getValue() == SoSeparator::AUTO);
        CHECK(sep->renderCaching.getValue() == SoSeparator::AUTO);
        CHECK(sep->pickCulling.getValue() == SoSeparator::AUTO);
        
        sep->unref();
    }
    
    SECTION("SoGroup functionality") {
        SoGroup* group = new SoGroup;
        group->ref();
        
        CHECK(group->getTypeId() == SoGroup::getClassTypeId());
        
        // Test multiple children
        SoCube* cube = new SoCube;
        SoSphere* sphere = new SoSphere;
        SoCylinder* cylinder = new SoCylinder;
        
        group->addChild(cube);
        group->addChild(sphere);
        group->addChild(cylinder);
        
        CHECK(group->getNumChildren() == 3);
        
        // Test child removal
        group->removeChild(1); // Remove sphere
        CHECK(group->getNumChildren() == 2);
        CHECK(group->getChild(1) == cylinder);
        
        group->unref();
    }
    
    SECTION("SoSwitch functionality") {
        SoSwitch* switch_node = new SoSwitch;
        switch_node->ref();
        
        // Add multiple children
        for (int i = 0; i < 5; ++i) {
            SoCube* cube = new SoCube;
            switch_node->addChild(cube);
        }
        
        // Test switching behavior
        CHECK(switch_node->whichChild.getValue() == SO_SWITCH_NONE);
        
        switch_node->whichChild.setValue(2);
        CHECK(switch_node->whichChild.getValue() == 2);
        
        switch_node->whichChild.setValue(SO_SWITCH_ALL);
        CHECK(switch_node->whichChild.getValue() == SO_SWITCH_ALL);
        
        switch_node->unref();
    }
}

// ============================================================================
// Geometry Node Tests  
// ============================================================================

TEST_CASE("Geometry Nodes - Basic Shapes", "[nodes][geometry][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("SoCube comprehensive testing") {
        SoCube* cube = new SoCube;
        cube->ref();
        
        // Test default values
        CHECK(cube->width.getValue() == 2.0f);
        CHECK(cube->height.getValue() == 2.0f);
        CHECK(cube->depth.getValue() == 2.0f);
        
        // Test field modifications
        cube->width.setValue(1.5f);
        cube->height.setValue(2.5f);
        cube->depth.setValue(0.5f);
        
        CHECK(cube->width.getValue() == 1.5f);
        CHECK(cube->height.getValue() == 2.5f);
        CHECK(cube->depth.getValue() == 0.5f);
        
        // Test rendering capability
        COIN_TEST_WITH_OSMESA_CONTEXT(256, 256) {
            RenderingTestUtils::RenderTestFixture render_fixture(256, 256);
            SoSeparator* scene = StandardTestScenes::createMinimalScene();
            scene->addChild(cube);
            
            CHECK(render_fixture.renderScene(scene));
            auto analysis = render_fixture.analyzeRenderedPixels();
            CHECK(analysis.non_black_pixels > 0);
            
            scene->unref();
        }
        
        cube->unref();
    }
    
    SECTION("SoSphere comprehensive testing") {
        SoSphere* sphere = new SoSphere;
        sphere->ref();
        
        // Test default values
        CHECK(sphere->radius.getValue() == 1.0f);
        
        // Test field modifications
        sphere->radius.setValue(2.5f);
        CHECK(sphere->radius.getValue() == 2.5f);
        
        // Test rendering
        COIN_TEST_WITH_OSMESA_CONTEXT(256, 256) {
            RenderingTestUtils::RenderTestFixture render_fixture(256, 256);
            SoSeparator* scene = StandardTestScenes::createMinimalScene();
            scene->addChild(sphere);
            
            CHECK(render_fixture.renderScene(scene));
            auto analysis = render_fixture.analyzeRenderedPixels();
            CHECK(analysis.non_black_pixels > 0);
            CHECK(analysis.has_color_variation); // Sphere should have shading variation
            
            scene->unref();
        }
        
        sphere->unref();
    }
    
    SECTION("SoCylinder comprehensive testing") {
        SoCylinder* cylinder = new SoCylinder;
        cylinder->ref();
        
        // Test default values and parts
        CHECK(cylinder->radius.getValue() == 1.0f);
        CHECK(cylinder->height.getValue() == 2.0f);
        CHECK((cylinder->parts.getValue() & SoCylinder::SIDES) != 0);
        CHECK((cylinder->parts.getValue() & SoCylinder::TOP) != 0);
        CHECK((cylinder->parts.getValue() & SoCylinder::BOTTOM) != 0);
        
        // Test parts modification
        cylinder->parts.setValue(SoCylinder::SIDES);
        CHECK(cylinder->parts.getValue() == SoCylinder::SIDES);
        
        // Test rendering with different parts
        COIN_TEST_WITH_OSMESA_CONTEXT(256, 256) {
            RenderingTestUtils::RenderTestFixture render_fixture(256, 256);
            SoSeparator* scene = StandardTestScenes::createMinimalScene();
            scene->addChild(cylinder);
            
            CHECK(render_fixture.renderScene(scene));
            auto analysis = render_fixture.analyzeRenderedPixels();
            CHECK(analysis.non_black_pixels > 0);
            
            scene->unref();
        }
        
        cylinder->unref();
    }
    
    SECTION("SoCone comprehensive testing") {
        SoCone* cone = new SoCone;
        cone->ref();
        
        // Test default values
        CHECK(cone->bottomRadius.getValue() == 1.0f);
        CHECK(cone->height.getValue() == 2.0f);
        
        // Test parts
        CHECK((cone->parts.getValue() & SoCone::SIDES) != 0);
        CHECK((cone->parts.getValue() & SoCone::BOTTOM) != 0);
        
        cone->parts.setValue(SoCone::ALL);
        CHECK(cone->parts.getValue() == SoCone::ALL);
        
        cone->unref();
    }
}

TEST_CASE("Geometry Nodes - Complex Shapes", "[nodes][geometry][complex][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("SoIndexedFaceSet comprehensive testing") {
        SoIndexedFaceSet* faceset = new SoIndexedFaceSet;
        faceset->ref();
        
        SoCoordinate3* coords = new SoCoordinate3;
        coords->point.setValues(0, 4, new SbVec3f[4] {
            SbVec3f(-1, -1, 0), SbVec3f(1, -1, 0),
            SbVec3f(1, 1, 0), SbVec3f(-1, 1, 0)
        });
        
        // Create a simple quad
        int32_t indices[] = {0, 1, 2, 3, -1};
        faceset->coordIndex.setValues(0, 5, indices);
        
        // Test with scene
        SoSeparator* scene = StandardTestScenes::createMinimalScene();
        scene->addChild(coords);
        scene->addChild(faceset);
        
        // Validate scene structure
        CHECK(SceneGraphValidator::validateSceneStructure(scene));
        auto node_counts = SceneGraphValidator::countNodeTypes(scene);
        
        CHECK(node_counts["IndexedFaceSet"] == 1);
        CHECK(node_counts["Coordinate3"] == 1);
        
        scene->unref();
        faceset->unref();
    }
    
    SECTION("SoLineSet comprehensive testing") {
        SoLineSet* lineset = new SoLineSet;
        lineset->ref();
        
        SoCoordinate3* coords = new SoCoordinate3;
        coords->point.setValues(0, 6, new SbVec3f[6] {
            SbVec3f(0, 0, 0), SbVec3f(1, 0, 0), SbVec3f(1, 1, 0),
            SbVec3f(2, 0, 0), SbVec3f(3, 0, 0), SbVec3f(3, 1, 0)
        });
        
        // Create two line segments
        int32_t vertex_counts[] = {3, 3};
        lineset->numVertices.setValues(0, 2, vertex_counts);
        
        SoSeparator* scene = StandardTestScenes::createMinimalScene();
        scene->addChild(coords);
        scene->addChild(lineset);
        
        CHECK(SceneGraphValidator::validateSceneStructure(scene));
        
        scene->unref();
        lineset->unref();
    }
}

// ============================================================================
// Property Node Tests
// ============================================================================

TEST_CASE("Property Nodes - Material and Appearance", "[nodes][properties][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("SoMaterial comprehensive testing") {
        SoMaterial* material = new SoMaterial;
        material->ref();
        
        // Test default values
        CHECK(material->diffuseColor.getNum() == 1);
        SbColor default_color = material->diffuseColor[0];
        CHECK(default_color[0] == 0.8f);
        CHECK(default_color[1] == 0.8f);
        CHECK(default_color[2] == 0.8f);
        
        // Test field modifications
        material->diffuseColor.setValue(SbColor(1.0f, 0.0f, 0.0f));
        SbColor red_color = material->diffuseColor[0];
        CHECK(red_color[0] == 1.0f);
        CHECK(red_color[1] == 0.0f);
        CHECK(red_color[2] == 0.0f);
        
        // Test multiple colors
        SbColor colors[] = {
            SbColor(1.0f, 0.0f, 0.0f),
            SbColor(0.0f, 1.0f, 0.0f),
            SbColor(0.0f, 0.0f, 1.0f)
        };
        material->diffuseColor.setValues(0, 3, colors);
        CHECK(material->diffuseColor.getNum() == 3);
        
        // Test transparency
        material->transparency.setValue(0.5f);
        CHECK(material->transparency[0] == 0.5f);
        
        // Test with rendering
        COIN_TEST_WITH_OSMESA_CONTEXT(256, 256) {
            RenderingTestUtils::RenderTestFixture render_fixture(256, 256);
            SoSeparator* scene = StandardTestScenes::createMinimalScene();
            scene->addChild(material);
            
            SoCube* cube = new SoCube;
            scene->addChild(cube);
            
            CHECK(render_fixture.renderScene(scene));
            auto analysis = render_fixture.analyzeRenderedPixels();
            CHECK(analysis.non_black_pixels > 0);
            
            scene->unref();
        }
        
        material->unref();
    }
    
    SECTION("SoTransform comprehensive testing") {
        SoTransform* transform = new SoTransform;
        transform->ref();
        
        // Test default values
        CHECK(transform->translation.getValue() == SbVec3f(0, 0, 0));
        CHECK(transform->scaleFactor.getValue() == SbVec3f(1, 1, 1));
        
        // Test transformations
        transform->translation.setValue(1, 2, 3);
        CHECK(transform->translation.getValue() == SbVec3f(1, 2, 3));
        
        transform->scaleFactor.setValue(2, 0.5f, 1.5f);
        CHECK(transform->scaleFactor.getValue() == SbVec3f(2, 0.5f, 1.5f));
        
        // Test rotation
        transform->rotation.setValue(SbVec3f(0, 1, 0), M_PI/4);
        SbVec3f axis;
        float angle;
        transform->rotation.getValue(axis, angle);
        CHECK(axis.equals(SbVec3f(0, 1, 0), 0.001f));
        CHECK(abs(angle - M_PI/4) < 0.001f);
        
        transform->unref();
    }
    
    SECTION("SoComplexity comprehensive testing") {
        SoComplexity* complexity = new SoComplexity;
        complexity->ref();
        
        // Test default values
        CHECK(complexity->value.getValue() == 0.5f);
        CHECK(complexity->type.getValue() == SoComplexity::OBJECT_SPACE);
        
        // Test modifications
        complexity->value.setValue(0.8f);
        CHECK(complexity->value.getValue() == 0.8f);
        
        complexity->type.setValue(SoComplexity::SCREEN_SPACE);
        CHECK(complexity->type.getValue() == SoComplexity::SCREEN_SPACE);
        
        complexity->unref();
    }
}

// ============================================================================
// Camera Node Tests
// ============================================================================

TEST_CASE("Camera Nodes - Viewing and Projection", "[nodes][cameras][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("SoPerspectiveCamera comprehensive testing") {
        SoPerspectiveCamera* camera = new SoPerspectiveCamera;
        camera->ref();
        
        // Test default values
        CHECK(camera->heightAngle.getValue() > 0);
        CHECK(camera->nearDistance.getValue() > 0);
        CHECK(camera->farDistance.getValue() > camera->nearDistance.getValue());
        
        // Test field modifications
        camera->position.setValue(0, 0, 10);
        CHECK(camera->position.getValue() == SbVec3f(0, 0, 10));
        
        camera->heightAngle.setValue(M_PI/3); // 60 degrees
        CHECK(abs(camera->heightAngle.getValue() - M_PI/3) < 0.001f);
        
        // Test orientation
        camera->orientation.setValue(SbVec3f(0, 1, 0), M_PI/4);
        SbVec3f axis;
        float angle;
        camera->orientation.getValue(axis, angle);
        CHECK(axis.equals(SbVec3f(0, 1, 0), 0.001f));
        
        // Test with scene rendering
        COIN_TEST_WITH_OSMESA_CONTEXT(256, 256) {
            RenderingTestUtils::RenderTestFixture render_fixture(256, 256);
            SoSeparator* scene = new SoSeparator;
            scene->ref();
            
            scene->addChild(camera);
            
            SoDirectionalLight* light = new SoDirectionalLight;
            scene->addChild(light);
            
            SoCube* cube = new SoCube;
            scene->addChild(cube);
            
            CHECK(render_fixture.renderScene(scene));
            auto analysis = render_fixture.analyzeRenderedPixels();
            CHECK(analysis.non_black_pixels > 0);
            
            scene->unref();
        }
        
        camera->unref();
    }
    
    SECTION("SoOrthographicCamera comprehensive testing") {
        SoOrthographicCamera* camera = new SoOrthographicCamera;
        camera->ref();
        
        // Test default values
        CHECK(camera->height.getValue() > 0);
        
        // Test field modifications
        camera->height.setValue(10.0f);
        CHECK(camera->height.getValue() == 10.0f);
        
        camera->position.setValue(0, 0, 5);
        CHECK(camera->position.getValue() == SbVec3f(0, 0, 5));
        
        camera->unref();
    }
}

// ============================================================================
// Light Node Tests
// ============================================================================

TEST_CASE("Light Nodes - Illumination", "[nodes][lights][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("SoDirectionalLight comprehensive testing") {
        SoDirectionalLight* light = new SoDirectionalLight;
        light->ref();
        
        // Test default values
        CHECK(light->on.getValue() == TRUE);
        CHECK(light->intensity.getValue() == 1.0f);
        
        // Test field modifications
        SbVec3f direction(1, -1, -1);
        direction.normalize();
        light->direction.setValue(direction);
        
        light->color.setValue(1.0f, 0.8f, 0.6f);
        SbColor color = light->color.getValue();
        CHECK(color[0] == 1.0f);
        CHECK(color[1] == 0.8f);
        CHECK(color[2] == 0.6f);
        
        light->intensity.setValue(0.7f);
        CHECK(light->intensity.getValue() == 0.7f);
        
        light->unref();
    }
    
    SECTION("SoPointLight comprehensive testing") {
        SoPointLight* light = new SoPointLight;
        light->ref();
        
        // Test default values
        CHECK(light->on.getValue() == TRUE);
        
        // Test position
        light->location.setValue(2, 3, 4);
        CHECK(light->location.getValue() == SbVec3f(2, 3, 4));
        
        // Note: SoPointLight in Coin3D doesn't have separate attenuation fields
        // Attenuation is inherited from SoLight base class
        
        light->unref();
    }
    
    SECTION("SoSpotLight comprehensive testing") {
        SoSpotLight* light = new SoSpotLight;
        light->ref();
        
        // Test specific spotlight properties
        light->cutOffAngle.setValue(M_PI/6); // 30 degrees
        CHECK(abs(light->cutOffAngle.getValue() - M_PI/6) < 0.001f);
        
        light->dropOffRate.setValue(0.5f);
        CHECK(light->dropOffRate.getValue() == 0.5f);
        
        // Test direction
        light->direction.setValue(0, -1, 0);
        CHECK(light->direction.getValue() == SbVec3f(0, -1, 0));
        
        light->unref();
    }
}

// ============================================================================
// Integration Tests - Node Combinations
// ============================================================================

TEST_CASE("Node Integration - Scene Graph Combinations", "[nodes][integration][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("Complex scene graph validation") {
        SoSeparator* scene = StandardTestScenes::createComplexScene();
        
        // Validate overall structure
        CHECK(SceneGraphValidator::validateSceneStructure(scene));
        CHECK(SceneGraphValidator::validateNodeTypes(scene));
        
        // Analyze scene composition
        auto node_counts = SceneGraphValidator::countNodeTypes(scene);
        CHECK(node_counts.size() > 5); // Should have multiple node types
        
        // Check for common issues
        auto issues = SceneGraphValidator::analyzeSceneIssues(scene);
        // Should have minimal issues for a well-formed test scene
        
        scene->unref();
    }
    
    SECTION("Material and geometry interaction") {
        COIN_TEST_WITH_OSMESA_CONTEXT(256, 256) {
            RenderingTestUtils::RenderTestFixture render_fixture(256, 256);
            SoSeparator* scene = StandardTestScenes::createMaterialTestScene();
            
            CHECK(render_fixture.renderScene(scene));
            auto analysis = render_fixture.analyzeRenderedPixels();
            
            // Material test scene should have significant color variation
            CHECK(analysis.non_black_pixels > 1000);
            CHECK(analysis.has_color_variation);
            CHECK(analysis.avg_brightness > 0.1f);
            
            scene->unref();
        }
    }
    
    SECTION("Transform hierarchy validation") {
        SoSeparator* scene = StandardTestScenes::createTransformTestScene();
        
        // Test that transforms are properly applied
        CHECK(ActionTestUtils::testBoundingBoxAction(scene));
        
        // Validate scene structure
        CHECK(SceneGraphValidator::validateSceneStructure(scene));
        
        scene->unref();
    }
    
    SECTION("Pick test scene validation") {
        SoSeparator* scene = StandardTestScenes::createPickTestScene();
        
        // Test picking capability
        CHECK(ActionTestUtils::testPickAction(scene));
        
        // Test bounding box computation
        CHECK(ActionTestUtils::testBoundingBoxAction(scene));
        
        scene->unref();
    }
}

// ============================================================================
// Standard Test Scene Validation
// ============================================================================

TEST_CASE("Standard Test Scenes - Validation", "[nodes][scenes][comprehensive]") {
    CoinTestFixture fixture;
    
    // Test all standard scenes using the convenient macro
    COIN_TEST_SCENE(Minimal);
    COIN_TEST_SCENE(BasicGeometry);
    COIN_TEST_SCENE(Complex);
    COIN_TEST_SCENE(PickTest);
    COIN_TEST_SCENE(MaterialTest);
    COIN_TEST_SCENE(TransformTest);
    COIN_TEST_SCENE(AnimationTest);
}

// ============================================================================
// Rendering Validation Tests
// ============================================================================

TEST_CASE("Node Rendering - Visual Validation", "[nodes][rendering][comprehensive]") {
    CoinTestFixture fixture;
    
    // Test rendering of standard scenes using the convenient macro
    COIN_RENDER_TEST(BasicGeometry, rendering);
    COIN_RENDER_TEST(MaterialTest, rendering);
    COIN_RENDER_TEST(TransformTest, rendering);
}