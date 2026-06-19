/*
 * Headless version of Inventor Mentor example 13.8
 *
 * Original: Blinker - blinking neon sign with fast and slow blinkers
 * Headless: app-owned blinker state toggles v2 object transforms
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

obol::Transform transform(float x, float y, float z, float sx, float sy, float sz)
{
    obol::Transform result;
    result.translation = {x, y, z};
    result.scale = {sx, sy, sz};
    return result;
}

} // namespace

int main(int argc, char **argv)
{
    initCoinHeadless();

    obol::Scene scene;
    obol::PerspectiveCamera camera;
    camera.position = {0.0f, -0.2f, 12.4f};
    camera.target = {0.0f, -0.2f, 0.0f};
    camera.verticalFieldOfViewRadians = 0.72f;
    scene.setCamera(camera);
    scene.addDirectionalLight(obol::DirectionalLight{});

    scene.addPrimitive(obol::Primitive::Cube,
                       material(0.8f, 0.8f, 0.8f),
                       transform(0.0f, 2.5f, 0.0f, 3.0f, 0.5f, 1.0f));

    const obol::Transform fastVisible = transform(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
    const obol::Transform slowVisible = transform(0.0f, -2.5f, 0.0f, 1.0f, 1.0f, 1.0f);
    const obol::SceneObjectId fast =
        scene.addPrimitive(obol::Primitive::Cone, material(1.0f, 0.0f, 0.0f), fastVisible);
    const obol::SceneObjectId slow =
        scene.addPrimitive(obol::Primitive::Cylinder, material(0.0f, 1.0f, 0.0f), slowVisible);

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::Renderer renderer(backend);

    const char *baseFilename = (argc > 1) ? argv[1] : "13.8.Blinker";
    char filename[256];

    for (int i = 0; i <= 16; i++) {
        const float time = i * 0.25f;
        const bool fastOn = (int(time / 0.5f) % 2) == 0;
        const bool slowOn = (int(time / 2.0f) % 2) == 0;
        scene.setObjectVisible(fast, fastOn);
        scene.setObjectVisible(slow, slowOn);
        scene.setObjectTransform(fast, fastVisible);
        scene.setObjectTransform(slow, slowVisible);

        printf("Time %.2f: Fast=%s, Slow=%s\n",
               time,
               fastOn ? "ON " : "OFF",
               slowOn ? "ON " : "OFF");
        snprintf(filename, sizeof(filename), "%s_frame%02d.rgb", baseFilename, i);
        if (!renderScene(renderer, scene, target, filename)) return 1;
    }

    return 0;
}
