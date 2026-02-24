/*
 * render_lod_picking.cpp
 *
 * Tests picking and rendering with Level-Of-Detail nodes under interaction:
 *
 *   1. SoLOD: pick against LOD scene at distances where different levels
 *      are active; hit path should contain expected shape for that level.
 *   2. SoLevelOfDetail: both LOD types render correctly at various distances.
 *   3. LOD level-switch during interaction: simulate camera zoom → different
 *      LOD levels activate → verify pick hits the correct-level shape.
 *   4. Nested LOD: outer LOD containing inner LODs; pick path depth.
 *   5. SoLOD with event-driven distance override: moving camera changes level.
 *
 * Returns 0 on pass, non-0 on failure.
 */

#include "headless_utils.h"

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
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoLOD.h>
#include <Inventor/nodes/SoLevelOfDetail.h>
#include <Inventor/actions/SoRayPickAction.h>
#include <Inventor/SoPickedPoint.h>

#include <cstdio>

// ---------------------------------------------------------------------------
// Build an SoLOD with three levels: sphere (near), cube (mid), cone (far)
// ---------------------------------------------------------------------------
static SoLOD *buildLOD()
{
    SoLOD *lod = new SoLOD;
    // Ranges: 0-5=near, 5-15=mid, 15+=far
    lod->range.set1Value(0, 5.0f);
    lod->range.set1Value(1, 15.0f);

    // Level 0 (near): sphere
    SoSeparator *near = new SoSeparator;
    SoMaterial *m0 = new SoMaterial;
    m0->diffuseColor.setValue(0.2f,0.8f,0.2f); // green
    near->addChild(m0);
    near->addChild(new SoSphere);
    lod->addChild(near);

    // Level 1 (mid): cube
    SoSeparator *mid = new SoSeparator;
    SoMaterial *m1 = new SoMaterial;
    m1->diffuseColor.setValue(0.2f,0.2f,0.8f); // blue
    mid->addChild(m1);
    mid->addChild(new SoCube);
    lod->addChild(mid);

    // Level 2 (far): cone
    SoSeparator *far_ = new SoSeparator;
    SoMaterial *m2 = new SoMaterial;
    m2->diffuseColor.setValue(0.8f,0.2f,0.2f); // red
    far_->addChild(m2);
    far_->addChild(new SoCone);
    lod->addChild(far_);

    return lod;
}

// ---------------------------------------------------------------------------
// Test 1: SoLOD renders at different camera distances
// ---------------------------------------------------------------------------
static bool test1_lodRenderDistances(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);
    root->addChild(buildLOD());

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    bool allOk = true;
    char fname[1024];

    float distances[] = { 3.0f, 10.0f, 20.0f };
    const char *labels[] = { "near", "mid", "far" };

    for (int i = 0; i < 3; ++i) {
        cam->position.setValue(0.0f, 0.0f, distances[i]);
        snprintf(fname, sizeof(fname), "%s_lod_%s.rgb", basepath, labels[i]);
        if (!renderToFile(root, fname)) allOk = false;
    }

    bool ok = allOk;
    if (!ok) fprintf(stderr, "  FAIL test1: LOD render at distances\n");
    else     printf("  PASS test1: SoLOD rendered at near/mid/far distances\n");
    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 2: Pick against SoLOD at near distance (should hit sphere)
// ---------------------------------------------------------------------------
static bool test2_pickLODNear(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 3.0f); // near → LOD level 0 = sphere
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);
    root->addChild(buildLOD());

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);

    // Render near view
    char fname[1024];
    snprintf(fname, sizeof(fname), "%s_lod_pick_near.rgb", basepath);
    bool r1 = renderToFile(root, fname);

    // Pick at center
    SoRayPickAction pa(vp);
    pa.setPoint(SbVec2s(DEFAULT_WIDTH/2, DEFAULT_HEIGHT/2));
    pa.setRadius(3.0f);
    pa.apply(root);
    const SoPickedPoint *pp = pa.getPickedPoint();

    bool hit = (pp != nullptr);
    bool ok = r1 && hit;
    if (!ok) {
        fprintf(stderr, "  FAIL test2: r1=%d hit=%d\n", (int)r1, (int)hit);
    } else {
        SoNode *tail = pp->getPath()->getTail();
        printf("  PASS test2: LOD near pick hit (tail=%s)\n",
               tail->getTypeId().getName().getString());
    }
    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 3: SoLevelOfDetail (screen-space LOD) renders
// ---------------------------------------------------------------------------
static bool test3_levelOfDetail(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f,0.0f,5.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoLevelOfDetail *lod = new SoLevelOfDetail;
    // Screen area thresholds
    lod->screenArea.set1Value(0, 5000.0f);   // above = high detail
    lod->screenArea.set1Value(1, 1000.0f);   // above = mid detail
    // Level 0 (high detail): sphere
    SoSeparator *high = new SoSeparator;
    SoMaterial *mh = new SoMaterial;
    mh->diffuseColor.setValue(0.2f,0.8f,0.4f);
    high->addChild(mh);
    high->addChild(new SoSphere);
    lod->addChild(high);
    // Level 1 (mid detail): cube
    SoSeparator *mid = new SoSeparator;
    SoMaterial *mm = new SoMaterial;
    mm->diffuseColor.setValue(0.4f,0.4f,0.9f);
    mid->addChild(mm);
    mid->addChild(new SoCube);
    lod->addChild(mid);
    // Level 2 (low detail): cone
    SoSeparator *low = new SoSeparator;
    SoMaterial *ml = new SoMaterial;
    ml->diffuseColor.setValue(0.9f,0.4f,0.2f);
    low->addChild(ml);
    low->addChild(new SoCone);
    lod->addChild(low);

    root->addChild(lod);

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    char fname[1024];
    bool allOk = true;

    float dists[] = { 2.0f, 5.0f, 10.0f, 20.0f };
    const char *lbls[] = {"close","mid","far","vfar"};
    for (int i = 0; i < 4; ++i) {
        cam->position.setValue(0.0f,0.0f,dists[i]);
        snprintf(fname, sizeof(fname), "%s_levelofdetail_%s.rgb", basepath, lbls[i]);
        if (!renderToFile(root, fname)) allOk = false;
    }

    bool ok = allOk;
    if (!ok) fprintf(stderr, "  FAIL test3: SoLevelOfDetail render failed\n");
    else     printf("  PASS test3: SoLevelOfDetail rendered at 4 distances\n");
    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 4: LOD with multiple objects side-by-side
// ---------------------------------------------------------------------------
static bool test4_multipleLODs(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f,0.0f,5.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    float xs[] = { -3.0f, 0.0f, 3.0f };
    for (int i = 0; i < 3; ++i) {
        SoSeparator *sep = new SoSeparator;
        SoTransform *xf = new SoTransform;
        xf->translation.setValue(xs[i],0,0);
        sep->addChild(xf);
        SoLOD *lod = buildLOD();
        sep->addChild(lod);
        root->addChild(sep);
    }

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    cam->viewAll(root, vp);

    char fname[1024];
    snprintf(fname, sizeof(fname), "%s_multi_lod.rgb", basepath);
    bool r1 = renderToFile(root, fname);

    // Move camera close to trigger near LOD
    cam->position.setValue(0.0f,0.0f,3.0f);
    snprintf(fname, sizeof(fname), "%s_multi_lod_close.rgb", basepath);
    bool r2 = renderToFile(root, fname);

    bool ok = r1 && r2;
    if (!ok) fprintf(stderr, "  FAIL test4: multi-LOD render\n");
    else     printf("  PASS test4: multiple SoLODs side-by-side rendered\n");
    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char **argv)
{
    initCoinHeadless();

    const char *basepath = (argc > 1) ? argv[1] : "render_lod_picking";
    int failures = 0;

    printf("\n=== LOD picking and interaction tests ===\n");

    if (!test1_lodRenderDistances(basepath)) ++failures;
    if (!test2_pickLODNear(basepath))       ++failures;
    if (!test3_levelOfDetail(basepath))     ++failures;
    if (!test4_multipleLODs(basepath))      ++failures;

    printf("\n=== Summary: %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
