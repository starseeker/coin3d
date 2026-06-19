/*
 * Headless version of Inventor Mentor example 13.3
 *
 * Original: TimeCounter - jumping figure using time counter engines
 * Headless: app-owned counter values update v2 object transforms
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cmath>
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
    camera.position = {-11.0f, -7.0f, 32.0f};
    camera.target = {-6.0f, 0.0f, 0.0f};
    camera.verticalFieldOfViewRadians = 1.25663706144f;
    camera.nearDistance = 0.1f;
    camera.farDistance = 100.0f;
    scene.setCamera(camera);
    scene.addDirectionalLight(obol::DirectionalLight{});

    obol::Material material;
    material.baseColor = {0.1f, 0.3f, 0.8f, 1.0f};
    obol::Transform objectTransform;
    objectTransform.translation = {-20.0f, 0.0f, 0.0f};
    objectTransform.scale = {4.0f, 4.0f, 4.0f};
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

    const char *baseFilename = (argc > 1) ? argv[1] : "13.3.TimeCounter";
    char filename[256];

    const obol::Time startTime = obol::Time::unixEpoch();
    for (int i = 0; i <= 20; i++) {
        const obol::Time currentTime =
            startTime + obol::TimeSpan::fromMilliseconds(i * 500.0);
        const float timeValue =
            static_cast<float>((currentTime - startTime).seconds);
        const float x = std::fmod(timeValue * 0.15f * 40.0f, 41.0f);
        const float y = std::fmod(timeValue * 1.5f * 4.0f, 5.0f);
        objectTransform.translation = {-20.0f + x, y, 0.0f};
        scene.setObjectTransform(object, objectTransform);

        printf("Time %.1f: Position = (%.1f, %.1f)\n", timeValue, x, y);
        snprintf(filename, sizeof(filename), "%s_frame%02d.rgb", baseFilename, i);
        if (!renderScene(renderer, scene, filename)) return 1;
    }

    return 0;
}
