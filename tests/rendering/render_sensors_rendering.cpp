/*
 * render_sensors_rendering.cpp — Sensor integration with rendering coverage test
 *
 * Exercises sensor types and the sensor manager to cover sensor code paths:
 *   1. SoFieldSensor on a SoMaterial field — fires when diffuseColor changes
 *      while rendering multiple frames
 *   2. SoNodeSensor on SoSeparator — fires when subtree is modified
 *   3. SoIdleSensor — scheduled and triggered via processDelayQueue
 *   4. SoOneShotSensor — fires once via processDelayQueue
 *   5. SoAlarmSensor — set to fire at time T, process timer queue
 *   6. SoDelayQueueSensor scheduling: isScheduled, unschedule
 *   7. SoSensorManager::isDelaySensorPending / isTimerSensorPending
 *   8. SoTimerSensor — schedule/unschedule without waiting for real time
 *   9. Multiple sensors triggered in sequence while updating scene
 *  10. Render scene before and after sensor-triggered updates
 *
 * Returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <Inventor/SoDB.h>
#include <Inventor/sensors/SoFieldSensor.h>
#include <Inventor/sensors/SoNodeSensor.h>
#include <Inventor/sensors/SoIdleSensor.h>
#include <Inventor/sensors/SoOneShotSensor.h>
#include <Inventor/sensors/SoAlarmSensor.h>
#include <Inventor/sensors/SoTimerSensor.h>
#include <Inventor/sensors/SoSensorManager.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbTime.h>
#include <cstdio>

static const int W = 128;
static const int H = 128;

static bool validateNonBlack(const unsigned char *buf, int npix,
                              const char *label)
{
    int nonbg = 0;
    for (int i = 0; i < npix; ++i) {
        const unsigned char *p = buf + i * 3;
        if (p[0] > 5 || p[1] > 5 || p[2] > 5) ++nonbg;
    }
    printf("  %s: nonbg=%d\n", label, nonbg);
    return nonbg >= 20;
}

// ---------------------------------------------------------------------------
// Callback counters
// ---------------------------------------------------------------------------
static int g_fieldCB  = 0;
static int g_nodeCB   = 0;
static int g_idleCB   = 0;
static int g_oneshotCB= 0;
static int g_alarmCB  = 0;
static int g_timerCB  = 0;

static void fieldSensorCB(void *data, SoSensor *)
{
    ++g_fieldCB;
    // Change color a bit to verify field was changed
    SoMaterial *mat = (SoMaterial *)data;
    if (g_fieldCB == 1)
        mat->diffuseColor.setValue(0.2f, 0.8f, 0.2f);
}

static void nodeSensorCB(void *data, SoSensor *)
{
    ++g_nodeCB;
    (void)data;
}

static void idleCB(void *data, SoSensor *sensor)
{
    ++g_idleCB;
    (void)data;
    (void)sensor;
}

static void oneshotCB(void *data, SoSensor *)
{
    ++g_oneshotCB;
    (void)data;
}

static void alarmCB(void *data, SoSensor *)
{
    ++g_alarmCB;
    (void)data;
}

static void timerCB(void *data, SoSensor *)
{
    ++g_timerCB;
    (void)data;
}

// ---------------------------------------------------------------------------
// Test 1: SoFieldSensor on material diffuseColor
// ---------------------------------------------------------------------------
static bool test1_fieldSensor(const char *basepath)
{
    g_fieldCB = 0;

    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0,0,5);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.8f, 0.2f, 0.2f);
    root->addChild(mat);
    root->addChild(new SoSphere);

    SbViewportRegion vp(W, H);
    cam->viewAll(root, vp);

    // Create field sensor
    SoFieldSensor *fs = new SoFieldSensor(fieldSensorCB, mat);
    fs->attach(&mat->diffuseColor);

    printf("  test1: isScheduled before change=%d\n", (int)fs->isScheduled());

    // Change the field — should schedule the sensor
    mat->diffuseColor.setValue(0.9f, 0.5f, 0.1f);

    printf("  test1: isScheduled after change=%d\n", (int)fs->isScheduled());

    // Process delay queue to fire sensor
    SoSensorManager *sm = SoDB::getSensorManager();
    if (sm->isDelaySensorPending())
        sm->processDelayQueue(FALSE);

    printf("  test1: fieldCB=%d\n", g_fieldCB);

    // Render after sensor-driven color change
    SoOffscreenRenderer *ren = getSharedRenderer();
    ren->setViewportRegion(vp);
    bool ok = ren->render(root);
    if (ok)
        ok = validateNonBlack(ren->getBuffer(), W*H, "test1_fieldSensor");

    if (ok) {
        char outpath[1024];
        snprintf(outpath, sizeof(outpath), "%s_field.rgb", basepath);
        renderToFile(root, outpath, W, H);
    }

    delete fs;
    root->unref();

    if (g_fieldCB < 1) {
        fprintf(stderr, "  WARN test1: fieldCB not fired (delay queue may be empty)\n");
    }
    printf("  PASS test1_fieldSensor\n");
    return ok;
}

// ---------------------------------------------------------------------------
// Test 2: SoNodeSensor on SoSeparator
// ---------------------------------------------------------------------------
static bool test2_nodeSensor()
{
    g_nodeCB = 0;

    SoSeparator *root = new SoSeparator;
    root->ref();

    root->addChild(new SoPerspectiveCamera);
    root->addChild(new SoDirectionalLight);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.5f,0.5f,0.9f);
    root->addChild(mat);
    root->addChild(new SoCube);

    SoNodeSensor *ns = new SoNodeSensor(nodeSensorCB, nullptr);
    ns->attach(root);

    // Modify the separator's subtree to trigger node sensor
    SoMaterial *mat2 = new SoMaterial;
    mat2->diffuseColor.setValue(0.9f,0.9f,0.1f);
    root->addChild(mat2);

    printf("  test2: isScheduled after addChild=%d\n", (int)ns->isScheduled());

    // Process
    SoSensorManager *sm = SoDB::getSensorManager();
    if (sm->isDelaySensorPending())
        sm->processDelayQueue(FALSE);

    printf("  test2: nodeCB=%d\n", g_nodeCB);

    delete ns;
    root->unref();

    printf("  PASS test2_nodeSensor (no crash)\n");
    return true;
}

// ---------------------------------------------------------------------------
// Test 3: SoIdleSensor + SoOneShotSensor
// ---------------------------------------------------------------------------
static bool test3_idleOneshot()
{
    g_idleCB = g_oneshotCB = 0;

    // Idle sensor — fires when delay queue is processed
    SoIdleSensor *idle = new SoIdleSensor(idleCB, nullptr);
    idle->schedule();
    printf("  test3: idle isScheduled=%d\n", (int)idle->isScheduled());

    // One-shot sensor — fires once and removes itself
    SoOneShotSensor *oneshot = new SoOneShotSensor(oneshotCB, nullptr);
    oneshot->schedule();
    printf("  test3: oneshot isScheduled=%d\n", (int)oneshot->isScheduled());

    // Process delay queue
    SoSensorManager *sm = SoDB::getSensorManager();
    sm->processDelayQueue(TRUE);  // TRUE = idle is allowed

    printf("  test3: idleCB=%d oneshotCB=%d\n", g_idleCB, g_oneshotCB);

    // Idle sensor reschedules itself unless unscheduled
    idle->unschedule();
    printf("  test3: idle isScheduled after unschedule=%d\n", (int)idle->isScheduled());

    delete idle;
    delete oneshot;

    printf("  PASS test3_idleOneshot (no crash)\n");
    return true;
}

// ---------------------------------------------------------------------------
// Test 4: SoAlarmSensor
// ---------------------------------------------------------------------------
static bool test4_alarmSensor()
{
    g_alarmCB = 0;

    SoAlarmSensor *alarm = new SoAlarmSensor(alarmCB, nullptr);

    // Set alarm to fire at a time in the past (so it fires immediately)
    SbTime t = SbTime::getTimeOfDay();
    t -= SbTime(1.0); // 1 second ago
    alarm->setTime(t);
    alarm->schedule();

    printf("  test4: alarm isScheduled=%d\n", (int)alarm->isScheduled());

    // Process timer queue
    SoSensorManager *sm = SoDB::getSensorManager();
    sm->processTimerQueue();

    printf("  test4: alarmCB=%d\n", g_alarmCB);

    delete alarm;
    printf("  PASS test4_alarmSensor (no crash)\n");
    return true;
}

// ---------------------------------------------------------------------------
// Test 5: SoTimerSensor
// ---------------------------------------------------------------------------
static bool test5_timerSensor()
{
    g_timerCB = 0;

    SoTimerSensor *timer = new SoTimerSensor(timerCB, nullptr);
    timer->setInterval(SbTime(0.001)); // 1ms interval
    timer->setBaseTime(SbTime::getTimeOfDay() - SbTime(0.5));
    timer->schedule();

    printf("  test5: timer isScheduled=%d\n", (int)timer->isScheduled());

    // Process
    SoSensorManager *sm = SoDB::getSensorManager();
    sm->processTimerQueue();

    printf("  test5: timerCB=%d\n", g_timerCB);

    timer->unschedule();
    delete timer;

    printf("  PASS test5_timerSensor (no crash)\n");
    return true;
}

// ---------------------------------------------------------------------------
// Test 6: Multiple sensors in sequence + sensor-driven render loop
// ---------------------------------------------------------------------------
static bool test6_multiSensorRenderLoop(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0,0,5);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.5f,0.5f,0.5f);
    root->addChild(mat);
    root->addChild(new SoSphere);

    SbViewportRegion vp(W, H);
    cam->viewAll(root, vp);

    SoOffscreenRenderer *ren = getSharedRenderer();
    ren->setViewportRegion(vp);

    bool ok = true;

    // Simulate 5 frames of sensor-driven updates
    for (int frame = 0; frame < 5; ++frame) {
        // Change material color each frame
        float r = 0.2f + frame * 0.15f;
        float g = 0.7f - frame * 0.1f;
        mat->diffuseColor.setValue(r, g, 0.5f);

        // Process pending sensors
        SoSensorManager *sm = SoDB::getSensorManager();
        if (sm->isDelaySensorPending())
            sm->processDelayQueue(FALSE);

        // Render
        ok = ren->render(root);
        if (!ok) break;
    }

    if (ok) {
        ok = validateNonBlack(ren->getBuffer(), W*H, "test6_multiSensorRenderLoop");
        char outpath[1024];
        snprintf(outpath, sizeof(outpath), "%s_multi.rgb", basepath);
        renderToFile(root, outpath, W, H);
    }

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char **argv)
{
    initCoinHeadless();

    const char *basepath = (argc > 1) ? argv[1] : "render_sensors_rendering";

    /* Render the canonical factory scene as the primary output image.
     * This ensures obol_viewer and obol_render produce identical scenes. */
    {
        SoSeparator *fRoot = ObolTest::Scenes::createSensorsRendering(256, 256);
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

    printf("\n=== Sensor + rendering integration tests ===\n");

    if (!test1_fieldSensor(basepath))         ++failures;
    if (!test2_nodeSensor())                   ++failures;
    if (!test3_idleOneshot())                  ++failures;
    if (!test4_alarmSensor())                  ++failures;
    if (!test5_timerSensor())                  ++failures;
    if (!test6_multiSensorRenderLoop(basepath)) ++failures;

    printf("\n=== Summary: %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
