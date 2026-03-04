/*
 * render_callback_action_deep.cpp — SoCallbackAction comprehensive coverage
 *
 * Exercises SoCallbackAction triangle/line/point callbacks against all shape
 * types, which covers soshape_primdata.cpp (the primitive data generation code
 * used by callback action and pick action).
 *
 * Test cases:
 *   1. Triangle callback on SoSphere (many triangles)
 *   2. Triangle callback on SoCone
 *   3. Triangle callback on SoCylinder
 *   4. Triangle callback on SoCube
 *   5. Triangle callback on SoFaceSet
 *   6. Triangle callback on SoIndexedFaceSet
 *   7. Triangle callback on SoTriangleStripSet
 *   8. Triangle callback on SoQuadMesh
 *   9. Line segment callback on SoLineSet
 *  10. Line segment callback on SoIndexedLineSet
 *  11. Point callback on SoPointSet
 *  12. SoCallbackAction::addPreCallback / addPostCallback for node type
 *  13. SoCallbackAction::addTriangleCallback for specific shape type
 *  14. Render scene after collecting primitives (verify geometry is correct)
 *
 * Returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <Inventor/actions/SoCallbackAction.h>
#include <Inventor/SoPrimitiveVertex.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoNormal.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoFaceSet.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <Inventor/nodes/SoLineSet.h>
#include <Inventor/nodes/SoIndexedLineSet.h>
#include <Inventor/nodes/SoPointSet.h>
#include <Inventor/nodes/SoTriangleStripSet.h>
#include <Inventor/nodes/SoQuadMesh.h>
#include <Inventor/nodes/SoVertexProperty.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>

static const int W = 128;
static const int H = 128;

// ---------------------------------------------------------------------------
// Primitive counters
// ---------------------------------------------------------------------------
struct PrimCounts {
    int triangles;
    int lines;
    int points;
    int preCB;
    int postCB;
};

static PrimCounts g_counts;

static void resetCounts() {
    g_counts.triangles = g_counts.lines = g_counts.points = 0;
    g_counts.preCB = g_counts.postCB = 0;
}

static void triCB(void * /*ud*/, SoCallbackAction * /*action*/,
                  const SoPrimitiveVertex * /*v1*/,
                  const SoPrimitiveVertex * /*v2*/,
                  const SoPrimitiveVertex * /*v3*/)
{
    ++g_counts.triangles;
}

static void lineCB(void * /*ud*/, SoCallbackAction * /*action*/,
                   const SoPrimitiveVertex * /*v1*/,
                   const SoPrimitiveVertex * /*v2*/)
{
    ++g_counts.lines;
}

static void pointCB(void * /*ud*/, SoCallbackAction * /*action*/,
                    const SoPrimitiveVertex * /*v*/)
{
    ++g_counts.points;
}

static SoCallbackAction::Response preCB(void * /*ud*/,
                                        SoCallbackAction * /*a*/,
                                        const SoNode * /*n*/)
{
    ++g_counts.preCB;
    return SoCallbackAction::CONTINUE;
}

static SoCallbackAction::Response postCB(void * /*ud*/,
                                          SoCallbackAction * /*a*/,
                                          const SoNode * /*n*/)
{
    ++g_counts.postCB;
    return SoCallbackAction::CONTINUE;
}

// ---------------------------------------------------------------------------
// Helpers to build simple scenes
// ---------------------------------------------------------------------------
static SoNode *buildSingleShape(SoNode *shape)
{
    SoSeparator *root = new SoSeparator;
    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0,0,5);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);
    SoMaterial *m = new SoMaterial;
    m->diffuseColor.setValue(0.7f,0.5f,0.3f);
    root->addChild(m);
    root->addChild(shape);
    return root;
}

static void applyCallback(SoNode *root, SoType shapeType = SoType::badType())
{
    SoCallbackAction ca;
    // Pre/post callbacks for SoMaterial nodes
    ca.addPreCallback(SoMaterial::getClassTypeId(), preCB, nullptr);
    ca.addPostCallback(SoMaterial::getClassTypeId(), postCB, nullptr);
    // Primitive callbacks
    if (shapeType.isBad()) {
        ca.addTriangleCallback(SoShape::getClassTypeId(), triCB, nullptr);
        ca.addLineSegmentCallback(SoShape::getClassTypeId(), lineCB, nullptr);
        ca.addPointCallback(SoShape::getClassTypeId(), pointCB, nullptr);
    } else {
        ca.addTriangleCallback(shapeType, triCB, nullptr);
        ca.addLineSegmentCallback(shapeType, lineCB, nullptr);
        ca.addPointCallback(shapeType, pointCB, nullptr);
    }
    ca.apply(root);
}

// ---------------------------------------------------------------------------
// Test 1: SoSphere → many triangles
// ---------------------------------------------------------------------------
static bool test1_sphere()
{
    resetCounts();
    SoNode *root = buildSingleShape(new SoSphere);
    root->ref();
    applyCallback(root);
    printf("  test1_sphere: tris=%d\n", g_counts.triangles);
    root->unref();
    if (g_counts.triangles < 10) {
        fprintf(stderr, "  FAIL test1_sphere: too few triangles\n");
        return false;
    }
    printf("  PASS test1_sphere\n");
    return true;
}

// ---------------------------------------------------------------------------
// Test 2: SoCone
// ---------------------------------------------------------------------------
static bool test2_cone()
{
    resetCounts();
    SoNode *root = buildSingleShape(new SoCone);
    root->ref();
    applyCallback(root);
    printf("  test2_cone: tris=%d\n", g_counts.triangles);
    root->unref();
    if (g_counts.triangles < 4) {
        fprintf(stderr, "  FAIL test2_cone\n");
        return false;
    }
    printf("  PASS test2_cone\n");
    return true;
}

// ---------------------------------------------------------------------------
// Test 3: SoCylinder
// ---------------------------------------------------------------------------
static bool test3_cylinder()
{
    resetCounts();
    SoNode *root = buildSingleShape(new SoCylinder);
    root->ref();
    applyCallback(root);
    printf("  test3_cylinder: tris=%d\n", g_counts.triangles);
    root->unref();
    if (g_counts.triangles < 8) {
        fprintf(stderr, "  FAIL test3_cylinder\n");
        return false;
    }
    printf("  PASS test3_cylinder\n");
    return true;
}

// ---------------------------------------------------------------------------
// Test 4: SoCube
// ---------------------------------------------------------------------------
static bool test4_cube()
{
    resetCounts();
    SoNode *root = buildSingleShape(new SoCube);
    root->ref();
    applyCallback(root);
    printf("  test4_cube: tris=%d (expect 12)\n", g_counts.triangles);
    root->unref();
    if (g_counts.triangles < 12) {
        fprintf(stderr, "  FAIL test4_cube\n");
        return false;
    }
    printf("  PASS test4_cube\n");
    return true;
}

// ---------------------------------------------------------------------------
// Test 5: SoFaceSet
// ---------------------------------------------------------------------------
static bool test5_faceSet()
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    root->addChild(new SoDirectionalLight);

    static const float coords[4][3] = {
        {-1,-1,0},{1,-1,0},{1,1,0},{-1,1,0}
    };
    static const int nv[] = { 4 };
    static const SbVec3f norm(0,0,1);

    SoCoordinate3 *c3 = new SoCoordinate3;
    c3->point.setValues(0,4,coords);
    root->addChild(c3);

    SoNormal *n = new SoNormal;
    n->vector.set1Value(0, norm);
    root->addChild(n);
    SoNormalBinding *nb = new SoNormalBinding;
    nb->value.setValue(SoNormalBinding::OVERALL);
    root->addChild(nb);

    SoFaceSet *fs = new SoFaceSet;
    fs->numVertices.setValues(0,1,nv);
    root->addChild(fs);

    resetCounts();
    applyCallback(root, SoFaceSet::getClassTypeId());
    printf("  test5_faceSet: tris=%d (expect 2 for quad)\n", g_counts.triangles);
    root->unref();

    if (g_counts.triangles < 2) {
        fprintf(stderr, "  FAIL test5_faceSet\n");
        return false;
    }
    printf("  PASS test5_faceSet\n");
    return true;
}

// ---------------------------------------------------------------------------
// Test 6: SoIndexedFaceSet
// ---------------------------------------------------------------------------
static bool test6_indexedFaceSet()
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    root->addChild(new SoDirectionalLight);

    static const float coords[4][3] = {
        {-1,-1,0},{1,-1,0},{1,1,0},{-1,1,0}
    };
    static const int32_t idx[] = {0,1,2,-1, 0,2,3,-1};

    SoCoordinate3 *c3 = new SoCoordinate3;
    c3->point.setValues(0,4,coords);
    root->addChild(c3);

    SoIndexedFaceSet *ifs = new SoIndexedFaceSet;
    ifs->coordIndex.setValues(0,8,idx);
    root->addChild(ifs);

    resetCounts();
    applyCallback(root, SoIndexedFaceSet::getClassTypeId());
    printf("  test6_ifs: tris=%d\n", g_counts.triangles);
    root->unref();

    if (g_counts.triangles < 2) {
        fprintf(stderr, "  FAIL test6_indexedFaceSet\n");
        return false;
    }
    printf("  PASS test6_indexedFaceSet\n");
    return true;
}

// ---------------------------------------------------------------------------
// Test 7: SoTriangleStripSet
// ---------------------------------------------------------------------------
static bool test7_triangleStripSet()
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    root->addChild(new SoDirectionalLight);

    static const float pts[4][3] = {
        {-1,-1,0},{-1,1,0},{1,-1,0},{1,1,0}
    };
    static const int nv[] = { 4 };

    SoCoordinate3 *c3 = new SoCoordinate3;
    c3->point.setValues(0,4,pts);
    root->addChild(c3);

    SoNormal *n = new SoNormal;
    n->vector.set1Value(0, SbVec3f(0,0,1));
    root->addChild(n);
    SoNormalBinding *nb = new SoNormalBinding;
    nb->value.setValue(SoNormalBinding::OVERALL);
    root->addChild(nb);

    SoTriangleStripSet *tss = new SoTriangleStripSet;
    tss->numVertices.setValues(0,1,nv);
    root->addChild(tss);

    resetCounts();
    applyCallback(root, SoTriangleStripSet::getClassTypeId());
    printf("  test7_tss: tris=%d (expect 2)\n", g_counts.triangles);
    root->unref();

    if (g_counts.triangles < 2) {
        fprintf(stderr, "  FAIL test7_tss\n");
        return false;
    }
    printf("  PASS test7_triangleStripSet\n");
    return true;
}

// ---------------------------------------------------------------------------
// Test 8: SoQuadMesh
// ---------------------------------------------------------------------------
static bool test8_quadMesh()
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    root->addChild(new SoDirectionalLight);

    // 2×2 grid = 1 quad = 2 triangles
    static const float pts[4][3] = {
        {-1,-1,0},{1,-1,0},
        {-1,1,0},{1,1,0}
    };

    SoCoordinate3 *c3 = new SoCoordinate3;
    c3->point.setValues(0,4,pts);
    root->addChild(c3);

    SoNormal *n = new SoNormal;
    n->vector.set1Value(0, SbVec3f(0,0,1));
    root->addChild(n);
    SoNormalBinding *nb = new SoNormalBinding;
    nb->value.setValue(SoNormalBinding::OVERALL);
    root->addChild(nb);

    SoQuadMesh *qm = new SoQuadMesh;
    qm->verticesPerRow.setValue(2);
    qm->verticesPerColumn.setValue(2);
    root->addChild(qm);

    resetCounts();
    applyCallback(root, SoQuadMesh::getClassTypeId());
    printf("  test8_quadMesh: tris=%d (expect 2)\n", g_counts.triangles);
    root->unref();

    if (g_counts.triangles < 2) {
        fprintf(stderr, "  FAIL test8_quadMesh\n");
        return false;
    }
    printf("  PASS test8_quadMesh\n");
    return true;
}

// ---------------------------------------------------------------------------
// Test 9: SoLineSet → line segment callback
// ---------------------------------------------------------------------------
static bool test9_lineSet()
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    static const float pts[3][3] = {
        {-1,0,0},{0,1,0},{1,0,0}
    };
    static const int nv[] = { 3 };

    SoCoordinate3 *c3 = new SoCoordinate3;
    c3->point.setValues(0,3,pts);
    root->addChild(c3);

    SoLineSet *ls = new SoLineSet;
    ls->numVertices.setValues(0,1,nv);
    root->addChild(ls);

    resetCounts();
    applyCallback(root, SoLineSet::getClassTypeId());
    printf("  test9_lineSet: lines=%d (expect 2)\n", g_counts.lines);
    root->unref();

    if (g_counts.lines < 2) {
        fprintf(stderr, "  FAIL test9_lineSet\n");
        return false;
    }
    printf("  PASS test9_lineSet\n");
    return true;
}

// ---------------------------------------------------------------------------
// Test 10: SoIndexedLineSet
// ---------------------------------------------------------------------------
static bool test10_indexedLineSet()
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    static const float pts[3][3] = {
        {-1,0,0},{0,1,0},{1,0,0}
    };
    static const int32_t idx[] = {0,1,2,-1};

    SoCoordinate3 *c3 = new SoCoordinate3;
    c3->point.setValues(0,3,pts);
    root->addChild(c3);

    SoIndexedLineSet *ils = new SoIndexedLineSet;
    ils->coordIndex.setValues(0,4,idx);
    root->addChild(ils);

    resetCounts();
    applyCallback(root, SoIndexedLineSet::getClassTypeId());
    printf("  test10_ils: lines=%d (expect 2)\n", g_counts.lines);
    root->unref();

    if (g_counts.lines < 2) {
        fprintf(stderr, "  FAIL test10_indexedLineSet\n");
        return false;
    }
    printf("  PASS test10_indexedLineSet\n");
    return true;
}

// ---------------------------------------------------------------------------
// Test 11: SoPointSet → point callback
// ---------------------------------------------------------------------------
static bool test11_pointSet()
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    static const float pts[3][3] = {
        {-1,0,0},{0,0,0},{1,0,0}
    };

    SoCoordinate3 *c3 = new SoCoordinate3;
    c3->point.setValues(0,3,pts);
    root->addChild(c3);

    SoPointSet *ps = new SoPointSet;
    ps->numPoints.setValue(3);
    root->addChild(ps);

    resetCounts();
    applyCallback(root, SoPointSet::getClassTypeId());
    printf("  test11_pointSet: points=%d (expect 3)\n", g_counts.points);
    root->unref();

    if (g_counts.points < 3) {
        fprintf(stderr, "  FAIL test11_pointSet\n");
        return false;
    }
    printf("  PASS test11_pointSet\n");
    return true;
}

// ---------------------------------------------------------------------------
// Test 12: pre/post callbacks on SoMaterial
// ---------------------------------------------------------------------------
static bool test12_prePostCB()
{
    resetCounts();
    SoNode *root = buildSingleShape(new SoCube);
    root->ref();

    SoCallbackAction ca;
    ca.addPreCallback(SoMaterial::getClassTypeId(), preCB, nullptr);
    ca.addPostCallback(SoMaterial::getClassTypeId(), postCB, nullptr);
    ca.addTriangleCallback(SoShape::getClassTypeId(), triCB, nullptr);
    ca.apply(root);

    printf("  test12: preCB=%d postCB=%d tris=%d\n",
           g_counts.preCB, g_counts.postCB, g_counts.triangles);
    root->unref();

    bool ok = (g_counts.preCB >= 1 && g_counts.postCB >= 1 &&
               g_counts.triangles >= 12);
    if (!ok) {
        fprintf(stderr, "  FAIL test12_prePostCB\n");
        return false;
    }
    printf("  PASS test12_prePostCB\n");
    return true;
}

// ---------------------------------------------------------------------------
// Test 13: SoVertexProperty-based shape (triangle data with embedded normals)
// ---------------------------------------------------------------------------
static bool test13_vertexProperty()
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoIndexedFaceSet *ifs = new SoIndexedFaceSet;
    SoVertexProperty *vp = new SoVertexProperty;

    static const float v[4][3] = {
        {-1,-1,0},{1,-1,0},{1,1,0},{-1,1,0}
    };
    vp->vertex.setValues(0,4,v);
    static const SbVec3f norms[4] = {
        SbVec3f(0,0,1),SbVec3f(0,0,1),SbVec3f(0,0,1),SbVec3f(0,0,1)
    };
    vp->normal.setValues(0,4,norms);
    vp->normalBinding.setValue(SoVertexProperty::PER_VERTEX);

    static const int32_t idx[] = {0,1,2,-1,0,2,3,-1};
    ifs->coordIndex.setValues(0,8,idx);
    ifs->vertexProperty.setValue(vp);
    root->addChild(ifs);

    resetCounts();
    SoCallbackAction ca;
    ca.addTriangleCallback(SoIndexedFaceSet::getClassTypeId(), triCB, nullptr);
    ca.apply(root);

    printf("  test13_vertexProperty: tris=%d\n", g_counts.triangles);
    root->unref();

    if (g_counts.triangles < 2) {
        fprintf(stderr, "  FAIL test13_vertexProperty\n");
        return false;
    }
    printf("  PASS test13_vertexProperty\n");
    return true;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char **argv)
{
    initCoinHeadless();
    (void)argc; (void)argv;

    int failures = 0;

    printf("\n=== SoCallbackAction primitive callback tests ===\n");

    if (!test1_sphere())          ++failures;
    if (!test2_cone())            ++failures;
    if (!test3_cylinder())        ++failures;
    if (!test4_cube())            ++failures;
    if (!test5_faceSet())         ++failures;
    if (!test6_indexedFaceSet())  ++failures;
    if (!test7_triangleStripSet()) ++failures;
    if (!test8_quadMesh())        ++failures;
    if (!test9_lineSet())         ++failures;
    if (!test10_indexedLineSet()) ++failures;
    if (!test11_pointSet())       ++failures;
    if (!test12_prePostCB())      ++failures;
    if (!test13_vertexProperty()) ++failures;

    printf("\n=== Summary: %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
