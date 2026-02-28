/*
 * render_checker_texture.cpp - Validates black-and-white checkerboard texturing
 *
 * Creates a pure black-and-white checkerboard texture applied to a cube.
 * The scene is built by the shared testlib factory (createCheckerTexture).
 *
 * Writes argv[1]+".rgb" (SGI RGB format) and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/nodes/SoTextureCoordinateDefault.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>

static const int W = 512;
static const int H = 512;

// Validate that the centre 50% of the RGBA buffer contains both black and
// white pixels in a ratio that confirms checkerboard texture is visible.
static bool validateCheckerPixels(const unsigned char *buf)
{
    int black = 0, white = 0, other = 0;

    for (int y = H / 4; y < 3 * H / 4; y += 8) {
        for (int x = W / 4; x < 3 * W / 4; x += 8) {
            const unsigned char *p = buf + (y * W + x) * 4;   // RGBA
            if (p[0] < 32  && p[1] < 32  && p[2] < 32)  black++;
            else if (p[0] > 220 && p[1] > 220 && p[2] > 220) white++;
            else                                               other++;
        }
    }

    int total = black + white + other;
    printf("Checker pixel counts: black=%d  white=%d  other=%d  total=%d\n",
           black, white, other, total);

    bool ok = total > 0 &&
              (black > total / 10) &&
              (white > total / 10) &&
              (other < total / 2);
    if (!ok)
        fprintf(stderr, "render_checker_texture: expected roughly equal "
                        "black+white pixels in centre region\n");
    else
        printf("render_checker_texture: PASS\n");
    return ok;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = ObolTest::Scenes::createCheckerTexture(W, H);

    SbViewportRegion vp(W, H);

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_checker_texture.rgb");

    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB_TRANSPARENCY);
    renderer.setBackgroundColor(SbColor(0.2f, 0.3f, 0.4f));

    bool ok = false;
    if (renderer.render(root)) {
        const unsigned char *buf = renderer.getBuffer();
        bool pixOk = (buf != nullptr) && validateCheckerPixels(buf);
        ok = pixOk && renderer.writeToRGB(outpath);
    } else {
        fprintf(stderr, "render_checker_texture: render() failed\n");
    }

    root->unref();
    return ok ? 0 : 1;
}
