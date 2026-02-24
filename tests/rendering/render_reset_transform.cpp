/*
 * render_reset_transform.cpp - Integration test: SoResetTransform node
 *
 * Renders two spheres:
 *   1. A blue sphere translated to the right via SoTranslation
 *   2. A red sphere that uses SoResetTransform to clear the transform,
 *      rendering at the origin.
 *
 * Pixel validation checks that:
 *   - There are red pixels near centre (reset sphere at origin)
 *   - There are blue pixels in the right half of the image (translated sphere)
 *
 * Exercises SoResetTransform, SoTranslation, SoMaterial, SoSphere,
 * SoOrthographicCamera, and the transform-reset rendering path.
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoResetTransform.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>
#include <cmath>

static const int W = 512;
static const int H = 256;

static bool validateResetTransform(const unsigned char *buf)
{
    int redCentre = 0;   // red sphere at origin (screen centre)
    int blueRight = 0;   // blue sphere translated right (right of centre)

    // Red sphere is at world origin, which maps to screen x=W/2 (+/-50 px)
    for (int y = H / 4; y < 3 * H / 4; y += 2) {
        for (int x = W / 2 - 60; x < W / 2 + 60; x += 2) {
            if (x < 0 || x >= W) continue;
            const unsigned char *p = buf + (y * W + x) * 3;
            if (p[0] > 150 && p[1] < 80 && p[2] < 80)
                ++redCentre;
        }
    }

    // Blue sphere is at x=+1.5 world units. World width = height * aspect = 4 * 2 = 8
    // x=1.5 → screen_x = W/2 + (1.5/4) * W/2 = 256 + 0.375 * 256 ≈ 352
    for (int y = H / 4; y < 3 * H / 4; y += 2) {
        for (int x = W / 2 + 40; x < W - 40; x += 2) {
            if (x < 0 || x >= W) continue;
            const unsigned char *p = buf + (y * W + x) * 3;
            if (p[2] > 100 && p[0] < 80)
                ++blueRight;
        }
    }

    printf("render_reset_transform: redCentre=%d blueRight=%d\n", redCentre, blueRight);

    if (redCentre < 3) {
        fprintf(stderr, "render_reset_transform: FAIL - not enough red pixels at centre\n");
        return false;
    }
    if (blueRight < 3) {
        fprintf(stderr, "render_reset_transform: FAIL - not enough blue pixels at right\n");
        return false;
    }
    printf("render_reset_transform: PASS\n");
    return true;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = new SoSeparator;
    root->ref();

    // Wide orthographic camera: world width = 8, height = 4
    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0, 0, 5);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 20.0f;
    cam->height       = 4.0f;
    cam->aspectRatio  = (float)W / (float)H;
    root->addChild(cam);

    root->addChild(new SoDirectionalLight);

    // Translate right by 1.5 world units, then draw a blue sphere
    {
        SoSeparator *grp = new SoSeparator;
        SoTranslation *tr = new SoTranslation;
        tr->translation.setValue(1.5f, 0.0f, 0.0f);
        grp->addChild(tr);

        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.0f, 0.3f, 1.0f);
        grp->addChild(mat);

        SoSphere *sph = new SoSphere;
        sph->radius = 0.5f;
        grp->addChild(sph);

        root->addChild(grp);
    }

    // Same translation context, then RESET transform, draw a red sphere at origin
    {
        SoSeparator *grp = new SoSeparator;
        SoTranslation *tr = new SoTranslation;
        tr->translation.setValue(1.5f, 0.0f, 0.0f); // this will be cleared
        grp->addChild(tr);

        SoResetTransform *rst = new SoResetTransform;
        rst->whatToReset.setValue(SoResetTransform::TRANSFORM);
        grp->addChild(rst);

        SoMaterial *mat = new SoMaterial;
        mat->emissiveColor.setValue(1.0f, 0.1f, 0.1f);
        mat->diffuseColor.setValue(0.0f, 0.0f, 0.0f);
        grp->addChild(mat);

        SoSphere *sph = new SoSphere;
        sph->radius = 0.5f;
        grp->addChild(sph);

        root->addChild(grp);
    }

    SbViewportRegion vp(W, H);
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_reset_transform.rgb");

    bool ok = false;
    if (renderer.render(root)) {
        const unsigned char *buf = renderer.getBuffer();
        bool pixOk = (buf != nullptr) && validateResetTransform(buf);
        ok = pixOk && renderer.writeToRGB(outpath);
    } else {
        fprintf(stderr, "render_reset_transform: render() failed\n");
    }

    root->unref();
    return ok ? 0 : 1;
}
