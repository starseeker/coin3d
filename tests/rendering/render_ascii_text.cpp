/*
 * render_ascii_text.cpp - Integration test: SoAsciiText rendering
 *
 * Renders SoAsciiText with string "HELLO" using an emissive white material
 * and verifies that non-black pixels appear in the rendered output.
 *
 * SoAsciiText is a 3D shape node (extruded text) so it needs a proper camera
 * setup and material.  A perspective camera with viewAll() centres the text.
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoAsciiText.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>

static const int W = 256;
static const int H = 256;

static bool validateAsciiText(const unsigned char * buf)
{
    int litFound = 0;

    // Scan the full buffer for any non-background pixels
    for (int y = 0; y < H; y += 2) {
        for (int x = 0; x < W; x += 2) {
            const unsigned char * p = buf + (y * W + x) * 3;
            if (p[0] > 80 || p[1] > 80 || p[2] > 80)
                ++litFound;
        }
    }

    printf("render_ascii_text: litFound=%d\n", litFound);

    if (litFound < 5) {
        fprintf(stderr, "render_ascii_text: FAIL - no lit pixels found for text\n");
        return false;
    }
    printf("render_ascii_text: PASS\n");
    return true;
}

int main(int argc, char ** argv)
{
    initCoinHeadless();

    SoSeparator * root = new SoSeparator;
    root->ref();

    // Perspective camera; viewAll() will be called after the scene is built
    SoPerspectiveCamera * cam = new SoPerspectiveCamera;
    root->addChild(cam);

    // Frontal directional light
    SoDirectionalLight * light = new SoDirectionalLight;
    light->direction.setValue(-0.3f, -0.5f, -0.8f);
    root->addChild(light);

    // Bright emissive + diffuse white material
    SoMaterial * mat = new SoMaterial;
    mat->diffuseColor .setValue(0.9f, 0.9f, 0.9f);
    mat->emissiveColor.setValue(0.5f, 0.5f, 0.5f);
    root->addChild(mat);

    SoAsciiText * text = new SoAsciiText;
    text->string.setValue("HELLO");
    text->justification.setValue(SoAsciiText::CENTER);
    root->addChild(text);

    // Auto-fit the camera to the scene bounding box
    SbViewportRegion vp(W, H);
    cam->viewAll(root, vp);

    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_ascii_text.rgb");

    bool ok = false;
    if (renderer.render(root)) {
        const unsigned char * buf = renderer.getBuffer();
        bool pixOk = (buf != nullptr) && validateAsciiText(buf);
        ok = pixOk && renderer.writeToRGB(outpath);
    } else {
        fprintf(stderr, "render_ascii_text: render() failed\n");
    }

    root->unref();
    return ok ? 0 : 1;
}
