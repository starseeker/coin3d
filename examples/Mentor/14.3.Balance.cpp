/*
 * Headless version of Inventor Mentor example 14.3
 *
 * Original: ShapeKit hierarchy plus keyboard event callback tips a balance.
 * Headless: application-owned hierarchy and key handling update v2 groups.
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cstdio>

namespace {

struct BalanceKit {
    obol::SceneGroupId beamGroup = obol::InvalidSceneGroupId;
    obol::SceneGroupId stringLeftGroup = obol::InvalidSceneGroupId;
    obol::SceneGroupId stringRightGroup = obol::InvalidSceneGroupId;
    float beamAngle = 0.0f;
};

obol::Material material(float r, float g, float b)
{
    obol::Material result;
    result.baseColor = {r, g, b, 1.0f};
    result.specular = {0.2f, 0.2f, 0.2f, 1.0f};
    result.shininess = 0.25f;
    return result;
}

obol::Transform transform(float x, float y, float z)
{
    obol::Transform xf;
    xf.translation = {x, y, z};
    return xf;
}

obol::Transform rotateZ(float x, float y, float z, float angle)
{
    obol::Transform xf = transform(x, y, z);
    xf.rotationAxis = {0.0f, 0.0f, 1.0f};
    xf.rotationRadians = angle;
    return xf;
}

void addCylinder(obol::Scene & scene,
                 float radius,
                 float height,
                 const obol::Material & mat,
                 const obol::Transform & xf,
                 obol::SceneGroupId parent)
{
    obol::PrimitiveOptions options;
    options.radius = radius;
    options.height = height;
    scene.addPrimitive(obol::Primitive::Cylinder, mat, xf, options, parent);
}

BalanceKit addBalance(obol::Scene & scene)
{
    BalanceKit kit;

    obol::PrimitiveOptions supportOptions;
    supportOptions.radius = 0.3f;
    supportOptions.height = 3.0f;
    scene.addPrimitive(obol::Primitive::Cone,
                       material(0.65f, 0.65f, 0.7f),
                       transform(0.0f, 0.0f, 0.0f),
                       supportOptions);

    kit.beamGroup = scene.addGroup(transform(0.0f, 1.5f, 0.0f));
    obol::PrimitiveOptions beamOptions;
    beamOptions.width = 3.0f;
    beamOptions.height = 0.2f;
    beamOptions.depth = 0.2f;
    scene.addPrimitive(obol::Primitive::Cube,
                       material(0.75f, 0.45f, 0.18f),
                       transform(0.0f, 0.0f, 0.0f),
                       beamOptions,
                       kit.beamGroup);

    kit.stringLeftGroup = scene.addGroup(transform(-1.5f, 0.0f, 0.0f), kit.beamGroup);
    kit.stringRightGroup = scene.addGroup(transform(1.5f, 0.0f, 0.0f), kit.beamGroup);

    const obol::Material stringMaterial = material(0.9f, 0.9f, 0.85f);
    addCylinder(scene,
                0.05f,
                2.0f,
                stringMaterial,
                transform(0.0f, -1.0f, 0.0f),
                kit.stringLeftGroup);
    addCylinder(scene,
                0.05f,
                2.0f,
                stringMaterial,
                transform(0.0f, -1.0f, 0.0f),
                kit.stringRightGroup);

    const obol::Material trayMaterial = material(0.35f, 0.55f, 0.85f);
    addCylinder(scene,
                0.75f,
                0.1f,
                trayMaterial,
                transform(0.0f, -2.0f, 0.0f),
                kit.stringLeftGroup);
    addCylinder(scene,
                0.75f,
                0.1f,
                trayMaterial,
                transform(0.0f, -2.0f, 0.0f),
                kit.stringRightGroup);

    obol::Text2D label;
    label.text = "Press Left or Right Arrow Key";
    label.fontName = "Helvetica";
    label.fontSize = 16.0f;
    label.justification = obol::TextJustification::Center;
    scene.addText2D(label,
                    material(0.9f, 0.9f, 0.9f),
                    transform(0.0f, -2.5f, 0.0f));

    return kit;
}

void tipBalance(obol::Scene & scene, BalanceKit & kit, float delta)
{
    kit.beamAngle += delta;
    scene.setGroupTransform(kit.beamGroup,
                            rotateZ(0.0f, 1.5f, 0.0f, kit.beamAngle));
    scene.setGroupTransform(kit.stringLeftGroup,
                            rotateZ(-1.5f, 0.0f, 0.0f, -kit.beamAngle));
    scene.setGroupTransform(kit.stringRightGroup,
                            rotateZ(1.5f, 0.0f, 0.0f, -kit.beamAngle));
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

} // namespace

int main(int argc, char **argv)
{
    initCoinHeadless();

    obol::Scene scene;
    obol::PerspectiveCamera camera;
    camera.position = {0.0f, 1.0f, 9.0f};
    camera.target = {0.0f, 0.2f, 0.0f};
    camera.verticalFieldOfViewRadians = 0.65f;
    scene.setCamera(camera);
    scene.addDirectionalLight(obol::DirectionalLight{});

    BalanceKit kit = addBalance(scene);

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::Renderer renderer(backend);

    const char *baseFilename = (argc > 1) ? argv[1] : "14.3.Balance";
    char filename[512];

    printf("Rendering Balance Scale with keyboard event simulation [Obol v2]...\n");

    snprintf(filename, sizeof(filename), "%s_00_initial.rgb", baseFilename);
    if (!renderScene(renderer, scene, target, filename)) return 1;
    snprintf(filename, sizeof(filename), "%s.rgb", baseFilename);
    if (!renderScene(renderer, scene, target, filename)) return 1;

    for (int i = 0; i < 5; i++) {
        tipBalance(scene, kit, -0.1f);
        snprintf(filename, sizeof(filename), "%s_%02d_right.rgb", baseFilename, i + 1);
        if (!renderScene(renderer, scene, target, filename)) return 1;
    }

    for (int i = 0; i < 10; i++) {
        tipBalance(scene, kit, 0.1f);
        snprintf(filename, sizeof(filename), "%s_%02d_left.rgb", baseFilename, i + 6);
        if (!renderScene(renderer, scene, target, filename)) return 1;
    }

    printf("Done! Rendered 16 frames showing balance tipping.\n");
    return 0;
}
