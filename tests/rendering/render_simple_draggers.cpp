/*
 * render_simple_draggers.cpp - Render tests for smaller dragger types
 *
 * Exercises dragger types not covered in render_draggers.cpp:
 *   - SoTranslate1Dragger  (constrained 1-axis translation)
 *   - SoTranslate2Dragger  (constrained 2-axis translation)
 *   - SoScale1Dragger      (uniform single-axis scale)
 *   - SoScale2Dragger      (2-axis scale)
 *   - SoScale2UniformDragger (2-axis uniform)
 *   - SoScaleUniformDragger  (isotropic scale)
 *   - SoRotateCylindricalDragger (cylinder-constrained rotation)
 *   - SoRotateDiscDragger        (disc-constrained rotation)
 *   - SoRotateSphericalDragger   (spherical rotation)
 *   - SoTrackballDragger         (full trackball)
 *   - SoDragPointDragger         (3-axis drag point)
 *   - SoJackDragger              (combined rotation + scale)
 *
 * For each dragger:
 *   1. Place it in a minimal scene (camera + light + geometry + dragger).
 *   2. Render before interaction.
 *   3. Simulate a mouse drag across the dragger geometry.
 *   4. Render after interaction (post-drag state).
 *
 * Returns 0 if all succeed, non-0 on failure.
 */

#include "headless_utils.h"

#include <Inventor/SoDB.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoTransform.h>

#include <Inventor/draggers/SoTranslate1Dragger.h>
#include <Inventor/draggers/SoTranslate2Dragger.h>
#include <Inventor/draggers/SoScale1Dragger.h>
#include <Inventor/draggers/SoScale2Dragger.h>
#include <Inventor/draggers/SoScale2UniformDragger.h>
#include <Inventor/draggers/SoScaleUniformDragger.h>
#include <Inventor/draggers/SoRotateCylindricalDragger.h>
#include <Inventor/draggers/SoRotateDiscDragger.h>
#include <Inventor/draggers/SoRotateSphericalDragger.h>
#include <Inventor/draggers/SoTrackballDragger.h>
#include <Inventor/draggers/SoDragPointDragger.h>
#include <Inventor/draggers/SoJackDragger.h>

#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// Build a minimal scene containing a dragger
// ---------------------------------------------------------------------------
static SoSeparator *buildDraggerScene(SoDragger *dragger)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    root->addChild(cam);

    SoDirectionalLight *lt = new SoDirectionalLight;
    lt->direction.setValue(-1.0f, -1.5f, -1.0f);
    root->addChild(lt);

    // Small reference geometry so SoSurroundScale has something to measure
    SoSeparator *geom = new SoSeparator;
    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.5f, 0.7f, 0.5f);
    geom->addChild(mat);
    geom->addChild(new SoCube);
    root->addChild(geom);

    root->addChild(dragger);

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    cam->viewAll(root, vp);

    root->unrefNoDelete();
    return root;
}

// ---------------------------------------------------------------------------
// Test one dragger: render pre-drag, simulate drag, render post-drag
// ---------------------------------------------------------------------------
static bool testDragger(SoDragger *dragger,
                        const char *basepath,
                        const char *name)
{
    dragger->ref();
    SoSeparator *root = buildDraggerScene(dragger);
    root->ref();

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);

    // Pre-drag render
    char fname[1024];
    snprintf(fname, sizeof(fname), "%s_%s_pre.rgb", basepath, name);
    bool r1 = renderToFile(root, fname);
    if (!r1) {
        fprintf(stderr, "  FAIL %s: pre-drag render\n", name);
        root->unref(); dragger->unref();
        return false;
    }

    // Simulate drag across viewport center
    int cx = DEFAULT_WIDTH  / 2;
    int cy = DEFAULT_HEIGHT / 2;
    simulateMouseDrag(root, vp,
                      cx - 25, cy - 15,
                      cx + 25, cy + 15,
                      10 /*steps*/);

    // Post-drag render
    snprintf(fname, sizeof(fname), "%s_%s_post.rgb", basepath, name);
    bool r2 = renderToFile(root, fname);
    if (!r2) {
        fprintf(stderr, "  FAIL %s: post-drag render\n", name);
        root->unref(); dragger->unref();
        return false;
    }

    root->unref();
    dragger->unref();
    printf("  OK %s\n", name);
    return true;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char **argv)
{
    initCoinHeadless();

    const char *basepath = (argc > 1) ? argv[1] : "render_simple_draggers";
    int failures = 0;

    printf("\n=== Simple dragger interaction tests ===\n");

    // Translation draggers
    { SoTranslate1Dragger *d = new SoTranslate1Dragger;
      if (!testDragger(d, basepath, "translate1")) ++failures; }

    { SoTranslate2Dragger *d = new SoTranslate2Dragger;
      if (!testDragger(d, basepath, "translate2")) ++failures; }

    // Scale draggers
    { SoScale1Dragger *d = new SoScale1Dragger;
      if (!testDragger(d, basepath, "scale1")) ++failures; }

    { SoScale2Dragger *d = new SoScale2Dragger;
      if (!testDragger(d, basepath, "scale2")) ++failures; }

    { SoScale2UniformDragger *d = new SoScale2UniformDragger;
      if (!testDragger(d, basepath, "scale2uniform")) ++failures; }

    { SoScaleUniformDragger *d = new SoScaleUniformDragger;
      if (!testDragger(d, basepath, "scaleuniform")) ++failures; }

    // Rotation draggers
    { SoRotateCylindricalDragger *d = new SoRotateCylindricalDragger;
      if (!testDragger(d, basepath, "rotate_cylindrical")) ++failures; }

    { SoRotateDiscDragger *d = new SoRotateDiscDragger;
      if (!testDragger(d, basepath, "rotate_disc")) ++failures; }

    { SoRotateSphericalDragger *d = new SoRotateSphericalDragger;
      if (!testDragger(d, basepath, "rotate_spherical")) ++failures; }

    // Trackball dragger
    { SoTrackballDragger *d = new SoTrackballDragger;
      if (!testDragger(d, basepath, "trackball")) ++failures; }

    // DragPoint dragger
    { SoDragPointDragger *d = new SoDragPointDragger;
      if (!testDragger(d, basepath, "dragpoint")) ++failures; }

    // Jack dragger
    { SoJackDragger *d = new SoJackDragger;
      if (!testDragger(d, basepath, "jack")) ++failures; }

    printf("\n=== Summary: %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
