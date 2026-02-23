/*
 * render_texture3.cpp - Integration test: SoTexture3 (3-D texture) on a cube
 *
 * Builds a procedural 8×8×8 RGBA 3-D texture with alternating colored voxels
 * and applies it to a cube via SoTexture3.  SoTexture3 uses 3-D texture
 * coordinates; the default coordinate generation maps object-space position
 * to [0,1] in s/t/r.
 *
 * Pixel validation: the rendered cube must be non-blank.
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoTexture3.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/fields/SoSFImage3.h>
#include <cstdio>
#include <cstring>

static const int W = 256;
static const int H = 256;

static int countNonBackground(const unsigned char *buf)
{
    int count = 0;
    for (int i = 0; i < W * H; ++i) {
        const unsigned char *p = buf + i * 3;
        if (p[0] > 15 || p[1] > 15 || p[2] > 15) ++count;
    }
    return count;
}

// Build a small 8×8×8 RGBA 3-D texture: alternating red/blue voxels
static void buildTexture3(SoTexture3 *tex)
{
    const int S = 8;
    const int NC = 4;  // RGBA
    unsigned char buf[S * S * S * NC];
    for (int z = 0; z < S; ++z) {
        for (int y = 0; y < S; ++y) {
            for (int x = 0; x < S; ++x) {
                int idx = (z * S * S + y * S + x) * NC;
                if ((x + y + z) % 2 == 0) {
                    buf[idx]   = 200; // R
                    buf[idx+1] =  50; // G
                    buf[idx+2] =  50; // B
                } else {
                    buf[idx]   =  50;
                    buf[idx+1] =  50;
                    buf[idx+2] = 200;
                }
                buf[idx+3] = 255; // A
            }
        }
    }
    tex->images.setValue(SbVec3s(S, S, S), NC, buf);
    tex->wrapR.setValue(SoTexture3::REPEAT);
    tex->wrapS.setValue(SoTexture3::REPEAT);
    tex->wrapT.setValue(SoTexture3::REPEAT);
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_texture3.rgb");

    SbViewportRegion vp(W, H);
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 50.0f;
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.3f, -0.7f, -0.6f);
    root->addChild(light);

    // 3-D texture
    SoTexture3 *tex3 = new SoTexture3;
    buildTexture3(tex3);
    root->addChild(tex3);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(1.0f, 1.0f, 1.0f);
    root->addChild(mat);

    SoCube *cube = new SoCube;
    cube->width .setValue(2.0f);
    cube->height.setValue(2.0f);
    cube->depth .setValue(2.0f);
    root->addChild(cube);

    bool ok = renderer.render(root);
    int nb  = ok ? countNonBackground(renderer.getBuffer()) : 0;
    printf("render_texture3: ok=%d nonbg=%d\n", ok, nb);
    renderer.writeToRGB(outpath);

    root->unref();

    if (!ok || nb < 100) {
        fprintf(stderr, "render_texture3: FAIL – scene blank (nb=%d)\n", nb);
        return 1;
    }
    printf("render_texture3: PASS\n");
    return 0;
}
