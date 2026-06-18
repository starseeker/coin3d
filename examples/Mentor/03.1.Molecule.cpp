/*
 * Headless version of Inventor Mentor example 3.1
 *
 * Original: constructs a water molecule scene graph.
 * Headless: constructs the same hierarchy with v2 scene objects/groups.
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cstdio>

namespace {

obol::Material material(float r, float g, float b)
{
    obol::Material result;
    result.baseColor = {r, g, b, 1.0f};
    result.specular = {0.5f, 0.5f, 0.5f, 1.0f};
    result.shininess = 0.5f;
    return result;
}

obol::Transform transform(float x, float y, float z)
{
    obol::Transform xf;
    xf.translation = {x, y, z};
    return xf;
}

void addWaterMolecule(obol::Scene & scene)
{
    const obol::SceneGroupId molecule = scene.addGroup();

    obol::PrimitiveOptions oxygen;
    oxygen.radius = 1.0f;
    scene.addPrimitive(obol::Primitive::Sphere,
                       material(1.0f, 0.0f, 0.0f),
                       transform(0.0f, 0.0f, 0.0f),
                       oxygen,
                       molecule);

    obol::PrimitiveOptions hydrogen;
    hydrogen.radius = 0.75f;
    scene.addPrimitive(obol::Primitive::Sphere,
                       material(1.0f, 1.0f, 1.0f),
                       transform(0.0f, -1.2f, 0.0f),
                       hydrogen,
                       molecule);
    scene.addPrimitive(obol::Primitive::Sphere,
                       material(1.0f, 1.0f, 1.0f),
                       transform(1.1852f, 1.3877f, 0.0f),
                       hydrogen,
                       molecule);
}

bool renderScene(obol::OffscreenRenderer & renderer,
                 obol::Scene & scene,
                 const char * filename)
{
    const obol::FrameResult result = renderer.render(scene);
    return result.success && renderer.writeRGB(filename);
}

void setCamera(obol::Scene & scene, const obol::Vec3 & position)
{
    obol::PerspectiveCamera camera;
    camera.position = position;
    camera.target = {0.3f, 0.0f, 0.0f};
    camera.verticalFieldOfViewRadians = 0.65f;
    scene.setCamera(camera);
}

} // namespace

int main(int argc, char **argv)
{
    initCoinHeadless();

    obol::Scene scene;
    scene.addDirectionalLight(obol::DirectionalLight{});
    addWaterMolecule(scene);

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::OffscreenRenderer renderer(backend, target);
    renderer.setBackgroundColor({0.0f, 0.0f, 0.0f, 1.0f});

    const char *baseFilename = (argc > 1) ? argv[1] : "03.1.Molecule";
    char filename[512];

    setCamera(scene, {0.0f, 0.0f, 6.0f});
    snprintf(filename, sizeof(filename), "%s_front.rgb", baseFilename);
    if (!renderScene(renderer, scene, filename)) return 1;
    snprintf(filename, sizeof(filename), "%s.rgb", baseFilename);
    if (!renderScene(renderer, scene, filename)) return 1;

    setCamera(scene, {6.0f, 0.0f, 0.0f});
    snprintf(filename, sizeof(filename), "%s_side.rgb", baseFilename);
    if (!renderScene(renderer, scene, filename)) return 1;

    setCamera(scene, {4.0f, 3.0f, 5.0f});
    snprintf(filename, sizeof(filename), "%s_angle.rgb", baseFilename);
    if (!renderScene(renderer, scene, filename)) return 1;

    return 0;
}
