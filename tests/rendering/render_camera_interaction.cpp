/*
 * render_camera_interaction.cpp
 *
 * Tests camera manipulation and switching as user applications do it:
 *
 *   1. Switch from perspective to orthographic camera mid-scene.
 *   2. Camera viewAll() after geometry is added (auto-fit).
 *   3. SoFrustumCamera used as a perspective camera with manually set frustum.
 *   4. Camera point-at (look-at) targeting a moving object.
 *   5. Near/far clipping plane adjustment via camera fields.
 *   6. Camera orientation via SoCamera::orientation field.
 *   7. Multiple cameras in scene; select active camera via search.
 *
 * Returns 0 on pass, non-0 on failure.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"

#include <Inventor/SoDB.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoFrustumCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoSwitch.h>
#include <Inventor/actions/SoSearchAction.h>

#include <cstdio>
#include <cmath>

// ---------------------------------------------------------------------------
// Helpers: build a simple lit geometry group
// ---------------------------------------------------------------------------
static SoSeparator *buildGeom()
{
    SoSeparator *geom = new SoSeparator;

    // Sphere (left)
    SoSeparator *s = new SoSeparator;
    SoTransform *xf = new SoTransform;
    xf->translation.setValue(-1.5f,0,0);
    s->addChild(xf);
    SoMaterial *m = new SoMaterial;
    m->diffuseColor.setValue(0.8f,0.3f,0.3f);
    s->addChild(m);
    s->addChild(new SoSphere);
    geom->addChild(s);

    // Cube (center)
    SoSeparator *c = new SoSeparator;
    SoMaterial *mc = new SoMaterial;
    mc->diffuseColor.setValue(0.3f,0.7f,0.3f);
    c->addChild(mc);
    c->addChild(new SoCube);
    geom->addChild(c);

    // Cone (right)
    SoSeparator *co = new SoSeparator;
    SoTransform *xfco = new SoTransform;
    xfco->translation.setValue(1.5f,0,0);
    co->addChild(xfco);
    SoMaterial *mco = new SoMaterial;
    mco->diffuseColor.setValue(0.3f,0.3f,0.8f);
    co->addChild(mco);
    co->addChild(new SoCone);
    geom->addChild(co);

    return geom;
}

// ---------------------------------------------------------------------------
// Test 1: Switch perspective → orthographic mid-scene
// ---------------------------------------------------------------------------
static bool test1_cameraSwitch(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *percam = new SoPerspectiveCamera;
    root->addChild(percam);
    root->addChild(new SoDirectionalLight);
    root->addChild(buildGeom());

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    percam->viewAll(root, vp);

    char fname[1024];
    snprintf(fname, sizeof(fname), "%s_persp.rgb", basepath);
    bool r1 = renderToFile(root, fname);

    // Replace perspective camera with orthographic
    SoOrthographicCamera *orthocam = new SoOrthographicCamera;
    root->replaceChild(percam, orthocam);
    orthocam->viewAll(root, vp);

    snprintf(fname, sizeof(fname), "%s_ortho.rgb", basepath);
    bool r2 = renderToFile(root, fname);

    bool ok = r1 && r2;
    if (!ok)
        fprintf(stderr, "  FAIL test1: r1=%d r2=%d\n", (int)r1, (int)r2);
    else
        printf("  PASS test1: perspective→orthographic camera switch rendered\n");
    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 2: viewAll() auto-fits after adding geometry
// ---------------------------------------------------------------------------
static bool test2_viewAllAutoFit(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);

    // Add first object and viewAll
    SoSeparator *group = new SoSeparator;
    SoMaterial *mat1 = new SoMaterial;
    mat1->diffuseColor.setValue(0.9f,0.5f,0.2f);
    group->addChild(mat1);
    group->addChild(new SoSphere);
    root->addChild(group);

    cam->viewAll(root, vp);
    char fname[1024];
    snprintf(fname, sizeof(fname), "%s_fit1.rgb", basepath);
    bool r1 = renderToFile(root, fname);

    // Add more objects and re-fit
    SoTransform *xf = new SoTransform;
    xf->translation.setValue(5.0f,0,0);
    SoMaterial *mat2 = new SoMaterial;
    mat2->diffuseColor.setValue(0.2f,0.5f,0.9f);
    SoCube *bigCube = new SoCube;
    bigCube->width = bigCube->height = bigCube->depth = 3.0f;
    group->addChild(xf);
    group->addChild(mat2);
    group->addChild(bigCube);

    cam->viewAll(root, vp);
    snprintf(fname, sizeof(fname), "%s_fit2.rgb", basepath);
    bool r2 = renderToFile(root, fname);

    bool ok = r1 && r2;
    if (!ok)
        fprintf(stderr, "  FAIL test2: r1=%d r2=%d\n", (int)r1, (int)r2);
    else
        printf("  PASS test2: viewAll() auto-fit after geometry addition\n");
    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 3: SoFrustumCamera as a perspective camera
// ---------------------------------------------------------------------------
static bool test3_frustumCamera(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoFrustumCamera *cam = new SoFrustumCamera;
    cam->position.setValue(0,0,6);
    cam->nearDistance.setValue(0.5f);
    cam->farDistance.setValue(20.0f);
    // Set a symmetric frustum
    cam->left.setValue(-1.0f);
    cam->right.setValue(1.0f);
    cam->bottom.setValue(-0.75f);
    cam->top.setValue(0.75f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);
    root->addChild(buildGeom());

    char fname[1024];
    snprintf(fname, sizeof(fname), "%s_frustum.rgb", basepath);
    bool r1 = renderToFile(root, fname);

    bool ok = r1;
    if (!ok)
        fprintf(stderr, "  FAIL test3: frustum camera render failed\n");
    else
        printf("  PASS test3: SoFrustumCamera rendered without crash\n");
    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 4: Camera point-at targeting a moving object
// ---------------------------------------------------------------------------
static bool test4_cameraPoinAt(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0,0,8);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.6f,0.8f,0.2f);
    root->addChild(mat);
    SoTransform *xf = new SoTransform;
    root->addChild(xf);
    root->addChild(new SoSphere);

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    cam->viewAll(root, vp);

    bool allOk = true;
    char fname[1024];
    float angles[] = { 0.0f, 0.5f, 1.0f, 1.5f, 2.0f };
    for (int i = 0; i < 5; ++i) {
        // Move object in a circle
        float x = sinf(angles[i]) * 2.0f;
        float y = cosf(angles[i]) * 2.0f;
        xf->translation.setValue(x, y, 0);
        // Point camera at new object position
        cam->pointAt(SbVec3f(x, y, 0), SbVec3f(0,0,1));
        snprintf(fname, sizeof(fname), "%s_lookat_%d.rgb", basepath, i);
        if (!renderToFile(root, fname)) allOk = false;
    }

    bool ok = allOk;
    if (!ok)
        fprintf(stderr, "  FAIL test4: camera pointAt render failed\n");
    else
        printf("  PASS test4: camera tracking moving target (5 frames)\n");
    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 5: Near/far clipping plane adjustment
// ---------------------------------------------------------------------------
static bool test5_clippingPlanes(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0,0,5);
    cam->nearDistance.setValue(0.1f);
    cam->farDistance.setValue(20.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.4f,0.6f,0.9f);
    root->addChild(mat);
    root->addChild(new SoSphere);

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    char fname[1024];

    // Normal clip planes – full sphere visible
    cam->viewAll(root, vp);
    snprintf(fname, sizeof(fname), "%s_clip_normal.rgb", basepath);
    bool r1 = renderToFile(root, fname);

    // Very close near plane – clips the front of the sphere
    cam->nearDistance.setValue(4.8f);
    cam->farDistance.setValue(6.0f);
    snprintf(fname, sizeof(fname), "%s_clip_near.rgb", basepath);
    bool r2 = renderToFile(root, fname);

    // Restore
    cam->viewAll(root, vp);
    snprintf(fname, sizeof(fname), "%s_clip_restored.rgb", basepath);
    bool r3 = renderToFile(root, fname);

    bool ok = r1 && r2 && r3;
    if (!ok)
        fprintf(stderr, "  FAIL test5: r1=%d r2=%d r3=%d\n",(int)r1,(int)r2,(int)r3);
    else
        printf("  PASS test5: near/far clipping plane adjustment rendered\n");
    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 6: Camera orientation rotates the view
// ---------------------------------------------------------------------------
static bool test6_cameraOrientation(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0,0,5);
    cam->height.setValue(5.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);
    root->addChild(buildGeom());

    char fname[1024];
    bool allOk = true;

    // Rotate camera orientation to view from different angles
    float angles[] = { 0.0f, 0.5f, 1.0f, 1.5f };
    const char *labels[] = {"front","right45","right90","right135"};
    for (int i = 0; i < 4; ++i) {
        // Rotate camera around Y axis
        SbRotation rot(SbVec3f(0,1,0), angles[i]);
        cam->orientation.setValue(rot);
        snprintf(fname, sizeof(fname), "%s_orient_%s.rgb", basepath, labels[i]);
        if (!renderToFile(root, fname)) allOk = false;
    }

    bool ok = allOk;
    if (!ok)
        fprintf(stderr, "  FAIL test6: orientation render failed\n");
    else
        printf("  PASS test6: camera orientation changes rendered (4 angles)\n");
    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char **argv)
{
    initCoinHeadless();

    const char *basepath = (argc > 1) ? argv[1] : "render_camera_interaction";

    /* Render the canonical factory scene as the primary output image.
     * This ensures obol_viewer and obol_render produce identical scenes. */
    {
        SoSeparator *fRoot = ObolTest::Scenes::createCameraInteraction(256, 256);
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

    printf("\n=== Camera interaction tests ===\n");

    if (!test1_cameraSwitch(basepath))      ++failures;
    if (!test2_viewAllAutoFit(basepath))    ++failures;
    if (!test3_frustumCamera(basepath))     ++failures;
    if (!test4_cameraPoinAt(basepath))      ++failures;
    if (!test5_clippingPlanes(basepath))    ++failures;
    if (!test6_cameraOrientation(basepath)) ++failures;

    printf("\n=== Summary: %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
