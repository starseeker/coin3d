/*
 * render_alpha_test.cpp - Integration test: SoAlphaTest threshold scene
 *
 * Tests SoAlphaTest by rendering a textured quad that has a checkerboard
 * RGBA texture where every other texel is fully transparent (alpha=0).
 *
 *   Frame 1 (SoAlphaTest::NONE / disabled): all fragments rendered.
 *     Scene must be non-blank.
 *
 *   Frame 2 (SoAlphaTest::GREATER, value=0.5): only fragments with alpha>0.5
 *     pass.  Since half the texels have alpha=0, roughly half the textured
 *     area should be clipped.  The scene must still be non-blank (the
 *     opaque texels remain visible).
 *
 *   Frame 3 (SoAlphaTest::ALWAYS): always passes — equivalent to disabled.
 *     Scene must be non-blank.
 *
 * Writes argv[1]+".rgb" (frame 1) and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoFaceSet.h>
#include <Inventor/nodes/SoTextureCoordinate2.h>
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/nodes/SoNormal.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/nodes/SoAlphaTest.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>

static const int W = 256;
static const int H = 256;

// Build a 16×16 RGBA checkerboard: opaque red / transparent white
static void buildAlphaTexture(SoTexture2 *tex)
{
    const int S  = 16;
    const int NC = 4;
    unsigned char buf[S * S * NC];
    for (int y = 0; y < S; ++y) {
        for (int x = 0; x < S; ++x) {
            int idx = (y * S + x) * NC;
            if ((x + y) % 2 == 0) {
                buf[idx]   = 200; buf[idx+1] = 50; buf[idx+2] = 50;
                buf[idx+3] = 255;  // opaque red
            } else {
                buf[idx]   = 255; buf[idx+1] = 255; buf[idx+2] = 255;
                buf[idx+3] = 0;    // fully transparent white
            }
        }
    }
    tex->image.setValue(SbVec2s(S, S), NC, buf);
    tex->wrapS.setValue(SoTexture2::REPEAT);
    tex->wrapT.setValue(SoTexture2::REPEAT);
    tex->model.setValue(SoTexture2::REPLACE);
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_alpha_test.rgb");

    SbViewportRegion vp(W, H);
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    // -----------------------------------------------------------------------
    // Build a flat textured quad with a mutable SoAlphaTest node
    // -----------------------------------------------------------------------
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->height.setValue(2.2f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 20.0f;
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(0.0f, 0.0f, -1.0f);
    root->addChild(light);

    SoAlphaTest *at = new SoAlphaTest;
    root->addChild(at);

    SoTexture2 *tex = new SoTexture2;
    buildAlphaTexture(tex);
    root->addChild(tex);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(1.0f, 1.0f, 1.0f);
    root->addChild(mat);

    SoTextureCoordinate2 *tc = new SoTextureCoordinate2;
    tc->point.set1Value(0, SbVec2f(0.0f, 0.0f));
    tc->point.set1Value(1, SbVec2f(4.0f, 0.0f));
    tc->point.set1Value(2, SbVec2f(4.0f, 4.0f));
    tc->point.set1Value(3, SbVec2f(0.0f, 4.0f));
    root->addChild(tc);

    SoNormal *nrm = new SoNormal;
    nrm->vector.set1Value(0, SbVec3f(0.0f, 0.0f, 1.0f));
    root->addChild(nrm);
    SoNormalBinding *nb = new SoNormalBinding;
    nb->value.setValue(SoNormalBinding::OVERALL);
    root->addChild(nb);

    SoCoordinate3 *coords = new SoCoordinate3;
    coords->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
    coords->point.set1Value(1, SbVec3f( 1.0f, -1.0f, 0.0f));
    coords->point.set1Value(2, SbVec3f( 1.0f,  1.0f, 0.0f));
    coords->point.set1Value(3, SbVec3f(-1.0f,  1.0f, 0.0f));
    root->addChild(coords);

    SoFaceSet *fs = new SoFaceSet;
    fs->numVertices.set1Value(0, 4);
    root->addChild(fs);

    // -----------------------------------------------------------------------
    // Frame 1: alpha test disabled (NONE)
    // -----------------------------------------------------------------------
    at->function.setValue(SoAlphaTest::NONE);
    at->value.setValue(0.5f);

    bool ok1 = renderer.render(root);
    // Alpha test uses RGBA buffer; non-bg check still works on RGB portion
    const unsigned char *buf1 = renderer.getBuffer();
    int nb1 = 0;
    if (ok1 && buf1) {
        for (int i = 0; i < W * H; ++i) {
            const unsigned char *p = buf1 + i * 4;
            if (p[0] > 15 || p[1] > 15 || p[2] > 15) ++nb1;
        }
    }
    printf("render_alpha_test frame1 (NONE): ok=%d nonbg=%d\n", ok1, nb1);
    renderer.writeToRGB(outpath);

    // -----------------------------------------------------------------------
    // Frame 2: GREATER threshold 0.5 (half the texels are clipped)
    // -----------------------------------------------------------------------
    at->function.setValue(SoAlphaTest::GREATER);
    at->value.setValue(0.5f);

    bool ok2 = renderer.render(root);
    const unsigned char *buf2 = renderer.getBuffer();
    int nb2 = 0;
    if (ok2 && buf2) {
        for (int i = 0; i < W * H; ++i) {
            const unsigned char *p = buf2 + i * 4;
            if (p[0] > 15 || p[1] > 15 || p[2] > 15) ++nb2;
        }
    }
    printf("render_alpha_test frame2 (GREATER 0.5): ok=%d nonbg=%d\n", ok2, nb2);

    // -----------------------------------------------------------------------
    // Frame 3: ALWAYS (all fragments pass — same as disabled)
    // -----------------------------------------------------------------------
    at->function.setValue(SoAlphaTest::ALWAYS);

    bool ok3 = renderer.render(root);
    const unsigned char *buf3 = renderer.getBuffer();
    int nb3 = 0;
    if (ok3 && buf3) {
        for (int i = 0; i < W * H; ++i) {
            const unsigned char *p = buf3 + i * 4;
            if (p[0] > 15 || p[1] > 15 || p[2] > 15) ++nb3;
        }
    }
    printf("render_alpha_test frame3 (ALWAYS): ok=%d nonbg=%d\n", ok3, nb3);

    root->unref();

    // -----------------------------------------------------------------------
    // Validation
    // -----------------------------------------------------------------------
    bool allOk = true;
    if (!ok1 || nb1 < 100) {
        fprintf(stderr, "render_alpha_test: FAIL frame1 (nb=%d)\n", nb1);
        allOk = false;
    }
    if (!ok2 || nb2 < 50) {
        fprintf(stderr, "render_alpha_test: FAIL frame2 (nb=%d)\n", nb2);
        allOk = false;
    }
    if (!ok3 || nb3 < 100) {
        fprintf(stderr, "render_alpha_test: FAIL frame3 (nb=%d)\n", nb3);
        allOk = false;
    }
    if (!allOk) return 1;
    printf("render_alpha_test: PASS\n");
    return 0;
}
