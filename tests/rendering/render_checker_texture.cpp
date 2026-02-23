/*
 * render_checker_texture.cpp - Validates black-and-white checkerboard texturing
 *
 * Creates a pure black-and-white checkerboard texture, applies it to a cube
 * and renders the scene.  The centre 50 % of the rendered image is sampled to
 * confirm it contains both near-black and near-white pixels in roughly equal
 * proportions, verifying that texture mapping is working correctly.
 *
 * Writes argv[1]+".rgb" (SGI RGB format) and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/nodes/SoTextureCoordinateDefault.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>
#include <vector>

static const int W = 512;
static const int H = 512;

static void makeCheckerData(int tw, int th, std::vector<unsigned char> &data)
{
    data.resize(tw * th * 3);
    const int cs = 32;   // checker square size in texels
    for (int y = 0; y < th; ++y) {
        for (int x = 0; x < tw; ++x) {
            unsigned char v = (((x / cs) + (y / cs)) % 2) ? 255 : 0;
            data[(y * tw + x) * 3 + 0] = v;
            data[(y * tw + x) * 3 + 1] = v;
            data[(y * tw + x) * 3 + 2] = v;
        }
    }
}

// Validate that the centre 50% of the RGBA buffer contains both black and
// white pixels in a ratio that confirms checkerboard texture is visible.
static bool validateCheckerPixels(const unsigned char *buf)
{
    int black = 0, white = 0, other = 0;

    for (int y = H / 4; y < 3 * H / 4; y += 8) {
        for (int x = W / 4; x < 3 * W / 4; x += 8) {
            const unsigned char *p = buf + (y * W + x) * 4;   // RGBA
            if (p[0] < 32  && p[1] < 32  && p[2] < 32)  black++;
            else if (p[0] > 220 && p[1] > 220 && p[2] > 220) white++;
            else                                               other++;
        }
    }

    int total = black + white + other;
    printf("Checker pixel counts: black=%d  white=%d  other=%d  total=%d\n",
           black, white, other, total);

    bool ok = total > 0 &&
              (black > total / 10) &&
              (white > total / 10) &&
              (other < total / 2);
    if (!ok)
        fprintf(stderr, "render_checker_texture: expected roughly equal "
                        "black+white pixels in centre region\n");
    else
        printf("render_checker_texture: PASS\n");
    return ok;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor .setValue(1, 1, 1);
    mat->ambientColor .setValue(0.2f, 0.2f, 0.2f);
    root->addChild(mat);

    const int tw = 128, th = 128;
    std::vector<unsigned char> texData;
    makeCheckerData(tw, th, texData);

    SoTexture2 *tex = new SoTexture2;
    tex->ref();
    tex->setImageData(tw, th, 3, texData.data());
    root->addChild(tex);
    root->addChild(new SoTextureCoordinateDefault);
    root->addChild(new SoCube);

    SbViewportRegion vp(W, H);
    cam->viewAll(root, vp);

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_checker_texture.rgb");

    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB_TRANSPARENCY);
    renderer.setBackgroundColor(SbColor(0.2f, 0.3f, 0.4f));

    bool ok = false;
    if (renderer.render(root)) {
        const unsigned char *buf = renderer.getBuffer();
        bool pixOk = (buf != nullptr) && validateCheckerPixels(buf);
        ok = pixOk && renderer.writeToRGB(outpath);
    } else {
        fprintf(stderr, "render_checker_texture: render() failed\n");
    }

    tex->unref();
    root->unref();
    return ok ? 0 : 1;
}
