/*
 * Headless version of Inventor Mentor example 2.4
 *
 * Original: examiner viewer tumbles and dollies around a cone.
 * Headless: app-owned camera operations update v2 camera state.
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cmath>
#include <cstdio>

namespace {

obol::Material red()
{
    obol::Material material;
    material.baseColor = {1.0f, 0.0f, 0.0f, 1.0f};
    return material;
}

bool renderScene(obol::OffscreenRenderer & renderer,
                 obol::Scene & scene,
                 const char * filename)
{
    const obol::FrameResult result = renderer.render(scene);
    return result.success && renderer.writeRGB(filename);
}

obol::PerspectiveCamera viewAllCamera(const obol::Scene & scene)
{
    obol::ViewAllRequest request;
    request.viewportWidth = DEFAULT_WIDTH;
    request.viewportHeight = DEFAULT_HEIGHT;
    return obol::CameraFraming::viewAllPerspective(scene, request);
}

float length(const obol::Vec3 & value)
{
    return std::sqrt(value.x * value.x +
                     value.y * value.y +
                     value.z * value.z);
}

} // namespace

int main(int argc, char **argv)
{
    initCoinHeadless();

    obol::Scene scene;
    scene.addDirectionalLight(obol::DirectionalLight{});
    scene.addPrimitive(obol::Primitive::Cone, red());
    const obol::PerspectiveCamera initialCamera = viewAllCamera(scene);
    scene.setCamera(initialCamera);

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::OffscreenRenderer renderer(backend, target);
    renderer.setBackgroundColor({0.0f, 0.0f, 0.0f, 1.0f});

    const char *baseFilename = (argc > 1) ? argv[1] : "02.4.Examiner";
    char filename[512];
    int frame = 0;

    snprintf(filename, sizeof(filename), "%s_frame%02d_initial.rgb", baseFilename, frame++);
    if (!renderScene(renderer, scene, filename)) return 1;
    snprintf(filename, sizeof(filename), "%s.rgb", baseFilename);
    if (!renderScene(renderer, scene, filename)) return 1;

    for (int i = 1; i <= 8; i++) {
        const float angle = (3.14159265358979323846f / 4.0f) * i;
        obol::PerspectiveCamera camera = initialCamera;
        const float radius = length(initialCamera.position);
        camera.position = {
            radius * std::sin(angle),
            initialCamera.position.y,
            radius * std::cos(angle)
        };
        camera.target = {0.0f, 0.0f, 0.0f};
        scene.setCamera(camera);
        snprintf(filename, sizeof(filename), "%s_frame%02d_tumble.rgb", baseFilename, frame++);
        if (!renderScene(renderer, scene, filename)) return 1;
    }

    for (int i = 0; i < 4; i++) {
        const float scale = 0.5f + i * 0.5f;
        obol::PerspectiveCamera camera = initialCamera;
        camera.position = {
            initialCamera.position.x * scale,
            initialCamera.position.y * scale,
            initialCamera.position.z * scale
        };
        camera.nearDistance = initialCamera.nearDistance * scale;
        camera.farDistance = initialCamera.farDistance * scale;
        scene.setCamera(camera);
        snprintf(filename, sizeof(filename), "%s_frame%02d_dolly.rgb", baseFilename, frame++);
        if (!renderScene(renderer, scene, filename)) return 1;
    }

    printf("Rendered %d frames simulating examiner viewer operations [Obol v2]\n", frame);
    return 0;
}
