/*
 * render_quad_mesh.cpp - Integration test: SoQuadMesh with per-vertex materials
 *
 * Creates a 5×5 grid of faces using SoQuadMesh.
 * The scene is built by the shared testlib factory (createQuadMesh).
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoMaterialBinding.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoNormal.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/nodes/SoQuadMesh.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>
#include <cmath>

static const int W = 400;
static const int H = 400;

static bool validateQuadMesh(const unsigned char *buf)
{
    // Left 20% of image should be reddish; right 20% should be bluish.
    int leftRed = 0, rightBlue = 0, nonbg = 0;
    for (int y = H / 4; y < 3 * H / 4; y += 4) {
        // Sample left strip
        for (int x = W / 16; x < W / 5; x += 4) {
            const unsigned char *p = buf + (y * W + x) * 3;
            if (p[0] < 10 && p[1] < 10 && p[2] < 10) continue;
            ++nonbg;
            if (p[0] > p[2] + 30) ++leftRed;
        }
        // Sample right strip
        for (int x = 4 * W / 5; x < 15 * W / 16; x += 4) {
            const unsigned char *p = buf + (y * W + x) * 3;
            if (p[0] < 10 && p[1] < 10 && p[2] < 10) continue;
            ++nonbg;
            if (p[2] > p[0] + 30) ++rightBlue;
        }
    }
    printf("render_quad_mesh: nonbg=%d leftRed=%d rightBlue=%d\n",
           nonbg, leftRed, rightBlue);

    if (nonbg < 20) {
        fprintf(stderr, "render_quad_mesh: FAIL – scene appears blank\n");
        return false;
    }
    if (leftRed < 3) {
        fprintf(stderr, "render_quad_mesh: FAIL – left side should be reddish\n");
        return false;
    }
    if (rightBlue < 3) {
        fprintf(stderr, "render_quad_mesh: FAIL – right side should be bluish\n");
        return false;
    }
    printf("render_quad_mesh: PASS\n");
    return true;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = ObolTest::Scenes::createQuadMesh(W, H);

    SbViewportRegion vp(W, H);

    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_quad_mesh.rgb");

    bool ok = false;
    if (renderer.render(root)) {
        const unsigned char *buf = renderer.getBuffer();
        bool pixOk = (buf != nullptr) && validateQuadMesh(buf);
        ok = pixOk && renderer.writeToRGB(outpath);
    } else {
        fprintf(stderr, "render_quad_mesh: render() failed\n");
    }

    root->unref();
    return ok ? 0 : 1;
}
