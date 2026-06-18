/*
 * truncated_cone_example.cpp
 *
 * Self-contained v2 Obol example that generates a truncated general cone
 * (frustum) as application-owned mesh data and renders it through the modern
 * scene API.  The same scene degrades through the OpenGL2/swrast bridge because
 * the geometry is expressed as indexed triangles and polylines.
 */

#include "headless_utils.h"

#include <Obol/Obol.h>

#include <cmath>
#include <cstdio>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

struct TruncatedConeParams {
    float bottomRadius = 1.0f;
    float topRadius = 0.5f;
    float height = 2.0f;
    int sides = 16;
};

obol::Material material(float r, float g, float b, float shininess = 0.2f)
{
    obol::Material mat;
    mat.baseColor = {r, g, b, 1.0f};
    mat.specular = {0.6f, 0.6f, 0.6f, 1.0f};
    mat.emissive = {r * 0.15f, g * 0.15f, b * 0.15f, 1.0f};
    mat.shininess = shininess;
    return mat;
}

obol::Mesh makeTruncatedConeMesh(const TruncatedConeParams & params)
{
    const int sides = params.sides < 3 ? 3 : params.sides;
    const float yBot = -params.height * 0.5f;
    const float yTop = params.height * 0.5f;

    obol::Mesh mesh;
    mesh.topology = obol::MeshTopology::Triangles;

    for (int i = 0; i < sides; ++i) {
        const float angle = static_cast<float>(2.0 * M_PI * i / sides);
        mesh.positions.push_back({params.bottomRadius * std::cos(angle),
                                  yBot,
                                  params.bottomRadius * std::sin(angle)});
    }
    for (int i = 0; i < sides; ++i) {
        const float angle = static_cast<float>(2.0 * M_PI * i / sides);
        mesh.positions.push_back({params.topRadius * std::cos(angle),
                                  yTop,
                                  params.topRadius * std::sin(angle)});
    }

    for (int i = 0; i < sides; ++i) {
        const uint32_t i0b = static_cast<uint32_t>(i);
        const uint32_t i1b = static_cast<uint32_t>((i + 1) % sides);
        const uint32_t i0t = static_cast<uint32_t>(sides + i);
        const uint32_t i1t = static_cast<uint32_t>(sides + ((i + 1) % sides));
        mesh.indices.insert(mesh.indices.end(), {i0b, i1b, i0t, i1b, i1t, i0t});
    }

    if (params.bottomRadius > 0.0f) {
        const uint32_t center = static_cast<uint32_t>(mesh.positions.size());
        mesh.positions.push_back({0.0f, yBot, 0.0f});
        for (int i = 0; i < sides; ++i) {
            mesh.indices.insert(mesh.indices.end(),
                                {center,
                                 static_cast<uint32_t>((i + 1) % sides),
                                 static_cast<uint32_t>(i)});
        }
    }

    if (params.topRadius > 0.0f) {
        const uint32_t center = static_cast<uint32_t>(mesh.positions.size());
        mesh.positions.push_back({0.0f, yTop, 0.0f});
        for (int i = 0; i < sides; ++i) {
            mesh.indices.insert(mesh.indices.end(),
                                {center,
                                 static_cast<uint32_t>(sides + i),
                                 static_cast<uint32_t>(sides + ((i + 1) % sides))});
        }
    }

    return mesh;
}

void addEdge(obol::Scene & scene,
             const obol::Vec3 & a,
             const obol::Vec3 & b,
             const obol::Transform & transform)
{
    obol::Polyline line;
    line.points = {a, b};
    line.lineWidth = 2.0f;
    scene.addPolyline(line, material(0.05f, 0.05f, 0.05f), transform);
}

void addWireTruncatedCone(obol::Scene & scene,
                          const TruncatedConeParams & params,
                          const obol::Transform & transform)
{
    const int sides = params.sides < 3 ? 3 : params.sides;
    const float yBot = -params.height * 0.5f;
    const float yTop = params.height * 0.5f;
    std::vector<obol::Vec3> bottom;
    std::vector<obol::Vec3> top;

    for (int i = 0; i < sides; ++i) {
        const float angle = static_cast<float>(2.0 * M_PI * i / sides);
        bottom.push_back({params.bottomRadius * std::cos(angle),
                          yBot,
                          params.bottomRadius * std::sin(angle)});
        top.push_back({params.topRadius * std::cos(angle),
                       yTop,
                       params.topRadius * std::sin(angle)});
    }

    for (int i = 0; i < sides; ++i) {
        addEdge(scene, bottom[i], bottom[(i + 1) % sides], transform);
        addEdge(scene, top[i], top[(i + 1) % sides], transform);
    }

    const int step = sides >= 12 ? sides / 8 : 1;
    for (int i = 0; i < sides; i += step) {
        addEdge(scene, bottom[i], top[i], transform);
    }
}

bool renderScene(const obol::Scene & scene, const char * filename)
{
    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;

    obol::OffscreenRenderer renderer(backend, target);
    const obol::FrameResult result = renderer.render(scene);
    return result.success && renderer.writeRGB(filename);
}

} // namespace

int main(int argc, char ** argv)
{
    initCoinHeadless();

    obol::Scene scene;
    scene.addDirectionalLight({{-0.5f, -0.8f, -0.6f}, {1.0f, 1.0f, 1.0f, 1.0f}, 1.0f});

    scene.addMesh(makeTruncatedConeMesh({}),
                  material(0.2f, 0.5f, 0.9f, 0.6f),
                  {{-2.0f, 0.0f, 0.0f}});

    const TruncatedConeParams wireParams{1.2f, 0.0f, 3.0f, 8};
    addWireTruncatedCone(scene, wireParams, {{2.0f, 0.0f, 0.0f}});

    scene.setCamera(obol::PerspectiveCamera{
        {0.0f, 1.0f, 7.5f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        0.7f,
        0.1f,
        100.0f});

    char outpath[1024];
    if (argc > 1) {
        std::snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    } else {
        std::snprintf(outpath, sizeof(outpath), "truncated_cone_example.rgb");
    }

    return renderScene(scene, outpath) ? 0 : 1;
}
