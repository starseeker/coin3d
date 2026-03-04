/*
 * render_sorender_manager.cpp - Integration test: SoRenderManager API
 *
 * Uses ObolTest::Scenes::createSoRenderManager — the same factory used by
 * obol_viewer — so CLI and viewer render identical scenes.
 *
 * Tests SoRenderManager API round-trips:
 *   - setAutoClipping / getAutoClipping round-trip
 *   - setBackgroundColor / getBackgroundColor round-trip
 *   - getGLRenderAction returns non-null
 *   - setViewportRegion does not crash
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <Inventor/SoRenderManager.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/SbColor4f.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>
#include <cmath>

static const int W = 256;
static const int H = 256;

int main(int argc, char ** argv)
{
    initCoinHeadless();

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_sorender_manager.rgb");

    bool ok = true;

    /* Use the shared factory so viewer and CLI render the same scene. */
    SoSeparator *root = ObolTest::Scenes::createSoRenderManager(W, H);

    SbViewportRegion vp(W, H);

    /* SoRenderManager API round-trip validation */
    SoRenderManager *mgr = new SoRenderManager;
    mgr->setViewportRegion(vp);
    mgr->setSceneGraph(root);

    mgr->setAutoClipping(SoRenderManager::VARIABLE_NEAR_PLANE);
    if (mgr->getAutoClipping() != SoRenderManager::VARIABLE_NEAR_PLANE) {
        fprintf(stderr, "render_sorender_manager: FAIL - autoClipping round-trip\n");
        ok = false;
    }

    SbColor4f bgColor(0.1f, 0.2f, 0.3f, 1.0f);
    mgr->setBackgroundColor(bgColor);
    const SbColor4f &gotBg = mgr->getBackgroundColor();
    if (std::fabs(gotBg[0] - bgColor[0]) > 1e-5f ||
        std::fabs(gotBg[1] - bgColor[1]) > 1e-5f ||
        std::fabs(gotBg[2] - bgColor[2]) > 1e-5f) {
        fprintf(stderr, "render_sorender_manager: FAIL - background color round-trip\n");
        ok = false;
    }

    if (mgr->getGLRenderAction() == nullptr) {
        fprintf(stderr, "render_sorender_manager: FAIL - getGLRenderAction() null\n");
        ok = false;
    }

    /* Delete the manager before rendering: the scene graph is ref-counted and
     * will not be freed by the manager's destructor.  Rendering is done through
     * a separate SoOffscreenRenderer so we can produce a portable .rgb file. */
    delete mgr;

    /* Render to file via SoOffscreenRenderer (matches viewer output). */
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.1f, 0.2f, 0.3f));

    if (!renderer.render(root)) {
        fprintf(stderr, "render_sorender_manager: render() failed\n");
        ok = false;
    } else {
        renderer.writeToRGB(outpath);
    }

    root->unref();
    if (ok) printf("render_sorender_manager: PASS\n");
    return ok ? 0 : 1;
}
