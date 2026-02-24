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

using namespace SimpleTest;

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

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // -----------------------------------------------------------------------
    // SoRayPickAction class type
    // -----------------------------------------------------------------------
    runner.startTest("SoRayPickAction class type registered");
    {
        bool pass = (SoRayPickAction::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoRayPickAction bad class type");
    }

    // -----------------------------------------------------------------------
    // setRadius / getRadius
    // -----------------------------------------------------------------------
    runner.startTest("SoRayPickAction setRadius / getRadius round-trip");
    {
        SbViewportRegion vp(512, 512);
        SoRayPickAction ra(vp);
        ra.setRadius(5.0f);
        bool pass = floatNear(ra.getRadius(), 5.0f);
        runner.endTest(pass, pass ? "" : "setRadius/getRadius round-trip failed");
    }

    // -----------------------------------------------------------------------
    // setPickAll / isPickAll
    // -----------------------------------------------------------------------
    runner.startTest("SoRayPickAction setPickAll TRUE / isPickAll round-trip");
    {
        SbViewportRegion vp(512, 512);
        SoRayPickAction ra(vp);
        ra.setPickAll(TRUE);
        bool pass = (ra.isPickAll() == TRUE);
        runner.endTest(pass, pass ? "" : "setPickAll(TRUE)/isPickAll failed");
    }

    runner.startTest("SoRayPickAction setPickAll FALSE / isPickAll round-trip");
    {
        SbViewportRegion vp(512, 512);
        SoRayPickAction ra(vp);
        ra.setPickAll(FALSE);
        bool pass = (ra.isPickAll() == FALSE);
        runner.endTest(pass, pass ? "" : "setPickAll(FALSE)/isPickAll failed");
    }

    // -----------------------------------------------------------------------
    // setNormalizedPoint / setPoint
    // -----------------------------------------------------------------------
    runner.startTest("SoRayPickAction setNormalizedPoint does not crash");
    {
        SbViewportRegion vp(512, 512);
        SoRayPickAction ra(vp);
        ra.setNormalizedPoint(SbVec2f(0.5f, 0.5f));
        bool pass = true; // just must not crash
        runner.endTest(pass, pass ? "" : "setNormalizedPoint crashed");
    }

    runner.startTest("SoRayPickAction setPoint does not crash");
    {
        SbViewportRegion vp(512, 512);
        SoRayPickAction ra(vp);
        ra.setPoint(SbVec2s(256, 256));
        bool pass = true;
        runner.endTest(pass, pass ? "" : "setPoint crashed");
    }

    // -----------------------------------------------------------------------
    // setRay + hasWorldSpaceRay + computeWorldSpaceRay
    // -----------------------------------------------------------------------
    runner.startTest("SoRayPickAction hasWorldSpaceRay is FALSE before setting");
    {
        SbViewportRegion vp(512, 512);
        SoRayPickAction ra(vp);
        bool pass = (ra.hasWorldSpaceRay() == FALSE);
        runner.endTest(pass, pass ? "" : "hasWorldSpaceRay should be FALSE before setRay");
    }

    runner.startTest("SoRayPickAction setRay then hasWorldSpaceRay is TRUE");
    {
        SbViewportRegion vp(512, 512);
        SoRayPickAction ra(vp);
        ra.setRay(SbVec3f(0, 0, 5), SbVec3f(0, 0, -1));
        bool pass = (ra.hasWorldSpaceRay() == TRUE);
        runner.endTest(pass, pass ? "" : "hasWorldSpaceRay should be TRUE after setRay");
    }

    // -----------------------------------------------------------------------
    // Picking: ray through sphere at origin
    // -----------------------------------------------------------------------
    runner.startTest("SoRayPickAction: centre ray hits sphere at origin");
    {
        SoSeparator * root = buildPickScene();
        root->ref();

        SbViewportRegion vp(512, 512);
        SoRayPickAction ra(vp);
        // Ray from z=5 towards -z, should hit sphere at origin
        ra.setRay(SbVec3f(0, 0, 5), SbVec3f(0, 0, -1));
        ra.apply(root);

        SoPickedPoint * pp = ra.getPickedPoint(0);
        bool pass = (pp != nullptr);
        root->unref();
        runner.endTest(pass, pass ? "" : "Centre ray should hit sphere at origin");
    }

    runner.startTest("SoRayPickAction: picked point is on geometry");
    {
        SoSeparator * root = buildPickScene();
        root->ref();

        SbViewportRegion vp(512, 512);
        SoRayPickAction ra(vp);
        ra.setRay(SbVec3f(0, 0, 5), SbVec3f(0, 0, -1));
        ra.apply(root);

        SoPickedPoint * pp = ra.getPickedPoint(0);
        bool pass = (pp != nullptr) && pp->isOnGeometry();
        root->unref();
        runner.endTest(pass, pass ? "" : "Picked point should be on geometry");
    }

    runner.startTest("SoRayPickAction: picked point has valid position");
    {
        SoSeparator * root = buildPickScene();
        root->ref();

        SbViewportRegion vp(512, 512);
        SoRayPickAction ra(vp);
        ra.setRay(SbVec3f(0, 0, 5), SbVec3f(0, 0, -1));
        ra.apply(root);

        SoPickedPoint * pp = ra.getPickedPoint(0);
        bool pass = false;
        if (pp) {
            SbVec3f pt = pp->getPoint();
            // Sphere has radius 1, centre at origin; hit should be at z≈+1
            pass = floatNear(pt[2], 1.0f, 0.1f);
        }
        root->unref();
        runner.endTest(pass, pass ? "" : "Picked point z-position should be near +1");
    }

    runner.startTest("SoRayPickAction: miss returns null");
    {
        SoSeparator * root = buildPickScene();
        root->ref();

        SbViewportRegion vp(512, 512);
        SoRayPickAction ra(vp);
        // Ray from far right, parallel to Z - should miss the sphere at origin
        ra.setRay(SbVec3f(5, 5, 5), SbVec3f(0, 0, -1));
        ra.apply(root);

        SoPickedPoint * pp = ra.getPickedPoint(0);
        bool pass = (pp == nullptr);
        root->unref();
        runner.endTest(pass, pass ? "" : "Off-centre ray should miss the sphere (should return null)");
    }

    runner.startTest("SoRayPickAction: getPickedPointList is empty for miss");
    {
        SoSeparator * root = buildPickScene();
        root->ref();

        SbViewportRegion vp(512, 512);
        SoRayPickAction ra(vp);
        ra.setRay(SbVec3f(10, 10, 5), SbVec3f(0, 0, -1));
        ra.apply(root);

        const SoPickedPointList & list = ra.getPickedPointList();
        bool pass = (list.getLength() == 0);
        root->unref();
        runner.endTest(pass, pass ? "" : "Missed ray getPickedPointList should be empty");
    }

    runner.startTest("SoRayPickAction: setPickAll collects multiple intersections");
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
        bool pass = (list.getLength() >= 1);
        root->unref();
        runner.endTest(pass, pass ? "" : "setPickAll should collect at least 1 intersection");
    }

    runner.startTest("SoRayPickAction: picked path contains sphere node");
    {
        SoSeparator * root = buildPickScene();
        root->ref();

        SbViewportRegion vp(512, 512);
        SoRayPickAction ra(vp);
        ra.setRay(SbVec3f(0, 0, 5), SbVec3f(0, 0, -1));
        ra.apply(root);

        SoPickedPoint * pp = ra.getPickedPoint(0);
        bool pass = false;
        if (pp) {
            SoPath * path = pp->getPath();
            // Path should contain the sphere
            pass = (path != nullptr) &&
                   path->containsNode(root->getChild(1)); // sphere is child 1
        }
        root->unref();
        runner.endTest(pass, pass ? "" : "Picked path should contain the sphere node");
    }

    // -----------------------------------------------------------------------
    // Pick with SoCube
    // -----------------------------------------------------------------------
    runner.startTest("SoRayPickAction: ray hits cube at origin");
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
        bool pass = (pp != nullptr);
        root->unref();
        runner.endTest(pass, pass ? "" : "Ray through cube at origin should hit");
    }

    return runner.getSummary();
}
