/*
 * Headless version of Inventor Mentor example 15.2
 *
 * Original: three Translate1Draggers feed a calculator engine for text motion.
 * Headless: application slider state composes a v2 transform directly.
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cstdio>

namespace {

struct SliderState {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
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

void addSlider(obol::Scene & scene,
               float x,
               float y,
               float z,
               const obol::Transform & orientation)
{
    obol::Polyline rail;
    rail.lineWidth = 2.0f;
    rail.points = {{-3.0f, 0.0f, 0.0f}, {3.0f, 0.0f, 0.0f}};
    scene.addPolyline(rail, material(0.75f, 0.75f, 0.75f), orientation);

    obol::PrimitiveOptions handle;
    handle.width = 0.5f;
    handle.height = 0.5f;
    handle.depth = 0.5f;
    obol::Transform handleTransform = orientation;
    handleTransform.translation = {
        orientation.translation.x + x,
        orientation.translation.y + y,
        orientation.translation.z + z
    };
    scene.addPrimitive(obol::Primitive::Cube,
                       material(0.2f, 0.7f, 1.0f),
                       handleTransform,
                       handle);
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
    addSlider(scene, 0.0f, 0.0f, 0.0f, transform(0.0f, -4.0f, 8.0f));
    addSlider(scene, 0.0f, 0.0f, 0.0f, rotate(-8.0f, 0.0f, 8.0f, 0.0f, 0.0f, 1.0f, 1.5708f));
    addSlider(scene, 0.0f, 0.0f, 0.0f, rotate(-8.0f, -4.0f, 0.0f, 0.0f, 1.0f, 0.0f, -1.5708f));

    obol::Text3D text;
    text.text = "Slide Arrows\nTo\nMove Me";
    text.fontSize = 2.0f;
    text.justification = obol::TextJustification::Center;
    const obol::SceneObjectId textId =
        scene.addText3D(text, material(1.0f, 1.0f, 0.0f), transform(0.0f, 0.0f, 0.0f));

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::OffscreenRenderer renderer(backend, target);
    renderer.setBackgroundColor({0.0f, 0.0f, 0.0f, 1.0f});

    const char *baseFilename = (argc > 1) ? argv[1] : "15.2.SliderBox";
    char filename[512];
    SliderState state;

    printf("Rendering Slider Box with app-owned slider positions [Obol v2]...\n");

    scene.setObjectTransform(textId, transform(state.x, state.y, state.z));
    snprintf(filename, sizeof(filename), "%s_00_center.rgb", baseFilename);
    if (!renderScene(renderer, scene, filename)) return 1;
    snprintf(filename, sizeof(filename), "%s.rgb", baseFilename);
    if (!renderScene(renderer, scene, filename)) return 1;

    for (int i = 1; i <= 4; i++) {
        state = {i * 2.0f, 0.0f, 0.0f};
        scene.setObjectTransform(textId, transform(state.x, state.y, state.z));
        snprintf(filename, sizeof(filename), "%s_%02d_x_pos.rgb", baseFilename, i);
        if (!renderScene(renderer, scene, filename)) return 1;
    }

    for (int i = 1; i <= 4; i++) {
        state = {0.0f, i * 1.5f, 0.0f};
        scene.setObjectTransform(textId, transform(state.x, state.y, state.z));
        snprintf(filename, sizeof(filename), "%s_%02d_y_pos.rgb", baseFilename, i + 4);
        if (!renderScene(renderer, scene, filename)) return 1;
    }

    for (int i = 1; i <= 4; i++) {
        state = {0.0f, 0.0f, i * 2.0f};
        scene.setObjectTransform(textId, transform(state.x, state.y, state.z));
        snprintf(filename, sizeof(filename), "%s_%02d_z_pos.rgb", baseFilename, i + 8);
        if (!renderScene(renderer, scene, filename)) return 1;
    }

    state = {4.0f, 2.0f, 4.0f};
    scene.setObjectTransform(textId, transform(state.x, state.y, state.z));
    snprintf(filename, sizeof(filename), "%s_13_combined.rgb", baseFilename);
    if (!renderScene(renderer, scene, filename)) return 1;

    printf("Done! Rendered 14 frames showing slider-controlled text movement.\n");
    return 0;
}
