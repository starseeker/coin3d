/*
 * render_gl_features.cpp - Exercises SoGLDriverDatabase and SoGLImage lifecycle
 *
 * This test covers:
 *   1. SoGLDriverDatabase::init() called explicitly (idempotent after SoDB::init).
 *   2. Rendering with multiple texture types exercises the GL feature detection
 *      code in SoGLDriverDatabase::isSupported internally for:
 *        - OBOL_texture_object       (SoTexture2 path)
 *        - OBOL_multitexture         (SoMultiTextureImageElement)
 *        - OBOL_texsubimage          (texture upload path)
 *        - OBOL_non_power_of_two_textures (when NPOT texture is used)
 *   3. SoGLImage: construct, setData, getImage, getQuality, getTypeId (pre-GL,
 *      no state required), unref.
 *   4. SoGLBigImage: getClassTypeId, setChangeLimit round-trip.
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/misc/SoGLDriverDatabase.h>
#include <Inventor/misc/SoGLImage.h>
#include <Inventor/misc/SoGLBigImage.h>
#include <Inventor/SbImage.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbColor.h>
#include <cstdio>
#include <cstring>

static const int W = 128;
static const int H = 128;
static const int TEX_SIZE = 64;   // power-of-two; safe on all GL implementations

// Create a simple red-and-white checkerboard (RGB)
static unsigned char * makeRGB(int w, int h, int cellSize = 8)
{
    unsigned char * data = new unsigned char[w * h * 3];
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            bool red = (((x / cellSize) + (y / cellSize)) & 1) == 0;
            unsigned char * p = data + (y * w + x) * 3;
            p[0] = red ? 220 : 240;
            p[1] = red ?   0 : 240;
            p[2] = red ?   0 : 240;
        }
    }
    return data;
}

int main(int argc, char ** argv)
{
    initCoinHeadless();

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_gl_features.rgb");

    bool ok = true;

    // -----------------------------------------------------------------------
    // SoGLDriverDatabase::init() (idempotent)
    // -----------------------------------------------------------------------
    SoGLDriverDatabase::init();  // already called by SoDB::init() but safe to call again
    printf("render_gl_features: SoGLDriverDatabase::init() OK\n");

    // -----------------------------------------------------------------------
    // SoGLImage: pre-GL accessors (no state required)
    // -----------------------------------------------------------------------
    {
        SoGLImage * img = new SoGLImage;

        // Type checks
        bool typeOk = (img->getTypeId() == SoGLImage::getClassTypeId());
        bool isOfType = img->isOfType(SoGLImage::getClassTypeId());
        if (!typeOk || !isOfType) {
            fprintf(stderr, "render_gl_features: FAIL - SoGLImage type check\n");
            ok = false;
        }

        // setData / getImage / getQuality
        unsigned char * rgb = makeRGB(TEX_SIZE, TEX_SIZE);
        img->setData(rgb, SbVec2s(TEX_SIZE, TEX_SIZE), 3,
                     SoGLImage::REPEAT, SoGLImage::REPEAT, 0.5f);
        delete[] rgb;

        const SbImage * sbImg = img->getImage();
        if (!sbImg) {
            fprintf(stderr, "render_gl_features: FAIL - SoGLImage::getImage() returned null\n");
            ok = false;
        } else {
            SbVec3s imgSize;
            int nc;
            sbImg->getValue(imgSize, nc);
            if (imgSize[0] != TEX_SIZE || imgSize[1] != TEX_SIZE || nc != 3) {
                fprintf(stderr, "render_gl_features: FAIL - SoGLImage size/nc mismatch "
                        "(%d×%d×%d)\n", imgSize[0], imgSize[1], nc);
                ok = false;
            } else {
                printf("render_gl_features: SoGLImage setData/getImage OK\n");
            }
        }

        float q = img->getQuality();
        if (q < 0.0f || q > 1.0f) {
            fprintf(stderr, "render_gl_features: FAIL - SoGLImage::getQuality() out of range\n");
            ok = false;
        } else {
            printf("render_gl_features: SoGLImage::getQuality() = %.2f OK\n", q);
        }

        img->unref(nullptr);  // no state; just decrements refcount / frees
    }

    // -----------------------------------------------------------------------
    // SoGLBigImage: pre-GL accessors
    // -----------------------------------------------------------------------
    {
        SoType bigType = SoGLBigImage::getClassTypeId();
        if (bigType == SoType::badType()) {
            fprintf(stderr, "render_gl_features: FAIL - SoGLBigImage bad class type\n");
            ok = false;
        }

        // setChangeLimit round-trip
        int old   = SoGLBigImage::setChangeLimit(50);
        int check = SoGLBigImage::setChangeLimit(old);
        if (check != 50) {
            fprintf(stderr, "render_gl_features: FAIL - setChangeLimit round-trip "
                    "(expected 50, got %d)\n", check);
            ok = false;
        } else {
            printf("render_gl_features: SoGLBigImage setChangeLimit OK\n");
        }
    }

    // -----------------------------------------------------------------------
    // Build a textured quad scene to exercise GL-side SoGLImage paths
    // -----------------------------------------------------------------------
    SoSeparator * root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera * cam = new SoPerspectiveCamera;
    root->addChild(cam);

    SoDirectionalLight * light = new SoDirectionalLight;
    light->direction.setValue(-0.5f, -0.7f, -0.5f);
    root->addChild(light);

    // A RGB texture applied to a sphere (sphere has automatic tex coords)
    SoTexture2 * tex = new SoTexture2;
    {
        unsigned char * data = makeRGB(TEX_SIZE, TEX_SIZE);
        tex->image.setValue(SbVec2s(TEX_SIZE, TEX_SIZE), 3, data);
        delete[] data;
    }
    tex->wrapS = SoTexture2::REPEAT;
    tex->wrapT = SoTexture2::REPEAT;
    root->addChild(tex);

    SoSphere * sphere = new SoSphere;
    sphere->radius.setValue(1.0f);
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
        fprintf(stderr, "render_gl_features: FAIL - render() returned false\n");
        ok = false;
    } else {
        const unsigned char * buf = renderer.getBuffer();
        if (!buf) {
            fprintf(stderr, "render_gl_features: FAIL - getBuffer() returned null\n");
            ok = false;
        } else {
            int nonBlack = 0;
            for (int i = 0; i < W * H; ++i) {
                const unsigned char * p = buf + i * 3;
                if (p[0] > 30 || p[1] > 30 || p[2] > 30)
                    ++nonBlack;
            }
            printf("render_gl_features: non-black pixels = %d / %d\n",
                   nonBlack, W * H);
            if (nonBlack < W * H / 8) {
                fprintf(stderr, "render_gl_features: FAIL - too few non-black pixels "
                        "(%d < %d)\n", nonBlack, W * H / 8);
                ok = false;
            } else {
                printf("render_gl_features: pixel validation OK\n");
                renderer.writeToRGB(outpath);
            }
        }
    }

    root->unref();
    printf("render_gl_features: %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
