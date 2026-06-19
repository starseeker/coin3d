/*
 * Headless version of Inventor Mentor example 16.2
 *
 * Original: material editor invokes callbacks that copy material node fields.
 * Headless: toolkit-owned material editor callbacks update v2 object material.
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

using MaterialChangedCB = void (*)(void *, const obol::Material &);

struct MaterialCallback {
    MaterialChangedCB callback = nullptr;
    void * userData = nullptr;
};

class MockMaterialEditor {
public:
    void addMaterialChangedCallback(MaterialChangedCB callback, void * userData)
    {
        callbacks_.push_back({callback, userData});
    }

    void setMaterial(const obol::Material & material)
    {
        material_ = material;
        for (const MaterialCallback & cb : callbacks_) {
            if (cb.callback) cb.callback(cb.userData, material_);
        }
    }

private:
    obol::Material material_;
    std::vector<MaterialCallback> callbacks_;
};

struct MaterialTarget {
    obol::Scene * scene = nullptr;
    std::vector<obol::SceneObjectId> objects;
};

obol::Material material(float r, float g, float b, float shininess)
{
    obol::Material result;
    result.baseColor = {r, g, b, 1.0f};
    result.specular = {0.55f, 0.55f, 0.55f, 1.0f};
    result.shininess = shininess;
    return result;
}

std::vector<obol::SceneObjectId> addDogDish(obol::Scene & scene, const obol::Material & mat)
{
    char path[512];
    const char *envDataDir = getenv("OBOL_DATA_DIR");
    if (!envDataDir) envDataDir = getenv("IVEXAMPLES_DATA_DIR");
    const char *candidateDataDirs[] = {
        envDataDir,
        "examples/Mentor/data",
        "../../data"
    };

    for (const char *dataDir : candidateDataDirs) {
        if (!dataDir) continue;
        snprintf(path, sizeof(path), "%s/dogDish.iv", dataDir);
        const obol::SceneObjectId dish =
            obol::SceneIO::addInventorFile(path,
                                           scene,
                                           obol::Transform{},
                                           obol::RootSceneGroupId,
                                           getCoinHeadlessContextManager());
        if (dish != obol::InvalidSceneObjectId) {
            scene.setObjectMaterial(dish, mat);
            printf("Loaded dog dish geometry from %s\n", path);
            return {dish};
        }
    }

    fprintf(stderr, "Error: Could not open dogDish.iv\n");
    return {};
}

void applyMaterialCallback(void * userData, const obol::Material & newMaterial)
{
    MaterialTarget * target = static_cast<MaterialTarget *>(userData);
    if (!target || !target->scene) return;

    printf("Material editor callback invoked - applying material by v2 object ID\n");
    for (obol::SceneObjectId object : target->objects) {
        target->scene->setObjectMaterial(object, newMaterial);
    }
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
    printf("=== Mentor Example 16.2: Material Editor Callback ===\n");
    printf("This demonstrates toolkit-agnostic material editor callbacks over Obol v2 IDs\n\n");

    initCoinHeadless();

    obol::Scene scene;
    obol::PerspectiveCamera camera;
    camera.position = {0.212482f, -0.881014f, 2.5f};
    camera.target = {0.212482f, -0.881014f, -2.5f};
    camera.up = {0.0f, 1.0f, 0.0f};
    camera.verticalFieldOfViewRadians = 0.78539816339f;
    scene.setCamera(camera);
    scene.addDirectionalLight(obol::DirectionalLight{});

    const obol::Material defaultMaterial = material(0.8f, 0.8f, 0.8f, 0.2f);
    const std::vector<obol::SceneObjectId> dish = addDogDish(scene, defaultMaterial);
    if (dish.empty()) return 1;

    MaterialTarget targetState{&scene, dish};
    MockMaterialEditor editor;
    editor.addMaterialChangedCallback(applyMaterialCallback, &targetState);

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::OffscreenRenderer renderer(backend, target);
    renderer.setBackgroundColor({0.0f, 0.0f, 0.0f, 1.0f});

    const char *baseFilename = (argc > 1) ? argv[1] : "16.2.Callback";
    char filename[512];

    printf("--- State 1: Default material ---\n");
    snprintf(filename, sizeof(filename), "%s_default.rgb", baseFilename);
    if (!renderScene(renderer, scene, filename)) return 1;

    printf("--- State 2: User changes to red material ---\n");
    editor.setMaterial(material(1.0f, 0.0f, 0.0f, 0.5f));
    snprintf(filename, sizeof(filename), "%s_red.rgb", baseFilename);
    if (!renderScene(renderer, scene, filename)) return 1;

    printf("--- State 3: User changes to blue material ---\n");
    editor.setMaterial(material(0.0f, 0.3f, 1.0f, 0.8f));
    snprintf(filename, sizeof(filename), "%s_blue.rgb", baseFilename);
    if (!renderScene(renderer, scene, filename)) return 1;
    snprintf(filename, sizeof(filename), "%s.rgb", baseFilename);
    if (!renderScene(renderer, scene, filename)) return 1;

    printf("--- State 4: User changes to gold material ---\n");
    editor.setMaterial(material(1.0f, 0.84f, 0.0f, 0.9f));
    snprintf(filename, sizeof(filename), "%s_gold.rgb", baseFilename);
    if (!renderScene(renderer, scene, filename)) return 1;

    printf("Generated 4 images showing material editor callbacks through Obol v2.\n");
    return 0;
}
