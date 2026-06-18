/*
 * Headless version of Inventor Mentor example 15.1
 *
 * Original: Translate1Dragger drives a cone radius through an engine.
 * Headless: application-owned drag value updates v2 primitive options by ID.
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cstdio>

namespace {

obol::Material material(float r, float g, float b)
{
    obol::Material result;
    result.baseColor = {r, g, b, 1.0f};
    result.specular = {0.25f, 0.25f, 0.25f, 1.0f};
    result.shininess = 0.35f;
    return result;
}

obol::Transform transform(float x, float y, float z)
{
    obol::Transform xf;
    xf.translation = {x, y, z};
    return xf;
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
    camera.position = {0.0f, 3.0f, 9.0f};
    camera.target = {0.0f, 1.6f, 0.0f};
    camera.verticalFieldOfViewRadians = 0.6f;
    scene.setCamera(camera);
    scene.addDirectionalLight(obol::DirectionalLight{});

    obol::PrimitiveOptions coneOptions;
    coneOptions.radius = 1.0f;
    coneOptions.height = 3.0f;
    const obol::SceneObjectId cone =
        scene.addPrimitive(obol::Primitive::Cone,
                           material(0.85f, 0.55f, 0.2f),
                           transform(0.0f, 2.2f, 0.0f),
                           coneOptions);

    obol::PrimitiveOptions markerOptions;
    markerOptions.width = 0.25f;
    markerOptions.height = 0.25f;
    markerOptions.depth = 0.25f;
    const obol::SceneObjectId dragger =
        scene.addPrimitive(obol::Primitive::Cube,
                           material(0.2f, 0.7f, 1.0f),
                           transform(1.0f, 0.0f, 0.0f),
                           markerOptions);

    obol::Polyline axis;
    axis.lineWidth = 2.0f;
    axis.points = {{0.0f, 0.0f, 0.0f}, {2.8f, 0.0f, 0.0f}};
    scene.addPolyline(axis, material(0.8f, 0.8f, 0.8f));

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::OffscreenRenderer renderer(backend, target);
    renderer.setBackgroundColor({0.0f, 0.0f, 0.0f, 1.0f});

    const char *baseFilename = (argc > 1) ? argv[1] : "15.1.ConeRadius";
    char filename[512];
    const float positions[] = {0.5f, 1.0f, 1.5f, 2.0f, 2.5f};

    printf("=== Dragger Controls Cone Radius via Obol v2 State ===\n");
    for (int i = 0; i < 5; i++) {
        const float radius = positions[i];
        coneOptions.radius = radius;
        scene.setObjectPrimitiveOptions(cone, coneOptions);
        scene.setObjectTransform(dragger, transform(radius, 0.0f, 0.0f));

        printf("Frame %d: dragger.x = %.1f, cone radius = %.1f\n",
               i, radius, radius);
        snprintf(filename, sizeof(filename), "%s_frame%02d_radius%.1f.rgb",
                 baseFilename, i, radius);
        if (!renderScene(renderer, scene, filename)) return 1;
    }

    snprintf(filename, sizeof(filename), "%s.rgb", baseFilename);
    if (!renderScene(renderer, scene, filename)) return 1;

    printf("Rendered 5 frames showing dragger-driven primitive options.\n");
    return 0;
}
