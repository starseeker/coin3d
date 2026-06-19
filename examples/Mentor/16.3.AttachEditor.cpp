/*
 * Headless version of Inventor Mentor example 16.3
 *
 * Original: material editor attaches bidirectionally to a material node.
 * Headless: editor attaches to v2 material state for one or more object IDs.
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

struct MaterialAttachment {
    obol::Scene * scene = nullptr;
    std::vector<obol::SceneObjectId> objects;
    obol::Material material;
};

class AttachedMaterialEditor {
public:
    void attach(MaterialAttachment * attachment)
    {
        attachment_ = attachment;
        if (attachment_) material_ = attachment_->material;
    }

    void setMaterial(const obol::Material & material)
    {
        material_ = material;
        if (!attachment_ || !attachment_->scene) return;
        attachment_->material = material_;
        for (obol::SceneObjectId object : attachment_->objects) {
            attachment_->scene->setObjectMaterial(object, material_);
        }
    }

    void syncFromAttachment()
    {
        if (attachment_) material_ = attachment_->material;
    }

    const obol::Material & material() const { return material_; }

private:
    MaterialAttachment * attachment_ = nullptr;
    obol::Material material_;
};

obol::Material material(float r, float g, float b, float shininess)
{
    obol::Material result;
    result.baseColor = {r, g, b, 1.0f};
    result.specular = {0.55f, 0.55f, 0.55f, 1.0f};
    result.shininess = shininess;
    return result;
}

std::vector<obol::SceneObjectId> addDogDish(obol::Scene & scene,
                                            const obol::Material & mat)
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

void applyProgrammaticMaterial(MaterialAttachment & attachment,
                               const obol::Material & material)
{
    attachment.material = material;
    if (!attachment.scene) return;
    for (obol::SceneObjectId object : attachment.objects) {
        attachment.scene->setObjectMaterial(object, material);
    }
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
    printf("=== Mentor Example 16.3: Attach Material Editor ===\n");
    printf("This demonstrates toolkit-agnostic material editor attachment over Obol v2 IDs\n\n");

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
    MaterialAttachment attachment;
    attachment.scene = &scene;
    attachment.material = defaultMaterial;
    attachment.objects = addDogDish(scene, defaultMaterial);
    if (attachment.objects.empty()) return 1;

    AttachedMaterialEditor editor;

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::Renderer renderer(backend);

    const char *baseFilename = (argc > 1) ? argv[1] : "16.3.AttachEditor";
    char filename[512];

    printf("--- State 1: Default material before attach ---\n");
    snprintf(filename, sizeof(filename), "%s_default.rgb", baseFilename);
    if (!renderScene(renderer, scene, target, filename)) return 1;

    printf("--- Attaching editor to material state ---\n");
    editor.attach(&attachment);
    printf("Editor synchronized with material color %.2f %.2f %.2f\n",
           editor.material().baseColor.r,
           editor.material().baseColor.g,
           editor.material().baseColor.b);

    printf("--- State 2: User edits to red via attached editor ---\n");
    editor.setMaterial(material(1.0f, 0.0f, 0.0f, 0.5f));
    snprintf(filename, sizeof(filename), "%s_red.rgb", baseFilename);
    if (!renderScene(renderer, scene, target, filename)) return 1;

    printf("--- State 3: User edits to blue via attached editor ---\n");
    editor.setMaterial(material(0.0f, 0.3f, 1.0f, 0.8f));
    snprintf(filename, sizeof(filename), "%s_blue.rgb", baseFilename);
    if (!renderScene(renderer, scene, target, filename)) return 1;
    snprintf(filename, sizeof(filename), "%s.rgb", baseFilename);
    if (!renderScene(renderer, scene, target, filename)) return 1;

    printf("--- State 4: User edits to green via attached editor ---\n");
    editor.setMaterial(material(0.0f, 0.8f, 0.1f, 0.6f));
    snprintf(filename, sizeof(filename), "%s_green.rgb", baseFilename);
    if (!renderScene(renderer, scene, target, filename)) return 1;

    printf("--- State 5: Programmatic material change syncs to editor ---\n");
    applyProgrammaticMaterial(attachment, material(1.0f, 0.5f, 0.0f, 0.4f));
    editor.syncFromAttachment();
    printf("Editor synced material color %.2f %.2f %.2f\n",
           editor.material().baseColor.r,
           editor.material().baseColor.g,
           editor.material().baseColor.b);
    snprintf(filename, sizeof(filename), "%s_orange.rgb", baseFilename);
    if (!renderScene(renderer, scene, target, filename)) return 1;

    printf("Generated 5 images showing bidirectional material editor attachment.\n");
    return 0;
}
