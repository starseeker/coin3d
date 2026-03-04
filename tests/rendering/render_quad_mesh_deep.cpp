/*
 * render_quad_mesh_deep.cpp — SoQuadMesh deep material/normal binding coverage
 *
 * Exercises SoQuadMesh rendering paths not covered by render_quad_mesh.cpp:
 *   1. OVERALL material binding (single colour for all faces)
 *   2. PER_FACE material binding (one colour per quad face)
 *   3. PER_PART material binding (one colour per row)
 *   4. Auto-generated normals (SoShapeHints triggers normal generation,
 *      no explicit SoNormal node)
 *   5. Texture coordinates on a quad mesh (SoTextureCoordinate2 +
 *      SoTextureCoordinateBinding PER_VERTEX)
 *   6. SoGetPrimitiveCountAction on a quad mesh
 *   7. Wireframe draw style (SoDrawStyle LINES)
 *
 * Returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoMaterialBinding.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoNormal.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/nodes/SoQuadMesh.h>
#include <Inventor/nodes/SoShapeHints.h>
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/nodes/SoTextureCoordinate2.h>
#include <Inventor/nodes/SoTextureCoordinateBinding.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/actions/SoGetPrimitiveCountAction.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>
#include <cstring>

static const int W = 256;
static const int H = 256;

/* 4×4 vertex grid → 3×3 = 9 quad faces */
static const int GRID_ROWS = 4;
static const int GRID_COLS = 4;
static const float CELL = 0.6f; /* world-space cell size */

static bool validateNonBlack(const unsigned char *buf, int npix, const char *lbl)
{
    int nonbg = 0;
    for (int i = 0; i < npix; ++i) {
        const unsigned char *p = buf + i * 3;
        if (p[0] > 5 || p[1] > 5 || p[2] > 5) ++nonbg;
    }
    printf("  %s: nonbg=%d\n", lbl, nonbg);
    return nonbg >= 50;
}

/* Build a flat GRID_ROWS × GRID_COLS coordinate grid on the XY plane */
static SoCoordinate3 *buildGrid()
{
    SoCoordinate3 *coords = new SoCoordinate3;
    int idx = 0;
    for (int r = 0; r < GRID_ROWS; ++r) {
        for (int c = 0; c < GRID_COLS; ++c) {
            float x = (c - (GRID_COLS - 1) * 0.5f) * CELL;
            float y = (r - (GRID_ROWS - 1) * 0.5f) * CELL;
            coords->point.set1Value(idx++, SbVec3f(x, y, 0.0f));
        }
    }
    return coords;
}

static SoQuadMesh *buildMesh()
{
    SoQuadMesh *qm = new SoQuadMesh;
    qm->verticesPerRow.setValue(GRID_COLS);
    qm->verticesPerColumn.setValue(GRID_ROWS);
    return qm;
}

static bool renderScene(SoNode *root, const char *label)
{
    SbViewportRegion vp(W, H);
    SoOffscreenRenderer ren(vp);
    ren.setComponents(SoOffscreenRenderer::RGB);
    ren.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
    if (!ren.render(root)) {
        fprintf(stderr, "  %s: render() failed\n", label);
        return false;
    }
    return validateNonBlack(ren.getBuffer(), W * H, label);
}

/* -------------------------------------------------------------------------
 * Test 1: OVERALL material binding
 * ----------------------------------------------------------------------- */
static bool test1_overall(const char *basepath)
{
    SoSeparator *root = new SoSeparator; root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 6.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoMaterialBinding *mb = new SoMaterialBinding;
    mb->value.setValue(SoMaterialBinding::OVERALL);
    root->addChild(mb);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(SbColor(0.8f, 0.3f, 0.2f));
    root->addChild(mat);

    /* Explicit normals (all +Z) */
    SoNormal *norms = new SoNormal;
    for (int i = 0; i < GRID_ROWS * GRID_COLS; ++i)
        norms->vector.set1Value(i, SbVec3f(0.0f, 0.0f, 1.0f));
    root->addChild(norms);
    SoNormalBinding *nb = new SoNormalBinding;
    nb->value.setValue(SoNormalBinding::PER_VERTEX);
    root->addChild(nb);

    root->addChild(buildGrid());
    root->addChild(buildMesh());

    char path[1024];
    snprintf(path, sizeof(path), "%s_overall.rgb", basepath);
    bool ok = renderScene(root, "test1_overall");
    if (ok) {
        SoOffscreenRenderer ren(SbViewportRegion(W, H));
        ren.render(root);
        ren.writeToRGB(path);
    }

    root->unref();
    return ok;
}

/* -------------------------------------------------------------------------
 * Test 2: PER_FACE material binding (one colour per quad)
 * ----------------------------------------------------------------------- */
static bool test2_perFace(const char *basepath)
{
    SoSeparator *root = new SoSeparator; root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 6.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoMaterialBinding *mb = new SoMaterialBinding;
    mb->value.setValue(SoMaterialBinding::PER_FACE);
    root->addChild(mb);

    /* 9 faces → 9 colours */
    SoMaterial *mat = new SoMaterial;
    const int NFACES = (GRID_ROWS - 1) * (GRID_COLS - 1);
    for (int i = 0; i < NFACES; ++i) {
        float r = (i % 3) == 0 ? 0.8f : 0.2f;
        float g = (i % 3) == 1 ? 0.8f : 0.2f;
        float b = (i % 3) == 2 ? 0.8f : 0.2f;
        mat->diffuseColor.set1Value(i, SbColor(r, g, b));
    }
    root->addChild(mat);

    /* Explicit normals */
    SoNormal *norms = new SoNormal;
    for (int i = 0; i < GRID_ROWS * GRID_COLS; ++i)
        norms->vector.set1Value(i, SbVec3f(0.0f, 0.0f, 1.0f));
    root->addChild(norms);
    SoNormalBinding *nb = new SoNormalBinding;
    nb->value.setValue(SoNormalBinding::PER_VERTEX);
    root->addChild(nb);

    root->addChild(buildGrid());
    root->addChild(buildMesh());

    char path[1024];
    snprintf(path, sizeof(path), "%s_perface.rgb", basepath);
    bool ok = renderScene(root, "test2_perFace");
    if (ok) {
        SoOffscreenRenderer ren(SbViewportRegion(W, H));
        ren.render(root);
        ren.writeToRGB(path);
    }

    root->unref();
    return ok;
}

/* -------------------------------------------------------------------------
 * Test 3: PER_PART material binding (one colour per row)
 * ----------------------------------------------------------------------- */
static bool test3_perPart(const char *basepath)
{
    SoSeparator *root = new SoSeparator; root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 6.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoMaterialBinding *mb = new SoMaterialBinding;
    mb->value.setValue(SoMaterialBinding::PER_PART);
    root->addChild(mb);

    /* 3 rows → 3 colours */
    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.set1Value(0, SbColor(0.9f, 0.1f, 0.1f)); /* red    */
    mat->diffuseColor.set1Value(1, SbColor(0.1f, 0.9f, 0.1f)); /* green  */
    mat->diffuseColor.set1Value(2, SbColor(0.1f, 0.1f, 0.9f)); /* blue   */
    root->addChild(mat);

    SoNormal *norms = new SoNormal;
    for (int i = 0; i < GRID_ROWS * GRID_COLS; ++i)
        norms->vector.set1Value(i, SbVec3f(0.0f, 0.0f, 1.0f));
    root->addChild(norms);
    SoNormalBinding *nb = new SoNormalBinding;
    nb->value.setValue(SoNormalBinding::PER_VERTEX);
    root->addChild(nb);

    root->addChild(buildGrid());
    root->addChild(buildMesh());

    char path[1024];
    snprintf(path, sizeof(path), "%s_perpart.rgb", basepath);
    bool ok = renderScene(root, "test3_perPart");
    if (ok) {
        SoOffscreenRenderer ren(SbViewportRegion(W, H));
        ren.render(root);
        ren.writeToRGB(path);
    }

    root->unref();
    return ok;
}

/* -------------------------------------------------------------------------
 * Test 4: Auto-generated normals via SoShapeHints (no explicit SoNormal)
 * ----------------------------------------------------------------------- */
static bool test4_autoNormals(const char *basepath)
{
    SoSeparator *root = new SoSeparator; root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 8.0f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 50.0f;
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    /* SoShapeHints: two-sided lighting so we can see from any angle;
       use UNKNOWN_SHAPE_TYPE to avoid backface culling surprises */
    SoShapeHints *sh = new SoShapeHints;
    sh->vertexOrdering.setValue(SoShapeHints::COUNTERCLOCKWISE);
    sh->shapeType.setValue(SoShapeHints::UNKNOWN_SHAPE_TYPE);
    root->addChild(sh);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(SbColor(0.5f, 0.7f, 0.3f));
    root->addChild(mat);

    /* Plain flat grid — auto-normals (+Z) will be generated */
    root->addChild(buildGrid());
    root->addChild(buildMesh());

    /* Align camera to see the whole scene */
    SbViewportRegion vp(W, H);
    cam->viewAll(root, vp);

    char path[1024];
    snprintf(path, sizeof(path), "%s_autonorm.rgb", basepath);
    bool ok = renderScene(root, "test4_autoNormals");
    if (ok) {
        SoOffscreenRenderer ren(SbViewportRegion(W, H));
        ren.render(root);
        ren.writeToRGB(path);
    }

    root->unref();
    return ok;
}

/* -------------------------------------------------------------------------
 * Test 5: Texture coordinates PER_VERTEX on a quad mesh
 * ----------------------------------------------------------------------- */
static bool test5_textured(const char *basepath)
{
    SoSeparator *root = new SoSeparator; root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 6.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    /* 4×4 checkerboard texture */
    SoTexture2 *tex = new SoTexture2;
    {
        unsigned char pixels[4 * 4 * 3];
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c) {
                unsigned char v = ((r + c) % 2 == 0) ? 210 : 60;
                int base = (r * 4 + c) * 3;
                pixels[base + 0] = v;
                pixels[base + 1] = v;
                pixels[base + 2] = v;
            }
        tex->image.setValue(SbVec2s(4, 4), 3, pixels);
    }
    root->addChild(tex);

    SoTextureCoordinateBinding *tcb = new SoTextureCoordinateBinding;
    tcb->value.setValue(SoTextureCoordinateBinding::PER_VERTEX);
    root->addChild(tcb);

    /* One tex-coord per vertex */
    SoTextureCoordinate2 *tc = new SoTextureCoordinate2;
    int idx = 0;
    for (int r = 0; r < GRID_ROWS; ++r)
        for (int c = 0; c < GRID_COLS; ++c)
            tc->point.set1Value(idx++,
                SbVec2f(c / (float)(GRID_COLS - 1),
                        r / (float)(GRID_ROWS - 1)));
    root->addChild(tc);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(SbColor(1.0f, 1.0f, 1.0f));
    root->addChild(mat);

    SoNormal *norms = new SoNormal;
    for (int i = 0; i < GRID_ROWS * GRID_COLS; ++i)
        norms->vector.set1Value(i, SbVec3f(0.0f, 0.0f, 1.0f));
    root->addChild(norms);
    SoNormalBinding *nb = new SoNormalBinding;
    nb->value.setValue(SoNormalBinding::PER_VERTEX);
    root->addChild(nb);

    root->addChild(buildGrid());
    root->addChild(buildMesh());

    char path[1024];
    snprintf(path, sizeof(path), "%s_tex.rgb", basepath);
    bool ok = renderScene(root, "test5_textured");
    if (ok) {
        SoOffscreenRenderer ren(SbViewportRegion(W, H));
        ren.render(root);
        ren.writeToRGB(path);
    }

    root->unref();
    return ok;
}

/* -------------------------------------------------------------------------
 * Test 6: SoGetPrimitiveCountAction
 * ----------------------------------------------------------------------- */
static bool test6_primitiveCount(const char * /*basepath*/)
{
    SoSeparator *root = new SoSeparator; root->ref();

    root->addChild(new SoPerspectiveCamera);
    root->addChild(buildGrid());
    root->addChild(buildMesh());

    SbViewportRegion vp(W, H);
    SoGetPrimitiveCountAction pca(vp);
    pca.apply(root);

    int tris = pca.getTriangleCount();
    printf("  test6_primCount: triangles=%d\n", tris);

    root->unref();
    /* The count should be positive; exact value depends on internal
       triangulation (may be 2 or more triangles per quad face) */
    return (tris > 0);
}

/* -------------------------------------------------------------------------
 * Test 7: Wireframe (SoDrawStyle LINES)
 * ----------------------------------------------------------------------- */
static bool test7_wireframe(const char *basepath)
{
    SoSeparator *root = new SoSeparator; root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 6.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoDrawStyle *ds = new SoDrawStyle;
    ds->style.setValue(SoDrawStyle::LINES);
    root->addChild(ds);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(SbColor(0.9f, 0.9f, 0.2f));
    root->addChild(mat);

    root->addChild(buildGrid());
    root->addChild(buildMesh());

    char path[1024];
    snprintf(path, sizeof(path), "%s_wire.rgb", basepath);
    bool ok = renderScene(root, "test7_wireframe");
    if (ok) {
        SoOffscreenRenderer ren(SbViewportRegion(W, H));
        ren.render(root);
        ren.writeToRGB(path);
    }

    root->unref();
    return ok;
}

/* -------------------------------------------------------------------------
 * main
 * ----------------------------------------------------------------------- */
int main(int argc, char **argv)
{
    initCoinHeadless();

    const char *basepath =
        (argc > 1) ? argv[1] : "render_quad_mesh_deep";

    /* Render the canonical factory scene as the primary output image.
     * This ensures obol_viewer and obol_render produce identical scenes. */
    {
        SoSeparator *root = ObolTest::Scenes::createQuadMeshDeep(256, 256);
        SbViewportRegion vp(256, 256);
        SoOffscreenRenderer ren(vp);
        ren.setComponents(SoOffscreenRenderer::RGB);
        ren.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
        if (ren.render(root)) {
            char primaryPath[4096];
            snprintf(primaryPath, sizeof(primaryPath), "%s.rgb", basepath);
            ren.writeToRGB(primaryPath);
        }
        root->unref();
    }

    printf("\n=== SoQuadMesh deep binding/normal tests ===\n");

    int failures = 0;
    if (!test1_overall(basepath))      { printf("FAIL test1\n"); ++failures; }
    if (!test2_perFace(basepath))      { printf("FAIL test2\n"); ++failures; }
    if (!test3_perPart(basepath))      { printf("FAIL test3\n"); ++failures; }
    if (!test4_autoNormals(basepath))  { printf("FAIL test4\n"); ++failures; }
    if (!test5_textured(basepath))     { printf("FAIL test5\n"); ++failures; }
    if (!test6_primitiveCount(basepath)){ printf("FAIL test6\n"); ++failures; }
    if (!test7_wireframe(basepath))    { printf("FAIL test7\n"); ++failures; }

    printf("\n=== Summary: %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
