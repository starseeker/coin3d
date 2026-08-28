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
 * @file test_caches_suite.cpp
 * @brief Tests for Coin3D cache classes.
 *
 * Covers:
 *   SoBoundingBoxCache - construct with null state, set, getBox, isCenterSet
 *   SoNormalCache      - construct with null state, set, getNum, getNormals
 *   SoConvexDataCache  - class type check
 */

#include "../test_utils.h"

#include <Inventor/caches/SoBoundingBoxCache.h>
#include <Inventor/caches/SoNormalCache.h>
#include <Inventor/caches/SoConvexDataCache.h>
#include <Inventor/SbXfBox3f.h>
#include <Inventor/SbBox3f.h>
#include <Inventor/SbVec3f.h>

using namespace ObolTest;

TEST(CachesSuite, SoBoundingBoxCacheConstructWithNullState)
{
    SoBoundingBoxCache * cache = new SoBoundingBoxCache(nullptr);
    EXPECT_TRUE((cache != nullptr));
    delete cache;
}

TEST(CachesSuite, SoBoundingBoxCacheSetAndGetBoxIsNonEmpty)
{
    SoBoundingBoxCache * cache = new SoBoundingBoxCache(nullptr);
    SbBox3f inner(SbVec3f(-1.0f, -1.0f, -1.0f),
                  SbVec3f( 1.0f,  1.0f,  1.0f));
    SbXfBox3f xb(inner);
    cache->set(xb, FALSE, SbVec3f(0.0f, 0.0f, 0.0f));

    EXPECT_TRUE(!cache->getBox().isEmpty()) << "SoBoundingBoxCache: getBox should not be empty after set()";
    delete cache;
}

TEST(CachesSuite, SoBoundingBoxCacheIsCenterSetReturnsFALSEWhenNotSet)
{
    SoBoundingBoxCache * cache = new SoBoundingBoxCache(nullptr);
    SbBox3f inner(SbVec3f(-1.0f, -1.0f, -1.0f),
                  SbVec3f( 1.0f,  1.0f,  1.0f));
    SbXfBox3f xb(inner);
    cache->set(xb, FALSE, SbVec3f(0.0f, 0.0f, 0.0f));

    EXPECT_TRUE((cache->isCenterSet() == FALSE)) << "SoBoundingBoxCache: isCenterSet should be FALSE";
    delete cache;
}

TEST(CachesSuite, SoBoundingBoxCacheIsCenterSetReturnsTRUEWhenSet)
{
    SoBoundingBoxCache * cache = new SoBoundingBoxCache(nullptr);
    SbBox3f inner(SbVec3f(-1.0f, -1.0f, -1.0f),
                  SbVec3f( 1.0f,  1.0f,  1.0f));
    SbXfBox3f xb(inner);
    cache->set(xb, TRUE, SbVec3f(0.5f, 0.5f, 0.5f));

    EXPECT_TRUE((cache->isCenterSet() == TRUE)) << "SoBoundingBoxCache: isCenterSet should be TRUE when center was set";
    delete cache;
}

// -----------------------------------------------------------------------
// SoNormalCache: construct, set normals, getNum, getNormals
// -----------------------------------------------------------------------

TEST(CachesSuite, SoNormalCacheConstructWithNullState)
{
    SoNormalCache * cache = new SoNormalCache(nullptr);
    EXPECT_TRUE((cache != nullptr));
    delete cache;
}

TEST(CachesSuite, SoNormalCacheSet3NormalsGetNum3)
{
    SoNormalCache * cache = new SoNormalCache(nullptr);
    SbVec3f normals[3] = {
        SbVec3f(1.0f, 0.0f, 0.0f),
        SbVec3f(0.0f, 1.0f, 0.0f),
        SbVec3f(0.0f, 0.0f, 1.0f)
    };
    cache->set(3, normals);

    EXPECT_TRUE((cache->getNum() == 3)) << "SoNormalCache: getNum() should be 3 after set(3, ...)";
    delete cache;
}

TEST(CachesSuite, SoNormalCacheGetNormals0MatchesWhatWasSet)
{
    SoNormalCache * cache = new SoNormalCache(nullptr);
    SbVec3f normals[3] = {
        SbVec3f(1.0f, 0.0f, 0.0f),
        SbVec3f(0.0f, 1.0f, 0.0f),
        SbVec3f(0.0f, 0.0f, 1.0f)
    };
    cache->set(3, normals);

    const SbVec3f * got = cache->getNormals();
    EXPECT_TRUE((got != nullptr) &&
                (got[0] == SbVec3f(1.0f, 0.0f, 0.0f))) << "SoNormalCache: getNormals()[0] should match (1,0,0)";
    delete cache;
}

// -----------------------------------------------------------------------
// SoConvexDataCache: construct and destroy (class type not exposed)
// -----------------------------------------------------------------------

TEST(CachesSuite, SoConvexDataCacheCanConstructWithNullState)
{
    SoConvexDataCache * cache = new SoConvexDataCache(nullptr);
    EXPECT_TRUE((cache != nullptr)) << "SoConvexDataCache: failed to construct";
    delete cache;
}
