/*
 * render_indexed_line_set.cpp - Integration test: SoIndexedLineSet polylines
 *
 * Renders three polyline paths using SoIndexedLineSet.
 * The scene is built by the shared testlib factory (createIndexedLineSet).
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoIndexedLineSet.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>
#include <cmath>

static const int W = 256;
static const int H = 256;

static bool validateIndexedLineSet(const unsigned char *buf)
{
    // Green horizontal line is at world Y ≈ +0.5.
    // Camera height = 2, so world +0.5 → pixel row H*(0.5 + 0.5/2) = H*0.75 = 192
    // (OpenGL row 0 = bottom)
    int greenFound = 0, redFound = 0;

    int greenRow = (int)(H * (0.5f + 0.5f / 2.0f));

    // Scan a band ±6 rows around the expected green line row
    for (int y = greenRow - 6; y <= greenRow + 6; ++y) {
        if (y < 0 || y >= H) continue;
        for (int x = W / 8; x < 7 * W / 8; x += 4) {
            const unsigned char *p = buf + (y * W + x) * 3;
            if (p[1] > 150 && p[0] < 80 && p[2] < 80) ++greenFound;
        }
    }

    // Red diagonal: somewhere in the middle band
    for (int y = H / 4; y < 3 * H / 4; y += 4) {
        for (int x = W / 4; x < 3 * W / 4; x += 4) {
            const unsigned char *p = buf + (y * W + x) * 3;
            if (p[0] > 150 && p[1] < 80 && p[2] < 80) ++redFound;
        }
    }

    printf("render_indexed_line_set: greenFound=%d redFound=%d\n",
           greenFound, redFound);

    if (greenFound < 3) {
        fprintf(stderr, "render_indexed_line_set: FAIL – green line not found\n");
        return false;
    }
    if (redFound < 3) {
        fprintf(stderr, "render_indexed_line_set: FAIL – red diagonal not found\n");
        return false;
    }
    printf("render_indexed_line_set: PASS\n");
    return true;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = ObolTest::Scenes::createIndexedLineSet(W, H);

    SbViewportRegion vp(W, H);
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_indexed_line_set.rgb");

    bool ok = false;
    if (renderer.render(root)) {
        const unsigned char *buf = renderer.getBuffer();
        bool pixOk = (buf != nullptr) && validateIndexedLineSet(buf);
        ok = pixOk && renderer.writeToRGB(outpath);
    } else {
        fprintf(stderr, "render_indexed_line_set: render() failed\n");
    }

    root->unref();
    return ok ? 0 : 1;
}
