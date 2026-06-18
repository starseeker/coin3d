/*
 * Headless version of Inventor Mentor example 13.7
 *
 * Original: Rotor - rotating windmill vanes
 * Headless: app-owned rotor angle updates a v2 group transform
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
    camera.position = {0.0f, -8.0f, 9.0f};
    camera.target = {0.0f, 0.0f, 0.0f};
    camera.verticalFieldOfViewRadians = 0.72f;
    scene.setCamera(camera);
    scene.addDirectionalLight(obol::DirectionalLight{});

    scene.addPrimitive(obol::Primitive::Cylinder,
                       material(0.5f, 0.3f, 0.1f),
                       transform(0.0f, -1.5f, 0.0f, 0.4f, 4.0f, 0.4f));

    const obol::SceneGroupId vaneGroup = scene.addGroup();
    scene.addPrimitive(obol::Primitive::Sphere,
                       material(0.75f, 0.75f, 0.85f),
                       obol::Transform{},
                       obol::PrimitiveOptions{},
                       vaneGroup);

    for (int b = 0; b < 4; ++b) {
        const float angle = static_cast<float>(b) * kPi / 2.0f;
        obol::Transform blade;
        blade.translation = {std::cos(angle) * 1.25f, std::sin(angle) * 1.25f, 0.0f};
        blade.rotationAxis = {0.0f, 0.0f, 1.0f};
        blade.rotationRadians = angle;
        blade.scale = {0.35f, 2.5f, 0.15f};
        scene.addPrimitive(obol::Primitive::Cube,
                           material(0.75f, 0.75f, 0.85f),
                           blade,
                           obol::PrimitiveOptions{},
                           vaneGroup);
    }

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::OffscreenRenderer renderer(backend, target);
    renderer.setBackgroundColor({0.0f, 0.0f, 0.0f, 1.0f});

    const char *baseFilename = (argc > 1) ? argv[1] : "13.7.Rotor";
    char filename[256];

    for (int i = 0; i <= 12; i++) {
        const float angle = static_cast<float>(i) * kPi / 6.0f;
        obol::Transform rotor;
        rotor.rotationAxis = {0.0f, 0.0f, 1.0f};
        rotor.rotationRadians = angle;
        scene.setGroupTransform(vaneGroup, rotor);

        printf("Frame %d: Rotation angle = %.1f degrees\n", i, angle * 180.0f / kPi);
        snprintf(filename, sizeof(filename), "%s_frame%02d.rgb", baseFilename, i);
        if (!renderScene(renderer, scene, filename)) return 1;
    }

    return 0;
}
