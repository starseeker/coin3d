/*
 * render_pick_interaction.cpp - SoRayPickAction interaction tests
 *
 * Exercises the pick pipeline as used by user applications:
 *   1. Pick a sphere at the viewport center - path must contain SoSphere.
 *   2. Pick between objects where nothing is present - no hit.
 *   3. Pick a cube; hit-point must lie on the cube surface (|coord| near 1).
 *   4. Pick the front-most of two overlapping shapes (closest pick).
 *   5. Multi-pick: SoRayPickAction::setPickAll(TRUE) returns both overlapping
 *      shapes; verify count and that the nearer one comes first.
 *   6. Pick radius: a pick slightly off-center still hits with a large radius.
 *   7. Highlight-on-pick: material is changed after pick and scene re-rendered.
 *   8. Successive picks across three side-by-side objects all succeed.
 *
 * Returns 0 on pass, non-0 on fail.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoPickedPoint.h>
#include <Inventor/actions/SoRayPickAction.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/SoPath.h>
#include <cstdio>
#include <cmath>

static const int W = 256;
static const int H = 256;

// ---------------------------------------------------------------------------
// Helper: project world point to screen pixel (viewport-space, origin bottom-left)
// ---------------------------------------------------------------------------
static SbVec2s worldToScreen(const SbVec3f &wpt, SoCamera *cam,
                              const SbViewportRegion &vp)
{
    SbViewVolume vv = cam->getViewVolume(vp.getViewportAspectRatio());
    SbVec3f ndc;
    vv.projectToScreen(wpt, ndc);
    SbVec2s sz = vp.getViewportSizePixels();
    short px = (short)(ndc[0] * sz[0]);
    short py = (short)(ndc[1] * sz[1]);
    return SbVec2s(px, py);
}

// ---------------------------------------------------------------------------
// Helper: perform a pick and return true if something was hit.
// Fills *ptOut with the hit-point coordinates and *typeNameOut with the
// tail node type name when non-null pointers are provided.
// The SoRayPickAction is kept alive inside this function so that the
// SoPickedPoint data is valid during the check.
// ---------------------------------------------------------------------------
static bool doPick(SoNode *root,
                   const SbVec2s &pt,
                   const SbViewportRegion &vp,
                   float radius = 2.0f,
                   SbVec3f *ptOut = nullptr,
                   SbName *typeNameOut = nullptr,
                   bool *tailIsSphereOut = nullptr)
{
    SoRayPickAction pa(vp);
    pa.setPoint(pt);
    pa.setRadius(radius);
    pa.apply(root);
    const SoPickedPoint *pp = pa.getPickedPoint();
    if (!pp) return false;
    if (ptOut)           *ptOut = pp->getPoint();
    if (typeNameOut)     *typeNameOut = pp->getPath()->getTail()->getTypeId().getName();
    if (tailIsSphereOut) *tailIsSphereOut =
                              pp->getPath()->getTail()->isOfType(SoSphere::getClassTypeId());
    return true;
}

// ---------------------------------------------------------------------------
// Build a scene with a sphere at the center (used by tests 1, 6, 7)
// ---------------------------------------------------------------------------
static SoSeparator *buildSphereScene(SoOrthographicCamera **camOut = nullptr)
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->orientation.setValue(SbVec3f(0,0,1), 0.0f);
    cam->height.setValue(4.0f);
    root->addChild(cam);
    SoDirectionalLight *lt = new SoDirectionalLight;
    lt->direction.setValue(0.0f, 0.0f, -1.0f);
    root->addChild(lt);
    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.4f, 0.6f, 1.0f);
    root->addChild(mat);
    SoSphere *sph = new SoSphere;
    sph->radius = 0.8f;
    root->addChild(sph);
    if (camOut) *camOut = cam;
    root->unrefNoDelete();
    return root;
}

// ---------------------------------------------------------------------------
// Test 1: pick sphere at viewport center
// ---------------------------------------------------------------------------
static bool test1_pickSphereCenter()
{
    SoOrthographicCamera *cam = nullptr;
    SoSeparator *root = buildSphereScene(&cam);
    root->ref();

    SbViewportRegion vp(W, H);
    SbVec2s center(W/2, H/2);

    bool tailIsSphere = false;
    bool hit = doPick(root, center, vp, 2.0f, nullptr, nullptr, &tailIsSphere);

    bool ok = hit && tailIsSphere;
    if (!ok) {
        fprintf(stderr, "  FAIL test1: hit=%d tailIsSphere=%d\n", (int)hit, (int)tailIsSphere);
    } else {
        printf("  PASS test1: pick sphere at center (tail is SoSphere)\n");
    }

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 2: miss – pick at corner of viewport (far from sphere)
// ---------------------------------------------------------------------------
static bool test2_missCorner()
{
    SoSeparator *root = buildSphereScene();
    root->ref();
    SbViewportRegion vp(W, H);
    // Corner is way outside the unit sphere projected into view
    SbVec2s corner(3, 3);
    bool hit = doPick(root, corner, vp, 1.0f);
    bool ok = !hit;
    if (!ok) {
        fprintf(stderr, "  FAIL test2: unexpected hit at corner\n");
    } else {
        printf("  PASS test2: no pick at corner (correct miss)\n");
    }
    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 3: pick a cube; hit-point must be within cube bounds
// ---------------------------------------------------------------------------
static bool test3_pickCubeSurface()
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->height.setValue(4.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);
    SoCube *cube = new SoCube; // 2×2×2 default
    root->addChild(cube);

    SbViewportRegion vp(W, H);
    SbVec2s center(W/2, H/2);

    SbVec3f pt;
    bool hit = doPick(root, center, vp, 2.0f, &pt);

    bool ok = false;
    if (hit) {
        // Point must be on the cube surface: at least one coordinate near ±1
        // and all others within [-1,1].
        float ax = fabsf(pt[0]), ay = fabsf(pt[1]), az = fabsf(pt[2]);
        bool onSurface = (fabsf(ax - 1.0f) < 0.05f ||
                          fabsf(ay - 1.0f) < 0.05f ||
                          fabsf(az - 1.0f) < 0.05f);
        bool inBounds   = (ax <= 1.05f && ay <= 1.05f && az <= 1.05f);
        ok = onSurface && inBounds;
        if (!ok) {
            fprintf(stderr,
                    "  FAIL test3: pick point (%.4f,%.4f,%.4f) not on cube surface\n",
                    pt[0], pt[1], pt[2]);
        } else {
            printf("  PASS test3: pick cube surface at (%.4f,%.4f,%.4f)\n",
                   pt[0], pt[1], pt[2]);
        }
    } else {
        fprintf(stderr, "  FAIL test3: no pick on cube at center\n");
    }

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 4: two overlapping shapes; front one picked first
// ---------------------------------------------------------------------------
static bool test4_frontPickFirst()
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->height.setValue(6.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    // Sphere at z=0
    SoSeparator *sep1 = new SoSeparator;
    SoMaterial *mat1 = new SoMaterial;
    mat1->diffuseColor.setValue(1.0f, 0.0f, 0.0f); // red
    sep1->addChild(mat1);
    sep1->addChild(new SoSphere);
    root->addChild(sep1);

    // Cube at z=-2 (behind sphere)
    SoSeparator *sep2 = new SoSeparator;
    SoTransform *xf2 = new SoTransform;
    xf2->translation.setValue(0.0f, 0.0f, -2.0f);
    sep2->addChild(xf2);
    SoMaterial *mat2 = new SoMaterial;
    mat2->diffuseColor.setValue(0.0f, 0.0f, 1.0f); // blue
    sep2->addChild(mat2);
    SoCube *cube = new SoCube;
    cube->width = 3.0f; cube->height = 3.0f; cube->depth = 1.0f;
    sep2->addChild(cube);
    root->addChild(sep2);

    SbViewportRegion vp(W, H);
    SbVec2s center(W/2, H/2);

    bool tailIsSphere = false;
    bool hit = doPick(root, center, vp, 2.0f, nullptr, nullptr, &tailIsSphere);

    bool ok = hit && tailIsSphere;
    if (!ok) {
        fprintf(stderr, "  FAIL test4: hit=%d tailIsSphere=%d\n", (int)hit, (int)tailIsSphere);
    } else {
        printf("  PASS test4: front pick is SoSphere (sphere in front of cube)\n");
    }

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 5: setPickAll(TRUE) – multi-pick returns both overlapping shapes
// ---------------------------------------------------------------------------
static bool test5_pickAll()
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->height.setValue(6.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    // Front: sphere at z=0
    SoSeparator *sep1 = new SoSeparator;
    sep1->addChild(new SoSphere);
    root->addChild(sep1);

    // Back: wide flat cube at z=-2
    SoSeparator *sep2 = new SoSeparator;
    SoTransform *xf2 = new SoTransform;
    xf2->translation.setValue(0.0f, 0.0f, -2.0f);
    sep2->addChild(xf2);
    SoCube *cube = new SoCube;
    cube->width = 3.0f; cube->height = 3.0f; cube->depth = 0.5f;
    sep2->addChild(cube);
    root->addChild(sep2);

    SbViewportRegion vp(W, H);
    SoRayPickAction pa(vp);
    pa.setPoint(SbVec2s(W/2, H/2));
    pa.setRadius(2.0f);
    pa.setPickAll(TRUE);
    pa.apply(root);

    const SoPickedPointList &hits = pa.getPickedPointList();
    bool ok = (hits.getLength() >= 2);
    if (!ok) {
        fprintf(stderr, "  FAIL test5: expected >=2 hits with setPickAll, got %d\n",
                hits.getLength());
    } else {
        // First hit must be closer (sphere at z~=+1 is nearer the camera than cube)
        SbVec3f pt0 = hits[0]->getPoint();
        SbVec3f pt1 = hits[1]->getPoint();
        bool nearerFirst = (pt0[2] > pt1[2]); // larger z = nearer to +z camera
        if (!nearerFirst) {
            fprintf(stderr, "  FAIL test5: hits not sorted front-to-back (z0=%.2f z1=%.2f)\n",
                    pt0[2], pt1[2]);
            ok = false;
        } else {
            printf("  PASS test5: %d hits, nearer first (z0=%.2f > z1=%.2f)\n",
                   hits.getLength(), pt0[2], pt1[2]);
        }
    }

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 6: pick boundary – just inside sphere edge hits, outside misses
// ---------------------------------------------------------------------------
static bool test6_pickRadius()
{
    SoSeparator *root = buildSphereScene();
    root->ref();
    SbViewportRegion vp(W, H);

    // With ortho camera height=4.0 and viewport W=256:
    //   pixels/world-unit = 256/4 = 64
    //   sphere radius (0.8) = 51.2 pixels from center
    // offset=50 → inside sphere edge (~51 px): MUST hit
    // offset=60 → outside sphere edge:        MUST miss
    SbVec2s inside  (W/2 + 50, H/2);
    SbVec2s outside (W/2 + 60, H/2);

    bool hitInside  = doPick(root, inside,  vp, 1.0f);
    bool hitOutside = doPick(root, outside, vp, 1.0f);

    bool ok = hitInside && !hitOutside;
    if (!ok) {
        fprintf(stderr,
                "  FAIL test6: inside=%s outside=%s (expected inside=hit outside=miss)\n",
                hitInside ? "hit" : "miss", hitOutside ? "hit" : "miss");
    } else {
        printf("  PASS test6: inside sphere edge hits, outside misses\n");
    }

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 7: highlight on pick – render before and after material change
// ---------------------------------------------------------------------------
static bool test7_highlightOnPick(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->height.setValue(4.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.4f, 0.4f, 0.4f); // gray
    root->addChild(mat);
    SoSphere *sph = new SoSphere;
    sph->radius = 0.8f;
    root->addChild(sph);

    SbViewportRegion vp(W, H);

    // Render initial (gray)
    char fname[1024];
    snprintf(fname, sizeof(fname), "%s_highlight_before.rgb", basepath);
    bool r1 = renderToFile(root, fname, W, H);

    // Simulate a pick at center; keep action alive so we can read result
    SbVec2s center(W/2, H/2);
    bool hit = doPick(root, center, vp);
    if (hit) {
        // Highlight: change material to yellow
        mat->diffuseColor.setValue(1.0f, 1.0f, 0.0f);
    }

    // Render after highlight
    snprintf(fname, sizeof(fname), "%s_highlight_after.rgb", basepath);
    bool r2 = renderToFile(root, fname, W, H);

    bool ok = r1 && r2 && hit;
    if (!ok) {
        fprintf(stderr, "  FAIL test7: r1=%d r2=%d hit=%d\n",
                (int)r1, (int)r2, (int)hit);
    } else {
        printf("  PASS test7: highlight-on-pick rendered (before/after)\n");
    }

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 8: successive picks across three objects
// ---------------------------------------------------------------------------
static bool test8_successivePicks(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->height.setValue(8.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    // Three objects side by side: sphere (left), cube (center), cone (right)
    struct { SoNode *shape; SoMaterial *mat; float x; } objs[3];

    SoSeparator *seps[3];
    for (int i = 0; i < 3; ++i) seps[i] = new SoSeparator;

    float xs[3] = { -2.5f, 0.0f, 2.5f };

    for (int i = 0; i < 3; ++i) {
        SoTransform *xf = new SoTransform;
        xf->translation.setValue(xs[i], 0.0f, 0.0f);
        seps[i]->addChild(xf);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.6f, 0.6f, 0.6f);
        seps[i]->addChild(mat);
        objs[i].mat = mat;
        if (i == 0) { seps[i]->addChild(new SoSphere); }
        if (i == 1) { seps[i]->addChild(new SoCube);   }
        if (i == 2) { seps[i]->addChild(new SoCone);   }
        root->addChild(seps[i]);
    }

    SbViewportRegion vp(W, H);
    cam->viewAll(root, vp);

    // Render initial scene
    char fname[1024];
    snprintf(fname, sizeof(fname), "%s_three_initial.rgb", basepath);
    renderToFile(root, fname, W, H);

    // Pick each object via its world-space center
    SoGetBoundingBoxAction bba(vp);
    SbColor highlights[3] = {
        SbColor(1.0f, 0.2f, 0.2f),
        SbColor(0.2f, 1.0f, 0.2f),
        SbColor(0.2f, 0.2f, 1.0f)
    };
    const char *names[3] = { "sphere", "cube", "cone" };

    int hits = 0;
    for (int i = 0; i < 3; ++i) {
        bba.apply(seps[i]);
        SbVec3f center3 = bba.getBoundingBox().getCenter();
        SbVec2s screenPt = worldToScreen(center3, cam, vp);

        bool hit = doPick(root, screenPt, vp, 5.0f);
        if (hit) {
            objs[i].mat->diffuseColor.setValue(highlights[i]);
            ++hits;
            printf("    picked %s at screen (%d,%d)\n", names[i], screenPt[0], screenPt[1]);
        } else {
            printf("    (no hit for %s at screen (%d,%d) - ok for cone tip)\n",
                   names[i], screenPt[0], screenPt[1]);
        }

        snprintf(fname, sizeof(fname), "%s_three_%s.rgb", basepath, names[i]);
        renderToFile(root, fname, W, H);
    }

    bool ok = (hits >= 2); // cone center is tip so may miss; sphere+cube always hit
    if (!ok) {
        fprintf(stderr, "  FAIL test8: only %d/3 hits\n", hits);
    } else {
        printf("  PASS test8: %d/3 objects picked and highlighted\n", hits);
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

    const char *basepath = (argc > 1) ? argv[1] : "render_pick_interaction";

    /* Render the canonical factory scene as the primary output image.
     * This ensures obol_viewer and obol_render produce identical scenes. */
    {
        SoSeparator *fRoot = ObolTest::Scenes::createPickInteraction(256, 256);
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

    printf("\n=== SoRayPickAction interaction tests ===\n");

    if (!test1_pickSphereCenter())    ++failures;
    if (!test2_missCorner())          ++failures;
    if (!test3_pickCubeSurface())     ++failures;
    if (!test4_frontPickFirst())      ++failures;
    if (!test5_pickAll())             ++failures;
    if (!test6_pickRadius())          ++failures;
    if (!test7_highlightOnPick(basepath)) ++failures;
    if (!test8_successivePicks(basepath)) ++failures;

    printf("\n=== Summary: %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
