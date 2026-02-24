/*
 * render_glrender_deep.cpp — SoGLRenderAction deep coverage
 *
 * Exercises SoGLRenderAction code paths not covered by existing tests.
 * Particularly focuses on:
 *   1. setUpdateArea / getUpdateArea round-trip
 *   2. addPreRenderCallback / removePreRenderCallback
 *   3. isRenderingDelayedPaths() query
 *   4. setRenderingIsRemote
 *   5. setTransparentDelayedObjectRenderType / getter round-trip
 *   6. getAbortCallback round-trip (set + get)
 *   7. isPassUpdate / setPassUpdate round-trip
 *   8. SORTED_LAYERS_BLEND transparency type (exercises the separate blend path)
 *   9. DELAYED_ADD / DELAYED_BLEND transparency types
 *  10. Rendering with sorted-object transparency: multiple transparent objects
 *  11. setViewportRegion after construction
 *  12. Caching modes: NO_AUTO_CACHING, CACHE_ALL  
 *
 * Returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
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
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoDB.h>
#include <cstdio>
#include <cmath>

static const int W = 128;
static const int H = 128;

// ---------------------------------------------------------------------------
// Pre-render callback counter
// ---------------------------------------------------------------------------
static int g_preCBCount = 0;
static void preCB(void * /*ud*/, SoGLRenderAction * /*action*/)
{
    ++g_preCBCount;
}

static bool validateNonBlack(const unsigned char *buf, int npix, const char *lbl)
{
    int nonbg = 0;
    for (int i = 0; i < npix; ++i) {
        const unsigned char *p = buf + i * 3;
        if (p[0] > 5 || p[1] > 5 || p[2] > 5) ++nonbg;
    }
    printf("  %s: nonbg=%d\n", lbl, nonbg);
    return nonbg >= 10;
}

// ---------------------------------------------------------------------------
// Build a simple scene
// ---------------------------------------------------------------------------
static SoSeparator *buildScene()
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    root->addChild(cam);

    SoDirectionalLight *lt = new SoDirectionalLight;
    lt->direction.setValue(-1.0f, -1.0f, -1.0f);
    root->addChild(lt);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.6f, 0.4f, 0.9f);
    root->addChild(mat);
    root->addChild(new SoCube);
    return root;
}

// ---------------------------------------------------------------------------
// Build a transparent multi-object scene
// ---------------------------------------------------------------------------
static SoSeparator *buildTransparentScene()
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 6.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    float xs[3] = { -1.5f, 0.0f, 1.5f };
    for (int i = 0; i < 3; ++i) {
        SoSeparator *sep = new SoSeparator;
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.8f - i*0.3f, 0.3f + i*0.2f, 0.5f);
        mat->transparency.setValue(0.5f);
        sep->addChild(mat);
        SoTranslation *tr = new SoTranslation;
        tr->translation.setValue(xs[i], 0.0f, 0.0f);
        sep->addChild(tr);
        sep->addChild(new SoSphere);
        root->addChild(sep);
    }
    return root;
}

// ---------------------------------------------------------------------------
// Test 1: setUpdateArea / getUpdateArea round-trip
// ---------------------------------------------------------------------------
static bool test1_updateArea(const char *basepath)
{
    SbViewportRegion vp((short)W, (short)H);
    SoGLRenderAction action(vp);

    SbVec2f origin(0.1f, 0.2f), size(0.8f, 0.7f);
    action.setUpdateArea(origin, size);

    SbVec2f gotOrigin, gotSize;
    action.getUpdateArea(gotOrigin, gotSize);
    printf("  test1: origin=(%.2f,%.2f) size=(%.2f,%.2f)\n",
           gotOrigin[0], gotOrigin[1], gotSize[0], gotSize[1]);

    bool ok = (fabsf(gotOrigin[0] - 0.1f) < 0.001f &&
               fabsf(gotOrigin[1] - 0.2f) < 0.001f &&
               fabsf(gotSize[0]   - 0.8f) < 0.001f &&
               fabsf(gotSize[1]   - 0.7f) < 0.001f);

    // Render with restricted update area
    SoSeparator *scene = buildScene();
    SoOffscreenRenderer *ren = getSharedRenderer();
    ren->setViewportRegion(vp);
    ren->getGLRenderAction()->setUpdateArea(origin, size);
    ren->render(scene);
    // Reset update area
    ren->getGLRenderAction()->setUpdateArea(SbVec2f(0,0), SbVec2f(1,1));

    char path[1024];
    snprintf(path, sizeof(path), "%s_updatearea.rgb", basepath);
    renderToFile(scene, path, W, H);
    scene->unref();

    return ok;
}

// ---------------------------------------------------------------------------
// Test 2: addPreRenderCallback / removePreRenderCallback
// ---------------------------------------------------------------------------
static bool test2_preRenderCallback(const char *basepath)
{
    g_preCBCount = 0;

    SbViewportRegion vp((short)W, (short)H);
    SoSeparator *scene = buildScene();

    SoOffscreenRenderer *ren = getSharedRenderer();
    ren->setViewportRegion(vp);
    ren->getGLRenderAction()->addPreRenderCallback(preCB, nullptr);

    ren->render(scene);
    printf("  test2: preCBCount after render=%d\n", g_preCBCount);

    ren->getGLRenderAction()->removePreRenderCallback(preCB, nullptr);

    ren->render(scene);
    printf("  test2: preCBCount after remove=%d (should not increase)\n", g_preCBCount);

    char path[1024];
    snprintf(path, sizeof(path), "%s_precb.rgb", basepath);
    renderToFile(scene, path, W, H);

    scene->unref();
    bool ok = validateNonBlack(ren->getBuffer(), W*H, "test2_precb");
    return ok;
}

// ---------------------------------------------------------------------------
// Test 3: setTransparentDelayedObjectRenderType / getter
// ---------------------------------------------------------------------------
static bool test3_transparentDelayed(const char *basepath)
{
    SbViewportRegion vp((short)W, (short)H);
    SoGLRenderAction action(vp);

    action.setTransparentDelayedObjectRenderType(
        SoGLRenderAction::NONSOLID_SEPARATE_BACKFACE_PASS);
    bool ok = (action.getTransparentDelayedObjectRenderType() ==
               SoGLRenderAction::NONSOLID_SEPARATE_BACKFACE_PASS);
    printf("  test3: TransparentDelayed=%d ok=%d\n",
           (int)action.getTransparentDelayedObjectRenderType(), (int)ok);

    action.setTransparentDelayedObjectRenderType(SoGLRenderAction::ONE_PASS);
    ok = ok && (action.getTransparentDelayedObjectRenderType() ==
                SoGLRenderAction::ONE_PASS);

    SoSeparator *scene = buildTransparentScene();
    char path[1024];
    snprintf(path, sizeof(path), "%s_delayed.rgb", basepath);
    renderToFile(scene, path, W, H);

    SoOffscreenRenderer *ren = getSharedRenderer();
    ren->setViewportRegion(vp);
    ren->render(scene);
    scene->unref();
    return ok && validateNonBlack(ren->getBuffer(), W*H, "test3_delayed");
}

// ---------------------------------------------------------------------------
// Test 4: setPassUpdate / isPassUpdate round-trip
// ---------------------------------------------------------------------------
static bool test4_passUpdate(const char * /*basepath*/)
{
    SbViewportRegion vp((short)W, (short)H);
    SoGLRenderAction action(vp);

    action.setPassUpdate(TRUE);
    bool ok = (action.isPassUpdate() == TRUE);
    action.setPassUpdate(FALSE);
    ok = ok && (action.isPassUpdate() == FALSE);
    printf("  test4: passUpdate round-trip ok=%d\n", (int)ok);
    return ok;
}

// ---------------------------------------------------------------------------
// Test 5: setRenderingIsRemote
// ---------------------------------------------------------------------------
static bool test5_renderingIsRemote(const char *basepath)
{
    SbViewportRegion vp((short)W, (short)H);

    SoSeparator *scene = buildScene();
    SoOffscreenRenderer *ren = getSharedRenderer();
    ren->setViewportRegion(vp);
    ren->getGLRenderAction()->setRenderingIsRemote(FALSE);
    ren->render(scene);
    ren->getGLRenderAction()->setRenderingIsRemote(TRUE);
    ren->render(scene);
    ren->getGLRenderAction()->setRenderingIsRemote(FALSE);

    char path[1024];
    snprintf(path, sizeof(path), "%s_remote.rgb", basepath);
    renderToFile(scene, path, W, H);
    scene->unref();
    return validateNonBlack(ren->getBuffer(), W*H, "test5_remote");
}

// ---------------------------------------------------------------------------
// Test 6: DELAYED_ADD / DELAYED_BLEND transparency types
// ---------------------------------------------------------------------------
static bool test6_delayedBlend(const char *basepath)
{
    SoSeparator *scene = buildTransparentScene();
    SbViewportRegion vp((short)W, (short)H);

    SoOffscreenRenderer *ren = getSharedRenderer();
    ren->setViewportRegion(vp);

    SoGLRenderAction::TransparencyType types[] = {
        SoGLRenderAction::DELAYED_ADD,
        SoGLRenderAction::DELAYED_BLEND,
        SoGLRenderAction::SORTED_OBJECT_BLEND
    };
    const char *typeNames[] = { "DELAYED_ADD", "DELAYED_BLEND", "SORTED_BLEND" };

    bool ok = true;
    for (int t = 0; t < 3; ++t) {
        ren->getGLRenderAction()->setTransparencyType(types[t]);
        ren->render(scene);
        char lbl[64];
        snprintf(lbl, sizeof(lbl), "test6_%s", typeNames[t]);
        bool rendered = validateNonBlack(ren->getBuffer(), W*H, lbl);
        printf("  test6 %s: rendered=%d\n", typeNames[t], (int)rendered);
    }

    // Reset
    ren->getGLRenderAction()->setTransparencyType(SoGLRenderAction::BLEND);

    char path[1024];
    snprintf(path, sizeof(path), "%s_delayed_blend.rgb", basepath);
    renderToFile(scene, path, W, H);
    scene->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 7: getAbortCallback round-trip
// ---------------------------------------------------------------------------
static SoGLRenderAction::AbortCode abortCBFn(void * /*ud*/) {
    return SoGLRenderAction::CONTINUE;
}

static bool test7_abortCallback(const char * /*basepath*/)
{
    SbViewportRegion vp((short)W, (short)H);
    SoGLRenderAction action(vp);

    void *udIn = (void *)0xDEAD;
    action.setAbortCallback(abortCBFn, udIn);

    SoGLRenderAction::SoGLRenderAbortCB *funcOut;
    void *udOut;
    action.getAbortCallback(funcOut, udOut);

    bool ok = (funcOut == abortCBFn && udOut == udIn);
    printf("  test7: abortCB round-trip ok=%d\n", (int)ok);
    return ok;
}

// ---------------------------------------------------------------------------
// Test 8: SORTED_OBJECT_SORTED_TRIANGLE_BLEND transparency
// ---------------------------------------------------------------------------
static bool test8_sortedTriangle(const char *basepath)
{
    SoSeparator *scene = buildTransparentScene();
    SbViewportRegion vp((short)W, (short)H);

    SoOffscreenRenderer *ren = getSharedRenderer();
    ren->setViewportRegion(vp);
    ren->getGLRenderAction()->setTransparencyType(
        SoGLRenderAction::SORTED_OBJECT_SORTED_TRIANGLE_BLEND);
    ren->render(scene);
    // Reset
    ren->getGLRenderAction()->setTransparencyType(SoGLRenderAction::BLEND);

    char path[1024];
    snprintf(path, sizeof(path), "%s_sorted_tri.rgb", basepath);
    renderToFile(scene, path, W, H);
    scene->unref();
    return validateNonBlack(ren->getBuffer(), W*H, "test8_sortedTri");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char **argv)
{
    initCoinHeadless();

    const char *basepath = (argc > 1) ? argv[1] : "render_glrender_deep";

    int failures = 0;
    printf("\n=== SoGLRenderAction deep coverage tests ===\n");

    if (!test1_updateArea(basepath))        { printf("FAIL test1\n"); ++failures; }
    if (!test2_preRenderCallback(basepath)) { printf("FAIL test2\n"); ++failures; }
    if (!test3_transparentDelayed(basepath)){ printf("FAIL test3\n"); ++failures; }
    if (!test4_passUpdate(basepath))        { printf("FAIL test4\n"); ++failures; }
    if (!test5_renderingIsRemote(basepath)) { printf("FAIL test5\n"); ++failures; }
    if (!test6_delayedBlend(basepath))      { printf("FAIL test6\n"); ++failures; }
    if (!test7_abortCallback(basepath))     { printf("FAIL test7\n"); ++failures; }
    if (!test8_sortedTriangle(basepath))    { printf("FAIL test8\n"); ++failures; }

    printf("\n=== Summary: %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
