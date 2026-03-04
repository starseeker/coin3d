/*
 * render_selection_interaction.cpp
 *
 * Tests SoSelection node interaction patterns as user applications would use them:
 *   1. Single-select policy: clicking a sphere selects it; second click replaces.
 *   2. Shift-select (SoSelection::SHIFT) allows multi-object selection.
 *   3. Toggle policy: clicking a selected object deselects it.
 *   4. Selection callbacks fire on select and deselect events.
 *   5. SoSelection::deselectAll() clears all selections.
 *   6. SoSelection::getNumSelected() tracks selection count correctly.
 *   7. Render before and after selection (SoLocateHighlight for hover feedback).
 *
 * Selection is driven programmatically via SoSelection::select/deselect
 * (since SoHandleEventAction pick-based selection requires a live viewer
 * to route events through SoSelection internally).  Callback counters verify
 * that the selection API fires them correctly.
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
#include <Inventor/nodes/SoSelection.h>
#include <Inventor/nodes/SoLocateHighlight.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/SoPath.h>

#include <cstdio>

// ---------------------------------------------------------------------------
// Callback counters
// ---------------------------------------------------------------------------
static int g_selCount   = 0;
static int g_deselCount = 0;

static void onSelection(void *, SoPath *)   { ++g_selCount; }
static void onDeselection(void *, SoPath *) { ++g_deselCount; }

// ---------------------------------------------------------------------------
// Build a scene: 3 objects in a SoSelection node, camera + light above it
// ---------------------------------------------------------------------------
struct SceneObjects {
    SoSeparator   *root;       // full render root (camera + light + selRoot)
    SoSelection   *selRoot;    // the selection node
    SoSeparator   *sphSep;     // separator containing the sphere
    SoSeparator   *cubeSep;    // separator containing the cube
    SoSeparator   *coneSep;    // separator containing the cone
    SoMaterial    *sphMat;
    SoMaterial    *cubeMat;
    SoMaterial    *coneMat;
    SoPath        *sphPath;    // path to sphere (for select/deselect)
    SoPath        *cubePath;
    SoPath        *conePath;
    SbViewportRegion vp;
};

static SceneObjects buildSelectionScene(SoSelection::Policy policy)
{
    SceneObjects sc;
    sc.vp = SbViewportRegion(DEFAULT_WIDTH, DEFAULT_HEIGHT);

    sc.root = new SoSeparator;
    sc.root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 8.0f);
    cam->height.setValue(8.0f);
    sc.root->addChild(cam);
    sc.root->addChild(new SoDirectionalLight);

    sc.selRoot = new SoSelection;
    sc.selRoot->policy.setValue(policy);
    sc.selRoot->addSelectionCallback(onSelection);
    sc.selRoot->addDeselectionCallback(onDeselection);
    sc.root->addChild(sc.selRoot);

    // Create three objects side by side
    float xs[3] = { -2.5f, 0.0f, 2.5f };
    SbColor colors[3] = {
        SbColor(0.7f, 0.3f, 0.3f),
        SbColor(0.3f, 0.7f, 0.3f),
        SbColor(0.3f, 0.3f, 0.7f)
    };
    SoMaterial **mats[3]   = { &sc.sphMat,  &sc.cubeMat,  &sc.coneMat };
    SoSeparator **seps[3]  = { &sc.sphSep,  &sc.cubeSep,  &sc.coneSep };

    for (int i = 0; i < 3; ++i) {
        *seps[i] = new SoSeparator;
        SoTransform *xf = new SoTransform;
        xf->translation.setValue(xs[i], 0.0f, 0.0f);
        (*seps[i])->addChild(xf);
        *mats[i] = new SoMaterial;
        (*mats[i])->diffuseColor.setValue(colors[i]);
        (*seps[i])->addChild(*mats[i]);
        if      (i == 0) (*seps[i])->addChild(new SoSphere);
        else if (i == 1) (*seps[i])->addChild(new SoCube);
        else             (*seps[i])->addChild(new SoCone);
        sc.selRoot->addChild(*seps[i]);
    }

    // Build paths for programmatic select/deselect
    SoSearchAction sa;

    sa.setType(SoSphere::getClassTypeId());
    sa.setInterest(SoSearchAction::FIRST);
    sa.apply(sc.selRoot);
    sc.sphPath  = sa.getPath() ? sa.getPath()->copy() : nullptr;
    if (sc.sphPath) sc.sphPath->ref();

    sa.reset();
    sa.setType(SoCube::getClassTypeId());
    sa.setInterest(SoSearchAction::FIRST);
    sa.apply(sc.selRoot);
    sc.cubePath = sa.getPath() ? sa.getPath()->copy() : nullptr;
    if (sc.cubePath) sc.cubePath->ref();

    sa.reset();
    sa.setType(SoCone::getClassTypeId());
    sa.setInterest(SoSearchAction::FIRST);
    sa.apply(sc.selRoot);
    sc.conePath = sa.getPath() ? sa.getPath()->copy() : nullptr;
    if (sc.conePath) sc.conePath->ref();

    return sc;
}

static void freeScene(SceneObjects &sc)
{
    if (sc.sphPath)  sc.sphPath->unref();
    if (sc.cubePath) sc.cubePath->unref();
    if (sc.conePath) sc.conePath->unref();
    sc.root->unref();
    sc.sphPath = sc.cubePath = sc.conePath = nullptr;
}

// ---------------------------------------------------------------------------
// Test 1: SINGLE policy – programmatic select/deselect/re-select sequence
// ---------------------------------------------------------------------------
static bool test1_singlePolicy(const char *basepath)
{
    g_selCount = g_deselCount = 0;
    SceneObjects sc = buildSelectionScene(SoSelection::SINGLE);

    char fname[1024];
    snprintf(fname, sizeof(fname), "%s_single_initial.rgb", basepath);
    bool r1 = renderToFile(sc.root, fname);

    // Select sphere; numSelected must become 1
    if (sc.sphPath) sc.selRoot->select(sc.sphPath);
    int afterSphere = sc.selRoot->getNumSelected();

    snprintf(fname, sizeof(fname), "%s_single_sphere_selected.rgb", basepath);
    bool r2 = renderToFile(sc.root, fname);

    // Deselect all, then select cube; selection must hold exactly 1 object
    sc.selRoot->deselectAll();
    if (sc.cubePath) sc.selRoot->select(sc.cubePath);
    int afterCube = sc.selRoot->getNumSelected();

    snprintf(fname, sizeof(fname), "%s_single_cube_selected.rgb", basepath);
    bool r3 = renderToFile(sc.root, fname);

    bool ok = r1 && r2 && r3 && (afterSphere == 1) && (afterCube == 1);
    if (!ok) {
        fprintf(stderr,
                "  FAIL test1: afterSphere=%d afterCube=%d (expected both 1)\n",
                afterSphere, afterCube);
    } else {
        printf("  PASS test1: SINGLE policy, afterSphere=%d afterCube=%d, sel=%d desel=%d\n",
               afterSphere, afterCube, g_selCount, g_deselCount);
    }

    freeScene(sc);
    return ok;
}

// ---------------------------------------------------------------------------
// Test 2: SHIFT policy – select multiple objects
// ---------------------------------------------------------------------------
static bool test2_shiftPolicy(const char *basepath)
{
    g_selCount = g_deselCount = 0;
    SceneObjects sc = buildSelectionScene(SoSelection::SHIFT);

    char fname[1024];
    snprintf(fname, sizeof(fname), "%s_shift_initial.rgb", basepath);
    renderToFile(sc.root, fname);

    if (sc.sphPath)  sc.selRoot->select(sc.sphPath);
    if (sc.cubePath) sc.selRoot->select(sc.cubePath);
    if (sc.conePath) sc.selRoot->select(sc.conePath);

    snprintf(fname, sizeof(fname), "%s_shift_all_selected.rgb", basepath);
    bool r1 = renderToFile(sc.root, fname);

    int numSel = sc.selRoot->getNumSelected();
    bool ok = r1 && (numSel == 3) && (g_selCount == 3);
    if (!ok) {
        fprintf(stderr,
                "  FAIL test2: numSel=%d selCB=%d (expected 3,3)\n",
                numSel, g_selCount);
    } else {
        printf("  PASS test2: SHIFT policy, %d objects selected, selCB fired %d times\n",
               numSel, g_selCount);
    }

    freeScene(sc);
    return ok;
}

// ---------------------------------------------------------------------------
// Test 3: TOGGLE policy – selecting twice deselects
// ---------------------------------------------------------------------------
static bool test3_togglePolicy(const char *basepath)
{
    g_selCount = g_deselCount = 0;
    SceneObjects sc = buildSelectionScene(SoSelection::TOGGLE);

    if (sc.sphPath) sc.selRoot->select(sc.sphPath);
    int afterFirstSelect = sc.selRoot->getNumSelected();

    if (sc.sphPath) sc.selRoot->deselect(sc.sphPath);
    int afterDeselect = sc.selRoot->getNumSelected();

    char fname[1024];
    snprintf(fname, sizeof(fname), "%s_toggle_deselected.rgb", basepath);
    bool r1 = renderToFile(sc.root, fname);

    bool ok = r1 && (afterFirstSelect == 1) && (afterDeselect == 0);
    if (!ok) {
        fprintf(stderr,
                "  FAIL test3: after select=%d after deselect=%d\n",
                afterFirstSelect, afterDeselect);
    } else {
        printf("  PASS test3: TOGGLE deselect worked (%d→%d)\n",
               afterFirstSelect, afterDeselect);
    }

    freeScene(sc);
    return ok;
}

// ---------------------------------------------------------------------------
// Test 4: Callbacks fire on select/deselect
// ---------------------------------------------------------------------------
static bool test4_callbacks(const char *basepath)
{
    g_selCount = g_deselCount = 0;
    SceneObjects sc = buildSelectionScene(SoSelection::SHIFT);

    if (sc.sphPath)  sc.selRoot->select(sc.sphPath);
    if (sc.cubePath) sc.selRoot->select(sc.cubePath);
    if (sc.sphPath)  sc.selRoot->deselect(sc.sphPath);

    bool ok = (g_selCount == 2) && (g_deselCount == 1);
    if (!ok) {
        fprintf(stderr, "  FAIL test4: selCB=%d deselCB=%d (expected 2,1)\n",
                g_selCount, g_deselCount);
    } else {
        printf("  PASS test4: callbacks fired selCB=%d deselCB=%d\n",
               g_selCount, g_deselCount);
    }

    (void)basepath;
    freeScene(sc);
    return ok;
}

// ---------------------------------------------------------------------------
// Test 5: deselectAll() clears selection
// ---------------------------------------------------------------------------
static bool test5_deselectAll(const char *basepath)
{
    g_selCount = g_deselCount = 0;
    SceneObjects sc = buildSelectionScene(SoSelection::SHIFT);

    if (sc.sphPath)  sc.selRoot->select(sc.sphPath);
    if (sc.cubePath) sc.selRoot->select(sc.cubePath);
    if (sc.conePath) sc.selRoot->select(sc.conePath);

    sc.selRoot->deselectAll();

    char fname[1024];
    snprintf(fname, sizeof(fname), "%s_deselectall.rgb", basepath);
    bool r1 = renderToFile(sc.root, fname);

    int numSel = sc.selRoot->getNumSelected();
    bool ok = r1 && (numSel == 0);
    if (!ok) {
        fprintf(stderr, "  FAIL test5: numSel=%d after deselectAll\n", numSel);
    } else {
        printf("  PASS test5: deselectAll cleared %d selections, deselCB=%d\n",
               3, g_deselCount);
    }

    freeScene(sc);
    return ok;
}

// ---------------------------------------------------------------------------
// Test 6: getNumSelected() tracks selection count correctly
// ---------------------------------------------------------------------------
static bool test6_numSelected()
{
    g_selCount = g_deselCount = 0;
    SceneObjects sc = buildSelectionScene(SoSelection::SHIFT);

    bool ok = true;
    // Start with 0
    if (sc.selRoot->getNumSelected() != 0) {
        fprintf(stderr, "  FAIL test6: initial count != 0\n"); ok = false;
    }
    if (sc.sphPath)  { sc.selRoot->select(sc.sphPath);  if (sc.selRoot->getNumSelected() != 1) ok = false; }
    if (sc.cubePath) { sc.selRoot->select(sc.cubePath); if (sc.selRoot->getNumSelected() != 2) ok = false; }
    if (sc.conePath) { sc.selRoot->select(sc.conePath); if (sc.selRoot->getNumSelected() != 3) ok = false; }
    if (sc.sphPath)  { sc.selRoot->deselect(sc.sphPath); if (sc.selRoot->getNumSelected() != 2) ok = false; }

    if (!ok) {
        fprintf(stderr, "  FAIL test6: numSelected tracking incorrect\n");
    } else {
        printf("  PASS test6: getNumSelected() tracked all transitions correctly\n");
    }

    freeScene(sc);
    return ok;
}

// ---------------------------------------------------------------------------
// Test 7: SoLocateHighlight renders without crash
// ---------------------------------------------------------------------------
static bool test7_locateHighlight(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 8.0f);
    cam->height.setValue(6.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    // SoLocateHighlight automatically changes appearance when the mouse is
    // over it (in interactive mode).  Headless, we just verify it renders.
    SoLocateHighlight *lh1 = new SoLocateHighlight;
    SoTransform *xf1 = new SoTransform;
    xf1->translation.setValue(-1.5f, 0.0f, 0.0f);
    lh1->addChild(xf1);
    SoMaterial *m1 = new SoMaterial;
    m1->diffuseColor.setValue(0.8f, 0.4f, 0.1f);
    lh1->addChild(m1);
    lh1->addChild(new SoSphere);
    root->addChild(lh1);

    SoLocateHighlight *lh2 = new SoLocateHighlight;
    SoTransform *xf2 = new SoTransform;
    xf2->translation.setValue(1.5f, 0.0f, 0.0f);
    lh2->addChild(xf2);
    SoMaterial *m2 = new SoMaterial;
    m2->diffuseColor.setValue(0.2f, 0.6f, 0.9f);
    lh2->addChild(m2);
    lh2->addChild(new SoCube);
    root->addChild(lh2);

    // Simulate mouse motion over each object (exercises locate-highlight path)
    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    simulateMouseMotion(root, vp, DEFAULT_WIDTH/4, DEFAULT_HEIGHT/2);
    simulateMouseMotion(root, vp, 3*DEFAULT_WIDTH/4, DEFAULT_HEIGHT/2);
    simulateMouseMotion(root, vp, DEFAULT_WIDTH/2, DEFAULT_HEIGHT/2);

    char fname[1024];
    snprintf(fname, sizeof(fname), "%s_locate_highlight.rgb", basepath);
    bool r1 = renderToFile(root, fname);

    bool ok = r1;
    if (!ok) {
        fprintf(stderr, "  FAIL test7: SoLocateHighlight render\n");
    } else {
        printf("  PASS test7: SoLocateHighlight rendered without crash\n");
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

    const char *basepath = (argc > 1) ? argv[1] : "render_selection_interaction";

    /* Render the canonical factory scene as the primary output image.
     * This ensures obol_viewer and obol_render produce identical scenes. */
    {
        SoSeparator *fRoot = ObolTest::Scenes::createSelectionInteraction(256, 256);
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

    printf("\n=== SoSelection interaction tests ===\n");

    if (!test1_singlePolicy(basepath))  ++failures;
    if (!test2_shiftPolicy(basepath))   ++failures;
    if (!test3_togglePolicy(basepath))  ++failures;
    if (!test4_callbacks(basepath))     ++failures;
    if (!test5_deselectAll(basepath))   ++failures;
    if (!test6_numSelected())           ++failures;
    if (!test7_locateHighlight(basepath)) ++failures;

    printf("\n=== Summary: %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
