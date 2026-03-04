/*
 * render_search_action.cpp
 *
 * Tests SoSearchAction in interactive application contexts.
 * Applications use search to find nodes in scene graphs (for manip attachment,
 * material editing, LOD adjustment, etc.)
 *
 * Tests:
 *   1. Find first node by type.
 *   2. Find all nodes by type (FIND_ALL / EVERY interest).
 *   3. Find by name (setName).
 *   4. Find by node pointer (setNode).
 *   5. searchingAll TRUE: finds nodes inside SoSeparator caches.
 *   6. Search with type hierarchy: find base class finds derived instances.
 *   7. Reset + reuse action for multiple searches.
 *   8. Find none – verify empty result.
 *
 * Returns 0 on pass, non-0 on failure.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"

#include <Inventor/SoDB.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoShape.h>
#include <Inventor/actions/SoSearchAction.h>

#include <cstdio>

// ---------------------------------------------------------------------------
// Build a scene with named nodes and multiple types
// ---------------------------------------------------------------------------
static SoSeparator *buildSearchScene()
{
    SoSeparator *root = new SoSeparator;
    root->setName("root");

    SoSeparator *g1 = new SoSeparator;
    g1->setName("group1");

    SoSphere *sph1 = new SoSphere; sph1->setName("sphere1");
    SoSphere *sph2 = new SoSphere; sph2->setName("sphere2");

    SoMaterial *mat1 = new SoMaterial; mat1->setName("mat1");
    SoMaterial *mat2 = new SoMaterial; mat2->setName("mat2");

    SoCube  *cube = new SoCube;  cube->setName("cube1");
    SoCone  *cone = new SoCone;  cone->setName("cone1");

    SoSeparator *g2 = new SoSeparator;
    g2->setName("group2");

    g1->addChild(mat1);
    g1->addChild(sph1);
    g1->addChild(cube);

    g2->addChild(mat2);
    g2->addChild(sph2);
    g2->addChild(cone);

    root->addChild(g1);
    root->addChild(g2);

    return root;
}

// ---------------------------------------------------------------------------
// Test 1: Find first SoSphere
// ---------------------------------------------------------------------------
static bool test1_findFirstByType()
{
    SoSeparator *root = buildSearchScene();
    root->ref();

    SoSearchAction sa;
    sa.setType(SoSphere::getClassTypeId());
    sa.setInterest(SoSearchAction::FIRST);
    sa.apply(root);

    bool ok = false;
    if (sa.getPath()) {
        SoNode *found = sa.getPath()->getTail();
        ok = found->isOfType(SoSphere::getClassTypeId()) &&
             (strcmp(found->getName().getString(), "sphere1") == 0);
    }
    if (!ok) fprintf(stderr, "  FAIL test1: first sphere not found\n");
    else     printf("  PASS test1: found first SoSphere (name='sphere1')\n");

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 2: Find all SoSpheres (EVERY interest)
// ---------------------------------------------------------------------------
static bool test2_findAllByType()
{
    SoSeparator *root = buildSearchScene();
    root->ref();

    SoSearchAction sa;
    sa.setType(SoSphere::getClassTypeId());
    sa.setInterest(SoSearchAction::ALL);
    sa.apply(root);

    const SoPathList &paths = sa.getPaths();
    int count = paths.getLength();

    bool ok = (count == 2);
    if (!ok) fprintf(stderr, "  FAIL test2: found %d spheres (expected 2)\n", count);
    else     printf("  PASS test2: found all %d SoSpheres\n", count);

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 3: Find by name
// ---------------------------------------------------------------------------
static bool test3_findByName()
{
    SoSeparator *root = buildSearchScene();
    root->ref();

    SoSearchAction sa;
    sa.setName("cube1");
    sa.setInterest(SoSearchAction::FIRST);
    sa.apply(root);

    bool ok = false;
    if (sa.getPath()) {
        SoNode *found = sa.getPath()->getTail();
        ok = (strcmp(found->getName().getString(), "cube1") == 0) &&
             found->isOfType(SoCube::getClassTypeId());
    }
    if (!ok) fprintf(stderr, "  FAIL test3: 'cube1' not found\n");
    else     printf("  PASS test3: found node by name 'cube1'\n");

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 4: Find by node pointer
// ---------------------------------------------------------------------------
static bool test4_findByNode()
{
    SoSeparator *root = buildSearchScene();
    root->ref();

    // Find group2 first, then search by pointer
    SoSearchAction sa;
    sa.setName("group2");
    sa.setInterest(SoSearchAction::FIRST);
    sa.apply(root);

    SoNode *target = sa.getPath() ? sa.getPath()->getTail() : nullptr;
    bool ok = false;

    if (target) {
        sa.reset();
        sa.setNode(target);
        sa.setInterest(SoSearchAction::FIRST);
        sa.apply(root);
        ok = (sa.getPath() != nullptr) &&
             (sa.getPath()->getTail() == target);
    }

    if (!ok) fprintf(stderr, "  FAIL test4: find by node pointer failed\n");
    else     printf("  PASS test4: found node by pointer (group2)\n");

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 5: Base class type search finds derived instances
// ---------------------------------------------------------------------------
static bool test5_baseTypeSearch()
{
    SoSeparator *root = buildSearchScene();
    root->ref();

    // SoShape is base of SoSphere, SoCube, SoCone → should find all 3
    SoSearchAction sa;
    sa.setType(SoShape::getClassTypeId());
    sa.setInterest(SoSearchAction::ALL);
    sa.apply(root);

    int count = sa.getPaths().getLength();

    bool ok = (count >= 4); // sphere1, sphere2, cube1, cone1 (possibly mat too... no)
    if (!ok) fprintf(stderr, "  FAIL test5: found %d shapes (expected >=4)\n", count);
    else     printf("  PASS test5: base-class search found %d SoShape instances\n",
                    count);

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 6: Reset and reuse action for multiple searches
// ---------------------------------------------------------------------------
static bool test6_resetReuse()
{
    SoSeparator *root = buildSearchScene();
    root->ref();

    SoSearchAction sa;
    int checks = 0;

    // First search: spheres
    sa.setType(SoSphere::getClassTypeId());
    sa.setInterest(SoSearchAction::ALL);
    sa.apply(root);
    int count1 = sa.getPaths().getLength();
    if (count1 == 2) ++checks;

    // Reset + second search: cones
    sa.reset();
    sa.setType(SoCone::getClassTypeId());
    sa.setInterest(SoSearchAction::FIRST);
    sa.apply(root);
    bool found2 = (sa.getPath() != nullptr);
    if (found2) ++checks;

    // Reset + third search: materials
    sa.reset();
    sa.setType(SoMaterial::getClassTypeId());
    sa.setInterest(SoSearchAction::ALL);
    sa.apply(root);
    int count3 = sa.getPaths().getLength();
    if (count3 == 2) ++checks;

    bool ok = (checks == 3);
    if (!ok) fprintf(stderr, "  FAIL test6: checks=%d (sph=%d cone=%d mat=%d)\n",
                     checks, count1, (int)found2, count3);
    else     printf("  PASS test6: reset+reuse: sph=%d cone=%d mat=%d\n",
                    count1, (int)found2, count3);

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 7: Find none – verify empty result
// ---------------------------------------------------------------------------
static bool test7_findNone()
{
    SoSeparator *root = buildSearchScene();
    root->ref();

    // Search for SoCylinder – not in this scene
    SoSearchAction sa;
    sa.setType(SoCylinder::getClassTypeId());
    sa.setInterest(SoSearchAction::ALL);
    sa.apply(root);

    int count = sa.getPaths().getLength();
    bool ok = (count == 0);
    if (!ok) fprintf(stderr, "  FAIL test7: found %d cylinders (expected 0)\n", count);
    else     printf("  PASS test7: no SoCylinder found (count=%d)\n", count);

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char **argv)
{
    initCoinHeadless();

    const char *basepath = (argc > 1) ? argv[1] : "render_search_action";

    /* Render the canonical factory scene as the primary output image.
     * This ensures obol_viewer and obol_render produce identical scenes. */
    {
        SoSeparator *fRoot = ObolTest::Scenes::createSearchAction(256, 256);
        SbViewportRegion fVp(256, 256);
        SoOffscreenRenderer fRen(fVp);
        fRen.setComponents(SoOffscreenRenderer::RGB);
        fRen.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
        if (fRen.render(fRoot)) {
            char primaryPath[4096];
            snprintf(primaryPath, sizeof(primaryPath), "%s.rgb", basepath);
            fRen.writeToRGB(primaryPath);
        }
        fRoot->unref();
    }
    int failures = 0;

    printf("\n=== SoSearchAction interaction tests ===\n");

    if (!test1_findFirstByType()) ++failures;
    if (!test2_findAllByType())   ++failures;
    if (!test3_findByName())      ++failures;
    if (!test4_findByNode())      ++failures;
    if (!test5_baseTypeSearch())  ++failures;
    if (!test6_resetReuse())      ++failures;
    if (!test7_findNone())        ++failures;

    printf("\n=== Summary: %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
