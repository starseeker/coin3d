/*
 * render_gl_features.cpp - Exercises SoGLDriverDatabase and SoGLImage lifecycle
 *
 * Uses ObolTest::Scenes::createGLFeatures — the same factory used by
 * obol_viewer — so CLI and viewer render identical scenes.
 *
 * Tests covered:
 *   1. SoGLDriverDatabase::init() (idempotent after SoDB::init)
 *   2. SoGLImage: pre-GL accessors (construct, setData, getImage, getQuality, typeId)
 *   3. SoGLBigImage: getClassTypeId, setChangeLimit round-trip
 *   4. Pixel validation: rendered sphere must have sufficient non-black pixels
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <Inventor/misc/SoGLDriverDatabase.h>
#include <Inventor/misc/SoGLImage.h>
#include <Inventor/misc/SoGLBigImage.h>
#include <Inventor/SbImage.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>

static const int W = 256;
static const int H = 256;
static const int TEX_SIZE = 64;

static unsigned char *makeRGB(int w, int h, int cellSize = 8)
{
    unsigned char *data = new unsigned char[w * h * 3];
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            bool red = (((x / cellSize) + (y / cellSize)) & 1) == 0;
            unsigned char *p = data + (y * w + x) * 3;
            p[0] = red ? 220 : 240;
            p[1] = red ?   0 : 240;
            p[2] = red ?   0 : 240;
        }
    }
    return data;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_gl_features.rgb");

    bool ok = true;

    /* SoGLDriverDatabase (idempotent) */
    SoGLDriverDatabase::init();
    printf("render_gl_features: SoGLDriverDatabase::init() OK\n");

    /* SoGLImage pre-GL API */
    {
        SoGLImage *img = new SoGLImage;
        if (img->getTypeId() != SoGLImage::getClassTypeId() ||
            !img->isOfType(SoGLImage::getClassTypeId())) {
            fprintf(stderr, "render_gl_features: FAIL - SoGLImage type check\n");
            ok = false;
        }
        unsigned char *rgb = makeRGB(TEX_SIZE, TEX_SIZE);
        img->setData(rgb, SbVec2s(TEX_SIZE, TEX_SIZE), 3,
                     SoGLImage::REPEAT, SoGLImage::REPEAT, 0.5f);
        delete[] rgb;
        const SbImage *sbImg = img->getImage();
        if (!sbImg) {
            fprintf(stderr, "render_gl_features: FAIL - getImage() null\n");
            ok = false;
        } else {
            SbVec3s sz; int nc;
            sbImg->getValue(sz, nc);
            if (sz[0] != TEX_SIZE || sz[1] != TEX_SIZE || nc != 3)
                ok = false;
            else
                printf("render_gl_features: SoGLImage setData/getImage OK\n");
        }
        float q = img->getQuality();
        if (q < 0.0f || q > 1.0f) ok = false;
        else printf("render_gl_features: SoGLImage::getQuality() = %.2f OK\n", q);
        img->unref(nullptr);
    }

    /* SoGLBigImage pre-GL API */
    {
        if (SoGLBigImage::getClassTypeId() == SoType::badType()) {
            fprintf(stderr, "render_gl_features: FAIL - SoGLBigImage bad class type\n");
            ok = false;
        }
        int old   = SoGLBigImage::setChangeLimit(50);
        int check = SoGLBigImage::setChangeLimit(old);
        if (check != 50) {
            fprintf(stderr, "render_gl_features: FAIL - setChangeLimit round-trip\n");
            ok = false;
        } else {
            printf("render_gl_features: SoGLBigImage setChangeLimit OK\n");
        }
    }

    /* Use the shared factory so viewer and CLI render the same scene. */
    SoSeparator *root = ObolTest::Scenes::createGLFeatures(W, H);

    SbViewportRegion vp(W, H);
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    if (!renderer.render(root)) {
        fprintf(stderr, "render_gl_features: FAIL - render() failed\n");
        ok = false;
    } else {
        const unsigned char *buf = renderer.getBuffer();
        int nonBlack = 0;
        if (buf) {
            for (int i = 0; i < W * H; ++i) {
                const unsigned char *p = buf + i * 3;
                if (p[0] > 30 || p[1] > 30 || p[2] > 30) ++nonBlack;
            }
        }
        printf("render_gl_features: non-black=%d\n", nonBlack);
        if (nonBlack < W * H / 8) {
            fprintf(stderr, "render_gl_features: FAIL - too few pixels\n");
            ok = false;
        } else {
            renderer.writeToRGB(outpath);
        }
    }

    root->unref();
    printf("render_gl_features: %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
