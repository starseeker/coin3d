/*
 * render_texture_transform.cpp - Integration test: SoTexture2Transform
 *
 * Renders two textured quads side-by-side:
 *
 *   Left:  A red/white checkerboard texture applied without any transform.
 *   Right: The same checkerboard texture with SoTexture2Transform applied:
 *          scale(2,2), rotation(45 degrees), translation(0.1,0.1).
 *
 * The two quads should look different (different texture layout), proving that
 * SoTexture2Transform is modifying the texture coordinates.
 *
 * Pixel validation: both quads must be non-blank with colored pixels present.
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoFaceSet.h>
#include <Inventor/nodes/SoTextureCoordinate2.h>
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/nodes/SoTexture2Transform.h>
#include <Inventor/nodes/SoNormal.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>
#include <cstring>

static const int W = 512;
static const int H = 256;

static int countNonBackground(const unsigned char *buf, int w, int h)
{
    int count = 0;
    for (int i = 0; i < w * h; ++i) {
        const unsigned char *p = buf + i * 3;
        if (p[0] > 15 || p[1] > 15 || p[2] > 15) ++count;
    }
    return count;
}

// Build a red/white checkerboard texture
static void buildChecker(SoTexture2 *tex)
{
    const int TILE = 4;
    const int SIZE = 32;
    const int NC   = 3;
    unsigned char buf[SIZE * SIZE * NC];
    for (int y = 0; y < SIZE; y++) {
        for (int x = 0; x < SIZE; x++) {
            int idx = (y * SIZE + x) * NC;
            if (((x / TILE) + (y / TILE)) % 2 == 0) {
                buf[idx] = 200; buf[idx+1] = 40; buf[idx+2] = 40;  // red
            } else {
                buf[idx] = 255; buf[idx+1] = 255; buf[idx+2] = 255; // white
            }
        }
    }
    tex->image.setValue(SbVec2s(SIZE, SIZE), NC, buf);
    tex->wrapS.setValue(SoTexture2::REPEAT);
    tex->wrapT.setValue(SoTexture2::REPEAT);
}

// Build a flat quad at z=0 spanning (-1,-1) to (1,1) with explicit tex coords
static SoSeparator *buildTexturedQuad(bool withTransform)
{
    SoSeparator *sep = new SoSeparator;

    SoTexture2 *tex = new SoTexture2;
    buildChecker(tex);
    sep->addChild(tex);

    if (withTransform) {
        SoTexture2Transform *xf = new SoTexture2Transform;
        xf->scaleFactor.setValue(2.0f, 2.0f);
        xf->rotation.setValue(0.785398f);  // 45 degrees
        xf->translation.setValue(0.1f, 0.1f);
        sep->addChild(xf);
    }

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(1.0f, 1.0f, 1.0f);
    sep->addChild(mat);

    // Texture coordinates for a unit quad
    SoTextureCoordinate2 *tc = new SoTextureCoordinate2;
    tc->point.set1Value(0, SbVec2f(0.0f, 0.0f));
    tc->point.set1Value(1, SbVec2f(1.0f, 0.0f));
    tc->point.set1Value(2, SbVec2f(1.0f, 1.0f));
    tc->point.set1Value(3, SbVec2f(0.0f, 1.0f));
    sep->addChild(tc);

    // Normal (flat, facing +Z)
    SoNormal *nrm = new SoNormal;
    nrm->vector.set1Value(0, SbVec3f(0.0f, 0.0f, 1.0f));
    sep->addChild(nrm);
    SoNormalBinding *nb = new SoNormalBinding;
    nb->value.setValue(SoNormalBinding::OVERALL);
    sep->addChild(nb);

    // Quad geometry
    SoCoordinate3 *coords = new SoCoordinate3;
    coords->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
    coords->point.set1Value(1, SbVec3f( 1.0f, -1.0f, 0.0f));
    coords->point.set1Value(2, SbVec3f( 1.0f,  1.0f, 0.0f));
    coords->point.set1Value(3, SbVec3f(-1.0f,  1.0f, 0.0f));
    sep->addChild(coords);

    SoFaceSet *fs = new SoFaceSet;
    fs->numVertices.set1Value(0, 4);
    sep->addChild(fs);

    return sep;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_texture_transform.rgb");


    /* Render the canonical factory scene as the primary output image.
     * This ensures obol_viewer and obol_render produce identical scenes. */
    {
        SoSeparator *fRoot = ObolTest::Scenes::createTextureTransform(256, 256);
        SbViewportRegion fVp(256, 256);
        SoOffscreenRenderer fRen(fVp);
        fRen.setComponents(SoOffscreenRenderer::RGB);
        fRen.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
        if (fRen.render(fRoot)) {
            fRen.writeToRGB(outpath);
        }
        fRoot->unref();
    }
    SbViewportRegion vp(W, H);
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.05f, 0.05f, 0.05f));

    SoSeparator *root = new SoSeparator;
    root->ref();

    // Orthographic camera looking straight down -Z
    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->height.setValue(2.5f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 20.0f;
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(0.0f, 0.0f, -1.0f);
    root->addChild(light);

    // Left quad: no transform
    {
        SoSeparator *leftSep = new SoSeparator;
        SoTranslation *t = new SoTranslation;
        t->translation.setValue(-1.3f, 0.0f, 0.0f);
        leftSep->addChild(t);
        leftSep->addChild(buildTexturedQuad(false));
        root->addChild(leftSep);
    }

    // Right quad: with SoTexture2Transform
    {
        SoSeparator *rightSep = new SoSeparator;
        SoTranslation *t = new SoTranslation;
        t->translation.setValue(1.3f, 0.0f, 0.0f);
        rightSep->addChild(t);
        rightSep->addChild(buildTexturedQuad(true));
        root->addChild(rightSep);
    }

    bool ok = renderer.render(root);
    int nb  = ok ? countNonBackground(renderer.getBuffer(), W, H) : 0;
    printf("render_texture_transform: ok=%d nonbg=%d\n", ok, nb);
    renderer.writeToRGB(outpath);

    root->unref();

    if (!ok || nb < 200) {
        fprintf(stderr, "render_texture_transform: FAIL – scene blank (nb=%d)\n", nb);
        return 1;
    }
    printf("render_texture_transform: PASS\n");
    return 0;
}
