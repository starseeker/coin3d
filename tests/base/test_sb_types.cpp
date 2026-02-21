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
 * @file test_sb_types.cpp
 * @brief Tests for Coin3D base types (SbVec*, SbBox*, SbMatrix, etc.)
 *
 * Tests are baselined against coin_vanilla COIN_TEST_SUITE blocks to verify
 * that behavior is consistent between vanilla Coin and this implementation.
 *
 * Vanilla sources covering these tests:
 *   src/base/SbVec3f.cpp   - toString, fromString
 *   src/base/SbBox2f.cpp   - checkSize, checkGetClosestPoint
 *   src/base/SbBox3f.cpp   - checkGetClosestPoint
 *   src/base/SbBox3i32.cpp - checkSize, checkGetClosestPoint
 *   src/base/SbByteBuffer.cpp - pushUnique, pushOnEmpty
 *   src/base/SbBSPTree.cpp - initialized (add/find/remove points)
 *   src/base/SbMatrix.cpp  - constructFromSbDPMatrix
 *   src/base/SbDPMatrix.cpp - constructFromSbMatrix
 *   src/base/SbRotation.cpp - toString, fromString, fromInvalidString
 *   src/base/SbString.cpp  - testAddition
 *   src/base/SbPlane.cpp   - signCorrect (plane-plane intersection)
 *   src/base/SbViewVolume.cpp - intersect_ortho, intersect_perspective
 */

#include "../test_utils.h"

#include <Inventor/SbVec2f.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbBox2f.h>
#include <Inventor/SbBox3f.h>
#include <Inventor/SbBox3i32.h>
#include <Inventor/SbByteBuffer.h>
#include <Inventor/SbBSPTree.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbDPMatrix.h>
#include <Inventor/SbRotation.h>
#include <Inventor/SbString.h>
#include <Inventor/SbPlane.h>
#include <Inventor/SbLine.h>
#include <Inventor/SbViewVolume.h>

#include <cmath>
#include <cstring>

using namespace SimpleTest;

// ---------------------------------------------------------------------------
// Floating-point comparison helper
// ---------------------------------------------------------------------------
static bool floatNear(float a, float b, float tol = 1e-5f) {
    return fabsf(a - b) <= tol;
}
static bool doubleNear(double a, double b, double tol = 1e-10) {
    return fabs(a - b) <= tol;
}

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // -----------------------------------------------------------------------
    // SbVec3f: fromString valid/invalid
    // Baseline: src/base/SbVec3f.cpp COIN_TEST_SUITE
    // Note: toString() uses SoSFVec3f internally which requires full DB init.
    //       Round-trip test is in tests that have full context.
    // -----------------------------------------------------------------------

    runner.startTest("SbVec3f fromString valid");
    {
        SbVec3f foo;
        SbString test = "0.333333343 -2 -3.0";
        SbVec3f trueVal(0.333333343f, -2.0f, -3.0f);
        SbBool ok = foo.fromString(test);
        bool pass = (ok == TRUE) && (trueVal == foo);
        runner.endTest(pass, pass ? "" :
            std::string("Mismatch: got '") + foo.toString().getString() +
            "' expected '" + trueVal.toString().getString() + "'");
    }

    runner.startTest("SbVec3f fromString invalid (non-numeric)");
    {
        SbVec3f foo;
        SbString test = "a 2 3";
        SbBool ok = foo.fromString(test);
        bool pass = (ok == FALSE);
        runner.endTest(pass, pass ? "" :
            "fromString should have returned FALSE for 'a 2 3'");
    }

    // -----------------------------------------------------------------------
    // SbBox2f: getSize / getClosestPoint
    // Baseline: src/base/SbBox2f.cpp COIN_TEST_SUITE
    // -----------------------------------------------------------------------
    runner.startTest("SbBox2f getSize");
    {
        SbVec2f lo(1.0f, 2.0f);
        SbVec2f hi(3.0f, 4.0f);
        SbVec2f expected = hi - lo;
        SbBox2f box(lo, hi);
        bool pass = (box.getSize() == expected);
        runner.endTest(pass, pass ? "" : "SbBox2f::getSize returned wrong value");
    }

    runner.startTest("SbBox2f getClosestPoint (outside)");
    {
        SbVec2f point(1524.0f, 13794.0f);
        SbVec2f lo(1557.0f, 3308.0f);
        SbVec2f hi(3113.0f, 30157.0f);
        SbBox2f box(lo, hi);
        SbVec2f expected(1557.0f, 13794.0f);
        bool pass = (box.getClosestPoint(point) == expected);
        runner.endTest(pass, pass ? "" : "SbBox2f::getClosestPoint wrong result for point outside box");
    }

    runner.startTest("SbBox2f getClosestPoint (center)");
    {
        SbVec2f lo(1557.0f, 3308.0f);
        SbVec2f hi(3113.0f, 30157.0f);
        SbBox2f box(lo, hi);
        SbVec2f sizes = box.getSize();
        SbVec2f expected(hi[0], sizes[1] / 2.0f);
        bool pass = (box.getClosestPoint(box.getCenter()) == expected);
        runner.endTest(pass, pass ? "" : "SbBox2f::getClosestPoint wrong result for center query");
    }

    // -----------------------------------------------------------------------
    // SbBox3f: getClosestPoint
    // Baseline: src/base/SbBox3f.cpp COIN_TEST_SUITE
    // -----------------------------------------------------------------------
    runner.startTest("SbBox3f getClosestPoint (outside)");
    {
        SbVec3f point(1524.0f, 13794.0f, 851.0f);
        SbVec3f lo(1557.0f, 3308.0f, 850.0f);
        SbVec3f hi(3113.0f, 30157.0f, 1886.0f);
        SbBox3f box(lo, hi);
        SbVec3f expected(1557.0f, 13794.0f, 851.0f);
        bool pass = (box.getClosestPoint(point) == expected);
        runner.endTest(pass, pass ? "" : "SbBox3f::getClosestPoint wrong result");
    }

    runner.startTest("SbBox3f getClosestPoint (center)");
    {
        SbVec3f lo(1557.0f, 3308.0f, 850.0f);
        SbVec3f hi(3113.0f, 30157.0f, 1886.0f);
        SbBox3f box(lo, hi);
        SbVec3f sizes = box.getSize();
        SbVec3f expected(sizes[0]/2.0f, sizes[1]/2.0f, hi[2]);
        bool pass = (box.getClosestPoint(box.getCenter()) == expected);
        runner.endTest(pass, pass ? "" : "SbBox3f::getClosestPoint wrong result for center query");
    }

    // -----------------------------------------------------------------------
    // SbBox3i32: getSize / getClosestPoint
    // Baseline: src/base/SbBox3i32.cpp COIN_TEST_SUITE
    // -----------------------------------------------------------------------
    runner.startTest("SbBox3i32 getSize");
    {
        SbVec3i32 lo(1, 2, 3);
        SbVec3i32 hi(3, 4, 5);
        SbVec3i32 expected = hi - lo;
        SbBox3i32 box(lo, hi);
        bool pass = (box.getSize() == expected);
        runner.endTest(pass, pass ? "" : "SbBox3i32::getSize returned wrong value");
    }

    runner.startTest("SbBox3i32 getClosestPoint (outside)");
    {
        SbVec3f  point(1524.0f, 13794.0f, 851.0f);
        SbVec3i32 lo(1557, 3308, 850);
        SbVec3i32 hi(3113, 30157, 1886);
        SbBox3i32 box(lo, hi);
        SbVec3f expected(1557.0f, 13794.0f, 851.0f);
        bool pass = (box.getClosestPoint(point) == expected);
        runner.endTest(pass, pass ? "" : "SbBox3i32::getClosestPoint wrong result");
    }

    // -----------------------------------------------------------------------
    // SbByteBuffer: push operations
    // Baseline: src/base/SbByteBuffer.cpp COIN_TEST_SUITE
    // -----------------------------------------------------------------------
    runner.startTest("SbByteBuffer pushUnique");
    {
        static const char A[] = "ABC";
        static const char B[] = "XYZ";
        SbByteBuffer a(3, A);
        SbByteBuffer b(3, B);
        SbByteBuffer c = a;
        c.push(b);

        bool sizeOk = (c.size() == 6);
        bool contentsOk = true;
        const char expected[] = "ABCXYZ";
        for (size_t i = 0; i < 6; ++i) {
            if (c[i] != expected[i]) { contentsOk = false; break; }
        }
        bool pass = sizeOk && contentsOk;
        runner.endTest(pass, pass ? "" :
            "SbByteBuffer::push gave wrong size or contents");
    }

    runner.startTest("SbByteBuffer push onto empty");
    {
        SbByteBuffer empty;
        SbByteBuffer content("foo");
        empty.push(content);
        bool pass = (empty.size() == content.size());
        runner.endTest(pass, pass ? "" :
            "SbByteBuffer push onto empty gave wrong size");
    }

    // -----------------------------------------------------------------------
    // SbBSPTree: add / find / remove points
    // Baseline: src/base/SbBSPTree.cpp COIN_TEST_SUITE
    // -----------------------------------------------------------------------
    runner.startTest("SbBSPTree add/find/remove");
    {
        SbBSPTree bsp;
        SbVec3f p0(0.0f, 0.0f, 0.0f);
        SbVec3f p1(1.0f, 0.0f, 0.0f);
        SbVec3f p2(2.0f, 0.0f, 0.0f);
        void* ud0 = reinterpret_cast<void*>(&p0);
        void* ud1 = reinterpret_cast<void*>(&p1);
        void* ud2 = reinterpret_cast<void*>(&p2);

        bool pass = true;
        if (bsp.addPoint(p0, ud0) != 0) { pass = false; }
        if (bsp.addPoint(p1, ud1) != 1) { pass = false; }
        if (bsp.addPoint(p2, ud2) != 2) { pass = false; }
        // re-adding same point returns existing index
        if (bsp.addPoint(p2, ud2) != 2) { pass = false; }
        if (bsp.numPoints() != 3)        { pass = false; }
        if (bsp.findPoint(p0) != 0)      { pass = false; }
        if (bsp.getUserData(0) != ud0)   { pass = false; }
        if (bsp.findPoint(p1) != 1)      { pass = false; }
        if (bsp.getUserData(1) != ud1)   { pass = false; }
        if (bsp.findPoint(p2) != 2)      { pass = false; }
        if (bsp.getUserData(2) != ud2)   { pass = false; }

        bsp.removePoint(p1);
        if (bsp.numPoints() != 2)        { pass = false; }
        bsp.removePoint(p0);
        bsp.removePoint(p2);
        if (bsp.numPoints() != 0)        { pass = false; }

        runner.endTest(pass, pass ? "" : "SbBSPTree add/find/remove failed");
    }

    // -----------------------------------------------------------------------
    // SbMatrix: construct from SbDPMatrix
    // Baseline: src/base/SbMatrix.cpp COIN_TEST_SUITE
    // -----------------------------------------------------------------------
    runner.startTest("SbMatrix construct from SbDPMatrix");
    {
        SbMatrixd a(0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15);
        float c[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
        SbMatrix b;
        b.setValue(c);
        SbMatrix d(a);
        bool pass = (b == d);
        runner.endTest(pass, pass ? "" : "SbMatrix construct from SbDPMatrix failed");
    }

    // -----------------------------------------------------------------------
    // SbDPMatrix: construct from SbMatrix
    // Baseline: src/base/SbDPMatrix.cpp COIN_TEST_SUITE
    // -----------------------------------------------------------------------
    runner.startTest("SbDPMatrix construct from SbMatrix");
    {
        SbMatrix a(0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15);
        double c[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
        SbDPMatrix b;
        b.setValue(c);
        SbDPMatrix d(a);
        bool pass = (b == d);
        runner.endTest(pass, pass ? "" : "SbDPMatrix construct from SbMatrix failed");
    }

    // -----------------------------------------------------------------------
    // SbRotation: toString / fromString / fromInvalidString
    // Baseline: src/base/SbRotation.cpp COIN_TEST_SUITE
    // -----------------------------------------------------------------------
    runner.startTest("SbRotation fromString valid");
    {
        SbRotation foo;
        SbString test = "0 -1 0 1";
        SbRotation trueVal(SbVec3f(0, -1, 0), 1.0f);
        SbBool ok = foo.fromString(test);
        bool pass = (ok == TRUE) && (trueVal == foo);
        runner.endTest(pass, pass ? "" :
            std::string("SbRotation fromString mismatch: got '") +
            foo.toString().getString() + "'");
    }

    runner.startTest("SbRotation fromString invalid");
    {
        SbRotation foo;
        SbString test = "2.- 2 3 4";
        SbBool ok = foo.fromString(test);
        bool pass = (ok == FALSE);
        runner.endTest(pass, pass ? "" :
            "SbRotation fromString should return FALSE for invalid input");
    }

    // -----------------------------------------------------------------------
    // SbString: operator+
    // Baseline: src/base/SbString.cpp COIN_TEST_SUITE
    // -----------------------------------------------------------------------
    runner.startTest("SbString operator+ (str+str)");
    {
        SbString s1("First");
        SbString s2("Second");
        SbString result = s1 + s2;
        bool pass = (result == SbString("FirstSecond"));
        runner.endTest(pass, pass ? "" :
            std::string("SbString operator+ got '") +
            result.getString() + "' expected 'FirstSecond'");
    }

    runner.startTest("SbString operator+ (cstr+str)");
    {
        const char* cstr = "Erste";
        SbString s2("Second");
        SbString result = cstr + s2;
        bool pass = (result == SbString("ErsteSecond"));
        runner.endTest(pass, pass ? "" :
            std::string("SbString cstr+str got '") +
            result.getString() + "' expected 'ErsteSecond'");
    }

    runner.startTest("SbString operator+ (str+cstr)");
    {
        SbString s1("First");
        const char* cstr = "Zweite";
        SbString result = s1 + cstr;
        bool pass = (result == SbString("FirstZweite"));
        runner.endTest(pass, pass ? "" :
            std::string("SbString str+cstr got '") +
            result.getString() + "' expected 'FirstZweite'");
    }

    // -----------------------------------------------------------------------
    // SbPlane: plane-plane intersection sign
    // Baseline: src/base/SbPlane.cpp COIN_TEST_SUITE
    // -----------------------------------------------------------------------
    runner.startTest("SbPlane intersect sign correct");
    {
        SbPlane plane1(SbVec3f(0.0f, 0.0f, 1.0f), 3.0f);
        SbPlane plane2(SbVec3f(1.0f, 0.0f, 0.0f), 21.0f);
        SbLine line;
        bool intersects = plane1.intersect(plane2, line);
        bool pass = intersects;
        if (pass) {
            SbVec3f pos = line.getPosition();
            SbVec3f expected(21.0f, 0.0f, 3.0f);
            pass = floatNear(pos[0], expected[0], 0.1f) &&
                   floatNear(pos[1], expected[1], 0.1f) &&
                   floatNear(pos[2], expected[2], 0.1f);
        }
        runner.endTest(pass, pass ? "" : "SbPlane intersect gave wrong position");
    }

    // -----------------------------------------------------------------------
    // SbViewVolume: intersectionBox for ortho/perspective
    // Baseline: src/base/SbViewVolume.cpp COIN_TEST_SUITE
    // -----------------------------------------------------------------------
    runner.startTest("SbViewVolume ortho intersect (partial overlap)");
    {
        SbViewVolume vv;
        vv.ortho(-0.5f, 0.5f, -0.5f, 0.5f, -1.0f, 10.0f);
        SbBox3f box(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
        SbBox3f isect = vv.intersectionBox(box);
        bool pass = floatNear(isect.getMin()[0], 0.0f) &&
                    floatNear(isect.getMin()[1], 0.0f) &&
                    floatNear(isect.getMin()[2], 0.0f) &&
                    floatNear(isect.getMax()[0], 0.5f) &&
                    floatNear(isect.getMax()[1], 0.5f) &&
                    floatNear(isect.getMax()[2], 1.0f);
        runner.endTest(pass, pass ? "" : "SbViewVolume ortho intersection wrong");
    }

    runner.startTest("SbViewVolume ortho intersect (bbox inside vv)");
    {
        SbViewVolume vv;
        vv.ortho(-0.5f, 0.5f, -0.5f, 0.5f, -1.0f, 10.0f);
        SbBox3f box(-0.25f, -0.25f, -0.25f, 0.25f, 0.25f, 0.25f);
        SbBox3f isect = vv.intersectionBox(box);
        bool pass = floatNear(isect.getMin()[0], -0.25f) &&
                    floatNear(isect.getMin()[1], -0.25f) &&
                    floatNear(isect.getMin()[2], -0.25f) &&
                    floatNear(isect.getMax()[0],  0.25f) &&
                    floatNear(isect.getMax()[1],  0.25f) &&
                    floatNear(isect.getMax()[2],  0.25f);
        runner.endTest(pass, pass ? "" : "SbViewVolume ortho (bbox inside) intersection wrong");
    }

    runner.startTest("SbViewVolume ortho intersect (vv inside bbox)");
    {
        SbViewVolume vv;
        vv.ortho(-0.5f, 0.5f, -0.5f, 0.5f, 0.0f, 5.0f);
        SbBox3f box(-10.0f, -10.0f, -10.0f, 10.0f, 10.0f, 10.0f);
        SbBox3f isect = vv.intersectionBox(box);
        bool pass = floatNear(isect.getMin()[0], -0.5f) &&
                    floatNear(isect.getMin()[1], -0.5f) &&
                    floatNear(isect.getMin()[2], -5.0f) &&
                    floatNear(isect.getMax()[0],  0.5f) &&
                    floatNear(isect.getMax()[1],  0.5f) &&
                    floatNear(isect.getMax()[2],  0.0f);
        runner.endTest(pass, pass ? "" : "SbViewVolume ortho (vv inside bbox) intersection wrong");
    }

    runner.startTest("SbViewVolume perspective intersect");
    {
        SbViewVolume vv;
        vv.perspective(0.78f, 1.0f, 4.25f, 4.75f);
        vv.translateCamera(SbVec3f(0.0f, 0.0f, 5.0f));
        SbBox3f box(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
        SbBox3f isect = vv.intersectionBox(box);
        bool pass = floatNear(isect.getMin()[0], 0.0f,  0.01f) &&
                    floatNear(isect.getMin()[1], 0.0f,  0.01f) &&
                    floatNear(isect.getMin()[2], 0.25f, 0.01f) &&
                    floatNear(isect.getMax()[0], 1.0f,  0.01f) &&
                    floatNear(isect.getMax()[1], 1.0f,  0.01f) &&
                    floatNear(isect.getMax()[2], 0.75f, 0.01f);
        runner.endTest(pass, pass ? "" : "SbViewVolume perspective intersection wrong");
    }

    return runner.getSummary();
}
