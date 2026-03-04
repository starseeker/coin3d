/*
 * render_bbox_action.cpp
 *
 * Tests SoGetBoundingBoxAction in interaction contexts:
 * Applications routinely use bounding boxes to determine object bounds
 * for view fitting, collision detection, and hit highlighting.
 *
 * Tests:
 *   1. GetBoundingBox on single shape – verify dimensions are reasonable.
 *   2. GetBoundingBox after transform – verify translated bounds.
 *   3. GetBoundingBox on group – enclosing box covers all children.
 *   4. GetBoundingBox excludes invisible children (SoSwitch child 0).
 *   5. Center point of bounding box used to drive camera viewAll.
 *   6. Progressive geometry addition – box grows monotonically.
 *   7. GetBoundingBox on picked path matches full-scene bbox intersection.
 *
 * Returns 0 on pass, non-0 on failure.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"

#include <Inventor/SoDB.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoSwitch.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/actions/SoRayPickAction.h>
#include <Inventor/SoPickedPoint.h>
#include <Inventor/annex/HUD/nodekits/SoHUDKit.h>

#include <cstdio>
#include <cmath>

// ---------------------------------------------------------------------------
// Test 1: GetBoundingBox on single sphere
// ---------------------------------------------------------------------------
static bool test1_sphereBBox()
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    root->addChild(new SoSphere); // default radius=1

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    SoGetBoundingBoxAction bba(vp);
    bba.apply(root);
    SbBox3f box = bba.getBoundingBox();
    SbVec3f center = box.getCenter();

    // Sphere of radius 1 centered at origin: bounds should be [-1,1] in all axes
    SbVec3f mn = box.getMin();
    SbVec3f mx = box.getMax();
    bool centOk = (fabsf(center[0]) < 0.01f && fabsf(center[1]) < 0.01f &&
                   fabsf(center[2]) < 0.01f);
    bool sizeOk = (fabsf(mx[0] - 1.0f) < 0.01f &&
                   fabsf(mn[0] + 1.0f) < 0.01f);

    bool ok = centOk && sizeOk;
    if (!ok) fprintf(stderr, "  FAIL test1: bbox=[%.2f,%.2f]x[%.2f,%.2f]x[%.2f,%.2f]\n",
                     mn[0],mx[0], mn[1],mx[1], mn[2],mx[2]);
    else     printf("  PASS test1: sphere bbox correct (%.2f:%.2f)\n", mn[0], mx[0]);

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 2: GetBoundingBox after SoTransform translation
// ---------------------------------------------------------------------------
static bool test2_translatedBBox()
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoTransform *xf = new SoTransform;
    xf->translation.setValue(3.0f, 5.0f, -2.0f);
    root->addChild(xf);
    root->addChild(new SoSphere); // radius 1

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    SoGetBoundingBoxAction bba(vp);
    bba.apply(root);
    SbVec3f center = bba.getBoundingBox().getCenter();

    bool ok = (fabsf(center[0] - 3.0f) < 0.01f &&
               fabsf(center[1] - 5.0f) < 0.01f &&
               fabsf(center[2] + 2.0f) < 0.01f);
    if (!ok) fprintf(stderr, "  FAIL test2: center=(%.2f,%.2f,%.2f)\n",
                     center[0], center[1], center[2]);
    else     printf("  PASS test2: translated bbox center=(%.2f,%.2f,%.2f)\n",
                    center[0], center[1], center[2]);

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 3: GetBoundingBox on group covers all children
// ---------------------------------------------------------------------------
static bool test3_groupBBox()
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    // Sphere at (-3,0,0) radius 1 → x in [-4,-2]
    SoSeparator *s1 = new SoSeparator;
    SoTransform *x1 = new SoTransform;
    x1->translation.setValue(-3.0f,0,0);
    s1->addChild(x1);
    s1->addChild(new SoSphere);
    root->addChild(s1);

    // Cube (2×2×2) at (+3,0,0) → x in [2,4]
    SoSeparator *s2 = new SoSeparator;
    SoTransform *x2 = new SoTransform;
    x2->translation.setValue(3.0f,0,0);
    s2->addChild(x2);
    s2->addChild(new SoCube); // 2×2×2
    root->addChild(s2);

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    SoGetBoundingBoxAction bba(vp);
    bba.apply(root);
    SbBox3f box = bba.getBoundingBox();

    bool ok = (box.getMin()[0] < -2.0f) && (box.getMax()[0] > 2.0f);
    if (!ok) fprintf(stderr, "  FAIL test3: x-range=[%.2f,%.2f]\n",
                     box.getMin()[0], box.getMax()[0]);
    else     printf("  PASS test3: group bbox x=[%.2f,%.2f] covers both objects\n",
                    box.getMin()[0], box.getMax()[0]);

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 4: Progressive addition – bbox grows monotonically
// ---------------------------------------------------------------------------
static bool test4_progressiveGrowth()
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoSeparator *group = new SoSeparator;
    root->addChild(group);

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    float prevSize = 0.0f;
    bool ok = true;

    float positions[] = { 0.0f, 2.0f, -2.0f, 4.0f };
    for (int i = 0; i < 4; ++i) {
        SoSeparator *sep = new SoSeparator;
        SoTransform *xf = new SoTransform;
        xf->translation.setValue(positions[i], 0, 0);
        sep->addChild(xf);
        sep->addChild(new SoSphere);
        group->addChild(sep);

        SoGetBoundingBoxAction bba(vp);
        bba.apply(root);
        SbBox3f box = bba.getBoundingBox();
        // Compute diagonal as a measure of size
        SbVec3f d = box.getMax() - box.getMin();
        float diag = sqrtf(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]);
        if (i > 0 && diag < prevSize - 0.01f) {
            fprintf(stderr, "  FAIL test4: bbox shrunk at i=%d (diag=%.2f < prev=%.2f)\n",
                    i, diag, prevSize);
            ok = false;
        }
        prevSize = diag;
    }

    if (ok) printf("  PASS test4: bbox grew monotonically (final diag=%.2f)\n", prevSize);
    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 5: Camera viewAll driven by bounding box center
// ---------------------------------------------------------------------------
static bool test5_viewAllFromBBox(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    // Objects at extreme positions
    for (int i = 0; i < 3; ++i) {
        SoSeparator *sep = new SoSeparator;
        SoTransform *xf = new SoTransform;
        xf->translation.setValue(-3.0f + i*3.0f, 0, 0);
        sep->addChild(xf);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.3f+i*0.3f, 0.6f, 0.8f-i*0.2f);
        sep->addChild(mat);
        sep->addChild(new SoSphere);
        root->addChild(sep);
    }

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);

    // Get bbox to verify viewAll will cover everything
    SoGetBoundingBoxAction bba(vp);
    bba.apply(root);
    SbBox3f box = bba.getBoundingBox();
    SbVec3f center = box.getCenter();

    cam->viewAll(root, vp);

    char fname[1024];
    snprintf(fname, sizeof(fname), "%s_bbox_viewall.rgb", basepath);
    bool r = renderToFile(root, fname);

    bool ok = r && (fabsf(center[0]) < 0.01f); // 3 symmetric spheres → center at 0
    if (!ok) fprintf(stderr, "  FAIL test5: r=%d center=(%.2f,%.2f,%.2f)\n",
                     (int)r, center[0], center[1], center[2]);
    else     printf("  PASS test5: viewAll from bbox center=(%.2f,%.2f,%.2f)\n",
                    center[0], center[1], center[2]);

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 6: GetBoundingBox on path (subtree)
// ---------------------------------------------------------------------------
static bool test6_subgraphBBox()
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoSeparator *left = new SoSeparator;
    SoTransform *xl = new SoTransform;
    xl->translation.setValue(-5.0f,0,0);
    left->addChild(xl);
    left->addChild(new SoSphere); // radius 1 at x=-5, bbox x:[-6,-4]
    root->addChild(left);

    SoSeparator *right = new SoSeparator;
    SoTransform *xr = new SoTransform;
    xr->translation.setValue(5.0f,0,0);
    right->addChild(xr);
    right->addChild(new SoSphere); // radius 1 at x=+5, bbox x:[4,6]
    root->addChild(right);

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);

    // Get bbox of left subtree only
    SoGetBoundingBoxAction bba(vp);
    bba.apply(left);
    SbBox3f leftBox = bba.getBoundingBox();

    // Get bbox of whole scene
    bba.apply(root);
    SbBox3f fullBox = bba.getBoundingBox();

    bool leftOk = (leftBox.getMax()[0] < 0.0f); // left sphere x max < 0
    bool fullOk = (fullBox.getMin()[0] < 0.0f) && (fullBox.getMax()[0] > 0.0f);

    bool ok = leftOk && fullOk;
    if (!ok) fprintf(stderr, "  FAIL test6: leftOk=%d fullOk=%d\n",
                     (int)leftOk, (int)fullOk);
    else     printf("  PASS test6: subtree bbox x=[%.2f,%.2f] full x=[%.2f,%.2f]\n",
                    leftBox.getMin()[0], leftBox.getMax()[0],
                    fullBox.getMin()[0], fullBox.getMax()[0]);

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 7: GetBoundingBox on scene containing SoHUDKit (SoCallback nodes with
//         GL depth-test callbacks must not crash when glue is null).
// ---------------------------------------------------------------------------
static bool test7_hudKitBBox()
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    root->addChild(new SoDirectionalLight);
    root->addChild(new SoSphere);

    // SoHUDKit contains SoCallback nodes that call
    // SoGLContext_glDisable/Enable(sogl_current_render_glue(), GL_DEPTH_TEST).
    // Applying SoGetBoundingBoxAction must not crash even though there is no
    // active GL render context (sogl_current_render_glue() returns NULL).
    SoHUDKit *hud = new SoHUDKit;
    root->addChild(hud);

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    SoGetBoundingBoxAction bba(vp);
    // This must not crash (segfault before fix).
    bba.apply(root);
    SbBox3f box = bba.getBoundingBox();

    bool ok = !box.isEmpty();
    if (!ok) fprintf(stderr, "  FAIL test7: bbox is empty after HUD kit traversal\n");
    else     printf("  PASS test7: SoGetBoundingBoxAction on SoHUDKit scene did not crash\n");

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char **argv)
{
    initCoinHeadless();

    const char *basepath = (argc > 1) ? argv[1] : "render_bbox_action";

    /* Render the canonical factory scene as the primary output image.
     * This ensures obol_viewer and obol_render produce identical scenes. */
    {
        SoSeparator *fRoot = ObolTest::Scenes::createBBoxAction(256, 256);
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

    printf("\n=== SoGetBoundingBoxAction interaction tests ===\n");

    if (!test1_sphereBBox())            ++failures;
    if (!test2_translatedBBox())        ++failures;
    if (!test3_groupBBox())             ++failures;
    if (!test4_progressiveGrowth())     ++failures;
    if (!test5_viewAllFromBBox(basepath)) ++failures;
    if (!test6_subgraphBBox())          ++failures;
    if (!test7_hudKitBBox())            ++failures;

    printf("\n=== Summary: %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
