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

using namespace SimpleTest;

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // -----------------------------------------------------------------------
    // SoBoundingBoxCache: construct, set, getBox, isCenterSet
    // Note: state=nullptr is used since we are not inside an action traversal.
    // The cache is immediately invalidated; we only test the accessors.
    // -----------------------------------------------------------------------
    runner.startTest("SoBoundingBoxCache: construct with null state");
    {
        SoBoundingBoxCache * cache = new SoBoundingBoxCache(nullptr);
        bool pass = (cache != nullptr);
        delete cache;
        runner.endTest(pass, "");
    }

    runner.startTest("SoBoundingBoxCache: set and getBox is non-empty");
    {
        SoBoundingBoxCache * cache = new SoBoundingBoxCache(nullptr);
        SbBox3f inner(SbVec3f(-1.0f, -1.0f, -1.0f),
                      SbVec3f( 1.0f,  1.0f,  1.0f));
        SbXfBox3f xb(inner);
        cache->set(xb, FALSE, SbVec3f(0.0f, 0.0f, 0.0f));

        bool pass = !cache->getBox().isEmpty();
        delete cache;
        runner.endTest(pass, pass ? "" :
            "SoBoundingBoxCache: getBox should not be empty after set()");
    }

    runner.startTest("SoBoundingBoxCache: isCenterSet returns FALSE when not set");
    {
        SoBoundingBoxCache * cache = new SoBoundingBoxCache(nullptr);
        SbBox3f inner(SbVec3f(-1.0f, -1.0f, -1.0f),
                      SbVec3f( 1.0f,  1.0f,  1.0f));
        SbXfBox3f xb(inner);
        cache->set(xb, FALSE, SbVec3f(0.0f, 0.0f, 0.0f));

        bool pass = (cache->isCenterSet() == FALSE);
        delete cache;
        runner.endTest(pass, pass ? "" :
            "SoBoundingBoxCache: isCenterSet should be FALSE");
    }

    runner.startTest("SoBoundingBoxCache: isCenterSet returns TRUE when set");
    {
        SoBoundingBoxCache * cache = new SoBoundingBoxCache(nullptr);
        SbBox3f inner(SbVec3f(-1.0f, -1.0f, -1.0f),
                      SbVec3f( 1.0f,  1.0f,  1.0f));
        SbXfBox3f xb(inner);
        cache->set(xb, TRUE, SbVec3f(0.5f, 0.5f, 0.5f));

        bool pass = (cache->isCenterSet() == TRUE);
        delete cache;
        runner.endTest(pass, pass ? "" :
            "SoBoundingBoxCache: isCenterSet should be TRUE when center was set");
    }

    // -----------------------------------------------------------------------
    // SoNormalCache: construct, set normals, getNum, getNormals
    // -----------------------------------------------------------------------
    runner.startTest("SoNormalCache: construct with null state");
    {
        SoNormalCache * cache = new SoNormalCache(nullptr);
        bool pass = (cache != nullptr);
        delete cache;
        runner.endTest(pass, "");
    }

    runner.startTest("SoNormalCache: set(3, normals) → getNum() == 3");
    {
        SoNormalCache * cache = new SoNormalCache(nullptr);
        SbVec3f normals[3] = {
            SbVec3f(1.0f, 0.0f, 0.0f),
            SbVec3f(0.0f, 1.0f, 0.0f),
            SbVec3f(0.0f, 0.0f, 1.0f)
        };
        cache->set(3, normals);

        bool pass = (cache->getNum() == 3);
        delete cache;
        runner.endTest(pass, pass ? "" :
            "SoNormalCache: getNum() should be 3 after set(3, ...)");
    }

    runner.startTest("SoNormalCache: getNormals()[0] matches what was set");
    {
        SoNormalCache * cache = new SoNormalCache(nullptr);
        SbVec3f normals[3] = {
            SbVec3f(1.0f, 0.0f, 0.0f),
            SbVec3f(0.0f, 1.0f, 0.0f),
            SbVec3f(0.0f, 0.0f, 1.0f)
        };
        cache->set(3, normals);

        const SbVec3f * got = cache->getNormals();
        bool pass = (got != nullptr) &&
                    (got[0] == SbVec3f(1.0f, 0.0f, 0.0f));
        delete cache;
        runner.endTest(pass, pass ? "" :
            "SoNormalCache: getNormals()[0] should match (1,0,0)");
    }

    // -----------------------------------------------------------------------
    // SoConvexDataCache: construct and destroy (class type not exposed)
    // -----------------------------------------------------------------------
    runner.startTest("SoConvexDataCache: can construct with null state");
    {
        SoConvexDataCache * cache = new SoConvexDataCache(nullptr);
        bool pass = (cache != nullptr);
        delete cache;
        runner.endTest(pass, pass ? "" :
            "SoConvexDataCache: failed to construct");
    }

    return runner.getSummary();
}
