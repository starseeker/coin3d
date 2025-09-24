#ifndef SIMPLE_TEST_UTILS_H
#define SIMPLE_TEST_UTILS_H

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
 * @file test_utils.h
 * @brief Simple test utilities without external frameworks
 *
 * This provides basic test infrastructure to replace Catch2 for simpler,
 * more direct testing. Each test executable returns 0 for success, 
 * non-zero for failure.
 */

#include <iostream>
#include <string>
#include <vector>
#include <Inventor/SoDB.h>
#include <Inventor/SoInteraction.h>

namespace SimpleTest {

// Test result tracking
struct TestResult {
    std::string test_name;
    bool passed;
    std::string error_message;
};

class TestRunner {
private:
    std::vector<TestResult> results;
    std::string current_test_name;
    
public:
    void startTest(const std::string& name) {
        current_test_name = name;
        std::cout << "Running: " << name << "..." << std::flush;
    }
    
    void endTest(bool passed, const std::string& error_msg = "") {
        TestResult result;
        result.test_name = current_test_name;
        result.passed = passed;
        result.error_message = error_msg;
        results.push_back(result);
        
        if (passed) {
            std::cout << " PASSED" << std::endl;
        } else {
            std::cout << " FAILED";
            if (!error_msg.empty()) {
                std::cout << " - " << error_msg;
            }
            std::cout << std::endl;
        }
    }
    
    int getSummary() {
        int passed = 0, failed = 0;
        for (const auto& result : results) {
            if (result.passed) passed++;
            else failed++;
        }
        
        std::cout << "\nTest Summary: " << passed << " passed, " << failed << " failed";
        if (results.size() > 0) {
            std::cout << " (total: " << results.size() << ")";
        }
        std::cout << std::endl;
        
        return failed; // Return number of failures
    }
};

// Simple assertion macro
#define SIMPLE_ASSERT(condition, message) \
    if (!(condition)) { \
        runner.endTest(false, message); \
        continue; \
    }

#define SIMPLE_CHECK(condition, message) \
    if (!(condition)) { \
        runner.endTest(false, message); \
        return 1; \
    }

// Test fixture base class for initialization
class TestFixture {
public:
    TestFixture() {
        // Initialize Coin3D if not already initialized
        if (!SoDB::isInitialized()) {
            SoDB::init(nullptr);
            SoInteraction::init();
        }
    }
    
    virtual ~TestFixture() {
        // Cleanup if needed
    }
};

} // namespace SimpleTest

#endif // SIMPLE_TEST_UTILS_H