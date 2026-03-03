/*
 * render_viewport.cpp — SoViewport API coverage test
 *
 * Exercises the SoViewport single-viewport manager:
 *   1. Default construction: getViewportRegion returns 800×600 full region.
 *   2. setWindowSize / getWindowSize round-trip.
 *   3. setViewportRegion / getViewportRegion round-trip (sub-viewport case).
 *   4. setCamera / getCamera round-trip.
 *   5. setBackgroundColor / getBackgroundColor round-trip.
 *   6. render() produces a non-blank image.
 *   7. viewAll() adjusts camera to fit scene (sanity: no crash + non-blank).
 *   8. processEvent() smoke test (no crash).
 *   9. setSceneGraph(nullptr) clears the scene cleanly.
 *  10. getRoot() returns the internal SoSeparator (non-null).
 *  11. setCamera(nullptr) removes the camera cleanly.
 *
 * The rendered image is written to argv[1]+".rgb".
 * Returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/SoViewport.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/events/SoKeyboardEvent.h>
#include <Inventor/events/SoButtonEvent.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbVec2s.h>
#include <cstdio>
#include <cmath>

static const int W = 256;
static const int H = 256;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool validateNonBlack(const unsigned char * buf, int npix,
                              const char * label, int threshold = 20)
{
    int nonbg = 0;
    for (int i = 0; i < npix; ++i) {
        const unsigned char * p = buf + i * 3;
        if (p[0] > 5 || p[1] > 5 || p[2] > 5) ++nonbg;
    }
    printf("  %s: nonbg=%d\n", label, nonbg);
    return nonbg >= threshold;
}

// Simple scene: directional light + blue sphere (no camera).
static SoSeparator * buildScene()
{
    SoSeparator * root = new SoSeparator;
    root->ref();

    SoDirectionalLight * lt = new SoDirectionalLight;
    lt->direction.setValue(-1.0f, -1.0f, -1.0f);
    root->addChild(lt);

    SoMaterial * mat = new SoMaterial;
    mat->diffuseColor.setValue(0.2f, 0.4f, 0.9f);
    root->addChild(mat);

    root->addChild(new SoSphere);
    return root;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char ** argv)
{
    initCoinHeadless();

    const char * basepath = (argc > 1) ? argv[1] : "render_viewport";
    char outpath[1024];
    snprintf(outpath, sizeof(outpath), "%s.rgb", basepath);

    int failures = 0;
    printf("\n=== SoViewport tests ===\n");

    // -----------------------------------------------------------------------
    // Test 1: default viewport region is full window (800×600)
    // -----------------------------------------------------------------------
    {
        SoViewport vp;
        SbVec2s ws = vp.getWindowSize();
        const SbViewportRegion & reg = vp.getViewportRegion();
        SbVec2s vpSize = reg.getViewportSizePixels();
        bool ok = (ws[0] == 800 && ws[1] == 600 &&
                   vpSize[0] == 800 && vpSize[1] == 600);
        printf("  test1 default region: %dx%d ok=%d\n", ws[0], ws[1], (int)ok);
        if (!ok) { printf("FAIL test1\n"); ++failures; }
    }

    // -----------------------------------------------------------------------
    // Test 2: setWindowSize / getWindowSize round-trip
    // -----------------------------------------------------------------------
    {
        SoViewport vp;
        vp.setWindowSize(SbVec2s((short)W, (short)H));
        SbVec2s ws = vp.getWindowSize();
        bool ok = (ws[0] == W && ws[1] == H);
        const SbViewportRegion & reg = vp.getViewportRegion();
        SbVec2s vpSize = reg.getViewportSizePixels();
        ok = ok && (vpSize[0] == W && vpSize[1] == H);
        printf("  test2 setWindowSize: %dx%d ok=%d\n", ws[0], ws[1], (int)ok);
        if (!ok) { printf("FAIL test2\n"); ++failures; }
    }

    // -----------------------------------------------------------------------
    // Test 3: setViewportRegion / getViewportRegion round-trip (sub-viewport)
    // -----------------------------------------------------------------------
    {
        SoViewport vp;
        SbViewportRegion sub;
        sub.setWindowSize((short)W, (short)H);
        sub.setViewportPixels(SbVec2s(10, 20), SbVec2s(100, 80));
        vp.setViewportRegion(sub);

        const SbViewportRegion & got = vp.getViewportRegion();
        SbVec2s orig = got.getViewportOriginPixels();
        SbVec2s size = got.getViewportSizePixels();
        bool ok = (orig[0] == 10 && orig[1] == 20 &&
                   size[0] == 100 && size[1] == 80);
        printf("  test3 sub-viewport: orig=(%d,%d) size=(%d,%d) ok=%d\n",
               orig[0], orig[1], size[0], size[1], (int)ok);
        if (!ok) { printf("FAIL test3\n"); ++failures; }
    }

    // -----------------------------------------------------------------------
    // Test 4: setCamera / getCamera round-trip
    // -----------------------------------------------------------------------
    {
        SoViewport vp;
        SoPerspectiveCamera * cam = new SoPerspectiveCamera;
        vp.setCamera(cam);
        bool ok = (vp.getCamera() == cam);
        vp.setCamera(nullptr);
        ok = ok && (vp.getCamera() == nullptr);
        printf("  test4 setCamera round-trip ok=%d\n", (int)ok);
        if (!ok) { printf("FAIL test4\n"); ++failures; }
    }

    // -----------------------------------------------------------------------
    // Test 5: setBackgroundColor / getBackgroundColor round-trip
    // -----------------------------------------------------------------------
    {
        SoViewport vp;
        SbColor col(0.1f, 0.5f, 0.9f);
        vp.setBackgroundColor(col);
        const SbColor & got = vp.getBackgroundColor();
        bool ok = (std::fabs(got[0] - col[0]) < 1e-4f &&
                   std::fabs(got[1] - col[1]) < 1e-4f &&
                   std::fabs(got[2] - col[2]) < 1e-4f);
        printf("  test5 background colour ok=%d\n", (int)ok);
        if (!ok) { printf("FAIL test5\n"); ++failures; }
    }

    // -----------------------------------------------------------------------
    // Test 6: render() produces a non-blank image
    // -----------------------------------------------------------------------
    {
        SoSeparator * geom = buildScene();

        SoViewport vp;
        vp.setWindowSize(SbVec2s((short)W, (short)H));
        vp.setSceneGraph(geom);

        SoPerspectiveCamera * cam = new SoPerspectiveCamera;
        cam->position.setValue(0.0f, 0.0f, 5.0f);
        vp.setCamera(cam);
        vp.viewAll();

        SoOffscreenRenderer * renderer =
            new SoOffscreenRenderer(getCoinHeadlessContextManager(),
                                    vp.getViewportRegion());
        renderer->setComponents(SoOffscreenRenderer::RGB);

        bool rendered = (vp.render(renderer) == TRUE);
        bool ok = rendered;
        if (rendered) {
            ok = validateNonBlack(renderer->getBuffer(), W * H, "test6_render");
            renderer->writeToRGB(outpath);
            printf("  test6: wrote %s\n", outpath);
        } else {
            printf("  test6 render FAILED\n");
        }

        delete renderer;
        geom->unref();
        if (!ok) { printf("FAIL test6\n"); ++failures; }
    }

    // -----------------------------------------------------------------------
    // Test 7: viewAll() adjusts camera — result is non-blank
    // -----------------------------------------------------------------------
    {
        SoSeparator * geom = buildScene();

        SoViewport vp;
        vp.setWindowSize(SbVec2s((short)W, (short)H));
        vp.setSceneGraph(geom);

        SoPerspectiveCamera * cam = new SoPerspectiveCamera;
        cam->position.setValue(0.0f, 0.0f, 100.0f);  // deliberately far away
        vp.setCamera(cam);
        vp.viewAll();  // should bring the scene into view

        SoOffscreenRenderer * renderer =
            new SoOffscreenRenderer(getCoinHeadlessContextManager(),
                                    vp.getViewportRegion());
        renderer->setComponents(SoOffscreenRenderer::RGB);

        bool rendered = (vp.render(renderer) == TRUE);
        bool ok = rendered &&
                  validateNonBlack(renderer->getBuffer(), W * H, "test7_viewall");
        printf("  test7 viewAll ok=%d\n", (int)ok);

        delete renderer;
        geom->unref();
        if (!ok) { printf("FAIL test7\n"); ++failures; }
    }

    // -----------------------------------------------------------------------
    // Test 8: processEvent() smoke test (no crash)
    // -----------------------------------------------------------------------
    {
        SoSeparator * geom = buildScene();
        SoViewport vp;
        vp.setWindowSize(SbVec2s((short)W, (short)H));
        vp.setSceneGraph(geom);

        SoPerspectiveCamera * cam = new SoPerspectiveCamera;
        cam->position.setValue(0.0f, 0.0f, 5.0f);
        vp.setCamera(cam);

        SoKeyboardEvent evt;
        evt.setKey(SoKeyboardEvent::ESCAPE);
        evt.setState(SoButtonEvent::DOWN);
        vp.processEvent(&evt);  // must not crash

        printf("  test8 processEvent smoke: PASS\n");
        geom->unref();
    }

    // -----------------------------------------------------------------------
    // Test 9: setSceneGraph(nullptr) clears the scene
    // -----------------------------------------------------------------------
    {
        SoSeparator * geom = buildScene();
        SoViewport vp;
        vp.setSceneGraph(geom);
        bool ok = (vp.getSceneGraph() == geom);
        vp.setSceneGraph(nullptr);
        ok = ok && (vp.getSceneGraph() == nullptr);
        printf("  test9 setSceneGraph(nullptr) ok=%d\n", (int)ok);
        geom->unref();
        if (!ok) { printf("FAIL test9\n"); ++failures; }
    }

    // -----------------------------------------------------------------------
    // Test 10: getRoot() returns non-null
    // -----------------------------------------------------------------------
    {
        SoViewport vp;
        bool ok = (vp.getRoot() != nullptr);
        printf("  test10 getRoot ok=%d\n", (int)ok);
        if (!ok) { printf("FAIL test10\n"); ++failures; }
    }

    // -----------------------------------------------------------------------
    // Test 11: setCamera(nullptr) removes camera without crash
    // -----------------------------------------------------------------------
    {
        SoViewport vp;
        SoPerspectiveCamera * cam = new SoPerspectiveCamera;
        vp.setCamera(cam);
        vp.setCamera(nullptr);
        bool ok = (vp.getCamera() == nullptr);
        printf("  test11 setCamera(nullptr) ok=%d\n", (int)ok);
        if (!ok) { printf("FAIL test11\n"); ++failures; }
    }

    // -----------------------------------------------------------------------
    printf("\n=== Summary: %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
