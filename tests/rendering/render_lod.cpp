/*
 * render_lod.cpp - Integration test: SoLOD level-of-detail switching
 *
 * Scene built by ObolTest::Scenes::createLOD.
 * Viewport: 256 x 256
 * Output: argv[1]+".rgb" (SGI RGB format)
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <cstdio>

static const int W = 256;
static const int H = 256;

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = ObolTest::Scenes::createLOD(W, H);

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_lod.rgb");

    bool ok = renderToFile(root, outpath, W, H);
    root->unref();
    return ok ? 0 : 1;
}
