/*
 * render_hud_overlay.cpp - Visual regression test: HUD overlay over a 3D scene.
 *
 * Scene built by ObolTest::Scenes::createHUDOverlay.
 * Viewport: 800 x 600
 * Output: argv[1]+".rgb"
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <cstdio>

static const int W = 800;
static const int H = 600;

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = ObolTest::Scenes::createHUDOverlay(W, H);

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_hud_overlay.rgb");

    bool ok = renderToFile(root, outpath, W, H, SbColor(0.08f, 0.08f, 0.12f));
    root->unref();
    return ok ? 0 : 1;
}

