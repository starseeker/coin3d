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
 * @file test_base_advanced.cpp
 * @brief Additional tests for Coin3D base types not covered by existing test files.
 *
 * Covers APIs not exercised in test_sb_types.cpp, test_sbdpmatrix_full.cpp,
 * test_base_extras.cpp, test_sbtime_sbdict.cpp, test_sb_matrix_box.cpp, or
 * test_sb_rotation_plane.cpp:
 *
 *   SbVec2f  - dot, length, normalize, arithmetic operators, == / !=
 *   SbVec2d  - construction, setValue/getValue round-trip, dot, length
 *   SbVec4d  - construction, setValue/getValue round-trip, dot with self
 *   SbMatrix - det4() on identity matrix
 *   SbRotation - slerp at endpoints t=0 and t=1
 *   SbTime   - getTimeOfDay(), setToTimeOfDay()
 *   SbName   - getString(), getLength(), operator==, operator!=
 */

#include "../test_utils.h"

#include <Inventor/SbVec2f.h>
#include <Inventor/SbVec2d.h>
#include <Inventor/SbVec4d.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbRotation.h>
#include <Inventor/SbTime.h>
#include <Inventor/SbName.h>
#include <cmath>
#include <cstring>

using namespace ObolTest;

static bool floatNear(float a, float b, float tol = 1e-5f) {
    return fabsf(a - b) <= tol;
}

static bool doubleNear(double a, double b, double tol = 1e-10) {
    return fabs(a - b) <= tol;
}

TEST(BaseAdvanced, SbVec2fSetValueGetValueRoundTrip)
{
    SbVec2f v;
    v.setValue(3.0f, 4.0f);
    float x, y;
    v.getValue(x, y);
    bool pass = floatNear(x, 3.0f) && floatNear(y, 4.0f);
    EXPECT_TRUE(pass) << "SbVec2f setValue/getValue round-trip failed";
}

// -----------------------------------------------------------------------
// SbVec2f: dot product
// -----------------------------------------------------------------------

TEST(BaseAdvanced, SbVec2fDot10010)
{
    SbVec2f a(1.0f, 0.0f), b(0.0f, 1.0f);
    bool pass = floatNear(a.dot(b), 0.0f);
    EXPECT_TRUE(pass) << "SbVec2f orthogonal dot product should be 0";
}

TEST(BaseAdvanced, SbVec2fDot10101)
{
    SbVec2f a(1.0f, 0.0f);
    bool pass = floatNear(a.dot(a), 1.0f);
    EXPECT_TRUE(pass) << "SbVec2f unit self-dot should be 1";
}

// -----------------------------------------------------------------------
// SbVec2f: length
// -----------------------------------------------------------------------

TEST(BaseAdvanced, SbVec2fLengthVec34Length5)
{
    SbVec2f v(3.0f, 4.0f);
    bool pass = floatNear(v.length(), 5.0f);
    EXPECT_TRUE(pass) << "SbVec2f length(3,4) should be 5";
}

// -----------------------------------------------------------------------
// SbVec2f: normalize
// -----------------------------------------------------------------------

TEST(BaseAdvanced, SbVec2fNormalizeLengthBecomes1)
{
    SbVec2f v(3.0f, 4.0f);
    v.normalize();
    bool pass = floatNear(v.length(), 1.0f);
    EXPECT_TRUE(pass) << "SbVec2f normalize should produce unit length";
}

// -----------------------------------------------------------------------
// SbVec2f: arithmetic operators
// -----------------------------------------------------------------------

TEST(BaseAdvanced, SbVec2fOperator)
{
    SbVec2f a(1.0f, 2.0f), b(3.0f, 4.0f);
    SbVec2f c = a + b;
    bool pass = floatNear(c[0], 4.0f) && floatNear(c[1], 6.0f);
    EXPECT_TRUE(pass) << "SbVec2f operator+ failed";
}

TEST(BaseAdvanced, SbVec2fOperator2)
{
    SbVec2f a(5.0f, 7.0f), b(2.0f, 3.0f);
    SbVec2f c = a - b;
    bool pass = floatNear(c[0], 3.0f) && floatNear(c[1], 4.0f);
    EXPECT_TRUE(pass) << "SbVec2f operator- failed";
}

TEST(BaseAdvanced, SbVec2fOperatorScalar)
{
    SbVec2f a(1.0f, 2.0f);
    SbVec2f c = a * 3.0f;
    bool pass = floatNear(c[0], 3.0f) && floatNear(c[1], 6.0f);
    EXPECT_TRUE(pass) << "SbVec2f operator* scalar failed";
}

TEST(BaseAdvanced, SbVec2fOperatorScalar2)
{
    SbVec2f a(4.0f, 6.0f);
    SbVec2f c = a / 2.0f;
    bool pass = floatNear(c[0], 2.0f) && floatNear(c[1], 3.0f);
    EXPECT_TRUE(pass) << "SbVec2f operator/ scalar failed";
}

// -----------------------------------------------------------------------
// SbVec2f: equality operators
// -----------------------------------------------------------------------

TEST(BaseAdvanced, SbVec2fOperator3)
{
    SbVec2f a(1.0f, 2.0f), b(1.0f, 2.0f);
    bool pass = (a == b);
    EXPECT_TRUE(pass) << "SbVec2f operator== failed for equal vectors";
}

TEST(BaseAdvanced, SbVec2fOperator4)
{
    SbVec2f a(1.0f, 2.0f), b(3.0f, 4.0f);
    bool pass = (a != b);
    EXPECT_TRUE(pass) << "SbVec2f operator!= failed for different vectors";
}

// -----------------------------------------------------------------------
// SbVec2d: construction, setValue/getValue round-trip
// -----------------------------------------------------------------------

TEST(BaseAdvanced, SbVec2dSetValueGetValueRoundTrip)
{
    SbVec2d v;
    v.setValue(1.5, 2.5);
    double x, y;
    v.getValue(x, y);
    bool pass = doubleNear(x, 1.5) && doubleNear(y, 2.5);
    EXPECT_TRUE(pass) << "SbVec2d setValue/getValue round-trip failed";
}

// -----------------------------------------------------------------------
// SbVec2d: dot product
// -----------------------------------------------------------------------

TEST(BaseAdvanced, SbVec2dDot10010)
{
    SbVec2d a(1.0, 0.0), b(0.0, 1.0);
    bool pass = doubleNear(a.dot(b), 0.0);
    EXPECT_TRUE(pass) << "SbVec2d orthogonal dot product should be 0";
}

// -----------------------------------------------------------------------
// SbVec2d: length
// -----------------------------------------------------------------------

TEST(BaseAdvanced, SbVec2dLengthVec34Length5)
{
    SbVec2d v(3.0, 4.0);
    bool pass = doubleNear(v.length(), 5.0);
    EXPECT_TRUE(pass) << "SbVec2d length(3,4) should be 5";
}

// -----------------------------------------------------------------------
// SbVec4d: construction, setValue/getValue round-trip
// -----------------------------------------------------------------------

TEST(BaseAdvanced, SbVec4dSetValueGetValueRoundTrip)
{
    SbVec4d v;
    v.setValue(1.0, 2.0, 3.0, 4.0);
    double x, y, z, w;
    v.getValue(x, y, z, w);
    bool pass = doubleNear(x, 1.0) && doubleNear(y, 2.0) &&
                doubleNear(z, 3.0) && doubleNear(w, 4.0);
    EXPECT_TRUE(pass) << "SbVec4d setValue/getValue round-trip failed";
}

// -----------------------------------------------------------------------
// SbVec4d: dot product with self equals squared length
// -----------------------------------------------------------------------

TEST(BaseAdvanced, SbVec4dDotSelfDotEqualsSquaredLength)
{
    SbVec4d v(1.0, 2.0, 3.0, 4.0);
    double expected = 1.0 + 4.0 + 9.0 + 16.0; // 30
    bool pass = doubleNear(v.dot(v), expected);
    EXPECT_TRUE(pass) << "SbVec4d self-dot should equal sum of squares";
}

// -----------------------------------------------------------------------
// SbMatrix: det4() on identity is 1
// -----------------------------------------------------------------------

TEST(BaseAdvanced, SbMatrixDet4OfIdentityIs1)
{
    SbMatrix m;
    m.makeIdentity();
    float d = m.det4();
    bool pass = floatNear(d, 1.0f);
    EXPECT_TRUE(pass) << "SbMatrix identity det4() should be 1";
}

// -----------------------------------------------------------------------
// SbRotation: slerp(a, b, 0.0) == a  and  slerp(a, b, 1.0) == b
// (half-way is tested in test_sb_rotation_plane.cpp; endpoints are not)
// -----------------------------------------------------------------------

TEST(BaseAdvanced, SbRotationSlerpAtT0ReturnsFirstRotation)
{
    SbRotation r0(SbVec3f(0.0f, 0.0f, 1.0f), 0.0f);
    SbRotation r1(SbVec3f(0.0f, 0.0f, 1.0f), static_cast<float>(M_PI));
    SbRotation result = SbRotation::slerp(r0, r1, 0.0f);
    float q0, q1, q2, q3, e0, e1, e2, e3;
    result.getValue(q0, q1, q2, q3);
    r0.getValue(e0, e1, e2, e3);
    bool pass = floatNear(q0, e0) && floatNear(q1, e1) &&
                floatNear(q2, e2) && floatNear(q3, e3);
    EXPECT_TRUE(pass) << "SbRotation slerp(r0,r1,0) should equal r0";
}

TEST(BaseAdvanced, SbRotationSlerpAtT1ReturnsSecondRotation)
{
    SbRotation r0(SbVec3f(0.0f, 0.0f, 1.0f), 0.0f);
    SbRotation r1(SbVec3f(0.0f, 0.0f, 1.0f), static_cast<float>(M_PI / 2.0));
    SbRotation result = SbRotation::slerp(r0, r1, 1.0f);
    SbVec3f axis; float angle;
    result.getValue(axis, angle);
    SbVec3f eAxis; float eAngle;
    r1.getValue(eAxis, eAngle);
    bool pass = floatNear(angle, eAngle, 1e-4f);
    EXPECT_TRUE(pass) << "SbRotation slerp(r0,r1,1) should equal r1";
}

// -----------------------------------------------------------------------
// SbTime: getTimeOfDay() returns a positive value
// -----------------------------------------------------------------------

TEST(BaseAdvanced, SbTimeGetTimeOfDayReturnsPositiveValue)
{
    SbTime t = SbTime::getTimeOfDay();
    bool pass = (t.getValue() > 0.0);
    EXPECT_TRUE(pass) << "SbTime::getTimeOfDay() should return positive time";
}

// -----------------------------------------------------------------------
// SbTime: setToTimeOfDay() does not crash and updates to positive value
// -----------------------------------------------------------------------

TEST(BaseAdvanced, SbTimeSetToTimeOfDaySetsPositiveValue)
{
    SbTime t;
    t.setToTimeOfDay();
    bool pass = (t.getValue() > 0.0);
    EXPECT_TRUE(pass) << "SbTime::setToTimeOfDay() should set a positive time";
}

// -----------------------------------------------------------------------
// SbName: construction from string, getString() round-trip
// -----------------------------------------------------------------------

TEST(BaseAdvanced, SbNameGetStringReturnsOriginalString)
{
    SbName n("hello");
    bool pass = (strcmp(n.getString(), "hello") == 0);
    EXPECT_TRUE(pass) << "SbName getString() did not return original string";
}

// -----------------------------------------------------------------------
// SbName: getLength() matches strlen
// -----------------------------------------------------------------------

TEST(BaseAdvanced, SbNameGetLengthMatchesStrlen)
{
    SbName n("world");
    bool pass = (n.getLength() == static_cast<int>(strlen("world")));
    EXPECT_TRUE(pass) << "SbName getLength() should equal strlen of string";
}

// -----------------------------------------------------------------------
// SbName: operator== for same names
// -----------------------------------------------------------------------

TEST(BaseAdvanced, SbNameOperatorSameName)
{
    SbName a("foo"), b("foo");
    bool pass = (a == b);
    EXPECT_TRUE(pass) << "SbName operator== should be true for same string";
}

// -----------------------------------------------------------------------
// SbName: operator!= for different names
// -----------------------------------------------------------------------

TEST(BaseAdvanced, SbNameOperatorDifferentNames)
{
    SbName a("foo"), b("bar");
    bool pass = (a != b);
    EXPECT_TRUE(pass) << "SbName operator!= should be true for different strings";
}

// -----------------------------------------------------------------------
// SbName: operator== with char*
// -----------------------------------------------------------------------

TEST(BaseAdvanced, SbNameOperatorWithChar)
{
    SbName n("baz");
    bool pass = (n == "baz");
    EXPECT_TRUE(pass) << "SbName operator==(char*) should match";
}

// -----------------------------------------------------------------------
// SbName: empty name has zero length
// -----------------------------------------------------------------------

TEST(BaseAdvanced, SbNameEmptyHasZeroLength)
{
    const SbName & e = SbName::empty();
    bool pass = (e.getLength() == 0);
    EXPECT_TRUE(pass) << "SbName::empty() should have length 0";
}
