/*
 * render_transparency.cpp - Integration test: SoTransparencyType node rendering
 *
 * Renders two spheres: an opaque blue sphere in the background and a
 * semi-transparent red sphere in the foreground.  The test verifies that
 * the red sphere renders as a non-black object (i.e., it is visible).
 *
 * Exercises SoTransparencyType, SoMaterial, SoSphere, SoOrthographicCamera,
 * SoDirectionalLight and the transparency rendering path.
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTransparencyType.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>

static const int W = 256;
static const int H = 256;

static bool validateTransparency(const unsigned char *buf)
{
    // Look for non-black pixels in the centre — the semi-transparent sphere
    // must produce visible output.
    int visiblePixels = 0;

    int cx = W / 2, cy = H / 2;
    int radius = W / 4;

    for (int y = cy - radius; y <= cy + radius; y += 2) {
        for (int x = cx - radius; x <= cx + radius; x += 2) {
            if (x < 0 || x >= W || y < 0 || y >= H) continue;
            const unsigned char *p = buf + (y * W + x) * 3;
            if (p[0] > 20 || p[1] > 20 || p[2] > 20)
                ++visiblePixels;
        }
    }

    printf("render_transparency: visiblePixels=%d\n", visiblePixels);
    if (visiblePixels < 5) {
        fprintf(stderr, "render_transparency: FAIL - no visible pixels (transparency scene empty)\n");
        return false;
    }
    printf("render_transparency: PASS\n");
    return true;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = new SoSeparator;
    root->ref();

    // Camera
    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0, 0, 5);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 20.0f;
    cam->height       = 4.0f;
    root->addChild(cam);

    root->addChild(new SoDirectionalLight);

    // Set transparency type to BLEND
    SoTransparencyType *tt = new SoTransparencyType;
    tt->value.setValue(SoTransparencyType::BLEND);
    root->addChild(tt);

    // Opaque blue sphere at z=-2 (background)
    {
        SoSeparator *grp = new SoSeparator;
        SoTranslation *tr = new SoTranslation;
        tr->translation.setValue(0, 0, -2.0f);
        grp->addChild(tr);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.0f, 0.3f, 1.0f);
        mat->transparency.setValue(0.0f);
        grp->addChild(mat);
        SoSphere *sph = new SoSphere;
        sph->radius = 0.8f;
        grp->addChild(sph);
        root->addChild(grp);
    }

    // Semi-transparent red sphere at origin (foreground)
    {
        SoSeparator *grp = new SoSeparator;
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(1.0f, 0.1f, 0.1f);
        mat->transparency.setValue(0.5f); // 50% transparent
        grp->addChild(mat);
        SoSphere *sph = new SoSphere;
        sph->radius = 0.6f;
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
        snprintf(outpath, sizeof(outpath), "render_transparency.rgb");

    bool ok = false;
    if (renderer.render(root)) {
        const unsigned char *buf = renderer.getBuffer();
        bool pixOk = (buf != nullptr) && validateTransparency(buf);
        ok = pixOk && renderer.writeToRGB(outpath);
    } else {
        fprintf(stderr, "render_transparency: render() failed\n");
    }

    root->unref();
    return ok ? 0 : 1;
}
