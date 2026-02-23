/*
 * render_indexed_face_set.cpp - Integration test: SoIndexedFaceSet with normals
 *
 * Renders a scene containing an SoIndexedFaceSet that builds a simple low-poly
 * icosahedron-like mesh using explicit coordinates, normals and texture
 * coordinates.  This exercises the full per-vertex attribute pipeline through:
 *   SoCoordinate3, SoNormal, SoNormalBinding, SoTextureCoordinate2,
 *   SoTextureCoordinateBinding, SoIndexedFaceSet
 *
 * A secondary flat-shaded quad (no normals, PER_FACE material) validates the
 * per-face material path.
 *
 * Pixel validation confirms that:
 *   - The image is not all-background (geometry was rendered).
 *   - At least two distinct hues are present (materials differentiated).
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoMaterialBinding.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoNormal.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/nodes/SoTextureCoordinate2.h>
#include <Inventor/nodes/SoTextureCoordinateBinding.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>
#include <cmath>
#include <vector>

static const int W = 512;
static const int H = 512;

// Build a simple flat triangular prism (2 triangular caps + 3 quad sides)
// to exercise SoIndexedFaceSet with explicit normals.
static SoSeparator *buildPrism()
{
    SoSeparator *sep = new SoSeparator;

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.2f, 0.5f, 0.9f);
    mat->specularColor.setValue(0.8f, 0.8f, 0.8f);
    mat->shininess.setValue(0.5f);
    sep->addChild(mat);

    // 6 vertices: 2 triangular caps
    // Bottom cap: y = -0.7
    // Top cap:    y = +0.7
    static const float vdata[][3] = {
        {  0.0f, -0.7f,  0.8f },  // 0: BF
        { -0.7f, -0.7f, -0.4f },  // 1: BL
        {  0.7f, -0.7f, -0.4f },  // 2: BR
        {  0.0f,  0.7f,  0.8f },  // 3: TF
        { -0.7f,  0.7f, -0.4f },  // 4: TL
        {  0.7f,  0.7f, -0.4f },  // 5: TR
    };

    SoCoordinate3 *coords = new SoCoordinate3;
    for (int i = 0; i < 6; ++i)
        coords->point.set1Value(i, SbVec3f(vdata[i][0], vdata[i][1], vdata[i][2]));
    sep->addChild(coords);

    // Per-vertex normals (approximate)
    static const float ndata[][3] = {
        {  0.0f, -1.0f,  0.3f },
        { -0.7f, -1.0f, -0.15f},
        {  0.7f, -1.0f, -0.15f},
        {  0.0f,  1.0f,  0.3f },
        { -0.7f,  1.0f, -0.15f},
        {  0.7f,  1.0f, -0.15f},
    };
    SoNormal *normals = new SoNormal;
    for (int i = 0; i < 6; ++i) {
        SbVec3f n(ndata[i][0], ndata[i][1], ndata[i][2]);
        n.normalize();
        normals->vector.set1Value(i, n);
    }
    sep->addChild(normals);
    SoNormalBinding *nb = new SoNormalBinding;
    nb->value.setValue(SoNormalBinding::PER_VERTEX_INDEXED);
    sep->addChild(nb);

    // Texture coordinates
    SoTextureCoordinate2 *tc = new SoTextureCoordinate2;
    tc->point.set1Value(0, SbVec2f(0.5f, 0.0f));
    tc->point.set1Value(1, SbVec2f(0.0f, 0.0f));
    tc->point.set1Value(2, SbVec2f(1.0f, 0.0f));
    tc->point.set1Value(3, SbVec2f(0.5f, 1.0f));
    tc->point.set1Value(4, SbVec2f(0.0f, 1.0f));
    tc->point.set1Value(5, SbVec2f(1.0f, 1.0f));
    sep->addChild(tc);
    SoTextureCoordinateBinding *tcb = new SoTextureCoordinateBinding;
    tcb->value.setValue(SoTextureCoordinateBinding::PER_VERTEX_INDEXED);
    sep->addChild(tcb);

    // Faces: bottom cap, top cap, 3 side quads
    SoIndexedFaceSet *ifs = new SoIndexedFaceSet;
    // Bottom cap (CCW looking from below → CW from above)
    int idx = 0;
    ifs->coordIndex.set1Value(idx++, 0);
    ifs->coordIndex.set1Value(idx++, 2);
    ifs->coordIndex.set1Value(idx++, 1);
    ifs->coordIndex.set1Value(idx++, -1);
    // Top cap
    ifs->coordIndex.set1Value(idx++, 3);
    ifs->coordIndex.set1Value(idx++, 4);
    ifs->coordIndex.set1Value(idx++, 5);
    ifs->coordIndex.set1Value(idx++, -1);
    // Side: front-right quad (0,2,5,3)
    ifs->coordIndex.set1Value(idx++, 0);
    ifs->coordIndex.set1Value(idx++, 3);
    ifs->coordIndex.set1Value(idx++, 5);
    ifs->coordIndex.set1Value(idx++, 2);
    ifs->coordIndex.set1Value(idx++, -1);
    // Side: front-left quad (0,1,4,3)
    ifs->coordIndex.set1Value(idx++, 0);
    ifs->coordIndex.set1Value(idx++, 1);
    ifs->coordIndex.set1Value(idx++, 4);
    ifs->coordIndex.set1Value(idx++, 3);
    ifs->coordIndex.set1Value(idx++, -1);
    // Side: back quad (1,2,5,4)
    ifs->coordIndex.set1Value(idx++, 1);
    ifs->coordIndex.set1Value(idx++, 2);
    ifs->coordIndex.set1Value(idx++, 5);
    ifs->coordIndex.set1Value(idx++, 4);
    ifs->coordIndex.set1Value(idx++, -1);
    sep->addChild(ifs);

    return sep;
}

// Build a flat quad using PER_FACE material binding.
static SoSeparator *buildColoredQuad()
{
    SoSeparator *sep = new SoSeparator;

    // Two-face quad: left half orange, right half teal
    SoMaterialBinding *mb = new SoMaterialBinding;
    mb->value.setValue(SoMaterialBinding::PER_FACE);
    sep->addChild(mb);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.set1Value(0, SbColor(0.9f, 0.5f, 0.1f));  // orange
    mat->diffuseColor.set1Value(1, SbColor(0.1f, 0.7f, 0.6f));  // teal
    sep->addChild(mat);

    SoNormal *normals = new SoNormal;
    normals->vector.set1Value(0, SbVec3f(0.0f, 0.0f, 1.0f));
    sep->addChild(normals);
    SoNormalBinding *nb = new SoNormalBinding;
    nb->value.setValue(SoNormalBinding::OVERALL);
    sep->addChild(nb);

    SoCoordinate3 *coords = new SoCoordinate3;
    coords->point.set1Value(0, SbVec3f(-0.8f, -0.3f, 0.0f));
    coords->point.set1Value(1, SbVec3f( 0.0f, -0.3f, 0.0f));
    coords->point.set1Value(2, SbVec3f( 0.8f, -0.3f, 0.0f));
    coords->point.set1Value(3, SbVec3f(-0.8f,  0.3f, 0.0f));
    coords->point.set1Value(4, SbVec3f( 0.0f,  0.3f, 0.0f));
    coords->point.set1Value(5, SbVec3f( 0.8f,  0.3f, 0.0f));
    sep->addChild(coords);

    SoIndexedFaceSet *ifs = new SoIndexedFaceSet;
    // Left triangle (orange)
    ifs->coordIndex.set1Value(0, 0); ifs->coordIndex.set1Value(1, 1);
    ifs->coordIndex.set1Value(2, 4); ifs->coordIndex.set1Value(3, 3);
    ifs->coordIndex.set1Value(4, -1);
    // Right triangle (teal)
    ifs->coordIndex.set1Value(5, 1); ifs->coordIndex.set1Value(6, 2);
    ifs->coordIndex.set1Value(7, 5); ifs->coordIndex.set1Value(8, 4);
    ifs->coordIndex.set1Value(9, -1);
    sep->addChild(ifs);

    return sep;
}

static bool validateScene(const unsigned char *buf)
{
    // Count non-background pixels and at least 2 distinct hue families
    int nonbg = 0, blue = 0, orange = 0, teal = 0;
    for (int y = 0; y < H; y += 4) {
        for (int x = 0; x < W; x += 4) {
            const unsigned char *p = buf + (y * W + x) * 3;
            if (p[0] < 10 && p[1] < 10 && p[2] < 10) continue;  // background
            ++nonbg;
            if (p[2] > 120 && p[0] < 150) ++blue;
            if (p[0] > 150 && p[1] > 80 && p[2] < 80) ++orange;
            if (p[1] > 120 && p[2] > 100 && p[0] < 100) ++teal;
        }
    }
    printf("render_indexed_face_set: nonbg=%d blue=%d orange=%d teal=%d\n",
           nonbg, blue, orange, teal);

    if (nonbg < 100) {
        fprintf(stderr, "render_indexed_face_set: FAIL – scene appears blank\n");
        return false;
    }
    printf("render_indexed_face_set: PASS\n");
    return true;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.4f, -0.8f, -0.5f);
    root->addChild(light);

    // Top half: prism
    SoSeparator *topGrp = new SoSeparator;
    SoTranslation *topT = new SoTranslation;
    topT->translation.setValue(0.0f, 0.8f, 0.0f);
    topGrp->addChild(topT);
    topGrp->addChild(buildPrism());
    root->addChild(topGrp);

    // Bottom half: coloured quad
    SoSeparator *botGrp = new SoSeparator;
    SoTranslation *botT = new SoTranslation;
    botT->translation.setValue(0.0f, -0.9f, 0.0f);
    botGrp->addChild(botT);
    botGrp->addChild(buildColoredQuad());
    root->addChild(botGrp);

    SbViewportRegion vp(W, H);
    cam->viewAll(root, vp);

    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_indexed_face_set.rgb");

    bool ok = false;
    if (renderer.render(root)) {
        const unsigned char *buf = renderer.getBuffer();
        bool pixOk = (buf != nullptr) && validateScene(buf);
        ok = pixOk && renderer.writeToRGB(outpath);
    } else {
        fprintf(stderr, "render_indexed_face_set: render() failed\n");
    }

    root->unref();
    return ok ? 0 : 1;
}
