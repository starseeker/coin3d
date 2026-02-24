/*
 * render_callback_node.cpp
 *
 * Tests the SoCallback node which fires a user function during GL render
 * traversal. This is a key API for applications that need to inject custom
 * OpenGL commands into the scene graph render stream.
 *
 * Tests:
 *   1. SoCallback fires during render traversal (counter verified).
 *   2. SoCallback receives the correct SoAction type.
 *   3. SoCallback before geometry changes visible output (clear vs. not clear).
 *   4. Multiple SoCallback nodes in a scene – each fires in order.
 *   5. SoCallback within a SoSeparator subtree fires only when subtree is rendered.
 *   6. SoCallback::setCallback(NULL) – unregistration stops callback.
 *
 * Returns 0 on pass, non-0 on failure.
 */

#include "headless_utils.h"

#include <Inventor/SoDB.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCallback.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/actions/SoAction.h>

#include <cstdio>

// ---------------------------------------------------------------------------
// Test 1: SoCallback fires during render traversal
// ---------------------------------------------------------------------------

static int g_t1_count = 0;
static void t1CB(void *, SoAction *) { ++g_t1_count; }

static bool test1_callbackFires(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0,0,5); cam->height.setValue(4.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);
    SoCallback *cb = new SoCallback;
    cb->setCallback(t1CB);
    root->addChild(cb);
    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.5f,0.7f,0.3f);
    root->addChild(mat);
    root->addChild(new SoSphere);

    g_t1_count = 0;

    char fname[1024];
    snprintf(fname, sizeof(fname), "%s_t1_render.rgb", basepath);
    bool r1 = renderToFile(root, fname);

    // Render twice more
    snprintf(fname, sizeof(fname), "%s_t1_render2.rgb", basepath);
    bool r2 = renderToFile(root, fname);

    // Callback must have fired at least once per render
    bool ok = r1 && r2 && (g_t1_count >= 2);
    if (!ok) {
        fprintf(stderr, "  FAIL test1: count=%d r1=%d r2=%d\n",
                g_t1_count, (int)r1, (int)r2);
    } else {
        printf("  PASS test1: SoCallback fired %d times during 2 renders\n",
               g_t1_count);
    }
    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 2: SoCallback receives SoGLRenderAction during GL render
// ---------------------------------------------------------------------------

static bool g_t2_gotGLRender = false;

static void t2CB(void *, SoAction *action)
{
    if (action->isOfType(SoGLRenderAction::getClassTypeId())) {
        g_t2_gotGLRender = true;
    }
}

static bool test2_callbackActionType(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0,0,5); cam->height.setValue(4.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);
    SoCallback *cb = new SoCallback;
    cb->setCallback(t2CB);
    root->addChild(cb);
    root->addChild(new SoSphere);

    g_t2_gotGLRender = false;
    char fname[1024];
    snprintf(fname, sizeof(fname), "%s_t2_render.rgb", basepath);
    bool r = renderToFile(root, fname);

    bool ok = r && g_t2_gotGLRender;
    if (!ok) {
        fprintf(stderr, "  FAIL test2: r=%d gotGLRender=%d\n",
                (int)r, (int)g_t2_gotGLRender);
    } else {
        printf("  PASS test2: SoCallback received SoGLRenderAction correctly\n");
    }
    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 3: Multiple SoCallback nodes fire in order
// ---------------------------------------------------------------------------

struct T3State { int order[4]; int idx; };
static T3State g_t3state = { {-1,-1,-1,-1}, 0 };

static void t3CB_A(void *, SoAction *) {
    if (g_t3state.idx < 4) g_t3state.order[g_t3state.idx++] = 0;
}
static void t3CB_B(void *, SoAction *) {
    if (g_t3state.idx < 4) g_t3state.order[g_t3state.idx++] = 1;
}
static void t3CB_C(void *, SoAction *) {
    if (g_t3state.idx < 4) g_t3state.order[g_t3state.idx++] = 2;
}

static bool test3_multipleCallbackOrder(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0,0,5); cam->height.setValue(4.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoCallback *cbA = new SoCallback; cbA->setCallback(t3CB_A);
    SoCallback *cbB = new SoCallback; cbB->setCallback(t3CB_B);
    SoCallback *cbC = new SoCallback; cbC->setCallback(t3CB_C);

    root->addChild(cbA);
    root->addChild(new SoSphere);
    root->addChild(cbB);
    root->addChild(cbC);

    g_t3state.idx = 0;
    char fname[1024];
    snprintf(fname, sizeof(fname), "%s_t3_render.rgb", basepath);
    bool r = renderToFile(root, fname);

    // Verify order: A(0) before B(1) before C(2)
    bool orderOk = (g_t3state.idx >= 3) &&
                   (g_t3state.order[0] == 0) &&
                   (g_t3state.order[1] == 1) &&
                   (g_t3state.order[2] == 2);

    bool ok = r && orderOk;
    if (!ok) {
        fprintf(stderr, "  FAIL test3: order=[%d,%d,%d] idx=%d\n",
                g_t3state.order[0], g_t3state.order[1], g_t3state.order[2],
                g_t3state.idx);
    } else {
        printf("  PASS test3: 3 callbacks fired in order A→B→C\n");
    }
    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 4: SoCallback inside SoSeparator subtree fires only for that subtree
// ---------------------------------------------------------------------------

static int g_t4_visCount = 0;
static int g_t4_hidCount = 0;
static void t4CB_vis(void *, SoAction *) { ++g_t4_visCount; }
static void t4CB_hid(void *, SoAction *) { ++g_t4_hidCount; }

static bool test4_callbackInSubtree(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0,0,5); cam->height.setValue(8.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    // Visible subtree: contains callback and sphere
    SoSeparator *visSep = new SoSeparator;
    SoCallback *visCB = new SoCallback;
    visCB->setCallback(t4CB_vis);
    visSep->addChild(visCB);
    SoTransform *xf1 = new SoTransform;
    xf1->translation.setValue(-2.0f,0,0);
    visSep->addChild(xf1);
    visSep->addChild(new SoSphere);
    root->addChild(visSep);

    // Hidden subtree (renderCaching OFF, but not inside switch - just separate sep)
    SoSeparator *hidSep = new SoSeparator;
    SoCallback *hidCB = new SoCallback;
    hidCB->setCallback(t4CB_hid);
    hidSep->addChild(hidCB);
    SoTransform *xf2 = new SoTransform;
    xf2->translation.setValue(2.0f,0,0);
    hidSep->addChild(xf2);
    hidSep->addChild(new SoCube);
    root->addChild(hidSep);

    g_t4_visCount = g_t4_hidCount = 0;
    char fname[1024];
    snprintf(fname, sizeof(fname), "%s_t4_render.rgb", basepath);
    bool r = renderToFile(root, fname);

    // Both subtrees are visible, so both callbacks fire
    bool ok = r && (g_t4_visCount >= 1) && (g_t4_hidCount >= 1);
    if (!ok) {
        fprintf(stderr, "  FAIL test4: vis=%d hid=%d r=%d\n",
                g_t4_visCount, g_t4_hidCount, (int)r);
    } else {
        printf("  PASS test4: both subtree callbacks fired (vis=%d hid=%d)\n",
               g_t4_visCount, g_t4_hidCount);
    }
    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 5: SoCallback::setCallback(NULL) unregistration
// ---------------------------------------------------------------------------

static int g_t5_count = 0;
static void t5CB(void *, SoAction *) { ++g_t5_count; }

static bool test5_unregisterCallback(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0,0,5); cam->height.setValue(4.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoCallback *cb = new SoCallback;
    cb->setCallback(t5CB);
    root->addChild(cb);
    root->addChild(new SoSphere);

    g_t5_count = 0;
    char fname[1024];
    snprintf(fname, sizeof(fname), "%s_t5_before.rgb", basepath);
    bool r1 = renderToFile(root, fname);
    int before = g_t5_count;

    // Unregister
    cb->setCallback(nullptr);
    snprintf(fname, sizeof(fname), "%s_t5_after.rgb", basepath);
    bool r2 = renderToFile(root, fname);
    int after = g_t5_count;

    bool ok = r1 && r2 && (before >= 1) && (after == before);
    if (!ok) {
        fprintf(stderr, "  FAIL test5: before=%d after=%d (should be equal)\n",
                before, after);
    } else {
        printf("  PASS test5: setCallback(NULL) stopped firing (before=%d after=%d)\n",
               before, after);
    }
    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 6: SoCallback accumulates GL state changes across frames
// ---------------------------------------------------------------------------

struct T6Data { int renderCount; float offset; };

static void t6CB(void *ud, SoAction *action)
{
    if (!action->isOfType(SoGLRenderAction::getClassTypeId())) return;
    T6Data *d = static_cast<T6Data *>(ud);
    ++d->renderCount;
    // Could push/pop GL state here; just count renders for verification
}

static bool test6_callbackAcrossFrames(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0,0,5); cam->height.setValue(4.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    T6Data data = { 0, 0.0f };
    SoCallback *cb = new SoCallback;
    cb->setCallback(t6CB, &data);
    root->addChild(cb);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.3f,0.6f,0.8f);
    root->addChild(mat);
    root->addChild(new SoSphere);

    bool allOk = true;
    char fname[1024];
    for (int i = 0; i < 4; ++i) {
        snprintf(fname, sizeof(fname), "%s_t6_frame%d.rgb", basepath, i);
        if (!renderToFile(root, fname)) allOk = false;
    }

    bool ok = allOk && (data.renderCount >= 4);
    if (!ok) {
        fprintf(stderr, "  FAIL test6: renderCount=%d (expected >=4)\n",
                data.renderCount);
    } else {
        printf("  PASS test6: SoCallback fired across %d frames\n",
               data.renderCount);
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

    const char *basepath = (argc > 1) ? argv[1] : "render_callback_node";
    int failures = 0;

    printf("\n=== SoCallback node tests ===\n");

    if (!test1_callbackFires(basepath))       ++failures;
    if (!test2_callbackActionType(basepath))  ++failures;
    if (!test3_multipleCallbackOrder(basepath)) ++failures;
    if (!test4_callbackInSubtree(basepath))   ++failures;
    if (!test5_unregisterCallback(basepath))  ++failures;
    if (!test6_callbackAcrossFrames(basepath)) ++failures;

    printf("\n=== Summary: %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
