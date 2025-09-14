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
#include <Inventor/SbBox3s.h>
#include <Inventor/SbBox3f.h>
#include <Inventor/SbBSPTree.h>

using namespace CoinTestUtils;

// Tests for SbBox3s class (ported from src/base/SbBox3s.cpp)
TEST_CASE("SbBox3s basic functionality", "[base][SbBox3s]") {
    CoinTestFixture fixture;

    SECTION("checkSize") {
        SbVec3s min(1, 2, 3);
        SbVec3s max(3, 4, 5);
        SbVec3s diff = max - min;
        SbBox3s box(min, max);

        CHECK(box.getSize() == diff);
    }

    SECTION("checkGetClosestPoint") {
        SbVec3f point(1524, 13794, 851);
        SbVec3s min(1557, 3308, 850);
        SbVec3s max(3113, 30157, 1886);

        SbBox3s box(min, max);
        SbVec3f expected(1557, 13794, 851);

        CHECK(box.getClosestPoint(point) == expected);

        SbVec3s sizes = box.getSize();
        SbVec3f expectedCenterQuery(sizes[0]/2.0f, sizes[1]/2.0f, max[2]);

        CHECK(box.getClosestPoint(box.getCenter()) == expectedCenterQuery);
    }
}

// Tests for SbBox3f class (ported from src/base/SbBox3f.cpp)
TEST_CASE("SbBox3f basic functionality", "[base][SbBox3f]") {
    CoinTestFixture fixture;

    SECTION("checkGetClosestPoint") {
        SbVec3f point(1524.0f, 13794.0f, 851.0f);
        SbVec3f min(1557.0f, 3308.0f, 850.0f);
        SbVec3f max(3113.0f, 30157.0f, 1886.0f);

        SbBox3f box(min, max);
        SbVec3f expected(1557.0f, 13794.0f, 851.0f);

        CHECK(box.getClosestPoint(point) == expected);

        SbVec3f sizes = box.getSize();
        SbVec3f expectedCenterQuery(sizes[0]/2.0f, sizes[1]/2.0f, max[2]);

        CHECK(box.getClosestPoint(box.getCenter()) == expectedCenterQuery);
    }
}

// Tests for SbBSPTree class (ported from src/base/SbBSPTree.cpp)
TEST_CASE("SbBSPTree basic functionality", "[base][SbBSPTree]") {
    CoinTestFixture fixture;

    SECTION("initialized") {
        SbBSPTree bsp;
        CHECK(bsp.numPoints() == 0);
    }
}