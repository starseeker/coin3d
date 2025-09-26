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
 * @file test_nodekits_comprehensive.cpp
 * @brief Comprehensive tests for Coin3D nodekit functionality
 *
 * This module provides comprehensive testing of nodekit creation, part management,
 * catalog operations, and scene graph integration.
 */

#include "utils/test_common.h"
#include "utils/scene_graph_test_utils.h"

// Core nodekits
#include <Inventor/nodekits/SoBaseKit.h>
#include <Inventor/nodekits/SoNodeKit.h>
#include <Inventor/nodekits/SoShapeKit.h>
#include <Inventor/nodekits/SoAppearanceKit.h>
#include <Inventor/nodekits/SoSeparatorKit.h>
#include <Inventor/nodekits/SoWrapperKit.h>

// Supporting nodes
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoMaterial.h>

using namespace CoinTestUtils;

// ============================================================================
// Core NodeKit Tests
// ============================================================================

TEST_CASE("Core NodeKits - Basic Functionality", "[nodekits][core][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("SoBaseKit creation and properties") {
        SoBaseKit* kit = new SoBaseKit;
        kit->ref();
        
        // Test basic type information
        CHECK(kit->getTypeId() != SoType::badType());
        CHECK(kit->isOfType(SoNode::getClassTypeId()));
        
        // Test that it has a catalog
        const SoNodekitCatalog* catalog = kit->getNodekitCatalog();
        CHECK(catalog != nullptr);
        
        kit->unref();
    }
    
    SECTION("SoShapeKit creation and properties") {
        SoShapeKit* kit = new SoShapeKit;
        kit->ref();
        
        // Test basic properties
        CHECK(kit->getTypeId() != SoType::badType());
        CHECK(kit->isOfType(SoBaseKit::getClassTypeId()));
        
        // Test catalog access
        const SoNodekitCatalog* catalog = kit->getNodekitCatalog();
        CHECK(catalog != nullptr);
        CHECK(catalog->getNumEntries() > 0);
        
        kit->unref();
    }
    
    SECTION("SoAppearanceKit creation and properties") {
        SoAppearanceKit* kit = new SoAppearanceKit;
        kit->ref();
        
        // Test basic properties
        CHECK(kit->getTypeId() != SoType::badType());
        CHECK(kit->isOfType(SoBaseKit::getClassTypeId()));
        
        kit->unref();
    }
    
    SECTION("SoSeparatorKit creation and properties") {
        SoSeparatorKit* kit = new SoSeparatorKit;
        kit->ref();
        
        // Test basic properties
        CHECK(kit->getTypeId() != SoType::badType());
        CHECK(kit->isOfType(SoBaseKit::getClassTypeId()));
        
        kit->unref();
    }
}

TEST_CASE("NodeKit Part Management", "[nodekits][parts][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("ShapeKit part management") {
        SoShapeKit* kit = new SoShapeKit;
        kit->ref();
        
        // Test getting parts
        SoNode* shape = kit->getPart("shape", FALSE);
        // Shape may be null initially, that's okay
        (void)shape; // Suppress unused variable warning
        
        // Test setting a shape part
        SoCube* cube = new SoCube;
        kit->setPart("shape", cube);
        
        // Verify the part was set
        SoNode* retrievedShape = kit->getPart("shape", FALSE);
        CHECK(retrievedShape != nullptr);
        CHECK(retrievedShape->isOfType(SoCube::getClassTypeId()));
        
        kit->unref();
    }
    
    SECTION("AppearanceKit part management") {
        SoAppearanceKit* kit = new SoAppearanceKit;
        kit->ref();
        
        // Test setting material part
        SoMaterial* material = new SoMaterial;
        material->diffuseColor.setValue(1.0f, 0.0f, 0.0f);
        kit->setPart("material", material);
        
        // Verify the part was set
        SoNode* retrievedMaterial = kit->getPart("material", FALSE);
        CHECK(retrievedMaterial != nullptr);
        CHECK(retrievedMaterial->isOfType(SoMaterial::getClassTypeId()));
        
        kit->unref();
    }
}

TEST_CASE("NodeKit Catalog Operations", "[nodekits][catalog][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("Basic catalog access") {
        SoShapeKit* kit = new SoShapeKit;
        kit->ref();
        
        // Test basic catalog functionality without complex enumeration
        const SoNodekitCatalog* catalog = kit->getNodekitCatalog();
        CHECK(catalog != nullptr);
        
        kit->unref();
    }
    
    SECTION("Basic catalog information") {
        SoShapeKit* kit = new SoShapeKit;
        kit->ref();
        
        // Test basic catalog access without complex operations
        const SoNodekitCatalog* catalog = kit->getNodekitCatalog();
        CHECK(catalog != nullptr);
        
        kit->unref();
    }
}

TEST_CASE("NodeKit Scene Graph Integration", "[nodekits][scene_graph][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("NodeKit in scene graph") {
        SoSeparator* scene = StandardTestScenes::createMinimalScene();
        
        SoShapeKit* kit = new SoShapeKit;
        SoCube* cube = new SoCube;
        kit->setPart("shape", cube);
        
        scene->addChild(kit);
        
        // Verify scene structure
        CHECK(scene->getNumChildren() >= 1);
        
        scene->unref();
    }
    
    SECTION("Multiple nodekits in scene") {
        SoSeparator* scene = StandardTestScenes::createMinimalScene();
        
        // Add different types of nodekits
        SoShapeKit* shapeKit = new SoShapeKit;
        SoCube* cube = new SoCube;
        shapeKit->setPart("shape", cube);
        scene->addChild(shapeKit);
        
        SoAppearanceKit* appearanceKit = new SoAppearanceKit;
        SoMaterial* material = new SoMaterial;
        material->diffuseColor.setValue(0.0f, 1.0f, 0.0f);
        appearanceKit->setPart("material", material);
        scene->addChild(appearanceKit);
        
        // Test that all kits are properly added
        CHECK(scene->getNumChildren() >= 2);
        
        scene->unref();
    }
}

TEST_CASE("NodeKit Edge Cases and Error Handling", "[nodekits][edge_cases][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("Invalid part names") {
        SoShapeKit* kit = new SoShapeKit;
        kit->ref();
        
        // Test getting a non-existent part
        SoNode* nonExistentPart = kit->getPart("nonexistent", FALSE);
        CHECK(nonExistentPart == nullptr);
        
        kit->unref();
    }
    
    SECTION("NULL part setting") {
        SoShapeKit* kit = new SoShapeKit;
        kit->ref();
        
        // Set a part first
        SoCube* cube = new SoCube;
        kit->setPart("shape", cube);
        
        // Verify it was set
        SoNode* shape = kit->getPart("shape", FALSE);
        CHECK(shape != nullptr);
        
        // Set the part to NULL (remove it)
        kit->setPart("shape", nullptr);
        
        // Verify it was removed
        SoNode* removedShape = kit->getPart("shape", FALSE);
        CHECK(removedShape == nullptr);
        
        kit->unref();
    }
}