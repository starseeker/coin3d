/*
 * render_array_multiple_copy.cpp - Integration test: SoArray and SoMultipleCopy
 *
 * Tests SoArray (3×3 grid) and SoMultipleCopy (3 explicit positions).
 * The scene is built by the shared testlib factory (createArrayMultipleCopy).
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoArray.h>
#include <Inventor/nodes/SoMultipleCopy.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbMatrix.h>
#include <cstdio>

static const int W = 512;
static const int H = 512;

static bool validateScene(const unsigned char *buf)
{
    int nonbg = 0, blue = 0, orange = 0;
    for (int y = 0; y < H; y += 4) {
        for (int x = 0; x < W; x += 4) {
            const unsigned char *p = buf + (y * W + x) * 3;
            if (p[0] < 10 && p[1] < 10 && p[2] < 10) continue;
            ++nonbg;
            if (p[2] > 100 && p[0] < 120) ++blue;
            if (p[0] > 150 && p[1] > 80 && p[2] < 80) ++orange;
        }
    }
    printf("render_array_multiple_copy: nonbg=%d blue=%d orange=%d\n",
           nonbg, blue, orange);

    if (nonbg < 50) {
        fprintf(stderr, "render_array_multiple_copy: FAIL – scene appears blank\n");
        return false;
    }
    printf("render_array_multiple_copy: PASS\n");
    return true;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = ObolTest::Scenes::createArrayMultipleCopy(W, H);

    SbViewportRegion vp(W, H);

    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_array_multiple_copy.rgb");

    bool ok = false;
    if (renderer.render(root)) {
        const unsigned char *buf = renderer.getBuffer();
        bool pixOk = (buf != nullptr) && validateScene(buf);
        ok = pixOk && renderer.writeToRGB(outpath);
    } else {
        fprintf(stderr, "render_array_multiple_copy: render() failed\n");
    }

    root->unref();
    return ok ? 0 : 1;
}
