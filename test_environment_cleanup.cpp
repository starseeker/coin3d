/*!
 * \file test_environment_cleanup.cpp
 * \brief Test for environment variable cleanup
 * 
 * This test verifies that the new CoinInternal environment variable utilities
 * work correctly and are compatible with the old coin_getenv/coin_setenv functions.
 */

#include "misc/SoEnvironment.h"
#include <Inventor/C/tidbits.h>
#include <iostream>
#include <cassert>
#include <cstdlib>

int main() {
    std::cout << "Testing environment variable cleanup..." << std::endl;
    
    // Test 1: Basic functionality with a test environment variable
    const std::string testVar = "COIN_TEST_CLEANUP_VAR";
    const std::string testValue = "test_value_123";
    
    // Set via new interface
    bool setResult = CoinInternal::setEnvironmentVariable(testVar, testValue);
    assert(setResult && "Failed to set environment variable");
    
    // Read via new interface
    auto optValue = CoinInternal::getEnvironmentVariable(testVar);
    assert(optValue.has_value() && "Environment variable should be set");
    assert(optValue.value() == testValue && "Value should match what was set");
    
    // Read via compatibility interface
    const char* rawValue = CoinInternal::getEnvironmentVariableRaw(testVar);
    assert(rawValue != nullptr && "Raw interface should return non-null");
    assert(std::string(rawValue) == testValue && "Raw value should match");
    
    // Read via old interface for compatibility
    const char* oldValue = coin_getenv(testVar.c_str());
    assert(oldValue != nullptr && "Old interface should still work");
    assert(std::string(oldValue) == testValue && "Old interface value should match");
    
    // Test 2: Default value functionality
    std::string defaultValue = CoinInternal::getEnvironmentVariable("NONEXISTENT_VAR", "default");
    assert(defaultValue == "default" && "Should return default value for non-existent variable");
    
    // Test 3: Optional functionality for non-existent variable
    auto nonExistent = CoinInternal::getEnvironmentVariable("NONEXISTENT_VAR");
    assert(!nonExistent.has_value() && "Should return nullopt for non-existent variable");
    
    // Clean up
    #ifdef _WIN32
    _putenv_s(testVar.c_str(), "");
    #else
    unsetenv(testVar.c_str());
    #endif
    
    std::cout << "All environment variable tests passed!" << std::endl;
    return 0;
}