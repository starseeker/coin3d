/*
 * render_quad_mesh.cpp - Integration test: SoQuadMesh with per-vertex materials
 *
 * Creates a 5×5 grid of faces using SoQuadMesh.  Each column is coloured
 * differently (red → orange → yellow → green → blue) using PER_VERTEX material
 * binding via SoMaterial with multiple diffuseColor values.  This exercises:
 *   SoQuadMesh, SoCoordinate3, SoMaterial (multi-value), SoMaterialBinding,
 *   SoNormal, SoNormalBinding (PER_VERTEX)
 *
 * Pixel validation confirms that:
 *   - The image is not blank.
 *   - Left and right halves contain different dominant hues (red vs. blue).
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
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
// Grid dimensions
static const int NCOLS = 5;
static const int NROWS = 5;

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

    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(0.0f, -0.5f, -1.0f);
    root->addChild(light);

    // Per-vertex material: 5 columns × 5 rows = 25 vertices
    // Colours cycle across columns: red → orange → yellow → green → blue
    SoMaterialBinding *mb = new SoMaterialBinding;
    mb->value.setValue(SoMaterialBinding::PER_VERTEX);
    root->addChild(mb);

    static const float colColors[NCOLS][3] = {
        { 0.9f, 0.1f, 0.1f },   // red
        { 0.9f, 0.5f, 0.1f },   // orange
        { 0.9f, 0.9f, 0.1f },   // yellow
        { 0.1f, 0.8f, 0.1f },   // green
        { 0.1f, 0.2f, 0.9f },   // blue
    };

    SoMaterial *mat = new SoMaterial;
    int mi = 0;
    for (int r = 0; r < NROWS; ++r) {
        for (int c = 0; c < NCOLS; ++c) {
            mat->diffuseColor.set1Value(mi++,
                SbColor(colColors[c][0], colColors[c][1], colColors[c][2]));
        }
    }
    root->addChild(mat);

    // Build grid coordinates (world XY plane, Z=0)
    float xStep = 2.0f / (NCOLS - 1);
    float yStep = 2.0f / (NROWS - 1);
    SoCoordinate3 *coords = new SoCoordinate3;
    int vi = 0;
    for (int r = 0; r < NROWS; ++r) {
        for (int c = 0; c < NCOLS; ++c) {
            float x = -1.0f + c * xStep;
            float y = -1.0f + r * yStep;
            coords->point.set1Value(vi++, SbVec3f(x, y, 0.0f));
        }
    }
    root->addChild(coords);

    // Normals – all facing camera (+Z)
    SoNormal *normals = new SoNormal;
    for (int i = 0; i < NCOLS * NROWS; ++i)
        normals->vector.set1Value(i, SbVec3f(0.0f, 0.0f, 1.0f));
    root->addChild(normals);
    SoNormalBinding *nb = new SoNormalBinding;
    nb->value.setValue(SoNormalBinding::PER_VERTEX);
    root->addChild(nb);

    // SoQuadMesh: NCOLS vertices per row, NROWS rows
    SoQuadMesh *qm = new SoQuadMesh;
    qm->verticesPerRow.setValue(NCOLS);
    qm->verticesPerColumn.setValue(NROWS);
    root->addChild(qm);

    SbViewportRegion vp(W, H);
    cam->viewAll(root, vp);

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
