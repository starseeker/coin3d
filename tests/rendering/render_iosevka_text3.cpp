/*
 * render_iosevka_text3.cpp - Visual regression test: SoText3 extruded 3D text with Iosevka Aile font
 *
 * Scene built by ObolTest::Scenes::createIosevkaText3.
 * Verifies that struetype correctly loads the Iosevka Aile TTC font and that
 * SoText3 generates proper 3-D extruded glyph geometry (not ProFont fallback).
 *
 * Viewport: 800 x 600
 * Output: argv[1]+".rgb" (SGI RGB format)
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <cstdio>

static const int W = 800;
static const int H = 600;

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = ObolTest::Scenes::createIosevkaText3(W, H);

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_iosevka_text3.rgb");

    bool ok = renderToFile(root, outpath, W, H);
    root->unref();
    return ok ? 0 : 1;
}
