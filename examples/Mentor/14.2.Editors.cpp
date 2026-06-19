/*
 * Headless version of Inventor Mentor example 14.2
 *
 * Original: SceneKit/WrapperKit plus material and directional light editors.
 * Headless: application-owned editor state rebuilds a v2 scene for each state.
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cstdio>
#include <vector>

namespace {

struct EditorState {
    obol::Material deskMaterial;
    obol::DirectionalLight light;
    bool lightOn = true;
};

obol::Material makeMaterial(float r, float g, float b)
{
    obol::Material material;
    material.baseColor = {r, g, b, 1.0f};
    material.specular = {0.25f, 0.25f, 0.25f, 1.0f};
    material.shininess = 0.3f;
    return material;
}

obol::Transform transform(float x, float y, float z,
                          float sx = 1.0f,
                          float sy = 1.0f,
                          float sz = 1.0f)
{
    obol::Transform xf;
    xf.translation = {x, y, z};
    xf.scale = {sx, sy, sz};
    return xf;
}

void addDesk(obol::Scene & scene, const obol::Material & material)
{
    obol::Material shadowMaterial;
    shadowMaterial.baseColor = {0.02f, 0.012f, 0.008f, 1.0f};

    obol::PrimitiveOptions top;
    top.width = 5.8f;
    top.height = 0.22f;
    top.depth = 2.25f;
    scene.addPrimitive(obol::Primitive::Cube,
                       material,
                       transform(0.0f, 0.95f, 0.0f),
                       top);

    obol::PrimitiveOptions apron;
    apron.width = 5.35f;
    apron.height = 0.38f;
    apron.depth = 0.16f;
    scene.addPrimitive(obol::Primitive::Cube,
                       material,
                       transform(0.0f, 0.68f, 1.02f),
                       apron);

    obol::PrimitiveOptions sideApron;
    sideApron.width = 0.18f;
    sideApron.height = 0.34f;
    sideApron.depth = 1.85f;
    scene.addPrimitive(obol::Primitive::Cube,
                       material,
                       transform(-2.55f, 0.66f, 0.0f),
                       sideApron);
    scene.addPrimitive(obol::Primitive::Cube,
                       material,
                       transform(2.55f, 0.66f, 0.0f),
                       sideApron);

    obol::PrimitiveOptions leg;
    leg.width = 0.34f;
    leg.height = 0.95f;
    leg.depth = 0.34f;
    const float x = 2.35f;
    const float z = 0.78f;
    scene.addPrimitive(obol::Primitive::Cube, material, transform(-x, 0.24f, -z), leg);
    scene.addPrimitive(obol::Primitive::Cube, material, transform( x, 0.24f, -z), leg);
    scene.addPrimitive(obol::Primitive::Cube, material, transform(-x, 0.24f,  z), leg);
    scene.addPrimitive(obol::Primitive::Cube, material, transform( x, 0.24f,  z), leg);

    obol::PrimitiveOptions foot;
    foot.width = 0.64f;
    foot.height = 0.16f;
    foot.depth = 0.52f;
    scene.addPrimitive(obol::Primitive::Cube, material, transform(-x, -0.30f, -z), foot);
    scene.addPrimitive(obol::Primitive::Cube, material, transform( x, -0.30f, -z), foot);
    scene.addPrimitive(obol::Primitive::Cube, material, transform(-x, -0.30f,  z), foot);
    scene.addPrimitive(obol::Primitive::Cube, material, transform( x, -0.30f,  z), foot);

    obol::PrimitiveOptions recess;
    recess.width = 1.65f;
    recess.height = 0.48f;
    recess.depth = 0.18f;
    scene.addPrimitive(obol::Primitive::Cube,
                       shadowMaterial,
                       transform(-0.25f, 0.46f, 1.12f),
                       recess);

    obol::PrimitiveOptions drawer;
    drawer.width = 1.35f;
    drawer.height = 0.42f;
    drawer.depth = 0.20f;
    scene.addPrimitive(obol::Primitive::Cube,
                       material,
                       transform(-0.25f, 0.43f, 1.23f),
                       drawer);
}

obol::Scene makeScene(const EditorState & state)
{
    obol::Scene scene;

    obol::PerspectiveCamera camera;
    camera.position = {8.8f, 3.1f, 14.5f};
    camera.target = {0.0f, 0.45f, 0.0f};
    camera.verticalFieldOfViewRadians = 0.48f;
    scene.setCamera(camera);

    obol::DirectionalLight light = state.light;
    if (!state.lightOn) {
        light.intensity = 0.0f;
    }
    scene.addDirectionalLight(light);
    addDesk(scene, state.deskMaterial);
    return scene;
}

bool renderState(obol::Renderer & renderer,
                 const EditorState & state,
                 const obol::RenderTarget & target,
                 const char * filename)
{
    obol::Scene scene = makeScene(state);
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
    printf("=== Mentor Example 14.2: NodeKit Editors ===\n");
    printf("This demonstrates toolkit-agnostic editor patterns over Obol v2 IDs/state\n\n");

    initCoinHeadless();

    EditorState state;
    state.deskMaterial = makeMaterial(0.8f, 0.3f, 0.1f);
    state.light.direction = {0.0f, -0.7f, -1.0f};
    state.light.color = {1.0f, 1.0f, 1.0f, 1.0f};
    state.light.intensity = 1.0f;

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::Renderer renderer(backend);

    const char *baseFilename = (argc > 1) ? argv[1] : "14.2.Editors";
    char filename[512];

    printf("--- State 1: Initial desk with default lighting ---\n");
    snprintf(filename, sizeof(filename), "%s_initial.rgb", baseFilename);
    if (!renderState(renderer, state, target, filename)) return 1;
    snprintf(filename, sizeof(filename), "%s.rgb", baseFilename);
    if (!renderState(renderer, state, target, filename)) return 1;

    printf("--- State 2: User changes desk to darker wood via material editor ---\n");
    state.deskMaterial = makeMaterial(0.5f, 0.25f, 0.1f);
    state.deskMaterial.specular = {0.3f, 0.3f, 0.3f, 1.0f};
    state.deskMaterial.shininess = 0.3f;
    snprintf(filename, sizeof(filename), "%s_dark_wood.rgb", baseFilename);
    if (!renderState(renderer, state, target, filename)) return 1;

    printf("--- State 3: User changes light direction via light editor ---\n");
    state.light.direction = {1.0f, -1.0f, -1.0f};
    snprintf(filename, sizeof(filename), "%s_light_direction.rgb", baseFilename);
    if (!renderState(renderer, state, target, filename)) return 1;

    printf("--- State 4: User changes light color and intensity ---\n");
    state.light.color = {1.0f, 1.0f, 0.8f, 1.0f};
    state.light.intensity = 1.2f;
    snprintf(filename, sizeof(filename), "%s_warm_bright_light.rgb", baseFilename);
    if (!renderState(renderer, state, target, filename)) return 1;

    printf("--- State 5: User changes desk to lighter finish ---\n");
    state.deskMaterial = makeMaterial(0.9f, 0.7f, 0.4f);
    state.deskMaterial.shininess = 0.6f;
    snprintf(filename, sizeof(filename), "%s_light_finish.rgb", baseFilename);
    if (!renderState(renderer, state, target, filename)) return 1;

    printf("--- State 6: User turns light off ---\n");
    state.lightOn = false;
    snprintf(filename, sizeof(filename), "%s_light_off.rgb", baseFilename);
    if (!renderState(renderer, state, target, filename)) return 1;

    printf("--- State 7: User turns light back on ---\n");
    state.lightOn = true;
    snprintf(filename, sizeof(filename), "%s_light_on.rgb", baseFilename);
    if (!renderState(renderer, state, target, filename)) return 1;

    printf("\nGenerated 7 images showing editor state applied through Obol v2 scenes.\n");
    printf("Material and light widgets are represented as toolkit-owned state that can target any backend.\n");
    return 0;
}
