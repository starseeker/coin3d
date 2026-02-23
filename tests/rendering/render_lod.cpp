/*
 * render_lod.cpp - Integration test: SoLOD level-of-detail switching
 *
 * Creates a scene with an SoLOD node containing three representations:
 *   LOD 0 (near,  d < 2):  high-res sphere (complex, many polygons)
 *   LOD 1 (mid,   d < 5):  cube
 *   LOD 2 (far,   d >= 5): empty separator (object disappears)
 *
 * The test renders the same scene three times from different camera distances
 * and validates pixel content:
 *
 *   Frame 1 (cam at z=1.5):  LOD 0 – sphere pixels visible
 *   Frame 2 (cam at z=3.5):  LOD 1 – cube pixels visible (different shape)
 *   Frame 3 (cam at z=8.0):  LOD 2 – scene blank (empty LOD)
 *
 * This exercises SoLOD, the SoGetBoundingBoxAction distance query used
 * internally, and the SoSwitch-derived visibility mechanism.
 *
 * Writes argv[1]+".rgb" (frame 1 image) and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoLOD.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>
#include <cmath>

static const int W = 256;
static const int H = 256;

// Count non-background pixels in the rendered buffer
static int countNonBackground(const unsigned char *buf)
{
    int count = 0;
    for (int i = 0; i < W * H; ++i) {
        const unsigned char *p = buf + i * 3;
        if (p[0] > 15 || p[1] > 15 || p[2] > 15) ++count;
    }
    return count;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = new SoSeparator;
    root->ref();

    // Camera: perspective, looking along -Z
    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position   .setValue(0.0f, 0.0f, 1.5f);
    cam->orientation.setValue(SbRotation::identity());
    cam->nearDistance = 0.1f;
    cam->farDistance  = 30.0f;
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.3f, -0.7f, -0.6f);
    root->addChild(light);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.7f, 0.3f, 0.8f);   // purple
    mat->specularColor.setValue(0.5f, 0.5f, 0.5f);
    mat->shininess   .setValue(0.4f);
    root->addChild(mat);

    // SoLOD: ranges [2, 5] → 3 levels
    SoLOD *lod = new SoLOD;
    lod->range.set1Value(0, 2.0f);
    lod->range.set1Value(1, 5.0f);
    // LOD 0: sphere (near)
    SoSphere *sph = new SoSphere;
    sph->radius = 0.8f;
    lod->addChild(sph);
    // LOD 1: cube (medium)
    SoCube *cube = new SoCube;
    cube->width .setValue(1.2f);
    cube->height.setValue(1.2f);
    cube->depth .setValue(1.2f);
    lod->addChild(cube);
    // LOD 2: empty (far)
    lod->addChild(new SoSeparator);
    root->addChild(lod);

    SbViewportRegion vp(W, H);
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    bool allOk = true;

    // --- Frame 1: near (d=1.5, LOD 0 – sphere) ---
    cam->position.setValue(0.0f, 0.0f, 1.5f);
    bool ok1 = false;
    if (renderer.render(root)) {
        int n = countNonBackground(renderer.getBuffer());
        printf("Frame1 (z=1.5, sphere): nonbg=%d\n", n);
        ok1 = n > 500;
        if (!ok1)
            fprintf(stderr, "render_lod: FAIL frame1 – expected sphere pixels\n");
    } else {
        fprintf(stderr, "render_lod: render() failed frame1\n");
    }
    allOk = allOk && ok1;

    // Save frame 1 as the output image
    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_lod.rgb");
    renderer.writeToRGB(outpath);

    // --- Frame 2: mid (d=3.5, LOD 1 – cube) ---
    cam->position.setValue(0.0f, 0.0f, 3.5f);
    bool ok2 = false;
    if (renderer.render(root)) {
        int n = countNonBackground(renderer.getBuffer());
        printf("Frame2 (z=3.5, cube): nonbg=%d\n", n);
        ok2 = n > 200;
        if (!ok2)
            fprintf(stderr, "render_lod: FAIL frame2 – expected cube pixels\n");
    } else {
        fprintf(stderr, "render_lod: render() failed frame2\n");
    }
    allOk = allOk && ok2;

    // --- Frame 3: far (d=8.0, LOD 2 – empty) ---
    cam->position.setValue(0.0f, 0.0f, 8.0f);
    bool ok3 = false;
    if (renderer.render(root)) {
        int n = countNonBackground(renderer.getBuffer());
        printf("Frame3 (z=8.0, empty): nonbg=%d\n", n);
        ok3 = n < 50;
        if (!ok3)
            fprintf(stderr, "render_lod: FAIL frame3 – expected blank scene\n");
    } else {
        fprintf(stderr, "render_lod: render() failed frame3\n");
    }
    allOk = allOk && ok3;

    if (allOk) printf("render_lod: PASS\n");

    root->unref();
    return allOk ? 0 : 1;
}
