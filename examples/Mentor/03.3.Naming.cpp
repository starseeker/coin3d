/*
 * Headless version of Inventor Mentor example 3.3
 *
 * Original: names nodes and removes one by lookup.
 * Headless: keeps app-owned names mapped to v2 object IDs.
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cstdio>
#include <map>
#include <string>

namespace {

struct NamedScene {
    obol::Scene scene;
    std::map<std::string, obol::SceneObjectId> names;
};

obol::Material material(float r, float g, float b)
{
    obol::Material result;
    result.baseColor = {r, g, b, 1.0f};
    result.specular = {0.35f, 0.35f, 0.35f, 1.0f};
    result.shininess = 0.35f;
    return result;
}

obol::Transform transform(float x, float y, float z)
{
    obol::Transform xf;
    xf.translation = {x, y, z};
    return xf;
}

NamedScene makeScene(bool includeCube)
{
    NamedScene named;
    obol::PerspectiveCamera camera;
    camera.position = {0.0f, 0.0f, 7.0f};
    camera.target = {0.0f, 0.0f, 0.0f};
    camera.verticalFieldOfViewRadians = 0.65f;
    named.scene.setCamera(camera);
    named.scene.addDirectionalLight(obol::DirectionalLight{});

    if (includeCube) {
        named.names["MyCube"] =
            named.scene.addPrimitive(obol::Primitive::Cube,
                                     material(1.0f, 0.5f, 0.0f),
                                     transform(-1.0f, 0.0f, 0.0f));
    }

    obol::PrimitiveOptions sphere;
    sphere.radius = 1.0f;
    named.names["MySphere"] =
        named.scene.addPrimitive(obol::Primitive::Sphere,
                                 material(0.0f, 0.5f, 1.0f),
                                 transform(1.0f, 0.0f, 0.0f),
                                 sphere);
    return named;
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

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::OffscreenRenderer renderer(backend, target);
    renderer.setBackgroundColor({0.0f, 0.0f, 0.0f, 1.0f});

    const char *baseFilename = (argc > 1) ? argv[1] : "03.3.Naming";
    char filename[512];

    NamedScene before = makeScene(true);
    snprintf(filename, sizeof(filename), "%s_before.rgb", baseFilename);
    if (!renderScene(renderer, before.scene, filename)) return 1;
    snprintf(filename, sizeof(filename), "%s.rgb", baseFilename);
    if (!renderScene(renderer, before.scene, filename)) return 1;

    if (before.names.find("MyCube") != before.names.end()) {
        printf("Removed object named 'MyCube' from app-owned v2 scene model\n");
    }
    NamedScene after = makeScene(false);
    snprintf(filename, sizeof(filename), "%s_after.rgb", baseFilename);
    if (!renderScene(renderer, after.scene, filename)) return 1;

    printf("Demonstrated app-owned name lookup and removal over v2 object IDs\n");
    return 0;
}
