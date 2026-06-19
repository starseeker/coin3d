/*
 * Headless version of Inventor Mentor example 15.1
 *
 * Original: Translate1Dragger drives a cone radius through an engine.
 * Headless: TransformDragger axis translation updates v2 primitive options by ID.
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

obol::Transform transform(float x,
                          float y,
                          float z,
                          float sx,
                          float sy,
                          float sz,
                          float radians = 0.0f)
{
    obol::Transform xf;
    xf.translation = {x, y, z};
    xf.scale = {sx, sy, sz};
    xf.rotationAxis = {0.0f, 0.0f, 1.0f};
    xf.rotationRadians = radians;
    return xf;
}

obol::PerspectiveCamera viewAllCamera(const obol::Scene & scene)
{
    obol::ViewAllRequest request;
    request.viewportWidth = DEFAULT_WIDTH;
    request.viewportHeight = DEFAULT_HEIGHT;
    return obol::CameraFraming::viewAllPerspective(scene, request);
}

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

void addTranslate1DraggerProxy(obol::Scene & scene, obol::SceneGroupId group)
{
    constexpr float pi = 3.14159265358979323846f;
    obol::Material white = material(0.92f, 0.92f, 0.88f);

    obol::PrimitiveOptions shaft;
    shaft.radius = 0.055f;
    shaft.height = 1.65f;
    scene.addPrimitive(obol::Primitive::Cylinder,
                       white,
                       transform(0.0f, 0.0f, 0.0f,
                                 1.0f, 1.0f, 1.0f,
                                 pi / 2.0f),
                       shaft,
                       group);

    obol::PrimitiveOptions head;
    head.radius = 0.22f;
    head.height = 0.42f;
    scene.addPrimitive(obol::Primitive::Cone,
                       white,
                       transform(0.98f, 0.0f, 0.0f,
                                 1.0f, 1.0f, 1.0f,
                                 -pi / 2.0f),
                       head,
                       group);
    scene.addPrimitive(obol::Primitive::Cone,
                       white,
                       transform(-0.98f, 0.0f, 0.0f,
                                 1.0f, 1.0f, 1.0f,
                                 pi / 2.0f),
                       head,
                       group);
}

} // namespace

int main(int argc, char **argv)
{
    initCoinHeadless();

    obol::Scene scene;
    scene.addDirectionalLight(obol::DirectionalLight{});

    obol::PrimitiveOptions coneOptions;
    coneOptions.radius = 1.0f;
    const obol::SceneObjectId cone =
        scene.addPrimitive(obol::Primitive::Cone,
                           material(0.8f, 0.8f, 0.8f),
                           transform(0.0f, 3.0f, 0.0f),
                           coneOptions);

    const obol::SceneGroupId dragger = scene.addGroup(transform(1.0f, 0.0f, 0.0f));
    addTranslate1DraggerProxy(scene, dragger);
    scene.setCamera(viewAllCamera(scene));

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::Renderer renderer(backend);

    const char *baseFilename = (argc > 1) ? argv[1] : "15.1.ConeRadius";
    char filename[512];
    const float positions[] = {0.5f, 1.0f, 1.5f, 2.0f, 2.5f};

    printf("=== Dragger Controls Cone Radius via Obol v2 State ===\n");
    for (int i = 0; i < 5; i++) {
        const float radius = positions[i];
        obol::AxisDragRequest dragRequest;
        dragRequest.startTransform = transform(0.0f, 0.0f, 0.0f);
        dragRequest.axis = {1.0f, 0.0f, 0.0f};
        dragRequest.distance = radius;
        const obol::AxisDragResult drag =
            obol::TransformDragger::translateOnAxis(dragRequest);
        if (!drag.valid) return 1;

        coneOptions.radius = radius;
        scene.setObjectPrimitiveOptions(cone, coneOptions);
        scene.setGroupTransform(dragger, drag.transform);

        printf("Frame %d: dragger.x = %.1f, cone radius = %.1f\n",
               i, radius, radius);
        snprintf(filename, sizeof(filename), "%s_frame%02d_radius%.1f.rgb",
                 baseFilename, i, radius);
        if (!renderScene(renderer, scene, target, filename)) return 1;
    }

    snprintf(filename, sizeof(filename), "%s.rgb", baseFilename);
    if (!renderScene(renderer, scene, target, filename)) return 1;

    printf("Rendered 5 frames showing dragger-driven primitive options.\n");
    return 0;
}
