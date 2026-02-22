/*
 * render_materials.cpp - Visual regression test: material properties
 *
 * Renders five spheres in a row with progressively different material
 * properties.  The scene is designed so that shininess and emission
 * differences are readily visible to a human reviewer.
 *
 *   Sphere 1 (left):    Matte red       – shininess 0.0, no emission
 *   Sphere 2:           Semi-shiny red  – shininess 0.3
 *   Sphere 3 (centre):  Shiny red       – shininess 0.7
 *   Sphere 4:           Mirror red      – shininess 1.0
 *   Sphere 5 (right):   Emissive yellow – shininess 0.0, strong emission
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
#include <cstdio>

static void addSphere(SoSeparator *root,
                      float dr, float dg, float db,   // diffuse
                      float er, float eg, float eb,   // emissive
                      float sr, float sg, float sb,   // specular
                      float shininess,
                      float tx)
{
    SoSeparator *sep = new SoSeparator;

    SoTranslation *t = new SoTranslation;
    t->translation.setValue(tx, 0.0f, 0.0f);
    sep->addChild(t);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(dr, dg, db);
    mat->emissiveColor.setValue(er, eg, eb);
    mat->specularColor.setValue(sr, sg, sb);
    mat->shininess.setValue(shininess);
    sep->addChild(mat);

    SoSphere *sph = new SoSphere;
    sph->radius.setValue(0.9f);   // slightly larger for better visibility
    sep->addChild(sph);
    root->addChild(sep);
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    root->addChild(cam);

    // Directional light from upper-right
    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.6f, -0.8f, -0.5f);
    root->addChild(light);

    // Five spheres spaced 2.5 units apart (centred at origin)
    const float sp = 2.5f;
    const float start = -2.0f * sp;

    // 1. Matte red
    addSphere(root, 0.85f,0.12f,0.12f,  0,0,0,  0,0,0,  0.0f, start + 0*sp);
    // 2. Semi-shiny red
    addSphere(root, 0.85f,0.12f,0.12f,  0,0,0,  0.6f,0.6f,0.6f, 0.3f, start + 1*sp);
    // 3. Shiny red
    addSphere(root, 0.85f,0.12f,0.12f,  0,0,0,  0.8f,0.8f,0.8f, 0.7f, start + 2*sp);
    // 4. Mirror red
    addSphere(root, 0.85f,0.12f,0.12f,  0,0,0,  1.0f,1.0f,1.0f, 1.0f, start + 3*sp);
    // 5. Emissive yellow
    addSphere(root, 0.7f,0.7f,0.0f,  0.7f,0.7f,0.0f,  0,0,0, 0.0f, start + 4*sp);

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    cam->viewAll(root, vp);
    cam->position.setValue(cam->position.getValue() * 1.1f);

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_materials.rgb");

    bool ok = renderToFile(root, outpath);
    root->unref();
    return ok ? 0 : 1;
}
