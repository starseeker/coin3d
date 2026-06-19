/*
 * Headless version of Inventor Mentor example 13.2
 *
 * Original: ElapsedTime - sliding figure using elapsed time engine
 * Headless: app-owned elapsed-time values update a v2 object transform
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

} // namespace

int main(int argc, char **argv)
{
    initCoinHeadless();

    obol::Scene scene;
    obol::PerspectiveCamera camera;
    camera.position = {2.5f, 0.0f, 8.0f};
    camera.target = {2.5f, 0.0f, 0.0f};
    camera.verticalFieldOfViewRadians = 0.8f;
    scene.setCamera(camera);
    scene.addDirectionalLight(obol::DirectionalLight{});

    obol::Material material;
    material.baseColor = {0.8f, 0.3f, 0.1f, 1.0f};
    obol::Transform objectTransform;
    const obol::SceneObjectId object =
        scene.addPrimitive(obol::Primitive::Cube, material, objectTransform);

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::OffscreenRenderer renderer(backend, target);
    renderer.setBackgroundColor({0.0f, 0.0f, 0.0f, 1.0f});

    const char *baseFilename = (argc > 1) ? argv[1] : "13.2.ElapsedTime";
    char filename[256];

    const obol::Time startTime = obol::Time::unixEpoch();
    for (int i = 0; i <= 10; i++) {
        const obol::Time currentTime =
            startTime + obol::TimeSpan::fromMilliseconds(i * 500.0);
        const float timeValue =
            static_cast<float>((currentTime - startTime).seconds);
        objectTransform.translation = {timeValue, 0.0f, 0.0f};
        scene.setObjectTransform(object, objectTransform);
        printf("Time %.1f: X position = %.2f\n", timeValue, timeValue);

        snprintf(filename, sizeof(filename), "%s_frame%02d.rgb", baseFilename, i);
        if (!renderScene(renderer, scene, filename)) return 1;
    }

    return 0;
}
