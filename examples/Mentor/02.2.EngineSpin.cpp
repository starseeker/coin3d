/*
 * Headless version of Inventor Mentor example 2.2
 *
 * Original: elapsed-time engine spins a cone.
 * Headless: app-owned time updates a v2 object transform.
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
    material.specular = {0.25f, 0.25f, 0.25f, 1.0f};
    material.shininess = 0.35f;
    return material;
}

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
    camera.position = {0.0f, 0.0f, 6.0f};
    camera.target = {0.0f, 0.0f, 0.0f};
    camera.verticalFieldOfViewRadians = 0.6f;
    scene.setCamera(camera);
    scene.addDirectionalLight(obol::DirectionalLight{});

    obol::PrimitiveOptions coneOptions;
    coneOptions.radius = 1.0f;
    coneOptions.height = 2.0f;
    obol::Transform coneTransform;
    coneTransform.rotationAxis = {1.0f, 0.0f, 0.0f};
    const obol::SceneObjectId cone =
        scene.addPrimitive(obol::Primitive::Cone, red(), coneTransform, coneOptions);

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::OffscreenRenderer renderer(backend, target);
    renderer.setBackgroundColor({0.0f, 0.0f, 0.0f, 1.0f});

    const char *baseFilename = (argc > 1) ? argv[1] : "02.2.EngineSpin";
    char filename[512];

    for (int i = 0; i < 8; i++) {
        coneTransform.rotationRadians = (2.0f * 3.14159265358979323846f * i) / 8.0f;
        scene.setObjectTransform(cone, coneTransform);
        snprintf(filename, sizeof(filename), "%s_frame%02d.rgb", baseFilename, i);
        if (!renderScene(renderer, scene, filename)) return 1;
        if (i == 0) {
            snprintf(filename, sizeof(filename), "%s.rgb", baseFilename);
            if (!renderScene(renderer, scene, filename)) return 1;
        }
    }

    printf("Rendered 8 frames showing rotation animation [Obol v2]\n");
    return 0;
}
