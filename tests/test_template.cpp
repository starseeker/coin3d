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

// Template for new test files
// Copy this file and rename to match your module: test_<module>.cpp

#include "utils/test_common.h"
#include <Inventor/SomeClass.h>  // Replace with actual class header

using namespace CoinTestUtils;

// Example test case - replace with actual tests
TEST_CASE("SomeClass functionality", "[module][SomeClass]") {
    CoinTestFixture fixture;  // Initialize Coin

    SECTION("basic initialization") {
        // Test basic class functionality
        // SomeClass obj;
        // CHECK(obj.isValid());
    }

    SECTION("specific behavior") {
        // Test specific functionality
        // CHECK(someCondition);
        // CHECK_THROWS(invalidOperation());
    }
}

// For testing multiple related classes in one file:
TEST_CASE("AnotherClass functionality", "[module][AnotherClass]") {
    CoinTestFixture fixture;

    SECTION("test name") {
        // Test code here
    }
}

// Tips for writing good tests:
// 1. Use descriptive test names that explain what is being tested
// 2. Use appropriate tags: [module][class] for organization  
// 3. Each SECTION should test one specific aspect
// 4. Use CHECK for non-fatal assertions, REQUIRE for fatal ones
// 5. Include edge cases and error conditions
// 6. Keep tests independent - don't rely on order of execution