/*
 * render_viewport.cpp — SoViewport API coverage test
 *
 * Exercises the SoViewport single-viewport manager:
 *   1. Default construction: getViewportRegion returns 800×600 full region.
 *   2. setWindowSize / getWindowSize round-trip.
 *   3. setViewportRegion / getViewportRegion round-trip (sub-viewport case).
 *   4. setCamera / getCamera round-trip.
 *   5. setBackgroundColor / getBackgroundColor round-trip.
 *   6. render() produces a non-blank image.
 *   7. viewAll() adjusts camera to fit scene (sanity: no crash + non-blank).
 *   8. processEvent() smoke test (no crash).
 *   9. setSceneGraph(nullptr) clears the scene cleanly.
 *  10. getRoot() returns the internal SoSeparator (non-null).
 *  11. setCamera(nullptr) removes the camera cleanly.
 *
 * The rendered image is written to outputStem+".rgb".
 * The GTest scenario reports any failed contract.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <Inventor/SoViewport.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/events/SoKeyboardEvent.h>
#include <Inventor/events/SoButtonEvent.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbVec2s.h>
#include <cstdio>
#include <cmath>

static const int W = 256;
static const int H = 256;

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

// Simple scene: directional light + blue sphere (no camera).
static SoSeparator * buildScene()
{
    SoSeparator * root = new SoSeparator;
    root->ref();

    SoDirectionalLight * lt = new SoDirectionalLight;
    lt->direction.setValue(-1.0f, -1.0f, -1.0f);
    root->addChild(lt);

    SoMaterial * mat = new SoMaterial;
    mat->diffuseColor.setValue(0.2f, 0.4f, 0.9f);
    root->addChild(mat);

    root->addChild(new SoSphere);
    return root;
}

static bool renderFactoryScene(const char * basepath)
{
    char outpath[1024];
    snprintf(outpath, sizeof(outpath), "%s.rgb", basepath);
    SoSeparator * root = ObolTest::Scenes::createViewport(W, H);
    SoOffscreenRenderer renderer(SbViewportRegion(W, H));
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
    const bool ok = renderer.render(root) &&
        validateNonBlack(renderer.getBuffer(), W * H, "factory") &&
        renderer.writeToRGB(outpath);
    root->unref();
    return ok;
}

static bool defaultRegionIsFullWindow()
{
    SoViewport viewport;
    const SbVec2s window = viewport.getWindowSize();
    const SbVec2s pixels = viewport.getViewportRegion().getViewportSizePixels();
    return window == SbVec2s(800, 600) && pixels == SbVec2s(800, 600);
}

static bool windowSizeRoundTrips()
{
    SoViewport viewport;
    viewport.setWindowSize(SbVec2s(W, H));
    return viewport.getWindowSize() == SbVec2s(W, H) &&
        viewport.getViewportRegion().getViewportSizePixels() == SbVec2s(W, H);
}

static bool viewportRegionRoundTrips()
{
    SoViewport viewport;
    SbViewportRegion region;
    region.setWindowSize(W, H);
    region.setViewportPixels(SbVec2s(10, 20), SbVec2s(100, 80));
    viewport.setViewportRegion(region);
    const SbViewportRegion & result = viewport.getViewportRegion();
    return result.getViewportOriginPixels() == SbVec2s(10, 20) &&
        result.getViewportSizePixels() == SbVec2s(100, 80);
}

static bool cameraRoundTripsAndClears()
{
    SoViewport viewport;
    SoPerspectiveCamera * camera = new SoPerspectiveCamera;
    viewport.setCamera(camera);
    const bool installed = viewport.getCamera() == camera;
    viewport.setCamera(nullptr);
    return installed && viewport.getCamera() == nullptr;
}

static bool backgroundColorRoundTrips()
{
    SoViewport viewport;
    const SbColor expected(0.1f, 0.5f, 0.9f);
    viewport.setBackgroundColor(expected);
    const SbColor & actual = viewport.getBackgroundColor();
    return std::fabs(actual[0] - expected[0]) < 1e-4f &&
        std::fabs(actual[1] - expected[1]) < 1e-4f &&
        std::fabs(actual[2] - expected[2]) < 1e-4f;
}

static bool perspectiveViewportRenders(const char * basepath)
{
    SoSeparator * scene = buildScene();
    SoViewport viewport;
    viewport.setWindowSize(SbVec2s(W, H));
    viewport.setSceneGraph(scene);
    SoPerspectiveCamera * camera = new SoPerspectiveCamera;
    camera->position.setValue(0.0f, 0.0f, 5.0f);
    viewport.setCamera(camera);
    viewport.viewAll();

    SoOffscreenRenderer renderer(getCoinHeadlessContextManager(),
                                 viewport.getViewportRegion());
    renderer.setComponents(SoOffscreenRenderer::RGB);
    char outpath[1024];
    snprintf(outpath, sizeof(outpath), "%s.rgb", basepath);
    const bool ok = camera->nearDistance.getValue() > 0.0f &&
        camera->farDistance.getValue() > camera->nearDistance.getValue() &&
        viewport.render(&renderer) &&
        validateNonBlack(renderer.getBuffer(), W * H, "perspective") &&
        renderer.writeToRGB(outpath);
    scene->unref();
    return ok;
}

static bool distantPerspectiveViewAllRenders()
{
    SoSeparator * scene = buildScene();
    SoViewport viewport;
    viewport.setWindowSize(SbVec2s(W, H));
    viewport.setSceneGraph(scene);
    SoPerspectiveCamera * camera = new SoPerspectiveCamera;
    camera->position.setValue(0.0f, 0.0f, 100.0f);
    viewport.setCamera(camera);
    viewport.viewAll();
    SoOffscreenRenderer renderer(getCoinHeadlessContextManager(),
                                 viewport.getViewportRegion());
    renderer.setComponents(SoOffscreenRenderer::RGB);
    const bool ok = viewport.render(&renderer) &&
        validateNonBlack(renderer.getBuffer(), W * H, "perspective_view_all");
    scene->unref();
    return ok;
}

static bool orthographicViewAllRenders()
{
    SoSeparator * scene = buildScene();
    SoViewport viewport;
    viewport.setWindowSize(SbVec2s(W, H));
    viewport.setSceneGraph(scene);
    SoOrthographicCamera * camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 100.0f);
    viewport.setCamera(camera);
    viewport.viewAll();
    SoOffscreenRenderer renderer(getCoinHeadlessContextManager(),
                                 viewport.getViewportRegion());
    renderer.setComponents(SoOffscreenRenderer::RGB);
    const bool ok = camera->nearDistance.getValue() > 0.0f &&
        camera->farDistance.getValue() > camera->nearDistance.getValue() &&
        viewport.render(&renderer) &&
        validateNonBlack(renderer.getBuffer(), W * H, "orthographic_view_all");
    scene->unref();
    return ok;
}

static bool eventProcessingPreservesViewportState()
{
    SoSeparator * scene = buildScene();
    SoViewport viewport;
    viewport.setSceneGraph(scene);
    SoPerspectiveCamera * camera = new SoPerspectiveCamera;
    viewport.setCamera(camera);
    SoKeyboardEvent event;
    event.setKey(SoKeyboardEvent::ESCAPE);
    event.setState(SoButtonEvent::DOWN);
    viewport.processEvent(&event);
    const bool ok = viewport.getSceneGraph() == scene &&
        viewport.getCamera() == camera;
    scene->unref();
    return ok;
}

static bool sceneGraphRoundTripsAndClears()
{
    SoSeparator * scene = buildScene();
    SoViewport viewport;
    viewport.setSceneGraph(scene);
    const bool installed = viewport.getSceneGraph() == scene;
    viewport.setSceneGraph(nullptr);
    const bool cleared = viewport.getSceneGraph() == nullptr;
    scene->unref();
    return installed && cleared;
}

static bool internalRootExists()
{
    SoViewport viewport;
    return viewport.getRoot() != nullptr;
}

#include "framework/render_test_registration.h"

OBOL_RENDER_TEST_CASE(ViewportRenderTest, FactorySceneRenders,
    "viewport_factory", renderFactoryScene(outputStem.c_str()))
OBOL_RENDER_TEST_CASE(ViewportRenderTest, DefaultRegionIsFullWindow,
    "viewport_default_region", defaultRegionIsFullWindow())
OBOL_RENDER_TEST_CASE(ViewportRenderTest, WindowSizeRoundTrips,
    "viewport_window_size", windowSizeRoundTrips())
OBOL_RENDER_TEST_CASE(ViewportRenderTest, SubregionRoundTrips,
    "viewport_subregion", viewportRegionRoundTrips())
OBOL_RENDER_TEST_CASE(ViewportRenderTest, CameraRoundTripsAndClears,
    "viewport_camera", cameraRoundTripsAndClears())
OBOL_RENDER_TEST_CASE(ViewportRenderTest, BackgroundColorRoundTrips,
    "viewport_background", backgroundColorRoundTrips())
OBOL_RENDER_TEST_CASE(ViewportRenderTest, PerspectiveViewRenders,
    "viewport_perspective", perspectiveViewportRenders(outputStem.c_str()))
OBOL_RENDER_TEST_CASE(ViewportRenderTest, DistantPerspectiveViewAllRenders,
    "viewport_perspective_view_all", distantPerspectiveViewAllRenders())
OBOL_RENDER_TEST_CASE(ViewportRenderTest, OrthographicViewAllRenders,
    "viewport_orthographic_view_all", orthographicViewAllRenders())
OBOL_RENDER_TEST_CASE(ViewportRenderTest, EventProcessingPreservesState,
    "viewport_event", eventProcessingPreservesViewportState())
OBOL_RENDER_TEST_CASE(ViewportRenderTest, SceneGraphRoundTripsAndClears,
    "viewport_scene_graph", sceneGraphRoundTripsAndClears())
OBOL_RENDER_TEST_CASE(ViewportRenderTest, InternalRootExists,
    "viewport_root", internalRootExists())
