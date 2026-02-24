/*
 * render_sogl_bindings.cpp — SoGL template coverage: PointSet/IndexedLineSet
 *
 * Exercises the static template instantiations in SoGL.cpp that are selected
 * based on (material-per-vertex, normals, textures) flags for SoPointSet, and
 * the (NormalBinding, MaterialBinding, TexturingEnabled) templates for
 * SoIndexedLineSet.
 *
 * SoPointSet dispatches to one of 8 sogl_render_pointset_m{0,1}n{0,1}t{0,1}
 * variants at run time.  The existing render_point_set test only exercises
 * m0n0t0 (OVERALL material, no normals, no texture).  This test covers the
 * other seven variants as well as several additional IndexedLineSet bindings.
 *
 * Tests:
 *   --- SoPointSet ---
 *   1. m0n0t0  OVERALL material, no normals, no texture     [already covered]
 *   2. m0n1t0  OVERALL material, PER_VERTEX normals, no tex
 *   3. m0n0t1  OVERALL material, no normals, with texture
 *   4. m0n1t1  OVERALL material, PER_VERTEX normals, texture
 *   5. m1n0t0  PER_VERTEX material, no normals, no texture
 *   6. m1n1t0  PER_VERTEX material, PER_VERTEX normals, no tex
 *   7. m1n0t1  PER_VERTEX material, no normals, texture
 *   8. m1n1t1  PER_VERTEX material, PER_VERTEX normals, texture
 *   --- SoIndexedLineSet ---
 *   9. PER_LINE material binding (per-polyline colour)
 *  10. PER_VERTEX material binding on IndexedLineSet
 *  11. PER_LINE with texture coordinates
 *  12. PER_VERTEX with texture coordinates
 *
 * Returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoMaterialBinding.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoNormal.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/nodes/SoPointSet.h>
#include <Inventor/nodes/SoIndexedLineSet.h>
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/nodes/SoTextureCoordinate2.h>
#include <Inventor/nodes/SoTextureCoordinateBinding.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>

static const int W = 128;
static const int H = 128;

/* 9 points arranged in a 3×3 grid */
static const int NPTS = 9;
static const SbVec3f PTS[NPTS] = {
    SbVec3f(-0.6f,  0.6f, 0.0f), SbVec3f(0.0f,  0.6f, 0.0f), SbVec3f(0.6f,  0.6f, 0.0f),
    SbVec3f(-0.6f,  0.0f, 0.0f), SbVec3f(0.0f,  0.0f, 0.0f), SbVec3f(0.6f,  0.0f, 0.0f),
    SbVec3f(-0.6f, -0.6f, 0.0f), SbVec3f(0.0f, -0.6f, 0.0f), SbVec3f(0.6f, -0.6f, 0.0f),
};
/* All normals face +Z */
static const SbVec3f NORMS[NPTS] = {
    SbVec3f(0,0,1), SbVec3f(0,0,1), SbVec3f(0,0,1),
    SbVec3f(0,0,1), SbVec3f(0,0,1), SbVec3f(0,0,1),
    SbVec3f(0,0,1), SbVec3f(0,0,1), SbVec3f(0,0,1),
};

static bool validateNonBlack(const unsigned char *buf, int npix, const char *lbl)
{
    int nonbg = 0;
    for (int i = 0; i < npix; ++i) {
        const unsigned char *p = buf + i * 3;
        if (p[0] > 5 || p[1] > 5 || p[2] > 5) ++nonbg;
    }
    printf("  %s: nonbg=%d\n", lbl, nonbg);
    return nonbg >= 2;  /* point sets render very few pixels */
}

static SoOrthographicCamera *makeOrthoCamera()
{
    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 2.0f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 10.0f;
    cam->height       = 2.2f;
    return cam;
}

static SoCoordinate3 *makeCoords()
{
    SoCoordinate3 *c = new SoCoordinate3;
    c->point.setValues(0, NPTS, PTS);
    return c;
}

static SoNormal *makeNormals()
{
    SoNormal *n = new SoNormal;
    n->vector.setValues(0, NPTS, NORMS);
    return n;
}

/* 8×8 white checkerboard texture */
static SoTexture2 *makeTexture()
{
    SoTexture2 *tex = new SoTexture2;
    unsigned char px[8 * 8 * 3];
    for (int r = 0; r < 8; ++r)
        for (int c = 0; c < 8; ++c) {
            unsigned char v = ((r + c) % 2 == 0) ? 200 : 80;
            int base = (r * 8 + c) * 3;
            px[base] = px[base+1] = px[base+2] = v;
        }
    tex->image.setValue(SbVec2s(8, 8), 3, px);
    return tex;
}

static SoTextureCoordinate2 *makeTexCoords()
{
    SoTextureCoordinate2 *tc = new SoTextureCoordinate2;
    for (int i = 0; i < NPTS; ++i) {
        int r = i / 3, c = i % 3;
        tc->point.set1Value(i, SbVec2f(c * 0.5f, r * 0.5f));
    }
    return tc;
}

static bool renderAndCheck(SoNode *root, const char *lbl,
                           const char *basepath, const char *suffix)
{
    SbViewportRegion vp(W, H);
    SoOffscreenRenderer ren(vp);
    ren.setComponents(SoOffscreenRenderer::RGB);
    ren.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
    if (!ren.render(root)) {
        fprintf(stderr, "  %s: render() failed\n", lbl);
        return false;
    }
    bool ok = validateNonBlack(ren.getBuffer(), W * H, lbl);
    char path[1024];
    snprintf(path, sizeof(path), "%s_%s.rgb", basepath, suffix);
    ren.writeToRGB(path);
    return ok;
}

/* ==========================================================================
 * SoPointSet tests
 * ======================================================================== */

/* m=material-per-vertex(0/1), n=normals(0/1), t=texture(0/1) */
static bool testPointSet(int m, int n, int t,
                         const char *basepath, const char *suffix)
{
    SoSeparator *root = new SoSeparator; root->ref();
    root->addChild(makeOrthoCamera());

    /* Light only relevant when normals are present; but include it always */
    root->addChild(new SoDirectionalLight);

    /* Draw large points so they produce visible pixels */
    SoDrawStyle *ds = new SoDrawStyle;
    ds->pointSize.setValue(6.0f);
    root->addChild(ds);

    /* Material */
    if (m) {
        SoMaterialBinding *mb = new SoMaterialBinding;
        mb->value.setValue(SoMaterialBinding::PER_VERTEX);
        root->addChild(mb);
        SoMaterial *mat = new SoMaterial;
        /* Cycle red/green/blue for every 3 points */
        for (int i = 0; i < NPTS; ++i) {
            float r = (i % 3 == 0) ? 0.9f : 0.2f;
            float g = (i % 3 == 1) ? 0.9f : 0.2f;
            float b = (i % 3 == 2) ? 0.9f : 0.2f;
            mat->diffuseColor.set1Value(i, SbColor(r, g, b));
        }
        root->addChild(mat);
    } else {
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(SbColor(0.8f, 0.8f, 0.8f));
        root->addChild(mat);
    }

    /* Texture */
    if (t) {
        root->addChild(makeTexture());
        SoTextureCoordinateBinding *tcb = new SoTextureCoordinateBinding;
        tcb->value.setValue(SoTextureCoordinateBinding::PER_VERTEX);
        root->addChild(tcb);
        root->addChild(makeTexCoords());
    }

    /* Normals */
    if (n) {
        root->addChild(makeNormals());
        SoNormalBinding *nb = new SoNormalBinding;
        nb->value.setValue(SoNormalBinding::PER_VERTEX);
        root->addChild(nb);
    } else {
        /* Force OVERALL normal binding so normals pointer is NULL in dispatch */
        SoNormalBinding *nb = new SoNormalBinding;
        nb->value.setValue(SoNormalBinding::OVERALL);
        root->addChild(nb);
    }

    root->addChild(makeCoords());

    SoPointSet *ps = new SoPointSet;
    ps->numPoints.setValue(NPTS);
    root->addChild(ps);

    bool ok = renderAndCheck(root, suffix, basepath, suffix);
    root->unref();
    return ok;
}

/* ==========================================================================
 * SoIndexedLineSet tests
 * ======================================================================== */

/*
 * Build a scene with two polylines using the given material binding.
 * binding: 0=OVERALL, 1=PER_PART(per-line), 2=PER_VERTEX
 */
static bool testIndexedLineSet(int matbind, int tex,
                               const char *basepath, const char *suffix)
{
    SoSeparator *root = new SoSeparator; root->ref();
    root->addChild(makeOrthoCamera());
    root->addChild(new SoDirectionalLight);

    SoDrawStyle *ds = new SoDrawStyle;
    ds->lineWidth.setValue(3.0f);
    root->addChild(ds);

    const int NCOORDS = 6;
    SoCoordinate3 *coords = new SoCoordinate3;
    /* Line 1: horizontal at Y=+0.5 — 3 vertices */
    coords->point.set1Value(0, SbVec3f(-0.8f,  0.5f, 0.0f));
    coords->point.set1Value(1, SbVec3f( 0.0f,  0.5f, 0.0f));
    coords->point.set1Value(2, SbVec3f( 0.8f,  0.5f, 0.0f));
    /* Line 2: diagonal at Y=-0.5 — 3 vertices */
    coords->point.set1Value(3, SbVec3f(-0.8f, -0.8f, 0.0f));
    coords->point.set1Value(4, SbVec3f( 0.0f, -0.3f, 0.0f));
    coords->point.set1Value(5, SbVec3f( 0.8f, -0.8f, 0.0f));
    root->addChild(coords);

    if (matbind == 0) {
        /* OVERALL */
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(SbColor(0.9f, 0.9f, 0.2f));
        root->addChild(mat);
    } else if (matbind == 1) {
        /* PER_PART = one colour per polyline */
        SoMaterialBinding *mb = new SoMaterialBinding;
        mb->value.setValue(SoMaterialBinding::PER_PART);
        root->addChild(mb);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.set1Value(0, SbColor(0.9f, 0.2f, 0.2f)); /* red  */
        mat->diffuseColor.set1Value(1, SbColor(0.2f, 0.2f, 0.9f)); /* blue */
        root->addChild(mat);
    } else {
        /* PER_VERTEX */
        SoMaterialBinding *mb = new SoMaterialBinding;
        mb->value.setValue(SoMaterialBinding::PER_VERTEX);
        root->addChild(mb);
        SoMaterial *mat = new SoMaterial;
        for (int i = 0; i < NCOORDS; ++i) {
            float r = (i % 3 == 0) ? 0.9f : 0.2f;
            float g = (i % 3 == 1) ? 0.9f : 0.2f;
            float b = (i % 3 == 2) ? 0.9f : 0.2f;
            mat->diffuseColor.set1Value(i, SbColor(r, g, b));
        }
        root->addChild(mat);
    }

    if (tex) {
        root->addChild(makeTexture());
        SoTextureCoordinateBinding *tcb = new SoTextureCoordinateBinding;
        tcb->value.setValue(SoTextureCoordinateBinding::PER_VERTEX);
        root->addChild(tcb);
        SoTextureCoordinate2 *tc = new SoTextureCoordinate2;
        for (int i = 0; i < NCOORDS; ++i)
            tc->point.set1Value(i, SbVec2f(i * 0.2f, 0.5f));
        root->addChild(tc);
    }

    SoIndexedLineSet *ils = new SoIndexedLineSet;
    /* Polyline 1: indices 0,1,2,-1 */
    ils->coordIndex.set1Value(0, 0);
    ils->coordIndex.set1Value(1, 1);
    ils->coordIndex.set1Value(2, 2);
    ils->coordIndex.set1Value(3, -1);
    /* Polyline 2: indices 3,4,5,-1 */
    ils->coordIndex.set1Value(4, 3);
    ils->coordIndex.set1Value(5, 4);
    ils->coordIndex.set1Value(6, 5);
    ils->coordIndex.set1Value(7, -1);
    root->addChild(ils);

    bool ok = renderAndCheck(root, suffix, basepath, suffix);
    root->unref();
    return ok;
}

/* ==========================================================================
 * main
 * ======================================================================== */
int main(int argc, char **argv)
{
    initCoinHeadless();

    const char *basepath =
        (argc > 1) ? argv[1] : "render_sogl_bindings";

    printf("\n=== SoGL PointSet/IndexedLineSet binding variant tests ===\n");

    int failures = 0;

    /* SoPointSet m/n/t variants */
    if (!testPointSet(0, 0, 0, basepath, "ps_m0n0t0"))
        { printf("FAIL ps_m0n0t0\n"); ++failures; }
    if (!testPointSet(0, 1, 0, basepath, "ps_m0n1t0"))
        { printf("FAIL ps_m0n1t0\n"); ++failures; }
    if (!testPointSet(0, 0, 1, basepath, "ps_m0n0t1"))
        { printf("FAIL ps_m0n0t1\n"); ++failures; }
    if (!testPointSet(0, 1, 1, basepath, "ps_m0n1t1"))
        { printf("FAIL ps_m0n1t1\n"); ++failures; }
    if (!testPointSet(1, 0, 0, basepath, "ps_m1n0t0"))
        { printf("FAIL ps_m1n0t0\n"); ++failures; }
    if (!testPointSet(1, 1, 0, basepath, "ps_m1n1t0"))
        { printf("FAIL ps_m1n1t0\n"); ++failures; }
    if (!testPointSet(1, 0, 1, basepath, "ps_m1n0t1"))
        { printf("FAIL ps_m1n0t1\n"); ++failures; }
    if (!testPointSet(1, 1, 1, basepath, "ps_m1n1t1"))
        { printf("FAIL ps_m1n1t1\n"); ++failures; }

    /* SoIndexedLineSet material binding variants */
    if (!testIndexedLineSet(0, 0, basepath, "ils_overall"))
        { printf("FAIL ils_overall\n"); ++failures; }
    if (!testIndexedLineSet(1, 0, basepath, "ils_perline"))
        { printf("FAIL ils_perline\n"); ++failures; }
    if (!testIndexedLineSet(2, 0, basepath, "ils_pervert"))
        { printf("FAIL ils_pervert\n"); ++failures; }
    if (!testIndexedLineSet(0, 1, basepath, "ils_overall_tex"))
        { printf("FAIL ils_overall_tex\n"); ++failures; }
    if (!testIndexedLineSet(1, 1, basepath, "ils_perline_tex"))
        { printf("FAIL ils_perline_tex\n"); ++failures; }
    if (!testIndexedLineSet(2, 1, basepath, "ils_pervert_tex"))
        { printf("FAIL ils_pervert_tex\n"); ++failures; }

    printf("\n=== Summary: %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
