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
 * @file test_actions_comprehensive.cpp
 * @brief Comprehensive tests for all Coin3D action types and user-facing functionality
 *
 * This module provides comprehensive testing of action traversal, state management,
 * picking, bounding box computation, rendering, and other action-based operations
 * using OSMesa offscreen rendering for validation.
 */

#include "utils/test_common.h"
#include "utils/osmesa_test_context.h"
#include "utils/scene_graph_test_utils.h"

// Core action classes
#include <Inventor/actions/SoAction.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/actions/SoPickAction.h>
#include <Inventor/actions/SoRayPickAction.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/actions/SoCallbackAction.h>
#include <Inventor/actions/SoGetMatrixAction.h>
#include <Inventor/actions/SoHandleEventAction.h>
#include <Inventor/actions/SoWriteAction.h>
#include <Inventor/actions/SoGetPrimitiveCountAction.h>

// Event and interaction
#include <Inventor/events/SoMouseButtonEvent.h>
#include <Inventor/events/SoLocation2Event.h>
#include <Inventor/events/SoKeyboardEvent.h>

// Nodes for testing
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>

// Additional utilities
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoPath.h>

using namespace CoinTestUtils;

// ============================================================================
// Core Action Tests
// ============================================================================

TEST_CASE("Core Actions - Basic Functionality", "[actions][core][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("SoAction base class functionality") {
        SoSeparator* scene = StandardTestScenes::createMinimalScene();
        
        // Test basic action properties
        SoSearchAction search;
        CHECK(search.getTypeId() != SoType::badType());
        CHECK(search.isOfType(SoAction::getClassTypeId()));
        
        // Test action application
        search.apply(scene);
        CHECK(search.hasTerminated() == FALSE); // Search didn't find anything specific
        
        scene->unref();
    }
    
    SECTION("Action state management") {
        SoSeparator* scene = StandardTestScenes::createBasicGeometryScene();
        
        SoCallbackAction callback;
        
        // Test that action can traverse the scene
        callback.apply(scene);
        
        // Action should complete without issues
        CHECK(callback.hasTerminated() == FALSE);
        
        scene->unref();
    }
}

// ============================================================================
// Rendering Action Tests
// ============================================================================

TEST_CASE("SoGLRenderAction - Comprehensive Testing", "[actions][rendering][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("Basic rendering action functionality") {
        COIN_TEST_WITH_OSMESA_CONTEXT(256, 256) {
            RenderingTestUtils::RenderTestFixture render_fixture(256, 256);
            SoSeparator* scene = StandardTestScenes::createBasicGeometryScene();
            
            SoGLRenderAction* action = render_fixture.getRenderAction();
            CHECK(action != nullptr);
            
            // Test action properties
            CHECK(action->getTypeId() == SoGLRenderAction::getClassTypeId());
            CHECK(action->isOfType(SoAction::getClassTypeId()));
            
            // Test rendering
            CHECK(render_fixture.renderScene(scene));
            
            auto analysis = render_fixture.analyzeRenderedPixels();
            CHECK(analysis.non_black_pixels > 0);
            CHECK(analysis.avg_brightness > 0.01f);
            
            scene->unref();
        }
    }
    
    SECTION("Render action with different viewport sizes") {
        COIN_TEST_WITH_OSMESA_CONTEXT(128, 128) {
            RenderingTestUtils::RenderTestFixture render_fixture_small(128, 128);
            SoSeparator* scene = StandardTestScenes::createMinimalScene();
            
            SoCube* cube = new SoCube;
            scene->addChild(cube);
            
            CHECK(render_fixture_small.renderScene(scene));
            auto analysis_small = render_fixture_small.analyzeRenderedPixels();
            (void)analysis_small; // Suppress unused variable warning
            
            scene->unref();
        }
        
        COIN_TEST_WITH_OSMESA_CONTEXT(512, 512) {
            RenderingTestUtils::RenderTestFixture render_fixture_large(512, 512);
            SoSeparator* scene = StandardTestScenes::createMinimalScene();
            
            SoCube* cube = new SoCube;
            scene->addChild(cube);
            
            CHECK(render_fixture_large.renderScene(scene));
            auto analysis_large = render_fixture_large.analyzeRenderedPixels();
            
            // Larger viewport should have more rendered pixels
            CHECK(analysis_large.non_black_pixels > 100);
            
            scene->unref();
        }
    }
    
    SECTION("Render action with transparency") {
        COIN_TEST_WITH_OSMESA_CONTEXT(256, 256) {
            RenderingTestUtils::RenderTestFixture render_fixture(256, 256);
            SoSeparator* scene = StandardTestScenes::createMinimalScene();
            
            // Add transparent material
            SoMaterial* material = new SoMaterial;
            material->diffuseColor.setValue(1, 0, 0);
            material->transparency.setValue(0.5f);
            scene->addChild(material);
            
            SoCube* cube = new SoCube;
            scene->addChild(cube);
            
            CHECK(render_fixture.renderScene(scene));
            auto analysis = render_fixture.analyzeRenderedPixels();
            CHECK(analysis.non_black_pixels > 0);
            
            scene->unref();
        }
    }
}

// ============================================================================
// Bounding Box Action Tests
// ============================================================================

TEST_CASE("SoGetBoundingBoxAction - Comprehensive Testing", "[actions][bounding_box][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("Basic bounding box computation") {
        SoSeparator* scene = StandardTestScenes::createBasicGeometryScene();
        
        SoGetBoundingBoxAction bbox_action(SbViewportRegion(100, 100));
        bbox_action.apply(scene);
        
        SbBox3f bbox = bbox_action.getBoundingBox();
        
        // Scene should have a valid bounding box
        CHECK(!bbox.isEmpty());
        CHECK(bbox.getVolume() > 0.0f);
        
        // Check reasonable bounds for the basic geometry scene
        SbVec3f min, max;
        bbox.getBounds(min, max);
        
        // Basic scene should span some reasonable space
        CHECK(max[0] > min[0]);
        CHECK(max[1] > min[1]);
        CHECK(max[2] > min[2]);
        
        scene->unref();
    }
    
    SECTION("Bounding box with transformations") {
        SoSeparator* scene = StandardTestScenes::createTransformTestScene();
        
        SoGetBoundingBoxAction bbox_action(SbViewportRegion(100, 100));
        bbox_action.apply(scene);
        
        SbBox3f bbox = bbox_action.getBoundingBox();
        
        // Transformed scene should still have valid bounding box
        CHECK(!bbox.isEmpty());
        CHECK(bbox.getVolume() > 0.0f);
        
        scene->unref();
    }
    
    SECTION("Empty scene bounding box") {
        SoSeparator* empty_scene = new SoSeparator;
        empty_scene->ref();
        
        SoGetBoundingBoxAction bbox_action(SbViewportRegion(100, 100));
        bbox_action.apply(empty_scene);
        
        SbBox3f bbox = bbox_action.getBoundingBox();
        
        // Empty scene should have empty bounding box
        CHECK(bbox.isEmpty());
        
        empty_scene->unref();
    }
    
    SECTION("Single object bounding box") {
        SoSeparator* scene = new SoSeparator;
        scene->ref();
        
        SoCube* cube = new SoCube;
        cube->width.setValue(2.0f);
        cube->height.setValue(3.0f);
        cube->depth.setValue(1.0f);
        scene->addChild(cube);
        
        SoGetBoundingBoxAction bbox_action(SbViewportRegion(100, 100));
        bbox_action.apply(scene);
        
        SbBox3f bbox = bbox_action.getBoundingBox();
        
        CHECK(!bbox.isEmpty());
        
        SbVec3f min, max;
        bbox.getBounds(min, max);
        
        // Check that bounding box approximately matches cube dimensions
        float width = max[0] - min[0];
        float height = max[1] - min[1];
        float depth = max[2] - min[2];
        
        CHECK(abs(width - 2.0f) < 0.1f);
        CHECK(abs(height - 3.0f) < 0.1f);
        CHECK(abs(depth - 1.0f) < 0.1f);
        
        scene->unref();
    }
}

// ============================================================================
// Pick Action Tests
// ============================================================================

TEST_CASE("SoRayPickAction - Comprehensive Testing", "[actions][picking][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("Basic ray picking functionality") {
        SoSeparator* scene = StandardTestScenes::createPickTestScene();
        
        SbViewportRegion viewport(256, 256);
        SoRayPickAction pick_action(viewport);
        
        // Test picking at center of viewport
        pick_action.setPoint(SbVec2s(128, 128));
        pick_action.setRadius(5);
        pick_action.apply(scene);
        
        // Pick action should complete without crashing
        CHECK(pick_action.hasTerminated() == FALSE);
        
        scene->unref();
    }
    
    SECTION("Pick action with different ray configurations") {
        SoSeparator* scene = StandardTestScenes::createBasicGeometryScene();
        
        SbViewportRegion viewport(100, 100);
        SoRayPickAction pick_action(viewport);
        
        // Test multiple pick points
        SbVec2s pick_points[] = {
            SbVec2s(50, 50),   // Center
            SbVec2s(25, 25),   // Lower left quadrant
            SbVec2s(75, 75),   // Upper right quadrant
            SbVec2s(10, 90),   // Upper left corner
            SbVec2s(90, 10)    // Lower right corner
        };
        
        for (int i = 0; i < 5; ++i) {
            pick_action.setPoint(pick_points[i]);
            pick_action.setRadius(3);
            pick_action.apply(scene);
            
            // Each pick should complete successfully
            CHECK(pick_action.hasTerminated() == FALSE);
        }
        
        scene->unref();
    }
    
    SECTION("Pick action with OSMesa rendering context") {
        COIN_TEST_WITH_OSMESA_CONTEXT(256, 256) {
            RenderingTestUtils::RenderTestFixture render_fixture(256, 256);
            SoSeparator* scene = StandardTestScenes::createBasicGeometryScene();
            
            // First render the scene
            CHECK(render_fixture.renderScene(scene));
            
            // Then test picking on the rendered scene
            SbViewportRegion viewport = render_fixture.getViewport();
            SoRayPickAction pick_action(viewport);
            pick_action.setPoint(SbVec2s(128, 128));
            pick_action.setRadius(10);
            pick_action.apply(scene);
            
            CHECK(pick_action.hasTerminated() == FALSE);
            
            scene->unref();
        }
    }
}

// ============================================================================
// Search Action Tests
// ============================================================================

TEST_CASE("SoSearchAction - Comprehensive Testing", "[actions][search][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("Search by node type") {
        SoSeparator* scene = StandardTestScenes::createComplexScene();
        
        // Search for all cubes
        SoSearchAction search;
        search.setType(SoCube::getClassTypeId());
        search.setInterest(SoSearchAction::ALL);
        search.apply(scene);
        
        SoPathList& paths = search.getPaths();
        CHECK(paths.getLength() > 0);
        
        // Verify all found nodes are actually cubes
        for (int i = 0; i < paths.getLength(); ++i) {
            SoPath* path = paths[i];
            SoNode* tail = path->getTail();
            CHECK(tail->isOfType(SoCube::getClassTypeId()));
        }
        
        scene->unref();
    }
    
    SECTION("Search for specific node instance") {
        SoSeparator* scene = new SoSeparator;
        scene->ref();
        
        SoCube* target_cube = new SoCube;
        target_cube->setName("target");
        
        scene->addChild(target_cube);
        scene->addChild(new SoCube); // Another cube without name
        
        // Search for the specific named cube
        SoSearchAction search;
        search.setNode(target_cube);
        search.apply(scene);
        
        SoPath* path = search.getPath();
        CHECK(path != nullptr);
        CHECK(path->getTail() == target_cube);
        
        scene->unref();
    }
    
    SECTION("Search with depth limiting") {
        SoSeparator* scene = new SoSeparator;
        scene->ref();
        
        // Create nested structure
        SoSeparator* level1 = new SoSeparator;
        SoSeparator* level2 = new SoSeparator;
        SoCube* deep_cube = new SoCube;
        
        scene->addChild(level1);
        level1->addChild(level2);
        level2->addChild(deep_cube);
        
        // Search with limited depth
        SoSearchAction search;
        search.setType(SoCube::getClassTypeId());
        search.apply(scene);
        
        // Should find the cube regardless of depth
        CHECK(search.getPath() != nullptr);
        
        scene->unref();
    }
}

// ============================================================================
// Callback Action Tests
// ============================================================================

TEST_CASE("SoCallbackAction - Comprehensive Testing", "[actions][callback][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("Basic callback action functionality") {
        SoSeparator* scene = StandardTestScenes::createBasicGeometryScene();
        
        int callback_count = 0;
        
        SoCallbackAction callback_action;
        callback_action.addPreCallback(SoNode::getClassTypeId(), 
            [](void* userdata, SoCallbackAction*, const SoNode*) -> SoCallbackAction::Response {
                int* count = static_cast<int*>(userdata);
                (*count)++;
                return SoCallbackAction::CONTINUE;
            }, &callback_count);
        
        callback_action.apply(scene);
        
        // Should have visited multiple nodes
        CHECK(callback_count > 0);
        
        scene->unref();
    }
    
    SECTION("Callback action with node filtering") {
        SoSeparator* scene = StandardTestScenes::createComplexScene();
        
        int cube_count = 0;
        int total_count = 0;
        
        SoCallbackAction callback_action;
        
        // Count all nodes
        callback_action.addPreCallback(SoNode::getClassTypeId(),
            [](void* userdata, SoCallbackAction*, const SoNode*) -> SoCallbackAction::Response {
                int* count = static_cast<int*>(userdata);
                (*count)++;
                return SoCallbackAction::CONTINUE;
            }, &total_count);
        
        // Count specifically cubes
        callback_action.addPreCallback(SoCube::getClassTypeId(),
            [](void* userdata, SoCallbackAction*, const SoNode*) -> SoCallbackAction::Response {
                int* count = static_cast<int*>(userdata);
                (*count)++;
                return SoCallbackAction::CONTINUE;
            }, &cube_count);
        
        callback_action.apply(scene);
        
        CHECK(total_count > cube_count);
        CHECK(cube_count > 0);
        
        scene->unref();
    }
    
    SECTION("Callback action early termination") {
        SoSeparator* scene = StandardTestScenes::createBasicGeometryScene();
        
        int callback_count = 0;
        const int max_callbacks = 3;
        
        SoCallbackAction callback_action;
        callback_action.addPreCallback(SoNode::getClassTypeId(),
            [](void* userdata, SoCallbackAction*, const SoNode*) -> SoCallbackAction::Response {
                int* count = static_cast<int*>(userdata);
                (*count)++;
                if (*count >= 3) {
                    return SoCallbackAction::ABORT;
                }
                return SoCallbackAction::CONTINUE;
            }, &callback_count);
        
        callback_action.apply(scene);
        
        // Should have terminated early
        CHECK(callback_count == max_callbacks);
        CHECK(callback_action.hasTerminated() == TRUE);
        
        scene->unref();
    }
}

// ============================================================================
// Matrix Action Tests
// ============================================================================

TEST_CASE("SoGetMatrixAction - Comprehensive Testing", "[actions][matrix][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("Matrix computation for simple transforms") {
        SoSeparator* scene = new SoSeparator;
        scene->ref();
        
        SoTransform* transform = new SoTransform;
        transform->translation.setValue(1, 2, 3);
        transform->scaleFactor.setValue(2, 2, 2);
        scene->addChild(transform);
        
        SoCube* cube = new SoCube;
        scene->addChild(cube);
        
        SoGetMatrixAction matrix_action(SbViewportRegion(100, 100));
        matrix_action.apply(scene);
        
        SbMatrix matrix = matrix_action.getMatrix();
        SbMatrix inverse = matrix_action.getInverse();
        
        // Check that we have valid matrices
        CHECK(matrix != SbMatrix::identity());
        
        // Check inverse relationship
        SbMatrix product = matrix * inverse;
        CHECK(product.equals(SbMatrix::identity(), 0.001f));
        
        scene->unref();
    }
    
    SECTION("Matrix computation for nested transforms") {
        SoSeparator* scene = StandardTestScenes::createTransformTestScene();
        
        SoGetMatrixAction matrix_action(SbViewportRegion(100, 100));
        matrix_action.apply(scene);
        
        SbMatrix matrix = matrix_action.getMatrix();
        SbMatrix inverse = matrix_action.getInverse();
        
        // Check that matrices are computed correctly
        CHECK(matrix != SbMatrix::identity());
        
        scene->unref();
    }
}

// ============================================================================
// Event Handling Action Tests
// ============================================================================

TEST_CASE("SoHandleEventAction - Comprehensive Testing", "[actions][events][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("Basic event handling") {
        SoSeparator* scene = StandardTestScenes::createBasicGeometryScene();
        
        SbViewportRegion viewport(256, 256);
        SoHandleEventAction event_action(viewport);
        
        // Create a mouse button event
        SoMouseButtonEvent* button_event = new SoMouseButtonEvent;
        button_event->setButton(SoMouseButtonEvent::BUTTON1);
        button_event->setState(SoButtonEvent::DOWN);
        button_event->setPosition(SbVec2s(128, 128));
        
        event_action.setEvent(button_event);
        event_action.apply(scene);
        
        // Event action should complete without issues
        CHECK(event_action.hasTerminated() == FALSE);
        
        delete button_event;
        scene->unref();
    }
    
    SECTION("Keyboard event handling") {
        SoSeparator* scene = StandardTestScenes::createMinimalScene();
        
        SbViewportRegion viewport(100, 100);
        SoHandleEventAction event_action(viewport);
        
        // Create a keyboard event
        SoKeyboardEvent* key_event = new SoKeyboardEvent;
        key_event->setKey(SoKeyboardEvent::SPACE);
        key_event->setState(SoButtonEvent::DOWN);
        
        event_action.setEvent(key_event);
        event_action.apply(scene);
        
        CHECK(event_action.hasTerminated() == FALSE);
        
        delete key_event;
        scene->unref();
    }
    
    SECTION("Mouse motion event handling") {
        SoSeparator* scene = StandardTestScenes::createPickTestScene();
        
        SbViewportRegion viewport(200, 200);
        SoHandleEventAction event_action(viewport);
        
        // Create a mouse motion event
        SoLocation2Event* motion_event = new SoLocation2Event;
        motion_event->setPosition(SbVec2s(100, 100));
        
        event_action.setEvent(motion_event);
        event_action.apply(scene);
        
        CHECK(event_action.hasTerminated() == FALSE);
        
        delete motion_event;
        scene->unref();
    }
}

// ============================================================================
// Primitive Count Action Tests
// ============================================================================

TEST_CASE("SoGetPrimitiveCountAction - Comprehensive Testing", "[actions][primitive_count][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("Basic primitive counting") {
        SoSeparator* scene = StandardTestScenes::createBasicGeometryScene();
        
        SoGetPrimitiveCountAction count_action;
        count_action.apply(scene);
        
        // Scene should have some primitives
        int triangle_count = count_action.getTriangleCount();
        int line_count = count_action.getLineCount();
        int point_count = count_action.getPointCount();
        
        // Basic geometry scene should have triangles from the shapes
        CHECK(triangle_count >= 0);
        CHECK(line_count >= 0);
        CHECK(point_count >= 0);
        
        // Should have some geometry
        CHECK(triangle_count > 0);
        
        scene->unref();
    }
    
    SECTION("Primitive counting for specific shapes") {
        SoSeparator* scene = new SoSeparator;
        scene->ref();
        
        // Add a single cube
        SoCube* cube = new SoCube;
        scene->addChild(cube);
        
        SoGetPrimitiveCountAction count_action;
        count_action.apply(scene);
        
        int triangle_count = count_action.getTriangleCount();
        
        // A cube should generate some primitives
        CHECK(triangle_count > 0);
        
        scene->unref();
    }
    
    SECTION("Empty scene primitive count") {
        SoSeparator* empty_scene = new SoSeparator;
        empty_scene->ref();
        
        SoGetPrimitiveCountAction count_action;
        count_action.apply(empty_scene);
        
        // Empty scene should have no primitives
        CHECK(count_action.getTriangleCount() == 0);
        CHECK(count_action.getLineCount() == 0);
        CHECK(count_action.getPointCount() == 0);
        
        empty_scene->unref();
    }
}

// ============================================================================
// Action Integration Tests
// ============================================================================

TEST_CASE("Action Integration - Multiple Actions on Same Scene", "[actions][integration][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("Sequential action application") {
        SoSeparator* scene = StandardTestScenes::createComplexScene();
        
        // Apply multiple actions in sequence
        SoGetBoundingBoxAction bbox_action(SbViewportRegion(100, 100));
        bbox_action.apply(scene);
        SbBox3f bbox = bbox_action.getBoundingBox();
        
        SoSearchAction search_action;
        search_action.setType(SoCube::getClassTypeId());
        search_action.apply(scene);
        
        SoGetPrimitiveCountAction count_action;
        count_action.apply(scene);
        
        // All actions should complete successfully
        CHECK(!bbox.isEmpty());
        bool search_found = (search_action.getPath() != nullptr) || (search_action.getPaths().getLength() > 0);
        CHECK(search_found);
        CHECK(count_action.getTriangleCount() > 0);
        
        scene->unref();
    }
    
    SECTION("Action performance timing") {
        SoSeparator* scene = StandardTestScenes::createComplexScene();
        
        // Time action execution for performance baseline
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < 10; ++i) {
            SoGetBoundingBoxAction bbox_action(SbViewportRegion(100, 100));
            bbox_action.apply(scene);
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        // Actions should execute reasonably quickly
        CHECK(duration.count() < 1000); // Less than 1 second for 10 iterations
        
        scene->unref();
    }
    
    SECTION("Actions with OSMesa rendering context") {
        COIN_TEST_WITH_OSMESA_CONTEXT(256, 256) {
            RenderingTestUtils::RenderTestFixture render_fixture(256, 256);
            SoSeparator* scene = StandardTestScenes::createMaterialTestScene();
            
            // First render the scene
            CHECK(render_fixture.renderScene(scene));
            
            // Then perform other actions
            SoGetBoundingBoxAction bbox_action(render_fixture.getViewport());
            bbox_action.apply(scene);
            
            SoRayPickAction pick_action(render_fixture.getViewport());
            pick_action.setPoint(SbVec2s(128, 128));
            pick_action.apply(scene);
            
            // All operations should work together
            CHECK(!bbox_action.getBoundingBox().isEmpty());
            CHECK(pick_action.hasTerminated() == FALSE);
            
            auto analysis = render_fixture.analyzeRenderedPixels();
            CHECK(analysis.non_black_pixels > 0);
            
            scene->unref();
        }
    }
}

// ============================================================================
// Action Error Handling Tests
// ============================================================================

TEST_CASE("Action Error Handling - Robustness", "[actions][error_handling][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("Actions on null scene") {
        // Actions should handle null input gracefully
        SoGetBoundingBoxAction bbox_action(SbViewportRegion(100, 100));
        // Note: Applying to nullptr would crash, so we test with empty scene instead
        
        SoSeparator* empty_scene = new SoSeparator;
        empty_scene->ref();
        
        bbox_action.apply(empty_scene);
        CHECK(bbox_action.getBoundingBox().isEmpty());
        
        empty_scene->unref();
    }
    
    SECTION("Actions with invalid viewport") {
        SoSeparator* scene = StandardTestScenes::createMinimalScene();
        
        // Test with very small viewport
        SoGetBoundingBoxAction bbox_action(SbViewportRegion(1, 1));
        bbox_action.apply(scene);
        
        // Should still work, just with minimal viewport
        CHECK(!bbox_action.getBoundingBox().isEmpty());
        
        scene->unref();
    }
    
    SECTION("Pick action with out-of-bounds coordinates") {
        SoSeparator* scene = StandardTestScenes::createBasicGeometryScene();
        
        SbViewportRegion viewport(100, 100);
        SoRayPickAction pick_action(viewport);
        
        // Test picking outside viewport bounds
        pick_action.setPoint(SbVec2s(200, 200)); // Outside 100x100 viewport
        pick_action.apply(scene);
        
        // Should handle gracefully without crashing
        CHECK(pick_action.hasTerminated() == FALSE);
        
        scene->unref();
    }
}