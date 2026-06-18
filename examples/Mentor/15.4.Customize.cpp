/*
 * Headless version of Inventor Mentor example 15.4
 *
 * Original: customized Translate1Dragger part geometry.
 * Headless: custom v2 slider handles and active-state materials.
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cstdio>

namespace {

enum class ActiveAxis {
    None,
    X,
    Y,
    Z
};

struct SliderHandle {
    obol::SceneObjectId object = obol::InvalidSceneObjectId;
    obol::Transform base;
    obol::Vec3 axis = {1.0f, 0.0f, 0.0f};
};

struct SliderRig {
    SliderHandle x;
    SliderHandle y;
    SliderHandle z;
    obol::SceneObjectId text = obol::InvalidSceneObjectId;
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

obol::Transform rotate(float x, float y, float z,
                       float ax, float ay, float az, float angle)
{
    obol::Transform xf = transform(x, y, z);
    xf.rotationAxis = {ax, ay, az};
    xf.rotationRadians = angle;
    return xf;
}

void addRail(obol::Scene & scene, const obol::Transform & xf)
{
    obol::Polyline rail;
    rail.lineWidth = 2.0f;
    rail.points = {{-3.0f, 0.0f, 0.0f}, {3.0f, 0.0f, 0.0f}};
    scene.addPolyline(rail, material(0.65f, 0.65f, 0.65f), xf);
}

SliderHandle addHandle(obol::Scene & scene,
                       const obol::Transform & base,
                       const obol::Vec3 & axis)
{
    obol::PrimitiveOptions cube;
    cube.width = 3.0f;
    cube.height = 0.4f;
    cube.depth = 0.4f;

    SliderHandle handle;
    handle.base = base;
    handle.axis = axis;
    handle.object = scene.addPrimitive(obol::Primitive::Cube,
                                       material(1.0f, 1.0f, 1.0f),
                                       base,
                                       cube);
    return handle;
}

void addWireBox(obol::Scene & scene)
{
    obol::Polyline box;
    box.lineWidth = 1.0f;
    box.points = {
        {-8.0f, -4.0f, -8.0f}, { 8.0f, -4.0f, -8.0f},
        { 8.0f,  4.0f, -8.0f}, {-8.0f,  4.0f, -8.0f},
        {-8.0f, -4.0f, -8.0f}, {-8.0f, -4.0f,  8.0f},
        { 8.0f, -4.0f,  8.0f}, { 8.0f,  4.0f,  8.0f},
        {-8.0f,  4.0f,  8.0f}, {-8.0f, -4.0f,  8.0f},
        { 8.0f, -4.0f,  8.0f}, { 8.0f, -4.0f, -8.0f},
        { 8.0f,  4.0f, -8.0f}, { 8.0f,  4.0f,  8.0f},
        {-8.0f,  4.0f,  8.0f}, {-8.0f,  4.0f, -8.0f}
    };
    scene.addPolyline(box, material(1.0f, 0.0f, 1.0f));
}

void moveHandle(obol::Scene & scene,
                const SliderHandle & handle,
                float value,
                bool active)
{
    obol::Transform xf = handle.base;
    xf.translation = {
        handle.base.translation.x + handle.axis.x * value,
        handle.base.translation.y + handle.axis.y * value,
        handle.base.translation.z + handle.axis.z * value
    };
    scene.setObjectTransform(handle.object, xf);
    scene.setObjectMaterial(handle.object,
                            active ? material(1.0f, 1.0f, 0.0f)
                                   : material(1.0f, 1.0f, 1.0f));
}

void updateRig(obol::Scene & scene,
               const SliderRig & rig,
               float x,
               float y,
               float z,
               ActiveAxis active)
{
    moveHandle(scene, rig.x, x, active == ActiveAxis::X);
    moveHandle(scene, rig.y, y, active == ActiveAxis::Y);
    moveHandle(scene, rig.z, z, active == ActiveAxis::Z);
    scene.setObjectTransform(rig.text, transform(x, y, z));
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
    camera.position = {0.0f, 0.0f, 35.0f};
    camera.target = {0.0f, 0.0f, 0.0f};
    camera.verticalFieldOfViewRadians = 0.55f;
    scene.setCamera(camera);
    scene.addDirectionalLight(obol::DirectionalLight{});

    addWireBox(scene);

    const obol::Transform xBase = transform(0.0f, -4.0f, 8.0f);
    const obol::Transform yBase = rotate(-8.0f, 0.0f, 8.0f, 0.0f, 0.0f, 1.0f, 1.5708f);
    const obol::Transform zBase = rotate(-8.0f, -4.0f, 0.0f, 0.0f, 1.0f, 0.0f, -1.5708f);
    addRail(scene, xBase);
    addRail(scene, yBase);
    addRail(scene, zBase);

    SliderRig rig;
    rig.x = addHandle(scene, xBase, {1.0f, 0.0f, 0.0f});
    rig.y = addHandle(scene, yBase, {0.0f, 1.0f, 0.0f});
    rig.z = addHandle(scene, zBase, {0.0f, 0.0f, 1.0f});

    obol::Text3D text;
    text.text = "Slide Cubes\nTo\nMove Me";
    text.fontSize = 2.0f;
    text.justification = obol::TextJustification::Center;
    rig.text = scene.addText3D(text,
                               material(1.0f, 1.0f, 0.0f),
                               transform(0.0f, 0.0f, 0.0f));

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::OffscreenRenderer renderer(backend, target);
    renderer.setBackgroundColor({0.0f, 0.0f, 0.0f, 1.0f});

    const char *baseFilename = (argc > 1) ? argv[1] : "15.4.Customize";
    char filename[512];

    printf("Rendering Customized Slider Box with v2 custom handles...\n");

    updateRig(scene, rig, 0.0f, 0.0f, 0.0f, ActiveAxis::None);
    snprintf(filename, sizeof(filename), "%s_00_center.rgb", baseFilename);
    if (!renderScene(renderer, scene, filename)) return 1;
    snprintf(filename, sizeof(filename), "%s.rgb", baseFilename);
    if (!renderScene(renderer, scene, filename)) return 1;

    for (int i = 1; i <= 3; i++) {
        updateRig(scene, rig, i * 2.5f, 0.0f, 0.0f, ActiveAxis::X);
        snprintf(filename, sizeof(filename), "%s_%02d_x_custom.rgb", baseFilename, i);
        if (!renderScene(renderer, scene, filename)) return 1;
    }

    for (int i = 1; i <= 3; i++) {
        updateRig(scene, rig, 0.0f, i * 2.0f, 0.0f, ActiveAxis::Y);
        snprintf(filename, sizeof(filename), "%s_%02d_y_custom.rgb", baseFilename, i + 3);
        if (!renderScene(renderer, scene, filename)) return 1;
    }

    for (int i = 1; i <= 3; i++) {
        updateRig(scene, rig, 0.0f, 0.0f, i * 2.5f, ActiveAxis::Z);
        snprintf(filename, sizeof(filename), "%s_%02d_z_custom.rgb", baseFilename, i + 6);
        if (!renderScene(renderer, scene, filename)) return 1;
    }

    updateRig(scene, rig, 5.0f, 3.0f, 5.0f, ActiveAxis::None);
    snprintf(filename, sizeof(filename), "%s_10_combined.rgb", baseFilename);
    if (!renderScene(renderer, scene, filename)) return 1;

    printf("Done! Rendered 11 frames showing customized dragger geometry.\n");
    return 0;
}
