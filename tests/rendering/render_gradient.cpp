/*
 * render_gradient.cpp - Visual regression test: colour gradient scene
 *
 * Scene built by ObolTest::Scenes::createGradient.
 * Gradient background applied via SoOffscreenRenderer::setBackgroundGradient()
 * so it works with all rendering backends (System GL, OSMesa, NanoRT).
 * Viewport: 800 x 600
 * Output: argv[1]+".rgb" (SGI RGB format)
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <Inventor/SoOffscreenRenderer.h>
#include <cstdio>

static const int W = 800;
static const int H = 600;

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = ObolTest::Scenes::createGradient(W, H);

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_gradient.rgb");

    SoOffscreenRenderer *renderer = getSharedRenderer();
    SbViewportRegion viewport(W, H);
    renderer->setViewportRegion(viewport);
    renderer->setComponents(SoOffscreenRenderer::RGB);
    renderer->setBackgroundGradient(SbColor(0.05f, 0.05f, 0.20f),
                                    SbColor(0.20f, 0.35f, 0.60f));

    bool ok = renderer->render(root);
    if (ok)
        ok = renderer->writeToRGB(outpath);

    renderer->clearBackgroundGradient();
    root->unref();
    return ok ? 0 : 1;
}
