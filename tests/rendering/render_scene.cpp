/*
 * render_scene.cpp - Renders a comprehensive multi-object 3D scene
 *
 * Creates a 2×2 grid of geometric primitives (sphere, cube, cone, cylinder)
 * each with a distinct diffuse colour, a single directional light and a
 * perspective camera.  This scene exercises basic geometry, materials and
 * lighting in a single pass.
 *
 * The scene is built by the shared testlib scene factory (createScene) so
 * the command-line test and the interactive obol_viewer show identical output.
 *
 * Writes argv[1]+".rgb" (SGI RGB format).
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <cstdio>

static const int W = 800;
static const int H = 600;

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = ObolTest::Scenes::createScene(W, H);

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_scene.rgb");

    bool ok = renderToFile(root, outpath, W, H);
    root->unref();
    return ok ? 0 : 1;
}
