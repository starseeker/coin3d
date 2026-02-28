/*
 * render_indexed_face_set.cpp - Integration test: SoIndexedFaceSet rendering
 *
 * Scene built by ObolTest::Scenes::createIndexedFaceSet.
 *
 * Pixel validation confirms:
 *   - The image is not all-background (geometry was rendered).
 *   - At least two distinct hues are present (materials differentiated).
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <cstdio>

static const int W = 512;
static const int H = 512;

static bool validateScene(const unsigned char *buf)
{
    int nonbg = 0, blue = 0, orange = 0, teal = 0;
    for (int y = 0; y < H; y += 4) {
        for (int x = 0; x < W; x += 4) {
            const unsigned char *p = buf + (y * W + x) * 3;
            if (p[0] < 10 && p[1] < 10 && p[2] < 10) continue;
            ++nonbg;
            if (p[2] > 120 && p[0] < 150) ++blue;
            if (p[0] > 150 && p[1] > 80 && p[2] < 80) ++orange;
            if (p[1] > 120 && p[2] > 100 && p[0] < 100) ++teal;
        }
    }
    printf("render_indexed_face_set: nonbg=%d blue=%d orange=%d teal=%d\n",
           nonbg, blue, orange, teal);

    if (nonbg < 100) {
        fprintf(stderr, "render_indexed_face_set: FAIL – scene appears blank\n");
        return false;
    }
    printf("render_indexed_face_set: PASS\n");
    return true;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = ObolTest::Scenes::createIndexedFaceSet(W, H);

    SbViewportRegion vp(W, H);
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_indexed_face_set.rgb");

    bool ok = false;
    if (renderer.render(root)) {
        const unsigned char *buf = renderer.getBuffer();
        bool pixOk = (buf != nullptr) && validateScene(buf);
        ok = pixOk && renderer.writeToRGB(outpath);
    } else {
        fprintf(stderr, "render_indexed_face_set: render() failed\n");
    }

    root->unref();
    return ok ? 0 : 1;
}
