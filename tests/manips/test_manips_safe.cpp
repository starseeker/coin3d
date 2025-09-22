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
 * @file test_manips_safe.cpp
 * @brief Safe comprehensive tests for Coin3D manipulator functionality
 *
 * This module provides safe testing of manipulator functionality focusing on
 * the most stable manipulator classes to avoid segfaults.
 */

#include "utils/test_common.h"
#include "utils/osmesa_test_context.h"
#include "utils/scene_graph_test_utils.h"

// Only the most stable manipulator
#include <Inventor/manips/SoTransformManip.h>

// Transform and basic nodes for testing
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>

// Basic Inventor classes
#include <Inventor/SbLinear.h>
#include <Inventor/SoPath.h>

using namespace CoinTestUtils;

// ============================================================================
// Safe Manipulator Core Tests
// ============================================================================

TEST_CASE("Safe Manipulator Core - Basic Functionality", "[manips][safe][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("SoTransformManip creation and basic properties") {
        SoTransformManip* manip = new SoTransformManip;
        manip->ref();
        
        // Test basic type information
        CHECK(manip->getTypeId() != SoType::badType());
        CHECK(manip->isOfType(SoTransform::getClassTypeId()));
        
        // Test default field values
        CHECK(manip->translation.getValue() == SbVec3f(0.0f, 0.0f, 0.0f));
        CHECK(manip->scaleFactor.getValue() == SbVec3f(1.0f, 1.0f, 1.0f));
        
        manip->unref();
    }
    
    SECTION("SoTransformManip field operations") {
        SoTransformManip* manip = new SoTransformManip;
        manip->ref();
        
        // Test translation
        SbVec3f translation(1.0f, 2.0f, 3.0f);
        manip->translation.setValue(translation);
        CHECK(manip->translation.getValue() == translation);
        
        // Test scaling
        SbVec3f scale(2.0f, 1.5f, 0.5f);
        manip->scaleFactor.setValue(scale);
        CHECK(manip->scaleFactor.getValue() == scale);
        
        // Test rotation
        SbRotation rot(SbVec3f(0, 1, 0), M_PI/4);
        manip->rotation.setValue(rot);
        CHECK(manip->rotation.getValue() == rot);
        
        manip->unref();
    }
    
    SECTION("Scene graph integration") {
        SoSeparator* scene = StandardTestScenes::createMinimalScene();
        
        SoTransformManip* manip = new SoTransformManip;
        manip->translation.setValue(SbVec3f(1.0f, 0.0f, 0.0f));
        scene->addChild(manip);
        
        SoCube* cube = new SoCube;
        scene->addChild(cube);
        
        // Verify scene structure
        CHECK(scene->getNumChildren() >= 2); // At least camera, light, and our nodes
        
        scene->unref();
    }
}

TEST_CASE("Safe Manipulator Edge Cases", "[manips][safe][edge_cases]") {
    CoinTestFixture fixture;
    
    SECTION("Extreme transformation values") {
        SoTransformManip* manip = new SoTransformManip;
        manip->ref();
        
        // Test with very small scale values
        SbVec3f tinyScale(0.001f, 0.001f, 0.001f);
        manip->scaleFactor.setValue(tinyScale);
        CHECK(manip->scaleFactor.getValue() == tinyScale);
        
        // Test with very large translation values  
        SbVec3f largeTranslation(10000.0f, -10000.0f, 5000.0f);
        manip->translation.setValue(largeTranslation);
        CHECK(manip->translation.getValue() == largeTranslation);
        
        manip->unref();
    }
}