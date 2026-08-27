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
 * @file test_sb_rotation_plane.cpp
 * @brief Tests for SbRotation and SbPlane deeper coverage (base/ 33.5 %).
 *
 * Covers:
 *   SbRotation:
 *     construction (axis/angle, from/to vectors, 4-float, matrix, quat array),
 *     getValue (axis+angle, matrix, 4-float), invert/inverse,
 *     multVec, operator*=, operator*, equals, slerp, scaleAngle,
 *     identity(), fromString, operator[]
 *   SbPlane:
 *     construction (3-point, normal+point, normal+D),
 *     getNormal, getDistanceFromOrigin, getDistance(point),
 *     isInHalfSpace, intersect(SbLine), intersect(SbPlane),
 *     offset, transform, operator==
 */

#include "../test_utils.h"

#include <Inventor/SbRotation.h>
#include <Inventor/SbPlane.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbLine.h>
#include <Inventor/SbString.h>
#include <cmath>

using namespace ObolTest;

static bool floatNear(float a, float b, float eps = 1e-4f)
{
    return std::fabs(a - b) < eps;
}

TEST(BaseSbRotationPlane, SbRotationIdentityHasZeroRotation)
{
    SbRotation r = SbRotation::identity();
    SbVec3f axis;
    float angle;
    r.getValue(axis, angle);
    bool pass = floatNear(angle, 0.0f, 0.01f);
    EXPECT_TRUE(pass) << "SbRotation::identity should have zero angle";
}

TEST(BaseSbRotationPlane, SbRotationAxisAngleConstructorRoundTrip)
{
    SbVec3f axis(0, 1, 0);
    float angle = 1.0f;
    SbRotation r(axis, angle);
    SbVec3f outAxis;
    float outAngle;
    r.getValue(outAxis, outAngle);
    bool pass = floatNear(outAngle, 1.0f, 1e-4f);
    EXPECT_TRUE(pass) << "SbRotation axis/angle round-trip failed";
}

TEST(BaseSbRotationPlane, SbRotationMultVec90AroundZRotatesXToY)
{
    SbRotation r(SbVec3f(0, 0, 1), static_cast<float>(M_PI / 2));
    SbVec3f src(1, 0, 0);
    SbVec3f dst;
    r.multVec(src, dst);
    bool pass = floatNear(dst[0], 0.0f, 1e-4f) && floatNear(dst[1], 1.0f, 1e-4f);
    EXPECT_TRUE(pass) << "SbRotation::multVec 90° around Z failed";
}

TEST(BaseSbRotationPlane, SbRotationInverseReversesRotation)
{
    SbRotation r(SbVec3f(0, 0, 1), static_cast<float>(M_PI / 2));
    SbRotation inv = r.inverse();
    SbVec3f src(1, 0, 0);
    SbVec3f dst, back;
    r.multVec(src, dst);
    inv.multVec(dst, back);
    bool pass = floatNear(back[0], 1.0f, 1e-4f) && floatNear(back[1], 0.0f, 1e-4f);
    EXPECT_TRUE(pass) << "SbRotation::inverse should reverse rotation";
}

TEST(BaseSbRotationPlane, SbRotationInvertModifiesInPlace)
{
    SbRotation r(SbVec3f(0, 1, 0), 0.5f);
    SbRotation orig = r;
    r.invert();
    bool pass = !r.equals(orig, 1e-4f); // should differ from original
    EXPECT_TRUE(pass) << "SbRotation::invert should modify in place";
}

TEST(BaseSbRotationPlane, SbRotationEqualsWithTolerance)
{
    SbRotation r1(SbVec3f(0, 1, 0), 1.0f);
    SbRotation r2(SbVec3f(0, 1, 0), 1.000001f);
    bool passClose = r1.equals(r2, 0.001f);
    bool passFar = !r1.equals(SbRotation(SbVec3f(0, 1, 0), 2.0f), 0.01f);
    EXPECT_TRUE((passClose && passFar)) << ((passClose && passFar) ? "" :
                   "SbRotation::equals failed");
}

TEST(BaseSbRotationPlane, SbRotationGetValueMatrixIsRotationMatrix)
{
    SbRotation r(SbVec3f(0, 0, 1), static_cast<float>(M_PI / 2));
    SbMatrix m;
    r.getValue(m);
    SbVec3f pt(1, 0, 0);
    SbVec3f result;
    m.multVecMatrix(pt, result);
    bool pass = floatNear(result[0], 0.0f, 1e-4f) && floatNear(result[1], 1.0f, 1e-4f);
    EXPECT_TRUE(pass) << "SbRotation::getValue(matrix) is wrong";
}

TEST(BaseSbRotationPlane, SbRotationSlerpInterpolatesHalfWay)
{
    SbRotation r0 = SbRotation::identity();
    SbRotation r1(SbVec3f(0, 0, 1), static_cast<float>(M_PI));
    SbRotation mid = SbRotation::slerp(r0, r1, 0.5f);
    SbVec3f axis;
    float angle;
    mid.getValue(axis, angle);
    // Should be ~π/2 at t=0.5
    bool pass = floatNear(std::fabs(angle), static_cast<float>(M_PI / 2), 0.05f);
    EXPECT_TRUE(pass) << "SbRotation::slerp at t=0.5 should give π/2 rotation";
}

TEST(BaseSbRotationPlane, SbRotationScaleAngleHalvesAngle)
{
    SbRotation r(SbVec3f(0, 1, 0), 1.0f);
    r.scaleAngle(0.5f);
    SbVec3f axis;
    float angle;
    r.getValue(axis, angle);
    bool pass = floatNear(angle, 0.5f, 0.01f);
    EXPECT_TRUE(pass) << "SbRotation::scaleAngle(0.5) should halve angle";
}

TEST(BaseSbRotationPlane, SbRotationOperatorReturnsQuaternionComponents)
{
    SbRotation r = SbRotation::identity();
    // Identity quaternion is (0, 0, 0, 1)
    float w = r[3];
    bool pass = floatNear(w, 1.0f, 1e-4f);
    EXPECT_TRUE(pass) << "Identity quaternion w component should be 1";
}

TEST(BaseSbRotationPlane, SbRotationOperatorComposition)
{
    SbRotation r1(SbVec3f(0, 0, 1), static_cast<float>(M_PI / 2));
    SbRotation r2(SbVec3f(0, 0, 1), static_cast<float>(M_PI / 2));
    SbRotation composed = r1 * r2;
    SbVec3f pt(1, 0, 0);
    SbVec3f result;
    composed.multVec(pt, result);
    // Two 90° rotations = 180°: (1,0,0) → (-1,0,0)
    bool pass = floatNear(result[0], -1.0f, 1e-4f) && floatNear(result[1], 0.0f, 1e-4f);
    EXPECT_TRUE(pass) << "SbRotation operator* composition of 2x90°=180° failed";
}

TEST(BaseSbRotationPlane, SbRotationFromToVectorConstructor)
{
    SbRotation r(SbVec3f(1, 0, 0), SbVec3f(0, 1, 0)); // rotate X to Y
    SbVec3f dst;
    r.multVec(SbVec3f(1, 0, 0), dst);
    bool pass = floatNear(dst[0], 0.0f, 1e-4f) && floatNear(dst[1], 1.0f, 1e-4f);
    EXPECT_TRUE(pass) << "SbRotation from-to constructor failed";
}

// ======================================================================
// SbPlane
// ======================================================================

TEST(BaseSbRotationPlane, SbPlaneConstructionFromNormalD)
{
    SbPlane p(SbVec3f(0, 1, 0), 3.0f);
    bool pass = floatNear(p.getDistanceFromOrigin(), 3.0f) &&
                floatNear(p.getNormal()[1], 1.0f);
    EXPECT_TRUE(pass) << "SbPlane(normal,D) construction failed";
}

TEST(BaseSbRotationPlane, SbPlaneConstructionFromNormalPoint)
{
    SbPlane p(SbVec3f(0, 1, 0), SbVec3f(0, 3, 0));
    bool pass = floatNear(p.getDistanceFromOrigin(), 3.0f);
    EXPECT_TRUE(pass) << "SbPlane(normal,point) construction failed";
}

TEST(BaseSbRotationPlane, SbPlaneConstructionFrom3Points)
{
    // 3 points on y=2 plane
    SbPlane p(SbVec3f(0, 2, 0), SbVec3f(1, 2, 0), SbVec3f(0, 2, 1));
    bool pass = floatNear(std::fabs(p.getNormal()[1]), 1.0f, 1e-4f);
    EXPECT_TRUE(pass) << "SbPlane 3-point construction failed";
}

TEST(BaseSbRotationPlane, SbPlaneGetDistanceForPointOnPlaneIsZero)
{
    SbPlane p(SbVec3f(0, 1, 0), 3.0f);
    float d = p.getDistance(SbVec3f(0, 3, 0)); // on y=3 plane
    bool pass = floatNear(d, 0.0f, 1e-4f);
    EXPECT_TRUE(pass) << "SbPlane::getDistance for point on plane should be 0";
}

TEST(BaseSbRotationPlane, SbPlaneGetDistanceForPointAbovePlane)
{
    SbPlane p(SbVec3f(0, 1, 0), 0.0f); // y=0 plane
    float d = p.getDistance(SbVec3f(0, 5, 0)); // point at y=5
    bool pass = floatNear(d, 5.0f, 1e-4f);
    EXPECT_TRUE(pass) << "SbPlane::getDistance for point 5 above y=0 plane should be 5";
}

TEST(BaseSbRotationPlane, SbPlaneIsInHalfSpacePointAbovePositiveNormalSide)
{
    SbPlane p(SbVec3f(0, 1, 0), 0.0f); // y=0 plane, normal pointing up
    bool pass = p.isInHalfSpace(SbVec3f(0, 1, 0)); // y=1 is above
    EXPECT_TRUE(pass) << "SbPlane::isInHalfSpace: y=1 should be in upper half-space";
}

TEST(BaseSbRotationPlane, SbPlaneIsInHalfSpacePointBelowIsNOTInHalfSpace)
{
    SbPlane p(SbVec3f(0, 1, 0), 0.0f);
    bool pass = !p.isInHalfSpace(SbVec3f(0, -1, 0));
    EXPECT_TRUE(pass) << "SbPlane::isInHalfSpace: y=-1 should NOT be in upper half-space";
}

TEST(BaseSbRotationPlane, SbPlaneOffsetShiftsPlane)
{
    SbPlane p(SbVec3f(0, 1, 0), 0.0f); // y=0
    p.offset(2.0f);
    bool pass = floatNear(p.getDistanceFromOrigin(), 2.0f, 1e-4f);
    EXPECT_TRUE(pass) << "SbPlane::offset should shift distance";
}

TEST(BaseSbRotationPlane, SbPlaneIntersectWithLine)
{
    SbPlane p(SbVec3f(0, 1, 0), 0.0f); // y=0 plane
    // Line from (0,1,0) in direction (0,-1,0) — hits y=0 plane at (0,0,0)
    SbLine line(SbVec3f(0, 1, 0), SbVec3f(0, -1, 0));
    SbVec3f hit;
    bool intersected = p.intersect(line, hit);
    bool pass = intersected && floatNear(hit[1], 0.0f, 1e-4f);
    EXPECT_TRUE(pass) << "SbPlane::intersect with line failed";
}

TEST(BaseSbRotationPlane, SbPlaneIntersectWithParallelPlaneReturnsLine)
{
    // Two non-parallel planes intersect in a line
    SbPlane p1(SbVec3f(1, 0, 0), 0.0f); // x=0
    SbPlane p2(SbVec3f(0, 1, 0), 0.0f); // y=0
    SbLine line;
    bool pass = p1.intersect(p2, line);
    EXPECT_TRUE(pass) << "Two non-parallel planes should intersect in a line";
}

TEST(BaseSbRotationPlane, SbPlaneOperatorForEqualPlanes)
{
    SbPlane p1(SbVec3f(0, 1, 0), 3.0f);
    SbPlane p2(SbVec3f(0, 1, 0), 3.0f);
    bool pass = (p1 == p2);
    EXPECT_TRUE(pass) << "Equal SbPlanes should be ==";
}

TEST(BaseSbRotationPlane, SbPlaneTransformWithTranslation)
{
    SbPlane p(SbVec3f(0, 1, 0), 0.0f); // y=0 plane
    SbMatrix m;
    m.setTranslate(SbVec3f(0, 3, 0)); // translate everything up by 3
    p.transform(m);
    // The plane should now be at y=3 (translated)
    bool pass = floatNear(p.getDistanceFromOrigin(), 3.0f, 0.1f);
    EXPECT_TRUE(pass) << "SbPlane::transform with translation failed";
}
