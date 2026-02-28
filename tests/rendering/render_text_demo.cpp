/*
 * render_text_demo.cpp  –  Visual regression test: Rendering/text demo scene
 *
 * Renders exactly the scene shown in obol_viewer under "Rendering/text":
 *   - SoText3 "Obol" (3-D extruded orange text, ALL parts)
 *   - SoText2 "3D Test" (2-D green screen-space label)
 *
 * The scene is built by the shared testlib scene factory (createText) so
 * the command-line test and the interactive obol_viewer show identical output.
 *
 * Viewport: 800 x 600
 * Output: argv[1]+".rgb" (SGI RGB, used by image-comparison infrastructure)
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <cstdio>

static const int W = 800;
static const int H = 600;

int main(int argc, char** argv)
{
    initCoinHeadless();

    SoSeparator* root = ObolTest::Scenes::createText(W, H);

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_text_demo.rgb");

    bool ok = renderToFile(root, outpath, W, H);
    root->unref();
    return ok ? 0 : 1;
}
