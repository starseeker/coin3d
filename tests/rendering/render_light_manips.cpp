/*
 * render_light_manips.cpp - Light manipulator render + interaction tests
 *
 * Exercises:
 *   1. SoDirectionalLightManip  - directional light with dragger handle
 *   2. SoPointLightManip        - point light with position dragger
 *   3. SoSpotLightManip         - spot light with cone + position dragger
 *   4. SoClipPlaneManip         - clip plane with interactive normal handle
 *
 * For each manip:
 *   - Place it in a scene with geometry (so lighting effects are visible).
 *   - Render the initial (unmodified) state.
 *   - Simulate a mouse drag on the manip geometry.
 *   - Render the post-interaction state.
 *
 * Uses SoSearchAction to locate the manip in the scene after construction.
 *
 * Returns 0 on pass, non-0 on failure.
 */

#include "headless_utils.h"

#include <Inventor/SoDB.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoPointLight.h>
#include <Inventor/nodes/SoSpotLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoClipPlane.h>

#include <Inventor/manips/SoDirectionalLightManip.h>
#include <Inventor/manips/SoPointLightManip.h>
#include <Inventor/manips/SoSpotLightManip.h>
#include <Inventor/manips/SoClipPlaneManip.h>

#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// Build a scene with diffuse spheres for light testing
// ---------------------------------------------------------------------------
static SoSeparator *buildLitScene()
{
    SoSeparator *scene = new SoSeparator;

    // A few spheres at different positions
    float positions[3][3] = { {-2.0f,0,0}, {0,0,0}, {2.0f,0,0} };
    SbColor colors[3] = {
        SbColor(0.8f, 0.3f, 0.3f),
        SbColor(0.3f, 0.8f, 0.3f),
        SbColor(0.3f, 0.3f, 0.8f)
    };
    for (int i = 0; i < 3; ++i) {
        SoSeparator *sep = new SoSeparator;
        SoTransform *xf = new SoTransform;
        xf->translation.setValue(positions[i][0], positions[i][1], positions[i][2]);
        sep->addChild(xf);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(colors[i]);
        mat->specularColor.setValue(0.8f, 0.8f, 0.8f);
        mat->shininess.setValue(0.5f);
        sep->addChild(mat);
        SoSphere *sph = new SoSphere;
        sph->radius = 0.7f;
        sep->addChild(sph);
        scene->addChild(sep);
    }
    // Floor plane (flattened cube)
    SoSeparator *floor = new SoSeparator;
    SoTransform *fxf = new SoTransform;
    fxf->translation.setValue(0.0f, -1.2f, 0.0f);
    floor->addChild(fxf);
    SoMaterial *fm = new SoMaterial;
    fm->diffuseColor.setValue(0.5f, 0.5f, 0.5f);
    floor->addChild(fm);
    SoCube *floorBox = new SoCube;
    floorBox->width  = 8.0f;
    floorBox->height = 0.1f;
    floorBox->depth  = 4.0f;
    floor->addChild(floorBox);
    scene->addChild(floor);

    return scene;
}

// ---------------------------------------------------------------------------
// Test SoDirectionalLightManip
// ---------------------------------------------------------------------------
static bool testDirectionalLightManip(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 3.0f, 8.0f);
    cam->pointAt(SbVec3f(0, 0, 0), SbVec3f(0, 1, 0));
    root->addChild(cam);

    SoDirectionalLightManip *manip = new SoDirectionalLightManip;
    manip->direction.setValue(-0.5f, -1.0f, -0.5f);
    manip->color.setValue(1.0f, 1.0f, 0.9f);
    root->addChild(manip);

    root->addChild(buildLitScene());

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    cam->viewAll(root, vp);

    char fname[1024];
    snprintf(fname, sizeof(fname), "%s_dirlight_pre.rgb", basepath);
    bool r1 = renderToFile(root, fname);

    // Simulate drag to reorient the light direction dragger
    int cx = DEFAULT_WIDTH / 2;
    int cy = DEFAULT_HEIGHT / 2;
    simulateMouseDrag(root, vp, cx - 40, cy - 10, cx + 40, cy + 10, 8);

    snprintf(fname, sizeof(fname), "%s_dirlight_post.rgb", basepath);
    bool r2 = renderToFile(root, fname);

    bool ok = r1 && r2;
    printf("  %s SoDirectionalLightManip\n", ok ? "OK" : "FAIL");
    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test SoPointLightManip
// ---------------------------------------------------------------------------
static bool testPointLightManip(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 4.0f, 8.0f);
    cam->pointAt(SbVec3f(0, 0, 0), SbVec3f(0, 1, 0));
    root->addChild(cam);

    SoPointLightManip *manip = new SoPointLightManip;
    manip->location.setValue(0.0f, 2.0f, 0.0f);
    manip->color.setValue(1.0f, 0.95f, 0.8f);
    root->addChild(manip);

    root->addChild(buildLitScene());

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    cam->viewAll(root, vp);

    char fname[1024];
    snprintf(fname, sizeof(fname), "%s_pointlight_pre.rgb", basepath);
    bool r1 = renderToFile(root, fname);

    // Simulate drag to reposition the point light
    int cx = DEFAULT_WIDTH / 2;
    int cy = DEFAULT_HEIGHT / 4; // aim at upper part of viewport where light handle is
    simulateMouseDrag(root, vp, cx - 30, cy, cx + 30, cy + 20, 8);

    snprintf(fname, sizeof(fname), "%s_pointlight_post.rgb", basepath);
    bool r2 = renderToFile(root, fname);

    bool ok = r1 && r2;
    printf("  %s SoPointLightManip\n", ok ? "OK" : "FAIL");
    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test SoSpotLightManip
// ---------------------------------------------------------------------------
static bool testSpotLightManip(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 4.0f, 9.0f);
    cam->pointAt(SbVec3f(0, 0, 0), SbVec3f(0, 1, 0));
    root->addChild(cam);

    SoSpotLightManip *manip = new SoSpotLightManip;
    manip->location.setValue(0.0f, 3.0f, 0.0f);
    manip->direction.setValue(0.0f, -1.0f, 0.0f);
    manip->cutOffAngle.setValue(0.4f);
    manip->color.setValue(1.0f, 1.0f, 0.8f);
    root->addChild(manip);

    root->addChild(buildLitScene());

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    cam->viewAll(root, vp);

    char fname[1024];
    snprintf(fname, sizeof(fname), "%s_spotlight_pre.rgb", basepath);
    bool r1 = renderToFile(root, fname);

    // Simulate drag to adjust spot light direction
    int cx = DEFAULT_WIDTH / 2;
    int cy = DEFAULT_HEIGHT / 3;
    simulateMouseDrag(root, vp, cx - 20, cy - 20, cx + 20, cy + 20, 8);

    snprintf(fname, sizeof(fname), "%s_spotlight_post.rgb", basepath);
    bool r2 = renderToFile(root, fname);

    bool ok = r1 && r2;
    printf("  %s SoSpotLightManip\n", ok ? "OK" : "FAIL");
    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test SoClipPlaneManip
// ---------------------------------------------------------------------------
static bool testClipPlaneManip(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 2.0f, 8.0f);
    cam->pointAt(SbVec3f(0, 0, 0), SbVec3f(0, 1, 0));
    root->addChild(cam);

    SoDirectionalLight *lt = new SoDirectionalLight;
    lt->direction.setValue(-0.5f, -1.0f, -0.5f);
    root->addChild(lt);

    // Clip plane manip: clip at x=0 (split the scene in half)
    SoClipPlaneManip *clipManip = new SoClipPlaneManip;
    clipManip->plane.setValue(SbPlane(SbVec3f(1.0f, 0.0f, 0.0f), 0.0f));
    root->addChild(clipManip);

    // Add a large sphere to make clipping visible
    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.6f, 0.3f, 0.8f);
    root->addChild(mat);
    SoSphere *sph = new SoSphere;
    sph->radius = 1.5f;
    root->addChild(sph);

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    cam->viewAll(root, vp);

    char fname[1024];
    snprintf(fname, sizeof(fname), "%s_clipplane_pre.rgb", basepath);
    bool r1 = renderToFile(root, fname);

    // Simulate drag on the clip plane handle
    int cx = DEFAULT_WIDTH / 2;
    int cy = DEFAULT_HEIGHT / 2;
    simulateMouseDrag(root, vp, cx - 30, cy, cx + 30, cy, 8);

    snprintf(fname, sizeof(fname), "%s_clipplane_post.rgb", basepath);
    bool r2 = renderToFile(root, fname);

    bool ok = r1 && r2;
    printf("  %s SoClipPlaneManip\n", ok ? "OK" : "FAIL");
    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char **argv)
{
    initCoinHeadless();

    const char *basepath = (argc > 1) ? argv[1] : "render_light_manips";
    int failures = 0;

    printf("\n=== Light manipulator interaction tests ===\n");

    if (!testDirectionalLightManip(basepath)) ++failures;
    if (!testPointLightManip(basepath))       ++failures;
    if (!testSpotLightManip(basepath))        ++failures;
    if (!testClipPlaneManip(basepath))        ++failures;

    printf("\n=== Summary: %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
