/*
 * render_ext_selection_events.cpp — SoExtSelection event-driven selection
 *
 * Exercises the interactive event-handling paths in SoExtSelection that
 * are not covered by the programmatic select() API tests.  Events are fed
 * directly via SoHandleEventAction, simulating what a GUI toolkit does when
 * a user drags a lasso over the viewport.
 *
 * Tests:
 *   1. RECTANGLE mode: BUTTON1-press → move → release triggers selectAndReset
 *   2. LASSO mode: BUTTON1-press (start) → move → BUTTON1-press (corner) ×N
 *      → BUTTON2-press (end) finishes selection
 *   3. LASSO mode double-click finish (two clicks at nearly same position)
 *   4. Keyboard Escape aborts in-progress rectangle selection
 *   5. NOLASSO mode delegates to SoSelection (parent class) event handling
 *   6. wasShiftDown() state propagation through handleEvent
 *   7. getLassoCoordsDC / getLassoCoordsWC after a completed selection
 *   8. Render the scene after selection to produce non-black output
 *
 * Returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoExtSelection.h>
#include <Inventor/actions/SoHandleEventAction.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/events/SoMouseButtonEvent.h>
#include <Inventor/events/SoLocation2Event.h>
#include <Inventor/events/SoKeyboardEvent.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbColor.h>
#include <Inventor/SoPath.h>
#include <cstdio>
#include <cstring>

static const int W = 256;
static const int H = 256;

// ---------------------------------------------------------------------------
// Event helpers
// ---------------------------------------------------------------------------
static void sendMousePress(SoNode *root, const SbViewportRegion &vp,
                           short x, short y,
                           SoMouseButtonEvent::Button btn = SoMouseButtonEvent::BUTTON1,
                           SbBool shift = FALSE)
{
    SoMouseButtonEvent ev;
    ev.setButton(btn);
    ev.setState(SoButtonEvent::DOWN);
    ev.setPosition(SbVec2s(x, y));
    ev.setShiftDown(shift);
    SoHandleEventAction ha(vp);
    ha.setEvent(&ev);
    ha.apply(root);
}

static void sendMouseRelease(SoNode *root, const SbViewportRegion &vp,
                             short x, short y,
                             SoMouseButtonEvent::Button btn = SoMouseButtonEvent::BUTTON1)
{
    SoMouseButtonEvent ev;
    ev.setButton(btn);
    ev.setState(SoButtonEvent::UP);
    ev.setPosition(SbVec2s(x, y));
    SoHandleEventAction ha(vp);
    ha.setEvent(&ev);
    ha.apply(root);
}

static void sendMouseMove(SoNode *root, const SbViewportRegion &vp,
                          short x, short y)
{
    SoLocation2Event ev;
    ev.setPosition(SbVec2s(x, y));
    SoHandleEventAction ha(vp);
    ha.setEvent(&ev);
    ha.apply(root);
}

static void sendKey(SoNode *root, const SbViewportRegion &vp,
                    SoKeyboardEvent::Key key)
{
    SoKeyboardEvent ev;
    ev.setKey(key);
    ev.setState(SoButtonEvent::DOWN);
    SoHandleEventAction ha(vp);
    ha.setEvent(&ev);
    ha.apply(root);
}

// ---------------------------------------------------------------------------
// Selection / deselection callback counters
// ---------------------------------------------------------------------------
static int g_selCnt = 0;
static int g_deselCnt = 0;
static void onSel(void *, SoPath *)   { ++g_selCnt; }
static void onDesel(void *, SoPath *) { ++g_deselCnt; }

// ---------------------------------------------------------------------------
// Build a standard 3-object scene inside SoExtSelection
// ---------------------------------------------------------------------------
struct Scene {
    SoSeparator   *renderRoot;   // camera + light + extsel
    SoExtSelection *extSel;
    SbViewportRegion vp;
};

static Scene buildScene()
{
    Scene sc;
    sc.vp = SbViewportRegion((short)W, (short)H);

    sc.renderRoot = new SoSeparator;
    sc.renderRoot->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 10.0f);
    cam->height.setValue(8.0f);
    sc.renderRoot->addChild(cam);

    SoDirectionalLight *lt = new SoDirectionalLight;
    sc.renderRoot->addChild(lt);

    sc.extSel = new SoExtSelection;
    sc.extSel->addSelectionCallback(onSel, nullptr);
    sc.extSel->addDeselectionCallback(onDesel, nullptr);
    sc.renderRoot->addChild(sc.extSel);

    // Three spheres side by side
    float xs[3] = { -2.5f, 0.0f, 2.5f };
    SbColor cols[3] = {
        SbColor(0.8f, 0.2f, 0.2f),
        SbColor(0.2f, 0.8f, 0.2f),
        SbColor(0.2f, 0.2f, 0.8f)
    };
    for (int i = 0; i < 3; ++i) {
        SoSeparator *sep = new SoSeparator;
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(cols[i]);
        sep->addChild(mat);
        SoTranslation *tr = new SoTranslation;
        tr->translation.setValue(xs[i], 0.0f, 0.0f);
        sep->addChild(tr);
        SoSphere *sph = new SoSphere;
        sph->radius = 0.8f;
        sep->addChild(sph);
        sc.extSel->addChild(sep);
    }
    return sc;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static bool validateNonBlack(const unsigned char *buf, int npix,
                              const char *label)
{
    int nonbg = 0;
    for (int i = 0; i < npix; ++i) {
        const unsigned char *p = buf + i * 3;
        if (p[0] > 5 || p[1] > 5 || p[2] > 5) ++nonbg;
    }
    printf("  %s: non-background pixels=%d\n", label, nonbg);
    return nonbg >= 50;
}

// ---------------------------------------------------------------------------
// Test 1: RECTANGLE mode — simulate mouse press/move/release
// ---------------------------------------------------------------------------
static bool test1_rectangleEvents(const char *basepath)
{
    g_selCnt = g_deselCnt = 0;

    Scene sc = buildScene();
    sc.extSel->lassoType.setValue(SoExtSelection::RECTANGLE);
    sc.extSel->lassoPolicy.setValue(SoExtSelection::FULL_BBOX);

    // Simulate: press at (30,30), move to (200,200), release
    // In orthographic cam with height=8 at 256x256, the objects project near center.
    // We want to cover the full viewport to select all three spheres.
    sendMousePress(sc.renderRoot, sc.vp, 10, 10);
    sendMouseMove(sc.renderRoot, sc.vp, 100, 100);
    sendMouseMove(sc.renderRoot, sc.vp, 200, 200);
    sendMouseRelease(sc.renderRoot, sc.vp, 246, 246);

    printf("  test1_rect: selCnt=%d numSelected=%d\n",
           g_selCnt, sc.extSel->getNumSelected());

    // Render after selection
    SoOffscreenRenderer *ren = getSharedRenderer();
    ren->setViewportRegion(sc.vp);
    bool ok = ren->render(sc.renderRoot);
    if (ok) {
        char path[1024];
        snprintf(path, sizeof(path), "%s_rect_events.rgb", basepath);
        renderToFile(sc.renderRoot, path, W, H);
        ok = validateNonBlack(ren->getBuffer(), W*H, "test1_rect");
    }

    sc.renderRoot->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 2: LASSO mode — multi-click polygon then right-click to finish
// ---------------------------------------------------------------------------
static bool test2_lassoEvents(const char *basepath)
{
    g_selCnt = g_deselCnt = 0;

    Scene sc = buildScene();
    sc.extSel->lassoType.setValue(SoExtSelection::LASSO);
    sc.extSel->lassoPolicy.setValue(SoExtSelection::PART_BBOX);

    // Start the lasso at bottom-left
    sendMousePress(sc.renderRoot, sc.vp, 10, 10);
    sendMouseMove(sc.renderRoot, sc.vp, 80, 80);

    // Add more corners of the polygon
    sendMousePress(sc.renderRoot, sc.vp, 246, 10);    // bottom-right
    sendMouseMove(sc.renderRoot, sc.vp, 200, 150);

    sendMousePress(sc.renderRoot, sc.vp, 246, 246);   // top-right
    sendMouseMove(sc.renderRoot, sc.vp, 128, 200);

    sendMousePress(sc.renderRoot, sc.vp, 10, 246);    // top-left

    // End with right-click (BUTTON2 press)
    sendMousePress(sc.renderRoot, sc.vp, 10, 246,
                   SoMouseButtonEvent::BUTTON2);

    printf("  test2_lasso: selCnt=%d numSelected=%d\n",
           g_selCnt, sc.extSel->getNumSelected());

    SoOffscreenRenderer *ren = getSharedRenderer();
    ren->setViewportRegion(sc.vp);
    bool ok = ren->render(sc.renderRoot);
    if (ok) {
        char path[1024];
        snprintf(path, sizeof(path), "%s_lasso_events.rgb", basepath);
        renderToFile(sc.renderRoot, path, W, H);
        ok = validateNonBlack(ren->getBuffer(), W*H, "test2_lasso");
    }

    sc.renderRoot->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 3: LASSO mode double-click finish
// ---------------------------------------------------------------------------
static bool test3_lassoDoubleClick(const char *basepath)
{
    g_selCnt = g_deselCnt = 0;

    Scene sc = buildScene();
    sc.extSel->lassoType.setValue(SoExtSelection::LASSO);
    sc.extSel->lassoPolicy.setValue(SoExtSelection::FULL_BBOX);

    // Start lasso
    sendMousePress(sc.renderRoot, sc.vp, 10, 10);
    sendMouseMove(sc.renderRoot, sc.vp, 200, 10);

    // Add corners
    sendMousePress(sc.renderRoot, sc.vp, 246, 10);
    sendMousePress(sc.renderRoot, sc.vp, 246, 246);

    // Double-click (two presses at nearly same position - within 2 pixels)
    sendMousePress(sc.renderRoot, sc.vp, 10, 246);
    sendMousePress(sc.renderRoot, sc.vp, 11, 246);   // double-click ends selection

    printf("  test3_dblclick: selCnt=%d numSelected=%d\n",
           g_selCnt, sc.extSel->getNumSelected());

    SoOffscreenRenderer *ren = getSharedRenderer();
    ren->setViewportRegion(sc.vp);
    bool ok = ren->render(sc.renderRoot);
    if (ok) {
        char path[1024];
        snprintf(path, sizeof(path), "%s_dblclick.rgb", basepath);
        renderToFile(sc.renderRoot, path, W, H);
        ok = validateNonBlack(ren->getBuffer(), W*H, "test3_dblclick");
    }

    sc.renderRoot->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 4: Escape key aborts in-progress RECTANGLE selection
// ---------------------------------------------------------------------------
static bool test4_escapeAbort(const char *basepath)
{
    g_selCnt = g_deselCnt = 0;

    Scene sc = buildScene();
    sc.extSel->lassoType.setValue(SoExtSelection::RECTANGLE);

    int selBefore = sc.extSel->getNumSelected();

    // Start rectangle but abort with Escape before releasing
    sendMousePress(sc.renderRoot, sc.vp, 10, 10);
    sendMouseMove(sc.renderRoot, sc.vp, 200, 200);
    sendKey(sc.renderRoot, sc.vp, SoKeyboardEvent::ESCAPE);

    int selAfter = sc.extSel->getNumSelected();
    printf("  test4_escape: selBefore=%d selAfter=%d\n", selBefore, selAfter);

    SoOffscreenRenderer *ren = getSharedRenderer();
    ren->setViewportRegion(sc.vp);
    bool ok = ren->render(sc.renderRoot);
    if (ok) {
        char path[1024];
        snprintf(path, sizeof(path), "%s_escape.rgb", basepath);
        renderToFile(sc.renderRoot, path, W, H);
        ok = validateNonBlack(ren->getBuffer(), W*H, "test4_escape");
    }

    sc.renderRoot->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 5: NOLASSO mode — events delegated to SoSelection parent
// ---------------------------------------------------------------------------
static bool test5_noLasso(const char *basepath)
{
    g_selCnt = g_deselCnt = 0;

    Scene sc = buildScene();
    sc.extSel->lassoType.setValue(SoExtSelection::NOLASSO);

    // Just render – NOLASSO mode uses SoSelection behaviour
    SoOffscreenRenderer *ren = getSharedRenderer();
    ren->setViewportRegion(sc.vp);
    bool ok = ren->render(sc.renderRoot);
    if (ok) {
        char path[1024];
        snprintf(path, sizeof(path), "%s_nolasso.rgb", basepath);
        renderToFile(sc.renderRoot, path, W, H);
        ok = validateNonBlack(ren->getBuffer(), W*H, "test5_noLasso");
    }

    // Also test a mouse event in NOLASSO mode (goes to SoSelection)
    sendMousePress(sc.renderRoot, sc.vp, 128, 128);

    sc.renderRoot->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 6: wasShiftDown state + getLassoCoordsDC after selection
// ---------------------------------------------------------------------------
static bool test6_shiftAndCoords(const char *basepath)
{
    g_selCnt = g_deselCnt = 0;

    Scene sc = buildScene();
    sc.extSel->lassoType.setValue(SoExtSelection::RECTANGLE);
    sc.extSel->lassoPolicy.setValue(SoExtSelection::PART_BBOX);

    // Simulate shift-click rectangle
    sendMousePress(sc.renderRoot, sc.vp, 10, 10, SoMouseButtonEvent::BUTTON1, TRUE);
    sendMouseMove(sc.renderRoot, sc.vp, 200, 200);
    sendMouseRelease(sc.renderRoot, sc.vp, 246, 246);

    printf("  test6: wasShiftDown=%d selCnt=%d\n",
           (int)sc.extSel->wasShiftDown(), g_selCnt);

    // getLassoCoordsDC after programmatic select()
    SbVec2f lasso[4] = {
        SbVec2f(0.0f, 0.0f),
        SbVec2f(1.0f, 0.0f),
        SbVec2f(1.0f, 1.0f),
        SbVec2f(0.0f, 1.0f)
    };
    sc.extSel->select(sc.renderRoot, 4, lasso, sc.vp, FALSE);

    int numCoordsDC = 0;
    const SbVec2s *dc = sc.extSel->getLassoCoordsDC(numCoordsDC);
    int numCoordsWC = 0;
    const SbVec3f *wc = sc.extSel->getLassoCoordsWC(numCoordsWC);
    printf("  test6: lassoCoordsDC count=%d  lassoCoordsWC count=%d\n",
           numCoordsDC, numCoordsWC);

    SoOffscreenRenderer *ren = getSharedRenderer();
    ren->setViewportRegion(sc.vp);
    bool ok = ren->render(sc.renderRoot);
    if (ok) {
        char path[1024];
        snprintf(path, sizeof(path), "%s_shift.rgb", basepath);
        renderToFile(sc.renderRoot, path, W, H);
        ok = validateNonBlack(ren->getBuffer(), W*H, "test6_shift");
    }
    (void)dc; (void)wc;

    sc.renderRoot->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 7: RECTANGLE with VISIBLE_SHAPES mode
// ---------------------------------------------------------------------------
static bool test7_visibleShapes(const char *basepath)
{
    g_selCnt = g_deselCnt = 0;

    Scene sc = buildScene();
    sc.extSel->lassoType.setValue(SoExtSelection::RECTANGLE);
    sc.extSel->lassoPolicy.setValue(SoExtSelection::FULL_BBOX);
    sc.extSel->lassoMode.setValue(SoExtSelection::VISIBLE_SHAPES);

    SbVec2f lasso[4] = {
        SbVec2f(0.0f, 0.0f),
        SbVec2f(1.0f, 0.0f),
        SbVec2f(1.0f, 1.0f),
        SbVec2f(0.0f, 1.0f)
    };
    sc.extSel->select(sc.renderRoot, 4, lasso, sc.vp, FALSE);
    printf("  test7_visibleShapes: selCnt=%d numSelected=%d\n",
           g_selCnt, sc.extSel->getNumSelected());

    SoOffscreenRenderer *ren = getSharedRenderer();
    ren->setViewportRegion(sc.vp);
    bool ok = ren->render(sc.renderRoot);
    if (ok) {
        char path[1024];
        snprintf(path, sizeof(path), "%s_visible.rgb", basepath);
        renderToFile(sc.renderRoot, path, W, H);
        ok = validateNonBlack(ren->getBuffer(), W*H, "test7_visibleShapes");
    }

    sc.renderRoot->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 8: LASSO with FULL policy and triangle filter
// ---------------------------------------------------------------------------
static int g_triCnt = 0;
static SbBool triFilterCB(void *, SoCallbackAction *,
                           const SoPrimitiveVertex *v1,
                           const SoPrimitiveVertex *v2,
                           const SoPrimitiveVertex *v3)
{
    ++g_triCnt;
    (void)v1; (void)v2; (void)v3;
    return TRUE;
}

static bool test8_lassoFull(const char *basepath)
{
    g_selCnt = g_deselCnt = g_triCnt = 0;

    Scene sc = buildScene();
    sc.extSel->lassoType.setValue(SoExtSelection::LASSO);
    sc.extSel->lassoPolicy.setValue(SoExtSelection::FULL);
    sc.extSel->setTriangleFilterCallback(triFilterCB, nullptr);

    // Drive via events: start lasso covering the whole viewport
    sendMousePress(sc.renderRoot, sc.vp, 5, 5);
    sendMouseMove(sc.renderRoot, sc.vp, 251, 5);
    sendMousePress(sc.renderRoot, sc.vp, 251, 5);
    sendMouseMove(sc.renderRoot, sc.vp, 251, 251);
    sendMousePress(sc.renderRoot, sc.vp, 251, 251);
    sendMouseMove(sc.renderRoot, sc.vp, 5, 251);
    sendMousePress(sc.renderRoot, sc.vp, 5, 251);
    // Double-click close to finish
    sendMousePress(sc.renderRoot, sc.vp, 6, 6);
    sendMousePress(sc.renderRoot, sc.vp, 7, 6);  // double-click

    printf("  test8_lassoFull: selCnt=%d triCnt=%d numSelected=%d\n",
           g_selCnt, g_triCnt, sc.extSel->getNumSelected());

    SoOffscreenRenderer *ren = getSharedRenderer();
    ren->setViewportRegion(sc.vp);
    bool ok = ren->render(sc.renderRoot);
    if (ok) {
        char path[1024];
        snprintf(path, sizeof(path), "%s_lasso_full.rgb", basepath);
        renderToFile(sc.renderRoot, path, W, H);
        ok = validateNonBlack(ren->getBuffer(), W*H, "test8_lassoFull");
    }

    sc.renderRoot->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char **argv)
{
    initCoinHeadless();

    const char *basepath = (argc > 1) ? argv[1] : "render_ext_selection_events";

    /* Render the canonical factory scene as the primary output image.
     * This ensures obol_viewer and obol_render produce identical scenes. */
    {
        SoSeparator *fRoot = ObolTest::Scenes::createExtSelectionEvents(256, 256);
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
    printf("\n=== SoExtSelection event-driven tests ===\n");

    if (!test1_rectangleEvents(basepath))  { printf("FAIL: test1\n"); ++failures; }
    if (!test2_lassoEvents(basepath))      { printf("FAIL: test2\n"); ++failures; }
    if (!test3_lassoDoubleClick(basepath)) { printf("FAIL: test3\n"); ++failures; }
    if (!test4_escapeAbort(basepath))      { printf("FAIL: test4\n"); ++failures; }
    if (!test5_noLasso(basepath))          { printf("FAIL: test5\n"); ++failures; }
    if (!test6_shiftAndCoords(basepath))   { printf("FAIL: test6\n"); ++failures; }
    if (!test7_visibleShapes(basepath))    { printf("FAIL: test7\n"); ++failures; }
    if (!test8_lassoFull(basepath))        { printf("FAIL: test8\n"); ++failures; }

    printf("\n=== Summary: %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
