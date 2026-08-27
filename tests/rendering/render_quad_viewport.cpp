/*
 * render_quad_viewport.cpp — SoQuadViewport API coverage test
 *
 * Exercises the SoQuadViewport manager and its SoViewport composition:
 *   1. getQuadrantSize() for a standard window.
 *   2. setCamera / getCamera round-trip + active-quadrant selection.
 *   3. Background colour round-trip (via underlying SoViewport).
 *   4. Viewport tile layout (origin/size for TOP_LEFT and BOTTOM_RIGHT).
 *   5. Render all four quadrants (shared LOD scene); verify non-blank output.
 *      LOD per-view: close camera → green sphere, far camera → red cone.
 *   6. getViewport() gives direct access to the underlying SoViewport tile.
 *   7. processEvent() smoke test (routes to active quadrant, no crash).
 *   8. setSceneGraph() replacement and nullptr removal.
 *   9. viewAll() / viewAllQuadrants() with no camera — must not crash.
 *
 * The last rendered quadrant is written to outputStem+".rgb".
 * The GTest scenario reports any failed contract.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <Inventor/SoQuadViewport.h>
#include <Inventor/SoViewport.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoLOD.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/events/SoKeyboardEvent.h>
#include <Inventor/events/SoButtonEvent.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbVec2s.h>
#include <cstdio>
#include <cmath>

static const int W = 800;
static const int H = 600;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool validateNonBlack(const unsigned char * buf, int npix,
                              const char * label, int threshold = 20)
{
    int nonbg = 0;
    for (int i = 0; i < npix; ++i) {
        const unsigned char * p = buf + i * 3;
        if (p[0] > 5 || p[1] > 5 || p[2] > 5) ++nonbg;
    }
    printf("  %s: nonbg=%d\n", label, nonbg);
    return nonbg >= threshold;
}

// Return index of the dominant colour channel (0=R, 1=G, 2=B).
static int dominantChannel(const unsigned char * buf, int npix)
{
    long sum[3] = { 0, 0, 0 };
    for (int i = 0; i < npix; ++i) {
        sum[0] += buf[i * 3 + 0];
        sum[1] += buf[i * 3 + 1];
        sum[2] += buf[i * 3 + 2];
    }
    int dom = 0;
    if (sum[1] > sum[dom]) dom = 1;
    if (sum[2] > sum[dom]) dom = 2;
    return dom;
}

// ---------------------------------------------------------------------------
// Shared LOD scene (no camera)
// ---------------------------------------------------------------------------
// SoLOD ranges: [0,5) → green sphere, [5,12) → orange cube, [12,∞) → red cone
static SoSeparator * buildLODScene()
{
    SoSeparator * root = new SoSeparator;
    root->ref();

    SoDirectionalLight * lt = new SoDirectionalLight;
    lt->direction.setValue(-1.0f, -1.0f, -1.0f);
    root->addChild(lt);

    SoLOD * lod = new SoLOD;
    lod->range.set1Value(0, 5.0f);
    lod->range.set1Value(1, 12.0f);

    SoSeparator * hi = new SoSeparator;
    SoMaterial  * hiMat = new SoMaterial;
    hiMat->diffuseColor.setValue(0.1f, 0.8f, 0.1f);   // green
    hi->addChild(hiMat);
    hi->addChild(new SoSphere);
    lod->addChild(hi);

    SoSeparator * med = new SoSeparator;
    SoMaterial  * medMat = new SoMaterial;
    medMat->diffuseColor.setValue(0.9f, 0.5f, 0.1f);  // orange
    med->addChild(medMat);
    med->addChild(new SoCube);
    lod->addChild(med);

    SoSeparator * lo = new SoSeparator;
    SoMaterial  * loMat = new SoMaterial;
    loMat->diffuseColor.setValue(0.8f, 0.1f, 0.1f);   // red
    lo->addChild(loMat);
    lo->addChild(new SoCone);
    lod->addChild(lo);

    root->addChild(lod);
    return root;
}

static void configureLodCameras(SoQuadViewport & viewport)
{
    const float cameraZ[3] = { 3.0f, 8.0f, 20.0f };
    for (int index = 0; index < 3; ++index) {
        SoPerspectiveCamera * camera = new SoPerspectiveCamera;
        camera->position.setValue(0.0f, 0.0f, cameraZ[index]);
        camera->pointAt(SbVec3f(0.0f, 0.0f, 0.0f));
        camera->nearDistance.setValue(0.1f);
        camera->farDistance.setValue(cameraZ[index] + 5.0f);
        viewport.setCamera(index, camera);
    }
    SoOrthographicCamera * camera = new SoOrthographicCamera;
    camera->position.setValue(5.0f, 0.0f, 5.0f);
    camera->pointAt(SbVec3f(0.0f, 0.0f, 0.0f));
    camera->height.setValue(4.0f);
    camera->nearDistance.setValue(0.1f);
    camera->farDistance.setValue(20.0f);
    viewport.setCamera(SoQuadViewport::BOTTOM_RIGHT, camera);
}

static bool renderFactoryScene(const char * basepath)
{
    char outpath[1024];
    snprintf(outpath, sizeof(outpath), "%s.rgb", basepath);
    SoSeparator * root = ObolTest::Scenes::createQuadViewport(256, 256);
    SoOffscreenRenderer renderer(SbViewportRegion(256, 256));
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
    const bool ok = renderer.render(root) &&
        validateNonBlack(renderer.getBuffer(), 256 * 256, "factory") &&
        renderer.writeToRGB(outpath);
    root->unref();
    return ok;
}

static bool quadrantSizeIsHalfWindow()
{
    SoQuadViewport viewport;
    viewport.setWindowSize(SbVec2s(W, H));
    return viewport.getQuadrantSize() == SbVec2s(W / 2, H / 2);
}

static bool camerasAndActiveQuadrantRoundTrip()
{
    SoQuadViewport viewport;
    SoPerspectiveCamera * cameras[SoQuadViewport::NUM_QUADS];
    for (int index = 0; index < SoQuadViewport::NUM_QUADS; ++index) {
        cameras[index] = new SoPerspectiveCamera;
        viewport.setCamera(index, cameras[index]);
    }
    bool ok = true;
    for (int index = 0; index < SoQuadViewport::NUM_QUADS; ++index) {
        ok = ok && viewport.getCamera(index) == cameras[index];
    }
    viewport.setActiveQuadrant(SoQuadViewport::BOTTOM_LEFT);
    return ok && viewport.getActiveQuadrant() == SoQuadViewport::BOTTOM_LEFT;
}

static bool backgroundColorReachesTile()
{
    SoQuadViewport viewport;
    const SbColor expected(0.2f, 0.3f, 0.4f);
    viewport.setBackgroundColor(SoQuadViewport::TOP_RIGHT, expected);
    const SbColor & actual = viewport.getBackgroundColor(SoQuadViewport::TOP_RIGHT);
    const SoViewport * tile = viewport.getViewport(SoQuadViewport::TOP_RIGHT);
    return tile != nullptr &&
        std::fabs(actual[0] - expected[0]) < 1e-4f &&
        std::fabs(actual[1] - expected[1]) < 1e-4f &&
        std::fabs(actual[2] - expected[2]) < 1e-4f &&
        std::fabs(tile->getBackgroundColor()[0] - expected[0]) < 1e-4f;
}

static bool tileLayoutIsCorrect()
{
    SoQuadViewport viewport;
    viewport.setWindowSize(SbVec2s(W, H));
    const SbViewportRegion & topLeft =
        viewport.getViewportRegion(SoQuadViewport::TOP_LEFT);
    const SbViewportRegion & bottomRight =
        viewport.getViewportRegion(SoQuadViewport::BOTTOM_RIGHT);
    return topLeft.getViewportOriginPixels() == SbVec2s(0, H / 2) &&
        topLeft.getViewportSizePixels() == SbVec2s(W / 2, H / 2) &&
        bottomRight.getViewportOriginPixels() == SbVec2s(W / 2, 0);
}

static bool quadrantsRenderExpectedLod(const char * basepath)
{
    SoSeparator * scene = buildLODScene();
    SoQuadViewport viewport;
    viewport.setWindowSize(SbVec2s(W, H));
    viewport.setSceneGraph(scene);
    configureLodCameras(viewport);
    const SbVec2s size = viewport.getQuadrantSize();
    SoOffscreenRenderer renderer(getCoinHeadlessContextManager(),
                                 SbViewportRegion(size[0], size[1]));
    renderer.setComponents(SoOffscreenRenderer::RGB);
    bool ok = true;
    int dominants[SoQuadViewport::NUM_QUADS] = {-1, -1, -1, -1};
    for (int index = 0; index < SoQuadViewport::NUM_QUADS; ++index) {
        const bool rendered = viewport.renderQuadrant(index, &renderer);
        ok = ok && rendered;
        if (rendered) {
            ok = ok && validateNonBlack(renderer.getBuffer(), size[0] * size[1],
                                        "quadrant");
            dominants[index] = dominantChannel(renderer.getBuffer(),
                                               size[0] * size[1]);
        }
    }
    char outpath[1024];
    snprintf(outpath, sizeof(outpath), "%s.rgb", basepath);
    ok = ok && dominants[SoQuadViewport::TOP_LEFT] == 1 &&
        dominants[SoQuadViewport::BOTTOM_LEFT] == 0 &&
        renderer.writeToRGB(outpath);
    scene->unref();
    return ok;
}

static bool viewportAccessChecksBounds()
{
    SoQuadViewport viewport;
    bool ok = true;
    for (int index = 0; index < SoQuadViewport::NUM_QUADS; ++index) {
        ok = ok && viewport.getViewport(index) != nullptr;
    }
    return ok && viewport.getViewport(-1) == nullptr &&
        viewport.getViewport(SoQuadViewport::NUM_QUADS) == nullptr;
}

static bool eventProcessingPreservesActiveTile()
{
    SoSeparator * scene = buildLODScene();
    SoQuadViewport viewport;
    viewport.setSceneGraph(scene);
    SoPerspectiveCamera * camera = new SoPerspectiveCamera;
    viewport.setCamera(SoQuadViewport::TOP_LEFT, camera);
    viewport.setActiveQuadrant(SoQuadViewport::TOP_LEFT);
    SoKeyboardEvent event;
    event.setKey(SoKeyboardEvent::ESCAPE);
    event.setState(SoButtonEvent::DOWN);
    viewport.processEvent(&event);
    const bool ok = viewport.getSceneGraph() == scene &&
        viewport.getCamera(SoQuadViewport::TOP_LEFT) == camera &&
        viewport.getActiveQuadrant() == SoQuadViewport::TOP_LEFT;
    scene->unref();
    return ok;
}

static bool sceneGraphReplacesAndClears()
{
    SoSeparator * first = buildLODScene();
    SoSeparator * second = buildLODScene();
    SoQuadViewport viewport;
    viewport.setSceneGraph(first);
    const bool firstInstalled = viewport.getSceneGraph() == first;
    viewport.setSceneGraph(second);
    const bool secondInstalled = viewport.getSceneGraph() == second;
    viewport.setSceneGraph(nullptr);
    const bool cleared = viewport.getSceneGraph() == nullptr;
    first->unref();
    second->unref();
    return firstInstalled && secondInstalled && cleared;
}

static bool viewAllWithoutCamerasPreservesState()
{
    SoSeparator * scene = buildLODScene();
    SoQuadViewport viewport;
    viewport.setSceneGraph(scene);
    viewport.viewAll(SoQuadViewport::TOP_LEFT);
    viewport.viewAllQuadrants();
    bool ok = viewport.getSceneGraph() == scene;
    for (int index = 0; index < SoQuadViewport::NUM_QUADS; ++index) {
        ok = ok && viewport.getCamera(index) == nullptr;
    }
    scene->unref();
    return ok;
}

static bool borderPropertiesRoundTripAndClamp()
{
    SoQuadViewport viewport;
    const bool defaultWidth = viewport.getBorderWidth() == 0;
    viewport.setBorderWidth(4);
    const bool setWidth = viewport.getBorderWidth() == 4;
    viewport.setBorderWidth(-1);
    const bool clamped = viewport.getBorderWidth() == 0;
    const SbColor expected(1.0f, 1.0f, 0.0f);
    viewport.setBorderColor(expected);
    const SbColor & actual = viewport.getBorderColor();
    return defaultWidth && setWidth && clamped &&
        std::fabs(actual[0] - expected[0]) < 1e-4f &&
        std::fabs(actual[1] - expected[1]) < 1e-4f &&
        std::fabs(actual[2] - expected[2]) < 1e-4f;
}

static bool compositeContainsWhiteBorders(const char * basepath)
{
    SoSeparator * scene = buildLODScene();
    SoQuadViewport viewport;
    viewport.setWindowSize(SbVec2s(W, H));
    viewport.setSceneGraph(scene);
    configureLodCameras(viewport);
    viewport.setBorderWidth(4);
    viewport.setBorderColor(SbColor(1.0f, 1.0f, 1.0f));
    const SbVec2s size = viewport.getQuadrantSize();
    SoOffscreenRenderer renderer(getCoinHeadlessContextManager(),
                                 SbViewportRegion(size[0], size[1]));
    renderer.setComponents(SoOffscreenRenderer::RGB);
    char outpath[1024];
    snprintf(outpath, sizeof(outpath), "%s.rgb", basepath);
    bool ok = viewport.writeCompositeToRGB(outpath, &renderer);

    FILE * file = ok ? fopen(outpath, "rb") : nullptr;
    ok = ok && file != nullptr;
    const int planeSize = W * H;
    unsigned char * planes = new unsigned char[planeSize * 3];
    if (file) {
        ok = fseek(file, 512, SEEK_SET) == 0 &&
            fread(planes, 1, planeSize * 3, file) ==
                static_cast<size_t>(planeSize * 3);
        fclose(file);
    }
    if (ok) {
        const int horizontal = (H / 2) * W + W / 4;
        const int vertical = (H / 4) * W + W / 2;
        const bool horizontalWhite = planes[horizontal] > 200 &&
            planes[planeSize + horizontal] > 200 &&
            planes[2 * planeSize + horizontal] > 200;
        const bool verticalWhite = planes[vertical] > 200 &&
            planes[planeSize + vertical] > 200 &&
            planes[2 * planeSize + vertical] > 200;
        ok = horizontalWhite && verticalWhite;
    }
    delete[] planes;
    scene->unref();
    return ok;
}

#include "framework/render_test_registration.h"

OBOL_RENDER_TEST_CASE(QuadViewportRenderTest, FactorySceneRenders,
    "quad_viewport_factory", renderFactoryScene(outputStem.c_str()))
OBOL_RENDER_TEST_CASE(QuadViewportRenderTest, QuadrantSizeIsHalfWindow,
    "quad_viewport_size", quadrantSizeIsHalfWindow())
OBOL_RENDER_TEST_CASE(QuadViewportRenderTest, CamerasAndActiveQuadrantRoundTrip,
    "quad_viewport_cameras", camerasAndActiveQuadrantRoundTrip())
OBOL_RENDER_TEST_CASE(QuadViewportRenderTest, BackgroundColorReachesTile,
    "quad_viewport_background", backgroundColorReachesTile())
OBOL_RENDER_TEST_CASE(QuadViewportRenderTest, TileLayoutIsCorrect,
    "quad_viewport_layout", tileLayoutIsCorrect())
OBOL_RENDER_TEST_CASE(QuadViewportRenderTest, QuadrantsRenderExpectedLod,
    "quad_viewport_lod", quadrantsRenderExpectedLod(outputStem.c_str()))
OBOL_RENDER_TEST_CASE(QuadViewportRenderTest, ViewportAccessChecksBounds,
    "quad_viewport_access", viewportAccessChecksBounds())
OBOL_RENDER_TEST_CASE(QuadViewportRenderTest, EventProcessingPreservesActiveTile,
    "quad_viewport_event", eventProcessingPreservesActiveTile())
OBOL_RENDER_TEST_CASE(QuadViewportRenderTest, SceneGraphReplacesAndClears,
    "quad_viewport_scene_graph", sceneGraphReplacesAndClears())
OBOL_RENDER_TEST_CASE(QuadViewportRenderTest, ViewAllWithoutCamerasPreservesState,
    "quad_viewport_view_all", viewAllWithoutCamerasPreservesState())
OBOL_RENDER_TEST_CASE(QuadViewportRenderTest, BorderPropertiesRoundTripAndClamp,
    "quad_viewport_border", borderPropertiesRoundTripAndClamp())
OBOL_RENDER_TEST_CASE(QuadViewportRenderTest, CompositeContainsWhiteBorders,
    "quad_viewport_composite", compositeContainsWhiteBorders(outputStem.c_str()))
