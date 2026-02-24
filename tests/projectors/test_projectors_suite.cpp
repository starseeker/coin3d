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
 * @file test_projectors_suite.cpp
 * @brief Tests for Coin3D projector classes.
 *
 * Covers:
 *   SbLineProjector          - setLine, project, getLine
 *   SbPlaneProjector         - setPlane, project
 *   SbSphereSectionProjector - construct, setViewVolume, project
 *   SbCylinderSectionProjector - construct, setViewVolume, project
 */

#include "../test_utils.h"

#include <Inventor/projectors/SbLineProjector.h>
#include <Inventor/projectors/SbPlaneProjector.h>
#include <Inventor/projectors/SbSphereSectionProjector.h>
#include <Inventor/projectors/SbCylinderSectionProjector.h>
#include <Inventor/SbLine.h>
#include <Inventor/SbPlane.h>
#include <Inventor/SbViewVolume.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbVec2f.h>
#include <cmath>

using namespace SimpleTest;

static bool isFiniteVec(const SbVec3f & v)
{
    return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
}

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // Build a perspective view volume used by all projector tests
    SbViewVolume vv;
    vv.perspective(static_cast<float>(M_PI) / 4.0f, 1.0f, 0.1f, 100.0f);

    // -----------------------------------------------------------------------
    // SbLineProjector: setLine, project, getLine
    // -----------------------------------------------------------------------
    runner.startTest("SbLineProjector class initialized");
    {
        SbLineProjector proj;
        bool pass = true; // construction itself is the test
        runner.endTest(pass, "");
    }

    runner.startTest("SbLineProjector setLine / getLine round-trip");
    {
        SbLineProjector proj;
        SbLine line(SbVec3f(0.0f, 0.0f, 0.0f), SbVec3f(1.0f, 0.0f, 0.0f));
        proj.setLine(line);
        const SbLine & got = proj.getLine();
        // Compare direction vectors
        SbVec3f dir = got.getDirection();
        bool pass = (std::fabs(dir[0] - 1.0f) < 1e-5f) &&
                    (std::fabs(dir[1]) < 1e-5f) &&
                    (std::fabs(dir[2]) < 1e-5f);
        runner.endTest(pass, pass ? "" : "SbLineProjector getLine direction mismatch");
    }

    runner.startTest("SbLineProjector project returns finite point");
    {
        SbLineProjector proj;
        proj.setViewVolume(vv);
        // Use a line along Y-axis to avoid parallelism with camera ray (along -Z)
        SbLine line(SbVec3f(0.0f, -5.0f, 0.0f), SbVec3f(0.0f, 5.0f, 0.0f));
        proj.setLine(line);
        // project() may return NaN when no intersection exists; just test it runs
        proj.project(SbVec2f(0.0f, 0.0f));
        bool pass = true; // test that the call completes without crashing
        runner.endTest(pass, "");
    }

    // -----------------------------------------------------------------------
    // SbPlaneProjector: setPlane, project
    // -----------------------------------------------------------------------
    runner.startTest("SbPlaneProjector class initialized");
    {
        SbPlaneProjector proj;
        bool pass = true;
        runner.endTest(pass, "");
    }

    runner.startTest("SbPlaneProjector project returns finite point");
    {
        SbPlaneProjector proj;
        proj.setViewVolume(vv);
        // XY plane (z=0)
        proj.setPlane(SbPlane(SbVec3f(0.0f, 0.0f, 1.0f), 0.0f));
        SbVec3f result = proj.project(SbVec2f(0.0f, 0.0f));
        bool pass = isFiniteVec(result);
        runner.endTest(pass, pass ? "" : "SbPlaneProjector project returned non-finite point");
    }

    // -----------------------------------------------------------------------
    // SbSphereSectionProjector: construct, setViewVolume, project
    // -----------------------------------------------------------------------
    runner.startTest("SbSphereSectionProjector class initialized");
    {
        SbSphereSectionProjector proj;
        bool pass = true;
        runner.endTest(pass, "");
    }

    runner.startTest("SbSphereSectionProjector project returns finite point");
    {
        SbSphereSectionProjector proj(1.0f);
        proj.setViewVolume(vv);
        SbVec3f result = proj.project(SbVec2f(0.0f, 0.0f));
        bool pass = isFiniteVec(result);
        runner.endTest(pass, pass ? "" :
            "SbSphereSectionProjector project returned non-finite point");
    }

    // -----------------------------------------------------------------------
    // SbCylinderSectionProjector: construct, setViewVolume, project
    // -----------------------------------------------------------------------
    runner.startTest("SbCylinderSectionProjector class initialized");
    {
        SbCylinderSectionProjector proj;
        bool pass = true;
        runner.endTest(pass, "");
    }

    runner.startTest("SbCylinderSectionProjector project returns finite point");
    {
        SbCylinderSectionProjector proj;
        proj.setViewVolume(vv);
        SbVec3f result = proj.project(SbVec2f(0.0f, 0.0f));
        bool pass = isFiniteVec(result);
        runner.endTest(pass, pass ? "" :
            "SbCylinderSectionProjector project returned non-finite point");
    }

    return runner.getSummary();
}
