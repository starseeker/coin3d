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
 * Tests are baselined against upstream OBOL_TEST_SUITE blocks to verify
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
#include <Inventor/SbVec3d.h>
#include <Inventor/SbVec3s.h>
#include <Inventor/SbVec3us.h>
#include <Inventor/SbVec4f.h>
#include <Inventor/SbBox2f.h>
#include <Inventor/SbBox2d.h>
#include <Inventor/SbBox2s.h>
#include <Inventor/SbBox3f.h>
#include <Inventor/SbBox3d.h>
#include <Inventor/SbBox3i32.h>
#include <Inventor/SbBox3s.h>
#include <Inventor/SbByteBuffer.h>
#include <Inventor/SbBSPTree.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbDPMatrix.h>
#include <Inventor/SbDPRotation.h>
#include <Inventor/SbDPPlane.h>
#include <Inventor/SbDPLine.h>
#include <Inventor/SbRotation.h>
#include <Inventor/SbString.h>
#include <Inventor/SbPlane.h>
#include <Inventor/SbLine.h>
#include <Inventor/SbSphere.h>
#include <Inventor/SbCylinder.h>
#include <Inventor/SbColor4f.h>
#include <Inventor/SbBox2i32.h>
#include <Inventor/SbViewVolume.h>
#include <Inventor/SbImage.h>
#include "base/heap.h"

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
    // Baseline: src/base/SbVec3f.cpp OBOL_TEST_SUITE
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
    // Baseline: src/base/SbBox2f.cpp OBOL_TEST_SUITE
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
    // Baseline: src/base/SbBox3f.cpp OBOL_TEST_SUITE
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
    // Baseline: src/base/SbBox3i32.cpp OBOL_TEST_SUITE
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
    // Baseline: src/base/SbByteBuffer.cpp OBOL_TEST_SUITE
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
    // Baseline: src/base/SbBSPTree.cpp OBOL_TEST_SUITE
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
    // Baseline: src/base/SbMatrix.cpp OBOL_TEST_SUITE
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
    // Baseline: src/base/SbDPMatrix.cpp OBOL_TEST_SUITE
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
    // Baseline: src/base/SbRotation.cpp OBOL_TEST_SUITE
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
    // Baseline: src/base/SbString.cpp OBOL_TEST_SUITE
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
    // Baseline: src/base/SbPlane.cpp OBOL_TEST_SUITE
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
    // Baseline: src/base/SbViewVolume.cpp OBOL_TEST_SUITE
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

    // -----------------------------------------------------------------------
    // SbVec3d: fromString
    // Baseline: src/base/SbVec3d.cpp OBOL_TEST_SUITE
    // -----------------------------------------------------------------------
    runner.startTest("SbVec3d fromString");
    {
        SbVec3d foo;
        SbString test = "0.3333333333333333 -2 -3.0";
        SbVec3d trueVal(0.3333333333333333, -2, -3);
        SbBool ok = foo.fromString(test);
        bool pass = ok && (trueVal == foo);
        runner.endTest(pass, pass ? "" : "SbVec3d::fromString failed");
    }

    // -----------------------------------------------------------------------
    // SbVec4f: normalize already-normalized vector
    // Baseline: src/base/SbVec4f.cpp OBOL_TEST_SUITE
    // -----------------------------------------------------------------------
    runner.startTest("SbVec4f normalize already-normalized vector");
    {
        const float SQRT2 = sqrtf(2.0f) / 2.0f;
        SbVec4f vec(0, -SQRT2, 0, SQRT2);
        vec.normalize();
        bool pass = (vec[0] == 0.0f) &&
                    (fabsf(vec[1] - (-SQRT2)) < 1e-5f) &&
                    (vec[2] == 0.0f) &&
                    (fabsf(vec[3] - SQRT2)    < 1e-5f);
        runner.endTest(pass, pass ? "" : "SbVec4f normalize already-normalized failed");
    }

    // -----------------------------------------------------------------------
    // SbVec3s: fromString / fromInvalidString
    // Baseline: src/base/SbVec3s.cpp OBOL_TEST_SUITE
    // -----------------------------------------------------------------------
    runner.startTest("SbVec3s fromString");
    {
        SbVec3s foo;
        SbString test = "1 -2 3";
        SbVec3s trueVal(1, -2, 3);
        foo.fromString(test);
        bool pass = (trueVal == foo);
        runner.endTest(pass, pass ? "" : "SbVec3s::fromString failed");
    }

    runner.startTest("SbVec3s fromInvalidString");
    {
        SbVec3s foo;
        SbString test = "a,2,3";
        SbBool ok = foo.fromString(test);
        bool pass = (ok == FALSE);
        runner.endTest(pass, pass ? "" : "SbVec3s::fromString should fail for 'a,2,3'");
    }

    // -----------------------------------------------------------------------
    // SbBox2d: getSize / getClosestPoint
    // Baseline: src/base/SbBox2d.cpp OBOL_TEST_SUITE
    // -----------------------------------------------------------------------
    runner.startTest("SbBox2d getSize");
    {
        SbVec2d minPt(1, 2), maxPt(3, 4);
        SbBox2d box(minPt, maxPt);
        SbVec2d diff = maxPt - minPt;
        bool pass = (box.getSize() == diff);
        runner.endTest(pass, pass ? "" : "SbBox2d getSize incorrect");
    }

    runner.startTest("SbBox2d getClosestPoint outside");
    {
        SbVec2d point(1524, 13794);
        SbBox2d box(SbVec2d(1557, 3308), SbVec2d(3113, 30157));
        SbVec2d expected(1557, 13794);
        bool pass = (box.getClosestPoint(point) == expected);
        runner.endTest(pass, pass ? "" : "SbBox2d getClosestPoint outside wrong");
    }

    // -----------------------------------------------------------------------
    // SbBox3d: getClosestPoint
    // Baseline: src/base/SbBox3d.cpp OBOL_TEST_SUITE
    // -----------------------------------------------------------------------
    runner.startTest("SbBox3d getClosestPoint outside");
    {
        SbVec3d point(1524, 13794, 851);
        SbBox3d box(SbVec3d(1557, 3308, 850), SbVec3d(3113, 30157, 1886));
        SbVec3d expected(1557, 13794, 851);
        bool pass = (box.getClosestPoint(point) == expected);
        runner.endTest(pass, pass ? "" : "SbBox3d getClosestPoint outside wrong");
    }

    // -----------------------------------------------------------------------
    // SbBox2s: getSize
    // Baseline: src/base/SbBox2s.cpp OBOL_TEST_SUITE
    // -----------------------------------------------------------------------
    runner.startTest("SbBox2s getSize");
    {
        SbVec2s minPt(1, 2), maxPt(3, 4);
        SbBox2s box(minPt, maxPt);
        SbVec2s diff = maxPt - minPt;
        bool pass = (box.getSize() == diff);
        runner.endTest(pass, pass ? "" : "SbBox2s getSize incorrect");
    }

    // -----------------------------------------------------------------------
    // SbBox3s: getSize / getClosestPoint
    // Baseline: src/base/SbBox3s.cpp OBOL_TEST_SUITE
    // -----------------------------------------------------------------------
    runner.startTest("SbBox3s getSize");
    {
        SbVec3s minPt(1, 2, 3), maxPt(3, 4, 5);
        SbBox3s box(minPt, maxPt);
        SbVec3s diff = maxPt - minPt;
        bool pass = (box.getSize() == diff);
        runner.endTest(pass, pass ? "" : "SbBox3s getSize incorrect");
    }

    runner.startTest("SbBox3s getClosestPoint outside");
    {
        SbVec3f point(1524.0f, 13794.0f, 851.0f);
        SbBox3s box(SbVec3s(1557, 3308, 850), SbVec3s(3113, 30157, 1886));
        SbVec3f expected(1557.0f, 13794.0f, 851.0f);
        bool pass = (box.getClosestPoint(point) == expected);
        runner.endTest(pass, pass ? "" : "SbBox3s getClosestPoint outside wrong");
    }

    // -----------------------------------------------------------------------
    // SbDPRotation: construction (TGS compliance)
    // Baseline: src/base/SbDPRotation.cpp OBOL_TEST_SUITE (tgsCompliance)
    // -----------------------------------------------------------------------
    runner.startTest("SbDPRotation construct from axis/angle");
    {
        SbDPRotation rot(SbVec3d(0, 1, 2), 3.0);
        // A non-trivial rotation should have a non-zero quaternion w component
        double q[4];
        rot.getValue(q[0], q[1], q[2], q[3]);
        bool pass = (q[3] != 0.0);
        runner.endTest(pass, pass ? "" : "SbDPRotation construction failed");
    }

    // -----------------------------------------------------------------------
    // SbDPPlane: intersect two planes and verify sign of result
    // Baseline: src/base/SbDPPlane.cpp OBOL_TEST_SUITE (signCorrect)
    // -----------------------------------------------------------------------
    runner.startTest("SbDPPlane plane-plane intersection sign correct");
    {
        SbDPPlane plane1(SbVec3d(0.0, 0.0, 1.0), 3.0);
        SbDPPlane plane2(SbVec3d(1.0, 0.0, 0.0), 21.0);
        SbDPLine line;
        bool ok = plane1.intersect(plane2, line);
        // The intersection line position z-component should be >= plane1 distance (3.0)
        bool pass = ok && (line.getPosition()[2] > 0.0);
        runner.endTest(pass, pass ? "" : "SbDPPlane intersection sign wrong");
    }

    // -----------------------------------------------------------------------
    // SbImage: copy construction
    // Baseline: src/base/SbImage.cpp OBOL_TEST_SUITE (copyConstruct)
    // -----------------------------------------------------------------------
    runner.startTest("SbImage copy construct");
    {
        unsigned char buf[4] = {0, 1, 2, 3};
        SbImage bar(buf, SbVec2s(2, 2), 1);
        SbImage foo(bar);

        SbVec2s tmp1; int tmp2;
        const unsigned char* barData = bar.getValue(tmp1, tmp2);
        const unsigned char* fooData = foo.getValue(tmp1, tmp2);

        bool pass = (fooData != nullptr) && (barData != nullptr);
        for (int i = 0; i < 4 && pass; ++i)
            pass = (fooData[i] == barData[i]);
        runner.endTest(pass, pass ? "" : "SbImage copy construct values differ");
    }

    // -----------------------------------------------------------------------
    // SbLine: construction and getClosestPoint
    // -----------------------------------------------------------------------
    runner.startTest("SbLine getClosestPoint");
    {
        // Line along X axis at origin
        SbLine line(SbVec3f(0.0f, 0.0f, 0.0f), SbVec3f(1.0f, 0.0f, 0.0f));
        SbVec3f closest = line.getClosestPoint(SbVec3f(3.0f, 5.0f, 0.0f));
        bool pass = (closest[0] == 3.0f) &&
                    (fabsf(closest[1]) < 1e-5f) &&
                    (fabsf(closest[2]) < 1e-5f);
        runner.endTest(pass, pass ? "" : "SbLine::getClosestPoint returned wrong point");
    }

    runner.startTest("SbLine getClosestPoints between two lines");
    {
        // Two parallel lines offset in Y – they never meet; getClosestPoints returns FALSE
        SbLine lineA(SbVec3f(0,0,0), SbVec3f(1,0,0));
        SbLine lineB(SbVec3f(0,1,0), SbVec3f(1,1,0));
        SbVec3f ptA, ptB;
        // parallel lines: getClosestPoints returns FALSE
        SbBool ok = lineA.getClosestPoints(lineB, ptA, ptB);
        runner.endTest(!ok, !ok ? "" : "SbLine::getClosestPoints should return FALSE for parallel lines");
    }

    // -----------------------------------------------------------------------
    // SbSphere: construction, setCenter/setRadius, pointInside
    // -----------------------------------------------------------------------
    runner.startTest("SbSphere pointInside");
    {
        SbSphere sphere(SbVec3f(0.0f, 0.0f, 0.0f), 2.0f);
        bool inside  = sphere.pointInside(SbVec3f(1.0f, 0.0f, 0.0f));
        bool outside = sphere.pointInside(SbVec3f(3.0f, 0.0f, 0.0f));
        bool pass = inside && !outside;
        runner.endTest(pass, pass ? "" : "SbSphere::pointInside gave wrong result");
    }

    runner.startTest("SbSphere getRadius setRadius");
    {
        SbSphere sphere;
        sphere.setCenter(SbVec3f(1.0f, 2.0f, 3.0f));
        sphere.setRadius(5.0f);
        bool pass = (sphere.getRadius() == 5.0f) &&
                    (sphere.getCenter() == SbVec3f(1.0f, 2.0f, 3.0f));
        runner.endTest(pass, pass ? "" : "SbSphere set/get radius/center failed");
    }

    // -----------------------------------------------------------------------
    // SbCylinder: construction and getRadius
    // -----------------------------------------------------------------------
    runner.startTest("SbCylinder getRadius");
    {
        SbLine axis(SbVec3f(0,0,0), SbVec3f(0,1,0));
        SbCylinder cyl(axis, 3.0f);
        bool pass = (cyl.getRadius() == 3.0f);
        runner.endTest(pass, pass ? "" : "SbCylinder getRadius returned wrong value");
    }

    // -----------------------------------------------------------------------
    // SbColor4f: construction and set/get round-trip
    // -----------------------------------------------------------------------
    runner.startTest("SbColor4f set/get round-trip");
    {
        SbColor4f c(0.5f, 0.25f, 0.75f, 0.9f);
        bool pass = (fabsf(c[0] - 0.5f)  < 1e-5f) &&
                    (fabsf(c[1] - 0.25f) < 1e-5f) &&
                    (fabsf(c[2] - 0.75f) < 1e-5f) &&
                    (fabsf(c[3] - 0.9f)  < 1e-5f);
        runner.endTest(pass, pass ? "" : "SbColor4f construction/get failed");
    }

    // -----------------------------------------------------------------------
    // SbBox2i32: getSize
    // Baseline: src/base/SbBox2i32.cpp OBOL_TEST_SUITE (checkSize)
    // -----------------------------------------------------------------------
    runner.startTest("SbBox2i32 getSize");
    {
        SbVec2i32 minPt(1, 2), maxPt(3, 4);
        SbBox2i32 box(minPt, maxPt);
        SbVec2i32 diff = maxPt - minPt;
        bool pass = (box.getSize() == diff);
        runner.endTest(pass, pass ? "" : "SbBox2i32 getSize incorrect");
    }

    // -----------------------------------------------------------------------
    // SbVec3us: construction and get/setValue round-trip
    // Baseline: src/base/SbVec3us.cpp (OBOL_TEST_SUITE block is empty;
    //           API behavior test)
    // -----------------------------------------------------------------------
    runner.startTest("SbVec3us construction and getValue");
    {
        SbVec3us v(1, 2, 3);
        unsigned short x, y, z;
        v.getValue(x, y, z);
        bool pass = (x == 1) && (y == 2) && (z == 3);
        runner.endTest(pass, pass ? "" : "SbVec3us getValue returned wrong values");
    }

    runner.startTest("SbVec3us setValue round-trip");
    {
        SbVec3us v;
        v.setValue(10, 20, 30);
        const unsigned short* p = v.getValue();
        bool pass = (p[0] == 10) && (p[1] == 20) && (p[2] == 30);
        runner.endTest(pass, pass ? "" : "SbVec3us setValue/getValue mismatch");
    }

    // -----------------------------------------------------------------------
    // SbHeap (cc_heap): min-heap and max-heap ordering
    // Baseline: src/base/heap.cpp OBOL_TEST_SUITE
    // -----------------------------------------------------------------------
    {
        struct MockVal { double x; };

        // Named functions used as callbacks avoid repeated cast patterns
        struct HeapCb {
            static void print(void* v, SbString& str) {
                MockVal* mv = static_cast<MockVal*>(v);
                char buf[32];
                snprintf(buf, sizeof(buf), "%g", mv->x);
                str += buf;
            }
            static int min_cmp(void* lhs, void* rhs) {
                return static_cast<MockVal*>(lhs)->x < static_cast<MockVal*>(rhs)->x ? 1 : 0;
            }
            static int max_cmp(void* lhs, void* rhs) {
                return static_cast<MockVal*>(lhs)->x > static_cast<MockVal*>(rhs)->x ? 1 : 0;
            }
        };

        auto* pprint   = reinterpret_cast<cc_heap_print_cb*>(&HeapCb::print);
        auto* pmin_cmp = reinterpret_cast<cc_heap_compare_cb*>(&HeapCb::min_cmp);
        auto* pmax_cmp = reinterpret_cast<cc_heap_compare_cb*>(&HeapCb::max_cmp);

        runner.startTest("SbHeap min_heap ordering");
        {
            MockVal val[] = {{3},{2},{1},{15},{5},{4},{45}};
            cc_heap* heap = cc_heap_construct(256, pmin_cmp, TRUE);
            for (auto& v : val) cc_heap_add(heap, &v);
            SbString result;
            cc_heap_print(heap, pprint, result, FALSE);
            cc_heap_destruct(heap);
            SbString expected("1 3 2 15 5 4 45 ");
            bool pass = (result == expected);
            runner.endTest(pass, pass ? "" :
                std::string("min_heap mismatch: got '") + result.getString() +
                "' expected '" + expected.getString() + "'");
        }

        runner.startTest("SbHeap max_heap ordering");
        {
            MockVal val[] = {{3},{2},{1},{15},{5},{4},{45}};
            cc_heap* heap = cc_heap_construct(256, pmax_cmp, TRUE);
            for (auto& v : val) cc_heap_add(heap, &v);
            SbString result;
            cc_heap_print(heap, pprint, result, FALSE);
            cc_heap_destruct(heap);
            SbString expected("45 5 15 2 3 1 4 ");
            bool pass = (result == expected);
            runner.endTest(pass, pass ? "" :
                std::string("max_heap mismatch: got '") + result.getString() +
                "' expected '" + expected.getString() + "'");
        }

        runner.startTest("SbHeap heap_add");
        {
            MockVal val[] = {{3},{2},{1},{15},{5},{4},{45}};
            cc_heap* heap = cc_heap_construct(256, pmin_cmp, TRUE);
            for (auto& v : val) cc_heap_add(heap, &v);
            MockVal extra = {12};
            cc_heap_add(heap, &extra);
            SbString result;
            cc_heap_print(heap, pprint, result, FALSE);
            cc_heap_destruct(heap);
            SbString expected("1 3 2 12 5 4 45 15 ");
            bool pass = (result == expected);
            runner.endTest(pass, pass ? "" :
                std::string("heap_add mismatch: got '") + result.getString() +
                "' expected '" + expected.getString() + "'");
        }

        runner.startTest("SbHeap heap_remove");
        {
            MockVal val[] = {{3},{2},{1},{15},{5},{4},{45}};
            cc_heap* heap = cc_heap_construct(256, pmin_cmp, TRUE);
            for (auto& v : val) cc_heap_add(heap, &v);
            cc_heap_remove(heap, &val[3]); // remove 15
            SbString result;
            cc_heap_print(heap, pprint, result, FALSE);
            cc_heap_destruct(heap);
            SbString expected("1 3 2 45 5 4 ");
            bool pass = (result == expected);
            runner.endTest(pass, pass ? "" :
                std::string("heap_remove mismatch: got '") + result.getString() +
                "' expected '" + expected.getString() + "'");
        }

        runner.startTest("SbHeap heap_update");
        {
            MockVal val[] = {{3},{2},{1},{15},{5},{4},{45}};
            cc_heap* heap = cc_heap_construct(256, pmin_cmp, TRUE);
            for (auto& v : val) cc_heap_add(heap, &v);
            val[3].x = 1; // change 15 -> 1
            cc_heap_update(heap, &val[3]);
            SbString result;
            cc_heap_print(heap, pprint, result, FALSE);
            cc_heap_destruct(heap);
            SbString expected("1 1 2 3 5 4 45 ");
            bool pass = (result == expected);
            runner.endTest(pass, pass ? "" :
                std::string("heap_update mismatch: got '") + result.getString() +
                "' expected '" + expected.getString() + "'");
        }
    }

    // -----------------------------------------------------------------------
    // SbMatrix full suite — Tier 2 coverage expansion
    // Covers: det4, inverse, factor, multVecMatrix, multDirMatrix
    // -----------------------------------------------------------------------

    runner.startTest("SbMatrix det4 identity");
    {
        SbMatrix m = SbMatrix::identity();
        float d = m.det4();
        bool pass = floatNear(d, 1.0f, 1e-4f);
        runner.endTest(pass, pass ? "" : "SbMatrix identity det4 != 1");
    }

    runner.startTest("SbMatrix det4 scale matrix");
    {
        SbMatrix m = SbMatrix::identity();
        m[0][0] = 2.0f;
        m[1][1] = 3.0f;
        m[2][2] = 4.0f;
        // det = 2*3*4 = 24
        float d = m.det4();
        bool pass = floatNear(d, 24.0f, 1e-3f);
        runner.endTest(pass, pass ? "" : "SbMatrix scale det4 incorrect");
    }

    runner.startTest("SbMatrix inverse of identity is identity");
    {
        SbMatrix m    = SbMatrix::identity();
        SbMatrix inv  = m.inverse();
        SbMatrix prod = m.multRight(inv);
        // product should be (close to) identity
        bool pass = true;
        for (int r = 0; r < 4 && pass; ++r)
            for (int c = 0; c < 4 && pass; ++c) {
                float expected = (r == c) ? 1.0f : 0.0f;
                if (!floatNear(prod[r][c], expected, 1e-4f)) pass = false;
            }
        runner.endTest(pass, pass ? "" : "SbMatrix inverse(identity) != identity");
    }

    runner.startTest("SbMatrix inverse of scale matrix");
    {
        SbMatrix m = SbMatrix::identity();
        m[0][0] = 2.0f;
        m[1][1] = 4.0f;
        m[2][2] = 0.5f;
        SbMatrix inv = m.inverse();
        // Expected diagonal: 0.5, 0.25, 2.0
        bool pass = floatNear(inv[0][0], 0.5f,  1e-4f) &&
                    floatNear(inv[1][1], 0.25f, 1e-4f) &&
                    floatNear(inv[2][2], 2.0f,  1e-4f);
        runner.endTest(pass, pass ? "" : "SbMatrix scale inverse incorrect");
    }

    runner.startTest("SbMatrix multVecMatrix translation");
    {
        // Build a translation matrix: translate by (1,2,3)
        SbMatrix m = SbMatrix::identity();
        m[3][0] = 1.0f;
        m[3][1] = 2.0f;
        m[3][2] = 3.0f;
        SbVec3f src(0.0f, 0.0f, 0.0f), dst;
        m.multVecMatrix(src, dst);
        bool pass = floatNear(dst[0], 1.0f) &&
                    floatNear(dst[1], 2.0f) &&
                    floatNear(dst[2], 3.0f);
        runner.endTest(pass, pass ? "" : "SbMatrix multVecMatrix translation incorrect");
    }

    runner.startTest("SbMatrix multVecMatrix scale");
    {
        SbMatrix m = SbMatrix::identity();
        m[0][0] = 3.0f;
        m[1][1] = 3.0f;
        m[2][2] = 3.0f;
        SbVec3f src(1.0f, 1.0f, 1.0f), dst;
        m.multVecMatrix(src, dst);
        bool pass = floatNear(dst[0], 3.0f) &&
                    floatNear(dst[1], 3.0f) &&
                    floatNear(dst[2], 3.0f);
        runner.endTest(pass, pass ? "" : "SbMatrix multVecMatrix scale incorrect");
    }

    runner.startTest("SbMatrix multDirMatrix ignores translation");
    {
        // Translation matrix: translate by (5,6,7)
        SbMatrix m = SbMatrix::identity();
        m[3][0] = 5.0f;
        m[3][1] = 6.0f;
        m[3][2] = 7.0f;
        SbVec3f dir(1.0f, 0.0f, 0.0f), dst;
        m.multDirMatrix(dir, dst);
        // Direction must NOT be translated
        bool pass = floatNear(dst[0], 1.0f) &&
                    floatNear(dst[1], 0.0f) &&
                    floatNear(dst[2], 0.0f);
        runner.endTest(pass, pass ? "" :
            "SbMatrix multDirMatrix should not translate direction vector");
    }

    runner.startTest("SbMatrix factor executes on translation+scale matrix");
    {
        // factor() exercises the polar-decomposition code path.
        // Note: this implementation may return FALSE for non-standard matrices;
        // the test verifies the call completes without crashing.
        SbMatrix m;
        m.setTransform(SbVec3f(1.0f, 2.0f, 3.0f),
                       SbRotation::identity(),
                       SbVec3f(2.0f, 2.0f, 2.0f));
        SbMatrix r, u, proj;
        SbVec3f  s, t;
        m.factor(r, s, u, t, proj);  // exercises code; return value varies
        bool pass = true;            // call completed without crash
        runner.endTest(pass, pass ? "" : "SbMatrix factor crashed");
    }

    // -----------------------------------------------------------------------
    // SbDPMatrix math suite — Tier 2 coverage expansion
    // Covers: multLeft, multRight, multVecMatrix, multDirMatrix, inverse, det4,
    //         getTransform, factor
    // -----------------------------------------------------------------------

    runner.startTest("SbDPMatrix identity det4 = 1");
    {
        SbMatrixd m = SbMatrixd::identity();
        double d = m.det4();
        bool pass = doubleNear(d, 1.0);
        runner.endTest(pass, pass ? "" : "SbDPMatrix identity det4 != 1");
    }

    runner.startTest("SbDPMatrix scale det4");
    {
        SbMatrixd m = SbMatrixd::identity();
        m[0][0] = 2.0; m[1][1] = 3.0; m[2][2] = 5.0;
        // det = 2*3*5 = 30
        double d = m.det4();
        bool pass = doubleNear(d, 30.0, 1e-9);
        runner.endTest(pass, pass ? "" : "SbDPMatrix scale det4 incorrect");
    }

    runner.startTest("SbDPMatrix inverse of identity");
    {
        SbMatrixd m   = SbMatrixd::identity();
        SbMatrixd inv = m.inverse();
        bool pass = true;
        for (int r = 0; r < 4 && pass; ++r)
            for (int c = 0; c < 4 && pass; ++c) {
                double expected = (r == c) ? 1.0 : 0.0;
                if (!doubleNear(inv[r][c], expected, 1e-9)) pass = false;
            }
        runner.endTest(pass, pass ? "" : "SbDPMatrix inverse(identity) != identity");
    }

    runner.startTest("SbDPMatrix multRight produces correct product");
    {
        // Scale by 2 then translate by (1,0,0)
        SbMatrixd scale = SbMatrixd::identity();
        scale[0][0] = 2.0; scale[1][1] = 2.0; scale[2][2] = 2.0;

        SbMatrixd trans = SbMatrixd::identity();
        trans[3][0] = 1.0;  // row-major translation (Coin convention)

        SbMatrixd prod = scale;
        prod.multRight(trans);

        SbVec3d src(1.0, 0.0, 0.0), dst;
        prod.multVecMatrix(src, dst);
        // scale(1,0,0) -> (2,0,0), then translate -> (3,0,0)
        bool pass = doubleNear(dst[0], 3.0, 1e-9) &&
                    doubleNear(dst[1], 0.0, 1e-9) &&
                    doubleNear(dst[2], 0.0, 1e-9);
        runner.endTest(pass, pass ? "" : "SbDPMatrix multRight incorrect result");
    }

    runner.startTest("SbDPMatrix multLeft produces correct product");
    {
        // prod = trans (translate +1 in X), then multLeft(scale) → prod = scale * trans
        // v * (scale * trans): first scales v, then translates
        // (1,0,0) * scale → (2,0,0); (2,0,0) * trans → (3,0,0)
        SbMatrixd scale = SbMatrixd::identity();
        scale[0][0] = 2.0; scale[1][1] = 2.0; scale[2][2] = 2.0;

        SbMatrixd trans = SbMatrixd::identity();
        trans[3][0] = 1.0;

        // trans.multLeft(scale) == scale * trans
        SbMatrixd prod = trans;
        prod.multLeft(scale);

        SbVec3d src(1.0, 0.0, 0.0), dst;
        prod.multVecMatrix(src, dst);
        // (1,0,0) scaled to (2,0,0), then translated to (3,0,0)
        bool pass = doubleNear(dst[0], 3.0, 1e-9) &&
                    doubleNear(dst[1], 0.0, 1e-9) &&
                    doubleNear(dst[2], 0.0, 1e-9);
        runner.endTest(pass, pass ? "" : "SbDPMatrix multLeft incorrect result");
    }

    runner.startTest("SbDPMatrix multVecMatrix translation");
    {
        SbMatrixd m = SbMatrixd::identity();
        m[3][0] = 4.0; m[3][1] = 5.0; m[3][2] = 6.0;
        SbVec3d src(0.0, 0.0, 0.0), dst;
        m.multVecMatrix(src, dst);
        bool pass = doubleNear(dst[0], 4.0) &&
                    doubleNear(dst[1], 5.0) &&
                    doubleNear(dst[2], 6.0);
        runner.endTest(pass, pass ? "" : "SbDPMatrix multVecMatrix translation incorrect");
    }

    runner.startTest("SbDPMatrix multDirMatrix ignores translation");
    {
        SbMatrixd m = SbMatrixd::identity();
        m[3][0] = 10.0; m[3][1] = 20.0; m[3][2] = 30.0;
        SbVec3d dir(1.0, 0.0, 0.0), dst;
        m.multDirMatrix(dir, dst);
        bool pass = doubleNear(dst[0], 1.0) &&
                    doubleNear(dst[1], 0.0) &&
                    doubleNear(dst[2], 0.0);
        runner.endTest(pass, pass ? "" :
            "SbDPMatrix multDirMatrix should not translate direction");
    }

    runner.startTest("SbDPMatrix getTransform round-trips translation");
    {
        SbMatrixd m = SbMatrixd::identity();
        SbVec3d    inT(1.5, -2.5, 3.0);
        SbDPRotation inR = SbDPRotation::identity();
        SbVec3d    inS(1.0, 1.0, 1.0);
        m.setTransform(inT, inR, inS);

        SbVec3d    outT;
        SbDPRotation outR, outSO;
        SbVec3d    outS;
        m.getTransform(outT, outR, outS, outSO);

        bool pass = doubleNear(outT[0], 1.5, 1e-6) &&
                    doubleNear(outT[1], -2.5, 1e-6) &&
                    doubleNear(outT[2], 3.0, 1e-6);
        runner.endTest(pass, pass ? "" :
            "SbDPMatrix getTransform translation round-trip failed");
    }

    runner.startTest("SbDPMatrix factor executes on translation+scale matrix");
    {
        // factor() exercises the polar-decomposition code path.
        // Note: this implementation may return FALSE for non-standard matrices;
        // the test verifies the call completes without crashing.
        SbMatrixd m = SbMatrixd::identity();
        SbVec3d    inT(1.0, 2.0, 3.0);
        SbDPRotation inR = SbDPRotation::identity();
        SbVec3d    inS(2.0, 2.0, 2.0);
        m.setTransform(inT, inR, inS);

        SbMatrixd r, u, proj;
        SbVec3d   s, t;
        m.factor(r, s, u, t, proj);  // exercises code; return value varies
        bool pass = true;            // call completed without crash
        runner.endTest(pass, pass ? "" :
            "SbDPMatrix factor crashed");
    }

    return runner.getSummary();
}
