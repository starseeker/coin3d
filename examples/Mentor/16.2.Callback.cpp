/*
 * Headless version of Inventor Mentor example 16.2
 *
 * Original: material editor invokes callbacks that copy material node fields.
 * Headless: toolkit-owned material editor callbacks update v2 object material.
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cmath>
#include <cstdio>
#include <cstdint>
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

obol::Transform transform(float x, float y, float z)
{
    obol::Transform xf;
    xf.translation = {x, y, z};
    return xf;
}

obol::Mesh makeDishMesh()
{
    obol::Mesh mesh;
    mesh.topology = obol::MeshTopology::Polygons;

    constexpr int segments = 48;
    constexpr float pi = 3.14159265358979323846f;

    struct Ring {
        float radius;
        float z;
        float radialNormal;
        float zNormal;
    };
    const Ring rings[] = {
        {0.95f, -0.34f,  0.0f, -1.0f},
        {1.22f, -0.34f,  0.0f, -1.0f},
        {1.42f, -0.12f,  0.85f, -0.52f},
        {1.52f,  0.24f,  0.95f,  0.28f},
        {1.44f,  0.36f,  0.25f,  0.97f},
        {1.02f,  0.36f,  0.0f,   1.0f},
        {0.78f,  0.10f, -0.70f,  0.72f},
        {0.55f, -0.12f, -0.25f,  0.97f},
        {0.10f, -0.16f,  0.0f,   1.0f}
    };

    const auto addRing = [&](const Ring & ring) {
        const uint32_t first = static_cast<uint32_t>(mesh.positions.size());
        for (int i = 0; i < segments; ++i) {
            const float angle = 2.0f * pi * static_cast<float>(i) /
                                static_cast<float>(segments);
            const float c = std::cos(angle);
            const float s = std::sin(angle);
            mesh.positions.push_back({ring.radius * c, ring.radius * s, ring.z});
            mesh.normals.push_back({ring.radialNormal * c,
                                    ring.radialNormal * s,
                                    ring.zNormal});
        }
        return first;
    };

    std::vector<uint32_t> ringStarts;
    ringStarts.reserve(sizeof(rings) / sizeof(rings[0]));
    for (const Ring & ring : rings) {
        ringStarts.push_back(addRing(ring));
    }

    const auto addQuad = [&](uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
        mesh.indices.push_back(a);
        mesh.indices.push_back(b);
        mesh.indices.push_back(c);
        mesh.indices.push_back(d);
        mesh.faceVertexCounts.push_back(4);
    };

    for (size_t ring = 0; ring + 1 < ringStarts.size(); ++ring) {
        const uint32_t aStart = ringStarts[ring];
        const uint32_t bStart = ringStarts[ring + 1];
        for (int i = 0; i < segments; ++i) {
            const uint32_t next = static_cast<uint32_t>((i + 1) % segments);
            addQuad(aStart + i, bStart + i, bStart + next, aStart + next);
        }
    }

    return mesh;
}

std::vector<obol::SceneObjectId> addDogDish(obol::Scene & scene, const obol::Material & mat)
{
    obol::SceneGroupId group = scene.addGroup(transform(0.0f, 0.0f, 0.0f));

    std::vector<obol::SceneObjectId> editable;
    editable.push_back(scene.addMesh(makeDishMesh(), mat, obol::Transform{}, group));

    obol::Material food = material(1.0f, 0.1f, 0.05f, 0.45f);
    obol::PrimitiveOptions kibble;
    kibble.radius = 0.17f;
    const obol::Vec3 foodCenters[] = {
        {-0.62f, -0.30f, 0.18f}, {-0.34f, -0.34f, 0.22f},
        {-0.05f, -0.32f, 0.25f}, { 0.24f, -0.34f, 0.22f},
        { 0.54f, -0.28f, 0.18f}, {-0.48f, -0.04f, 0.28f},
        {-0.18f, -0.02f, 0.34f}, { 0.12f, -0.01f, 0.36f},
        { 0.42f,  0.03f, 0.29f}, { 0.70f,  0.05f, 0.22f},
        {-0.60f,  0.30f, 0.20f}, {-0.31f,  0.29f, 0.30f},
        { 0.00f,  0.30f, 0.38f}, { 0.30f,  0.30f, 0.32f},
        { 0.58f,  0.31f, 0.24f}, {-0.16f,  0.54f, 0.34f},
        { 0.16f,  0.52f, 0.34f}, { 0.46f,  0.50f, 0.28f}
    };
    for (const obol::Vec3 & center : foodCenters) {
        scene.addPrimitive(obol::Primitive::Sphere,
                           food,
                           transform(center.x, center.y, center.z),
                           kibble,
                           group);
    }

    return editable;
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
    camera.position = {0.0f, -4.8f, 2.0f};
    camera.target = {0.0f, 0.0f, 0.12f};
    camera.up = {0.0f, 0.0f, 1.0f};
    camera.verticalFieldOfViewRadians = 0.82f;
    scene.setCamera(camera);
    scene.addDirectionalLight(obol::DirectionalLight{});

    const obol::Material defaultMaterial = material(0.8f, 0.8f, 0.8f, 0.2f);
    const std::vector<obol::SceneObjectId> dish = addDogDish(scene, defaultMaterial);

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
