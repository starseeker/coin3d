/*
 * render_cameras.cpp - Visual regression test: camera projection types
 *
 * Renders the same scene twice and writes two output images so that the
 * difference between perspective and orthographic projection is clearly
 * visible:
 *
 *   argv[1]_perspective.rgb  –  SoPerspectiveCamera (default fov ~45°)
 *   argv[1]_orthographic.rgb –  SoOrthographicCamera
 *
 * The test scene is a row of three blue cubes at z = 0, -3, -6.  With a
 * perspective camera the cubes shrink noticeably in the distance; with an
 * orthographic camera all three cubes appear the same size.
 *
 * The primary output used by the CTest driver is argv[1]+".rgb" which is
 * the perspective render.
 *
 * The executable writes argv[1]+".rgb" (primary, perspective) and
 * argv[1]+"_ortho.rgb" (orthographic) in SGI RGB format.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <cstdio>

// Build a row of three cubes at different depths on the Z axis
static SoSeparator *buildScene()
{
    SoSeparator *scene = new SoSeparator;

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.4f, -0.8f, -0.5f);
    scene->addChild(light);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.2f, 0.4f, 0.9f);
    mat->specularColor.setValue(0.6f, 0.6f, 0.6f);
    mat->shininess.setValue(0.5f);
    scene->addChild(mat);

    float depths[3] = {0.0f, -3.5f, -7.0f};
    for (int i = 0; i < 3; i++) {
        SoSeparator *csep = new SoSeparator;
        SoTranslation *t = new SoTranslation;
        t->translation.setValue(0.0f, 0.0f, depths[i]);
        csep->addChild(t);
        csep->addChild(new SoCube);
        scene->addChild(csep);
    }
    return scene;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    const char *base = (argc > 1) ? argv[1] : "render_cameras";

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);

    // --- Perspective render (primary output: base+".rgb") ---
    {
        SoSeparator *root = new SoSeparator;
        root->ref();

        SoPerspectiveCamera *cam = new SoPerspectiveCamera;
        root->addChild(cam);
        root->addChild(buildScene());

        // viewAll sets up correct near/far clipping planes automatically
        SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
        cam->viewAll(root, vp);
        // Look slightly from above to show the depth arrangement
        cam->position.setValue(cam->position.getValue() + SbVec3f(0.0f, 3.0f, 0.0f));
        cam->pointAt(SbVec3f(0.0f, 0.0f, -3.5f), SbVec3f(0.0f, 1.0f, 0.0f));
        // Re-adjust near/far after repositioning
        SoGetBoundingBoxAction bba(vp);
        bba.apply(root);
        SbBox3f bbox = bba.getBoundingBox();
        float diag = (bbox.getMax() - bbox.getMin()).length();
        cam->nearDistance.setValue(0.1f);
        cam->farDistance.setValue(diag * 4.0f);

        char outpath[1024];
        snprintf(outpath, sizeof(outpath), "%s.rgb", base);
        renderToFile(root, outpath);
        root->unref();
    }

    // --- Orthographic render (secondary: base+"_ortho.rgb") ---
    {
        SoSeparator *root = new SoSeparator;
        root->ref();

        SoOrthographicCamera *cam = new SoOrthographicCamera;
        root->addChild(cam);
        root->addChild(buildScene());

        SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
        cam->viewAll(root, vp);
        cam->position.setValue(cam->position.getValue() + SbVec3f(0.0f, 3.0f, 0.0f));
        cam->pointAt(SbVec3f(0.0f, 0.0f, -3.5f), SbVec3f(0.0f, 1.0f, 0.0f));
        SoGetBoundingBoxAction bba2(vp);
        bba2.apply(root);
        SbBox3f bbox2 = bba2.getBoundingBox();
        float diag2 = (bbox2.getMax() - bbox2.getMin()).length();
        cam->nearDistance.setValue(-diag2);
        cam->farDistance.setValue(diag2 * 4.0f);

        char outpath[1024];
        snprintf(outpath, sizeof(outpath), "%s_ortho.rgb", base);
        renderToFile(root, outpath);
        root->unref();
    }

    return 0;
}
