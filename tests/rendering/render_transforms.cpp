/*
 * render_transforms.cpp - Visual regression test: SoTransform node types
 *
 * Renders a 3×3 grid of coloured cubes demonstrating the main transform
 * nodes.  The grid is arranged as follows (left→right, top→bottom):
 *
 *   Row 1 – Translation:
 *     [0,0] Origin (reference)    [1,0] +X translate   [2,0] +Y translate
 *   Row 2 – Rotation:
 *     [0,1] 45° X-axis            [1,1] 45° Y-axis      [2,1] 45° Z-axis
 *   Row 3 – Scale:
 *     [0,2] Uniform ×1.5          [1,2] Uniform ×0.5    [2,2] Non-uniform (1,2,0.5)
 *
 * Each cube is tinted differently so the viewer can quickly identify it.
 * The scene is lit with a single directional light from above-right.
 *
 * The executable writes argv[1]+".rgb" (SGI RGB format).
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoRotation.h>
#include <Inventor/nodes/SoScale.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/SbRotation.h>
#include <cstdio>

static const float COLORS[9][3] = {
    {0.9f,0.9f,0.9f},  // reference   – near-white
    {0.9f,0.5f,0.1f},  // +X          – orange
    {0.1f,0.8f,0.3f},  // +Y          – green
    {0.2f,0.4f,0.9f},  // rot X       – blue
    {0.8f,0.2f,0.8f},  // rot Y       – purple
    {0.9f,0.9f,0.1f},  // rot Z       – yellow
    {0.1f,0.8f,0.8f},  // scale ×1.5  – cyan
    {0.9f,0.3f,0.3f},  // scale ×0.5  – red
    {0.5f,0.7f,0.3f},  // non-uniform – olive
};

// Build one cell: translate the grid position, apply the extra transform, draw cube
static void addCell(SoSeparator *root,
                    int col, int row,   // grid position
                    int colorIdx,
                    SoNode *extra)      // extra transform (or nullptr)
{
    SoSeparator *sep = new SoSeparator;

    // Grid layout: 3.5 units spacing
    SoTranslation *grid = new SoTranslation;
    grid->translation.setValue((col - 1) * 3.5f, -(row - 1) * 3.5f, 0.0f);
    sep->addChild(grid);

    if (extra)
        sep->addChild(extra);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(COLORS[colorIdx][0],
                               COLORS[colorIdx][1],
                               COLORS[colorIdx][2]);
    mat->specularColor.setValue(0.5f, 0.5f, 0.5f);
    mat->shininess.setValue(0.4f);
    sep->addChild(mat);

    sep->addChild(new SoCube);
    root->addChild(sep);
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.5f, -0.7f, -0.5f);
    root->addChild(light);

    // Row 0: Translation
    addCell(root, 0, 0, 0, nullptr);                            // reference

    SoTranslation *tx = new SoTranslation; tx->translation.setValue(0.4f,0,0);
    addCell(root, 1, 0, 1, tx);                                 // +X

    SoTranslation *ty = new SoTranslation; ty->translation.setValue(0,0.4f,0);
    addCell(root, 2, 0, 2, ty);                                 // +Y

    // Row 1: Rotation (45 degrees around each axis)
    SoRotation *rx = new SoRotation;
    rx->rotation.setValue(SbVec3f(1,0,0), (float)(M_PI/4.0));
    addCell(root, 0, 1, 3, rx);

    SoRotation *ry = new SoRotation;
    ry->rotation.setValue(SbVec3f(0,1,0), (float)(M_PI/4.0));
    addCell(root, 1, 1, 4, ry);

    SoRotation *rz = new SoRotation;
    rz->rotation.setValue(SbVec3f(0,0,1), (float)(M_PI/4.0));
    addCell(root, 2, 1, 5, rz);

    // Row 2: Scale
    SoScale *su = new SoScale; su->scaleFactor.setValue(1.5f,1.5f,1.5f);
    addCell(root, 0, 2, 6, su);                                 // uniform ×1.5

    SoScale *sd = new SoScale; sd->scaleFactor.setValue(0.5f,0.5f,0.5f);
    addCell(root, 1, 2, 7, sd);                                 // uniform ×0.5

    SoScale *sn = new SoScale; sn->scaleFactor.setValue(1.0f,2.0f,0.5f);
    addCell(root, 2, 2, 8, sn);                                 // non-uniform

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    cam->viewAll(root, vp);
    cam->position.setValue(cam->position.getValue() * 1.15f);

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_transforms.rgb");

    bool ok = renderToFile(root, outpath);
    root->unref();
    return ok ? 0 : 1;
}
