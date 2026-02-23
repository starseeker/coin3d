/*
 * render_scene.cpp - Renders a comprehensive multi-object 3D scene
 *
 * Creates a 2×2 grid of geometric primitives (sphere, cube, cone, cylinder)
 * each with a distinct diffuse colour, a single directional light and a
 * perspective camera.  This scene exercises basic geometry, materials and
 * lighting in a single pass.
 *
 * Writes argv[1]+".rgb" (SGI RGB format).
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCylinder.h>
#include <cstdio>

static void addObject(SoSeparator *root,
                      float r, float g, float b,
                      float tx, float ty,
                      SoNode *shape)
{
    SoSeparator *sep = new SoSeparator;

    SoTranslation *t = new SoTranslation;
    t->translation.setValue(tx, ty, 0);
    sep->addChild(t);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor .setValue(r, g, b);
    mat->specularColor.setValue(0.4f, 0.4f, 0.4f);
    mat->shininess    .setValue(0.3f);
    sep->addChild(mat);

    sep->addChild(shape);
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
    light->direction.setValue(-0.5f, -0.8f, -0.6f);
    root->addChild(light);

    // 2×2 grid with spacing s; each object centred at ±s/2 in X and Y
    const float s = 2.0f;
    addObject(root, 0.85f, 0.15f, 0.15f, -s * 0.5f,  s * 0.5f, new SoSphere);
    addObject(root, 0.15f, 0.75f, 0.15f,  s * 0.5f,  s * 0.5f, new SoCube);
    addObject(root, 0.15f, 0.35f, 0.90f, -s * 0.5f, -s * 0.5f, new SoCone);
    addObject(root, 0.90f, 0.75f, 0.15f,  s * 0.5f, -s * 0.5f, new SoCylinder);

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    cam->viewAll(root, vp);
    // Pull the camera back slightly so shapes have breathing room
    cam->position.setValue(cam->position.getValue() * 1.2f);

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_scene.rgb");

    bool ok = renderToFile(root, outpath);
    root->unref();
    return ok ? 0 : 1;
}
