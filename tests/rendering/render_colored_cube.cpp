/*
 * render_colored_cube.cpp - Renders a single coloured cube with lighting
 *
 * Simple scene: perspective camera, directional light, red diffuse cube.
 * Primarily a smoke-test for basic geometry + material + lighting rendering.
 *
 * Writes argv[1]+".rgb" (SGI RGB format).
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoCube.h>
#include <cstdio>

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-1.0f, -1.0f, -1.0f);
    root->addChild(light);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor .setValue(0.85f, 0.10f, 0.10f);
    mat->specularColor.setValue(0.50f, 0.50f, 0.50f);
    mat->shininess    .setValue(0.40f);
    root->addChild(mat);

    root->addChild(new SoCube);

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    cam->viewAll(root, vp);

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_colored_cube.rgb");

    bool ok = renderToFile(root, outpath);
    root->unref();
    return ok ? 0 : 1;
}
