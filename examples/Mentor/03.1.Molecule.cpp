/*
 * Headless version of Inventor Mentor example 3.1
 *
 * Original: constructs a water molecule scene graph.
 * Headless: constructs the same hierarchy with v2 scene objects/groups.
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cmath>
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

    obol::PrimitiveOptions scaledHydrogen;
    scaledHydrogen.radius = 0.75f;
    scene.addPrimitive(obol::Primitive::Sphere,
                       material(1.0f, 1.0f, 1.0f),
                       transform(0.0f, -1.2f, 0.0f),
                       scaledHydrogen,
                       molecule);

    obol::PrimitiveOptions defaultHydrogen;
    defaultHydrogen.radius = 0.75f;
    // The original example uses SoGroup, so hydrogen1's scale/translation
    // remains in the traversal state when hydrogen2 is visited.
    const float inheritedScale = 0.75f;
    const float inheritedTranslationY = -1.2f;
    scene.addPrimitive(obol::Primitive::Sphere,
                       material(1.0f, 1.0f, 1.0f),
                       transform(1.1852f * inheritedScale,
                                 inheritedTranslationY +
                                     1.3877f * inheritedScale,
                                 0.0f),
                       defaultHydrogen,
                       molecule);
}

bool renderView(obol::Renderer & renderer,
                obol::Scene & scene,
                const obol::RenderTarget & target,
                const char * filename,
                const obol::PerspectiveCamera & camera)
{
    scene.setCamera(camera);
    obol::FrameRequest request;
    request.scene = &scene;
    request.target = target;
    request.background = {0.0f, 0.0f, 0.0f, 1.0f};
    const obol::FrameResult result = renderer.render(request);
    return result.success && renderer.writeRGB(filename);
}

obol::PerspectiveCamera orbitCamera(const obol::PerspectiveCamera & camera,
                                    float azimuth,
                                    float elevation)
{
    obol::CameraOrbitRequest request;
    request.camera = camera;
    request.azimuthRadians = azimuth;
    request.elevationRadians = elevation;
    return obol::CameraFraming::orbit(request);
}

void makeCameras(const obol::Scene & scene,
                 obol::PerspectiveCamera & frontCamera,
                 obol::PerspectiveCamera & sideCamera,
                 obol::PerspectiveCamera & angleCamera)
{
    obol::ViewAllRequest request;
    request.viewportWidth = DEFAULT_WIDTH;
    request.viewportHeight = DEFAULT_HEIGHT;
    frontCamera = obol::CameraFraming::viewAllPerspective(scene, request);
    sideCamera = orbitCamera(frontCamera, static_cast<float>(M_PI / 2.0), 0.0f);
    angleCamera = orbitCamera(frontCamera, 0.0f, static_cast<float>(M_PI / 4.0));
}

} // namespace

int main(int argc, char **argv)
{
    initCoinHeadless();

    obol::Scene scene;
    scene.addDirectionalLight(obol::DirectionalLight{});
    addWaterMolecule(scene);
    obol::PerspectiveCamera frontCamera;
    obol::PerspectiveCamera sideCamera;
    obol::PerspectiveCamera angleCamera;
    makeCameras(scene, frontCamera, sideCamera, angleCamera);

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::Renderer renderer(backend);

    const char *baseFilename = (argc > 1) ? argv[1] : "03.1.Molecule";
    char filename[512];

    snprintf(filename, sizeof(filename), "%s_front.rgb", baseFilename);
    if (!renderView(renderer, scene, target, filename, frontCamera)) return 1;
    snprintf(filename, sizeof(filename), "%s.rgb", baseFilename);
    if (!renderView(renderer, scene, target, filename, frontCamera)) return 1;

    snprintf(filename, sizeof(filename), "%s_side.rgb", baseFilename);
    if (!renderView(renderer, scene, target, filename, sideCamera)) return 1;

    snprintf(filename, sizeof(filename), "%s_angle.rgb", baseFilename);
    if (!renderView(renderer, scene, target, filename, angleCamera)) return 1;

    return 0;
}
