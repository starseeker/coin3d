/*
 * render_callback_action.cpp
 *
 * Tests SoCallbackAction – a traversal action that fires user callbacks for
 * shapes (pre-shape, post-shape, triangles, line segments, points).
 * This is widely used for ray casting, export, spatial indexing, etc.
 *
 * Tests:
 *   1. Pre-shape callback counts shapes in scene.
 *   2. Post-shape callback fires after each shape.
 *   3. Triangle callback collects triangle data from SoCube.
 *   4. Triangle callback on SoSphere: verify normals are unit-length.
 *   5. Pre-node callback for every node type (count SoSeparators visited).
 *   6. SoCallbackAction used for geometry measurement (total triangle count).
 *   7. Interaction: modify scene then recount triangles.
 *
 * Returns 0 on pass, non-0 on failure.
 */

#include "headless_utils.h"

#include <Inventor/SoDB.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/nodes/SoSwitch.h>
#include <Inventor/actions/SoCallbackAction.h>
#include <Inventor/SoPrimitiveVertex.h>

#include <cstdio>
#include <cmath>
#include <vector>

// ---------------------------------------------------------------------------
// Test 1 & 2: Pre/post shape callbacks count shapes
// ---------------------------------------------------------------------------

struct ShapeCounts { int pre; int post; };

static SoCallbackAction::Response preCB(void *ud, SoCallbackAction *, const SoNode *)
{
    ++(static_cast<ShapeCounts*>(ud)->pre);
    return SoCallbackAction::CONTINUE;
}

static SoCallbackAction::Response postCB(void *ud, SoCallbackAction *, const SoNode *)
{
    ++(static_cast<ShapeCounts*>(ud)->post);
    return SoCallbackAction::CONTINUE;
}

static bool test1_preShapeCallback()
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    root->addChild(new SoSphere);
    root->addChild(new SoCube);
    root->addChild(new SoCone);

    ShapeCounts counts = {0,0};
    SoCallbackAction cba;
    // Add pre-callback for all shape types
    cba.addPreCallback(SoSphere::getClassTypeId(),   preCB, &counts);
    cba.addPreCallback(SoCube::getClassTypeId(),     preCB, &counts);
    cba.addPreCallback(SoCone::getClassTypeId(),     preCB, &counts);
    cba.addPostCallback(SoSphere::getClassTypeId(),  postCB, &counts);
    cba.addPostCallback(SoCube::getClassTypeId(),    postCB, &counts);
    cba.addPostCallback(SoCone::getClassTypeId(),    postCB, &counts);
    cba.apply(root);

    bool ok = (counts.pre == 3) && (counts.post == 3);
    if (!ok) fprintf(stderr, "  FAIL test1: pre=%d post=%d (expected 3 each)\n",
                     counts.pre, counts.post);
    else     printf("  PASS test1: pre=%d post=%d callbacks (3 shapes)\n",
                    counts.pre, counts.post);
    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 2: Triangle callback collects triangle data from SoCube
// ---------------------------------------------------------------------------

struct TriData { int count; };

static void triCB(void *ud,
                  SoCallbackAction *,
                  const SoPrimitiveVertex *,
                  const SoPrimitiveVertex *,
                  const SoPrimitiveVertex *)
{
    ++(static_cast<TriData*>(ud)->count);
}

static bool test2_triangleCallback()
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    root->addChild(new SoCube); // 6 faces × 2 triangles = 12

    TriData data = {0};
    SoCallbackAction cba;
    cba.addTriangleCallback(SoCube::getClassTypeId(), triCB, &data);
    cba.apply(root);

    bool ok = (data.count == 12);
    if (!ok) fprintf(stderr, "  FAIL test2: cube triangles=%d (expected 12)\n",
                     data.count);
    else     printf("  PASS test2: SoCube triangle callback: %d triangles\n",
                    data.count);
    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 3: Triangle callback on SoSphere – verify normals are unit-length
// ---------------------------------------------------------------------------

struct SphereNormalData { int count; int badNormals; };

static void sphereTriCB(void *ud,
                        SoCallbackAction *,
                        const SoPrimitiveVertex *v1,
                        const SoPrimitiveVertex *v2,
                        const SoPrimitiveVertex *v3)
{
    SphereNormalData *d = static_cast<SphereNormalData*>(ud);
    ++d->count;
    // Check each vertex normal is approximately unit length
    float lens[3];
    lens[0] = v1->getNormal().length();
    lens[1] = v2->getNormal().length();
    lens[2] = v3->getNormal().length();
    for (int i = 0; i < 3; ++i) {
        if (fabsf(lens[i] - 1.0f) > 0.01f) ++d->badNormals;
    }
}

static bool test3_sphereNormals()
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    root->addChild(new SoSphere);

    SphereNormalData data = {0,0};
    SoCallbackAction cba;
    cba.addTriangleCallback(SoSphere::getClassTypeId(), sphereTriCB, &data);
    cba.apply(root);

    bool ok = (data.count > 0) && (data.badNormals == 0);
    if (!ok) fprintf(stderr, "  FAIL test3: tris=%d badNormals=%d\n",
                     data.count, data.badNormals);
    else     printf("  PASS test3: sphere %d triangles, all normals unit-length\n",
                    data.count);
    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 4: Pre-node callback counts SoSeparators
// ---------------------------------------------------------------------------

struct NodeCount { int sepCount; int shapeCount; };

static SoCallbackAction::Response countSepCB(void *ud, SoCallbackAction *, const SoNode *)
{
    ++(static_cast<NodeCount*>(ud)->sepCount);
    return SoCallbackAction::CONTINUE;
}

static SoCallbackAction::Response countShapeCB(void *ud, SoCallbackAction *, const SoNode *)
{
    ++(static_cast<NodeCount*>(ud)->shapeCount);
    return SoCallbackAction::CONTINUE;
}

static bool test4_nodeCallbackCounting()
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    for (int i = 0; i < 3; ++i) {
        SoSeparator *sep = new SoSeparator;
        sep->addChild(new SoSphere);
        sep->addChild(new SoCube);
        root->addChild(sep);
    }

    NodeCount nc = {0, 0};
    SoCallbackAction cba;
    cba.addPreCallback(SoSeparator::getClassTypeId(), countSepCB,  &nc);
    cba.addPreCallback(SoSphere::getClassTypeId(),    countShapeCB, &nc);
    cba.addPreCallback(SoCube::getClassTypeId(),      countShapeCB, &nc);
    cba.apply(root);

    // root (1 sep) + 3 child seps = 4 seps; 3 spheres + 3 cubes = 6 shapes
    bool ok = (nc.sepCount == 4) && (nc.shapeCount == 6);
    if (!ok) fprintf(stderr, "  FAIL test4: sepCount=%d shapeCount=%d "
                     "(expected 4, 6)\n", nc.sepCount, nc.shapeCount);
    else     printf("  PASS test4: sepCount=%d shapeCount=%d\n",
                    nc.sepCount, nc.shapeCount);
    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 5: Total triangle count for a scene (geometry measurement)
// ---------------------------------------------------------------------------

static bool test5_geometryMeasurement()
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    // Add 1 cube (12 tris), 1 sphere (many), 1 cone (many)
    root->addChild(new SoCube);
    root->addChild(new SoSphere);
    root->addChild(new SoCone);

    TriData data = {0};
    SoCallbackAction cba;
    cba.addTriangleCallback(SoShape::getClassTypeId(), triCB, &data);
    cba.apply(root);

    // Just verify we get a substantial number of triangles
    bool ok = (data.count >= 12); // at minimum the cube contributes 12
    if (!ok) fprintf(stderr, "  FAIL test5: total triangles=%d\n", data.count);
    else     printf("  PASS test5: total scene triangles=%d\n", data.count);
    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 6: Modify scene and recount triangles
// ---------------------------------------------------------------------------

static bool test6_recountAfterModification()
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    root->addChild(new SoCube); // 12 triangles

    auto countTris = [&]() -> int {
        TriData d = {0};
        SoCallbackAction cba;
        cba.addTriangleCallback(SoShape::getClassTypeId(), triCB, &d);
        cba.apply(root);
        return d.count;
    };

    int count1 = countTris();

    // Add more shapes
    root->addChild(new SoSphere);
    root->addChild(new SoCone);
    int count2 = countTris();

    // Remove sphere
    root->removeChild(1);
    int count3 = countTris();

    bool ok = (count1 == 12) && (count2 > count1) && (count3 < count2);
    if (!ok) fprintf(stderr, "  FAIL test6: count1=%d count2=%d count3=%d\n",
                     count1, count2, count3);
    else     printf("  PASS test6: tris: initial=%d added=%d removed=%d\n",
                    count1, count2, count3);
    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char **argv)
{
    initCoinHeadless();

    const char *basepath = (argc > 1) ? argv[1] : "render_callback_action";
    (void)basepath;
    int failures = 0;

    printf("\n=== SoCallbackAction traversal tests ===\n");

    if (!test1_preShapeCallback())        ++failures;
    if (!test2_triangleCallback())        ++failures;
    if (!test3_sphereNormals())           ++failures;
    if (!test4_nodeCallbackCounting())    ++failures;
    if (!test5_geometryMeasurement())     ++failures;
    if (!test6_recountAfterModification()) ++failures;

    printf("\n=== Summary: %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
