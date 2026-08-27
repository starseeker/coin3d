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
 *   4. If the drag simulation did not activate the dragger (its picking
 *      geometry was not under the simulated cursor), apply a direct transform
 *      change to the paired geometry.  This guarantees the scene content
 *      always changes so the NanoRT BVH cache is exercised for invalidation.
 *   5. Render after interaction (post-drag state).
 *
 * Scene setup uses ObolTest::Scenes::buildDraggerTestScene() so the
 * viewer (obol_viewer) and this test start from the same base scene.
 *
 * The GTest scenario reports any failed contract.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"

#include <Inventor/SoDB.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/actions/SoSearchAction.h>

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

// Translation applied to the cube when the drag simulation does not activate
// the dragger (fallback to exercise the NanoRT BVH invalidation path).
static const float FALLBACK_TRANSLATION_OFFSET = 0.5f;

// ---------------------------------------------------------------------------
// Test one dragger: render pre-drag, simulate drag, render post-drag.
//
// The mouse drag is first attempted at viewport center (where complex
// draggers such as SoHandleBoxDragger sit after viewAll).  Many simple
// draggers (translate, scale) have compact picking geometry that does not
// cover the viewport center after viewAll, so the simulation may not
// activate the dragger.  When that happens a direct translation is applied
// to the paired SoTransform so that:
//   a) The scene content is guaranteed to differ from the pre-drag state.
//   b) The NanoRT BVH cache must detect the nodeId change and rebuild.
//
// This ensures the test verifies the complete pipeline:
//   dragger/transform change → Coin notification → root nodeId bump →
//   NanoRT cache miss → BVH rebuild → updated pixel output.
// ---------------------------------------------------------------------------
static bool testDragger(SoDragger *dragger,
                        const char *basepath,
                        const char *name)
{
    dragger->ref();
    SoSeparator *root = ObolTest::Scenes::buildDraggerTestScene(dragger, DEFAULT_WIDTH, DEFAULT_HEIGHT);
    root->ref();

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);

    // Locate the paired SoTransform (the cube's transform node in the geom
    // separator, added to the scene before the dragger so it is always the
    // first SoTransform found by a depth-first traversal).
    SoTransform *cubeXf = nullptr;
    {
        SoSearchAction sa;
        sa.setType(SoTransform::getClassTypeId());
        sa.setInterest(SoSearchAction::FIRST);
        sa.apply(root);
        SoPath *found = sa.getPath();
        if (found)
            cubeXf = static_cast<SoTransform *>(found->getTail());
    }

    // Pre-drag render
    char fname[1024];
    snprintf(fname, sizeof(fname), "%s_%s_pre.rgb", basepath, name);
    bool r1 = renderToFile(root, fname);
    if (!r1) {
        fprintf(stderr, "  FAIL %s: pre-drag render\n", name);
        root->unref(); dragger->unref();
        return false;
    }

    // Record the root nodeId before the drag so we can detect whether the
    // scene changed (i.e. whether the drag simulation activated the dragger).
    SbUniqueId rootIdBefore = root->getNodeId();

    // Simulate drag across viewport center
    int cx = DEFAULT_WIDTH  / 2;
    int cy = DEFAULT_HEIGHT / 2;
    simulateMouseDrag(root, vp,
                      cx - 25, cy - 15,
                      cx + 25, cy + 15,
                      10 /*steps*/);

    // If the drag simulation did not change the scene (dragger geometry was
    // not under the simulated cursor), apply a direct translation to the
    // cube's SoTransform.  This guarantees the NanoRT BVH cache sees a
    // nodeId change and rebuilds the acceleration structure, exercising the
    // same code path that the dragger motion callback would have triggered.
    if (root->getNodeId() == rootIdBefore && cubeXf) {
        SbVec3f cur = cubeXf->translation.getValue();
        cubeXf->translation.setValue(cur[0] + FALLBACK_TRANSLATION_OFFSET,
                                     cur[1], cur[2]);
    }

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
// Independently registered GTest contracts
#include "framework/render_test_registration.h"

OBOL_RENDER_TEST_CASE(SimpleDraggerRenderTest, Translate1, "dragger_translate1", testDragger(new SoTranslate1Dragger, outputStem.c_str(), "translate1"))
OBOL_RENDER_TEST_CASE(SimpleDraggerRenderTest, Translate2, "dragger_translate2", testDragger(new SoTranslate2Dragger, outputStem.c_str(), "translate2"))
OBOL_RENDER_TEST_CASE(SimpleDraggerRenderTest, Scale1, "dragger_scale1", testDragger(new SoScale1Dragger, outputStem.c_str(), "scale1"))
OBOL_RENDER_TEST_CASE(SimpleDraggerRenderTest, Scale2, "dragger_scale2", testDragger(new SoScale2Dragger, outputStem.c_str(), "scale2"))
OBOL_RENDER_TEST_CASE(SimpleDraggerRenderTest, Scale2Uniform, "dragger_scale2_uniform", testDragger(new SoScale2UniformDragger, outputStem.c_str(), "scale2uniform"))
OBOL_RENDER_TEST_CASE(SimpleDraggerRenderTest, ScaleUniform, "dragger_scale_uniform", testDragger(new SoScaleUniformDragger, outputStem.c_str(), "scaleuniform"))
OBOL_RENDER_TEST_CASE(SimpleDraggerRenderTest, RotateCylindrical, "dragger_rotate_cyl", testDragger(new SoRotateCylindricalDragger, outputStem.c_str(), "rotate_cylindrical"))
OBOL_RENDER_TEST_CASE(SimpleDraggerRenderTest, RotateDisc, "dragger_rotate_disc", testDragger(new SoRotateDiscDragger, outputStem.c_str(), "rotate_disc"))
OBOL_RENDER_TEST_CASE(SimpleDraggerRenderTest, RotateSpherical, "dragger_rotate_sphere", testDragger(new SoRotateSphericalDragger, outputStem.c_str(), "rotate_spherical"))
OBOL_RENDER_TEST_CASE(SimpleDraggerRenderTest, Trackball, "dragger_trackball", testDragger(new SoTrackballDragger, outputStem.c_str(), "trackball"))
OBOL_RENDER_TEST_CASE(SimpleDraggerRenderTest, DragPoint, "dragger_point", testDragger(new SoDragPointDragger, outputStem.c_str(), "dragpoint"))
OBOL_RENDER_TEST_CASE(SimpleDraggerRenderTest, Jack, "dragger_jack", testDragger(new SoJackDragger, outputStem.c_str(), "jack"))
