/*
 * render_ext_selection.cpp — SoExtSelection coverage test
 *
 * Exercises SoExtSelection and related paths:
 *   1. Programmatic lasso select via select(root, ncoords, lasso, vp, shift)
 *      with FULL_BBOX mode (all shapes whose bounding box is within lasso)
 *   2. PART_BBOX lasso mode (any shape that overlaps the lasso)
 *   3. RECTANGLE lassoType field
 *   4. LASSO lassoType field
 *   5. SoExtSelection::setTriangleFilterCallback — filter callback is invoked
 *   6. SoExtSelection::setLassoFilterCallback — filter decides per-path
 *   7. getLassoColor, setLassoColor, getLassoWidth, setLassoWidth
 *   8. getOverlayLassoPattern, setOverlayLassoPattern
 *   9. Selection/deselection callbacks (inherited from SoSelection) fire
 *      after programmatic lasso selection
 *  10. Render the scene with SoExtSelection as the root separator
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
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoExtSelection.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbColor.h>
#include <Inventor/SoPath.h>
#include <cstdio>

static const int W = 256;
static const int H = 256;

static bool validateNonBlack(const unsigned char *buf, int npix,
                              const char *label)
{
    int nonbg = 0;
    for (int i = 0; i < npix; ++i) {
        const unsigned char *p = buf + i * 3;
        if (p[0] > 5 || p[1] > 5 || p[2] > 5) ++nonbg;
    }
    printf("  %s: nonbg=%d\n", label, nonbg);
    if (nonbg < 50) {
        fprintf(stderr, "  FAIL %s: blank\n", label);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------
static int g_selCB   = 0;
static int g_deselCB = 0;
static int g_triCB   = 0;
static int g_lassoCB = 0;

static void onSel(void *, SoPath *)   { ++g_selCB; }
static void onDesel(void *, SoPath *) { ++g_deselCB; }

static SbBool onTriangle(void * /*ud*/, SoCallbackAction * /*cb*/,
                         const SoPrimitiveVertex * /*v1*/,
                         const SoPrimitiveVertex * /*v2*/,
                         const SoPrimitiveVertex * /*v3*/)
{
    ++g_triCB;
    return TRUE; // include in selection
}

static SoPath *onLassoFilter(void * /*ud*/, const SoPath *path)
{
    ++g_lassoCB;
    return const_cast<SoPath *>(path); // pass through
}

// ---------------------------------------------------------------------------
// Build a simple 3-object scene with an SoExtSelection root
// ---------------------------------------------------------------------------
struct ExtScene {
    SoSeparator      *renderRoot;  // camera + light + extsel
    SoExtSelection   *extSel;
    SbViewportRegion  vp;
};

static ExtScene buildScene()
{
    ExtScene sc;
    sc.vp = SbViewportRegion(W, H);

    sc.renderRoot = new SoSeparator;
    sc.renderRoot->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 10.0f);
    cam->height.setValue(10.0f);
    sc.renderRoot->addChild(cam);
    sc.renderRoot->addChild(new SoDirectionalLight);

    sc.extSel = new SoExtSelection;
    sc.renderRoot->addChild(sc.extSel);

    // Three objects at predictable positions
    float xs[3] = { -3.0f, 0.0f, 3.0f };
    SoNode *shapes[3] = { new SoSphere, new SoCube, new SoCone };
    for (int i = 0; i < 3; ++i) {
        SoSeparator *sep = new SoSeparator;
        SoTranslation *tr = new SoTranslation;
        tr->translation.setValue(xs[i], 0.0f, 0.0f);
        sep->addChild(tr);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.3f + i*0.2f, 0.5f, 0.8f - i*0.2f);
        sep->addChild(mat);
        sep->addChild(shapes[i]);
        sc.extSel->addChild(sep);
    }

    return sc;
}

// ---------------------------------------------------------------------------
// Test 1: FULL_BBOX lasso — lasso covers center object only
// ---------------------------------------------------------------------------
static bool test1_fullBBoxLasso(const char *basepath)
{
    g_selCB = g_deselCB = 0;

    ExtScene sc = buildScene();
    sc.extSel->addSelectionCallback(onSel, nullptr);
    sc.extSel->addDeselectionCallback(onDesel, nullptr);
    sc.extSel->lassoType.setValue(SoExtSelection::LASSO);
    sc.extSel->lassoMode.setValue(SoExtSelection::FULL_BBOX);

    // Lasso rectangle covering center viewport (−40%…+40% in both axes)
    SbVec2f lasso[4] = {
        SbVec2f(0.1f, 0.1f),
        SbVec2f(0.9f, 0.1f),
        SbVec2f(0.9f, 0.9f),
        SbVec2f(0.1f, 0.9f)
    };
    sc.extSel->select(sc.renderRoot, 4, lasso, sc.vp, FALSE);

    printf("  test1_fullBBoxLasso: selCB=%d deselCB=%d numSel=%d\n",
           g_selCB, g_deselCB, sc.extSel->getNumSelected());

    // Render the scene
    SoOffscreenRenderer *ren = getSharedRenderer();
    ren->setViewportRegion(sc.vp);
    bool ok = ren->render(sc.renderRoot);

    if (ok) {
        char outpath[1024];
        snprintf(outpath, sizeof(outpath), "%s_full_bbox.rgb", basepath);
        renderToFile(sc.renderRoot, outpath, W, H);
        ok = validateNonBlack(ren->getBuffer(), W*H, "test1_fullBBoxLasso");
    }

    sc.renderRoot->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 2: PART_BBOX lasso + RECTANGLE type
// ---------------------------------------------------------------------------
static bool test2_partBBoxRect(const char *basepath)
{
    g_selCB = g_deselCB = 0;

    ExtScene sc = buildScene();
    sc.extSel->addSelectionCallback(onSel, nullptr);
    sc.extSel->addDeselectionCallback(onDesel, nullptr);
    sc.extSel->lassoType.setValue(SoExtSelection::RECTANGLE);
    sc.extSel->lassoMode.setValue(SoExtSelection::PART_BBOX);

    // A rectangle that partially covers both the left and center objects
    SbVec2f lasso[4] = {
        SbVec2f(0.0f, 0.0f),
        SbVec2f(0.6f, 0.0f),
        SbVec2f(0.6f, 1.0f),
        SbVec2f(0.0f, 1.0f)
    };
    sc.extSel->select(sc.renderRoot, 4, lasso, sc.vp, FALSE);

    printf("  test2_partBBoxRect: selCB=%d numSel=%d\n",
           g_selCB, sc.extSel->getNumSelected());

    SoOffscreenRenderer *ren = getSharedRenderer();
    ren->setViewportRegion(sc.vp);
    bool ok = ren->render(sc.renderRoot);

    if (ok) {
        char outpath[1024];
        snprintf(outpath, sizeof(outpath), "%s_part_bbox.rgb", basepath);
        renderToFile(sc.renderRoot, outpath, W, H);
        ok = validateNonBlack(ren->getBuffer(), W*H, "test2_partBBoxRect");
    }

    sc.renderRoot->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 3: Triangle filter callback (requires FULL mode for triangle testing)
// ---------------------------------------------------------------------------
static bool test3_triangleFilter(const char *basepath)
{
    g_triCB = 0;

    ExtScene sc = buildScene();
    sc.extSel->lassoType.setValue(SoExtSelection::RECTANGLE);
    sc.extSel->lassoMode.setValue(SoExtSelection::FULL);

    // Register triangle filter callback
    sc.extSel->setTriangleFilterCallback(onTriangle, nullptr);

    // Big lasso covering the whole scene
    SbVec2f lasso[4] = {
        SbVec2f(0.0f, 0.0f),
        SbVec2f(1.0f, 0.0f),
        SbVec2f(1.0f, 1.0f),
        SbVec2f(0.0f, 1.0f)
    };
    sc.extSel->select(sc.renderRoot, 4, lasso, sc.vp, FALSE);

    printf("  test3_triangleFilter: triCB=%d numSel=%d\n",
           g_triCB, sc.extSel->getNumSelected());

    SoOffscreenRenderer *ren = getSharedRenderer();
    ren->setViewportRegion(sc.vp);
    bool ok = ren->render(sc.renderRoot);

    if (ok) {
        char outpath[1024];
        snprintf(outpath, sizeof(outpath), "%s_tri_filter.rgb", basepath);
        renderToFile(sc.renderRoot, outpath, W, H);
        ok = validateNonBlack(ren->getBuffer(), W*H, "test3_triangleFilter");
    }

    sc.renderRoot->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 4: Lasso filter callback + accessors (color, width, pattern)
// ---------------------------------------------------------------------------
static bool test4_lassoFilter(const char *basepath)
{
    g_lassoCB = 0;

    ExtScene sc = buildScene();
    sc.extSel->lassoType.setValue(SoExtSelection::LASSO);
    sc.extSel->lassoMode.setValue(SoExtSelection::PART_BBOX);

    // Register lasso filter
    sc.extSel->setLassoFilterCallback(onLassoFilter, nullptr);

    // Exercise lasso appearance API
    sc.extSel->setLassoColor(SbColor(1.0f, 0.5f, 0.0f));
    const SbColor &c = sc.extSel->getLassoColor();
    printf("  test4 lassoColor: (%.2f,%.2f,%.2f)\n", c[0], c[1], c[2]);

    sc.extSel->setLassoWidth(2.5f);
    printf("  test4 lassoWidth: %.2f\n", sc.extSel->getLassoWidth());

    sc.extSel->setOverlayLassoPattern(0x00FF);
    printf("  test4 lassoPattern: 0x%04X\n",
           (unsigned)sc.extSel->getOverlayLassoPattern());

    sc.extSel->animateOverlayLasso(FALSE);
    printf("  test4 animated: %d\n",
           (int)sc.extSel->isOverlayLassoAnimated());

    // Small lasso in the upper-right
    SbVec2f lasso[4] = {
        SbVec2f(0.6f, 0.6f),
        SbVec2f(1.0f, 0.6f),
        SbVec2f(1.0f, 1.0f),
        SbVec2f(0.6f, 1.0f)
    };
    sc.extSel->select(sc.renderRoot, 4, lasso, sc.vp, FALSE);

    printf("  test4_lassoFilter: lassoCB=%d numSel=%d\n",
           g_lassoCB, sc.extSel->getNumSelected());

    SoOffscreenRenderer *ren = getSharedRenderer();
    ren->setViewportRegion(sc.vp);
    bool ok = ren->render(sc.renderRoot);

    if (ok) {
        char outpath[1024];
        snprintf(outpath, sizeof(outpath), "%s_lasso_filter.rgb", basepath);
        renderToFile(sc.renderRoot, outpath, W, H);
        ok = validateNonBlack(ren->getBuffer(), W*H, "test4_lassoFilter");
    }

    sc.renderRoot->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 5: 3D (SbVec3f) lasso interface
// ---------------------------------------------------------------------------
static bool test5_3dlasso(const char *basepath)
{
    ExtScene sc = buildScene();
    sc.extSel->lassoType.setValue(SoExtSelection::LASSO);
    sc.extSel->lassoMode.setValue(SoExtSelection::PART_BBOX);

    // 3-D viewport-space lasso (z=0 is typical)
    SbVec3f lasso3[4] = {
        SbVec3f(0.0f, 0.0f, 0.0f),
        SbVec3f(W * 1.0f, 0.0f, 0.0f),
        SbVec3f(W * 1.0f, H * 1.0f, 0.0f),
        SbVec3f(0.0f, H * 1.0f, 0.0f)
    };
    sc.extSel->select(sc.renderRoot, 4, lasso3, sc.vp, FALSE);

    printf("  test5_3dlasso: numSel=%d\n", sc.extSel->getNumSelected());

    SoOffscreenRenderer *ren = getSharedRenderer();
    ren->setViewportRegion(sc.vp);
    bool ok = ren->render(sc.renderRoot);

    if (ok) {
        char outpath[1024];
        snprintf(outpath, sizeof(outpath), "%s_3dlasso.rgb", basepath);
        renderToFile(sc.renderRoot, outpath, W, H);
        ok = validateNonBlack(ren->getBuffer(), W*H, "test5_3dlasso");
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

    const char *basepath = (argc > 1) ? argv[1] : "render_ext_selection";

    /* Render the canonical factory scene as the primary output image.
     * This ensures obol_viewer and obol_render produce identical scenes. */
    {
        SoSeparator *fRoot = ObolTest::Scenes::createExtSelection(256, 256);
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

    printf("\n=== SoExtSelection tests ===\n");

    if (!test1_fullBBoxLasso(basepath))  ++failures;
    if (!test2_partBBoxRect(basepath))   ++failures;
    if (!test3_triangleFilter(basepath)) ++failures;
    if (!test4_lassoFilter(basepath))    ++failures;
    if (!test5_3dlasso(basepath))        ++failures;

    printf("\n=== Summary: %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
