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
 * @file test_base_extras.cpp
 * @brief Tests for additional base types.
 *
 * Covers:
 *   SbClip        - addVertex, clip against plane, getNumVertices
 *   SbXfBox3f     - construct, extendBy, intersect, transform
 *   SbXfBox3d     - double-precision variant
 *   SbTesselator  - tesselate convex quad → 2 triangles
 */

#include "../test_utils.h"

#include <Inventor/SbClip.h>
#include <Inventor/SbXfBox3f.h>
#include <Inventor/SbXfBox3d.h>
#include <Inventor/SbTesselator.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbPlane.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbVec2f.h>
#include <cmath>

using namespace SimpleTest;

// ---------------------------------------------------------------------------
// SbTesselator callback: count triangles
// ---------------------------------------------------------------------------
static int g_triCount = 0;

static void triCb(void * /*v0*/, void * /*v1*/, void * /*v2*/, void * /*data*/)
{
    ++g_triCount;
}

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // -----------------------------------------------------------------------
    // SbClip: add 4 vertices of a unit quad, clip against z>=0 plane —
    // all vertices survive because they all have z=0
    // -----------------------------------------------------------------------
    runner.startTest("SbClip: quad vertices survive clip against z=0 plane");
    {
        SbClip clip;
        clip.addVertex(SbVec3f(-1.0f, -1.0f, 0.0f));
        clip.addVertex(SbVec3f( 1.0f, -1.0f, 0.0f));
        clip.addVertex(SbVec3f( 1.0f,  1.0f, 0.0f));
        clip.addVertex(SbVec3f(-1.0f,  1.0f, 0.0f));

        // Clip against z >= 0 (normal +z, offset 0): keeps everything at z=0
        clip.clip(SbPlane(SbVec3f(0.0f, 0.0f, 1.0f), 0.0f));

        int n = clip.getNumVertices();
        bool pass = (n == 4);
        runner.endTest(pass, pass ? "" :
            "SbClip: all 4 vertices at z=0 should survive z>=0 clip");
    }

    runner.startTest("SbClip: clip against z>=1 plane removes all z=0 vertices");
    {
        SbClip clip;
        clip.addVertex(SbVec3f(-1.0f, -1.0f, 0.0f));
        clip.addVertex(SbVec3f( 1.0f, -1.0f, 0.0f));
        clip.addVertex(SbVec3f( 1.0f,  1.0f, 0.0f));
        clip.addVertex(SbVec3f(-1.0f,  1.0f, 0.0f));

        // Clip against z >= 1: all points at z=0 are behind the plane
        clip.clip(SbPlane(SbVec3f(0.0f, 0.0f, 1.0f), 1.0f));

        int n = clip.getNumVertices();
        bool pass = (n == 0);
        runner.endTest(pass, pass ? "" :
            "SbClip: vertices at z=0 should not survive clip against z>=1 plane");
    }

    runner.startTest("SbClip: partial clip produces fewer vertices");
    {
        SbClip clip;
        // Two vertices below z=0.5, two above
        clip.addVertex(SbVec3f(-1.0f, 0.0f, 0.0f));  // z=0, behind
        clip.addVertex(SbVec3f( 1.0f, 0.0f, 0.0f));  // z=0, behind
        clip.addVertex(SbVec3f( 1.0f, 0.0f, 1.0f));  // z=1, in front
        clip.addVertex(SbVec3f(-1.0f, 0.0f, 1.0f));  // z=1, in front

        // Clip z >= 0.5: two vertices are clipped, two new edge intersections
        clip.clip(SbPlane(SbVec3f(0.0f, 0.0f, 1.0f), 0.5f));

        int n = clip.getNumVertices();
        // Expect 4 vertices: 2 originals above + 2 intersection points
        bool pass = (n > 0) && (n <= 4);
        runner.endTest(pass, pass ? "" :
            "SbClip: partial clip should produce 1-4 vertices");
    }

    // -----------------------------------------------------------------------
    // SbXfBox3f: construct from SbBox3f, extendBy, intersect, transform
    // -----------------------------------------------------------------------
    runner.startTest("SbXfBox3f: construct from SbBox3f is not empty");
    {
        SbBox3f b(SbVec3f(-1.0f, -1.0f, -1.0f), SbVec3f(1.0f, 1.0f, 1.0f));
        SbXfBox3f xb(b);
        bool pass = !xb.isEmpty();
        runner.endTest(pass, pass ? "" :
            "SbXfBox3f constructed from SbBox3f should not be empty");
    }

    runner.startTest("SbXfBox3f: extendBy point outside box grows box");
    {
        SbBox3f b(SbVec3f(-1.0f, -1.0f, -1.0f), SbVec3f(1.0f, 1.0f, 1.0f));
        SbXfBox3f xb(b);
        xb.extendBy(SbVec3f(2.0f, 0.0f, 0.0f));
        // After extension, the projected box should contain (2,0,0)
        SbBox3f proj = xb.project();
        bool pass = proj.intersect(SbVec3f(2.0f, 0.0f, 0.0f));
        runner.endTest(pass, pass ? "" :
            "SbXfBox3f: extendBy point (2,0,0) should grow box to include it");
    }

    runner.startTest("SbXfBox3f: intersect with overlapping box returns TRUE");
    {
        SbBox3f b1(SbVec3f(-1.0f, -1.0f, -1.0f), SbVec3f(1.0f, 1.0f, 1.0f));
        SbBox3f b2(SbVec3f( 0.0f,  0.0f,  0.0f), SbVec3f(2.0f, 2.0f, 2.0f));
        SbXfBox3f xb1(b1);
        SbXfBox3f xb2(b2);
        bool pass = xb1.intersect(xb2);
        runner.endTest(pass, pass ? "" :
            "SbXfBox3f: overlapping boxes should intersect");
    }

    runner.startTest("SbXfBox3f: transform with translation shifts projected box");
    {
        SbBox3f b(SbVec3f(-1.0f, -1.0f, -1.0f), SbVec3f(1.0f, 1.0f, 1.0f));
        SbXfBox3f xb(b);

        // Apply translation of (5, 0, 0)
        SbMatrix tx;
        tx.setTranslate(SbVec3f(5.0f, 0.0f, 0.0f));
        xb.transform(tx);

        SbBox3f proj = xb.project();
        SbVec3f center = proj.getCenter();
        bool pass = (std::fabs(center[0] - 5.0f) < 0.1f);
        runner.endTest(pass, pass ? "" :
            "SbXfBox3f: transform translate should shift projected box center");
    }

    // -----------------------------------------------------------------------
    // SbXfBox3d: double-precision variant
    // -----------------------------------------------------------------------
    runner.startTest("SbXfBox3d: construct from SbBox3d is not empty");
    {
        SbBox3d b(SbVec3d(-1.0, -1.0, -1.0), SbVec3d(1.0, 1.0, 1.0));
        SbXfBox3d xb(b);
        bool pass = !xb.isEmpty();
        runner.endTest(pass, pass ? "" :
            "SbXfBox3d constructed from SbBox3d should not be empty");
    }

    runner.startTest("SbXfBox3d: intersect with overlapping box returns TRUE");
    {
        SbBox3d b1(SbVec3d(-1.0, -1.0, -1.0), SbVec3d(1.0, 1.0, 1.0));
        SbBox3d b2(SbVec3d( 0.0,  0.0,  0.0), SbVec3d(2.0, 2.0, 2.0));
        SbXfBox3d xb1(b1);
        SbXfBox3d xb2(b2);
        bool pass = xb1.intersect(xb2);
        runner.endTest(pass, pass ? "" :
            "SbXfBox3d: overlapping boxes should intersect");
    }

    // -----------------------------------------------------------------------
    // SbTesselator: tesselate a convex quad → 2 triangles
    // -----------------------------------------------------------------------
    runner.startTest("SbTesselator: convex quad produces exactly 2 triangles");
    {
        g_triCount = 0;
        SbTesselator tess(triCb, nullptr);

        tess.beginPolygon();
        tess.addVertex(SbVec3f(-1.0f, -1.0f, 0.0f), nullptr);
        tess.addVertex(SbVec3f( 1.0f, -1.0f, 0.0f), nullptr);
        tess.addVertex(SbVec3f( 1.0f,  1.0f, 0.0f), nullptr);
        tess.addVertex(SbVec3f(-1.0f,  1.0f, 0.0f), nullptr);
        tess.endPolygon();

        bool pass = (g_triCount == 2);
        runner.endTest(pass, pass ? "" :
            "SbTesselator: convex quad should produce exactly 2 triangles");
    }

    return runner.getSummary();
}
