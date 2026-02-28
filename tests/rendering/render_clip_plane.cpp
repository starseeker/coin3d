/*
 * render_clip_plane.cpp - Integration test: SoClipPlane geometry clipping
 *
 * Renders a large red sphere clipped in half by SoClipPlane.
 * The scene is built by the shared testlib factory (createClipPlane).
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoClipPlane.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbPlane.h>
#include <cstdio>
#include <cmath>

static const int W = 256;
static const int H = 256;

static bool validateClipPlane(const unsigned char *buf)
{
    // Sphere is red (emissive), background is very dark grey.
    // Upper half of the image (rows H/2..H-1 in OpenGL order, y=0=bottom)
    // should contain red sphere pixels.
    // Lower half (rows 0..H/2-1) should be background (dark).
    int redUpper    = 0;
    int darkLower   = 0;
    int totalUpper  = 0;
    int totalLower  = 0;

    // Sample the upper half (avoid the topmost row which may have edge artifacts)
    for (int y = H / 2 + 4; y < H - 4; y += 4) {
        for (int x = W / 4; x < 3 * W / 4; x += 4) {
            const unsigned char *p = buf + (y * W + x) * 3;
            if (p[0] > 150 && p[1] < 80 && p[2] < 80) ++redUpper;
            ++totalUpper;
        }
    }

    // Sample the lower half (strictly below the clip plane)
    for (int y = 4; y < H / 2 - 4; y += 4) {
        for (int x = W / 4; x < 3 * W / 4; x += 4) {
            const unsigned char *p = buf + (y * W + x) * 3;
            if (p[0] < 60 && p[1] < 60 && p[2] < 60) ++darkLower;
            ++totalLower;
        }
    }

    printf("render_clip_plane: redUpper=%d/%d  darkLower=%d/%d\n",
           redUpper, totalUpper, darkLower, totalLower);

    bool upperOk = (totalUpper > 0) && (redUpper * 10 > totalUpper);   // >10%
    bool lowerOk = (totalLower > 0) && (darkLower * 2 > totalLower);   // >50%

    if (!upperOk) {
        fprintf(stderr, "render_clip_plane: FAIL - expected red sphere in upper half\n");
        return false;
    }
    if (!lowerOk) {
        fprintf(stderr, "render_clip_plane: FAIL - expected dark background in lower half\n");
        return false;
    }
    printf("render_clip_plane: PASS\n");
    return true;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = ObolTest::Scenes::createClipPlane(W, H);

    SbViewportRegion vp(W, H);
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.02f, 0.02f, 0.02f));

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_clip_plane.rgb");

    bool ok = false;
    if (renderer.render(root)) {
        const unsigned char *buf = renderer.getBuffer();
        bool pixOk = (buf != nullptr) && validateClipPlane(buf);
        ok = pixOk && renderer.writeToRGB(outpath);
    } else {
        fprintf(stderr, "render_clip_plane: render() failed\n");
    }

    root->unref();
    return ok ? 0 : 1;
}
