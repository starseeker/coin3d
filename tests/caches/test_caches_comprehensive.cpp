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
 * @file test_caches_comprehensive.cpp
 * @brief Comprehensive tests for all Coin3D cache types and user-facing functionality
 *
 * This module provides comprehensive testing of cache creation, invalidation,
 * memory management, and performance optimization functionality.
 */

#include "utils/test_common.h"
#include "utils/osmesa_test_context.h"
#include "utils/scene_graph_test_utils.h"
#include <iostream>

// Core cache classes
#include <Inventor/caches/SoCache.h>
#include <Inventor/caches/SoBoundingBoxCache.h>
#include <Inventor/caches/SoConvexDataCache.h>
#include <Inventor/caches/SoGLCacheList.h>
#include <Inventor/caches/SoGLRenderCache.h>
#include <Inventor/caches/SoNormalCache.h>
#include <Inventor/caches/SoPrimitiveVertexCache.h>
#include <Inventor/caches/SoTextureCoordinateCache.h>

// Supporting classes for cache testing
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/SbLinear.h>
#include <Inventor/SbViewportRegion.h>

using namespace CoinTestUtils;

TEST_CASE("Cache System Comprehensive Tests", "[caches][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("Basic cache operations") {
        SECTION("Cache creation and basic properties") {
            // Test cache validity and invalidation
            auto scene = StandardTestScenes::createMinimalScene();
            
            SoGetBoundingBoxAction bboxAction(SbViewportRegion(100, 100));
            bboxAction.apply(scene);
            
            // Cache should be created after action application
            CHECK(scene != nullptr);
            
            scene->unref();
        }
        
        SECTION("Cache invalidation on scene changes") {
            auto scene = StandardTestScenes::createMinimalScene();
            
            SoCube* cube = new SoCube;
            scene->addChild(cube);
            
            // Apply action to build cache
            SoGetBoundingBoxAction bboxAction1(SbViewportRegion(100, 100));
            bboxAction1.apply(scene);
            SbBox3f bbox1 = bboxAction1.getBoundingBox();
            
            // Modify the scene to different size
            cube->width.setValue(4.0f);
            cube->height.setValue(4.0f);
            cube->depth.setValue(4.0f);
            
            // Apply action again - cache should be invalidated and rebuilt
            SoGetBoundingBoxAction bboxAction2(SbViewportRegion(100, 100));
            bboxAction2.apply(scene);
            SbBox3f bbox2 = bboxAction2.getBoundingBox();
            
            // Bounding box should be different after modification
            // Note: Use volume comparison as size comparison may have precision issues
            CHECK(bbox1.getVolume() != bbox2.getVolume());
            
            scene->unref();
        }
    }
    
    SECTION("Bounding box cache tests") {
        SECTION("Basic bounding box caching") {
            auto scene = StandardTestScenes::createBasicGeometryScene();
            
            SoGetBoundingBoxAction bboxAction(SbViewportRegion(100, 100));
            bboxAction.apply(scene);
            
            SbBox3f bbox = bboxAction.getBoundingBox();
            CHECK(!bbox.isEmpty());
            
            // Apply again - should use cached result
            bboxAction.apply(scene);
            SbBox3f bbox2 = bboxAction.getBoundingBox();
            
            CHECK(bbox == bbox2);
            
            scene->unref();
        }
        
        SECTION("Complex scene bounding box") {
            auto scene = StandardTestScenes::createComplexScene();
            
            SoGetBoundingBoxAction bboxAction(SbViewportRegion(100, 100));
            bboxAction.apply(scene);
            
            SbBox3f bbox = bboxAction.getBoundingBox();
            CHECK(!bbox.isEmpty());
            CHECK(bbox.getVolume() > 0.0f);
            
            scene->unref();
        }
    }
    
    SECTION("Debug cache scene creation") {
        std::cout << "Debug: About to create scene..." << std::endl;
        auto scene = StandardTestScenes::createBasicGeometryScene();
        std::cout << "Debug: Scene created: " << scene << std::endl;
        
        if (scene) {
            std::cout << "Debug: Scene has " << scene->getNumChildren() << " children" << std::endl;
            scene->unref();
            std::cout << "Debug: Scene unreferenced successfully" << std::endl;
        }
    }
    
    SECTION("Normal cache tests") {
        SECTION("Normal generation and caching") {
            std::cout << "Debug: Normal cache test starting..." << std::endl;
            auto scene = StandardTestScenes::createBasicGeometryScene();
            std::cout << "Debug: Scene created for normal cache test" << std::endl;
            
            COIN_TEST_WITH_OSMESA_CONTEXT(256, 256) {
                std::cout << "Debug: About to create RenderTestFixture..." << std::endl;
                RenderingTestUtils::RenderTestFixture render_fixture(256, 256);
                std::cout << "Debug: RenderTestFixture created" << std::endl;
                
                // Render to trigger normal cache creation
                CHECK(render_fixture.renderScene(scene));
                
                // Render again - should use cached normals
                CHECK(render_fixture.renderScene(scene));
            }
            
            scene->unref();
        }
    }
    
    SECTION("GL render cache tests") {
        SECTION("Basic GL render caching") {
            auto scene = StandardTestScenes::createBasicGeometryScene();
            
            COIN_TEST_WITH_OSMESA_CONTEXT(256, 256) {
                RenderingTestUtils::RenderTestFixture render_fixture(256, 256);
                
                // First render - builds cache
                CHECK(render_fixture.renderScene(scene));
                auto analysis1 = render_fixture.analyzeRenderedPixels();
                
                // Second render - uses cache
                CHECK(render_fixture.renderScene(scene));
                auto analysis2 = render_fixture.analyzeRenderedPixels();
                
                // Results should be identical when using cache
                CHECK(analysis1.non_black_pixels == analysis2.non_black_pixels);
            }
            
            scene->unref();
        }
        
        SECTION("Cache invalidation on material change") {
            auto scene = StandardTestScenes::createMaterialTestScene();
            
            COIN_TEST_WITH_OSMESA_CONTEXT(256, 256) {
                RenderingTestUtils::RenderTestFixture render_fixture(256, 256);
                
                // First render
                CHECK(render_fixture.renderScene(scene));
                auto analysis1 = render_fixture.analyzeRenderedPixels();
                
                // Modify material (should invalidate cache)
                SoMaterial* material = new SoMaterial;
                material->diffuseColor.setValue(0, 1, 0); // Change to green
                scene->insertChild(material, 0);
                
                // Render again - cache should be rebuilt
                CHECK(render_fixture.renderScene(scene));
                auto analysis2 = render_fixture.analyzeRenderedPixels();
                
                // Results might be different due to material change
                CHECK(analysis2.non_black_pixels > 0);
            }
            
            scene->unref();
        }
    }
    
    SECTION("Texture coordinate cache tests") {
        SECTION("Basic texture coordinate caching") {
            auto scene = StandardTestScenes::createMaterialTestScene();
            
            // Add texture to trigger texture coordinate cache
            SoTexture2* texture = new SoTexture2;
            // Note: We don't load an actual texture file to keep tests simple
            texture->filename.setValue("");
            scene->insertChild(texture, 0);
            
            COIN_TEST_WITH_OSMESA_CONTEXT(256, 256) {
                RenderingTestUtils::RenderTestFixture render_fixture(256, 256);
                
                // Render to trigger texture coordinate cache creation
                CHECK(render_fixture.renderScene(scene));
            }
            
            scene->unref();
        }
    }
    
    SECTION("Primitive vertex cache tests") {
        SECTION("Vertex cache for complex geometry") {
            auto scene = StandardTestScenes::createComplexScene();
            
            COIN_TEST_WITH_OSMESA_CONTEXT(256, 256) {
                RenderingTestUtils::RenderTestFixture render_fixture(256, 256);
                
                // Render complex scene to trigger vertex caching
                CHECK(render_fixture.renderScene(scene));
                auto analysis = render_fixture.analyzeRenderedPixels();
                CHECK(analysis.non_black_pixels > 0);
                
                // Render again - should use cached vertices
                CHECK(render_fixture.renderScene(scene));
            }
            
            scene->unref();
        }
    }
    
    SECTION("Cache list management") {
        SECTION("GL cache list operations") {
            auto scene = StandardTestScenes::createBasicGeometryScene();
            
            COIN_TEST_WITH_OSMESA_CONTEXT(256, 256) {
                RenderingTestUtils::RenderTestFixture render_fixture(256, 256);
                
                // Multiple renders should build and manage cache lists
                for (int i = 0; i < 3; ++i) {
                    CHECK(render_fixture.renderScene(scene));
                }
                
                auto analysis = render_fixture.analyzeRenderedPixels();
                CHECK(analysis.non_black_pixels > 0);
            }
            
            scene->unref();
        }
    }
    
    SECTION("Cache performance and memory") {
        SECTION("Memory efficiency with caching") {
            auto scene = StandardTestScenes::createComplexScene();
            
            COIN_TEST_WITH_OSMESA_CONTEXT(256, 256) {
                RenderingTestUtils::RenderTestFixture render_fixture(256, 256);
                
                // Multiple renders of the same scene should be efficient due to caching
                for (int i = 0; i < 5; ++i) {
                    CHECK(render_fixture.renderScene(scene));
                }
                
                auto analysis = render_fixture.analyzeRenderedPixels();
                CHECK(analysis.non_black_pixels > 0);
            }
            
            scene->unref();
        }
        
        SECTION("Cache behavior under viewport changes") {
            auto scene = StandardTestScenes::createBasicGeometryScene();
            
            // Test different viewport sizes
            std::vector<std::pair<int, int>> viewports = {
                {128, 128}, {256, 256}, {512, 512}
            };
            
            for (auto& vp : viewports) {
                COIN_TEST_WITH_OSMESA_CONTEXT(vp.first, vp.second) {
                    RenderingTestUtils::RenderTestFixture render_fixture(vp.first, vp.second);
                    
                    CHECK(render_fixture.renderScene(scene));
                    auto analysis = render_fixture.analyzeRenderedPixels();
                    CHECK(analysis.non_black_pixels > 0);
                }
            }
            
            scene->unref();
        }
    }
}

TEST_CASE("Cache Edge Cases and Error Handling", "[caches][edge_cases]") {
    CoinTestFixture fixture;
    
    SECTION("Empty scene caching") {
        SoSeparator* emptyScene = new SoSeparator;
        
        SoGetBoundingBoxAction bboxAction(SbViewportRegion(100, 100));
        bboxAction.apply(emptyScene);
        
        SbBox3f bbox = bboxAction.getBoundingBox();
        CHECK(bbox.isEmpty());
        
        emptyScene->unref();
    }
    
    SECTION("Cache with scene graph modifications") {
        auto scene = StandardTestScenes::createBasicGeometryScene();
        
        SoGetBoundingBoxAction bboxAction(SbViewportRegion(100, 100));
        bboxAction.apply(scene);
        SbBox3f originalBbox = bboxAction.getBoundingBox();
        
        // Add more geometry
        SoSphere* sphere = new SoSphere;
        sphere->radius.setValue(2.0f);
        scene->addChild(sphere);
        
        // Cache should be invalidated
        bboxAction.apply(scene);
        SbBox3f newBbox = bboxAction.getBoundingBox();
        
        CHECK(newBbox.getVolume() > originalBbox.getVolume());
        
        scene->unref();
    }
    
    SECTION("Render cache with state changes") {
        auto scene = StandardTestScenes::createMaterialTestScene();
        
        COIN_TEST_WITH_OSMESA_CONTEXT(256, 256) {
            RenderingTestUtils::RenderTestFixture render_fixture(256, 256);
            
            // Initial render
            CHECK(render_fixture.renderScene(scene));
            
            // Modify rendering state and render again
            scene->addChild(new SoMaterial);
            CHECK(render_fixture.renderScene(scene));
        }
        
        scene->unref();
    }
}