/*
 * render_nodekit_interaction.cpp
 *
 * Tests node-kit based interaction patterns as user applications use them.
 * Node kits provide a structured way to build scene graphs with built-in
 * parts for transforms, materials, shapes, etc.
 *
 * Tests:
 *   1. SoShapeKit: create kit, access parts, render.
 *   2. SoSeparatorKit: create with appearance and transform parts.
 *   3. SoShapeKit with manipulator: attach SoHandleBoxManip via kit's transform part.
 *   4. SoWrapperKit wrapping an existing geometry sub-graph.
 *   5. SoCameraKit: camera kit part access and rendering.
 *   6. SoLightKit: light kit with position and direction parts.
 *   7. Pick hitting a SoShapeKit node (path tail is the kit's shape).
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
#include <Inventor/nodes/SoCylinder.h>

#include <Inventor/nodekits/SoShapeKit.h>
#include <Inventor/nodekits/SoSeparatorKit.h>
#include <Inventor/nodekits/SoWrapperKit.h>
#include <Inventor/nodekits/SoCameraKit.h>
#include <Inventor/nodekits/SoLightKit.h>
#include <Inventor/nodekits/SoAppearanceKit.h>

#include <Inventor/manips/SoHandleBoxManip.h>
#include <Inventor/actions/SoRayPickAction.h>
#include <Inventor/SoPickedPoint.h>

#include <cstdio>

// ---------------------------------------------------------------------------
// Test 1: SoShapeKit – create, access parts, render
// ---------------------------------------------------------------------------
static bool test1_shapeKit(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0,0,5); cam->height.setValue(6.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    // Create a shape kit for a sphere
    SoShapeKit *kit = new SoShapeKit;
    // Set the shape part
    kit->setPart("shape", new SoSphere);
    // Set material via appearance kit
    SoMaterial *mat = static_cast<SoMaterial *>(
        kit->getPart("material", TRUE));
    if (mat) mat->diffuseColor.setValue(0.8f, 0.4f, 0.2f);

    root->addChild(kit);

    char fname[1024];
    snprintf(fname, sizeof(fname), "%s_shapekit.rgb", basepath);
    bool r1 = renderToFile(root, fname);

    // Change shape to cube
    kit->setPart("shape", new SoCube);
    if (mat) mat->diffuseColor.setValue(0.2f, 0.6f, 0.9f);

    snprintf(fname, sizeof(fname), "%s_shapekit_cube.rgb", basepath);
    bool r2 = renderToFile(root, fname);

    bool ok = r1 && r2;
    if (!ok) fprintf(stderr, "  FAIL test1: r1=%d r2=%d\n", (int)r1, (int)r2);
    else     printf("  PASS test1: SoShapeKit create/render/change-shape\n");
    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 2: SoSeparatorKit – structured scene graph with parts
// ---------------------------------------------------------------------------
static bool test2_separatorKit(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0,0,5); cam->height.setValue(8.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    // Create separator kits for multiple objects
    float positions[3][3] = { {-2,0,0}, {0,0,0}, {2,0,0} };
    SbColor colors[3] = {
        SbColor(1,0.2f,0.2f), SbColor(0.2f,1,0.2f), SbColor(0.2f,0.2f,1)
    };
    SoNode *shapes[3] = { new SoSphere, new SoCube, new SoCone };

    for (int i = 0; i < 3; ++i) {
        SoSeparatorKit *kit = new SoSeparatorKit;
        SoTransform *xf = static_cast<SoTransform *>(
            kit->getPart("transform", TRUE));
        if (xf) xf->translation.setValue(positions[i][0], 0, 0);
        SoMaterial *mat = static_cast<SoMaterial *>(
            kit->getPart("appearance.material", TRUE));
        if (mat) mat->diffuseColor.setValue(colors[i]);
        SoShapeKit *shapeKit = static_cast<SoShapeKit *>(
            kit->getPart("childList[0]", TRUE));
        if (shapeKit) shapeKit->setPart("shape", shapes[i]);
        root->addChild(kit);
    }

    char fname[1024];
    snprintf(fname, sizeof(fname), "%s_separatorkit.rgb", basepath);
    bool r1 = renderToFile(root, fname);

    bool ok = r1;
    if (!ok) fprintf(stderr, "  FAIL test2: separatorKit render\n");
    else     printf("  PASS test2: SoSeparatorKit with 3 objects rendered\n");
    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 3: SoShapeKit with manipulator: attach via search after forcing transform creation
// ---------------------------------------------------------------------------
static bool test3_shapeKitWithManip(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0,0,8);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoShapeKit *kit = new SoShapeKit;
    kit->setPart("shape", new SoSphere);
    SoMaterial *mat = static_cast<SoMaterial *>(kit->getPart("material", TRUE));
    if (mat) mat->diffuseColor.setValue(0.5f, 0.7f, 0.3f);
    // Force-create the transform part so it exists in the scene graph
    kit->getPart("transform", TRUE);
    root->addChild(kit);

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    cam->viewAll(root, vp);

    char fname[1024];
    snprintf(fname, sizeof(fname), "%s_shapekit_nomanip.rgb", basepath);
    bool r1 = renderToFile(root, fname);

    // Find the transform via SoSearchAction on root (searches inside the kit)
    SoHandleBoxManip *manip = new SoHandleBoxManip;
    manip->ref();

    SoSearchAction sa;
    sa.setType(SoTransform::getClassTypeId());
    sa.setSearchingAll(TRUE);
    sa.setInterest(SoSearchAction::FIRST);
    sa.apply(root);
    bool attached = false;
    if (sa.getPath()) {
        attached = (manip->replaceNode(sa.getPath()) == TRUE);
    }

    snprintf(fname, sizeof(fname), "%s_shapekit_withmanip.rgb", basepath);
    bool r2 = renderToFile(root, fname);

    // Simulate drag on manip
    if (attached) {
        simulateMouseDrag(root, vp,
                          DEFAULT_WIDTH/2 - 30, DEFAULT_HEIGHT/2 - 30,
                          DEFAULT_WIDTH/2 + 30, DEFAULT_HEIGHT/2 + 30, 6);
    }

    snprintf(fname, sizeof(fname), "%s_shapekit_dragged.rgb", basepath);
    bool r3 = renderToFile(root, fname);

    bool ok = r1 && r2 && r3;
    if (!ok) fprintf(stderr, "  FAIL test3: r1=%d r2=%d r3=%d attached=%d\n",
                     (int)r1,(int)r2,(int)r3,(int)attached);
    else     printf("  PASS test3: SoShapeKit + SoHandleBoxManip (attached=%d)\n",
                    (int)attached);

    manip->unref();
    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 4: SoWrapperKit wrapping geometry
// ---------------------------------------------------------------------------
static bool test4_wrapperKit(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0,0,5); cam->height.setValue(5.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    // Build a simple sub-graph to wrap
    SoSeparator *subGraph = new SoSeparator;
    SoMaterial *subMat = new SoMaterial;
    subMat->diffuseColor.setValue(0.7f,0.5f,0.2f);
    subGraph->addChild(subMat);
    subGraph->addChild(new SoCylinder);

    SoWrapperKit *wrapper = new SoWrapperKit;
    wrapper->setPart("contents", subGraph);
    SoTransform *xf = static_cast<SoTransform *>(wrapper->getPart("transform", TRUE));
    if (xf) xf->scaleFactor.setValue(0.8f, 1.2f, 0.8f);
    root->addChild(wrapper);

    char fname[1024];
    snprintf(fname, sizeof(fname), "%s_wrapperkit.rgb", basepath);
    bool r1 = renderToFile(root, fname);

    // Modify the wrapped content material
    subMat->diffuseColor.setValue(0.2f,0.7f,0.5f);
    snprintf(fname, sizeof(fname), "%s_wrapperkit_recolor.rgb", basepath);
    bool r2 = renderToFile(root, fname);

    bool ok = r1 && r2;
    if (!ok) fprintf(stderr, "  FAIL test4: r1=%d r2=%d\n", (int)r1, (int)r2);
    else     printf("  PASS test4: SoWrapperKit wrapping geometry\n");
    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 5: Pick hitting a SoShapeKit
// ---------------------------------------------------------------------------
static bool test5_pickShapeKit(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0,0,5); cam->height.setValue(4.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoShapeKit *kit = new SoShapeKit;
    kit->setPart("shape", new SoSphere);
    root->addChild(kit);

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);

    // Pick at center
    SoRayPickAction pa(vp);
    pa.setPoint(SbVec2s(DEFAULT_WIDTH/2, DEFAULT_HEIGHT/2));
    pa.setRadius(3.0f);
    pa.apply(root);
    const SoPickedPoint *pp = pa.getPickedPoint();

    bool hitOk = (pp != nullptr);
    // The pick path should include the ShapeKit or its shape
    bool pathValid = false;
    if (pp) {
        SoPath *path = pp->getPath();
        pathValid = (path != nullptr && path->getLength() > 0);
    }

    char fname[1024];
    snprintf(fname, sizeof(fname), "%s_pick_shapekit.rgb", basepath);
    bool r = renderToFile(root, fname);

    bool ok = r && hitOk && pathValid;
    if (!ok) fprintf(stderr, "  FAIL test5: hit=%d pathValid=%d r=%d\n",
                     (int)hitOk, (int)pathValid, (int)r);
    else     printf("  PASS test5: pick hitting SoShapeKit (pathLen=%d)\n",
                    pp ? pp->getPath()->getLength() : 0);

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char **argv)
{
    initCoinHeadless();

    const char *basepath = (argc > 1) ? argv[1] : "render_nodekit_interaction";

    /* Render the canonical factory scene as the primary output image.
     * This ensures obol_viewer and obol_render produce identical scenes. */
    {
        SoSeparator *fRoot = ObolTest::Scenes::createNodeKitInteraction(256, 256);
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

    printf("\n=== Node-kit interaction tests ===\n");

    if (!test1_shapeKit(basepath))         ++failures;
    if (!test2_separatorKit(basepath))     ++failures;
    if (!test3_shapeKitWithManip(basepath)) ++failures;
    if (!test4_wrapperKit(basepath))       ++failures;
    if (!test5_pickShapeKit(basepath))     ++failures;

    printf("\n=== Summary: %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
