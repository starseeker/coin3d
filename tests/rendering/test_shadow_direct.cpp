#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/annex/FXViz/nodes/SoShadowGroup.h>
#include <Inventor/annex/FXViz/nodes/SoShadowStyle.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>

int main() {
    initCoinHeadless();
    printf("Init OK\n");

    // Build a simple scene with SoShadowGroup
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 3.0f, 5.0f);
    root->addChild(cam);
    printf("Camera OK\n");

    SoShadowGroup *sg = new SoShadowGroup;
    printf("ShadowGroup created\n");
    sg->isActive.setValue(TRUE);
    root->addChild(sg);
    printf("ShadowGroup added\n");

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.8f, 0.2f, 0.2f);
    sg->addChild(mat);

    SoSphere *sph = new SoSphere;
    sph->radius.setValue(0.6f);
    sg->addChild(sph);
    printf("Scene built OK\n");

    SbViewportRegion vp(64, 64);
    cam->viewAll(root, vp);

    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
    printf("About to render...\n");
    fflush(stdout);

    bool ok = renderer.render(root);
    printf("Render result: %d\n", (int)ok);

    root->unref();
    return ok ? 0 : 1;
}
