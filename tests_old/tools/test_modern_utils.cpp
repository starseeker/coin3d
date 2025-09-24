/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 * 
 * This file contains tests for Phase 3 modernization utilities:
 * - std::optional for optional return values
 * - std::string_view for efficient string handling  
 * - Enhanced RAII patterns
 \**************************************************************************/

#include <iostream>
#include "utils/test_common.h"
#include <catch2/catch_test_macros.hpp>
#include <Inventor/tools/SbModernUtils.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/SbName.h>

using namespace CoinTestUtils;
using namespace SbModernUtils;

TEST_CASE("Modern utilities - std::optional support", "[tools][modern][optional]") {
    CoinTestFixture fixture;
    
    SECTION("findNodeByName returns nullopt for non-existent node") {
        auto result = findNodeByName(SbName("nonexistent"));
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result == std::nullopt);
    }
    
    SECTION("findNodeByName returns node when found") {
        // Create and name a node
        SoCube* cube = new SoCube;
        cube->setName("testCube");
        cube->ref();
        
        auto result = findNodeByName(SbName("testCube"));
        REQUIRE(result.has_value());
        REQUIRE(result.value() == cube);
        
        cube->unref();
    }
    
    SECTION("optional can be used in conditional statements") {
        SoCube* cube = new SoCube;
        cube->setName("conditionalTest");
        cube->ref();
        
        if (auto node = findNodeByName(SbName("conditionalTest"))) {
            REQUIRE(node.value() == cube);
        } else {
            FAIL("Node should have been found");
        }
        
        cube->unref();
    }
}

TEST_CASE("Modern utilities - std::string_view support", "[tools][modern][string_view]") {
    CoinTestFixture fixture;
    
    SECTION("nameEquals works with string literals") {
        SbName name("TestNode");
        REQUIRE(nameEquals(name, "TestNode"));
        REQUIRE_FALSE(nameEquals(name, "OtherNode"));
    }
    
    SECTION("nameEquals works with std::string") {
        SbName name("AnotherTest");
        std::string str = "AnotherTest";
        REQUIRE(nameEquals(name, str));
        
        std::string differentStr = "Different";
        REQUIRE_FALSE(nameEquals(name, differentStr));
    }
    
    SECTION("nameEquals handles empty names and strings") {
        SbName emptyName("");
        REQUIRE(nameEquals(emptyName, ""));
        REQUIRE(nameEquals(emptyName, std::string_view()));
        REQUIRE_FALSE(nameEquals(emptyName, "notEmpty"));
        
        SbName regularName("notEmpty");
        REQUIRE_FALSE(nameEquals(regularName, ""));
    }
    
    SECTION("nameEquals is efficient with string_view") {
        SbName name("EfficiencyTest");
        std::string longString = "This is a very long string that contains EfficiencyTest somewhere in it";
        std::string_view view = longString.substr(longString.find("EfficiencyTest"), 14);
	// The string_view doesn't seem to reliably survive going through
	// the Catch2 wrappers, so do the check up front and check the boolean
	// output.
        bool check = !nameEquals(name, view);
        REQUIRE_FALSE(check);
    }
}

TEST_CASE("Modern utilities - Enhanced RAII patterns", "[tools][modern][raii]") {
    CoinTestFixture fixture;
    
    SECTION("SoNodeRef automatically manages reference counting") {
        SoCube* cube = new SoCube;
        cube->ref(); // Start with 1 reference to keep it alive for testing
        int initialRefCount = cube->getRefCount();
        
        {
            SoNodeRef nodeRef(cube);
            REQUIRE(cube->getRefCount() == initialRefCount + 1);
            REQUIRE(nodeRef.get() == cube);
            REQUIRE(&(*nodeRef) == cube);
            REQUIRE(nodeRef->getTypeId() == SoCube::getClassTypeId());
        }
        
        // After nodeRef goes out of scope, ref count should be back to original
        REQUIRE(cube->getRefCount() == initialRefCount);
        cube->unref(); // Final cleanup - this will delete the object
    }
    
    SECTION("SoNodeRef can be moved") {
        SoCube* cube = new SoCube;
        cube->ref(); // Keep it alive
        int initialRefCount = cube->getRefCount();
        
        SoNodeRef nodeRef1(cube);
        REQUIRE(cube->getRefCount() == initialRefCount + 1);
        
        SoNodeRef nodeRef2 = std::move(nodeRef1);
        REQUIRE(cube->getRefCount() == initialRefCount + 1); // Still same count
        REQUIRE(nodeRef2.get() == cube);
        REQUIRE(nodeRef1.get() == nullptr); // Moved from
        
        cube->unref(); // Cleanup
    }
    
    SECTION("SoNodeRef release returns ownership") {
        SoCube* cube = new SoCube;
        int initialRefCount = cube->getRefCount();
        
        SoNodeRef nodeRef(cube);
        REQUIRE(cube->getRefCount() == initialRefCount + 1);
        
        SoNode* released = nodeRef.release();
        REQUIRE(released == cube);
        REQUIRE(nodeRef.get() == nullptr);
        REQUIRE(cube->getRefCount() == initialRefCount + 1); // Still referenced
        
        cube->unref(); // Manual cleanup after release
    }
    
    SECTION("makeNodeRef factory function works") {
        SoCube* cube = new SoCube;
        cube->ref(); // Start with 1 reference to keep it alive for testing
        int initialRefCount = cube->getRefCount();
        
        {
            auto nodeRef = makeNodeRef(cube);
            REQUIRE(cube->getRefCount() == initialRefCount + 1);
            REQUIRE(nodeRef.get() == cube);
        }
        
        REQUIRE(cube->getRefCount() == initialRefCount);
        cube->unref(); // Final cleanup - this will delete the object
    }
}

TEST_CASE("Modern utilities - RefCountedPtr template", "[tools][modern][ref_counted_ptr]") {
    CoinTestFixture fixture;
    
    SECTION("RefCountedPtr manages node references") {
        SoCube* cube = new SoCube;
        cube->ref(); // Start with 1 reference to keep it alive for testing
        int initialRefCount = cube->getRefCount();
        
        {
            RefCountedPtr<SoCube> ptr(cube);
            REQUIRE(cube->getRefCount() == initialRefCount + 1);
            REQUIRE(ptr.get() == cube);
            REQUIRE(&(*ptr) == cube);
            REQUIRE(ptr->getTypeId() == SoCube::getClassTypeId());
        }
        
        REQUIRE(cube->getRefCount() == initialRefCount);
        cube->unref(); // Final cleanup - this will delete the object
    }
    
    SECTION("RefCountedPtr can be moved") {
        SoCube* cube = new SoCube;
        cube->ref(); // Keep alive
        int initialRefCount = cube->getRefCount();
        
        RefCountedPtr<SoCube> ptr1(cube);
        REQUIRE(cube->getRefCount() == initialRefCount + 1);
        
        RefCountedPtr<SoCube> ptr2 = std::move(ptr1);
        REQUIRE(cube->getRefCount() == initialRefCount + 1); // Same count
        REQUIRE(ptr2.get() == cube);
        REQUIRE(ptr1.get() == nullptr); // Moved from
        
        cube->unref(); // Cleanup
    }
    
    SECTION("RefCountedPtr reset works correctly") {
        SoCube* cube1 = new SoCube;
        SoCube* cube2 = new SoCube;
        cube1->ref(); // Keep initial reference to test properly
        cube2->ref(); // Keep initial reference to test properly
        
        RefCountedPtr<SoCube> ptr(cube1);
        REQUIRE(cube1->getRefCount() == 2); // ptr + initial ref
        
        ptr.reset(cube2);
        REQUIRE(cube1->getRefCount() == 1); // Back to initial ref
        REQUIRE(cube2->getRefCount() == 2); // ptr + initial ref 
        REQUIRE(ptr.get() == cube2);
        
        // Clean up initial references
        cube1->unref(); // This will delete cube1
        cube2->unref(); // cube2 will be cleaned up by ptr destructor
    }
    
    SECTION("makeRefCountedPtr factory function works") {
        SoCube* cube = new SoCube;
        cube->ref(); // Start with 1 reference to keep it alive for testing
        int initialRefCount = cube->getRefCount();
        
        {
            auto ptr = makeRefCountedPtr(cube);
            REQUIRE(cube->getRefCount() == initialRefCount + 1);
            REQUIRE(ptr.get() == cube);
        }
        
        REQUIRE(cube->getRefCount() == initialRefCount);
        cube->unref(); // Final cleanup - this will delete the object
    }
}

TEST_CASE("Modern utilities - Integration test", "[tools][modern][integration]") {
    CoinTestFixture fixture;
    
    SECTION("Combining optional and RAII patterns") {
        // Create a scene with named nodes
        SoSeparator* root = new SoSeparator;
        root->ref();
        
        SoCube* cube = new SoCube;
        cube->setName("integrationCube");
        root->addChild(cube);
        
        // Use modern utilities to find and manage node
        if (auto foundNode = findNodeByName(SbName("integrationCube"))) {
            auto nodeRef = makeNodeRef(foundNode.value());
            REQUIRE(nodeRef.get() == cube);
            REQUIRE(nameEquals(SbName("integrationCube"), "integrationCube"));
        } else {
            FAIL("Should have found the node");
        }
        
        root->unref();
    }
}
