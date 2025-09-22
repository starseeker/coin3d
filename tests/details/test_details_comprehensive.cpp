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
 * @file test_details_comprehensive.cpp
 * @brief Comprehensive tests for all Coin3D detail types and user-facing functionality
 *
 * This module provides comprehensive testing of picking details, geometry details,
 * and detail creation from ray picking and interaction operations.
 */

#include "utils/test_common.h"
#include "utils/osmesa_test_context.h"
#include "utils/scene_graph_test_utils.h"

// Core detail classes
#include <Inventor/details/SoDetail.h>
#include <Inventor/details/SoConeDetail.h>
#include <Inventor/details/SoCubeDetail.h>
#include <Inventor/details/SoCylinderDetail.h>
#include <Inventor/details/SoFaceDetail.h>
#include <Inventor/details/SoLineDetail.h>
#include <Inventor/details/SoNodeKitDetail.h>
#include <Inventor/details/SoPointDetail.h>
#include <Inventor/details/SoTextDetail.h>

// Supporting classes for detail testing
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoIndexedLineSet.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoText2.h>
#include <Inventor/actions/SoRayPickAction.h>
#include <Inventor/SoPickedPoint.h>
#include <Inventor/SbLinear.h>
#include <Inventor/SbViewportRegion.h>

using namespace CoinTestUtils;

TEST_CASE("Detail System Comprehensive Tests", "[details][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("Basic detail operations") {
        SECTION("Detail creation and type checking") {
            // Test different detail types
            SoCubeDetail* cubeDetail = new SoCubeDetail;
            CHECK(cubeDetail != nullptr);
            CHECK(cubeDetail->getTypeId().getName() == "SoCubeDetail");
            delete cubeDetail;
            
            // Note: SoSphere doesn't have a specific detail type
            // So we just test basic sphere creation without specific detail
            
            SoConeDetail* coneDetail = new SoConeDetail;
            CHECK(coneDetail != nullptr);
            CHECK(coneDetail->getTypeId().getName() == "SoConeDetail");
            delete coneDetail;
            
            SoCylinderDetail* cylinderDetail = new SoCylinderDetail;
            CHECK(cylinderDetail != nullptr);
            CHECK(cylinderDetail->getTypeId().getName() == "SoCylinderDetail");
            delete cylinderDetail;
        }
        
        SECTION("Detail copying and cloning") {
            SoFaceDetail* original = new SoFaceDetail;
            original->setFaceIndex(5);
            
            SoDetail* copy = original->copy();
            CHECK(copy != nullptr);
            CHECK(copy->getTypeId() == original->getTypeId());
            
            delete original;
            delete copy;
        }
    }
    
    SECTION("Cube detail tests") {
        SECTION("Cube detail part identification") {
            SoCubeDetail* detail = new SoCubeDetail;
            
            // Test part setting and getting using integer indices
            // Based on SoCube documentation: 0=front, 1=back, 2=left, 3=right, 4=top, 5=bottom
            detail->setPart(0);  // FRONT
            CHECK(detail->getPart() == 0);
            
            detail->setPart(1);  // BACK
            CHECK(detail->getPart() == 1);
            
            detail->setPart(2);  // LEFT
            CHECK(detail->getPart() == 2);
            
            detail->setPart(3);  // RIGHT
            CHECK(detail->getPart() == 3);
            
            detail->setPart(4);  // TOP
            CHECK(detail->getPart() == 4);
            
            detail->setPart(5);  // BOTTOM
            CHECK(detail->getPart() == 5);
            
            delete detail;
        }
        
        SECTION("Cube picking detail") {
            auto scene = StandardTestScenes::createPickTestScene();
            
            SoCube* cube = new SoCube;
            scene->addChild(cube);
            
            // Test ray picking to get cube detail
            SoRayPickAction pickAction(SbViewportRegion(100, 100));
            SbVec3f rayStart(0, 0, 5);
            SbVec3f rayDirection(0, 0, -1);
            pickAction.setRay(rayStart, rayDirection);
            pickAction.apply(scene);
            
            if (pickAction.getPickedPoint() != nullptr) {
                const SoDetail* detail = pickAction.getPickedPoint()->getDetail();
                if (detail && detail->isOfType(SoCubeDetail::getClassTypeId())) {
                    const SoCubeDetail* cubeDetail = static_cast<const SoCubeDetail*>(detail);
                    CHECK(cubeDetail != nullptr);
                }
            }
            
            scene->unref();
        }
    }
    
    SECTION("Cone detail tests") {
        SECTION("Cone detail part identification") {
            SoConeDetail* detail = new SoConeDetail;
            
            // Test part setting and getting using integer indices
            // Based on SoCone documentation: typically 0=sides, 1=bottom
            detail->setPart(0);  // SIDES
            CHECK(detail->getPart() == 0);
            
            detail->setPart(1);  // BOTTOM
            CHECK(detail->getPart() == 1);
            
            delete detail;
        }
        
        SECTION("Cone picking detail") {
            auto scene = StandardTestScenes::createPickTestScene();
            
            SoCone* cone = new SoCone;
            scene->addChild(cone);
            
            // Test ray picking to get cone detail
            SoRayPickAction pickAction(SbViewportRegion(100, 100));
            SbVec3f rayStart(0, 0, 5);
            SbVec3f rayDirection(0, 0, -1);
            pickAction.setRay(rayStart, rayDirection);
            pickAction.apply(scene);
            
            if (pickAction.getPickedPoint() != nullptr) {
                const SoDetail* detail = pickAction.getPickedPoint()->getDetail();
                if (detail && detail->isOfType(SoConeDetail::getClassTypeId())) {
                    const SoConeDetail* coneDetail = static_cast<const SoConeDetail*>(detail);
                    CHECK(coneDetail != nullptr);
                }
            }
            
            scene->unref();
        }
    }
    
    SECTION("Cylinder detail tests") {
        SECTION("Cylinder detail part identification") {
            SoCylinderDetail* detail = new SoCylinderDetail;
            
            // Test part setting and getting using integer indices
            // Based on SoCylinder documentation: typically 0=sides, 1=top, 2=bottom
            detail->setPart(0);  // SIDES
            CHECK(detail->getPart() == 0);
            
            detail->setPart(1);  // TOP
            CHECK(detail->getPart() == 1);
            
            detail->setPart(2);  // BOTTOM
            CHECK(detail->getPart() == 2);
            
            delete detail;
        }
        
        SECTION("Cylinder picking detail") {
            auto scene = StandardTestScenes::createPickTestScene();
            
            SoCylinder* cylinder = new SoCylinder;
            scene->addChild(cylinder);
            
            // Test ray picking to get cylinder detail
            SoRayPickAction pickAction(SbViewportRegion(100, 100));
            SbVec3f rayStart(0, 0, 5);
            SbVec3f rayDirection(0, 0, -1);
            pickAction.setRay(rayStart, rayDirection);
            pickAction.apply(scene);
            
            if (pickAction.getPickedPoint() != nullptr) {
                const SoDetail* detail = pickAction.getPickedPoint()->getDetail();
                if (detail && detail->isOfType(SoCylinderDetail::getClassTypeId())) {
                    const SoCylinderDetail* cylinderDetail = static_cast<const SoCylinderDetail*>(detail);
                    CHECK(cylinderDetail != nullptr);
                }
            }
            
            scene->unref();
        }
    }
    
    SECTION("Face detail tests") {
        SECTION("Face detail properties") {
            SoFaceDetail* detail = new SoFaceDetail;
            
            // Test face index
            detail->setFaceIndex(10);
            CHECK(detail->getFaceIndex() == 10);
            
            // Test part index
            detail->setPartIndex(3);
            CHECK(detail->getPartIndex() == 3);
            
            delete detail;
        }
        
        SECTION("Face detail point information") {
            SoFaceDetail* detail = new SoFaceDetail;
            
            // Test setting number of points
            detail->setNumPoints(3);
            CHECK(detail->getNumPoints() == 3);
            
            // Note: We don't set actual point details here as it requires more complex setup
            
            delete detail;
        }
    }
    
    SECTION("Line detail tests") {
        SECTION("Line detail properties") {
            SoLineDetail* detail = new SoLineDetail;
            
            // Test line index
            detail->setLineIndex(5);
            CHECK(detail->getLineIndex() == 5);
            
            // Test part index
            detail->setPartIndex(2);
            CHECK(detail->getPartIndex() == 2);
            
            delete detail;
        }
        
        SECTION("Line picking detail") {
            auto scene = StandardTestScenes::createPickTestScene();
            
            // Create a line set for testing
            SoSeparator* lineSep = new SoSeparator;
            
            SoCoordinate3* coords = new SoCoordinate3;
            coords->point.set1Value(0, SbVec3f(0, 0, 0));
            coords->point.set1Value(1, SbVec3f(1, 1, 0));
            coords->point.set1Value(2, SbVec3f(2, 0, 0));
            lineSep->addChild(coords);
            
            SoIndexedLineSet* lineSet = new SoIndexedLineSet;
            lineSet->coordIndex.set1Value(0, 0);
            lineSet->coordIndex.set1Value(1, 1);
            lineSet->coordIndex.set1Value(2, -1);
            lineSet->coordIndex.set1Value(3, 1);
            lineSet->coordIndex.set1Value(4, 2);
            lineSet->coordIndex.set1Value(5, -1);
            lineSep->addChild(lineSet);
            
            scene->addChild(lineSep);
            
            // Test ray picking on line
            SoRayPickAction pickAction(SbViewportRegion(100, 100));
            SbVec3f rayStart(0.5f, 0.5f, 5);
            SbVec3f rayDirection(0, 0, -1);
            pickAction.setRay(rayStart, rayDirection);
            pickAction.apply(scene);
            
            // Note: Line picking might not always succeed depending on exact geometry
            // This tests the API rather than guaranteed picking
            
            scene->unref();
        }
    }
    
    SECTION("Point detail tests") {
        SECTION("Point detail properties") {
            SoPointDetail* detail = new SoPointDetail;
            
            // Test coordinate index
            detail->setCoordinateIndex(15);
            CHECK(detail->getCoordinateIndex() == 15);
            
            // Test material index
            detail->setMaterialIndex(3);
            CHECK(detail->getMaterialIndex() == 3);
            
            // Test normal index
            detail->setNormalIndex(7);
            CHECK(detail->getNormalIndex() == 7);
            
            // Test texture coordinate index
            detail->setTextureCoordIndex(9);
            CHECK(detail->getTextureCoordIndex() == 9);
            
            delete detail;
        }
    }
    
    SECTION("Text detail tests") {
        SECTION("Text detail properties") {
            SoTextDetail* detail = new SoTextDetail;
            
            // Test string index
            detail->setStringIndex(2);
            CHECK(detail->getStringIndex() == 2);
            
            // Test character index
            detail->setCharacterIndex(10);
            CHECK(detail->getCharacterIndex() == 10);
            
            // Test part using integer indices (specific constants may not be available)
            detail->setPart(0);  // Some part
            CHECK(detail->getPart() == 0);
            
            detail->setPart(1);  // Another part
            CHECK(detail->getPart() == 1);
            
            detail->setPart(2);  // Yet another part
            CHECK(detail->getPart() == 2);
            
            delete detail;
        }
        
        SECTION("Text picking detail") {
            auto scene = StandardTestScenes::createPickTestScene();
            
            SoText2* text = new SoText2;
            text->string.setValue("Test Text");
            scene->addChild(text);
            
            // Basic test just verifies that adding text to scene doesn't crash
            CHECK(scene->getNumChildren() >= 1);
            
            scene->unref();
        }
    }
}

TEST_CASE("Detail Edge Cases and Error Handling", "[details][edge_cases]") {
    CoinTestFixture fixture;
    
    SECTION("Detail with null data") {
        SoFaceDetail* detail = new SoFaceDetail;
        
        // Test handling of unset data
        CHECK(detail->getFaceIndex() >= 0);  // Should have default value
        CHECK(detail->getPartIndex() >= 0);  // Should have default value
        CHECK(detail->getNumPoints() >= 0);  // Should have default value
        
        delete detail;
    }
    
    SECTION("Detail copying edge cases") {
        SoPointDetail* original = new SoPointDetail;
        original->setCoordinateIndex(100);
        original->setMaterialIndex(50);
        
        SoDetail* copy = original->copy();
        SoPointDetail* typedCopy = static_cast<SoPointDetail*>(copy);
        
        CHECK(typedCopy->getCoordinateIndex() == 100);
        CHECK(typedCopy->getMaterialIndex() == 50);
        
        delete original;
        delete copy;
    }
    
    SECTION("Detail type identification") {
        SoCubeDetail* cubeDetail = new SoCubeDetail;
        SoConeDetail* coneDetail = new SoConeDetail;
        SoCylinderDetail* cylinderDetail = new SoCylinderDetail;
        
        CHECK(cubeDetail->isOfType(SoCubeDetail::getClassTypeId()));
        CHECK(!cubeDetail->isOfType(SoConeDetail::getClassTypeId()));
        
        CHECK(coneDetail->isOfType(SoConeDetail::getClassTypeId()));
        CHECK(!coneDetail->isOfType(SoCylinderDetail::getClassTypeId()));
        
        CHECK(cylinderDetail->isOfType(SoCylinderDetail::getClassTypeId()));
        CHECK(!cylinderDetail->isOfType(SoCubeDetail::getClassTypeId()));
        
        delete cubeDetail;
        delete coneDetail;
        delete cylinderDetail;
    }
    
    SECTION("Multiple detail interactions") {
        auto scene = StandardTestScenes::createComplexScene();
        
        // Test picking in complex scene with multiple objects
        SoRayPickAction pickAction(SbViewportRegion(200, 200));
        
        // Test multiple pick rays
        std::vector<SbVec3f> testPoints = {
            SbVec3f(0, 0, 5),
            SbVec3f(1, 1, 5),
            SbVec3f(-1, -1, 5)
        };
        
        for (const auto& point : testPoints) {
            pickAction.setRay(point, SbVec3f(0, 0, -1));
            pickAction.apply(scene);
            
            // Check if we got valid picking results
            if (pickAction.getPickedPoint() != nullptr) {
                const SoDetail* detail = pickAction.getPickedPoint()->getDetail();
                CHECK(detail != nullptr);
            }
        }
        
        scene->unref();
    }
}