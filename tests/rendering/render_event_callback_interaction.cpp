/*
 * render_event_callback_interaction.cpp
 * 
 * Tests SoEventCallback with mouse and keyboard events driving visible
 * scene changes, as a user application would implement interactive
 * object manipulation via event callbacks.
 *
 * Tests:
 *   1. Mouse-button event callback toggles object visibility (SoSwitch).
 *   2. Mouse-move callback updates a transform (object follows cursor).
 *   3. Keyboard callback scales an object on Up/Down arrow keys.
 *   4. Multiple callbacks on the same node (both fire for a single event).
 *   5. Event callback with setHandled() prevents propagation to a second node.
 *   6. SoEventCallback::removeEventCallback() – callback no longer fires.
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
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoSwitch.h>
#include <Inventor/nodes/SoEventCallback.h>
#include <Inventor/events/SoMouseButtonEvent.h>
#include <Inventor/events/SoLocation2Event.h>
#include <Inventor/events/SoKeyboardEvent.h>
#include <Inventor/sensors/SoSensorManager.h>

#include <cstdio>
#include <cstring>
#include <cmath>

// ---------------------------------------------------------------------------
// Test 1: mouse button callback toggles SoSwitch visibility
// ---------------------------------------------------------------------------

struct Test1Data {
    SoSwitch *sw;
    int       toggleCount;
};

static void mouseToggleCB(void *ud, SoEventCallback *cb)
{
    const SoMouseButtonEvent *evt =
        static_cast<const SoMouseButtonEvent *>(cb->getEvent());
    if (SoMouseButtonEvent::isButtonPressEvent(evt, SoMouseButtonEvent::BUTTON1)) {
        Test1Data *d = static_cast<Test1Data *>(ud);
        int cur = d->sw->whichChild.getValue();
        d->sw->whichChild.setValue((cur == SO_SWITCH_ALL) ? SO_SWITCH_NONE : SO_SWITCH_ALL);
        ++d->toggleCount;
        cb->setHandled();
    }
}

static bool test1_mouseButtonToggle(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->height.setValue(4.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    Test1Data data;
    data.toggleCount = 0;
    data.sw = new SoSwitch;
    data.sw->whichChild.setValue(SO_SWITCH_ALL);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.2f, 0.8f, 0.3f);
    data.sw->addChild(mat);
    SoSphere *sph = new SoSphere;
    sph->radius = 0.8f;
    data.sw->addChild(sph);

    // Event callback sits above the switch so it receives events first
    SoEventCallback *ecb = new SoEventCallback;
    ecb->addEventCallback(SoMouseButtonEvent::getClassTypeId(), mouseToggleCB, &data);
    root->addChild(ecb);
    root->addChild(data.sw);

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);

    // Render: sphere visible
    char fname[1024];
    snprintf(fname, sizeof(fname), "%s_toggle_visible.rgb", basepath);
    bool r1 = renderToFile(root, fname);

    // Simulate button press → sphere disappears
    simulateMousePress(root, vp, DEFAULT_WIDTH/2, DEFAULT_HEIGHT/2);
    snprintf(fname, sizeof(fname), "%s_toggle_hidden.rgb", basepath);
    bool r2 = renderToFile(root, fname);

    // Simulate another press → sphere reappears
    simulateMousePress(root, vp, DEFAULT_WIDTH/2, DEFAULT_HEIGHT/2);
    snprintf(fname, sizeof(fname), "%s_toggle_visible2.rgb", basepath);
    bool r3 = renderToFile(root, fname);

    bool ok = r1 && r2 && r3 && (data.toggleCount == 2);
    if (!ok) {
        fprintf(stderr, "  FAIL test1: toggleCount=%d (expected 2), r1=%d r2=%d r3=%d\n",
                data.toggleCount, (int)r1, (int)r2, (int)r3);
    } else {
        printf("  PASS test1: mouse button callback toggled visibility %d times\n",
               data.toggleCount);
    }

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 2: mouse-move callback updates a SoTransform (tracker pattern)
// ---------------------------------------------------------------------------

struct Test2Data {
    SoTransform *xf;
    int          moveCount;
};

static void mouseMoveCB(void *ud, SoEventCallback *cb)
{
    const SoLocation2Event *evt =
        static_cast<const SoLocation2Event *>(cb->getEvent());
    SbVec2s pos = evt->getPosition();
    Test2Data *d = static_cast<Test2Data *>(ud);
    // Map viewport [0..800] → world [-2..2]
    float wx = (pos[0] / (float)DEFAULT_WIDTH  - 0.5f) * 4.0f;
    float wy = (pos[1] / (float)DEFAULT_HEIGHT - 0.5f) * 4.0f;
    d->xf->translation.setValue(wx, wy, 0.0f);
    ++d->moveCount;
}

static bool test2_mouseMoveTracker(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->height.setValue(6.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    Test2Data data;
    data.moveCount = 0;
    data.xf = new SoTransform;

    SoEventCallback *ecb = new SoEventCallback;
    ecb->addEventCallback(SoLocation2Event::getClassTypeId(), mouseMoveCB, &data);
    root->addChild(ecb);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.9f, 0.5f, 0.1f);
    root->addChild(mat);
    root->addChild(data.xf);
    SoCube *cube = new SoCube;
    cube->width = cube->height = cube->depth = 0.6f;
    root->addChild(cube);

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);

    // Move to a few positions and render
    char fname[1024];
    int cx = DEFAULT_WIDTH/2, cy = DEFAULT_HEIGHT/2;
    int positions[][2] = { {cx, cy}, {cx+100, cy+80}, {cx-80, cy+60}, {cx-50, cy-70} };
    const char *labels[] = { "center", "upperright", "upperleft", "lowerleft" };
    bool allOk = true;

    for (int i = 0; i < 4; ++i) {
        simulateMouseMotion(root, vp, positions[i][0], positions[i][1]);
        SoDB::getSensorManager()->processDelayQueue(TRUE);
        snprintf(fname, sizeof(fname), "%s_tracker_%s.rgb", basepath, labels[i]);
        if (!renderToFile(root, fname)) {
            allOk = false;
            fprintf(stderr, "  FAIL test2: render at position %s\n", labels[i]);
        }
    }

    bool ok = allOk && (data.moveCount >= 4);
    if (!ok) {
        fprintf(stderr, "  FAIL test2: moveCount=%d (expected >=4)\n", data.moveCount);
    } else {
        printf("  PASS test2: mouse-move callback updated transform %d times\n",
               data.moveCount);
    }

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 3: keyboard callback scales object on Up/Down arrow
// ---------------------------------------------------------------------------

struct Test3Data {
    SoTransform *xf;
    int          upCount;
    int          downCount;
};

static void keyCB(void *ud, SoEventCallback *cb)
{
    const SoEvent *evt = cb->getEvent();
    Test3Data *d = static_cast<Test3Data *>(ud);
    if (SO_KEY_PRESS_EVENT(evt, UP_ARROW)) {
        SbVec3f s = d->xf->scaleFactor.getValue();
        d->xf->scaleFactor.setValue(s * 1.2f);
        ++d->upCount;
        cb->setHandled();
    } else if (SO_KEY_PRESS_EVENT(evt, DOWN_ARROW)) {
        SbVec3f s = d->xf->scaleFactor.getValue();
        d->xf->scaleFactor.setValue(s / 1.2f);
        ++d->downCount;
        cb->setHandled();
    }
}

static bool test3_keyboardScale(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->height.setValue(4.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    Test3Data data;
    data.upCount = data.downCount = 0;
    data.xf = new SoTransform;
    data.xf->scaleFactor.setValue(1.0f, 1.0f, 1.0f);

    SoEventCallback *ecb = new SoEventCallback;
    ecb->addEventCallback(SoKeyboardEvent::getClassTypeId(), keyCB, &data);
    root->addChild(ecb);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.3f, 0.5f, 0.9f);
    root->addChild(mat);
    root->addChild(data.xf);
    root->addChild(new SoSphere);

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);

    char fname[1024];
    snprintf(fname, sizeof(fname), "%s_key_initial.rgb", basepath);
    bool r0 = renderToFile(root, fname);

    // Press Up 3 times
    for (int i = 0; i < 3; ++i) {
        simulateKeyPress(root, vp, SoKeyboardEvent::UP_ARROW);
        simulateKeyRelease(root, vp, SoKeyboardEvent::UP_ARROW);
        SoDB::getSensorManager()->processDelayQueue(TRUE);
    }
    snprintf(fname, sizeof(fname), "%s_key_scaledup.rgb", basepath);
    bool r1 = renderToFile(root, fname);

    // Press Down 5 times
    for (int i = 0; i < 5; ++i) {
        simulateKeyPress(root, vp, SoKeyboardEvent::DOWN_ARROW);
        simulateKeyRelease(root, vp, SoKeyboardEvent::DOWN_ARROW);
        SoDB::getSensorManager()->processDelayQueue(TRUE);
    }
    snprintf(fname, sizeof(fname), "%s_key_scaleddown.rgb", basepath);
    bool r2 = renderToFile(root, fname);

    // Verify scale changed correctly
    SbVec3f finalScale = data.xf->scaleFactor.getValue();
    // initial * 1.2^3 / 1.2^5 = 1.2^-2 ≈ 0.694
    float expectedScale = (float)pow(1.2, 3 - 5);
    bool scaleOk = (fabsf(finalScale[0] - expectedScale) < 0.01f);

    bool ok = r0 && r1 && r2
              && (data.upCount == 3) && (data.downCount == 5)
              && scaleOk;
    if (!ok) {
        fprintf(stderr,
                "  FAIL test3: up=%d down=%d scale=(%.3f,%.3f,%.3f) expected~%.3f\n",
                data.upCount, data.downCount,
                finalScale[0], finalScale[1], finalScale[2], expectedScale);
    } else {
        printf("  PASS test3: keyboard scale: up=%d down=%d final=%.3f\n",
               data.upCount, data.downCount, finalScale[0]);
    }

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 4: two callbacks on the same event node both fire
// ---------------------------------------------------------------------------

static int g_cb4a_count = 0;
static int g_cb4b_count = 0;

static void cb4a(void *, SoEventCallback *cb)
{
    const SoMouseButtonEvent *evt =
        static_cast<const SoMouseButtonEvent *>(cb->getEvent());
    if (SoMouseButtonEvent::isButtonPressEvent(evt, SoMouseButtonEvent::BUTTON1))
        ++g_cb4a_count;
}

static void cb4b(void *, SoEventCallback *cb)
{
    const SoMouseButtonEvent *evt =
        static_cast<const SoMouseButtonEvent *>(cb->getEvent());
    if (SoMouseButtonEvent::isButtonPressEvent(evt, SoMouseButtonEvent::BUTTON1))
        ++g_cb4b_count;
}

static bool test4_multipleCallbacks()
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->height.setValue(4.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);
    root->addChild(new SoSphere);

    SoEventCallback *ecb = new SoEventCallback;
    ecb->addEventCallback(SoMouseButtonEvent::getClassTypeId(), cb4a, nullptr);
    ecb->addEventCallback(SoMouseButtonEvent::getClassTypeId(), cb4b, nullptr);
    root->addChild(ecb);

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);

    g_cb4a_count = g_cb4b_count = 0;
    simulateMousePress(root, vp, DEFAULT_WIDTH/2, DEFAULT_HEIGHT/2);
    simulateMousePress(root, vp, DEFAULT_WIDTH/2, DEFAULT_HEIGHT/2);

    bool ok = (g_cb4a_count == 2) && (g_cb4b_count == 2);
    if (!ok) {
        fprintf(stderr, "  FAIL test4: cb4a=%d cb4b=%d (expected both 2)\n",
                g_cb4a_count, g_cb4b_count);
    } else {
        printf("  PASS test4: both callbacks fired %d times each\n", g_cb4a_count);
    }

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 5: setHandled() blocks event from second callback node in scene
// ---------------------------------------------------------------------------

static int g_cb5_counter = 0;

static void cb5Handler(void *, SoEventCallback *cb)
{
    const SoKeyboardEvent *evt =
        static_cast<const SoKeyboardEvent *>(cb->getEvent());
    if (evt->getState() == SoButtonEvent::DOWN)
        ++g_cb5_counter;
    cb->setHandled(); // consume so second ecb never sees it
}

static void cb5SecondHandler(void *, SoEventCallback *cb)
{
    // This must NOT be called because the first callback consumed the event
    const SoKeyboardEvent *evt =
        static_cast<const SoKeyboardEvent *>(cb->getEvent());
    if (evt->getState() == SoButtonEvent::DOWN)
        g_cb5_counter += 100; // clearly wrong if called
}

static bool test5_setHandledBlocksPropagation()
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->height.setValue(4.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);
    root->addChild(new SoSphere);

    // First ecb: consumes the event
    SoEventCallback *ecb1 = new SoEventCallback;
    ecb1->addEventCallback(SoKeyboardEvent::getClassTypeId(), cb5Handler, nullptr);
    root->addChild(ecb1);

    // Second ecb: must not fire because first consumed the event
    SoEventCallback *ecb2 = new SoEventCallback;
    ecb2->addEventCallback(SoKeyboardEvent::getClassTypeId(), cb5SecondHandler, nullptr);
    root->addChild(ecb2);

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);

    g_cb5_counter = 0;
    simulateKeyPress(root, vp, SoKeyboardEvent::UP_ARROW);
    simulateKeyPress(root, vp, SoKeyboardEvent::DOWN_ARROW);

    // Each key press increments by 1 from cb5Handler; second handler adds 100 if called.
    bool ok = (g_cb5_counter == 2);
    if (!ok) {
        fprintf(stderr, "  FAIL test5: counter=%d (expected 2, second handler fired if >10)\n",
                g_cb5_counter);
    } else {
        printf("  PASS test5: setHandled() blocked propagation (counter=%d)\n",
               g_cb5_counter);
    }

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 6: removeEventCallback() stops callback from firing
// ---------------------------------------------------------------------------

static int g_cb6_count = 0;

static void cb6(void *, SoEventCallback *cb)
{
    const SoMouseButtonEvent *evt =
        static_cast<const SoMouseButtonEvent *>(cb->getEvent());
    if (SoMouseButtonEvent::isButtonPressEvent(evt, SoMouseButtonEvent::BUTTON1))
        ++g_cb6_count;
}

static bool test6_removeCallback()
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->height.setValue(4.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);
    root->addChild(new SoSphere);

    SoEventCallback *ecb = new SoEventCallback;
    ecb->addEventCallback(SoMouseButtonEvent::getClassTypeId(), cb6, nullptr);
    root->addChild(ecb);

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);

    g_cb6_count = 0;
    simulateMousePress(root, vp, DEFAULT_WIDTH/2, DEFAULT_HEIGHT/2);
    // Should have fired once
    int afterFirst = g_cb6_count;

    // Remove the callback
    ecb->removeEventCallback(SoMouseButtonEvent::getClassTypeId(), cb6, nullptr);
    simulateMousePress(root, vp, DEFAULT_WIDTH/2, DEFAULT_HEIGHT/2);
    simulateMousePress(root, vp, DEFAULT_WIDTH/2, DEFAULT_HEIGHT/2);
    // Counter must not have changed
    int afterRemove = g_cb6_count;

    bool ok = (afterFirst == 1) && (afterRemove == 1);
    if (!ok) {
        fprintf(stderr, "  FAIL test6: afterFirst=%d afterRemove=%d (expected 1,1)\n",
                afterFirst, afterRemove);
    } else {
        printf("  PASS test6: removeEventCallback() stopped firing (count stayed at %d)\n",
               g_cb6_count);
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

    const char *basepath = (argc > 1) ? argv[1] : "render_event_callback_interaction";

    /* Render the canonical factory scene as the primary output image.
     * This ensures obol_viewer and obol_render produce identical scenes. */
    {
        SoSeparator *fRoot = ObolTest::Scenes::createEventCallbackInteraction(256, 256);
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

    printf("\n=== SoEventCallback interaction tests ===\n");

    if (!test1_mouseButtonToggle(basepath)) ++failures;
    if (!test2_mouseMoveTracker(basepath))  ++failures;
    if (!test3_keyboardScale(basepath))     ++failures;
    if (!test4_multipleCallbacks())         ++failures;
    if (!test5_setHandledBlocksPropagation()) ++failures;
    if (!test6_removeCallback())            ++failures;

    printf("\n=== Summary: %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
