/*
 * render_gradient.cpp - Renders a horizontal colour gradient (red → green)
 *
 * Creates a scene with strips of emissive geometry producing a colour gradient
 * from pure red on the left edge to pure green on the right edge.  Useful as a
 * sanity-check that colour output and coordinate mapping are working correctly.
 *
 * Writes argv[1]+".rgb" (SGI RGB format).
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <cstdio>

static SoSeparator *createGradientScene()
{
    SoSeparator *root = new SoSeparator;

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0, 0, 1);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 10.0f;
    cam->height       = 2.0f;   // world Y: -1 to +1
    root->addChild(cam);

    const int   nStrips    = 10;
    const float stripWidth = 2.0f / nStrips;

    for (int i = 0; i < nStrips; ++i) {
        float t   = (float)i / (nStrips - 1);   // 0 → 1
        float x0  = -1.0f + i * stripWidth;
        float x1  = x0 + stripWidth;

        SoSeparator *strip = new SoSeparator;

        SoMaterial *mat = new SoMaterial;
        mat->emissiveColor.setValue(1.0f - t, t, 0.0f);
        mat->diffuseColor .setValue(0.0f, 0.0f, 0.0f);
        strip->addChild(mat);

        SoCoordinate3 *coords = new SoCoordinate3;
        coords->point.set1Value(0, SbVec3f(x0, -1.0f, 0.0f));
        coords->point.set1Value(1, SbVec3f(x1, -1.0f, 0.0f));
        coords->point.set1Value(2, SbVec3f(x1,  1.0f, 0.0f));
        coords->point.set1Value(3, SbVec3f(x0,  1.0f, 0.0f));
        strip->addChild(coords);

        SoIndexedFaceSet *face = new SoIndexedFaceSet;
        face->coordIndex.set1Value(0, 0);
        face->coordIndex.set1Value(1, 1);
        face->coordIndex.set1Value(2, 2);
        face->coordIndex.set1Value(3, 3);
        face->coordIndex.set1Value(4, -1);
        strip->addChild(face);

        root->addChild(strip);
    }

    return root;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = createGradientScene();
    root->ref();

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_gradient.rgb");

    bool ok = renderToFile(root, outpath,
                           DEFAULT_WIDTH, DEFAULT_HEIGHT,
                           SbColor(0.0f, 0.0f, 0.0f));
    root->unref();
    return ok ? 0 : 1;
}
