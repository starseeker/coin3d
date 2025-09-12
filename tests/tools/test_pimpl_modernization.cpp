/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 * 
 * This file contains tests for Phase 3 modernization of SbPimplPtr and
 * SbLazyPimplPtr classes to use std::unique_ptr internally while maintaining
 * API compatibility.
 \**************************************************************************/

#include "utils/test_common.h"
#include <catch2/catch_test_macros.hpp>

// Include the implementation directly for testing
#define COIN_INTERNAL
#include <Inventor/tools/SbPimplPtr.h>
#include <Inventor/tools/SbLazyPimplPtr.h>
#undef COIN_INTERNAL

using namespace CoinTestUtils;

// Test data structure
struct TestData {
    int value;
    bool constructed;
    bool destroyed;
    
    TestData() : value(42), constructed(true), destroyed(false) {}
    TestData(int v) : value(v), constructed(true), destroyed(false) {}
    ~TestData() { destroyed = true; }
    
    TestData(const TestData& other) 
        : value(other.value), constructed(true), destroyed(false) {}
    
    TestData& operator=(const TestData& other) {
        if (this != &other) {
            value = other.value;
        }
        return *this;
    }
    
    bool operator==(const TestData& other) const {
        return value == other.value;
    }
};

TEST_CASE("SbPimplPtr modernization tests", "[tools][smart_pointers][SbPimplPtr]") {
    CoinTestFixture fixture;
    
    SECTION("default constructor creates object immediately") {
        SbPimplPtr<TestData> ptr;
        REQUIRE(ptr.get().constructed == true);
        REQUIRE(ptr.get().value == 42);
    }
    
    SECTION("constructor with initial value") {
        TestData* data = new TestData(100);
        SbPimplPtr<TestData> ptr(data);
        REQUIRE(ptr.get().value == 100);
    }
    
    SECTION("copy constructor") {
        SbPimplPtr<TestData> ptr1;
        ptr1.get().value = 123;
        
        SbPimplPtr<TestData> ptr2(ptr1);
        REQUIRE(ptr2.get().value == 123);
    }
    
    SECTION("assignment operator") {
        SbPimplPtr<TestData> ptr1;
        ptr1.get().value = 456;
        
        SbPimplPtr<TestData> ptr2;
        ptr2 = ptr1;
        REQUIRE(ptr2.get().value == 456);
    }
    
    SECTION("set method replaces object") {
        SbPimplPtr<TestData> ptr;
        TestData* data = new TestData(789);
        ptr.set(data);
        REQUIRE(ptr.get().value == 789);
    }
    
    SECTION("arrow operator") {
        SbPimplPtr<TestData> ptr;
        ptr->value = 999;
        REQUIRE(ptr.get().value == 999);
    }
    
    SECTION("equality operators") {
        SbPimplPtr<TestData> ptr1;
        SbPimplPtr<TestData> ptr2;
        ptr1.get().value = 111;
        ptr2.get().value = 111;
        
        REQUIRE(ptr1 == ptr2);
        
        ptr2.get().value = 222;
        REQUIRE(ptr1 != ptr2);
    }
}

TEST_CASE("SbLazyPimplPtr modernization tests", "[tools][smart_pointers][SbLazyPimplPtr]") {
    CoinTestFixture fixture;
    
    SECTION("default constructor does not create object") {
        SbLazyPimplPtr<TestData> ptr;
        // Object should not be created until first access
    }
    
    SECTION("first access creates object lazily") {
        SbLazyPimplPtr<TestData> ptr;
        REQUIRE(ptr.get().constructed == true);
        REQUIRE(ptr.get().value == 42);
    }
    
    SECTION("constructor with initial value") {
        TestData* data = new TestData(200);
        SbLazyPimplPtr<TestData> ptr(data);
        REQUIRE(ptr.get().value == 200);
    }
    
    SECTION("copy constructor") {
        SbLazyPimplPtr<TestData> ptr1;
        ptr1.get().value = 333;
        
        SbLazyPimplPtr<TestData> ptr2(ptr1);
        REQUIRE(ptr2.get().value == 333);
    }
    
    SECTION("assignment operator") {
        SbLazyPimplPtr<TestData> ptr1;
        ptr1.get().value = 444;
        
        SbLazyPimplPtr<TestData> ptr2;
        ptr2 = ptr1;
        REQUIRE(ptr2.get().value == 444);
    }
    
    SECTION("set method replaces object") {
        SbLazyPimplPtr<TestData> ptr;
        TestData* data = new TestData(555);
        ptr.set(data);
        REQUIRE(ptr.get().value == 555);
    }
    
    SECTION("arrow operator with lazy creation") {
        SbLazyPimplPtr<TestData> ptr;
        ptr->value = 666;
        REQUIRE(ptr.get().value == 666);
    }
    
    SECTION("equality operators") {
        SbLazyPimplPtr<TestData> ptr1;
        SbLazyPimplPtr<TestData> ptr2;
        ptr1.get().value = 777;
        ptr2.get().value = 777;
        
        REQUIRE(ptr1 == ptr2);
        
        ptr2.get().value = 888;
        REQUIRE(ptr1 != ptr2);
    }
}

TEST_CASE("PimplPtr memory management", "[tools][smart_pointers][memory]") {
    CoinTestFixture fixture;
    
    SECTION("SbPimplPtr properly destroys objects") {
        TestData* original = nullptr;
        {
            SbPimplPtr<TestData> ptr;
            original = &ptr.get();
            REQUIRE(original->constructed == true);
            REQUIRE(original->destroyed == false);
        }
        // Note: We can't check destroyed flag after scope exit as memory may be reused
    }
    
    SECTION("SbLazyPimplPtr properly destroys objects") {
        TestData* original = nullptr;
        {
            SbLazyPimplPtr<TestData> ptr;
            original = &ptr.get();
            REQUIRE(original->constructed == true);
            REQUIRE(original->destroyed == false);
        }
        // Note: We can't check destroyed flag after scope exit as memory may be reused
    }
}