/*
 * render_image_node.cpp - Integration test: SoImage node rendering
 *
 * Creates an SoImage node with a 32×32 checkerboard of red/green pixels.
 * The scene is built by the shared testlib factory (createImageNode).
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoImage.h>
#include <Inventor/fields/SoSFImage.h>
#include <Inventor/SbVec2s.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>

static const int W = 256;
static const int H = 256;

static bool validateImage(const unsigned char * buf)
{
    int redFound = 0, greenFound = 0;

    // SoImage renders at the current raster position at its native pixel size.
    // Scan the full buffer for red and green pixels.
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const unsigned char * p = buf + (y * W + x) * 3;
            if (p[0] > 150 && p[1] < 50  && p[2] < 50)  ++redFound;
            if (p[1] > 150 && p[0] < 50  && p[2] < 50)  ++greenFound;
        }
    }

    printf("render_image_node: redFound=%d greenFound=%d\n", redFound, greenFound);

    if (redFound < 4 || greenFound < 4) {
        fprintf(stderr,
                "render_image_node: FAIL - expected both red and green pixels "
                "(red=%d, green=%d)\n", redFound, greenFound);
        return false;
    }
    printf("render_image_node: PASS\n");
    return true;
}

int main(int argc, char ** argv)
{
    initCoinHeadless();

    SoSeparator * root = ObolTest::Scenes::createImageNode(W, H);

    SbViewportRegion vp(W, H);
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.2f, 0.2f, 0.2f));

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_image_node.rgb");

    bool ok = false;
    if (renderer.render(root)) {
        const unsigned char * buf = renderer.getBuffer();
        bool pixOk = (buf != nullptr) && validateImage(buf);
        ok = pixOk && renderer.writeToRGB(outpath);
    } else {
        fprintf(stderr, "render_image_node: render() failed\n");
    }

    root->unref();
    return ok ? 0 : 1;
}
