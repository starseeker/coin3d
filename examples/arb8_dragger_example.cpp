/*
 * arb8_dragger_example.cpp
 *
 * Headless v2 Obol demonstration of an ARB8-style editable solid.  The
 * application owns the parametric vertex data, validates it, and materializes
 * portable mesh/polyline/handle geometry through obol::Scene.  Interactive
 * draggers remain a viewer/UI concern; this example renders the editable
 * handles as ordinary v2 scene objects so the output works on OpenGL2/swrast
 * and non-OpenGL backends that support the v2 subset.
 */

#include "headless_utils.h"

#include <Obol/Obol.h>

#include <array>
#include <cmath>
#include <cstdio>

namespace {

using Arb8Params = std::array<obol::Vec3, 8>;

constexpr int kFaces[6][4] = {
    {0, 3, 2, 1},
    {4, 5, 6, 7},
    {0, 1, 5, 4},
    {1, 2, 6, 5},
    {2, 3, 7, 6},
    {3, 0, 4, 7}
};

constexpr int kEdges[12][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0},
    {4, 5}, {5, 6}, {6, 7}, {7, 4},
    {0, 4}, {1, 5}, {2, 6}, {3, 7}
};

Arb8Params defaultArb8()
{
    return {{
        {-1.0f, -1.0f, -1.0f},
        { 1.0f, -1.0f, -1.0f},
        { 1.0f, -1.0f,  1.0f},
        {-1.0f, -1.0f,  1.0f},
        {-1.0f,  1.0f, -1.0f},
        { 1.0f,  1.0f, -1.0f},
        { 1.0f,  1.0f,  1.0f},
        {-1.0f,  1.0f,  1.0f}
    }};
}

Arb8Params shearedArb8()
{
    Arb8Params params = defaultArb8();
    for (int i = 4; i < 8; ++i) {
        params[static_cast<size_t>(i)].x += 0.4f;
    }
    return params;
}

obol::Material material(float r, float g, float b, float shininess = 0.3f)
{
    obol::Material mat;
    mat.baseColor = {r, g, b, 1.0f};
    mat.specular = {0.4f, 0.4f, 0.4f, 1.0f};
    mat.emissive = {r * 0.15f, g * 0.15f, b * 0.15f, 1.0f};
    mat.shininess = shininess;
    return mat;
}

obol::Transform translation(float x, float y, float z)
{
    obol::Transform transform;
    transform.translation = {x, y, z};
    return transform;
}

obol::Vec3 add(const obol::Vec3 & a, const obol::Vec3 & b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

obol::Vec3 scale(const obol::Vec3 & v, float factor)
{
    return {v.x * factor, v.y * factor, v.z * factor};
}

obol::Vec3 subtract(const obol::Vec3 & a, const obol::Vec3 & b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

obol::Vec3 cross(const obol::Vec3 & a, const obol::Vec3 & b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float dot(const obol::Vec3 & a, const obol::Vec3 & b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

bool validateConvexOrientation(const Arb8Params & params)
{
    obol::Vec3 centroid;
    for (const obol::Vec3 & vertex : params) {
        centroid = add(centroid, vertex);
    }
    centroid = scale(centroid, 1.0f / 8.0f);

    for (const auto & face : kFaces) {
        const obol::Vec3 v0 = params[face[0]];
        const obol::Vec3 v1 = params[face[1]];
        const obol::Vec3 v2 = params[face[2]];
        const obol::Vec3 normal = cross(subtract(v1, v0), subtract(v2, v0));
        if (std::fabs(dot(normal, subtract(v0, centroid))) <= 1e-6f) {
            return false;
        }
    }
    return true;
}

obol::Mesh makeArb8Mesh(const Arb8Params & params)
{
    obol::Mesh mesh;
    mesh.topology = obol::MeshTopology::Triangles;
    for (const obol::Vec3 & vertex : params) {
        mesh.positions.push_back(vertex);
    }

    for (const auto & face : kFaces) {
        mesh.indices.insert(mesh.indices.end(),
                            {static_cast<uint32_t>(face[0]),
                             static_cast<uint32_t>(face[1]),
                             static_cast<uint32_t>(face[2]),
                             static_cast<uint32_t>(face[0]),
                             static_cast<uint32_t>(face[2]),
                             static_cast<uint32_t>(face[3])});
    }
    return mesh;
}

void addWireEdge(obol::Scene & scene,
                 obol::SceneGroupId parent,
                 const obol::Vec3 & a,
                 const obol::Vec3 & b,
                 float lineWidth = 2.0f)
{
    obol::Polyline line;
    line.points = {a, b};
    line.lineWidth = lineWidth;
    scene.addPolyline(line, material(0.05f, 0.05f, 0.05f), obol::Transform{}, parent);
}

void addWireframe(obol::Scene & scene,
                  obol::SceneGroupId parent,
                  const Arb8Params & params,
                  const obol::Material & wireMaterial)
{
    for (const auto & edge : kEdges) {
        obol::Polyline line;
        line.points = {params[edge[0]], params[edge[1]]};
        line.lineWidth = 2.0f;
        scene.addPolyline(line, wireMaterial, obol::Transform{}, parent);
    }
}

void addHandles(obol::Scene & scene,
                obol::SceneGroupId parent,
                const Arb8Params & params)
{
    obol::PrimitiveOptions vertexHandle;
    vertexHandle.radius = 0.11f;
    for (const obol::Vec3 & vertex : params) {
        scene.addPrimitive(obol::Primitive::Sphere,
                           material(0.95f, 0.15f, 0.12f, 0.4f),
                           {vertex},
                           vertexHandle,
                           parent);
    }

    obol::PrimitiveOptions faceHandle;
    faceHandle.radius = 0.14f;
    for (const auto & face : kFaces) {
        obol::Vec3 center;
        for (int i = 0; i < 4; ++i) {
            center = add(center, params[face[i]]);
        }
        center = scale(center, 0.25f);
        scene.addPrimitive(obol::Primitive::Sphere,
                           material(0.1f, 0.8f, 0.25f, 0.35f),
                           {center},
                           faceHandle,
                           parent);
    }
}

void addArb8(obol::Scene & scene,
             const Arb8Params & params,
             float x,
             const obol::Material & solidMaterial,
             bool solid,
             bool handles)
{
    if (!validateConvexOrientation(params)) {
        std::fprintf(stderr, "ARB8 parameters failed orientation validation\n");
        return;
    }

    const obol::SceneGroupId group = scene.addGroup(translation(x, 0.0f, 0.0f));
    if (solid) {
        scene.addMesh(makeArb8Mesh(params), solidMaterial, obol::Transform{}, group);
    }
    addWireframe(scene, group, params, material(0.05f, 0.05f, 0.05f));
    if (handles) {
        addHandles(scene, group, params);
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

    const Arb8Params box = defaultArb8();
    const Arb8Params sheared = shearedArb8();
    addArb8(scene, box, -3.5f, material(0.2f, 0.5f, 0.9f, 0.45f), true, false);
    addArb8(scene, sheared, 0.0f, material(0.9f, 0.5f, 0.2f, 0.45f), true, true);
    addArb8(scene, sheared, 3.5f, material(0.1f, 0.9f, 0.3f, 0.25f), false, false);

    scene.setCamera(obol::PerspectiveCamera{
        {0.0f, 1.4f, 9.5f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        0.72f,
        0.1f,
        100.0f});

    char outpath[1024];
    if (argc > 1) {
        std::snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    } else {
        std::snprintf(outpath, sizeof(outpath), "arb8_dragger_example.rgb");
    }

    return renderScene(scene, outpath) ? 0 : 1;
}
