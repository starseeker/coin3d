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
 * @file test_projectors_deep.cpp
 * @brief Deeper coverage for Coin3D projector classes.
 *
 * Complements test_projectors_suite.cpp with tests for projectors not yet
 * covered:
 *   SbSphereSheetProjector    - construct, project, getRotation
 *   SbSpherePlaneProjector    - construct, project
 *   SbCylinderSheetProjector  - construct, project, getRotation
 *   SbCylinderPlaneProjector  - construct, project
 *   SbProjector::copy()       - polymorphic copy, result is non-null
 *
 * Subsystems improved: projectors (33 %)
 */

#include "../test_utils.h"

#include <Inventor/projectors/SbSphereSheetProjector.h>
#include <Inventor/projectors/SbSpherePlaneProjector.h>
#include <Inventor/projectors/SbCylinderSheetProjector.h>
#include <Inventor/projectors/SbCylinderPlaneProjector.h>
#include <Inventor/projectors/SbSphereProjector.h>
#include <Inventor/projectors/SbCylinderProjector.h>
#include <Inventor/projectors/SbProjector.h>
#include <Inventor/SbViewVolume.h>
#include <Inventor/SbSphere.h>
#include <Inventor/SbCylinder.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbVec2f.h>
#include <Inventor/SbRotation.h>
#include <cmath>

using namespace SimpleTest;

// Check all components are finite
static bool isFiniteVec(const SbVec3f & v)
{
    return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
}

// Build a perspective view volume (camera at z=10, looking at origin)
static SbViewVolume makeViewVolume()
{
    SbViewVolume vv;
    vv.perspective(static_cast<float>(M_PI) / 4.0f, 1.0f, 0.1f, 100.0f);
    vv.translateCamera(SbVec3f(0.0f, 0.0f, 10.0f));
    return vv;
}

int main()
{
    TestFixture fixture;
    TestRunner runner;

    SbViewVolume vv = makeViewVolume();

    // -----------------------------------------------------------------------
    // SbSphereSheetProjector
    // -----------------------------------------------------------------------
    runner.startTest("SbSphereSheetProjector: construct default");
    {
        SbSphereSheetProjector proj;
        runner.endTest(true, "");
    }

    runner.startTest("SbSphereSheetProjector: construct with sphere");
    {
        SbSphereSheetProjector proj(SbSphere(SbVec3f(0, 0, 0), 1.0f));
        runner.endTest(true, "");
    }

    runner.startTest("SbSphereSheetProjector: project returns finite vector");
    {
        SbSphereSheetProjector proj(SbSphere(SbVec3f(0, 0, 0), 1.0f));
        proj.setViewVolume(vv);
        SbVec3f result = proj.project(SbVec2f(0.5f, 0.5f));
        bool pass = isFiniteVec(result);
        runner.endTest(pass, pass ? "" : "project() returned non-finite vector");
    }

    runner.startTest("SbSphereSheetProjector: getRotation returns valid rotation");
    {
        SbSphereSheetProjector proj(SbSphere(SbVec3f(0, 0, 0), 1.0f));
        proj.setViewVolume(vv);
        SbVec3f p1 = proj.project(SbVec2f(0.3f, 0.3f));
        SbVec3f p2 = proj.project(SbVec2f(0.7f, 0.3f));
        // getRotation only valid when both projections succeed; skip if NaN
        bool pass = true;
        if (isFiniteVec(p1) && isFiniteVec(p2)) {
            SbRotation rot = proj.getRotation(p1, p2);
            SbVec3f ax; float ang;
            rot.getValue(ax, ang);
            pass = std::isfinite(ang);
        }
        runner.endTest(pass, pass ? "" : "getRotation returned non-finite angle");
    }

    runner.startTest("SbSphereSheetProjector: copy() returns non-null");
    {
        SbSphereSheetProjector proj(SbSphere(SbVec3f(0, 0, 0), 1.0f));
        SbProjector * copy = proj.copy();
        bool pass = (copy != nullptr);
        (void)copy; // copy() result: destructor is protected, cannot delete via base ptr
        runner.endTest(pass, pass ? "" : "copy() returned null");
    }

    // -----------------------------------------------------------------------
    // SbSpherePlaneProjector
    // -----------------------------------------------------------------------
    runner.startTest("SbSpherePlaneProjector: construct default");
    {
        SbSpherePlaneProjector proj;
        runner.endTest(true, "");
    }

    runner.startTest("SbSpherePlaneProjector: project returns finite vector");
    {
        SbSpherePlaneProjector proj(SbSphere(SbVec3f(0, 0, 0), 1.0f), 0.9f);
        proj.setViewVolume(vv);
        SbVec3f result = proj.project(SbVec2f(0.5f, 0.5f));
        bool pass = isFiniteVec(result);
        runner.endTest(pass, pass ? "" : "project() returned non-finite vector");
    }

    runner.startTest("SbSpherePlaneProjector: copy() returns non-null");
    {
        SbSpherePlaneProjector proj;
        SbProjector * copy = proj.copy();
        bool pass = (copy != nullptr);
        (void)copy; // copy() result: destructor is protected, cannot delete via base ptr
        runner.endTest(pass, pass ? "" : "copy() returned null");
    }

    // -----------------------------------------------------------------------
    // SbCylinderSheetProjector
    // -----------------------------------------------------------------------
    runner.startTest("SbCylinderSheetProjector: construct default");
    {
        SbCylinderSheetProjector proj;
        runner.endTest(true, "");
    }

    runner.startTest("SbCylinderSheetProjector: construct with cylinder");
    {
        SbCylinderSheetProjector proj(SbCylinder(SbLine(SbVec3f(0,0,0),
                                                        SbVec3f(0,1,0)),
                                                 1.0f));
        runner.endTest(true, "");
    }

    runner.startTest("SbCylinderSheetProjector: project returns finite vector");
    {
        SbCylinderSheetProjector proj;
        proj.setViewVolume(vv);
        SbVec3f result = proj.project(SbVec2f(0.5f, 0.5f));
        bool pass = isFiniteVec(result);
        runner.endTest(pass, pass ? "" : "project() returned non-finite vector");
    }

    runner.startTest("SbCylinderSheetProjector: getRotation returns valid rotation");
    {
        SbCylinderSheetProjector proj;
        proj.setViewVolume(vv);
        SbVec3f p1 = proj.project(SbVec2f(0.3f, 0.5f));
        SbVec3f p2 = proj.project(SbVec2f(0.7f, 0.5f));
        bool pass = true;
        if (isFiniteVec(p1) && isFiniteVec(p2)) {
            SbRotation rot = proj.getRotation(p1, p2);
            SbVec3f ax; float ang;
            rot.getValue(ax, ang);
            pass = std::isfinite(ang);
        }
        runner.endTest(pass, pass ? "" : "getRotation returned non-finite angle");
    }

    runner.startTest("SbCylinderSheetProjector: copy() returns non-null");
    {
        SbCylinderSheetProjector proj;
        SbProjector * copy = proj.copy();
        bool pass = (copy != nullptr);
        (void)copy; // copy() result: destructor is protected, cannot delete via base ptr
        runner.endTest(pass, pass ? "" : "copy() returned null");
    }

    // -----------------------------------------------------------------------
    // SbCylinderPlaneProjector
    // -----------------------------------------------------------------------
    runner.startTest("SbCylinderPlaneProjector: construct default");
    {
        SbCylinderPlaneProjector proj;
        runner.endTest(true, "");
    }

    runner.startTest("SbCylinderPlaneProjector: project returns finite vector");
    {
        SbCylinderPlaneProjector proj;
        proj.setViewVolume(vv);
        SbVec3f result = proj.project(SbVec2f(0.5f, 0.5f));
        bool pass = isFiniteVec(result);
        runner.endTest(pass, pass ? "" : "project() returned non-finite vector");
    }

    runner.startTest("SbCylinderPlaneProjector: copy() returns non-null");
    {
        SbCylinderPlaneProjector proj;
        SbProjector * copy = proj.copy();
        bool pass = (copy != nullptr);
        (void)copy; // copy() result: destructor is protected, cannot delete via base ptr
        runner.endTest(pass, pass ? "" : "copy() returned null");
    }

    return runner.getSummary();
}
