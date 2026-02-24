/*
 * render_image_node.cpp - Integration test: SoImage node rendering
 *
 * Creates an SoImage node with a 32×32 checkerboard of red/green pixels.
 * Renders and verifies that both red and green pixel values are present
 * in the output buffer.
 *
 * The 32×32 image is large enough that sampling at step 2 within the image
 * region reliably hits both colors of the checkerboard.
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoImage.h>
#include <Inventor/fields/SoSFImage.h>
#include <Inventor/SbVec2s.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>

static const int W = 256;
static const int H = 256;

// Red/green checkerboard: easy to distinguish (avoids blue vs black confusion)
static const int IMG_W = 32, IMG_H = 32;

static bool validateImage(const unsigned char * buf)
{
    int redFound = 0, greenFound = 0;

    // SoImage renders at the current raster position at its native pixel size.
    // Scan the full buffer for red and green pixels.
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const unsigned char * p = buf + (y * W + x) * 3;
            if (p[0] > 150 && p[1] < 50  && p[2] < 50)  ++redFound;
            if (p[1] > 150 && p[0] < 50  && p[2] < 50)  ++greenFound;
        }
    }

    printf("render_image_node: redFound=%d greenFound=%d\n", redFound, greenFound);

    if (redFound < 4 || greenFound < 4) {
        fprintf(stderr,
                "render_image_node: FAIL - expected both red and green pixels "
                "(red=%d, green=%d)\n", redFound, greenFound);
        return false;
    }
    printf("render_image_node: PASS\n");
    return true;
}

int main(int argc, char ** argv)
{
    initCoinHeadless();

    // Build checkerboard pixel data (32×32, RGB, red/green alternating)
    unsigned char pixels[IMG_W * IMG_H * 3];
    for (int row = 0; row < IMG_H; ++row) {
        for (int col = 0; col < IMG_W; ++col) {
            unsigned char * p = pixels + (row * IMG_W + col) * 3;
            if ((row + col) % 2 == 0) {
                p[0] = 255; p[1] = 0; p[2] = 0;    // red
            } else {
                p[0] = 0;   p[1] = 255; p[2] = 0;  // green
            }
        }
    }

    SoSeparator * root = new SoSeparator;
    root->ref();

    SoOrthographicCamera * cam = new SoOrthographicCamera;
    cam->position    .setValue(0.0f, 0.0f, 1.0f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 10.0f;
    cam->height       = 2.0f;
    root->addChild(cam);

    SoImage * img = new SoImage;
    img->image.setValue(SbVec2s(IMG_W, IMG_H), 3, pixels);
    root->addChild(img);

    SbViewportRegion vp(W, H);
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.2f, 0.2f, 0.2f));

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_image_node.rgb");

    bool ok = false;
    if (renderer.render(root)) {
        const unsigned char * buf = renderer.getBuffer();
        bool pixOk = (buf != nullptr) && validateImage(buf);
        ok = pixOk && renderer.writeToRGB(outpath);
    } else {
        fprintf(stderr, "render_image_node: render() failed\n");
    }

    root->unref();
    return ok ? 0 : 1;
}
