/*
 * render_hud_no3d.cpp - Visual regression test: HUD overlay without 3D geometry.
 *
 * Scene built by ObolTest::Scenes::createHUDNo3D.
 * Viewport: 800 x 600
 * Background: very dark blue (0.05, 0.05, 0.10)
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

    SoSeparator *root = ObolTest::Scenes::createHUDNo3D(W, H);

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_hud_no3d.rgb");

    bool ok = renderToFile(root, outpath, W, H, SbColor(0.05f, 0.05f, 0.10f));
    root->unref();
    return ok ? 0 : 1;
}

