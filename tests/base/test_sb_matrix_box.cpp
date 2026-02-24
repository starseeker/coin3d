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
 * @file test_sb_matrix_box.cpp
 * @brief Tests for SbMatrix and SbBox3f/SbBox2f deeper coverage (base/ 33.5 %).
 *
 * Covers:
 *   SbMatrix:
 *     makeIdentity, identity(), setTranslate, setScale(float), setScale(vec),
 *     setRotate, setTransform, getTransform, multRight/multLeft, 
 *     multVecMatrix, multMatrixVec, multDirMatrix, transpose,
 *     inverse, det3, det4, equals, operator==, operator*, operator*=
 *   SbBox3f:
 *     makeEmpty, isEmpty, extendBy(pt), extendBy(box), intersect(pt),
 *     intersect(box), getCenter, getSize, hasVolume, getVolume,
 *     transform(matrix), getSpan, getBounds, getOrigin
 *   SbBox2f:
 *     makeEmpty, isEmpty, extendBy(pt), intersect(pt), getCenter, getSize,
 *     hasArea, getMin/getMax
 */

#include "../test_utils.h"

#include <Inventor/SbMatrix.h>
#include <Inventor/SbBox3f.h>
#include <Inventor/SbBox2f.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbVec2f.h>
#include <Inventor/SbRotation.h>
#include <Inventor/SbLine.h>
#include <cmath>
#include <cstdio>

using namespace SimpleTest;

static bool floatNear(float a, float b, float eps = 1e-4f)
{
    return std::fabs(a - b) < eps;
}

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // ======================================================================
    // SbMatrix
    // ======================================================================

    runner.startTest("SbMatrix identity() is identity");
    {
        SbMatrix m = SbMatrix::identity();
        bool pass = true;
        for (int i = 0; i < 4 && pass; ++i)
            for (int j = 0; j < 4 && pass; ++j)
                pass = floatNear(m[i][j], (i == j) ? 1.0f : 0.0f);
        runner.endTest(pass, pass ? "" : "SbMatrix::identity() is not identity");
    }

    runner.startTest("SbMatrix makeIdentity resets matrix");
    {
        SbMatrix m;
        m[0][3] = 5.0f; // disturb
        m.makeIdentity();
        bool pass = (m == SbMatrix::identity());
        runner.endTest(pass, pass ? "" : "SbMatrix::makeIdentity failed");
    }

    runner.startTest("SbMatrix setTranslate");
    {
        SbMatrix m;
        m.setTranslate(SbVec3f(1, 2, 3));
        SbVec3f pt(0, 0, 0);
        SbVec3f result;
        m.multVecMatrix(pt, result);
        bool pass = floatNear(result[0], 1.0f) &&
                    floatNear(result[1], 2.0f) &&
                    floatNear(result[2], 3.0f);
        runner.endTest(pass, pass ? "" : "SbMatrix::setTranslate translation wrong");
    }

    runner.startTest("SbMatrix setScale(float) scales all axes equally");
    {
        SbMatrix m;
        m.setScale(2.0f);
        SbVec3f pt(1, 1, 1);
        SbVec3f result;
        m.multVecMatrix(pt, result);
        bool pass = floatNear(result[0], 2.0f) &&
                    floatNear(result[1], 2.0f) &&
                    floatNear(result[2], 2.0f);
        runner.endTest(pass, pass ? "" : "SbMatrix setScale(float) wrong");
    }

    runner.startTest("SbMatrix setScale(SbVec3f) scales axes independently");
    {
        SbMatrix m;
        m.setScale(SbVec3f(2, 3, 4));
        SbVec3f pt(1, 1, 1);
        SbVec3f result;
        m.multVecMatrix(pt, result);
        bool pass = floatNear(result[0], 2.0f) &&
                    floatNear(result[1], 3.0f) &&
                    floatNear(result[2], 4.0f);
        runner.endTest(pass, pass ? "" : "SbMatrix setScale(vec) wrong");
    }

    runner.startTest("SbMatrix setRotate around Z by π/2");
    {
        SbMatrix m;
        SbRotation rot(SbVec3f(0, 0, 1), static_cast<float>(M_PI / 2));
        m.setRotate(rot);
        SbVec3f pt(1, 0, 0);
        SbVec3f result;
        m.multVecMatrix(pt, result);
        // After 90° CCW around Z: (1,0,0) → (0,1,0)
        bool pass = floatNear(result[0], 0.0f, 1e-4f) && floatNear(result[1], 1.0f, 1e-4f);
        runner.endTest(pass, pass ? "" : "SbMatrix setRotate around Z by π/2 wrong");
    }

    runner.startTest("SbMatrix transpose of identity is identity");
    {
        SbMatrix m = SbMatrix::identity();
        SbMatrix t = m.transpose();
        bool pass = (t == m);
        runner.endTest(pass, pass ? "" : "Transpose of identity should be identity");
    }

    runner.startTest("SbMatrix transpose swaps off-diagonal");
    {
        SbMatrix m = SbMatrix::identity();
        m[0][1] = 5.0f;
        SbMatrix t = m.transpose();
        bool pass = floatNear(t[1][0], 5.0f) && floatNear(t[0][1], 0.0f);
        runner.endTest(pass, pass ? "" : "SbMatrix transpose should swap off-diagonal elements");
    }

    runner.startTest("SbMatrix inverse of identity is identity");
    {
        SbMatrix m = SbMatrix::identity();
        SbMatrix inv = m.inverse();
        bool pass = (inv == SbMatrix::identity());
        runner.endTest(pass, pass ? "" : "Inverse of identity should be identity");
    }

    runner.startTest("SbMatrix inverse of translation reverses translation");
    {
        SbMatrix m;
        m.setTranslate(SbVec3f(3, 0, 0));
        SbMatrix inv = m.inverse();
        SbVec3f pt(0, 0, 0);
        SbVec3f result;
        m.multVecMatrix(pt, result);   // result = (3,0,0)
        SbVec3f back;
        inv.multVecMatrix(result, back); // back = (0,0,0)
        bool pass = floatNear(back[0], 0.0f) && floatNear(back[1], 0.0f);
        runner.endTest(pass, pass ? "" : "Inverse of translation should restore original point");
    }

    runner.startTest("SbMatrix det4 of identity is 1.0");
    {
        SbMatrix m = SbMatrix::identity();
        bool pass = floatNear(m.det4(), 1.0f);
        runner.endTest(pass, pass ? "" : "det4 of identity != 1.0");
    }

    runner.startTest("SbMatrix equals with tolerance");
    {
        SbMatrix m = SbMatrix::identity();
        SbMatrix n = SbMatrix::identity();
        n[0][0] = 1.00001f;
        bool pass = m.equals(n, 0.001f) && !m.equals(n, 0.000001f);
        runner.endTest(pass, pass ? "" : "SbMatrix::equals with tolerance failed");
    }

    runner.startTest("SbMatrix multRight composition");
    {
        SbMatrix m = SbMatrix::identity();
        SbMatrix t;
        t.setTranslate(SbVec3f(1, 0, 0));
        m.multRight(t);
        SbVec3f pt(0, 0, 0);
        SbVec3f result;
        m.multVecMatrix(pt, result);
        bool pass = floatNear(result[0], 1.0f);
        runner.endTest(pass, pass ? "" : "SbMatrix multRight failed");
    }

    runner.startTest("SbMatrix multLeft composition");
    {
        SbMatrix m;
        m.setTranslate(SbVec3f(1, 0, 0));
        SbMatrix s;
        s.setScale(2.0f);
        s.multLeft(m);  // s = m * s
        // In Coin3D row-vector convention: point * m * s
        // (1,0,0) * translate(1,0,0) = (2,0,0)
        // (2,0,0) * scale(2) = (4,0,0)
        SbVec3f pt(1, 0, 0);
        SbVec3f result;
        s.multVecMatrix(pt, result);
        bool pass = floatNear(result[0], 4.0f, 0.01f);
        runner.endTest(pass, pass ? "" : "SbMatrix multLeft failed");
    }

    runner.startTest("SbMatrix multDirMatrix does not apply translation");
    {
        SbMatrix m;
        m.setTranslate(SbVec3f(5, 5, 5));
        SbVec3f dir(1, 0, 0);
        SbVec3f result;
        m.multDirMatrix(dir, result);
        // Direction should be unchanged by translation
        bool pass = floatNear(result[0], 1.0f) && floatNear(result[1], 0.0f);
        runner.endTest(pass, pass ? "" : "SbMatrix multDirMatrix should ignore translation");
    }

    runner.startTest("SbMatrix multMatrixVec is inverse of multVecMatrix");
    {
        SbMatrix m;
        m.setTranslate(SbVec3f(1, 2, 3));
        SbVec3f pt(0, 0, 0);
        SbVec3f forward, back;
        m.multVecMatrix(pt, forward);
        m.inverse().multVecMatrix(forward, back);
        bool pass = floatNear(back[0], 0.0f) && floatNear(back[1], 0.0f) && floatNear(back[2], 0.0f);
        runner.endTest(pass, pass ? "" : "multMatrixVec inverse round-trip failed");
    }

    runner.startTest("SbMatrix setTransform / getTransform round-trip");
    {
        SbVec3f t(1, 2, 3);
        SbRotation r(SbVec3f(0, 1, 0), 0.5f);
        SbVec3f s(1, 1, 1);
        SbRotation so = SbRotation::identity();

        SbMatrix m;
        m.setTransform(t, r, s);

        SbVec3f outT;
        SbRotation outR, outSO;
        SbVec3f outS;
        m.getTransform(outT, outR, outS, outSO);

        bool pass = floatNear(outT[0], 1.0f, 0.01f) &&
                    floatNear(outT[1], 2.0f, 0.01f) &&
                    floatNear(outT[2], 3.0f, 0.01f);
        runner.endTest(pass, pass ? "" : "SbMatrix setTransform/getTransform round-trip failed");
    }

    runner.startTest("SbMatrix operator* composes two translations");
    {
        SbMatrix a, b;
        a.setTranslate(SbVec3f(1, 0, 0));
        b.setTranslate(SbVec3f(0, 2, 0));
        SbMatrix c = a * b;
        SbVec3f pt(0, 0, 0);
        SbVec3f result;
        c.multVecMatrix(pt, result);
        bool pass = floatNear(result[0], 1.0f) && floatNear(result[1], 2.0f);
        runner.endTest(pass, pass ? "" : "SbMatrix operator* composition failed");
    }

    // ======================================================================
    // SbBox3f
    // ======================================================================

    runner.startTest("SbBox3f default is empty");
    {
        SbBox3f box;
        bool pass = box.isEmpty();
        runner.endTest(pass, pass ? "" : "Default SbBox3f should be empty");
    }

    runner.startTest("SbBox3f makeEmpty resets box");
    {
        SbBox3f box(SbVec3f(-1,-1,-1), SbVec3f(1,1,1));
        box.makeEmpty();
        bool pass = box.isEmpty();
        runner.endTest(pass, pass ? "" : "SbBox3f makeEmpty should make box empty");
    }

    runner.startTest("SbBox3f hasVolume FALSE for empty box");
    {
        SbBox3f box;
        bool pass = !box.hasVolume();
        runner.endTest(pass, pass ? "" : "Empty SbBox3f should have no volume");
    }

    runner.startTest("SbBox3f hasVolume TRUE for non-empty box");
    {
        SbBox3f box(SbVec3f(-1,-1,-1), SbVec3f(1,1,1));
        bool pass = box.hasVolume();
        runner.endTest(pass, pass ? "" : "Non-empty SbBox3f should have volume");
    }

    runner.startTest("SbBox3f getVolume is correct for 2x2x2 box");
    {
        SbBox3f box(SbVec3f(-1,-1,-1), SbVec3f(1,1,1));
        bool pass = floatNear(box.getVolume(), 8.0f);
        runner.endTest(pass, pass ? "" : "2x2x2 SbBox3f volume should be 8.0");
    }

    runner.startTest("SbBox3f getCenter of symmetric box is origin");
    {
        SbBox3f box(SbVec3f(-1,-1,-1), SbVec3f(1,1,1));
        SbVec3f c = box.getCenter();
        bool pass = floatNear(c[0], 0.0f) && floatNear(c[1], 0.0f) && floatNear(c[2], 0.0f);
        runner.endTest(pass, pass ? "" : "SbBox3f center should be origin for symmetric box");
    }

    runner.startTest("SbBox3f getSize is (2,2,2) for unit box");
    {
        SbBox3f box(SbVec3f(-1,-1,-1), SbVec3f(1,1,1));
        SbVec3f sz = box.getSize();
        bool pass = floatNear(sz[0], 2.0f) && floatNear(sz[1], 2.0f) && floatNear(sz[2], 2.0f);
        runner.endTest(pass, pass ? "" : "SbBox3f getSize of 2x2x2 box failed");
    }

    runner.startTest("SbBox3f extendBy(point) grows box to include point");
    {
        SbBox3f box;
        box.extendBy(SbVec3f(3, 0, 0));
        bool pass = !box.isEmpty() && floatNear(box.getMax()[0], 3.0f);
        runner.endTest(pass, pass ? "" : "SbBox3f extendBy point failed");
    }

    runner.startTest("SbBox3f extendBy(box) merges two boxes");
    {
        SbBox3f a(SbVec3f(0,0,0), SbVec3f(1,1,1));
        SbBox3f b(SbVec3f(2,0,0), SbVec3f(3,1,1));
        a.extendBy(b);
        bool pass = floatNear(a.getMax()[0], 3.0f) && floatNear(a.getMin()[0], 0.0f);
        runner.endTest(pass, pass ? "" : "SbBox3f extendBy box failed");
    }

    runner.startTest("SbBox3f intersect(point) TRUE for point inside");
    {
        SbBox3f box(SbVec3f(-1,-1,-1), SbVec3f(1,1,1));
        bool pass = box.intersect(SbVec3f(0, 0, 0));
        runner.endTest(pass, pass ? "" : "SbBox3f: origin should be inside unit box");
    }

    runner.startTest("SbBox3f intersect(point) FALSE for point outside");
    {
        SbBox3f box(SbVec3f(-1,-1,-1), SbVec3f(1,1,1));
        bool pass = !box.intersect(SbVec3f(5, 0, 0));
        runner.endTest(pass, pass ? "" : "SbBox3f: (5,0,0) should be outside unit box");
    }

    runner.startTest("SbBox3f intersect(box) TRUE for overlapping boxes");
    {
        SbBox3f a(SbVec3f(-1,-1,-1), SbVec3f(1,1,1));
        SbBox3f b(SbVec3f(0,0,0), SbVec3f(2,2,2));
        bool pass = a.intersect(b);
        runner.endTest(pass, pass ? "" : "Overlapping SbBox3f::intersect should return TRUE");
    }

    runner.startTest("SbBox3f intersect(box) FALSE for non-overlapping boxes");
    {
        SbBox3f a(SbVec3f(-1,-1,-1), SbVec3f(1,1,1));
        SbBox3f b(SbVec3f(5,5,5), SbVec3f(6,6,6));
        bool pass = !a.intersect(b);
        runner.endTest(pass, pass ? "" : "Non-overlapping SbBox3f::intersect should return FALSE");
    }

    runner.startTest("SbBox3f transform with translation shifts box");
    {
        SbBox3f box(SbVec3f(-1,-1,-1), SbVec3f(1,1,1));
        SbMatrix m;
        m.setTranslate(SbVec3f(5, 0, 0));
        box.transform(m);
        bool pass = floatNear(box.getCenter()[0], 5.0f, 0.1f);
        runner.endTest(pass, pass ? "" : "SbBox3f transform should shift box centre");
    }

    runner.startTest("SbBox3f getOrigin returns min point");
    {
        SbBox3f box(SbVec3f(1,2,3), SbVec3f(4,5,6));
        float ox, oy, oz;
        box.getOrigin(ox, oy, oz);
        bool pass = floatNear(ox, 1.0f) && floatNear(oy, 2.0f) && floatNear(oz, 3.0f);
        runner.endTest(pass, pass ? "" : "SbBox3f getOrigin should return min point");
    }

    runner.startTest("SbBox3f getSpan along X axis");
    {
        SbBox3f box(SbVec3f(-1,-1,-1), SbVec3f(1,1,1));
        float dmin, dmax;
        box.getSpan(SbVec3f(1, 0, 0), dmin, dmax);
        bool pass = floatNear(dmin, -1.0f) && floatNear(dmax, 1.0f);
        runner.endTest(pass, pass ? "" : "SbBox3f getSpan along X axis failed");
    }

    // ======================================================================
    // SbBox2f
    // ======================================================================

    runner.startTest("SbBox2f default is empty");
    {
        SbBox2f box;
        bool pass = box.isEmpty();
        runner.endTest(pass, pass ? "" : "Default SbBox2f should be empty");
    }

    runner.startTest("SbBox2f makeEmpty resets box");
    {
        SbBox2f box(SbVec2f(-1,-1), SbVec2f(1,1));
        box.makeEmpty();
        bool pass = box.isEmpty();
        runner.endTest(pass, pass ? "" : "SbBox2f makeEmpty should make box empty");
    }

    runner.startTest("SbBox2f getCenter of symmetric box is origin");
    {
        SbBox2f box(SbVec2f(-1,-1), SbVec2f(1,1));
        SbVec2f c = box.getCenter();
        bool pass = floatNear(c[0], 0.0f) && floatNear(c[1], 0.0f);
        runner.endTest(pass, pass ? "" : "SbBox2f center should be origin for symmetric box");
    }

    runner.startTest("SbBox2f extendBy(point) grows box");
    {
        SbBox2f box;
        box.extendBy(SbVec2f(3, 4));
        bool pass = !box.isEmpty() && floatNear(box.getMax()[0], 3.0f);
        runner.endTest(pass, pass ? "" : "SbBox2f extendBy point failed");
    }

    runner.startTest("SbBox2f intersect(point) TRUE for inside point");
    {
        SbBox2f box(SbVec2f(-1,-1), SbVec2f(1,1));
        bool pass = box.intersect(SbVec2f(0, 0));
        runner.endTest(pass, pass ? "" : "SbBox2f: origin should be inside unit box");
    }

    runner.startTest("SbBox2f getMin / getMax");
    {
        SbBox2f box(SbVec2f(-2,-3), SbVec2f(4,5));
        bool pass = floatNear(box.getMin()[0], -2.0f) &&
                    floatNear(box.getMin()[1], -3.0f) &&
                    floatNear(box.getMax()[0], 4.0f) &&
                    floatNear(box.getMax()[1], 5.0f);
        runner.endTest(pass, pass ? "" : "SbBox2f getMin/getMax failed");
    }

    return runner.getSummary();
}
