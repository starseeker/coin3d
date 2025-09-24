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
#include <Inventor/SoType.h>
#include <Inventor/SbName.h>
#include <Inventor/nodes/SoNode.h>

using namespace CoinTestUtils;

// Helper function for SoType tests (ported from src/misc/SoType.cpp)
static void * createInstance(void)
{
    return (void *)0x1234;
}

// Tests for SoType class (ported from src/misc/SoType.cpp)
TEST_CASE("SoType tests", "[misc][SoType]") {

    SECTION("basic type operations") {
        // Test basic SoType functionality without createType/removeType
        SoType badType = SoType::badType();
        CHECK(badType == SoType::badType());

        SoType nodeType = SoNode::getClassTypeId();
        CHECK(nodeType != SoType::badType());
    }

    SECTION("testRemoveType") {
        // Test each step one by one to isolate the issue

        // Step 1: Create SbName
        SbName className("MyClass");
        CHECK(true); // If we get here, SbName creation is OK

        // Try to get badType first to ensure SoType system is working
        SoType badType = SoType::badType();
        CHECK(badType == SoType::badType()); // If we get here, basic SoType is OK

        // Step 2: Call SoType::fromName - THIS IS WHERE THE SEGFAULT HAPPENS
        SoType existing = SoType::fromName(className);

        // Step 3: Check if it's badType
        CHECK(existing == SoType::badType());

        SoType newtype = SoType::createType(SoNode::getClassTypeId(), className, createInstance, 0);
        (void)newtype; // Variable needed for type registration but not used afterwards
        CHECK(SoType::fromName(className) != SoType::badType());

        bool success = SoType::removeType(className);
        CHECK(success);
        CHECK(SoType::fromName(className) == SoType::badType());
    }
}
