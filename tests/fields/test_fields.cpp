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
#include <Inventor/fields/SoSFVec4b.h>
#include <Inventor/fields/SoSFBool.h>
#include <Inventor/fields/SoSFFloat.h>
#include <Inventor/SoType.h>

using namespace CoinTestUtils;

// Tests for SoSFVec4b field (ported from src/fields/SoSFVec4b.cpp)
TEST_CASE("SoSFVec4b field tests", "[fields][SoSFVec4b]") {
    CoinTestFixture fixture;

    SECTION("initialized") {
        SoSFVec4b field;
        CHECK(SoSFVec4b::getClassTypeId() != SoType::badType());
        CHECK(field.getTypeId() != SoType::badType());
    }
}

// Tests for SoSFBool field (ported from src/fields/SoSFBool.cpp) 
TEST_CASE("SoSFBool field tests", "[fields][SoSFBool]") {
    CoinTestFixture fixture;

    SECTION("initialized") {
        SoSFBool field;
        CHECK(SoSFBool::getClassTypeId() != SoType::badType());
        CHECK(field.getTypeId() != SoType::badType());
    }
}

// Tests for SoSFFloat field (ported from src/fields/SoSFFloat.cpp)
TEST_CASE("SoSFFloat field tests", "[fields][SoSFFloat]") {
    CoinTestFixture fixture;

    SECTION("initialized") {
        SoSFFloat field;
        CHECK(SoSFFloat::getClassTypeId() != SoType::badType());
        CHECK(field.getTypeId() != SoType::badType());
    }
}