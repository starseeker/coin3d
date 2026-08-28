/*
 * render_draggers.cpp - Renders complex draggers and manips with simulated events
 *
 * Exercises SoHandleBoxDragger, SoTabBoxDragger, SoTransformBoxDragger,
 * SoTransformerDragger, SoCenterballDragger, and their corresponding manips
 * using a real GL context (OSMesa/GLX).  Simulates mouse-press, mouse-move
 * and mouse-release events via SoHandleEventAction to verify that the full
 * interaction pipeline works without crashing.
 *
 * This test verifies:
 *   1. Every complex dragger/manip can be placed in a scene graph and rendered.
 *   2. Event simulation (press + drag + release) does not crash.
 *   3. The manip replaceNode / replaceManip lifecycle works under a GL context.
 *
 * Scene setup uses ObolTest::Scenes::buildDraggerTestScene() so the
 * viewer (obol_viewer) and this test start from the same base scene.
 *
 * Writes one RGB image per dragger/manip to outputStem_<name>.rgb.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"

#include <Inventor/SoDB.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/actions/SoSearchAction.h>

#include <Inventor/draggers/SoHandleBoxDragger.h>
#include <Inventor/draggers/SoTabBoxDragger.h>
#include <Inventor/draggers/SoTransformBoxDragger.h>
#include <Inventor/draggers/SoTransformerDragger.h>
#include <Inventor/draggers/SoCenterballDragger.h>

#include <Inventor/manips/SoHandleBoxManip.h>
#include <Inventor/manips/SoTabBoxManip.h>
#include <Inventor/manips/SoTransformBoxManip.h>
#include <Inventor/manips/SoTransformerManip.h>
#include <Inventor/manips/SoCenterballManip.h>

#include <cstdio>
#include <cstring>
#include <cmath>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Render a scene and write an RGB file, returning true on success
static bool renderScene(SoNode *root, const char *basepath, const char *suffix)
{
    char filename[1024];
    snprintf(filename, sizeof(filename), "%s_%s.rgb", basepath, suffix);
    return renderToFile(root, filename);
}

// ---------------------------------------------------------------------------
// Dragger event-simulation test
//
// Places a dragger in a scene, renders before interaction, simulates a drag
// gesture (press + move + release), then renders after interaction.
// Returns true if both renders succeed without crashing.
// ---------------------------------------------------------------------------
static bool testDraggerInteraction(SoDragger *dragger,
                                   const char *basepath,
                                   const char *name)
{
    dragger->ref();
    SoSeparator *root = ObolTest::Scenes::buildDraggerTestScene(dragger, DEFAULT_WIDTH, DEFAULT_HEIGHT);
    root->ref();

    SbViewportRegion viewport(DEFAULT_WIDTH, DEFAULT_HEIGHT);

    // Render the initial (pre-drag) state
    if (!renderScene(root, basepath, name)) {
        fprintf(stderr, "  FAIL: initial render failed for %s\n", name);
        root->unref();
        dragger->unref();
        return false;
    }

    // Simulate a mouse drag across the center of the viewport.
    // These coordinates land on the dragger's geometry (which sits roughly
    // at the viewport center after viewAll()).
    int cx = DEFAULT_WIDTH  / 2;
    int cy = DEFAULT_HEIGHT / 2;
    simulateMouseDrag(root, viewport,
                      cx - 30, cy - 30,
                      cx + 30, cy + 30,
                      8 /*steps*/);

    // Render the post-drag state
    char postname[256];
    snprintf(postname, sizeof(postname), "%s_post", name);
    if (!renderScene(root, basepath, postname)) {
        fprintf(stderr, "  FAIL: post-drag render failed for %s\n", name);
        root->unref();
        dragger->unref();
        return false;
    }

    root->unref();
    dragger->unref();
    printf("  OK: %s (pre + post drag rendered)\n", name);
    return true;
}

// ---------------------------------------------------------------------------
// Manip event-simulation test
//
// Attaches a manip to a SoTransform node in a scene, simulates mouse events,
// then detaches the manip.  Returns true if all steps succeed.
// ---------------------------------------------------------------------------
static bool testManipInteraction(SoTransformManip *manip,
                                 const char *basepath,
                                 const char *name)
{
    manip->ref();

    // Build scene with a cube + plain SoTransform, then replace with manip
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    root->addChild(cam);
    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-1.0f, -1.5f, -1.0f);
    root->addChild(light);

    SoSeparator *objSep = new SoSeparator;
    SoTransform *xf = new SoTransform;
    xf->translation.setValue(0.0f, 0.0f, 0.0f);
    objSep->addChild(xf);
    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.8f, 0.5f, 0.2f);
    objSep->addChild(mat);
    objSep->addChild(new SoCube);
    root->addChild(objSep);

    SbViewportRegion viewport(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    cam->viewAll(root, viewport);
    // Do not move camera after viewAll - moving it without re-running viewAll
    // can push geometry outside the near/far clip planes

    // Render before attaching manip
    if (!renderScene(root, basepath, name)) {
        fprintf(stderr, "  FAIL: pre-attach render failed for %s\n", name);
        root->unref();
        manip->unref();
        return false;
    }

    // Attach manip: replace the SoTransform with the manip
    SoSearchAction sa;
    sa.setType(SoTransform::getClassTypeId());
    sa.setInterest(SoSearchAction::FIRST);
    sa.apply(root);
    SoPath *xfPath = sa.getPath();
    bool attached = false;
    if (xfPath) {
        attached = (manip->replaceNode(xfPath) == TRUE);
    }

    if (!attached) {
        fprintf(stderr, "  FAIL: replaceNode failed for %s\n", name);
        root->unref();
        manip->unref();
        return false;
    }

    // Render with manip attached
    char attachname[256];
    snprintf(attachname, sizeof(attachname), "%s_attached", name);
    if (!renderScene(root, basepath, attachname)) {
        fprintf(stderr, "  FAIL: attached render failed for %s\n", name);
        root->unref();
        manip->unref();
        return false;
    }

    // Simulate a drag interaction with the manip
    int cx = DEFAULT_WIDTH  / 2;
    int cy = DEFAULT_HEIGHT / 2;
    simulateMouseDrag(root, viewport,
                      cx - 20, cy - 20,
                      cx + 20, cy + 20,
                      6 /*steps*/);

    // Render post-drag
    char postname[256];
    snprintf(postname, sizeof(postname), "%s_post_drag", name);
    const bool postrendered = renderScene(root, basepath, postname);

    // Detach the manip
    SoSearchAction sa2;
    sa2.setType(manip->getTypeId());
    sa2.setInterest(SoSearchAction::FIRST);
    sa2.apply(root);
    SoPath *manipPath = sa2.getPath();
    bool detached = false;
    if (manipPath) {
        detached = (manip->replaceManip(manipPath, nullptr) == TRUE);
    }

    // Render after detaching manip
    char detachname[256];
    snprintf(detachname, sizeof(detachname), "%s_detached", name);
    const bool detachrendered = renderScene(root, basepath, detachname);

    root->unref();
    manip->unref();

    if (!detached || !postrendered || !detachrendered) {
        fprintf(stderr,
                "  FAIL: %s detached=%d post-render=%d detach-render=%d\n",
                name, (int)detached, (int)postrendered, (int)detachrendered);
        return false;
    }

    printf("  OK: %s (attach, drag, detach)\n", name);
    return true;
}

// ---------------------------------------------------------------------------
// Independently registered GTest contracts
#include "framework/render_test_registration.h"

OBOL_RENDER_TEST_CASE(DraggerInteractionRenderTest, HandleBoxDragger, "dragger_handlebox", testDraggerInteraction(new SoHandleBoxDragger, outputStem.c_str(), "handlebox_dragger"))
OBOL_RENDER_TEST_CASE(DraggerInteractionRenderTest, TabBoxDragger, "dragger_tabbox", testDraggerInteraction(new SoTabBoxDragger, outputStem.c_str(), "tabbox_dragger"))
OBOL_RENDER_TEST_CASE(DraggerInteractionRenderTest, TransformBoxDragger, "dragger_transformbox", testDraggerInteraction(new SoTransformBoxDragger, outputStem.c_str(), "transformbox_dragger"))
OBOL_RENDER_TEST_CASE(DraggerInteractionRenderTest, TransformerDragger, "dragger_transformer", testDraggerInteraction(new SoTransformerDragger, outputStem.c_str(), "transformer_dragger"))
OBOL_RENDER_TEST_CASE(DraggerInteractionRenderTest, CenterballDragger, "dragger_centerball", testDraggerInteraction(new SoCenterballDragger, outputStem.c_str(), "centerball_dragger"))
OBOL_RENDER_TEST_CASE(DraggerInteractionRenderTest, HandleBoxManip, "manip_handlebox", testManipInteraction(new SoHandleBoxManip, outputStem.c_str(), "handlebox_manip"))
OBOL_RENDER_TEST_CASE(DraggerInteractionRenderTest, TabBoxManip, "manip_tabbox", testManipInteraction(new SoTabBoxManip, outputStem.c_str(), "tabbox_manip"))
OBOL_RENDER_TEST_CASE(DraggerInteractionRenderTest, TransformBoxManip, "manip_transformbox", testManipInteraction(new SoTransformBoxManip, outputStem.c_str(), "transformbox_manip"))
OBOL_RENDER_TEST_CASE(DraggerInteractionRenderTest, TransformerManip, "manip_transformer", testManipInteraction(new SoTransformerManip, outputStem.c_str(), "transformer_manip"))
OBOL_RENDER_TEST_CASE(DraggerInteractionRenderTest, CenterballManip, "manip_centerball", testManipInteraction(new SoCenterballManip, outputStem.c_str(), "centerball_manip"))
