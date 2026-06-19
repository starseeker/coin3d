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

bool renderScene(obol::Renderer & renderer,
                 obol::Scene & scene,
                 const obol::RenderTarget & target,
                 const char * filename)
{
    obol::FrameRequest request;
    request.scene = &scene;
    request.target = target;
    request.background = {0.0f, 0.0f, 0.0f, 1.0f};
    const obol::FrameResult result = renderer.render(request);
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
        scene.addPrimitive(obol::Primitive::Sphere, material(0.0f, 0.0f, 1.0f));
    scene.setObjectVisible(sphere, false);

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
    obol::Renderer renderer(backend);

    const char *baseFilename = (argc > 1) ? argv[1] : "13.5.Boolean";
    char filename[256];

    for (int i = 0; i <= 8; i++) {
        const float timeValue = i * 0.5f;
        const int which = i % 2;
        scene.setObjectVisible(cube, which == 0);
        scene.setObjectVisible(sphere, which == 1);
        scene.setObjectTransform(cube, visible());
        scene.setObjectTransform(sphere, visible());

        printf("Time %.1f: Showing %s (whichChild=%d)\n",
               timeValue, which == 0 ? "Cube" : "Sphere", which);
        snprintf(filename, sizeof(filename), "%s_frame%02d.rgb", baseFilename, i);
        if (!renderScene(renderer, scene, target, filename)) return 1;
    }

    return 0;
}
