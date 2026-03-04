/*
 * render_sensor_interaction.cpp
 *
 * Tests sensor-driven scene changes as user applications use them:
 *
 *   1. SoFieldSensor on SoTransform::translation – sensor fires when the
 *      field changes; counter is verified and intermediate renders captured.
 *   2. SoNodeSensor on a SoMaterial – sensor fires when material is changed,
 *      triggering re-render (simulated by processing the delay queue).
 *   3. SoFieldSensor with priority and detach/reattach cycle.
 *   4. Multiple sensors on same field – both fire.
 *   5. SoFieldSensor detach prevents firing after detach.
 *   6. Field connection + sensor: changing a source field propagates to a
 *      connected destination field and the destination's sensor fires.
 *
 * Each test that involves visual output writes RGB images to basepath_<name>.rgb
 *
 * Returns 0 on pass, non-0 on failure.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"

#include <Inventor/SoDB.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/sensors/SoFieldSensor.h>
#include <Inventor/sensors/SoNodeSensor.h>
#include <Inventor/sensors/SoSensorManager.h>

#include <cstdio>
#include <cmath>

// ---------------------------------------------------------------------------
// Test 1: SoFieldSensor fires when SoTransform::translation changes
// ---------------------------------------------------------------------------

struct T1Data {
    int    count;
    SbVec3f lastValue;
};

static void t1CB(void *ud, SoSensor *)
{
    T1Data *d = static_cast<T1Data *>(ud);
    ++d->count;
}

static bool test1_fieldSensorOnTranslation(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0,0,5); cam->height.setValue(8.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);
    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.8f, 0.3f, 0.2f);
    root->addChild(mat);
    SoTransform *xf = new SoTransform;
    root->addChild(xf);
    root->addChild(new SoSphere);

    T1Data data = { 0, SbVec3f(0,0,0) };
    SoFieldSensor *sensor = new SoFieldSensor(t1CB, &data);
    sensor->attach(&xf->translation);

    // Change translation 3 times
    SbVec3f positions[] = {
        SbVec3f(-2.0f, 0.0f, 0.0f),
        SbVec3f( 0.0f, 1.5f, 0.0f),
        SbVec3f( 1.5f,-1.0f, 0.0f)
    };
    const char *labels[] = { "left", "up", "rightdown" };
    bool allOk = true;

    for (int i = 0; i < 3; ++i) {
        xf->translation.setValue(positions[i]);
        SoDB::getSensorManager()->processDelayQueue(TRUE);

        char fname[1024];
        snprintf(fname, sizeof(fname), "%s_t1_%s.rgb", basepath, labels[i]);
        if (!renderToFile(root, fname))
            allOk = false;
    }

    bool ok = allOk && (data.count >= 3);
    if (!ok) {
        fprintf(stderr, "  FAIL test1: count=%d (expected >=3)\n", data.count);
    } else {
        printf("  PASS test1: SoFieldSensor fired %d times on translation changes\n",
               data.count);
    }

    delete sensor;
    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 2: SoNodeSensor fires when SoMaterial is changed
// ---------------------------------------------------------------------------

struct T2Data { int count; };

static void t2CB(void *ud, SoSensor *) { ++static_cast<T2Data*>(ud)->count; }

static bool test2_nodeSensorOnMaterial(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0,0,5); cam->height.setValue(4.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);
    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.5f,0.5f,0.5f);
    root->addChild(mat);
    root->addChild(new SoSphere);

    T2Data data = { 0 };
    SoNodeSensor *sensor = new SoNodeSensor(t2CB, &data);
    sensor->attach(mat);

    SbColor colors[] = {
        SbColor(1.0f, 0.2f, 0.2f),  // red
        SbColor(0.2f, 0.8f, 0.2f),  // green
        SbColor(0.2f, 0.2f, 1.0f),  // blue
        SbColor(1.0f, 1.0f, 0.2f),  // yellow
    };
    const char *names[] = {"red","green","blue","yellow"};
    bool allOk = true;

    for (int i = 0; i < 4; ++i) {
        mat->diffuseColor.setValue(colors[i]);
        SoDB::getSensorManager()->processDelayQueue(TRUE);
        char fname[1024];
        snprintf(fname, sizeof(fname), "%s_t2_%s.rgb", basepath, names[i]);
        if (!renderToFile(root, fname)) allOk = false;
    }

    bool ok = allOk && (data.count >= 4);
    if (!ok) {
        fprintf(stderr, "  FAIL test2: nodeCount=%d (expected >=4)\n", data.count);
    } else {
        printf("  PASS test2: SoNodeSensor fired %d times on material changes\n",
               data.count);
    }

    delete sensor;
    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 3: SoFieldSensor detach/reattach cycle
// ---------------------------------------------------------------------------

static int g_t3_count = 0;
static void t3CB(void *, SoSensor *) { ++g_t3_count; }

static bool test3_detachReattach()
{
    SoTransform *xf = new SoTransform;
    xf->ref();

    SoFieldSensor *sensor = new SoFieldSensor(t3CB, nullptr);
    sensor->attach(&xf->translation);

    g_t3_count = 0;
    xf->translation.setValue(1.0f, 0.0f, 0.0f);
    SoDB::getSensorManager()->processDelayQueue(TRUE);
    int afterFirst = g_t3_count; // should be 1

    // Detach
    sensor->detach();
    xf->translation.setValue(2.0f, 0.0f, 0.0f);
    SoDB::getSensorManager()->processDelayQueue(TRUE);
    int afterDetach = g_t3_count; // should still be 1

    // Reattach
    sensor->attach(&xf->translation);
    xf->translation.setValue(3.0f, 0.0f, 0.0f);
    SoDB::getSensorManager()->processDelayQueue(TRUE);
    int afterReattach = g_t3_count; // should be 2

    bool ok = (afterFirst == 1) && (afterDetach == 1) && (afterReattach == 2);
    if (!ok) {
        fprintf(stderr, "  FAIL test3: first=%d detach=%d reattach=%d\n",
                afterFirst, afterDetach, afterReattach);
    } else {
        printf("  PASS test3: detach/reattach cycle correct (%d,%d,%d)\n",
               afterFirst, afterDetach, afterReattach);
    }

    delete sensor;
    xf->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 4: Multiple sensors on same field – both fire
// ---------------------------------------------------------------------------

static int g_t4a = 0, g_t4b = 0;
static void t4aCB(void *, SoSensor *) { ++g_t4a; }
static void t4bCB(void *, SoSensor *) { ++g_t4b; }

static bool test4_multiSensors()
{
    SoTransform *xf = new SoTransform;
    xf->ref();

    SoFieldSensor *sA = new SoFieldSensor(t4aCB, nullptr);
    SoFieldSensor *sB = new SoFieldSensor(t4bCB, nullptr);
    sA->attach(&xf->translation);
    sB->attach(&xf->translation);

    g_t4a = g_t4b = 0;
    xf->translation.setValue(1,0,0);
    SoDB::getSensorManager()->processDelayQueue(TRUE);
    xf->translation.setValue(2,0,0);
    SoDB::getSensorManager()->processDelayQueue(TRUE);

    bool ok = (g_t4a >= 2) && (g_t4b >= 2);
    if (!ok) {
        fprintf(stderr, "  FAIL test4: t4a=%d t4b=%d (expected both >=2)\n",
                g_t4a, g_t4b);
    } else {
        printf("  PASS test4: both sensors fired (a=%d b=%d)\n", g_t4a, g_t4b);
    }

    delete sA;
    delete sB;
    xf->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 5: SoFieldSensor detach prevents firing
// ---------------------------------------------------------------------------

static int g_t5_count = 0;
static void t5CB(void *, SoSensor *) { ++g_t5_count; }

static bool test5_detachPrevents()
{
    SoTransform *xf = new SoTransform;
    xf->ref();

    SoFieldSensor *sensor = new SoFieldSensor(t5CB, nullptr);
    sensor->attach(&xf->translation);

    g_t5_count = 0;
    xf->translation.setValue(1,0,0);
    SoDB::getSensorManager()->processDelayQueue(TRUE);
    int beforeDetach = g_t5_count;

    sensor->detach();
    xf->translation.setValue(2,0,0);
    xf->translation.setValue(3,0,0);
    xf->translation.setValue(4,0,0);
    SoDB::getSensorManager()->processDelayQueue(TRUE);
    int afterDetach = g_t5_count;

    bool ok = (beforeDetach >= 1) && (afterDetach == beforeDetach);
    if (!ok) {
        fprintf(stderr, "  FAIL test5: before=%d after=%d (should be equal)\n",
                beforeDetach, afterDetach);
    } else {
        printf("  PASS test5: detach prevents firing (before=%d, after=%d unchanged)\n",
               beforeDetach, afterDetach);
    }

    delete sensor;
    xf->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 6: Field connection + sensor – changing source propagates
// ---------------------------------------------------------------------------

static int g_t6_count = 0;
static void t6CB(void *, SoSensor *) { ++g_t6_count; }

static bool test6_fieldConnectionSensor(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0,0,5); cam->height.setValue(6.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    // Source transform (not in graph)
    SoTransform *src = new SoTransform;
    src->ref();

    // Destination transform (in graph) – its translation is connected from src
    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.3f, 0.7f, 0.5f);
    root->addChild(mat);
    SoTransform *dst = new SoTransform;
    root->addChild(dst);
    root->addChild(new SoSphere);

    // Connect src.scaleFactor → dst.scaleFactor
    dst->scaleFactor.connectFrom(&src->scaleFactor);

    // Sensor on destination field
    SoFieldSensor *sensor = new SoFieldSensor(t6CB, nullptr);
    sensor->attach(&dst->scaleFactor);

    g_t6_count = 0;

    // Change source: should propagate to dst, firing the sensor
    float scales[] = { 0.5f, 1.5f, 2.0f };
    const char *labels[] = { "small", "medium", "large" };
    bool allOk = true;

    for (int i = 0; i < 3; ++i) {
        src->scaleFactor.setValue(scales[i], scales[i], scales[i]);
        SoDB::getSensorManager()->processDelayQueue(TRUE);
        char fname[1024];
        snprintf(fname, sizeof(fname), "%s_t6_%s.rgb", basepath, labels[i]);
        if (!renderToFile(root, fname)) allOk = false;
    }

    bool ok = allOk && (g_t6_count >= 3);
    if (!ok) {
        fprintf(stderr, "  FAIL test6: sensor fired %d times (expected >=3)\n",
                g_t6_count);
    } else {
        printf("  PASS test6: field connection + sensor: fired %d times\n",
               g_t6_count);
    }

    delete sensor;
    dst->scaleFactor.disconnect();
    src->unref();
    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char **argv)
{
    initCoinHeadless();

    const char *basepath = (argc > 1) ? argv[1] : "render_sensor_interaction";

    /* Render the canonical factory scene as the primary output image.
     * This ensures obol_viewer and obol_render produce identical scenes. */
    {
        SoSeparator *fRoot = ObolTest::Scenes::createSensorInteraction(256, 256);
        SbViewportRegion fVp(256, 256);
        SoOffscreenRenderer fRen(fVp);
        fRen.setComponents(SoOffscreenRenderer::RGB);
        fRen.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
        if (fRen.render(fRoot)) {
            char primaryPath[4096];
            snprintf(primaryPath, sizeof(primaryPath), "%s.rgb", basepath);
            fRen.writeToRGB(primaryPath);
        }
        fRoot->unref();
    }
    int failures = 0;

    printf("\n=== Sensor-driven interaction tests ===\n");

    if (!test1_fieldSensorOnTranslation(basepath)) ++failures;
    if (!test2_nodeSensorOnMaterial(basepath))     ++failures;
    if (!test3_detachReattach())                   ++failures;
    if (!test4_multiSensors())                     ++failures;
    if (!test5_detachPrevents())                   ++failures;
    if (!test6_fieldConnectionSensor(basepath))    ++failures;

    printf("\n=== Summary: %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
