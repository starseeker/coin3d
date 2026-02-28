/*
 * render_multi_texture.cpp - Integration test: SoTextureUnit + multi-texture
 *
 * Exercises the multi-texture path by rendering a sphere with two texture
 * units:
 *   Unit 0: a red-dominated checkerboard
 *   Unit 1: a blue-tinted solid fill (MODULATE)
 *
 * Both textures are applied through SoTextureUnit::unit and SoTexture2 nodes.
 * SoTextureCombine is used for unit 1.
 *
 * Validates:
 *   1. SoTextureUnit::initClass is registered (class type id valid).
 *   2. Render with two active texture units does not crash.
 *   3. The output buffer contains at least some coloured pixels.
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/nodes/SoTextureUnit.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbColor.h>
#include <Inventor/SoType.h>
#include <cstdio>
#include <cstring>

static const int W = 256;
static const int H = 256;
static const int TEX_SIZE = 32;

// Build a simple RGB checkerboard
static void makeCheckerRGB(unsigned char * data, int w, int h, int cs,
                            unsigned char r0, unsigned char g0, unsigned char b0,
                            unsigned char r1, unsigned char g1, unsigned char b1)
{
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            bool cell0 = (((x / cs) + (y / cs)) & 1) == 0;
            unsigned char * p = data + (y * w + x) * 3;
            p[0] = cell0 ? r0 : r1;
            p[1] = cell0 ? g0 : g1;
            p[2] = cell0 ? b0 : b1;
        }
    }
}

int main(int argc, char ** argv)
{
    initCoinHeadless();

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_multi_texture.rgb");

    bool ok = true;

    // -----------------------------------------------------------------------
    // Verify SoTextureUnit class type is registered
    // -----------------------------------------------------------------------
    {
        SoType t = SoTextureUnit::getClassTypeId();
        if (t == SoType::badType()) {
            fprintf(stderr, "render_multi_texture: FAIL - SoTextureUnit has bad class type\n");
            ok = false;
        } else {
            printf("render_multi_texture: SoTextureUnit classTypeId OK\n");
        }
    }

    // -----------------------------------------------------------------------
    // Build scene with two texture units applied to a sphere
    // -----------------------------------------------------------------------
    SoSeparator * root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera * cam = new SoPerspectiveCamera;
    root->addChild(cam);

    SoDirectionalLight * light = new SoDirectionalLight;
    light->direction.setValue(-0.5f, -0.7f, -0.5f);
    root->addChild(light);

    // ----- Texture Unit 0 -----
    SoTextureUnit * tu0 = new SoTextureUnit;
    tu0->unit = 0;
    root->addChild(tu0);

    SoTexture2 * tex0 = new SoTexture2;
    {
        unsigned char data[TEX_SIZE * TEX_SIZE * 3];
        makeCheckerRGB(data, TEX_SIZE, TEX_SIZE, 8,
                       220, 30, 30,  // red cells
                       220, 220, 220); // white cells
        tex0->image.setValue(SbVec2s(TEX_SIZE, TEX_SIZE), 3, data);
        tex0->model = SoTexture2::MODULATE;
    }
    root->addChild(tex0);

    // ----- Texture Unit 1 -----
    SoTextureUnit * tu1 = new SoTextureUnit;
    tu1->unit = 1;
    root->addChild(tu1);

    SoTexture2 * tex1 = new SoTexture2;
    {
        // Solid blue tint
        unsigned char data[TEX_SIZE * TEX_SIZE * 3];
        for (int i = 0; i < TEX_SIZE * TEX_SIZE; ++i) {
            data[i * 3 + 0] = 30;
            data[i * 3 + 1] = 30;
            data[i * 3 + 2] = 180;
        }
        tex1->image.setValue(SbVec2s(TEX_SIZE, TEX_SIZE), 3, data);
        tex1->model = SoTexture2::MODULATE;
    }
    root->addChild(tex1);

    // Reset to unit 0 for subsequent shape rendering
    SoTextureUnit * tuReset = new SoTextureUnit;
    tuReset->unit = 0;
    root->addChild(tuReset);

    // Sphere
    SoSphere * sphere = new SoSphere;
    sphere->radius.setValue(1.2f);
    root->addChild(sphere);

    SbViewportRegion vp(W, H);
    cam->viewAll(root, vp);

    // -----------------------------------------------------------------------
    // Render and validate
    // -----------------------------------------------------------------------
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    if (!renderer.render(root)) {
        fprintf(stderr, "render_multi_texture: FAIL - render() returned false\n");
        ok = false;
    } else {
        const unsigned char * buf = renderer.getBuffer();
        if (!buf) {
            fprintf(stderr, "render_multi_texture: FAIL - getBuffer() returned null\n");
            ok = false;
        } else {
            int nonBlack = 0;
            for (int i = 0; i < W * H; ++i) {
                const unsigned char * p = buf + i * 3;
                if (p[0] > 20 || p[1] > 20 || p[2] > 20)
                    ++nonBlack;
            }
            printf("render_multi_texture: non-black pixels = %d / %d\n",
                   nonBlack, W * H);
            if (nonBlack < (W * H / 10)) {
                fprintf(stderr, "render_multi_texture: FAIL - too few non-black pixels "
                        "(%d < %d)\n", nonBlack, W * H / 10);
                ok = false;
            } else {
                /* Verify that BOTH texture units are actually applied.
                 *
                 * Unit 0 is a red/white checkerboard (MODULATE).
                 * Unit 1 is a solid blue fill (MODULATE).
                 *
                 * When both units are active the red checker cells produce
                 * pixels where R >> G (e.g. R~26, G~3 at full brightness),
                 * while the white cells produce R~G with dominant B.
                 * If only unit 1 is applied all pixels have R~G (no red
                 * cells), so the count of "red-cell" pixels is a reliable
                 * indicator that unit 0 is also being used.
                 *
                 * The sphere covers ~13% of the 256x256 frame; roughly half
                 * of those pixels correspond to red checker cells, giving
                 * ~4000 qualifying pixels.  We require at least W*H/30 to
                 * give comfortable headroom while still being strict.
                 */
                /* Pixels below this value in all channels are considered black
                 * (unlit sphere background). */
                const int BLACK_THRESHOLD = 5;
                /* A pixel is "red-cell" when its R channel is more than this
                 * many times its G channel (reflects the (220,30) ratio after
                 * MODULATE with the blue unit-1 texture). */
                const int RED_GREEN_RATIO = 3;
                int varPixels = 0;
                for (int i = 0; i < W * H; ++i) {
                    const unsigned char * p = buf + i * 3;
                    if (p[0] < BLACK_THRESHOLD && p[1] < BLACK_THRESHOLD &&
                        p[2] < BLACK_THRESHOLD) continue;
                    if ((int)p[0] > (int)p[1] * RED_GREEN_RATIO)
                        ++varPixels;
                }
                printf("render_multi_texture: checker-variation pixels = %d / %d\n",
                       varPixels, W * H);
                if (varPixels < (W * H / 30)) {
                    fprintf(stderr,
                            "render_multi_texture: FAIL - both texture units not applied "
                            "(checker variation pixels: %d < %d)\n",
                            varPixels, W * H / 30);
                    ok = false;
                } else {
                    printf("render_multi_texture: multi-texture validation OK\n");
                    renderer.writeToRGB(outpath);
                }
            }
        }
    }

    root->unref();
    printf("render_multi_texture: %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
