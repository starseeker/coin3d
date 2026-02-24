/*
 * render_annotation.cpp - Integration test: SoAnnotation node rendering
 *
 * Renders a scene where a SoAnnotation contains a small sphere.
 * The annotation renders on top regardless of depth; this test just
 * verifies the SoAnnotation code path executes without crash and
 * produces non-black pixels in the output.
 *
 * Exercises SoAnnotation, SoSeparator, SoMaterial, SoSphere,
 * SoOrthographicCamera, and the annotation rendering path.
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoAnnotation.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>

static const int W = 256;
static const int H = 256;

// Count non-black pixels in the centre of the image.
static bool validateAnnotation(const unsigned char *buf)
{
    int nonBlack = 0;
    int cx = W / 2, cy = H / 2;
    int radius = W / 4;

    for (int y = cy - radius; y <= cy + radius; y += 2) {
        for (int x = cx - radius; x <= cx + radius; x += 2) {
            if (x < 0 || x >= W || y < 0 || y >= H) continue;
            const unsigned char *p = buf + (y * W + x) * 3;
            if (p[0] > 20 || p[1] > 20 || p[2] > 20)
                ++nonBlack;
        }
    }

    printf("render_annotation: nonBlack=%d\n", nonBlack);
    if (nonBlack < 5) {
        fprintf(stderr, "render_annotation: FAIL - no visible pixels from annotation\n");
        return false;
    }
    printf("render_annotation: PASS\n");
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

    // Light
    root->addChild(new SoDirectionalLight);

    // Background sphere (far back)
    {
        SoSeparator *grp = new SoSeparator;
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.3f, 0.3f, 0.8f);
        grp->addChild(mat);
        SoTranslation *tr = new SoTranslation;
        tr->translation.setValue(0, 0, -2.0f);
        grp->addChild(tr);
        SoSphere *sph = new SoSphere;
        sph->radius = 0.5f;
        grp->addChild(sph);
        root->addChild(grp);
    }

    // Annotation node with a red sphere at origin (should render on top)
    {
        SoAnnotation *ann = new SoAnnotation;
        SoMaterial *mat = new SoMaterial;
        mat->emissiveColor.setValue(1.0f, 0.2f, 0.2f);
        ann->addChild(mat);
        SoSphere *sph = new SoSphere;
        sph->radius = 0.4f;
        ann->addChild(sph);
        root->addChild(ann);
    }

    SbViewportRegion vp(W, H);
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_annotation.rgb");

    bool ok = false;
    if (renderer.render(root)) {
        const unsigned char *buf = renderer.getBuffer();
        bool pixOk = (buf != nullptr) && validateAnnotation(buf);
        ok = pixOk && renderer.writeToRGB(outpath);
    } else {
        fprintf(stderr, "render_annotation: render() failed\n");
    }

    root->unref();
    return ok ? 0 : 1;
}
