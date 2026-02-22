/*
 * render_texture.cpp - Visual regression test: SoTexture2 texture mapping
 *
 * Renders two spheres side-by-side to demonstrate SoTexture2 texture upload:
 *
 *   Left:   Untextured white sphere (reference)
 *   Right:  Sphere with a procedural 8×8 checkerboard texture (red/white)
 *
 * The checker pattern and colour contrast make it immediately obvious whether
 * texture mapping is working.  The texture is generated in-memory so no
 * external image files are needed.
 *
 * The executable writes argv[1]+".rgb" (SGI RGB format).
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/nodes/SoTextureCoordinateDefault.h>
#include <cstdio>
#include <cstring>

// Checker tile colors
static const unsigned char CHECKER_RED[3]   = { 220, 40,  40  };
static const unsigned char CHECKER_WHITE[3] = { 255, 255, 255 };

// Build a red/white checkerboard texture image (8×8 tiles, 16 pixels each side)
static void buildCheckerTexture(SoTexture2 *tex)
{
    const int TILE = 8;             // pixels per checker tile
    const int SIZE = TILE * 8;     // 64 × 64 image
    const int NC   = 3;             // RGB
    unsigned char buf[SIZE * SIZE * NC];

    for (int y = 0; y < SIZE; y++) {
        for (int x = 0; x < SIZE; x++) {
            int tx = x / TILE;
            int ty = y / TILE;
            int idx = (y * SIZE + x) * NC;
            const unsigned char *color = ((tx + ty) % 2 == 0) ? CHECKER_RED : CHECKER_WHITE;
            buf[idx]   = color[0];
            buf[idx+1] = color[1];
            buf[idx+2] = color[2];
        }
    }

    tex->image.setValue(SbVec2s(SIZE, SIZE), NC, buf);
    tex->model.setValue(SoTexture2::MODULATE);
    tex->wrapS.setValue(SoTexture2::REPEAT);
    tex->wrapT.setValue(SoTexture2::REPEAT);
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

    // Left: plain white sphere (reference – no texture)
    {
        SoSeparator *left = new SoSeparator;
        SoTranslation *t = new SoTranslation;
        t->translation.setValue(-1.8f, 0.0f, 0.0f);
        left->addChild(t);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(1.0f, 1.0f, 1.0f);
        left->addChild(mat);
        SoSphere *sph = new SoSphere;
        sph->radius.setValue(1.3f);
        left->addChild(sph);
        root->addChild(left);
    }

    // Right: textured sphere with red/white checkerboard
    {
        SoSeparator *right = new SoSeparator;
        SoTranslation *t = new SoTranslation;
        t->translation.setValue(1.8f, 0.0f, 0.0f);
        right->addChild(t);

        SoTexture2 *tex = new SoTexture2;
        buildCheckerTexture(tex);
        right->addChild(tex);

        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(1.0f, 1.0f, 1.0f);
        right->addChild(mat);

        SoSphere *sph = new SoSphere;
        sph->radius.setValue(1.3f);
        right->addChild(sph);
        root->addChild(right);
    }

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    cam->viewAll(root, vp);
    cam->position.setValue(cam->position.getValue() * 1.1f);

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_texture.rgb");

    bool ok = renderToFile(root, outpath);
    root->unref();
    return ok ? 0 : 1;
}
