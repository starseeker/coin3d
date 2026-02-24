/*
 * render_material_binding.cpp — All SoMaterialBinding modes coverage test
 *
 * Exercises SoMaterialBinding with SoFaceSet and SoIndexedFaceSet for all
 * binding modes:
 *   1. OVERALL — one material for the entire shape
 *   2. PER_PART — one per "part" (face for FaceSet)
 *   3. PER_FACE — one material per face
 *   4. PER_VERTEX — one per vertex (Gouraud interpolation)
 *   5. PER_FACE_INDEXED — indexed per-face material
 *   6. PER_VERTEX_INDEXED — indexed per-vertex material (with SoIndexedFaceSet)
 *   7. SoBaseColor (replaces diffuse without shininess/specular)
 *   8. SoMaterial with multiple diffuseColor values (MFColor array)
 *   9. SoMaterial fields: ambientColor, specularColor, emissiveColor,
 *      shininess, transparency (exercises SoMaterial deeper code paths)
 *  10. Render with SoVertexProperty per-vertex colors
 *
 * Returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoMaterialBinding.h>
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoNormal.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/nodes/SoFaceSet.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <Inventor/nodes/SoVertexProperty.h>
#include <Inventor/nodes/SoPackedColor.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>

static const int W = 256;
static const int H = 256;

static bool validateNonBlack(const unsigned char *buf, int npix,
                              const char *label)
{
    int nonbg = 0;
    for (int i = 0; i < npix; ++i) {
        const unsigned char *p = buf + i * 3;
        if (p[0] > 5 || p[1] > 5 || p[2] > 5) ++nonbg;
    }
    printf("  %s: nonbg=%d\n", label, nonbg);
    if (nonbg < 50) {
        fprintf(stderr, "  FAIL %s: blank\n", label);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Build a grid of two quads as separate SoFaceSet shapes (for testing
// PER_PART / PER_FACE bindings)
// ---------------------------------------------------------------------------

// Quad geometry: two separate quad faces
static const float quadCoords[8][3] = {
    // Face 0: left quad
    {-2.0f,-1.0f, 0.0f},
    {-0.2f,-1.0f, 0.0f},
    {-0.2f, 1.0f, 0.0f},
    {-2.0f, 1.0f, 0.0f},
    // Face 1: right quad
    { 0.2f,-1.0f, 0.0f},
    { 2.0f,-1.0f, 0.0f},
    { 2.0f, 1.0f, 0.0f},
    { 0.2f, 1.0f, 0.0f}
};
static const int quadVertCounts[2] = { 4, 4 };

static const SbVec3f faceNormals[2] = {
    SbVec3f(0,0,1), SbVec3f(0,0,1)
};

// ---------------------------------------------------------------------------
// Test 1: OVERALL binding – single material for all faces
// ---------------------------------------------------------------------------
static bool test1_overall(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->height.setValue(6.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoMaterialBinding *mb = new SoMaterialBinding;
    mb->value.setValue(SoMaterialBinding::OVERALL);
    root->addChild(mb);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.7f, 0.2f, 0.2f);
    mat->specularColor.setValue(1.0f, 1.0f, 1.0f);
    mat->shininess.setValue(0.9f);
    root->addChild(mat);

    SoCoordinate3 *c3 = new SoCoordinate3;
    c3->point.setValues(0, 8, quadCoords);
    root->addChild(c3);

    SoNormal *n = new SoNormal;
    n->vector.setValues(0, 2, faceNormals);
    root->addChild(n);

    SoNormalBinding *nb = new SoNormalBinding;
    nb->value.setValue(SoNormalBinding::PER_FACE);
    root->addChild(nb);

    SoFaceSet *fs = new SoFaceSet;
    fs->numVertices.setValues(0, 2, quadVertCounts);
    root->addChild(fs);

    char outpath[1024];
    snprintf(outpath, sizeof(outpath), "%s_overall.rgb", basepath);
    bool ok = renderToFile(root, outpath, W, H);

    if (ok) {
        SoOffscreenRenderer *ren = getSharedRenderer();
        ren->setViewportRegion(SbViewportRegion(W, H));
        ok = ren->render(root) &&
             validateNonBlack(ren->getBuffer(), W*H, "test1_overall");
    }

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 2: PER_FACE – different material per face
// ---------------------------------------------------------------------------
static bool test2_perFace(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->height.setValue(6.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoMaterialBinding *mb = new SoMaterialBinding;
    mb->value.setValue(SoMaterialBinding::PER_FACE);
    root->addChild(mb);

    // Two materials: face 0 = red, face 1 = blue
    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.set1Value(0, SbColor(0.9f, 0.1f, 0.1f));
    mat->diffuseColor.set1Value(1, SbColor(0.1f, 0.1f, 0.9f));
    root->addChild(mat);

    SoCoordinate3 *c3 = new SoCoordinate3;
    c3->point.setValues(0, 8, quadCoords);
    root->addChild(c3);

    SoNormal *n = new SoNormal;
    n->vector.setValues(0, 2, faceNormals);
    root->addChild(n);

    SoNormalBinding *nb = new SoNormalBinding;
    nb->value.setValue(SoNormalBinding::PER_FACE);
    root->addChild(nb);

    SoFaceSet *fs = new SoFaceSet;
    fs->numVertices.setValues(0, 2, quadVertCounts);
    root->addChild(fs);

    char outpath[1024];
    snprintf(outpath, sizeof(outpath), "%s_per_face.rgb", basepath);
    bool ok = renderToFile(root, outpath, W, H);

    if (ok) {
        SoOffscreenRenderer *ren = getSharedRenderer();
        ren->setViewportRegion(SbViewportRegion(W, H));
        ok = ren->render(root) &&
             validateNonBlack(ren->getBuffer(), W*H, "test2_perFace");
    }

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 3: PER_VERTEX – per-vertex interpolation with SoFaceSet
// ---------------------------------------------------------------------------
static bool test3_perVertex(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->height.setValue(4.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoMaterialBinding *mb = new SoMaterialBinding;
    mb->value.setValue(SoMaterialBinding::PER_VERTEX);
    root->addChild(mb);

    // Four vertices of a single quad, each with a different color
    static const float vcoords[4][3] = {
        {-1.5f,-1.5f, 0.0f},
        { 1.5f,-1.5f, 0.0f},
        { 1.5f, 1.5f, 0.0f},
        {-1.5f, 1.5f, 0.0f}
    };
    static const int nv[] = { 4 };
    static const SbVec3f vtxNorm(0,0,1);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.set1Value(0, SbColor(1.0f, 0.0f, 0.0f));
    mat->diffuseColor.set1Value(1, SbColor(0.0f, 1.0f, 0.0f));
    mat->diffuseColor.set1Value(2, SbColor(0.0f, 0.0f, 1.0f));
    mat->diffuseColor.set1Value(3, SbColor(1.0f, 1.0f, 0.0f));
    root->addChild(mat);

    SoCoordinate3 *c3 = new SoCoordinate3;
    c3->point.setValues(0, 4, vcoords);
    root->addChild(c3);

    SoNormal *n = new SoNormal;
    n->vector.set1Value(0, vtxNorm);
    root->addChild(n);
    SoNormalBinding *nb = new SoNormalBinding;
    nb->value.setValue(SoNormalBinding::OVERALL);
    root->addChild(nb);

    SoFaceSet *fs = new SoFaceSet;
    fs->numVertices.setValues(0, 1, nv);
    root->addChild(fs);

    char outpath[1024];
    snprintf(outpath, sizeof(outpath), "%s_per_vertex.rgb", basepath);
    bool ok = renderToFile(root, outpath, W, H);

    if (ok) {
        SoOffscreenRenderer *ren = getSharedRenderer();
        ren->setViewportRegion(SbViewportRegion(W, H));
        ok = ren->render(root) &&
             validateNonBlack(ren->getBuffer(), W*H, "test3_perVertex");
    }

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 4: PER_VERTEX_INDEXED – indexed per-vertex material with SoIndexedFaceSet
// ---------------------------------------------------------------------------
static bool test4_perVertexIndexed(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->height.setValue(4.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoMaterialBinding *mb = new SoMaterialBinding;
    mb->value.setValue(SoMaterialBinding::PER_VERTEX_INDEXED);
    root->addChild(mb);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.set1Value(0, SbColor(1.0f, 0.0f, 0.0f)); // red
    mat->diffuseColor.set1Value(1, SbColor(0.0f, 1.0f, 0.0f)); // green
    mat->diffuseColor.set1Value(2, SbColor(0.0f, 0.0f, 1.0f)); // blue
    mat->diffuseColor.set1Value(3, SbColor(1.0f, 1.0f, 0.0f)); // yellow
    root->addChild(mat);

    static const float coords[4][3] = {
        {-1.5f,-1.5f, 0.0f},
        { 1.5f,-1.5f, 0.0f},
        { 1.5f, 1.5f, 0.0f},
        {-1.5f, 1.5f, 0.0f}
    };
    static const int32_t coordIdx[] = { 0, 1, 2, -1, 0, 2, 3, -1 };
    static const int32_t matIdx[]   = { 0, 1, 2, -1, 0, 2, 3, -1 };

    SoCoordinate3 *c3 = new SoCoordinate3;
    c3->point.setValues(0, 4, coords);
    root->addChild(c3);

    SoIndexedFaceSet *ifs = new SoIndexedFaceSet;
    ifs->coordIndex.setValues(0, 8, coordIdx);
    ifs->materialIndex.setValues(0, 8, matIdx);
    root->addChild(ifs);

    char outpath[1024];
    snprintf(outpath, sizeof(outpath), "%s_per_vertex_idx.rgb", basepath);
    bool ok = renderToFile(root, outpath, W, H);

    if (ok) {
        SoOffscreenRenderer *ren = getSharedRenderer();
        ren->setViewportRegion(SbViewportRegion(W, H));
        ok = ren->render(root) &&
             validateNonBlack(ren->getBuffer(), W*H, "test4_perVertexIndexed");
    }

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 5: SoBaseColor node (replaces material diffuse only)
// ---------------------------------------------------------------------------
static bool test5_baseColor(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    // SoBaseColor sets only diffuse; ambient/specular/shininess come from state
    SoBaseColor *bc = new SoBaseColor;
    bc->rgb.setValue(SbColor(0.2f, 0.9f, 0.4f));
    root->addChild(bc);

    root->addChild(new SoSphere);

    SbViewportRegion vp(W, H);
    cam->viewAll(root, vp);

    char outpath[1024];
    snprintf(outpath, sizeof(outpath), "%s_base_color.rgb", basepath);
    bool ok = renderToFile(root, outpath, W, H);

    if (ok) {
        SoOffscreenRenderer *ren = getSharedRenderer();
        ren->setViewportRegion(vp);
        ok = ren->render(root) &&
             validateNonBlack(ren->getBuffer(), W*H, "test5_baseColor");
    }

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 6: SoMaterial full field suite — all material properties on a sphere
// ---------------------------------------------------------------------------
static bool test6_fullMaterial(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoMaterial *mat = new SoMaterial;
    mat->ambientColor.setValue(0.2f, 0.2f, 0.0f);
    mat->diffuseColor.setValue(0.8f, 0.4f, 0.1f);
    mat->specularColor.setValue(1.0f, 1.0f, 0.8f);
    mat->emissiveColor.setValue(0.05f, 0.02f, 0.0f);
    mat->shininess.setValue(0.8f);
    mat->transparency.setValue(0.0f); // opaque
    root->addChild(mat);

    root->addChild(new SoSphere);

    SbViewportRegion vp(W, H);
    cam->viewAll(root, vp);

    char outpath[1024];
    snprintf(outpath, sizeof(outpath), "%s_full_mat.rgb", basepath);
    bool ok = renderToFile(root, outpath, W, H);

    if (ok) {
        SoOffscreenRenderer *ren = getSharedRenderer();
        ren->setViewportRegion(vp);
        ok = ren->render(root) &&
             validateNonBlack(ren->getBuffer(), W*H, "test6_fullMaterial");
    }

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 7: SoVertexProperty per-vertex colors on SoIndexedFaceSet
// ---------------------------------------------------------------------------
static bool test7_vertexPropertyColors(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->height.setValue(4.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoIndexedFaceSet *ifs = new SoIndexedFaceSet;
    SoVertexProperty *vp_node = new SoVertexProperty;

    static const float vcoords[4][3] = {
        {-1.5f,-1.5f, 0.0f},
        { 1.5f,-1.5f, 0.0f},
        { 1.5f, 1.5f, 0.0f},
        {-1.5f, 1.5f, 0.0f}
    };
    vp_node->vertex.setValues(0, 4, vcoords);

    // Per-vertex normals
    static const SbVec3f norms[4] = {
        SbVec3f(0,0,1), SbVec3f(0,0,1), SbVec3f(0,0,1), SbVec3f(0,0,1)
    };
    vp_node->normal.setValues(0, 4, norms);
    vp_node->normalBinding.setValue(SoVertexProperty::PER_VERTEX);

    // Per-vertex packed colors (ABGR)
    uint32_t colors[4] = {
        0xff0000ff,  // red
        0xff00ff00,  // green
        0xffff0000,  // blue
        0xff00ffff   // yellow
    };
    vp_node->orderedRGBA.setValues(0, 4, colors);
    vp_node->materialBinding.setValue(SoVertexProperty::PER_VERTEX);

    static const int32_t cidx[] = { 0, 1, 2, -1, 0, 2, 3, -1 };
    ifs->coordIndex.setValues(0, 8, cidx);
    ifs->vertexProperty.setValue(vp_node);

    root->addChild(ifs);

    char outpath[1024];
    snprintf(outpath, sizeof(outpath), "%s_vp_colors.rgb", basepath);
    bool ok = renderToFile(root, outpath, W, H);

    if (ok) {
        SoOffscreenRenderer *ren = getSharedRenderer();
        ren->setViewportRegion(SbViewportRegion(W, H));
        ok = ren->render(root) &&
             validateNonBlack(ren->getBuffer(), W*H, "test7_vertexPropertyColors");
    }

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 8: PER_PART_INDEXED binding with SoIndexedFaceSet
// ---------------------------------------------------------------------------
static bool test8_perPartIndexed(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->height.setValue(6.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoMaterialBinding *mb = new SoMaterialBinding;
    mb->value.setValue(SoMaterialBinding::PER_PART_INDEXED);
    root->addChild(mb);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.set1Value(0, SbColor(0.9f, 0.2f, 0.2f));
    mat->diffuseColor.set1Value(1, SbColor(0.2f, 0.7f, 0.2f));
    root->addChild(mat);

    SoCoordinate3 *c3 = new SoCoordinate3;
    c3->point.setValues(0, 8, quadCoords);
    root->addChild(c3);

    SoNormal *n = new SoNormal;
    n->vector.setValues(0, 2, faceNormals);
    root->addChild(n);
    SoNormalBinding *nb = new SoNormalBinding;
    nb->value.setValue(SoNormalBinding::PER_FACE);
    root->addChild(nb);

    static const int32_t cidx[] = {
        0, 1, 2, 3, -1,
        4, 5, 6, 7, -1
    };
    static const int32_t midx[] = {
        0, -1,
        1, -1
    };

    SoIndexedFaceSet *ifs = new SoIndexedFaceSet;
    ifs->coordIndex.setValues(0, 10, cidx);
    ifs->materialIndex.setValues(0, 4, midx);
    root->addChild(ifs);

    char outpath[1024];
    snprintf(outpath, sizeof(outpath), "%s_per_part_idx.rgb", basepath);
    bool ok = renderToFile(root, outpath, W, H);

    if (ok) {
        SoOffscreenRenderer *ren = getSharedRenderer();
        ren->setViewportRegion(SbViewportRegion(W, H));
        ok = ren->render(root) &&
             validateNonBlack(ren->getBuffer(), W*H, "test8_perPartIndexed");
    }

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char **argv)
{
    initCoinHeadless();

    const char *basepath = (argc > 1) ? argv[1] : "render_material_binding";

    int failures = 0;

    printf("\n=== SoMaterialBinding modes tests ===\n");

    if (!test1_overall(basepath))          ++failures;
    if (!test2_perFace(basepath))          ++failures;
    if (!test3_perVertex(basepath))        ++failures;
    if (!test4_perVertexIndexed(basepath)) ++failures;
    if (!test5_baseColor(basepath))        ++failures;
    if (!test6_fullMaterial(basepath))     ++failures;
    if (!test7_vertexPropertyColors(basepath)) ++failures;
    if (!test8_perPartIndexed(basepath))   ++failures;

    printf("\n=== Summary: %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
