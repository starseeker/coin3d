/*
 * render_vertex_colors.cpp - Integration test: per-vertex colours via SoPackedColor
 *
 * Creates a quad with four distinctly coloured corners.
 * The scene is built by the shared testlib factory (createVertexColors).
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoPackedColor.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoNormal.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/nodes/SoMaterialBinding.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>
#include <cmath>

static const int W = 256;
static const int H = 256;
static const int MARGIN = 8;   // pixels from corners to sample

// Check that a pixel near (px,py) matches the expected RGB within tolerance.
static bool checkCorner(const unsigned char *buf, int px, int py,
                        unsigned char er, unsigned char eg, unsigned char eb,
                        int tol = 70, const char *label = "")
{
    if (px < 0 || px >= W || py < 0 || py >= H) return false;
    const unsigned char *p = buf + (py * W + px) * 3;
    bool ok = (std::abs((int)p[0] - (int)er) <= tol &&
               std::abs((int)p[1] - (int)eg) <= tol &&
               std::abs((int)p[2] - (int)eb) <= tol);
    printf("  Corner %-12s px=(%d,%d) got=(%3d,%3d,%3d) exp=(%3d,%3d,%3d) %s\n",
           label, px, py,
           (int)p[0], (int)p[1], (int)p[2],
           (int)er, (int)eg, (int)eb,
           ok ? "OK" : "FAIL");
    return ok;
}

static bool validateVertexColors(const unsigned char *buf)
{
    // OpenGL row order: y=0=bottom.
    // Bottom-left:  Red
    bool bl = checkCorner(buf, MARGIN,     MARGIN,     200,   0,   0, 80, "BottomLeft");
    // Bottom-right: Green
    bool br = checkCorner(buf, W-MARGIN-1, MARGIN,       0, 200,   0, 80, "BottomRight");
    // Top-right:    Blue
    bool tr = checkCorner(buf, W-MARGIN-1, H-MARGIN-1,   0,   0, 200, 80, "TopRight");
    // Top-left:     Yellow
    bool tl = checkCorner(buf, MARGIN,     H-MARGIN-1, 200, 200,   0, 80, "TopLeft");

    if (bl && br && tr && tl) {
        printf("render_vertex_colors: PASS\n");
        return true;
    }
    fprintf(stderr, "render_vertex_colors: FAIL - corner colour mismatch\n");
    return false;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = ObolTest::Scenes::createVertexColors(W, H);

    SbViewportRegion vpr(W, H);
    SoOffscreenRenderer renderer(vpr);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.5f, 0.5f, 0.5f));

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_vertex_colors.rgb");

    bool ok = false;
    if (renderer.render(root)) {
        const unsigned char *buf = renderer.getBuffer();
        bool pixOk = (buf != nullptr) && validateVertexColors(buf);
        ok = pixOk && renderer.writeToRGB(outpath);
    } else {
        fprintf(stderr, "render_vertex_colors: render() failed\n");
    }

    root->unref();
    return ok ? 0 : 1;
}
