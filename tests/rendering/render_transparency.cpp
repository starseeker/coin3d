/*
 * render_transparency.cpp - Integration test: SoTransparencyType rendering
 *
 * Scene built by ObolTest::Scenes::createTransparency.
 *
 * Pixel validation confirms that the semi-transparent sphere is visible
 * (non-background pixels are present in the viewport centre).
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <cstdio>

static const int W = 256;
static const int H = 256;

static bool validateTransparency(const unsigned char *buf)
{
    int visiblePixels = 0;

    int cx = W / 2, cy = H / 2;
    int radius = W / 4;

    for (int y = cy - radius; y <= cy + radius; y += 2) {
        for (int x = cx - radius; x <= cx + radius; x += 2) {
            if (x < 0 || x >= W || y < 0 || y >= H) continue;
            const unsigned char *p = buf + (y * W + x) * 3;
            if (p[0] > 20 || p[1] > 20 || p[2] > 20)
                ++visiblePixels;
        }
    }

    printf("render_transparency: visiblePixels=%d\n", visiblePixels);
    if (visiblePixels < 5) {
        fprintf(stderr, "render_transparency: FAIL - no visible pixels (transparency scene empty)\n");
        return false;
    }
    printf("render_transparency: PASS\n");
    return true;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = ObolTest::Scenes::createTransparency(W, H);

    SbViewportRegion vp(W, H);
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_transparency.rgb");

    bool ok = false;
    if (renderer.render(root)) {
        const unsigned char *buf = renderer.getBuffer();
        bool pixOk = (buf != nullptr) && validateTransparency(buf);
        ok = pixOk && renderer.writeToRGB(outpath);
    } else {
        fprintf(stderr, "render_transparency: render() failed\n");
    }

    root->unref();
    return ok ? 0 : 1;
}
