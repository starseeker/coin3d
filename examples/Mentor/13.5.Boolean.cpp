/*
 * Headless version of Inventor Mentor example 13.5
 *
 * Original: Boolean - uses boolean engine to toggle between objects
 * Headless: app-owned boolean state toggles v2 object transforms
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cstdio>

namespace {

bool renderScene(obol::OffscreenRenderer & renderer,
                 obol::Scene & scene,
                 const char * filename)
{
    const obol::FrameResult result = renderer.render(scene);
    return result.success && renderer.writeRGB(filename);
}

obol::Material material(float r, float g, float b)
{
    obol::Material result;
    result.baseColor = {r, g, b, 1.0f};
    return result;
}

obol::Transform visible()
{
    return obol::Transform{};
}

obol::Transform hidden()
{
    obol::Transform transform;
    transform.translation = {1000.0f, 1000.0f, 1000.0f};
    transform.scale = {0.001f, 0.001f, 0.001f};
    return transform;
}

obol::PerspectiveCamera viewAllCamera(const obol::Scene & scene)
{
    obol::ViewAllRequest request;
    request.viewportWidth = DEFAULT_WIDTH;
    request.viewportHeight = DEFAULT_HEIGHT;
    return obol::CameraFraming::viewAllPerspective(scene, request);
}

} // namespace

int main(int argc, char **argv)
{
    initCoinHeadless();

    obol::Scene scene;
    scene.addDirectionalLight(obol::DirectionalLight{});

    const obol::SceneObjectId cube =
        scene.addPrimitive(obol::Primitive::Cube, material(1.0f, 0.0f, 0.0f));
    const obol::SceneObjectId sphere =
        scene.addPrimitive(obol::Primitive::Sphere, material(0.0f, 0.0f, 1.0f), hidden());

    obol::Scene cameraScene;
    cameraScene.addDirectionalLight(obol::DirectionalLight{});
    cameraScene.addPrimitive(obol::Primitive::Cube, material(1.0f, 0.0f, 0.0f));
    scene.setCamera(viewAllCamera(cameraScene));

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::OffscreenRenderer renderer(backend, target);
    renderer.setBackgroundColor({0.0f, 0.0f, 0.0f, 1.0f});

    const char *baseFilename = (argc > 1) ? argv[1] : "13.5.Boolean";
    char filename[256];

    for (int i = 0; i <= 8; i++) {
        const float timeValue = i * 0.5f;
        const int which = i % 2;
        scene.setObjectTransform(cube, which == 0 ? visible() : hidden());
        scene.setObjectTransform(sphere, which == 1 ? visible() : hidden());

        printf("Time %.1f: Showing %s (whichChild=%d)\n",
               timeValue, which == 0 ? "Cube" : "Sphere", which);
        snprintf(filename, sizeof(filename), "%s_frame%02d.rgb", baseFilename, i);
        if (!renderScene(renderer, scene, filename)) return 1;
    }

    return 0;
}
