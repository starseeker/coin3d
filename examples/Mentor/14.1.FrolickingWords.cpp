/*
 * Headless version of Inventor Mentor example 14.1
 *
 * Original: ShapeKits plus elapsed time and calculator engines animate 3D text.
 * Headless: application-owned kit state drives v2 text transforms/materials.
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cmath>
#include <cstdio>
#include <string>

namespace {

struct WordKit {
    obol::SceneObjectId text = obol::InvalidSceneObjectId;
    obol::Transform transform;
    obol::Material material;
    float phase = 0.0f;
};

obol::Material material(float r, float g, float b)
{
    obol::Material result;
    result.baseColor = {r, g, b, 1.0f};
    result.specular = {0.35f, 0.35f, 0.35f, 1.0f};
    result.shininess = 0.45f;
    return result;
}

WordKit addWord(obol::Scene & scene, const char * string, float phase)
{
    obol::Text3D text;
    text.text = string;
    text.fontSize = 2.0f;
    text.parts = static_cast<uint32_t>(obol::Text3DParts::All);

    WordKit kit;
    kit.phase = phase;
    kit.material = material(0.8f, 0.8f, 0.2f);
    kit.text = scene.addText3D(text, kit.material, kit.transform);
    return kit;
}

void evaluateWord(obol::Scene & scene, WordKit & kit, float time)
{
    const float ta = std::cos(2.0f * time + kit.phase);
    const float tb = std::sin(2.0f * time + kit.phase);

    kit.transform.translation = {
        3.0f * ta * ta * ta,
        3.0f * tb * tb * tb,
        0.0f
    };
    kit.transform.scale = {
        std::fabs(ta) + 0.1f,
        std::fabs(tb) * 0.5f + 0.1f,
        1.0f
    };
    kit.material.baseColor = {
        std::fabs(ta),
        std::fabs(tb),
        0.5f,
        1.0f
    };

    scene.setObjectTransform(kit.text, kit.transform);
    scene.setObjectMaterial(kit.text, kit.material);
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
    camera.position = {0.0f, 0.0f, 15.0f};
    camera.target = {0.0f, 0.0f, 0.0f};
    camera.verticalFieldOfViewRadians = 0.6f;
    scene.setCamera(camera);
    scene.addDirectionalLight(obol::DirectionalLight{});

    WordKit happy = addWord(scene, "HAPPY", 0.0f);
    WordKit nice = addWord(scene, "NICE", 2.0f);

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::OffscreenRenderer renderer(backend, target);
    renderer.setBackgroundColor({0.0f, 0.0f, 0.0f, 1.0f});

    const char *baseFilename = (argc > 1) ? argv[1] : "14.1.FrolickingWords";
    char filename[512];

    printf("Rendering Frolicking Words animation sequence [Obol v2]...\n");
    for (int frame = 0; frame < 20; frame++) {
        const float time = static_cast<float>(frame) * 0.4f;
        evaluateWord(scene, happy, time);
        evaluateWord(scene, nice, time);

        snprintf(filename, sizeof(filename), "%s_frame%02d.rgb", baseFilename, frame);
        if (!renderScene(renderer, scene, filename)) return 1;
    }

    const std::string primaryFilename = std::string(baseFilename) + ".rgb";
    if (!renderScene(renderer, scene, primaryFilename.c_str())) return 1;

    printf("Done! Rendered 20 animation frames.\n");
    return 0;
}
