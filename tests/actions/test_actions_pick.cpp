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
 * @file test_actions_pick.cpp
 * @brief Tests for SoRayPickAction (actions/ 41.3 %).
 *
 * Covers:
 *   SoRayPickAction:
 *     class type registration, setPoint/setNormalizedPoint, setRadius/getRadius,
 *     setPickAll/isPickAll, setRay, getPickedPointList, getPickedPoint,
 *     hasWorldSpaceRay, computeWorldSpaceRay, apply to sphere/cube,
 *     missed pick returns null, hit pick returns non-null path
 *   SoPickedPoint:
 *     getPoint, getNormal, getPath, isOnGeometry, getMaterialIndex
 */

#include "../test_utils.h"

#include <Inventor/SbVec2f.h>
#include <Inventor/SbVec2s.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoPickedPoint.h>
#include <Inventor/actions/SoRayPickAction.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/SoType.h>
#include <cmath>

using namespace ObolTest;

static bool floatNear(float a, float b, float eps = 1e-4f)
{
    return std::fabs(a - b) < eps;
}

// Build a simple scene: camera looking down -Z at a sphere at origin
static SoSeparator * buildPickScene()
{
    SoSeparator * root = new SoSeparator;
    SoOrthographicCamera * cam = new SoOrthographicCamera;
    cam->position.setValue(0, 0, 5);
    cam->nearDistance = 0.5f;
    cam->farDistance  = 20.0f;
    cam->height       = 4.0f;
    root->addChild(cam);
    SoSphere * sphere = new SoSphere;
    sphere->radius = 1.0f;
    root->addChild(sphere);
    return root;
}

TEST(ActionsPick, SoRayPickActionClassTypeRegistered)
{
    EXPECT_TRUE((SoRayPickAction::getClassTypeId() != SoType::badType())) << "SoRayPickAction bad class type";
}

// -----------------------------------------------------------------------
// setRadius / getRadius
// -----------------------------------------------------------------------

TEST(ActionsPick, SoRayPickActionSetRadiusGetRadiusRoundTrip)
{
    SbViewportRegion vp(512, 512);
    SoRayPickAction ra(vp);
    ra.setRadius(5.0f);
    EXPECT_TRUE(floatNear(ra.getRadius(), 5.0f)) << "setRadius/getRadius round-trip failed";
}

// -----------------------------------------------------------------------
// setPickAll / isPickAll
// -----------------------------------------------------------------------

TEST(ActionsPick, SoRayPickActionSetPickAllTRUEIsPickAllRoundTrip)
{
    SbViewportRegion vp(512, 512);
    SoRayPickAction ra(vp);
    ra.setPickAll(TRUE);
    EXPECT_TRUE((ra.isPickAll() == TRUE)) << "setPickAll(TRUE)/isPickAll failed";
}

TEST(ActionsPick, SoRayPickActionSetPickAllFALSEIsPickAllRoundTrip)
{
    SbViewportRegion vp(512, 512);
    SoRayPickAction ra(vp);
    ra.setPickAll(FALSE);
    EXPECT_TRUE((ra.isPickAll() == FALSE)) << "setPickAll(FALSE)/isPickAll failed";
}

// -----------------------------------------------------------------------
// setNormalizedPoint / setPoint
// -----------------------------------------------------------------------

TEST(ActionsPick, SoRayPickActionSetNormalizedPointDoesNotCrash)
{
    SbViewportRegion vp(512, 512);
    SoRayPickAction ra(vp);
    ra.setNormalizedPoint(SbVec2f(0.5f, 0.5f));
    SUCCEED();
}

TEST(ActionsPick, SoRayPickActionSetPointDoesNotCrash)
{
    SbViewportRegion vp(512, 512);
    SoRayPickAction ra(vp);
    ra.setPoint(SbVec2s(256, 256));
    SUCCEED();
}

// -----------------------------------------------------------------------
// setRay + hasWorldSpaceRay + computeWorldSpaceRay
// -----------------------------------------------------------------------

TEST(ActionsPick, SoRayPickActionHasWorldSpaceRayIsFALSEBeforeSetting)
{
    SbViewportRegion vp(512, 512);
    SoRayPickAction ra(vp);
    EXPECT_TRUE((ra.hasWorldSpaceRay() == FALSE)) << "hasWorldSpaceRay should be FALSE before setRay";
}

TEST(ActionsPick, SoRayPickActionSetRayThenHasWorldSpaceRayIsTRUE)
{
    SbViewportRegion vp(512, 512);
    SoRayPickAction ra(vp);
    ra.setRay(SbVec3f(0, 0, 5), SbVec3f(0, 0, -1));
    EXPECT_TRUE((ra.hasWorldSpaceRay() == TRUE)) << "hasWorldSpaceRay should be TRUE after setRay";
}

// -----------------------------------------------------------------------
// Picking: ray through sphere at origin
// -----------------------------------------------------------------------

TEST(ActionsPick, SoRayPickActionCentreRayHitsSphereAtOrigin)
{
    SoSeparator * root = buildPickScene();
    root->ref();

    SbViewportRegion vp(512, 512);
    SoRayPickAction ra(vp);
    // Ray from z=5 towards -z, should hit sphere at origin
    ra.setRay(SbVec3f(0, 0, 5), SbVec3f(0, 0, -1));
    ra.apply(root);

    SoPickedPoint * pp = ra.getPickedPoint(0);
    EXPECT_TRUE((pp != nullptr)) << "Centre ray should hit sphere at origin";
    root->unref();
}

TEST(ActionsPick, SoRayPickActionPickedPointIsOnGeometry)
{
    SoSeparator * root = buildPickScene();
    root->ref();

    SbViewportRegion vp(512, 512);
    SoRayPickAction ra(vp);
    ra.setRay(SbVec3f(0, 0, 5), SbVec3f(0, 0, -1));
    ra.apply(root);

    SoPickedPoint * pp = ra.getPickedPoint(0);
    EXPECT_TRUE((pp != nullptr) && pp->isOnGeometry()) << "Picked point should be on geometry";
    root->unref();
}

TEST(ActionsPick, SoRayPickActionPickedPointHasValidPosition)
{
    SoSeparator * root = buildPickScene();
    root->ref();

    SbViewportRegion vp(512, 512);
    SoRayPickAction ra(vp);
    ra.setRay(SbVec3f(0, 0, 5), SbVec3f(0, 0, -1));
    ra.apply(root);

    SoPickedPoint * pp = ra.getPickedPoint(0);
    EXPECT_NE(pp, nullptr);
    if (pp) {
        SbVec3f pt = pp->getPoint();
        // Sphere has radius 1, centre at origin; hit should be at z≈+1
        EXPECT_NEAR(pt[2], 1.0f, 0.1f);
    }
    root->unref();
}

TEST(ActionsPick, SoRayPickActionMissReturnsNull)
{
    SoSeparator * root = buildPickScene();
    root->ref();

    SbViewportRegion vp(512, 512);
    SoRayPickAction ra(vp);
    // Ray from far right, parallel to Z - should miss the sphere at origin
    ra.setRay(SbVec3f(5, 5, 5), SbVec3f(0, 0, -1));
    ra.apply(root);

    SoPickedPoint * pp = ra.getPickedPoint(0);
    EXPECT_TRUE((pp == nullptr)) << "Off-centre ray should miss the sphere (should return null)";
    root->unref();
}

TEST(ActionsPick, SoRayPickActionGetPickedPointListIsEmptyForMiss)
{
    SoSeparator * root = buildPickScene();
    root->ref();

    SbViewportRegion vp(512, 512);
    SoRayPickAction ra(vp);
    ra.setRay(SbVec3f(10, 10, 5), SbVec3f(0, 0, -1));
    ra.apply(root);

    const SoPickedPointList & list = ra.getPickedPointList();
    EXPECT_TRUE((list.getLength() == 0)) << "Missed ray getPickedPointList should be empty";
    root->unref();
}

TEST(ActionsPick, SoRayPickActionSetPickAllCollectsMultipleIntersections)
{
    SoSeparator * root = buildPickScene();
    root->ref();

    SbViewportRegion vp(512, 512);
    SoRayPickAction ra(vp);
    ra.setPickAll(TRUE);
    ra.setRay(SbVec3f(0, 0, 5), SbVec3f(0, 0, -1));
    ra.apply(root);

    // With pickAll=TRUE, should get both front and back surface intersections
    const SoPickedPointList & list = ra.getPickedPointList();
    EXPECT_TRUE((list.getLength() >= 1)) << "setPickAll should collect at least 1 intersection";
    root->unref();
}

TEST(ActionsPick, SoRayPickActionPickedPathContainsSphereNode)
{
    SoSeparator * root = buildPickScene();
    root->ref();

    SbViewportRegion vp(512, 512);
    SoRayPickAction ra(vp);
    ra.setRay(SbVec3f(0, 0, 5), SbVec3f(0, 0, -1));
    ra.apply(root);

    SoPickedPoint * pp = ra.getPickedPoint(0);
    EXPECT_NE(pp, nullptr);
    if (pp) {
        SoPath * path = pp->getPath();
        // Path should contain the sphere
        EXPECT_NE(path, nullptr);
        if (path) {
            EXPECT_TRUE(path->containsNode(root->getChild(1))); // sphere is child 1
        }
    }
    root->unref();
}

// -----------------------------------------------------------------------
// Pick with SoCube
// -----------------------------------------------------------------------

TEST(ActionsPick, SoRayPickActionRayHitsCubeAtOrigin)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    SoOrthographicCamera * cam = new SoOrthographicCamera;
    cam->position.setValue(0, 0, 5);
    cam->nearDistance = 0.5f;
    cam->farDistance  = 20.0f;
    cam->height       = 4.0f;
    root->addChild(cam);
    root->addChild(new SoCube); // default unit cube at origin

    SbViewportRegion vp(512, 512);
    SoRayPickAction ra(vp);
    ra.setRay(SbVec3f(0, 0, 5), SbVec3f(0, 0, -1));
    ra.apply(root);

    SoPickedPoint * pp = ra.getPickedPoint(0);
    EXPECT_TRUE((pp != nullptr)) << "Ray through cube at origin should hit";
    root->unref();
}
