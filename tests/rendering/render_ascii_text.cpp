/*
 * render_ascii_text.cpp - Integration test: SoAsciiText rendering
 *
 * Renders SoAsciiText with string "HELLO".
 * The scene is built by the shared testlib factory (createAsciiText).
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoAsciiText.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>

static const int W = 256;
static const int H = 256;

static bool validateAsciiText(const unsigned char * buf)
{
    int litFound = 0;

    // Scan the full buffer for any non-background pixels
    for (int y = 0; y < H; y += 2) {
        for (int x = 0; x < W; x += 2) {
            const unsigned char * p = buf + (y * W + x) * 3;
            if (p[0] > 80 || p[1] > 80 || p[2] > 80)
                ++litFound;
        }
    }

    printf("render_ascii_text: litFound=%d\n", litFound);

    if (litFound < 5) {
        fprintf(stderr, "render_ascii_text: FAIL - no lit pixels found for text\n");
        return false;
    }
    printf("render_ascii_text: PASS\n");
    return true;
}

int main(int argc, char ** argv)
{
    initCoinHeadless();

    SoSeparator * root = ObolTest::Scenes::createAsciiText(W, H);

    SbViewportRegion vp(W, H);

    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_ascii_text.rgb");

    bool ok = false;
    if (renderer.render(root)) {
        const unsigned char * buf = renderer.getBuffer();
        bool pixOk = (buf != nullptr) && validateAsciiText(buf);
        ok = pixOk && renderer.writeToRGB(outpath);
    } else {
        fprintf(stderr, "render_ascii_text: render() failed\n");
    }

    root->unref();
    return ok ? 0 : 1;
}
