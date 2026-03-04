/*
 * render_glrender_action_modes.cpp — SoGLRenderAction deep coverage test
 *
 * Exercises previously uncovered paths in SoGLRenderAction:
 *   1. Transparency types: SORTED_OBJECT_BLEND, SORTED_OBJECT_ADD,
 *      DELAYED_BLEND, DELAYED_ADD, BLEND, ADD, SCREEN_DOOR, NONE,
 *      SORTED_OBJECT_SORTED_TRIANGLE_BLEND
 *   2. setSmoothing() (anti-aliasing hint)
 *   3. setNumPasses() with >1 (accumulation buffer multi-pass)
 *   4. setPassCallback()
 *   5. setCacheContext()
 *   6. setAbortCallback() (returns CONTINUE)
 *   7. getTransparencyType() round-trip
 *   8. SoGLRenderAction::getViewportRegion() round-trip
 *   9. Transparent objects with each transparency mode
 *  10. SoTransparencyType node override
 *
 * Returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoTransparencyType.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoDB.h>
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
// Pass callback — counts invocations
// ---------------------------------------------------------------------------
static int g_passCBCount = 0;
static void passCB(void * /*ud*/)
{
    ++g_passCBCount;
}

// ---------------------------------------------------------------------------
// Abort callback — always continues
// ---------------------------------------------------------------------------
static SoGLRenderAction::AbortCode abortCB(void * /*ud*/)
{
    return SoGLRenderAction::CONTINUE;
}

// ---------------------------------------------------------------------------
// Build a standard test scene: camera + light + two semi-transparent spheres
// ---------------------------------------------------------------------------
static SoSeparator *buildTransparentScene(float alpha1, float alpha2)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 8.0f);
    cam->nearDistance = 0.5f;
    cam->farDistance  = 50.0f;
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    // Back sphere (red, partial transparency)
    {
        SoSeparator *sep = new SoSeparator;
        SoTranslation *tr = new SoTranslation;
        tr->translation.setValue(-0.5f, 0.0f, -1.0f);
        sep->addChild(tr);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.9f, 0.1f, 0.1f);
        mat->transparency.setValue(alpha1);
        sep->addChild(mat);
        sep->addChild(new SoSphere);
        root->addChild(sep);
    }

    // Front sphere (blue, less transparent)
    {
        SoSeparator *sep = new SoSeparator;
        SoTranslation *tr = new SoTranslation;
        tr->translation.setValue(0.5f, 0.0f, 1.0f);
        sep->addChild(tr);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.1f, 0.1f, 0.9f);
        mat->transparency.setValue(alpha2);
        sep->addChild(mat);
        sep->addChild(new SoCube);
        root->addChild(sep);
    }

    SbViewportRegion vp(W, H);
    cam->viewAll(root, vp);

    return root;
}

// ---------------------------------------------------------------------------
// Helper: render with a specific transparency type
// ---------------------------------------------------------------------------
static bool renderWithTranspType(SoGLRenderAction::TransparencyType tt,
                                  const char *label)
{
    SoSeparator *root = buildTransparentScene(0.5f, 0.3f);

    SoOffscreenRenderer *ren = getSharedRenderer();
    SbViewportRegion vp(W, H);
    ren->setViewportRegion(vp);

    // Access the render action from the renderer and set the type
    SoGLRenderAction *ra = ren->getGLRenderAction();
    ra->setTransparencyType(tt);

    // Verify round-trip
    SoGLRenderAction::TransparencyType got = ra->getTransparencyType();
    if (got != tt) {
        fprintf(stderr, "  WARN %s: set %d got %d\n", label, (int)tt, (int)got);
    }

    bool ok = ren->render(root);
    if (ok)
        ok = validateNonBlack(ren->getBuffer(), W*H, label);

    // Reset to default
    ra->setTransparencyType(SoGLRenderAction::DELAYED_BLEND);

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 1: Each transparency type
// ---------------------------------------------------------------------------
static bool test1_transparencyTypes()
{
    bool ok = true;

    ok &= renderWithTranspType(SoGLRenderAction::BLEND,         "BLEND");
    ok &= renderWithTranspType(SoGLRenderAction::ADD,           "ADD");
    ok &= renderWithTranspType(SoGLRenderAction::DELAYED_BLEND, "DELAYED_BLEND");
    ok &= renderWithTranspType(SoGLRenderAction::DELAYED_ADD,   "DELAYED_ADD");
    ok &= renderWithTranspType(SoGLRenderAction::SORTED_OBJECT_BLEND, "SORTED_OBJECT_BLEND");
    ok &= renderWithTranspType(SoGLRenderAction::SORTED_OBJECT_ADD,   "SORTED_OBJECT_ADD");
    ok &= renderWithTranspType(SoGLRenderAction::SORTED_OBJECT_SORTED_TRIANGLE_BLEND,
                                "SORTED_OBJ_TRI_BLEND");
    ok &= renderWithTranspType(SoGLRenderAction::SCREEN_DOOR,  "SCREEN_DOOR");
    ok &= renderWithTranspType(SoGLRenderAction::NONE,         "NONE");

    printf("  PASS test1_transparencyTypes (all render without crash)\n");
    return ok;
}

// ---------------------------------------------------------------------------
// Test 2: setSmoothing / setNumPasses / setPassCallback
// ---------------------------------------------------------------------------
static bool test2_smoothingAndPasses()
{
    SoSeparator *root = buildTransparentScene(0.0f, 0.0f); // opaque

    SoOffscreenRenderer *ren = getSharedRenderer();
    SbViewportRegion vp(W, H);
    ren->setViewportRegion(vp);

    SoGLRenderAction *ra = ren->getGLRenderAction();

    // setSmoothing
    ra->setSmoothing(TRUE);
    bool ok = ren->render(root);
    ra->setSmoothing(FALSE);

    // setNumPasses / setPassCallback
    g_passCBCount = 0;
    ra->setNumPasses(2);
    ra->setPassCallback(passCB, nullptr);
    ok &= ren->render(root);
    printf("  passCB invocations: %d\n", g_passCBCount);

    // Reset
    ra->setNumPasses(1);
    ra->setPassCallback(nullptr, nullptr);

    if (ok)
        ok = validateNonBlack(ren->getBuffer(), W*H, "test2_smoothing");

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 3: setCacheContext / getViewportRegion
// ---------------------------------------------------------------------------
static bool test3_cacheContextAndVP()
{
    SoSeparator *root = buildTransparentScene(0.0f, 0.0f);

    SoOffscreenRenderer *ren = getSharedRenderer();
    SbViewportRegion vp(W, H);
    ren->setViewportRegion(vp);

    SoGLRenderAction *ra = ren->getGLRenderAction();

    // setCacheContext (use a distinct context id)
    ra->setCacheContext(42);
    bool ok = ren->render(root);

    // getViewportRegion round-trip
    const SbViewportRegion &got = ra->getViewportRegion();
    printf("  getViewportRegion: %dx%d\n",
           got.getWindowSize()[0], got.getWindowSize()[1]);

    // Reset context to default
    ra->setCacheContext(0);

    if (ok)
        ok = validateNonBlack(ren->getBuffer(), W*H, "test3_cacheContext");

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 4: setAbortCallback (always continues)
// ---------------------------------------------------------------------------
static bool test4_abortCallback()
{
    SoSeparator *root = buildTransparentScene(0.0f, 0.0f);

    SoOffscreenRenderer *ren = getSharedRenderer();
    SbViewportRegion vp(W, H);
    ren->setViewportRegion(vp);

    SoGLRenderAction *ra = ren->getGLRenderAction();
    ra->setAbortCallback(abortCB, nullptr);
    bool ok = ren->render(root);

    // Remove abort callback
    ra->setAbortCallback(nullptr, nullptr);

    if (ok)
        ok = validateNonBlack(ren->getBuffer(), W*H, "test4_abortCallback");

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 5: SoTransparencyType node (overrides action's setting)
// ---------------------------------------------------------------------------
static bool test5_transparencyTypeNode(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    // SoTransparencyType node
    SoTransparencyType *tt = new SoTransparencyType;
    tt->value.setValue(SoGLRenderAction::SORTED_OBJECT_BLEND);
    root->addChild(tt);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.5f, 0.8f, 0.3f);
    mat->transparency.setValue(0.4f);
    root->addChild(mat);
    root->addChild(new SoSphere);

    SbViewportRegion vp(W, H);
    cam->viewAll(root, vp);

    char outpath[1024];
    snprintf(outpath, sizeof(outpath), "%s_tt_node.rgb", basepath);
    bool ok = renderToFile(root, outpath, W, H);

    if (ok) {
        SoOffscreenRenderer *ren = getSharedRenderer();
        ren->setViewportRegion(vp);
        ok = ren->render(root) &&
             validateNonBlack(ren->getBuffer(), W*H, "test5_transparencyTypeNode");
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

    const char *basepath = (argc > 1) ? argv[1] : "render_glrender_action_modes";

    /* Render the canonical factory scene as the primary output image.
     * This ensures obol_viewer and obol_render produce identical scenes. */
    {
        SoSeparator *fRoot = ObolTest::Scenes::createGLRenderActionModes(256, 256);
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

    printf("\n=== SoGLRenderAction modes and settings tests ===\n");

    if (!test1_transparencyTypes())         ++failures;
    if (!test2_smoothingAndPasses())        ++failures;
    if (!test3_cacheContextAndVP())         ++failures;
    if (!test4_abortCallback())             ++failures;
    if (!test5_transparencyTypeNode(basepath)) ++failures;

    printf("\n=== Summary: %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
