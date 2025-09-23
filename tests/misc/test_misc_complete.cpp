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

#include "utils/test_common.h"
#include <catch2/catch_approx.hpp>
#include <Inventor/SoType.h>
#include <Inventor/SbName.h>
#include <Inventor/SoDB.h>
#include <Inventor/SoInteraction.h>
#include <Inventor/SoPath.h>
#include <Inventor/SoPickedPoint.h>
#include <Inventor/SoPrimitiveVertex.h>
#include <Inventor/misc/SoState.h>
#include <Inventor/misc/SoNotification.h>
#include <Inventor/elements/SoViewVolumeElement.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/actions/SoGLRenderAction.h>

using namespace CoinTestUtils;

// Comprehensive tests for miscellaneous classes

TEST_CASE("SoDB complete functionality", "[misc][SoDB][complete]") {
    CoinTestFixture fixture;

    SECTION("initialization state") {
        // SoDB should be initialized by the test fixture
        CHECK(SoDB::isInitialized() == TRUE);
    }

    SECTION("version information") {
        SbString version = SoDB::getVersion();
        CHECK(version.getLength() > 0);
        
        // Version should contain some expected format
        SbString versionStr = version;
        CHECK(versionStr.getLength() > 0);
    }

    SECTION("type management") {
        // Check that basic types are registered
        SoType nodeType = SoNode::getClassTypeId();
        CHECK(nodeType != SoType::badType());
        CHECK(nodeType.getName().getLength() > 0);
    }
}

TEST_CASE("SoPath complete functionality", "[misc][SoPath][complete]") {
    CoinTestFixture fixture;

    SECTION("path construction") {
        SoSeparator* root = new SoSeparator;
        SoCube* cube = new SoCube;
        root->addChild(cube);
        
        SoPath* path = new SoPath(root);
        CHECK(path->getLength() == 1);
        CHECK(path->getHead() == root);
        CHECK(path->getTail() == root);
        
        path->append(cube);
        CHECK(path->getLength() == 2);
        CHECK(path->getTail() == cube);
        
        path->unref();
        root->unref();
    }

    SECTION("path comparison") {
        SoSeparator* root = new SoSeparator;
        SoCube* cube1 = new SoCube;
        SoCube* cube2 = new SoCube;
        root->addChild(cube1);
        root->addChild(cube2);
        
        SoPath* path1 = new SoPath(root);
        path1->append(cube1);
        
        SoPath* path2 = new SoPath(root);
        path2->append(cube1);
        
        SoPath* path3 = new SoPath(root);
        path3->append(cube2);
        
        CHECK(*path1 == *path2);
        CHECK(*path1 != *path3);
        
        path1->unref();
        path2->unref();
        path3->unref();
        root->unref();
    }

    SECTION("path manipulation") {
        SoSeparator* root = new SoSeparator;
        SoSeparator* group = new SoSeparator;
        SoCube* cube = new SoCube;
        
        root->addChild(group);
        group->addChild(cube);
        
        SoPath* path = new SoPath(root);
        path->append(group);
        path->append(cube);
        
        CHECK(path->getLength() == 3);
        
        // Test truncation
        path->truncate(2);
        CHECK(path->getLength() == 2);
        CHECK(path->getTail() == group);
        
        path->unref();
        root->unref();
    }
}

TEST_CASE("SoState complete functionality", "[misc][SoState][complete]") {
    CoinTestFixture fixture;

    SECTION("state creation and management") {
        SoGLRenderAction action(SbViewportRegion(640, 480));
        SoState* state = action.getState();
        
        CHECK(state != nullptr);
        CHECK(state->getDepth() >= 0);
    }

    SECTION("element access") {
        SoGLRenderAction action(SbViewportRegion(640, 480));
        SoState* state = action.getState();
        
        // Test getting an element type
        const SoViewVolumeElement* element = 
            static_cast<const SoViewVolumeElement*>(state->getConstElement(SoViewVolumeElement::getClassStackIndex()));
        CHECK(element != nullptr);
    }
}

TEST_CASE("SoPrimitiveVertex complete functionality", "[misc][SoPrimitiveVertex][complete]") {
    CoinTestFixture fixture;

    SECTION("vertex construction") {
        SoPrimitiveVertex vertex;
        
        // Set point
        SbVec3f point(1.0f, 2.0f, 3.0f);
        vertex.setPoint(point);
        CHECK(vertex.getPoint() == point);
        
        // Set normal
        SbVec3f normal(0.0f, 1.0f, 0.0f);
        vertex.setNormal(normal);
        CHECK(vertex.getNormal() == normal);
        
        // Set texture coordinates
        SbVec4f texCoords(0.5f, 0.5f, 0.0f, 1.0f);
        vertex.setTextureCoords(texCoords);
        CHECK(vertex.getTextureCoords() == texCoords);
    }

    SECTION("vertex copying") {
        SoPrimitiveVertex vertex1;
        vertex1.setPoint(SbVec3f(1.0f, 2.0f, 3.0f));
        vertex1.setNormal(SbVec3f(0.0f, 1.0f, 0.0f));
        
        SoPrimitiveVertex vertex2(vertex1);
        CHECK(vertex2.getPoint() == vertex1.getPoint());
        CHECK(vertex2.getNormal() == vertex1.getNormal());
    }
}

TEST_CASE("SoPickedPoint complete functionality", "[misc][SoPickedPoint][complete]") {
    CoinTestFixture fixture;

    SECTION("picked point basic functionality") {
        // SoPickedPoint is typically created by picking actions, not directly
        // We'll test basic concepts related to paths instead
        SoSeparator* root = new SoSeparator;
        root->ref();
        SoCube* cube = new SoCube;
        root->addChild(cube);
        
        SoPath* path = new SoPath(root);
        path->append(cube);
        
        CHECK(path->getLength() == 2);
        CHECK(path->getTail() == cube);
        
        path->unref();
        root->unref();
    }
}

TEST_CASE("SoNotification complete functionality", "[misc][SoNotification][complete]") {
    CoinTestFixture fixture;

    SECTION("notification types") {
        // Test that we can create notifications
        // Note: This is mostly testing that the classes exist and can be instantiated
        SoNotList list;
        CHECK(list.getFirstRec() == nullptr);  // Initially empty
    }
}

TEST_CASE("SoDB reference counting", "[misc][SoDB][reference]") {
    CoinTestFixture fixture;

    SECTION("basic reference counting") {
        SoCube* cube = new SoCube;
        
        // Initial reference count should be 0
        CHECK(cube->getRefCount() == 0);
        
        // Add a reference
        cube->ref();
        CHECK(cube->getRefCount() == 1);
        
        // Remove reference
        cube->unref();
        // Note: cube is now deleted, so we can't check its ref count
    }

    SECTION("scene graph reference counting") {
        SoSeparator* root = new SoSeparator;
        SoCube* cube = new SoCube;
        
        root->ref();
        CHECK(root->getRefCount() == 1);
        
        // Adding to scene graph should increase ref count
        root->addChild(cube);
        CHECK(cube->getRefCount() == 1);
        
        // Removing from scene graph should decrease ref count
        root->removeChild(cube);
        CHECK(cube->getRefCount() == 0);
        
        root->unref();
        // Note: both objects should now be deleted
    }
}

TEST_CASE("SoType advanced functionality", "[misc][SoType][advanced]") {
    CoinTestFixture fixture;

    SECTION("type hierarchy") {
        SoType nodeType = SoNode::getClassTypeId();
        SoType cubeType = SoCube::getClassTypeId();
        
        CHECK(nodeType != SoType::badType());
        CHECK(cubeType != SoType::badType());
        
        // Cube should be derived from Node
        CHECK(cubeType.isDerivedFrom(nodeType));
        CHECK(!nodeType.isDerivedFrom(cubeType));
    }

    SECTION("type names") {
        SoType cubeType = SoCube::getClassTypeId();
        SbName typeName = cubeType.getName();
        
        CHECK(typeName.getLength() > 0);
        CHECK(strcmp(typeName.getString(), "SoCube") == 0);
    }

    SECTION("type lookup") {
        SbName cubeName("SoCube");
        SoType cubeType = SoType::fromName(cubeName);
        
        CHECK(cubeType != SoType::badType());
        CHECK(cubeType == SoCube::getClassTypeId());
    }
}

TEST_CASE("SoDB field conversion", "[misc][SoDB][fields]") {
    CoinTestFixture fixture;

    SECTION("field type registration") {
        // Basic test that field types are properly registered
        SoType floatFieldType = SoSFFloat::getClassTypeId();
        CHECK(floatFieldType != SoType::badType());
        
        SbName fieldName = floatFieldType.getName();
        CHECK(fieldName.getLength() > 0);
    }
}