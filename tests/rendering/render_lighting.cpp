/*
 * render_lighting.cpp - Visual regression test: light source types
 *
 * Renders three white spheres on a dark grey plane, each illuminated by a
 * different light source type.  The deliberate positioning makes the light
 * character clearly visible:
 *
 *   Left:   SoDirectionalLight  – uniform illumination from above-right
 *   Centre: SoPointLight        – positioned directly above, falls off with distance
 *   Right:  SoSpotLight         – tight cone aimed at the sphere
 *
 * A weak ambient component is added so the unlit sides are not pure black.
 *
 * The executable writes argv[1]+".rgb" (SGI RGB format).
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoPointLight.h>
#include <Inventor/nodes/SoSpotLight.h>
#include <Inventor/nodes/SoLightModel.h>
#include <Inventor/nodes/SoEnvironment.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoSphere.h>
#include <cstdio>

// Build one column: a positioned light + a white sphere beneath it
static void addColumn(SoSeparator *root, float cx, SoNode *light)
{
    SoSeparator *col = new SoSeparator;

    // Light (already positioned/configured by caller)
    col->addChild(light);

    // White sphere – radius 1.2 for better visibility
    SoSeparator *sphereSep = new SoSeparator;
    SoTranslation *t = new SoTranslation;
    t->translation.setValue(cx, 0.0f, 0.0f);
    sphereSep->addChild(t);
    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.95f, 0.95f, 0.95f);
    mat->specularColor.setValue(1.0f, 1.0f, 1.0f);
    mat->shininess.setValue(0.6f);
    sphereSep->addChild(mat);
    SoSphere *sph = new SoSphere;
    sph->radius.setValue(1.2f);
    sphereSep->addChild(sph);
    col->addChild(sphereSep);

    root->addChild(col);
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = new SoSeparator;
    root->ref();

    // Camera: slightly elevated to show the top of the spheres
    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 4.0f, 12.0f);
    cam->pointAt(SbVec3f(0.0f, 0.0f, 0.0f), SbVec3f(0.0f, 1.0f, 0.0f));
    root->addChild(cam);

    // Very dim environment light so dark sides stay visible
    SoEnvironment *env = new SoEnvironment;
    env->ambientIntensity.setValue(0.08f);
    env->ambientColor.setValue(SbColor(1.0f, 1.0f, 1.0f));
    root->addChild(env);

    // No default light – each column provides its own
    SoLightModel *lm = new SoLightModel;
    lm->model.setValue(SoLightModel::PHONG);
    root->addChild(lm);

    // Column spacing
    const float sp = 3.5f;

    // Left column: directional light
    {
        SoDirectionalLight *dl = new SoDirectionalLight;
        dl->direction.setValue(-0.4f, -0.9f, -0.3f);
        dl->intensity.setValue(1.0f);
        dl->color.setValue(SbColor(1.0f, 1.0f, 1.0f));
        addColumn(root, -sp, dl);
    }

    // Centre column: point light above the sphere
    {
        SoPointLight *pl = new SoPointLight;
        pl->location.setValue(0.0f, 3.5f, 0.0f);
        pl->intensity.setValue(1.0f);
        pl->color.setValue(SbColor(1.0f, 1.0f, 1.0f));
        addColumn(root, 0.0f, pl);
    }

    // Right column: spot light with tight cutoff
    {
        SoSpotLight *sl = new SoSpotLight;
        sl->location.setValue(sp, 4.0f, 1.5f);
        sl->direction.setValue(-0.3f, -0.9f, -0.3f);
        sl->cutOffAngle.setValue(0.35f);   // ~20 degrees
        sl->dropOffRate.setValue(0.6f);
        sl->intensity.setValue(1.0f);
        sl->color.setValue(SbColor(1.0f, 1.0f, 1.0f));
        addColumn(root, sp, sl);
    }

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    cam->viewAll(root, vp);
    cam->position.setValue(cam->position.getValue() * 1.1f);

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_lighting.rgb");

    bool ok = renderToFile(root, outpath);
    root->unref();
    return ok ? 0 : 1;
}
