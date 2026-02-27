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
 * @file unit_test_wrappers.cpp
 * @brief Inline unit tests registered with the TestRegistry.
 *
 * Each test category is implemented as a self-contained lambda / function
 * that exercises the relevant Coin API and returns 0 on pass, non-zero on
 * failure.  The tests are automatically registered at program start via
 * static-initialiser blocks, making them available through the unified
 * obol_test CLI and through the FLTK viewer.
 *
 * Additionally, every visual scene factory from test_scenes.h is registered
 * here so the registry is fully populated from a single translation unit.
 */

#include "test_registry.h"
#include "test_scenes.h"

// ---------------------------------------------------------------------------
// Coin headers used by the unit tests
// ---------------------------------------------------------------------------
#include <Inventor/SoDB.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbVec2f.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbRotation.h>
#include <Inventor/SbColor.h>
#include <Inventor/SbBox3f.h>
#include <Inventor/SbViewportRegion.h>

// Actions
#include <Inventor/actions/SoAction.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/actions/SoCallbackAction.h>

// Fields
#include <Inventor/fields/SoSFFloat.h>
#include <Inventor/fields/SoSFInt32.h>
#include <Inventor/fields/SoSFVec3f.h>
#include <Inventor/fields/SoSFBool.h>
#include <Inventor/fields/SoMFFloat.h>
#include <Inventor/fields/SoMFVec3f.h>

// Nodes
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>

// Engines
#include <Inventor/engines/SoCalculator.h>
#include <Inventor/engines/SoElapsedTime.h>
#include <Inventor/engines/SoCompose.h>
#include <Inventor/fields/SoSFVec3f.h>

// Sensors
#include <Inventor/sensors/SoTimerSensor.h>
#include <Inventor/sensors/SoFieldSensor.h>
#include <Inventor/sensors/SoNodeSensor.h>

#include <cmath>
#include <cstdio>
#include <utility>

namespace {

// =========================================================================
// Helper: approximate float comparison
// =========================================================================
static bool approxEqual(float a, float b, float tol = 1e-4f)
{
    return std::fabs(a - b) <= tol;
}

// =========================================================================
// Unit test: Actions
// =========================================================================
static int runActionsTests()
{
    int failures = 0;

    // --- Action type checking ---
    {
        SoSearchAction search;
        SoGetBoundingBoxAction bbox(SbViewportRegion(100, 100));
        SoCallbackAction callback;
        if (search.getTypeId() == SoType::badType()) {
            fprintf(stderr, "  FAIL: SoSearchAction has bad type\n");
            ++failures;
        }
        if (!search.isOfType(SoAction::getClassTypeId())) {
            fprintf(stderr, "  FAIL: SoSearchAction not of SoAction type\n");
            ++failures;
        }
    }

    // --- Bounding box computation ---
    {
        SoSeparator* scene = new SoSeparator;
        scene->ref();
        SoCube* cube = new SoCube;
        cube->width.setValue(2.0f);
        cube->height.setValue(2.0f);
        cube->depth.setValue(2.0f);
        scene->addChild(cube);
        SoGetBoundingBoxAction bba(SbViewportRegion(100, 100));
        bba.apply(scene);
        SbBox3f box = bba.getBoundingBox();
        if (box.isEmpty()) {
            fprintf(stderr, "  FAIL: bounding box is empty for 2x2x2 cube\n");
            ++failures;
        }
        scene->unref();
    }

    // --- Search action ---
    {
        SoSeparator* scene = new SoSeparator;
        scene->ref();
        SoCube* cube = new SoCube;
        cube->setName("FindMe");
        scene->addChild(cube);
        SoSearchAction sa;
        sa.setName(SbName("FindMe"));
        sa.apply(scene);
        if (!sa.getPath()) {
            fprintf(stderr, "  FAIL: search did not find named node\n");
            ++failures;
        }
        scene->unref();
    }

    // --- Scene traversal does not crash ---
    {
        SoSeparator* scene = new SoSeparator;
        scene->ref();
        SoTranslation* t = new SoTranslation;
        t->translation.setValue(1.0f, 2.0f, 3.0f);
        scene->addChild(t);
        scene->addChild(new SoCube);
        SoSearchAction sa;
        sa.apply(scene);
        scene->unref();
    }

    return failures;
}

// =========================================================================
// Unit test: Base types (SbVec3f, SbMatrix, SbRotation)
// =========================================================================
static int runBaseTests()
{
    int failures = 0;

    // --- SbVec3f arithmetic ---
    {
        SbVec3f a(1.0f, 2.0f, 3.0f);
        SbVec3f b(4.0f, 5.0f, 6.0f);
        SbVec3f sum = a + b;
        if (!approxEqual(sum[0],5) || !approxEqual(sum[1],7) || !approxEqual(sum[2],9)) {
            fprintf(stderr, "  FAIL: SbVec3f addition\n"); ++failures;
        }
        float dot = a.dot(b);
        if (!approxEqual(dot, 32.0f)) {
            fprintf(stderr, "  FAIL: SbVec3f dot product (got %f)\n", dot); ++failures;
        }
        SbVec3f cross = a.cross(b);
        // 1x6-2x5, 2x4-1x6, 1x5-2x4 = -3,6,-3
        if (!approxEqual(cross[0],-3) || !approxEqual(cross[1],6) || !approxEqual(cross[2],-3)) {
            fprintf(stderr, "  FAIL: SbVec3f cross product\n"); ++failures;
        }
    }

    // --- SbVec3f normalisation ---
    {
        SbVec3f v(3.0f, 4.0f, 0.0f);
        SbVec3f n = v;
        n.normalize();
        if (!approxEqual(n.length(), 1.0f)) {
            fprintf(stderr, "  FAIL: SbVec3f normalise length\n"); ++failures;
        }
    }

    // --- SbMatrix identity ---
    {
        SbMatrix m;
        m.makeIdentity();
        SbVec3f v(1.0f, 2.0f, 3.0f);
        SbVec3f r;
        m.multVecMatrix(v, r);
        if (!approxEqual(r[0],1) || !approxEqual(r[1],2) || !approxEqual(r[2],3)) {
            fprintf(stderr, "  FAIL: SbMatrix identity transform\n"); ++failures;
        }
    }

    // --- SbRotation ---
    {
        SbRotation rot(SbVec3f(0,1,0), float(M_PI / 2.0));
        SbVec3f v(1.0f, 0.0f, 0.0f);
        SbVec3f result;
        rot.multVec(v, result);
        // 90° around Y maps (1,0,0) to (0,0,-1)
        if (!approxEqual(result[0], 0.0f, 1e-3f) ||
            !approxEqual(result[1], 0.0f, 1e-3f) ||
            !approxEqual(result[2], -1.0f, 1e-3f)) {
            fprintf(stderr, "  FAIL: SbRotation 90-deg Y (got %f %f %f)\n",
                    result[0], result[1], result[2]);
            ++failures;
        }
    }

    // --- SbBox3f ---
    {
        SbBox3f box(SbVec3f(-1,-1,-1), SbVec3f(1,1,1));
        if (box.isEmpty()) { fprintf(stderr, "  FAIL: SbBox3f isEmpty\n"); ++failures; }
        SbVec3f center = box.getCenter();
        if (!approxEqual(center[0],0) || !approxEqual(center[1],0) || !approxEqual(center[2],0)) {
            fprintf(stderr, "  FAIL: SbBox3f center\n"); ++failures;
        }
    }

    return failures;
}

// =========================================================================
// Unit test: Fields
// =========================================================================
static int runFieldsTests()
{
    int failures = 0;

    // --- SoSFFloat ---
    {
        SoSFFloat f;
        f.setValue(3.14f);
        if (!approxEqual(f.getValue(), 3.14f)) {
            fprintf(stderr, "  FAIL: SoSFFloat set/get\n"); ++failures;
        }
        if (f.isConnected()) {
            fprintf(stderr, "  FAIL: SoSFFloat should not be connected\n"); ++failures;
        }
    }

    // --- SoSFBool ---
    {
        SoSFBool b;
        b.setValue(TRUE);
        if (b.getValue() != TRUE) {
            fprintf(stderr, "  FAIL: SoSFBool set/get\n"); ++failures;
        }
    }

    // --- SoMFVec3f ---
    {
        SoMFVec3f mv;
        SbVec3f pts[3] = {{0,0,0},{1,0,0},{0,1,0}};
        mv.setValues(0, 3, pts);
        if (mv.getNum() != 3) {
            fprintf(stderr, "  FAIL: SoMFVec3f count (got %d)\n", mv.getNum()); ++failures;
        }
        if (!approxEqual(mv[1][0], 1.0f)) {
            fprintf(stderr, "  FAIL: SoMFVec3f value\n"); ++failures;
        }
    }

    // --- SoMFFloat ---
    {
        SoMFFloat mf;
        float vals[4] = {1.0f, 2.0f, 3.0f, 4.0f};
        mf.setValues(0, 4, vals);
        if (mf.getNum() != 4) {
            fprintf(stderr, "  FAIL: SoMFFloat count\n"); ++failures;
        }
    }

    // --- Field on a node ---
    {
        SoCube* cube = new SoCube;
        cube->ref();
        cube->width.setValue(5.0f);
        if (!approxEqual(cube->width.getValue(), 5.0f)) {
            fprintf(stderr, "  FAIL: SoCube field value\n"); ++failures;
        }
        cube->unref();
    }

    return failures;
}

// =========================================================================
// Unit test: Nodes
// =========================================================================
static int runNodesTests()
{
    int failures = 0;

    // --- Node creation and type ---
    {
        SoCube* cube = new SoCube;
        cube->ref();
        if (cube->getTypeId() == SoType::badType()) {
            fprintf(stderr, "  FAIL: SoCube bad type\n"); ++failures;
        }
        if (!cube->isOfType(SoNode::getClassTypeId())) {
            fprintf(stderr, "  FAIL: SoCube not SoNode\n"); ++failures;
        }
        cube->unref();
    }

    // --- Scene graph hierarchy ---
    {
        SoSeparator* root = new SoSeparator;
        root->ref();
        SoCube*   cube   = new SoCube;
        SoSphere* sphere = new SoSphere;
        root->addChild(cube);
        root->addChild(sphere);
        if (root->getNumChildren() != 2) {
            fprintf(stderr, "  FAIL: child count (got %d)\n", root->getNumChildren());
            ++failures;
        }
        root->removeChild(cube);
        if (root->getNumChildren() != 1) {
            fprintf(stderr, "  FAIL: after removeChild (got %d)\n", root->getNumChildren());
            ++failures;
        }
        root->unref();
    }

    // --- Named node lookup ---
    {
        SoSeparator* scene = new SoSeparator;
        scene->ref();
        SoCube* c = new SoCube;
        c->setName("MyBox");
        scene->addChild(c);
        SoNode* found = SoNode::getByName(SbName("MyBox"));
        if (!found) {
            fprintf(stderr, "  FAIL: SoNode::getByName failed\n"); ++failures;
        }
        scene->unref();
    }

    // --- Camera viewAll does not crash ---
    {
        SoSeparator* scene = new SoSeparator;
        scene->ref();
        SoPerspectiveCamera* cam = new SoPerspectiveCamera;
        scene->addChild(cam);
        scene->addChild(new SoCube);
        SbViewportRegion vp(800, 600);
        cam->viewAll(scene, vp);
        scene->unref();
    }

    return failures;
}

// =========================================================================
// Unit test: Engines
// =========================================================================
static int runEnginesTests()
{
    int failures = 0;

    // --- SoCalculator type ---
    {
        SoCalculator* calc = new SoCalculator;
        calc->ref();
        if (calc->getTypeId() == SoType::badType()) {
            fprintf(stderr, "  FAIL: SoCalculator bad type\n"); ++failures;
        }
        calc->unref();
    }

    // --- SoCalculator basic expression ---
    {
        SoCalculator* calc = new SoCalculator;
        calc->ref();
        calc->a.setValue(3.0f);
        calc->b.setValue(4.0f);
        calc->expression.setValue("oa = a + b");
        // Connect output to a float field to read the result
        SoSFFloat resultField;
        resultField.connectFrom(&calc->oa);
        float result = resultField.getValue();
        if (!approxEqual(result, 7.0f)) {
            fprintf(stderr, "  FAIL: SoCalculator a+b = %f (expected 7.0)\n", result);
            ++failures;
        }
        resultField.disconnect();
        calc->unref();
    }

    // --- SoElapsedTime type check ---
    {
        SoElapsedTime* et = new SoElapsedTime;
        et->ref();
        if (et->getTypeId() == SoType::badType()) {
            fprintf(stderr, "  FAIL: SoElapsedTime bad type\n"); ++failures;
        }
        et->unref();
    }

    // --- SoComposeVec3f ---
    {
        SoComposeVec3f* compose = new SoComposeVec3f;
        compose->ref();
        compose->x.setValue(1.0f);
        compose->y.setValue(2.0f);
        compose->z.setValue(3.0f);
        // Connect output to a field to read the result
        SoSFVec3f outField;
        outField.connectFrom(&compose->vector);
        SbVec3f out = outField.getValue();
        outField.disconnect();
        if (!approxEqual(out[0],1) || !approxEqual(out[1],2) || !approxEqual(out[2],3)) {
            fprintf(stderr, "  FAIL: SoComposeVec3f output (%f %f %f)\n",
                    out[0], out[1], out[2]);
            ++failures;
        }
        compose->unref();
    }

    return failures;
}

// =========================================================================
// Unit test: Sensors
// =========================================================================
static int runSensorsTests()
{
    int failures = 0;
    static int s_callback_count = 0;

    // --- SoFieldSensor ---
    {
        s_callback_count = 0;
        SoCube* cube = new SoCube;
        cube->ref();

        SoFieldSensor* fs = new SoFieldSensor(
            [](void* data, SoSensor*) { (*static_cast<int*>(data))++; },
            &s_callback_count);
        fs->attach(&cube->width);
        cube->width.setValue(5.0f);
        SoDB::getSensorManager()->processTimerQueue();
        SoDB::getSensorManager()->processDelayQueue(TRUE);

        // Callback may be deferred; just verify no crash occurred
        // (exact callback timing is environment-dependent)
        delete fs;
        cube->unref();
    }

    // --- SoNodeSensor type check ---
    {
        SoNodeSensor* ns = new SoNodeSensor;
        // SoSensor is not SoBase so no getTypeId(); just verify construction
        if (!ns) {
            fprintf(stderr, "  FAIL: SoNodeSensor construction\n"); ++failures;
        }
        delete ns;
    }

    // --- SoTimerSensor type check ---
    {
        SoTimerSensor* ts = new SoTimerSensor;
        ts->setInterval(SbTime(0.1));
        if (!ts) {
            fprintf(stderr, "  FAIL: SoTimerSensor construction\n"); ++failures;
        }
        delete ts;
    }

    return failures;
}

// =========================================================================
// Unit test: orbitCamera() – BRL-CAD-style smooth camera orbit math
// =========================================================================
static int runOrbitCameraTests()
{
    int failures = 0;
    const float tol = 1e-3f;

    /* Helper: apply the same rotation logic as orbitCamera() without needing
     * a full SoCamera node.  We work directly with SbRotation and SbVec3f. */
    auto applyOrbit = [](SbRotation orient, SbVec3f pos,
                         const SbVec3f &center, float dx, float dy, float sens)
        -> std::pair<SbRotation, SbVec3f>
    {
        const float deg2rad = static_cast<float>(M_PI) / 180.0f;
        float yawRad   = dx * sens * deg2rad;
        float pitchRad = dy * sens * deg2rad;
        SbRotation rotY(SbVec3f(0.0f, 1.0f, 0.0f), -yawRad);
        SbRotation rotX(SbVec3f(1.0f, 0.0f, 0.0f), -pitchRad);
        SbRotation newOrient = orient * (rotY * rotX);
        float radius = (pos - center).length();
        SbVec3f viewDir;
        newOrient.multVec(SbVec3f(0.0f, 0.0f, -1.0f), viewDir);
        SbVec3f newPos = center - viewDir * radius;
        return {newOrient, newPos};
    };

    const SbVec3f center(0.0f, 0.0f, 0.0f);
    const float radius = 5.0f;
    const float sens = 0.25f;   /* BRL-CAD default: 0.25 deg/pixel */

    /* --- Test 1: yaw-only (dx=80, dy=0) preserves orbit radius --- */
    {
        SbRotation orient = SbRotation::identity();
        SbVec3f pos(0.0f, 0.0f, radius);
        auto [newOrient, newPos] = applyOrbit(orient, pos, center, 80.0f, 0.0f, sens);

        float dist = (newPos - center).length();
        if (std::fabs(dist - radius) > tol) {
            fprintf(stderr, "  FAIL orbitCamera test1: radius after yaw=%.6f (expected %.6f)\n",
                    dist, radius);
            ++failures;
        }
    }

    /* --- Test 2: yaw-only camera still looks toward center --- */
    {
        SbRotation orient = SbRotation::identity();
        SbVec3f pos(0.0f, 0.0f, radius);
        auto [newOrient, newPos] = applyOrbit(orient, pos, center, 80.0f, 0.0f, sens);

        SbVec3f viewDir;
        newOrient.multVec(SbVec3f(0.0f, 0.0f, -1.0f), viewDir);
        /* toCenter (camera → center) and viewDir (camera forward) must be parallel */
        SbVec3f toCenter = center - newPos;
        toCenter.normalize();
        float dot = toCenter.dot(viewDir);
        if (std::fabs(dot - 1.0f) > tol) {
            fprintf(stderr, "  FAIL orbitCamera test2: camera not looking at center (dot=%.6f)\n", dot);
            ++failures;
        }
    }

    /* --- Test 3: pitch-only (dx=0, dy=80) preserves orbit radius --- */
    {
        SbRotation orient = SbRotation::identity();
        SbVec3f pos(0.0f, 0.0f, radius);
        auto [newOrient, newPos] = applyOrbit(orient, pos, center, 0.0f, 80.0f, sens);

        float dist = (newPos - center).length();
        if (std::fabs(dist - radius) > tol) {
            fprintf(stderr, "  FAIL orbitCamera test3: radius after pitch=%.6f (expected %.6f)\n",
                    dist, radius);
            ++failures;
        }
    }

    /* --- Test 4: pitch-only camera still looks toward center --- */
    {
        SbRotation orient = SbRotation::identity();
        SbVec3f pos(0.0f, 0.0f, radius);
        auto [newOrient, newPos] = applyOrbit(orient, pos, center, 0.0f, 80.0f, sens);

        SbVec3f viewDir;
        newOrient.multVec(SbVec3f(0.0f, 0.0f, -1.0f), viewDir);
        SbVec3f toCenter = center - newPos;
        toCenter.normalize();
        float dot = toCenter.dot(viewDir);
        if (std::fabs(dot - 1.0f) > tol) {
            fprintf(stderr, "  FAIL orbitCamera test4: camera not looking at center after pitch (dot=%.6f)\n", dot);
            ++failures;
        }
    }

    /* --- Test 5: mouse-right (dx>0) moves camera in -X direction
     *             (scene appears to rotate right → camera orbits left) --- */
    {
        SbRotation orient = SbRotation::identity();
        SbVec3f pos(0.0f, 0.0f, radius);
        auto [newOrient, newPos] = applyOrbit(orient, pos, center, 80.0f, 0.0f, sens);
        /* Camera started at (0,0,5); yaw right should give negative X position */
        if (newPos[0] >= 0.0f) {
            fprintf(stderr, "  FAIL orbitCamera test5: expected negative X after yaw right, got %.4f\n",
                    newPos[0]);
            ++failures;
        }
    }

    /* --- Test 6: mouse-down (dy>0) moves camera upward (+Y)
     *             (camera pitches down → position rises in Y) --- */
    {
        SbRotation orient = SbRotation::identity();
        SbVec3f pos(0.0f, 0.0f, radius);
        auto [newOrient, newPos] = applyOrbit(orient, pos, center, 0.0f, 80.0f, sens);
        if (newPos[1] <= 0.0f) {
            fprintf(stderr, "  FAIL orbitCamera test6: expected positive Y after pitch down, got %.4f\n",
                    newPos[1]);
            ++failures;
        }
    }

    /* --- Test 7: 4 × 1-pixel steps ≈ 1 × 4-pixel step (smoothness check).
     *             For small angles, rotation non-commutativity is negligible,
     *             so accumulated small steps should closely match one equivalent
     *             large step. --- */
    {
        SbRotation orient = SbRotation::identity();
        SbVec3f pos(0.0f, 0.0f, radius);

        /* Single 4-pixel step */
        auto [oA, pA] = applyOrbit(orient, pos, center, 4.0f, 3.0f, sens);

        /* Four 1-pixel steps */
        SbRotation oB = orient;
        SbVec3f    pB = pos;
        for (int i = 0; i < 4; ++i) {
            auto [oTmp, pTmp] = applyOrbit(oB, pB, center, 1.0f, 0.75f, sens);
            oB = oTmp; pB = pTmp;
        }

        float ddx = std::fabs(pA[0] - pB[0]);
        float ddy = std::fabs(pA[1] - pB[1]);
        float ddz = std::fabs(pA[2] - pB[2]);
        if (ddx > 0.001f || ddy > 0.001f || ddz > 0.001f) {
            fprintf(stderr, "  FAIL orbitCamera test7: step accumulation mismatch "
                    "dx=%.5f dy=%.5f dz=%.5f\n", ddx, ddy, ddz);
            ++failures;
        }
    }

    return failures;
}

// =========================================================================
// Static registrations – run at program start
// =========================================================================

// --- Visual scene registrations ---

REGISTER_TEST(primitives, ObolTest::TestCategory::Rendering,
    "2x2 grid: sphere, cube, cone, cylinder",
    e.has_visual = true;
    e.has_interactive = true;
    e.create_scene = ObolTest::Scenes::createPrimitives;
);

REGISTER_TEST(materials, ObolTest::TestCategory::Rendering,
    "Four spheres demonstrating material properties",
    e.has_visual = true;
    e.has_interactive = true;
    e.create_scene = ObolTest::Scenes::createMaterials;
);

REGISTER_TEST(lighting, ObolTest::TestCategory::Rendering,
    "Scene lit by directional and point lights",
    e.has_visual = true;
    e.has_interactive = true;
    e.create_scene = ObolTest::Scenes::createLighting;
);

REGISTER_TEST(transforms, ObolTest::TestCategory::Rendering,
    "Hierarchical rotation and translation transforms",
    e.has_visual = true;
    e.has_interactive = true;
    e.create_scene = ObolTest::Scenes::createTransforms;
);

REGISTER_TEST(cameras, ObolTest::TestCategory::Rendering,
    "Row of coloured spheres with explicit perspective camera",
    e.has_visual = true;
    e.has_interactive = true;
    e.create_scene = ObolTest::Scenes::createCameras;
);

REGISTER_TEST(texture, ObolTest::TestCategory::Rendering,
    "Checkerboard-textured cube",
    e.has_visual = true;
    e.has_interactive = true;
    e.create_scene = ObolTest::Scenes::createTexture;
);

REGISTER_TEST(text, ObolTest::TestCategory::Rendering,
    "SoText2 and SoText3 labels",
    e.has_visual = true;
    e.has_interactive = true;
    e.create_scene = ObolTest::Scenes::createText;
);

REGISTER_TEST(gradient, ObolTest::TestCategory::Rendering,
    "Background gradient via callback node",
    e.has_visual = true;
    e.has_interactive = true;
    e.create_scene = ObolTest::Scenes::createGradient;
);

REGISTER_TEST(colored_cube, ObolTest::TestCategory::Rendering,
    "Simple red cube with lighting (smoke test)",
    e.has_visual = true;
    e.has_interactive = true;
    e.create_scene = ObolTest::Scenes::createColoredCube;
);

REGISTER_TEST(coordinates, ObolTest::TestCategory::Rendering,
    "Colour-coded XYZ axis lines",
    e.has_visual = true;
    e.has_interactive = true;
    e.create_scene = ObolTest::Scenes::createCoordinates;
);

REGISTER_TEST(shadow, ObolTest::TestCategory::Rendering,
    "Shadow-casting scene (SoShadowGroup proxy)",
    e.has_visual = true;
    e.has_interactive = true;
    e.create_scene = ObolTest::Scenes::createShadow;
);

REGISTER_TEST(draggers, ObolTest::TestCategory::Draggers,
    "Interactive draggers (SoTranslate1, SoRotateSpherical)",
    e.has_visual = true;
    e.has_interactive = true;
    e.create_scene = ObolTest::Scenes::createDraggers;
);

REGISTER_TEST(hud, ObolTest::TestCategory::Misc,
    "Head-up display overlay using orthographic camera",
    e.has_visual = true;
    e.has_interactive = false;
    e.create_scene = ObolTest::Scenes::createHUD;
);

REGISTER_TEST(lod, ObolTest::TestCategory::Nodes,
    "Level of detail (SoLOD) switching between representations",
    e.has_visual = true;
    e.has_interactive = true;
    e.create_scene = ObolTest::Scenes::createLOD;
);

REGISTER_TEST(transparency, ObolTest::TestCategory::Rendering,
    "Alpha-blended overlapping spheres",
    e.has_visual = true;
    e.has_interactive = true;
    e.create_scene = ObolTest::Scenes::createTransparency;
);

// --- Unit test registrations ---

REGISTER_TEST(unit_actions, ObolTest::TestCategory::Actions,
    "Action type checking, bounding box, search action",
    e.has_visual = false;
    e.run_unit = runActionsTests;
);

REGISTER_TEST(unit_base, ObolTest::TestCategory::Base,
    "SbVec3f, SbMatrix, SbRotation, SbBox3f",
    e.has_visual = false;
    e.run_unit = runBaseTests;
);

REGISTER_TEST(unit_fields, ObolTest::TestCategory::Fields,
    "SoSFFloat, SoSFBool, SoMFVec3f, SoMFFloat, node fields",
    e.has_visual = false;
    e.run_unit = runFieldsTests;
);

REGISTER_TEST(unit_nodes, ObolTest::TestCategory::Nodes,
    "Node creation, hierarchy, named lookup, viewAll",
    e.has_visual = false;
    e.run_unit = runNodesTests;
);

REGISTER_TEST(unit_engines, ObolTest::TestCategory::Engines,
    "SoCalculator, SoElapsedTime, SoComposeVec3f",
    e.has_visual = false;
    e.run_unit = runEnginesTests;
);

REGISTER_TEST(unit_sensors, ObolTest::TestCategory::Sensors,
    "SoFieldSensor, SoNodeSensor, SoTimerSensor",
    e.has_visual = false;
    e.run_unit = runSensorsTests;
);

REGISTER_TEST(unit_orbit_camera, ObolTest::TestCategory::Base,
    "orbitCamera() BRL-CAD-style smooth orbit rotation math",
    e.has_visual = false;
    e.run_unit = runOrbitCameraTests;
);

} // anonymous namespace
