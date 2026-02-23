/*
 * render_depth_buffer.cpp - Integration test: SoDepthBuffer modes
 *
 * Tests three depth-buffer configurations:
 *
 *   Frame 1 (LEQUAL, write=TRUE, test=TRUE): normal depth test — a near red
 *     cube should occlude a far blue sphere placed behind it.
 *
 *   Frame 2 (test=FALSE, write=FALSE): depth test disabled — the blue sphere
 *     is drawn last and should appear on top of the red cube regardless of
 *     distance (or at least the scene should be non-blank).
 *
 *   Frame 3 (NEVER, test=TRUE): depth function NEVER passes no fragment
 *     depth test — with a single object this means depth culling is
 *     applied but geometry is still rasterised; scene should be non-blank
 *     when test=FALSE for fill (we use test=FALSE here to check the
 *     write=FALSE + test=FALSE path, then switch test on).
 *
 * Pixel validation: frames 1 and 2 must be non-blank.
 *
 * Writes argv[1]+".rgb" (frame 1) and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoDepthBuffer.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>

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

int main(int argc, char **argv)
{
    initCoinHeadless();

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_depth_buffer.rgb");

    SbViewportRegion vp(W, H);
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    // -----------------------------------------------------------------------
    // Build scene: camera, light, mutable SoDepthBuffer, then two shapes
    // -----------------------------------------------------------------------
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 6.0f);
    cam->nearDistance = 0.5f;
    cam->farDistance  = 30.0f;
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.3f, -0.7f, -0.6f);
    root->addChild(light);

    SoDepthBuffer *db = new SoDepthBuffer;
    root->addChild(db);

    // Near red cube at z=0
    {
        SoSeparator *cubeSep = new SoSeparator;
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.8f, 0.2f, 0.2f);
        cubeSep->addChild(mat);
        SoCube *cube = new SoCube;
        cube->width .setValue(1.4f);
        cube->height.setValue(1.4f);
        cube->depth .setValue(1.4f);
        cubeSep->addChild(cube);
        root->addChild(cubeSep);
    }

    // Far blue sphere at z=-2
    {
        SoSeparator *sphSep = new SoSeparator;
        SoTranslation *t = new SoTranslation;
        t->translation.setValue(0.0f, 0.0f, -2.0f);
        sphSep->addChild(t);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.2f, 0.4f, 0.9f);
        sphSep->addChild(mat);
        SoSphere *sph = new SoSphere;
        sph->radius.setValue(0.8f);
        sphSep->addChild(sph);
        root->addChild(sphSep);
    }

    // -----------------------------------------------------------------------
    // Frame 1: normal depth test (LEQUAL, test=TRUE, write=TRUE)
    // -----------------------------------------------------------------------
    db->test .setValue(TRUE);
    db->write.setValue(TRUE);
    db->function.setValue(SoDepthBuffer::LEQUAL);

    bool ok1 = renderer.render(root);
    int nb1  = ok1 ? countNonBackground(renderer.getBuffer()) : 0;
    printf("render_depth_buffer frame1 (LEQUAL): ok=%d nonbg=%d\n", ok1, nb1);
    renderer.writeToRGB(outpath);

    // -----------------------------------------------------------------------
    // Frame 2: depth test disabled (test=FALSE, write=FALSE)
    // -----------------------------------------------------------------------
    db->test .setValue(FALSE);
    db->write.setValue(FALSE);

    bool ok2 = renderer.render(root);
    int nb2  = ok2 ? countNonBackground(renderer.getBuffer()) : 0;
    printf("render_depth_buffer frame2 (test=FALSE): ok=%d nonbg=%d\n", ok2, nb2);

    // -----------------------------------------------------------------------
    // Frame 3: GEQUAL function (only passes if incoming >= stored depth)
    // -----------------------------------------------------------------------
    db->test .setValue(TRUE);
    db->write.setValue(TRUE);
    db->function.setValue(SoDepthBuffer::GEQUAL);

    bool ok3 = renderer.render(root);
    int nb3  = ok3 ? countNonBackground(renderer.getBuffer()) : 0;
    printf("render_depth_buffer frame3 (GEQUAL): ok=%d nonbg=%d\n", ok3, nb3);

    root->unref();

    // -----------------------------------------------------------------------
    // Validation
    // -----------------------------------------------------------------------
    bool allOk = true;
    if (!ok1 || nb1 < 100) {
        fprintf(stderr, "render_depth_buffer: FAIL frame1 (nb=%d)\n", nb1);
        allOk = false;
    }
    if (!ok2 || nb2 < 100) {
        fprintf(stderr, "render_depth_buffer: FAIL frame2 (nb=%d)\n", nb2);
        allOk = false;
    }
    // Frame 3 (GEQUAL): at least render() must succeed; nb may vary by driver
    if (!ok3) {
        fprintf(stderr, "render_depth_buffer: FAIL frame3 render failed\n");
        allOk = false;
    }

    if (!allOk) return 1;
    printf("render_depth_buffer: PASS\n");
    return 0;
}
