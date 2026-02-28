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
#include <Inventor/actions/SoRaytraceRenderAction.h>
#include <Inventor/SoPrimitiveVertex.h>

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
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoLight.h>
#include <Inventor/nodes/SoShape.h>

// Engines
#include <Inventor/engines/SoCalculator.h>
#include <Inventor/engines/SoElapsedTime.h>
#include <Inventor/engines/SoCompose.h>
#include <Inventor/fields/SoSFVec3f.h>

// Sensors
#include <Inventor/sensors/SoTimerSensor.h>
#include <Inventor/sensors/SoFieldSensor.h>
#include <Inventor/sensors/SoNodeSensor.h>

#include <Inventor/SbTime.h>
#include <Inventor/actions/SoGetPrimitiveCountAction.h>

#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

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
// Unit test: SoRaytraceRenderAction
// =========================================================================
static int runRaytraceActionTests()
{
    int failures = 0;

    // --- Type registration ---
    {
        SoRaytraceRenderAction rta(SbViewportRegion(100, 100));
        if (rta.getTypeId() == SoType::badType()) {
            fprintf(stderr, "  FAIL: SoRaytraceRenderAction has bad type\n");
            ++failures;
        }
        if (!rta.isOfType(SoCallbackAction::getClassTypeId())) {
            fprintf(stderr, "  FAIL: SoRaytraceRenderAction not derived from SoCallbackAction\n");
            ++failures;
        }
        if (!rta.isOfType(SoAction::getClassTypeId())) {
            fprintf(stderr, "  FAIL: SoRaytraceRenderAction not derived from SoAction\n");
            ++failures;
        }
    }

    // --- Viewport region round-trip ---
    {
        SbViewportRegion vp(320, 240);
        SoRaytraceRenderAction rta(vp);
        SbVec2s size = rta.getViewportRegion().getWindowSize();
        if (size[0] != 320 || size[1] != 240) {
            fprintf(stderr, "  FAIL: SoRaytraceRenderAction viewport size mismatch (%d,%d)\n",
                    (int)size[0], (int)size[1]);
            ++failures;
        }
    }

    // --- Triangle collection via generatePrimitives ---
    {
        // Apply to a cube; count the triangles generated
        SoSeparator* root = new SoSeparator;
        root->ref();
        root->addChild(new SoCube);

        int triangleCount = 0;
        SoRaytraceRenderAction rta(SbViewportRegion(100, 100));
        rta.addTriangleCallback(
            SoShape::getClassTypeId(),
            [](void* ud, SoCallbackAction*, const SoPrimitiveVertex*,
               const SoPrimitiveVertex*, const SoPrimitiveVertex*) {
                (*static_cast<int*>(ud))++;
            },
            &triangleCount);
        rta.apply(root);
        root->unref();

        // A cube has 6 faces × 2 triangles each = 12 triangles
        if (triangleCount != 12) {
            fprintf(stderr, "  FAIL: expected 12 triangles for cube, got %d\n", triangleCount);
            ++failures;
        }
    }

    // --- Lights collected after traversal ---
    {
        SoSeparator* root = new SoSeparator;
        root->ref();
        root->addChild(new SoDirectionalLight);
        root->addChild(new SoCube);

        SoRaytraceRenderAction rta(SbViewportRegion(100, 100));
        rta.addTriangleCallback(SoShape::getClassTypeId(),
            [](void*, SoCallbackAction*, const SoPrimitiveVertex*,
               const SoPrimitiveVertex*, const SoPrimitiveVertex*) {},
            nullptr);
        rta.apply(root);
        root->unref();

        const SoNodeList& lights = rta.getLights();
        if (lights.getLength() == 0) {
            fprintf(stderr, "  FAIL: SoRaytraceRenderAction getLights() returned empty list\n");
            ++failures;
        } else if (!lights[0]->isOfType(SoDirectionalLight::getClassTypeId())) {
            fprintf(stderr, "  FAIL: light is not SoDirectionalLight\n");
            ++failures;
        }
    }

    // --- Model matrix applied to vertices ---
    {
        // Translate a cube by (5,0,0); collected vertices should be near x=5
        SoSeparator* root = new SoSeparator;
        root->ref();

        SoTranslation* t = new SoTranslation;
        t->translation.setValue(5.0f, 0.0f, 0.0f);
        root->addChild(t);
        root->addChild(new SoCube);

        struct Collector {
            SbVec3f first;
            bool set = false;
        } col;

        SoRaytraceRenderAction rta(SbViewportRegion(100, 100));
        rta.addTriangleCallback(
            SoShape::getClassTypeId(),
            [](void* ud, SoCallbackAction* action,
               const SoPrimitiveVertex* v1,
               const SoPrimitiveVertex*, const SoPrimitiveVertex*) {
                Collector* c = static_cast<Collector*>(ud);
                if (!c->set) {
                    SbVec3f p;
                    action->getModelMatrix().multVecMatrix(v1->getPoint(), p);
                    c->first = p;
                    c->set = true;
                }
            },
            &col);
        rta.apply(root);
        root->unref();

        if (!col.set) {
            fprintf(stderr, "  FAIL: no triangle callback was invoked\n");
            ++failures;
        } else if (std::fabs(col.first[0] - 5.0f) > 1.5f) {
            fprintf(stderr, "  FAIL: expected vertex near x=5, got x=%f\n", col.first[0]);
            ++failures;
        }
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
// Unit test: SoCamera::orbitCamera() – BRL-CAD-style smooth camera orbit math
// =========================================================================
static int runOrbitCameraTests()
{
    int failures = 0;
    const float tol = 1e-3f;

    const SbVec3f center(0.0f, 0.0f, 0.0f);
    const float radius = 5.0f;
    const float sens = 0.25f;   /* BRL-CAD default: 0.25 deg/pixel */

    /* --- Test 1: yaw-only (dx=80, dy=0) preserves orbit radius --- */
    {
        SoPerspectiveCamera *cam = new SoPerspectiveCamera;
        cam->ref();
        cam->position.setValue(0.0f, 0.0f, radius);
        cam->orientation.setValue(SbRotation::identity());
        cam->orbitCamera(center, 80.0f, 0.0f, sens);

        float dist = (cam->position.getValue() - center).length();
        if (std::fabs(dist - radius) > tol) {
            fprintf(stderr, "  FAIL orbitCamera test1: radius after yaw=%.6f (expected %.6f)\n",
                    dist, radius);
            ++failures;
        }
        cam->unref();
    }

    /* --- Test 2: yaw-only camera still looks toward center --- */
    {
        SoPerspectiveCamera *cam = new SoPerspectiveCamera;
        cam->ref();
        cam->position.setValue(0.0f, 0.0f, radius);
        cam->orientation.setValue(SbRotation::identity());
        cam->orbitCamera(center, 80.0f, 0.0f, sens);

        SbVec3f viewDir;
        cam->orientation.getValue().multVec(SbVec3f(0.0f, 0.0f, -1.0f), viewDir);
        SbVec3f toCenter = center - cam->position.getValue();
        toCenter.normalize();
        float dot = toCenter.dot(viewDir);
        if (std::fabs(dot - 1.0f) > tol) {
            fprintf(stderr, "  FAIL orbitCamera test2: camera not looking at center (dot=%.6f)\n", dot);
            ++failures;
        }
        cam->unref();
    }

    /* --- Test 3: pitch-only (dx=0, dy=80) preserves orbit radius --- */
    {
        SoPerspectiveCamera *cam = new SoPerspectiveCamera;
        cam->ref();
        cam->position.setValue(0.0f, 0.0f, radius);
        cam->orientation.setValue(SbRotation::identity());
        cam->orbitCamera(center, 0.0f, 80.0f, sens);

        float dist = (cam->position.getValue() - center).length();
        if (std::fabs(dist - radius) > tol) {
            fprintf(stderr, "  FAIL orbitCamera test3: radius after pitch=%.6f (expected %.6f)\n",
                    dist, radius);
            ++failures;
        }
        cam->unref();
    }

    /* --- Test 4: pitch-only camera still looks toward center --- */
    {
        SoPerspectiveCamera *cam = new SoPerspectiveCamera;
        cam->ref();
        cam->position.setValue(0.0f, 0.0f, radius);
        cam->orientation.setValue(SbRotation::identity());
        cam->orbitCamera(center, 0.0f, 80.0f, sens);

        SbVec3f viewDir;
        cam->orientation.getValue().multVec(SbVec3f(0.0f, 0.0f, -1.0f), viewDir);
        SbVec3f toCenter = center - cam->position.getValue();
        toCenter.normalize();
        float dot = toCenter.dot(viewDir);
        if (std::fabs(dot - 1.0f) > tol) {
            fprintf(stderr, "  FAIL orbitCamera test4: camera not looking at center after pitch (dot=%.6f)\n", dot);
            ++failures;
        }
        cam->unref();
    }

    /* --- Test 5: mouse-right (dx>0) moves camera in -X direction
     *             (scene appears to rotate right → camera orbits left) --- */
    {
        SoPerspectiveCamera *cam = new SoPerspectiveCamera;
        cam->ref();
        cam->position.setValue(0.0f, 0.0f, radius);
        cam->orientation.setValue(SbRotation::identity());
        cam->orbitCamera(center, 80.0f, 0.0f, sens);

        if (cam->position.getValue()[0] >= 0.0f) {
            fprintf(stderr, "  FAIL orbitCamera test5: expected negative X after yaw right, got %.4f\n",
                    cam->position.getValue()[0]);
            ++failures;
        }
        cam->unref();
    }

    /* --- Test 6: mouse-down (dy>0) moves camera upward (+Y)
     *             (camera pitches down → position rises in Y) --- */
    {
        SoPerspectiveCamera *cam = new SoPerspectiveCamera;
        cam->ref();
        cam->position.setValue(0.0f, 0.0f, radius);
        cam->orientation.setValue(SbRotation::identity());
        cam->orbitCamera(center, 0.0f, 80.0f, sens);

        if (cam->position.getValue()[1] <= 0.0f) {
            fprintf(stderr, "  FAIL orbitCamera test6: expected positive Y after pitch down, got %.4f\n",
                    cam->position.getValue()[1]);
            ++failures;
        }
        cam->unref();
    }

    /* --- Test 7: 4 × 1-pixel steps ≈ 1 × 4-pixel step (smoothness check).
     *             For small angles, rotation non-commutativity is negligible,
     *             so accumulated small steps should closely match one equivalent
     *             large step. --- */
    {
        /* Single 4-pixel step */
        SoPerspectiveCamera *camA = new SoPerspectiveCamera;
        camA->ref();
        camA->position.setValue(0.0f, 0.0f, radius);
        camA->orientation.setValue(SbRotation::identity());
        camA->orbitCamera(center, 4.0f, 3.0f, sens);
        SbVec3f pA = camA->position.getValue();
        camA->unref();

        /* Four 1-pixel steps */
        SoPerspectiveCamera *camB = new SoPerspectiveCamera;
        camB->ref();
        camB->position.setValue(0.0f, 0.0f, radius);
        camB->orientation.setValue(SbRotation::identity());
        for (int i = 0; i < 4; ++i) {
            camB->orbitCamera(center, 1.0f, 0.75f, sens);
        }
        SbVec3f pB = camB->position.getValue();
        camB->unref();

        float ddx = std::fabs(pA[0] - pB[0]);
        float ddy = std::fabs(pA[1] - pB[1]);
        float ddz = std::fabs(pA[2] - pB[2]);
        if (ddx > 0.001f || ddy > 0.001f || ddz > 0.001f) {
            fprintf(stderr, "  FAIL orbitCamera test7: step accumulation mismatch "
                    "dx=%.5f dy=%.5f dz=%.5f\n", ddx, ddy, ddz);
            ++failures;
        }
    }

    /* --- Test 8: camera-local yaw leaves the camera's world-up direction
     *             unchanged.  orbitCamera() yaws around the camera's own Y
     *             axis (camera-local space), so the camera's world-up vector
     *             q*(0,1,0) is invariant under pure yaw – rotating a vector
     *             around itself cannot change it.  After accumulating 45° of
     *             pitch the world-up is no longer (0,1,0); 720 subsequent
     *             yaw-only steps (720 pixels × 0.25 deg/pixel = 180° total)
     *             must leave it the same. --- */
    {
        SoPerspectiveCamera *cam = new SoPerspectiveCamera;
        cam->ref();
        cam->position.setValue(0.0f, 0.0f, radius);
        cam->orientation.setValue(SbRotation::identity());

        /* Pitch 45° first (180 pixels × 0.25 deg/pixel) */
        cam->orbitCamera(center, 0.0f, 180.0f, sens);

        SbVec3f upAfterPitch;
        cam->orientation.getValue().multVec(SbVec3f(0.0f, 1.0f, 0.0f), upAfterPitch);

        /* 720 yaw-only steps (720 pixels × 0.25 deg/pixel = 180° total) */
        for (int i = 0; i < 720; ++i)
            cam->orbitCamera(center, 1.0f, 0.0f, sens);

        SbVec3f upAfterYaw;
        cam->orientation.getValue().multVec(SbVec3f(0.0f, 1.0f, 0.0f), upAfterYaw);

        float drift = (upAfterYaw - upAfterPitch).length();
        if (drift > tol) {
            fprintf(stderr, "  FAIL orbitCamera test8: camera-local yaw changed world-up "
                    "by %.6f (expected < %.6f)\n", drift, tol);
            ++failures;
        }
        cam->unref();
    }

    return failures;
}

// =========================================================================
// Scalability benchmark: scene graph construction and traversal
//
// Characterises the cost of building and traversing large Obol scene graphs
// that are representative of a BRL-CAD CSG tree (tens of thousands of nodes).
//
// Three structures are tested at several node counts:
//
//  flat          – root SoSeparator with N * (SoTranslation + SoCube) groups.
//                  Simulates a wide, single-level assembly list.
//
//  binary_tree   – balanced binary tree of SoSeparators with SoCubes at the
//                  leaves.  Simulates a balanced CSG combination tree.
//
//  linear_chain  – depth-N linear chain of SoSeparators with a single SoCube
//                  at the tip.  Simulates a maximally deep unary CSG tree.
//                  Capped at CHAIN_MAX nodes to avoid system-stack exhaustion.
//
//  chain_bottomup  – same deep chain but built leaf-first (bottom-up),
//                    avoiding the O(n^2) top-down notification cost.
//
//  chain_deferred  – same deep chain built top-down but with
//                    enableNotify(FALSE) on each node during construction,
//                    also achieving O(n) build time.
//
// For each (structure, size) pair the following operations are timed:
//   build    – allocate all nodes and wire up the hierarchy
//   bbox     – SoGetBoundingBoxAction traversal (CPU; no GL)
//   search   – SoSearchAction for ALL SoCube nodes
//   destroy  – root->unref() (reclaims all memory)
//
// KEY FINDINGS (see tests/nodes/README_scalability.md for full analysis):
//
//  1. Flat and balanced binary trees: O(n) build and traversal.  Suitable
//     for tens-of-thousands of nodes.
//
//  2. Deep linear chain – CONSTRUCTION: O(n^2) when built top-down.
//     Root cause: SoChildList::append() calls parent->startNotify(), which
//     propagates upward through the existing ancestor chain.  For a chain of
//     depth d at step i, the notification cost is O(i), yielding O(n^2) total.
//     MITIGATION: build bottom-up (chain_bottomup) or disable notifications
//     during construction (chain_deferred) – both reduce to O(n).
//
//  3. Deep linear chain – TRAVERSAL: O(n^2) regardless of construction order.
//     Root cause: SoCacheElement::addElement() iterates through all currently
//     open SoCacheElement entries in the state stack.  For a chain at depth d,
//     d caches are simultaneously open, so every element access during
//     traversal costs O(d).  Total traversal cost = sum(d for d=0..n) = O(n^2).
//     MITIGATION: avoid very deep single-branch CSG chains; prefer balanced
//     trees or flat assembly lists.  Setting boundingBoxCaching=OFF removes
//     the O(n^2) addElement overhead but eliminates caching benefits.
//
// The function always returns 0 (characterisation only; no pass/fail on
// timing), but prints a CSV table to stdout for post-run analysis.
// =========================================================================
static int runScalabilityTests()
{
    // ----- configuration --------------------------------------------------
    // Sizes express the number of *leaf geometry nodes* (SoCube instances).
    // Total node count depends on the structure (see comments below).
    static const int kFlatSizes[]  = { 100, 500, 1000, 5000, 10000, 20000 };
    static const int kTreeSizes[]  = { 100, 500, 1000, 5000, 10000, 20000 };
    // Deep recursive traversal can exhaust the system stack; cap accordingly.
    static const int kChainSizes[] = { 100, 500, 1000, 2000, 5000 };
    static const int CHAIN_MAX     = 5000;
    (void)CHAIN_MAX;

    printf("# Obol Scene Graph Scalability Benchmark\n");
    printf("# structure, leaves, total_nodes,"
           " build_ms, bbox_ms, search_ms, destroy_ms\n");

    // ------------------------------------------------------------------
    // 1. Flat scene
    //    Layout: root SoSeparator
    //               +-- SoSeparator (group_0)
    //               |      +-- SoTranslation
    //               |      +-- SoCube
    //               +-- SoSeparator (group_1)
    //               ...
    //    Total nodes: 1 + 3*N
    // ------------------------------------------------------------------
    for (int n : kFlatSizes) {
        SbTime t0 = SbTime::getTimeOfDay();

        SoSeparator* root = new SoSeparator;
        root->ref();
        int cols = (int)std::sqrt((double)n);
        if (cols < 1) cols = 1;
        for (int i = 0; i < n; ++i) {
            SoSeparator* grp = new SoSeparator;
            SoTranslation* tr = new SoTranslation;
            tr->translation.setValue(float(i % cols) * 2.0f,
                                     float(i / cols) * 2.0f, 0.0f);
            grp->addChild(tr);
            grp->addChild(new SoCube);
            root->addChild(grp);
        }

        SbTime t1 = SbTime::getTimeOfDay();

        SoGetBoundingBoxAction bba(SbViewportRegion(800, 600));
        bba.apply(root);

        SbTime t2 = SbTime::getTimeOfDay();

        SoSearchAction sa;
        sa.setType(SoCube::getClassTypeId());
        sa.setInterest(SoSearchAction::ALL);
        sa.apply(root);

        SbTime t3 = SbTime::getTimeOfDay();

        root->unref();

        SbTime t4 = SbTime::getTimeOfDay();

        printf("flat, %d, %d, %.3f, %.3f, %.3f, %.3f\n",
               n,
               1 + 3 * n,
               (t1 - t0).getValue() * 1000.0,
               (t2 - t1).getValue() * 1000.0,
               (t3 - t2).getValue() * 1000.0,
               (t4 - t3).getValue() * 1000.0);
    }

    // ------------------------------------------------------------------
    // 2. Balanced binary tree (CSG-like)
    //    N leaf groups: each is SoSeparator + SoTranslation + SoCube.
    //    Internal nodes: SoSeparators that pair up children bottom-up.
    //    Depth ≈ ceil(log2(N)).
    //    Total nodes: 3*N leaves + (N-1) internal SoSeparators ≈ 4*N
    // ------------------------------------------------------------------
    for (int n : kTreeSizes) {
        SbTime t0 = SbTime::getTimeOfDay();

        // Build leaf groups
        std::vector<SoNode*> level;
        level.reserve((size_t)n);
        for (int i = 0; i < n; ++i) {
            SoSeparator* leaf = new SoSeparator;
            SoTranslation* tr = new SoTranslation;
            tr->translation.setValue(float(i) * 2.0f, 0.0f, 0.0f);
            leaf->addChild(tr);
            leaf->addChild(new SoCube);
            level.push_back(leaf);
        }

        // Pair up levels until a single root remains
        while (level.size() > 1) {
            std::vector<SoNode*> next;
            next.reserve((level.size() + 1) / 2);
            for (size_t i = 0; i < level.size(); i += 2) {
                SoSeparator* parent = new SoSeparator;
                parent->addChild(level[i]);
                if (i + 1 < level.size())
                    parent->addChild(level[i + 1]);
                next.push_back(parent);
            }
            level = std::move(next);
        }

        SoNode* treeRoot = level[0];
        treeRoot->ref();

        SbTime t1 = SbTime::getTimeOfDay();

        SoGetBoundingBoxAction bba(SbViewportRegion(800, 600));
        bba.apply(treeRoot);

        SbTime t2 = SbTime::getTimeOfDay();

        SoSearchAction sa;
        sa.setType(SoCube::getClassTypeId());
        sa.setInterest(SoSearchAction::ALL);
        sa.apply(treeRoot);

        SbTime t3 = SbTime::getTimeOfDay();

        treeRoot->unref();

        SbTime t4 = SbTime::getTimeOfDay();

        // internal separators ≈ n-1; leaves: n * (sep + trans + cube) = 3n
        int total = 3 * n + (n - 1);
        printf("binary_tree, %d, %d, %.3f, %.3f, %.3f, %.3f\n",
               n, total,
               (t1 - t0).getValue() * 1000.0,
               (t2 - t1).getValue() * 1000.0,
               (t3 - t2).getValue() * 1000.0,
               (t4 - t3).getValue() * 1000.0);
    }

    // ------------------------------------------------------------------
    // 3. Deep linear chain
    //    root → sep_1 → sep_2 → … → sep_(N-1) → SoCube
    //    Total nodes: N separators + 1 cube = N+1
    //    NOTE: Deep trees may exhaust the system call stack during
    //          recursive action traversal; CHAIN_MAX is set conservatively.
    // ------------------------------------------------------------------
    for (int n : kChainSizes) {
        SbTime t0 = SbTime::getTimeOfDay();

        SoSeparator* root = new SoSeparator;
        root->ref();
        SoSeparator* cur = root;
        for (int i = 1; i < n; ++i) {
            SoSeparator* child = new SoSeparator;
            cur->addChild(child);
            cur = child;
        }
        cur->addChild(new SoCube);

        SbTime t1 = SbTime::getTimeOfDay();

        SoGetBoundingBoxAction bba(SbViewportRegion(800, 600));
        bba.apply(root);

        SbTime t2 = SbTime::getTimeOfDay();

        SoSearchAction sa;
        sa.setType(SoCube::getClassTypeId());
        sa.setInterest(SoSearchAction::ALL);
        sa.apply(root);

        SbTime t3 = SbTime::getTimeOfDay();

        root->unref();

        SbTime t4 = SbTime::getTimeOfDay();

        printf("linear_chain, %d, %d, %.3f, %.3f, %.3f, %.3f\n",
               n,
               n + 1,
               (t1 - t0).getValue() * 1000.0,
               (t2 - t1).getValue() * 1000.0,
               (t3 - t2).getValue() * 1000.0,
               (t4 - t3).getValue() * 1000.0);
    }

    printf("# Benchmark complete\n");
    printf("# NOTE: linear_chain build times show O(n^2) growth due to\n");
    printf("#       upward-notification on each addChild() call.\n");
    printf("#       Mitigations: bottom-up construction or\n");
    printf("#       enableNotify(FALSE)/enableNotify(TRUE) bracketing.\n");

    // ------------------------------------------------------------------
    // 4. Deep linear chain – bottom-up construction
    //    Build leaf first, then wrap in parent separators moving up.
    //    Each addChild() has no ancestor chain yet → O(1) notification.
    //    Total nodes: N separators + 1 cube = N+1 (same as linear_chain)
    // ------------------------------------------------------------------
    printf("# Bottom-up chain: builds deepest node first (O(n) vs O(n^2) naive)\n");
    for (int n : kChainSizes) {
        SbTime t0 = SbTime::getTimeOfDay();

        // Start with the leaf cube
        SoNode* cur = new SoCube;
        // Wrap in separators from deepest outward
        for (int i = 0; i < n; ++i) {
            SoSeparator* parent = new SoSeparator;
            parent->addChild(cur);   // cur has no ancestor yet → O(1) notify
            cur = parent;
        }
        cur->ref();  // cur is now the outermost separator (root)

        SbTime t1 = SbTime::getTimeOfDay();

        SoGetBoundingBoxAction bba(SbViewportRegion(800, 600));
        bba.apply(cur);

        SbTime t2 = SbTime::getTimeOfDay();

        SoSearchAction sa;
        sa.setType(SoCube::getClassTypeId());
        sa.setInterest(SoSearchAction::ALL);
        sa.apply(cur);

        SbTime t3 = SbTime::getTimeOfDay();

        cur->unref();

        SbTime t4 = SbTime::getTimeOfDay();

        printf("chain_bottomup, %d, %d, %.3f, %.3f, %.3f, %.3f\n",
               n,
               n + 1,
               (t1 - t0).getValue() * 1000.0,
               (t2 - t1).getValue() * 1000.0,
               (t3 - t2).getValue() * 1000.0,
               (t4 - t3).getValue() * 1000.0);
    }

    // ------------------------------------------------------------------
    // 5. Deep linear chain – deferred notification
    //    Top-down build but with enableNotify(FALSE) on each node before
    //    adding its child, re-enabled afterwards.
    //    Prevents the O(depth) upward notification on each addChild().
    // ------------------------------------------------------------------
    printf("# Deferred-notify chain: top-down but notifications suppressed\n");
    for (int n : kChainSizes) {
        SbTime t0 = SbTime::getTimeOfDay();

        SoSeparator* root = new SoSeparator;
        root->ref();
        root->enableNotify(FALSE);
        SoSeparator* cur = root;
        for (int i = 1; i < n; ++i) {
            SoSeparator* child = new SoSeparator;
            child->enableNotify(FALSE);
            cur->addChild(child);
            cur = child;
        }
        cur->addChild(new SoCube);
        // Re-enable notifications from tip to root
        cur = root;
        cur->enableNotify(TRUE);
        for (int i = 1; i < n; ++i) {
            SoSeparator* child =
                static_cast<SoSeparator*>(
                    static_cast<SoGroup*>(cur)->getChild(0));
            if (!child || !child->isOfType(SoSeparator::getClassTypeId()))
                break;
            child->enableNotify(TRUE);
            cur = child;
        }

        SbTime t1 = SbTime::getTimeOfDay();

        SoGetBoundingBoxAction bba(SbViewportRegion(800, 600));
        bba.apply(root);

        SbTime t2 = SbTime::getTimeOfDay();

        SoSearchAction sa;
        sa.setType(SoCube::getClassTypeId());
        sa.setInterest(SoSearchAction::ALL);
        sa.apply(root);

        SbTime t3 = SbTime::getTimeOfDay();

        root->unref();

        SbTime t4 = SbTime::getTimeOfDay();

        printf("chain_deferred, %d, %d, %.3f, %.3f, %.3f, %.3f\n",
               n,
               n + 1,
               (t1 - t0).getValue() * 1000.0,
               (t2 - t1).getValue() * 1000.0,
               (t3 - t2).getValue() * 1000.0,
               (t4 - t3).getValue() * 1000.0);
    }

    printf("# Scalability benchmark complete\n");
    return 0; // characterisation only – no pass/fail on timing
}

// =========================================================================
// Static registrations – run at program start
// =========================================================================

// --- Visual scene registrations ---

REGISTER_TEST(primitives, ObolTest::TestCategory::Rendering,
    "2x2 grid: sphere, cube, cone, cylinder",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createPrimitives;
);

REGISTER_TEST(materials, ObolTest::TestCategory::Rendering,
    "Four spheres demonstrating material properties",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createMaterials;
);

REGISTER_TEST(lighting, ObolTest::TestCategory::Rendering,
    "Scene lit by directional, point and spot lights (NanoRT: shadows via SoRaytracingParams)",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createLighting;
);

REGISTER_TEST(transforms, ObolTest::TestCategory::Rendering,
    "Hierarchical rotation and translation transforms",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createTransforms;
);

REGISTER_TEST(cameras, ObolTest::TestCategory::Rendering,
    "Row of coloured spheres with explicit perspective camera",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createCameras;
);

REGISTER_TEST(texture, ObolTest::TestCategory::Rendering,
    "Checkerboard-textured cube",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createTexture;
);

REGISTER_TEST(text, ObolTest::TestCategory::Rendering,
    "SoText2 and SoText3 labels",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createText;
);

REGISTER_TEST(gradient, ObolTest::TestCategory::Rendering,
    "Background gradient via callback node",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createGradient;
);

REGISTER_TEST(colored_cube, ObolTest::TestCategory::Rendering,
    "Simple red cube with lighting (smoke test)",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createColoredCube;
);

REGISTER_TEST(coordinates, ObolTest::TestCategory::Rendering,
    "Colour-coded XYZ axis lines",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createCoordinates;
);

REGISTER_TEST(shadow, ObolTest::TestCategory::Rendering,
    "Shadow-casting scene: SoShadowGroup (GL) + SoRaytracingParams (NanoRT)",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createShadow;
);

REGISTER_TEST(draggers, ObolTest::TestCategory::Draggers,
    "Interactive draggers (SoTranslate1, SoRotateSpherical)",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createDraggers;
);

REGISTER_TEST(hud, ObolTest::TestCategory::Misc,
    "Head-up display overlay using orthographic camera",
    e.has_visual = true;
    e.has_interactive = false;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createHUD;
);

REGISTER_TEST(lod, ObolTest::TestCategory::Nodes,
    "Level of detail (SoLOD) switching between representations",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createLOD;
);

REGISTER_TEST(transparency, ObolTest::TestCategory::Rendering,
    "Alpha-blended overlapping spheres",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createTransparency;
);

REGISTER_TEST(drawstyle, ObolTest::TestCategory::Rendering,
    "Filled, wireframe, and points draw style comparison",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createDrawStyle;
);

REGISTER_TEST(indexed_face_set, ObolTest::TestCategory::Rendering,
    "SoIndexedFaceSet tetrahedron",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createIndexedFaceSet;
);

REGISTER_TEST(manips, ObolTest::TestCategory::Manips,
    "Interactive manipulators (SoTrackballManip, SoTabBoxManip)",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createManips;
);

REGISTER_TEST(scene, ObolTest::TestCategory::Rendering,
    "2×2 grid of primitives: sphere, cube, cone, cylinder",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createScene;
);

REGISTER_TEST(face_set, ObolTest::TestCategory::Rendering,
    "SoFaceSet green quad in the lower-left quadrant",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createFaceSet;
);

REGISTER_TEST(line_set, ObolTest::TestCategory::Rendering,
    "SoLineSet red horizontal line across the viewport",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createLineSet;
);

REGISTER_TEST(indexed_line_set, ObolTest::TestCategory::Rendering,
    "SoIndexedLineSet: green horizontal, red diagonal, blue V-shape",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createIndexedLineSet;
);

REGISTER_TEST(point_set, ObolTest::TestCategory::Rendering,
    "SoPointSet: four distinctly coloured points in the four quadrants",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createPointSet;
);

REGISTER_TEST(triangle_strip_set, ObolTest::TestCategory::Rendering,
    "SoTriangleStripSet: emissive blue strip quad in lower half",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createTriangleStripSet;
);

REGISTER_TEST(quad_mesh, ObolTest::TestCategory::Rendering,
    "SoQuadMesh: 5×5 colour-gradient grid (red → blue across columns)",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createQuadMesh;
);

REGISTER_TEST(vertex_colors, ObolTest::TestCategory::Rendering,
    "Per-vertex coloured quad via SoPackedColor + SoIndexedFaceSet",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createVertexColors;
);

REGISTER_TEST(switch_visibility, ObolTest::TestCategory::Nodes,
    "Two coloured spheres controlled by SoSwitch (both visible)",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createSwitchVisibility;
);

REGISTER_TEST(sphere_position, ObolTest::TestCategory::Rendering,
    "Emissive sphere offset from centre with SoOrthographicCamera",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createSpherePosition;
);

REGISTER_TEST(checker_texture, ObolTest::TestCategory::Rendering,
    "Checkerboard-textured cube via procedural SoTexture2",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createCheckerTexture;
);

REGISTER_TEST(clip_plane, ObolTest::TestCategory::Rendering,
    "Large sphere clipped in half by SoClipPlane at Y=0",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createClipPlane;
);

REGISTER_TEST(array_multiple_copy, ObolTest::TestCategory::Nodes,
    "3×3 SoArray grid of spheres + three SoMultipleCopy cubes",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createArrayMultipleCopy;
);

REGISTER_TEST(annotation, ObolTest::TestCategory::Nodes,
    "SoAnnotation sphere composited on top of a background sphere",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createAnnotation;
);

REGISTER_TEST(ascii_text, ObolTest::TestCategory::Nodes,
    "SoAsciiText \"HELLO\" centred with perspective camera",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createAsciiText;
);

REGISTER_TEST(reset_transform, ObolTest::TestCategory::Rendering,
    "SoResetTransform: blue sphere at offset + red sphere reset to origin",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createResetTransform;
);

REGISTER_TEST(shape_hints, ObolTest::TestCategory::Rendering,
    "SoShapeHints SOLID+CCW sphere with backface culling enabled",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createShapeHints;
);

REGISTER_TEST(image_node, ObolTest::TestCategory::Nodes,
    "SoImage: red/green checkerboard image node centred in viewport",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createImageNode;
);

REGISTER_TEST(marker_set, ObolTest::TestCategory::Nodes,
    "SoMarkerSet: five markers arranged in a cross pattern",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createMarkerSet;
);

REGISTER_TEST(material_binding, ObolTest::TestCategory::Rendering,
    "SoMaterialBinding PER_FACE: red left quad, blue right quad",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createMaterialBinding;
);

// --- Texture / Visual / HUD rendering tests ---

REGISTER_TEST(alpha_test, ObolTest::TestCategory::Rendering,
    "SoAlphaTest GREATER threshold scene with checkerboard RGBA texture",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = false;
    e.create_scene = ObolTest::Scenes::createAlphaTest;
);

REGISTER_TEST(background_gradient, ObolTest::TestCategory::Rendering,
    "2x2 primitive grid scene (background gradient set on renderer)",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createBackgroundGradient;
);

REGISTER_TEST(bump_map, ObolTest::TestCategory::Rendering,
    "Sphere with SoBumpMap sinusoidal normal-map texture",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = false;
    e.create_scene = ObolTest::Scenes::createBumpMap;
);

REGISTER_TEST(multi_texture, ObolTest::TestCategory::Rendering,
    "Sphere with two SoTextureUnit texture units (multi-texture)",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = false;
    e.create_scene = ObolTest::Scenes::createMultiTexture;
);

REGISTER_TEST(texture3, ObolTest::TestCategory::Rendering,
    "Cube with procedural 8x8x8 SoTexture3 (3-D texture)",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = false;
    e.create_scene = ObolTest::Scenes::createTexture3;
);

REGISTER_TEST(texture_transform, ObolTest::TestCategory::Rendering,
    "Two textured quads: plain and SoTexture2Transform applied",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = false;
    e.create_scene = ObolTest::Scenes::createTextureTransform;
);

REGISTER_TEST(environment, ObolTest::TestCategory::Rendering,
    "Red sphere with SoEnvironment (no fog, high ambient intensity)",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = false;
    e.create_scene = ObolTest::Scenes::createEnvironment;
);

REGISTER_TEST(cubemap, ObolTest::TestCategory::Rendering,
    "Sphere with SoTextureCubeMap cube-map (six solid-colour faces)",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = false;
    e.create_scene = ObolTest::Scenes::createCubemap;
);

REGISTER_TEST(scene_texture, ObolTest::TestCategory::Rendering,
    "Flat quad with SoSceneTexture2 render-to-texture (orange cone sub-scene)",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = false;
    e.create_scene = ObolTest::Scenes::createSceneTexture;
);

REGISTER_TEST(hud_overlay, ObolTest::TestCategory::Rendering,
    "Blue sphere with HUD overlay: status bar and side-menu buttons",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = false;
    e.create_scene = ObolTest::Scenes::createHUDOverlay;
);

REGISTER_TEST(hud_no3d, ObolTest::TestCategory::Rendering,
    "Pure 2-D HUD scene: title, menu buttons, info panel, status bar",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = false;
    e.create_scene = ObolTest::Scenes::createHUDNo3D;
);

REGISTER_TEST(hud_interaction, ObolTest::TestCategory::Rendering,
    "Blue sphere with interactive HUD buttons (static visual layout)",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = false;
    e.create_scene = ObolTest::Scenes::createHUDInteraction;
);

REGISTER_TEST(text3_parts, ObolTest::TestCategory::Rendering,
    "SoText3 with FRONT, ALL, and BACK parts visible in one scene",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createText3Parts;
);

REGISTER_TEST(depth_buffer, ObolTest::TestCategory::Rendering,
    "Near red cube + far blue sphere with SoDepthBuffer LEQUAL depth test",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = false;
    e.create_scene = ObolTest::Scenes::createDepthBuffer;
);

REGISTER_TEST(procedural_shape, ObolTest::TestCategory::Rendering,
    "Solid and wireframe truncated-cone SoProceduralShape side by side",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createProceduralShape;
);

REGISTER_TEST(gl_big_image, ObolTest::TestCategory::Rendering,
    "Textured quad with SoTextureScalePolicy::FRACTURE (SoGLBigImage path)",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = false;
    e.create_scene = ObolTest::Scenes::createGLBigImage;
);

REGISTER_TEST(image_deep, ObolTest::TestCategory::Nodes,
    "SoImage node with 48x48 RGBA checkerboard (deep coverage)",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = false;
    e.create_scene = ObolTest::Scenes::createImageDeep;
);

REGISTER_TEST(shader_program, ObolTest::TestCategory::Rendering,
    "Sphere with basic GLSL vertex + fragment SoShaderProgram",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = false;
    e.create_scene = ObolTest::Scenes::createShaderProgram;
);

REGISTER_TEST(sorender_manager, ObolTest::TestCategory::Rendering,
    "Camera + light + SoCube scene rendered via SoRenderManager",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createSoRenderManager;
);

REGISTER_TEST(gl_features, ObolTest::TestCategory::Rendering,
    "Textured sphere exercising SoGLImage / SoGLDriverDatabase paths",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = false;
    e.create_scene = ObolTest::Scenes::createGLFeatures;
);

REGISTER_TEST(quad_mesh_deep, ObolTest::TestCategory::Rendering,
    "4x4 SoQuadMesh with PER_FACE material binding (nine coloured faces)",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createQuadMeshDeep;
);

REGISTER_TEST(offscreen, ObolTest::TestCategory::Rendering,
    "Red sphere scene for SoOffscreenRenderer API coverage",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createOffscreen;
);

REGISTER_TEST(bbox_action, ObolTest::TestCategory::Actions,
    "Three coloured spheres for SoGetBoundingBoxAction coverage",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createBBoxAction;
);

REGISTER_TEST(search_action, ObolTest::TestCategory::Actions,
    "Hierarchical scene with named nodes for SoSearchAction coverage",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createSearchAction;
);

REGISTER_TEST(callback_action, ObolTest::TestCategory::Actions,
    "Sphere + cube + cone scene for SoCallbackAction triangle traversal",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createCallbackAction;
);

REGISTER_TEST(callback_action_deep, ObolTest::TestCategory::Actions,
    "All primitive shape types for deep SoCallbackAction primitive coverage",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createCallbackActionDeep;
);

REGISTER_TEST(callback_node, ObolTest::TestCategory::Nodes,
    "Three SoCallback nodes interleaved with geometry",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createCallbackNode;
);

REGISTER_TEST(event_propagation, ObolTest::TestCategory::Actions,
    "SoEventCallback nodes in nested separators with sphere + cube",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createEventPropagation;
);

REGISTER_TEST(path_operations, ObolTest::TestCategory::Actions,
    "Sphere left + cube right for SoPath pick-and-copy tests",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createPathOperations;
);

REGISTER_TEST(write_read_action, ObolTest::TestCategory::Actions,
    "Red sphere + blue cube input scene for SoWriteAction / SoDB::readAll",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createWriteReadAction;
);

REGISTER_TEST(field_connections, ObolTest::TestCategory::Fields,
    "Sphere driven by SoComposeVec3f engine via field connection",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createFieldConnections;
);

REGISTER_TEST(sensors_rendering, ObolTest::TestCategory::Sensors,
    "Static sphere representing the final frame of a sensor-driven animation",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createSensorsRendering;
);

REGISTER_TEST(render_manager_full, ObolTest::TestCategory::Rendering,
    "Camera + light + SoCube scene for SoRenderManager comprehensive tests",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createRenderManagerFull;
);

REGISTER_TEST(sogl_bindings, ObolTest::TestCategory::Rendering,
    "9-point PER_VERTEX-coloured grid for SoGL PointSet binding variants",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = false;
    e.create_scene = ObolTest::Scenes::createSOGLBindings;
);

REGISTER_TEST(glrender_action_modes, ObolTest::TestCategory::Rendering,
    "Two semi-transparent overlapping objects for SoGLRenderAction mode tests",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = false;
    e.create_scene = ObolTest::Scenes::createGLRenderActionModes;
);

REGISTER_TEST(glrender_deep, ObolTest::TestCategory::Rendering,
    "Three semi-transparent spheres for deep SoGLRenderAction coverage",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = false;
    e.create_scene = ObolTest::Scenes::createGLRenderDeep;
);

REGISTER_TEST(offscreen_advanced, ObolTest::TestCategory::Rendering,
    "Camera + light + cube for SoOffscreenRenderer advanced API tests",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createOffscreenAdvanced;
);

REGISTER_TEST(view_volume_ops, ObolTest::TestCategory::Rendering,
    "Perspective camera + purple sphere for SbViewVolume operation tests",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createViewVolumeOps;
);

REGISTER_TEST(lod_picking, ObolTest::TestCategory::Rendering,
    "Three SoLOD nodes (sphere/cube/cone levels) for LOD + picking tests",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createLODPicking;
);

REGISTER_TEST(stt_gl, ObolTest::TestCategory::Rendering,
    "Five SoText2 rows in an orthographic viewport for STT GL reference",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createSTTGL;
);

// --- Unit test registrations ---

REGISTER_TEST(unit_actions, ObolTest::TestCategory::Actions,
    "Action type checking, bounding box, search action",
    e.has_visual = false;
    e.run_unit = runActionsTests;
);

REGISTER_TEST(unit_raytrace_action, ObolTest::TestCategory::Actions,
    "SoRaytraceRenderAction type, viewport, triangle collection, lights",
    e.has_visual = false;
    e.run_unit = runRaytraceActionTests;
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

REGISTER_TEST(unit_scalability, ObolTest::TestCategory::Nodes,
    "Scene graph scalability: flat / binary-tree / linear-chain at 100..20000 nodes",
    e.has_visual = false;
    e.run_unit = runScalabilityTests;
);

// ---- Group 4: Interaction / Draggers / Special ----

REGISTER_TEST(camera_interaction, ObolTest::TestCategory::Rendering,
    "Three-object scene (sphere, cube, cone) for camera manipulation testing",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createCameraInteraction;
);

REGISTER_TEST(scene_interaction, ObolTest::TestCategory::Rendering,
    "Dynamic sphere/cube/cylinder scene for scene-graph mutation testing",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createSceneInteraction;
);

REGISTER_TEST(engine_interaction, ObolTest::TestCategory::Engines,
    "Sphere driven by SoComposeVec3f engine — engine-driven animation",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createEngineInteraction;
);

REGISTER_TEST(engine_converter, ObolTest::TestCategory::Engines,
    "Sphere with material driven via SoConvertAll automatic field-type conversion",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createEngineConverter;
);

REGISTER_TEST(event_callback_interaction, ObolTest::TestCategory::Events,
    "Green sphere behind SoSwitch with SoEventCallback node in the graph",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createEventCallbackInteraction;
);

REGISTER_TEST(pick_interaction, ObolTest::TestCategory::Actions,
    "Blue sphere for SoRayPickAction pick and highlight testing",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createPickInteraction;
);

REGISTER_TEST(pick_filter, ObolTest::TestCategory::Actions,
    "SoSelection + sphere + cube for SoSelection pick-filter callback tests",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createPickFilter;
);

REGISTER_TEST(selection_interaction, ObolTest::TestCategory::Nodes,
    "SoSelection (SHIFT policy) with sphere, cube, and cone",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createSelectionInteraction;
);

REGISTER_TEST(sensor_interaction, ObolTest::TestCategory::Sensors,
    "Gray sphere scene for sensor-driven field and node sensor testing",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createSensorInteraction;
);

REGISTER_TEST(nodekit_interaction, ObolTest::TestCategory::Nodes,
    "SoShapeKit containing a sphere — node-kit interaction scene",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createNodeKitInteraction;
);

REGISTER_TEST(manip_sequences, ObolTest::TestCategory::Manips,
    "Sphere with SoCenterballManip — complex manipulator sequence scene",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createManipSequences;
);

REGISTER_TEST(light_manips, ObolTest::TestCategory::Manips,
    "Three spheres + floor lit by a SoDirectionalLightManip",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createLightManips;
);

REGISTER_TEST(simple_draggers, ObolTest::TestCategory::Draggers,
    "Cube + SoTranslate1Dragger — simple dragger types scene",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createSimpleDraggers;
);

REGISTER_TEST(arb8_draggers, ObolTest::TestCategory::Draggers,
    "ARB8-style box (solid + wireframe) for procedural dragger topology tests",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createArb8Draggers;
);

REGISTER_TEST(arb8_edit_cycle, ObolTest::TestCategory::Draggers,
    "ARB8 box with golden corner-handle spheres — edit-cycle visualization",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createArb8EditCycle;
);

REGISTER_TEST(ext_selection, ObolTest::TestCategory::Nodes,
    "SoExtSelection (LASSO, FULL_BBOX) with three pickable shapes",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createExtSelection;
);

REGISTER_TEST(ext_selection_events, ObolTest::TestCategory::Nodes,
    "SoExtSelection (RECTANGLE, PART_BBOX) with three shapes for event tests",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createExtSelectionEvents;
);

REGISTER_TEST(raypick_shapes, ObolTest::TestCategory::Actions,
    "Four-quadrant scene: SoLineSet, SoIndexedLineSet, SoPointSet, SoCylinder",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createRaypickShapes;
);

REGISTER_TEST(shadow_advanced, ObolTest::TestCategory::Rendering,
    "Advanced shadows: SoShadowGroup + SoShadowSpotLight + sphere + ground plane",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = false;
    e.create_scene = ObolTest::Scenes::createShadowAdvanced;
);

REGISTER_TEST(rt_proxy_shapes, ObolTest::TestCategory::Rendering,
    "RT proxy shapes: SoLineSet, SoIndexedLineSet, SoPointSet, SoCylinder quad",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createRTProxyShapes;
);

REGISTER_TEST(nanort, ObolTest::TestCategory::Rendering,
    "Four primitives + SoRaytracingParams — NanoRT raytracing scene",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createNanoRT;
);

REGISTER_TEST(nanort_shadow, ObolTest::TestCategory::Rendering,
    "Ground plane + red sphere + SoRaytracingParams(shadows) — NanoRT shadow scene",
    e.has_visual = true;
    e.has_interactive = true;
    e.nanort_ok = true;
    e.create_scene = ObolTest::Scenes::createNanoRTShadow;
);

} // anonymous namespace
