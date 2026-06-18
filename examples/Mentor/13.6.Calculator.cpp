/*
 * Headless version of Inventor Mentor example 13.6
 *
 * Original: Calculator - uses calculator engine for complex motion paths
 * Headless: app-owned math expression updates v2 object transforms
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cmath>
#include <cstdio>

namespace {

constexpr float kPi = 3.14159265358979323846f;

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
    camera.position = {0.0f, 0.0f, 7.0f};
    camera.target = {0.0f, 0.0f, 0.0f};
    camera.verticalFieldOfViewRadians = 0.6f;
    scene.setCamera(camera);
    scene.addDirectionalLight(obol::DirectionalLight{});

    obol::Material material;
    material.baseColor = {0.1f, 0.8f, 0.3f, 1.0f};
    obol::Transform transform;
    transform.translation = {1.0f, 0.0f, 0.0f};
    const obol::SceneObjectId object =
        scene.addPrimitive(obol::Primitive::Cube, material, transform);

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::OffscreenRenderer renderer(backend, target);
    renderer.setBackgroundColor({0.0f, 0.0f, 0.0f, 1.0f});

    const char *baseFilename = (argc > 1) ? argv[1] : "13.6.Calculator";
    char filename[256];

    for (int i = 0; i <= 16; i++) {
        const float timeValue = static_cast<float>(i) * kPi / 8.0f;
        const float x = std::cos(timeValue);
        const float y = std::sin(timeValue);
        transform.translation = {x, y, 0.0f};
        scene.setObjectTransform(object, transform);

        printf("Time %.3f: Position = (%.2f, %.2f, %.2f)\n",
               timeValue, x, y, 0.0f);
        snprintf(filename, sizeof(filename), "%s_frame%02d.rgb", baseFilename, i);
        if (!renderScene(renderer, scene, filename)) return 1;
    }

    return 0;
}
