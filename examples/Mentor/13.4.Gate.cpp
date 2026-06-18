/*
 * Headless version of Inventor Mentor example 13.4
 *
 * Original: Gate - toggles gate to enable/disable motion
 * Headless: app-owned gate state controls whether v2 transforms update
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cstdio>

namespace {

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

obol::Transform transform(float x, float y, float z,
                          float sx = 1.0f,
                          float sy = 1.0f,
                          float sz = 1.0f)
{
    obol::Transform result;
    result.translation = {x, y, z};
    result.scale = {sx, sy, sz};
    return result;
}

obol::Transform hidden()
{
    return transform(1000.0f, 1000.0f, 1000.0f, 0.001f, 0.001f, 0.001f);
}

obol::Polyline trackLine()
{
    obol::Polyline line;
    line.lineWidth = 3.0f;
    line.points = {
        {-0.25f, -0.8f, 0.0f}, {2.25f, -0.8f, 0.0f}
    };
    return line;
}

obol::Text3D label(const char * text, float size)
{
    obol::Text3D result;
    result.text = text;
    result.fontSize = size;
    result.justification = obol::TextJustification::Center;
    return result;
}

} // namespace

int main(int argc, char **argv)
{
    initCoinHeadless();

    obol::Scene scene;
    obol::PerspectiveCamera camera;
    camera.position = {1.0f, -5.5f, 7.0f};
    camera.target = {1.0f, 0.0f, 0.0f};
    camera.verticalFieldOfViewRadians = 0.7f;
    scene.setCamera(camera);
    scene.addDirectionalLight(obol::DirectionalLight{});

    obol::Transform objectTransform;
    objectTransform.scale = {0.45f, 0.45f, 0.45f};
    const obol::SceneObjectId object =
        scene.addPrimitive(obol::Primitive::Cube,
                           material(0.8f, 0.3f, 0.1f),
                           objectTransform);

    scene.addPolyline(trackLine(), material(0.45f, 0.45f, 0.45f));

    const obol::SceneObjectId closedGate =
        scene.addPrimitive(obol::Primitive::Cube,
                           material(1.0f, 0.05f, 0.02f),
                           transform(-0.25f, 0.0f, 0.0f, 0.08f, 1.2f, 0.08f));
    const obol::SceneObjectId openGate =
        scene.addPrimitive(obol::Primitive::Cube,
                           material(0.0f, 0.8f, 0.18f),
                           hidden());
    const obol::SceneObjectId closedLabel =
        scene.addText3D(label("GATE CLOSED", 0.28f),
                        material(1.0f, 0.05f, 0.02f),
                        transform(1.0f, 1.65f, 0.0f));
    const obol::SceneObjectId openLabel =
        scene.addText3D(label("GATE OPEN", 0.28f),
                        material(0.0f, 0.8f, 0.18f),
                        hidden());

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::OffscreenRenderer renderer(backend, target);
    renderer.setBackgroundColor({0.0f, 0.0f, 0.0f, 1.0f});

    const char *baseFilename = (argc > 1) ? argv[1] : "13.4.Gate";
    char filename[256];

    printf("=== Gate DISABLED ===\n");
    for (int i = 0; i < 5; ++i) {
        const float timeValue = i * 0.5f;
        objectTransform.translation = {0.0f, 0.0f, 0.0f};
        scene.setObjectTransform(object, objectTransform);
        scene.setObjectTransform(closedGate, transform(-0.25f, 0.0f, 0.0f,
                                                       0.08f, 1.2f, 0.08f));
        scene.setObjectTransform(openGate, hidden());
        scene.setObjectTransform(closedLabel, transform(1.0f, 1.2f, 0.0f));
        scene.setObjectTransform(openLabel, hidden());
        printf("Time %.1f: Gate disabled, X = %.2f\n", timeValue, objectTransform.translation.x);
        snprintf(filename, sizeof(filename), "%s_disabled_%02d.rgb", baseFilename, i);
        if (!renderScene(renderer, scene, filename)) return 1;
    }

    printf("\n=== Gate ENABLED ===\n");
    for (int i = 0; i < 5; ++i) {
        const float timeValue = i * 0.5f;
        objectTransform.translation = {timeValue, 0.0f, 0.0f};
        scene.setObjectTransform(object, objectTransform);
        scene.setObjectTransform(closedGate, hidden());
        scene.setObjectTransform(openGate, transform(-0.25f, 0.65f, 0.0f,
                                                     0.08f, 0.35f, 0.08f));
        scene.setObjectTransform(closedLabel, hidden());
        scene.setObjectTransform(openLabel, transform(1.0f, 1.2f, 0.0f));
        printf("Time %.1f: Gate enabled, X = %.2f\n", timeValue, objectTransform.translation.x);
        snprintf(filename, sizeof(filename), "%s_enabled_%02d.rgb", baseFilename, i);
        if (!renderScene(renderer, scene, filename)) return 1;
    }

    return 0;
}
