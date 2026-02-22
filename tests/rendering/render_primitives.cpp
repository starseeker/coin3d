/*
 * render_primitives.cpp - Visual regression test: basic geometric primitives
 *
 * Renders a 2×2 grid of the four built-in Coin primitives, each in a
 * distinct colour, with a single directional light.  This scene makes it
 * immediately obvious to a human reviewer whether basic geometry rendering
 * and material colouring are working correctly.
 *
 *   Top-left:     Red   SoSphere
 *   Top-right:    Green SoCube
 *   Bottom-left:  Blue  SoCone
 *   Bottom-right: Gold  SoCylinder
 *
 * The executable writes argv[1]+".rgb" (SGI RGB format) suitable for
 * comparison with generate_controls.sh.
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
#include <cstring>

// Helper: add a coloured primitive at the given XY offset
static void addPrimitive(SoSeparator *root,
                         float r, float g, float b,
                         float tx, float ty,
                         SoNode *shape)
{
    SoSeparator *sep = new SoSeparator;

    SoTranslation *t = new SoTranslation;
    t->translation.setValue(tx, ty, 0.0f);
    sep->addChild(t);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(r, g, b);
    mat->specularColor.setValue(0.6f, 0.6f, 0.6f);
    mat->shininess.setValue(0.5f);
    sep->addChild(mat);

    sep->addChild(shape);
    root->addChild(sep);
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = new SoSeparator;
    root->ref();

    // Camera
    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    root->addChild(cam);

    // Single directional light from slightly above-right
    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.5f, -0.8f, -0.6f);
    root->addChild(light);

    // 2×2 grid of primitives, spacing = 2.5 units
    const float s = 2.5f;
    addPrimitive(root, 0.85f, 0.15f, 0.15f, -s*0.5f,  s*0.5f, new SoSphere);    // red   sphere (TL)
    addPrimitive(root, 0.15f, 0.75f, 0.15f,  s*0.5f,  s*0.5f, new SoCube);      // green cube   (TR)
    addPrimitive(root, 0.15f, 0.35f, 0.90f, -s*0.5f, -s*0.5f, new SoCone);      // blue  cone   (BL)
    addPrimitive(root, 0.90f, 0.75f, 0.15f,  s*0.5f, -s*0.5f, new SoCylinder);  // gold  cyl    (BR)

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    cam->viewAll(root, vp);
    // Pull back a little so shapes have breathing room
    cam->position.setValue(cam->position.getValue() * 1.1f);

    // Output path: argv[1] (no extension) → write argv[1]+".rgb"
    char outpath[1024];
    if (argc > 1) {
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    } else {
        snprintf(outpath, sizeof(outpath), "render_primitives.rgb");
    }

    bool ok = renderToFile(root, outpath);
    root->unref();
    return ok ? 0 : 1;
}
