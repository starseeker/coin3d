/**************************************************************************\
 * Simple test to isolate the PimplPtr copy constructor issue
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
    TestData() : value(42) {}
    TestData(int v) : value(v) {}
    TestData(const TestData& other) : value(other.value) {}
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

TEST_CASE("SbPimplPtr basic test", "[tools][basic]") {
    CoinTestFixture fixture;
    
    SECTION("default constructor") {
        SbPimplPtr<TestData> ptr;
        REQUIRE(ptr.get().value == 42);
    }
}

TEST_CASE("SbLazyPimplPtr basic test", "[tools][basic]") {
    CoinTestFixture fixture;
    
    SECTION("lazy creation") {
        SbLazyPimplPtr<TestData> ptr;
        REQUIRE(ptr.get().value == 42);
    }
}