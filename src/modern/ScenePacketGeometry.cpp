#include <Obol/scene/ScenePacketGeometry.h>

#include <algorithm>
#include <cmath>

namespace obol {
namespace {

constexpr float Pi = 3.14159265358979323846f;

Vec3 pktSubtract(Vec3 a, Vec3 b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 pktCross(Vec3 a, Vec3 b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x};
}

float pktLength(Vec3 v)
{
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

Vec3 pktNormalize(Vec3 v, Vec3 fallback = {0.0f, 0.0f, 1.0f})
{
    const float len = pktLength(v);
    if (len <= 1.0e-8f) {
        return fallback;
    }
    return {v.x / len, v.y / len, v.z / len};
}

Vec3 pktTransformPoint(const Matrix4 & matrix, Vec3 point)
{
    return {
        matrix.values[0] * point.x + matrix.values[4] * point.y +
            matrix.values[8] * point.z + matrix.values[12],
        matrix.values[1] * point.x + matrix.values[5] * point.y +
            matrix.values[9] * point.z + matrix.values[13],
        matrix.values[2] * point.x + matrix.values[6] * point.y +
            matrix.values[10] * point.z + matrix.values[14]};
}

Vec3 pktTransformDirection(const Matrix4 & matrix, Vec3 direction)
{
    return {
        matrix.values[0] * direction.x + matrix.values[4] * direction.y +
            matrix.values[8] * direction.z,
        matrix.values[1] * direction.x + matrix.values[5] * direction.y +
            matrix.values[9] * direction.z,
        matrix.values[2] * direction.x + matrix.values[6] * direction.y +
            matrix.values[10] * direction.z};
}

Vec3 pktTransformNormal(const Matrix4 & matrix, Vec3 normal)
{
    const float a00 = matrix.values[0];
    const float a01 = matrix.values[4];
    const float a02 = matrix.values[8];
    const float a10 = matrix.values[1];
    const float a11 = matrix.values[5];
    const float a12 = matrix.values[9];
    const float a20 = matrix.values[2];
    const float a21 = matrix.values[6];
    const float a22 = matrix.values[10];

    const float c00 = a11 * a22 - a12 * a21;
    const float c01 = a12 * a20 - a10 * a22;
    const float c02 = a10 * a21 - a11 * a20;
    const float c10 = a02 * a21 - a01 * a22;
    const float c11 = a00 * a22 - a02 * a20;
    const float c12 = a01 * a20 - a00 * a21;
    const float c20 = a01 * a12 - a02 * a11;
    const float c21 = a02 * a10 - a00 * a12;
    const float c22 = a00 * a11 - a01 * a10;

    const float det = a00 * c00 + a01 * c01 + a02 * c02;
    if (std::fabs(det) <= 1.0e-8f) {
        return pktNormalize(pktTransformDirection(matrix, normal));
    }

    const float invDet = 1.0f / det;
    return pktNormalize({
        (c00 * normal.x + c01 * normal.y + c02 * normal.z) * invDet,
        (c10 * normal.x + c11 * normal.y + c12 * normal.z) * invDet,
        (c20 * normal.x + c21 * normal.y + c22 * normal.z) * invDet});
}

void addDiagnostic(std::vector<PacketGeometryDiagnostic> * diagnostics,
                   PacketGeometryDiagnosticSeverity severity,
                   SceneObjectId objectId,
                   const std::string & message)
{
    if (!diagnostics) {
        return;
    }
    PacketGeometryDiagnostic diagnostic;
    diagnostic.severity = severity;
    diagnostic.objectId = objectId;
    diagnostic.message = message;
    diagnostics->push_back(diagnostic);
}

Color faceColorFor(const Mesh & mesh, size_t faceIndex, const Material & material)
{
    if (mesh.faceColors.empty()) {
        return material.baseColor;
    }
    size_t colorIndex = faceIndex;
    if (faceIndex < mesh.faceColorIndices.size()) {
        colorIndex = mesh.faceColorIndices[faceIndex];
    }
    if (colorIndex >= mesh.faceColors.size()) {
        return material.baseColor;
    }
    return mesh.faceColors[colorIndex];
}

bool vertexFor(const SceneObjectRecord & object,
               const Mesh & mesh,
               size_t streamIndex,
               uint32_t positionIndex,
               Color defaultColor,
               PacketTriangleVertex & vertex)
{
    if (positionIndex >= mesh.positions.size()) {
        return false;
    }

    vertex.position =
        pktTransformPoint(object.localToWorld, mesh.positions[positionIndex]);
    vertex.normal = {0.0f, 0.0f, 0.0f};
    if (positionIndex < mesh.normals.size()) {
        vertex.normal =
            pktTransformNormal(object.localToWorld, mesh.normals[positionIndex]);
    }
    vertex.texCoord = {};
    size_t texCoordIndex = positionIndex;
    if (streamIndex < mesh.texCoordIndices.size()) {
        texCoordIndex = mesh.texCoordIndices[streamIndex];
    }
    if (texCoordIndex < mesh.texCoords.size()) {
        vertex.texCoord = mesh.texCoords[texCoordIndex];
    }
    vertex.color = defaultColor;
    size_t vertexColorIndex = positionIndex;
    if (streamIndex < mesh.vertexColorIndices.size()) {
        vertexColorIndex = mesh.vertexColorIndices[streamIndex];
    }
    if (vertexColorIndex < mesh.vertexColors.size()) {
        vertex.color = mesh.vertexColors[vertexColorIndex];
    }
    return true;
}

uint32_t positionIndexFor(const Mesh & mesh, size_t streamIndex)
{
    if (!mesh.indices.empty()) {
        return mesh.indices[streamIndex];
    }
    return static_cast<uint32_t>(streamIndex);
}

bool appendTriangle(const SceneObjectRecord & object,
                    const Mesh & mesh,
                    size_t aStream,
                    size_t bStream,
                    size_t cStream,
                    size_t faceIndex,
                    std::vector<PacketTriangle> & triangles,
                    std::vector<PacketGeometryDiagnostic> * diagnostics)
{
    const size_t streamSize =
        mesh.indices.empty() ? mesh.positions.size() : mesh.indices.size();
    if (aStream >= streamSize || bStream >= streamSize || cStream >= streamSize) {
        addDiagnostic(diagnostics,
                      PacketGeometryDiagnosticSeverity::Error,
                      object.id,
                      "mesh topology references vertices beyond the available stream");
        return false;
    }

    PacketTriangle triangle;
    triangle.objectId = object.id;
    triangle.material = object.material;
    const Color defaultColor = faceColorFor(mesh, faceIndex, object.material);
    if (!vertexFor(object,
                   mesh,
                   aStream,
                   positionIndexFor(mesh, aStream),
                   defaultColor,
                   triangle.vertices[0]) ||
        !vertexFor(object,
                   mesh,
                   bStream,
                   positionIndexFor(mesh, bStream),
                   defaultColor,
                   triangle.vertices[1]) ||
        !vertexFor(object,
                   mesh,
                   cStream,
                   positionIndexFor(mesh, cStream),
                   defaultColor,
                   triangle.vertices[2])) {
        addDiagnostic(diagnostics,
                      PacketGeometryDiagnosticSeverity::Error,
                      object.id,
                      "mesh index references a missing position");
        return false;
    }

    const Vec3 faceNormal = pktNormalize(
        pktCross(pktSubtract(triangle.vertices[1].position,
                       triangle.vertices[0].position),
              pktSubtract(triangle.vertices[2].position,
                       triangle.vertices[0].position)));
    for (PacketTriangleVertex & vertex : triangle.vertices) {
        if (pktLength(vertex.normal) <= 1.0e-8f) {
            vertex.normal = faceNormal;
        }
    }
    triangles.push_back(triangle);
    return true;
}

bool appendMeshTriangles(const SceneObjectRecord & object,
                         const Mesh & mesh,
                         std::vector<PacketTriangle> & triangles,
                         std::vector<PacketGeometryDiagnostic> * diagnostics)
{
    bool ok = true;
    const size_t streamSize =
        mesh.indices.empty() ? mesh.positions.size() : mesh.indices.size();
    size_t faceIndex = 0;

    switch (mesh.topology) {
    case MeshTopology::Triangles:
        if (streamSize % 3 != 0) {
            addDiagnostic(diagnostics,
                          PacketGeometryDiagnosticSeverity::Error,
                          object.id,
                          "triangle mesh stream is not divisible by three");
            ok = false;
        }
        for (size_t i = 0; i + 2 < streamSize; i += 3, ++faceIndex) {
            ok = appendTriangle(object,
                                mesh,
                                i,
                                i + 1,
                                i + 2,
                                faceIndex,
                                triangles,
                                diagnostics) && ok;
        }
        break;
    case MeshTopology::Polygons: {
        size_t cursor = 0;
        if (mesh.faceVertexCounts.empty()) {
            addDiagnostic(diagnostics,
                          PacketGeometryDiagnosticSeverity::Error,
                          object.id,
                          "polygon mesh has no face vertex counts");
            return false;
        }
        for (uint32_t count : mesh.faceVertexCounts) {
            if (cursor + count > streamSize) {
                addDiagnostic(diagnostics,
                              PacketGeometryDiagnosticSeverity::Error,
                              object.id,
                              "polygon face exceeds the mesh vertex stream");
                return false;
            }
            for (uint32_t i = 1; i + 1 < count; ++i) {
                ok = appendTriangle(object,
                                    mesh,
                                    cursor,
                                    cursor + i,
                                    cursor + i + 1,
                                    faceIndex,
                                    triangles,
                                    diagnostics) && ok;
            }
            cursor += count;
            ++faceIndex;
        }
        break;
    }
    case MeshTopology::TriangleStrips: {
        const bool hasExplicitStrips = !mesh.stripVertexCounts.empty();
        size_t cursor = 0;
        const size_t stripCount =
            hasExplicitStrips ? mesh.stripVertexCounts.size() : 1;
        for (size_t strip = 0; strip < stripCount; ++strip) {
            const size_t count =
                hasExplicitStrips ? mesh.stripVertexCounts[strip] : streamSize;
            if (cursor + count > streamSize) {
                addDiagnostic(diagnostics,
                              PacketGeometryDiagnosticSeverity::Error,
                              object.id,
                              "triangle strip exceeds the mesh vertex stream");
                return false;
            }
            for (size_t i = 0; i + 2 < count; ++i, ++faceIndex) {
                const size_t a = cursor + i;
                const size_t b = cursor + i + 1;
                const size_t c = cursor + i + 2;
                ok = (i % 2 == 0
                          ? appendTriangle(object, mesh, a, b, c, faceIndex,
                                           triangles, diagnostics)
                          : appendTriangle(object, mesh, b, a, c, faceIndex,
                                           triangles, diagnostics)) && ok;
            }
            cursor += count;
        }
        break;
    }
    case MeshTopology::QuadGrid: {
        const uint32_t rows = mesh.gridVertexRows;
        const uint32_t columns = mesh.gridVertexColumns;
        if (rows < 2 || columns < 2 ||
            static_cast<size_t>(rows) * static_cast<size_t>(columns) >
                mesh.positions.size()) {
            addDiagnostic(diagnostics,
                          PacketGeometryDiagnosticSeverity::Error,
                          object.id,
                          "quad-grid mesh has invalid dimensions");
            return false;
        }
        for (uint32_t row = 0; row + 1 < rows; ++row) {
            for (uint32_t column = 0; column + 1 < columns; ++column) {
                const size_t i0 =
                    static_cast<size_t>(row) * columns + column;
                const size_t i1 = i0 + 1;
                const size_t i2 = i0 + columns;
                const size_t i3 = i2 + 1;
                ok = appendTriangle(object, mesh, i0, i1, i3, faceIndex,
                                    triangles, diagnostics) && ok;
                ok = appendTriangle(object, mesh, i0, i3, i2, faceIndex,
                                    triangles, diagnostics) && ok;
                ++faceIndex;
            }
        }
        break;
    }
    }

    return ok;
}

void appendRawTriangle(const SceneObjectRecord & object,
                       Vec3 a,
                       Vec3 b,
                       Vec3 c,
                       Vec3 normal,
                       std::vector<PacketTriangle> & triangles)
{
    PacketTriangle triangle;
    triangle.objectId = object.id;
    triangle.material = object.material;
    const Vec3 worldNormal = pktTransformNormal(object.localToWorld, normal);
    triangle.vertices[0].position = pktTransformPoint(object.localToWorld, a);
    triangle.vertices[1].position = pktTransformPoint(object.localToWorld, b);
    triangle.vertices[2].position = pktTransformPoint(object.localToWorld, c);
    for (PacketTriangleVertex & vertex : triangle.vertices) {
        vertex.normal = worldNormal;
        vertex.color = object.material.baseColor;
    }
    triangles.push_back(triangle);
}

Mesh primitiveSphereMesh(const SceneObjectRecord & object,
                         const PacketTriangleExtractionOptions & options)
{
    return makeSphereMesh(object.primitiveOptions.radius,
                          std::max<uint32_t>(3, options.sphereSlices),
                          std::max<uint32_t>(2, options.sphereStacks));
}

bool appendCubeTriangles(const SceneObjectRecord & object,
                         std::vector<PacketTriangle> & triangles)
{
    const float x = object.primitiveOptions.width * 0.5f;
    const float y = object.primitiveOptions.height * 0.5f;
    const float z = object.primitiveOptions.depth * 0.5f;

    appendRawTriangle(object, {-x, -y, z}, {x, -y, z}, {x, y, z},
                      {0.0f, 0.0f, 1.0f}, triangles);
    appendRawTriangle(object, {-x, -y, z}, {x, y, z}, {-x, y, z},
                      {0.0f, 0.0f, 1.0f}, triangles);
    appendRawTriangle(object, {x, -y, -z}, {-x, -y, -z}, {-x, y, -z},
                      {0.0f, 0.0f, -1.0f}, triangles);
    appendRawTriangle(object, {x, -y, -z}, {-x, y, -z}, {x, y, -z},
                      {0.0f, 0.0f, -1.0f}, triangles);
    appendRawTriangle(object, {x, -y, z}, {x, -y, -z}, {x, y, -z},
                      {1.0f, 0.0f, 0.0f}, triangles);
    appendRawTriangle(object, {x, -y, z}, {x, y, -z}, {x, y, z},
                      {1.0f, 0.0f, 0.0f}, triangles);
    appendRawTriangle(object, {-x, -y, -z}, {-x, -y, z}, {-x, y, z},
                      {-1.0f, 0.0f, 0.0f}, triangles);
    appendRawTriangle(object, {-x, -y, -z}, {-x, y, z}, {-x, y, -z},
                      {-1.0f, 0.0f, 0.0f}, triangles);
    appendRawTriangle(object, {-x, y, z}, {x, y, z}, {x, y, -z},
                      {0.0f, 1.0f, 0.0f}, triangles);
    appendRawTriangle(object, {-x, y, z}, {x, y, -z}, {-x, y, -z},
                      {0.0f, 1.0f, 0.0f}, triangles);
    appendRawTriangle(object, {-x, -y, -z}, {x, -y, -z}, {x, -y, z},
                      {0.0f, -1.0f, 0.0f}, triangles);
    appendRawTriangle(object, {-x, -y, -z}, {x, -y, z}, {-x, -y, z},
                      {0.0f, -1.0f, 0.0f}, triangles);
    return true;
}

bool appendCylinderTriangles(const SceneObjectRecord & object,
                             const PacketTriangleExtractionOptions & options,
                             std::vector<PacketTriangle> & triangles)
{
    const uint32_t slices = std::max<uint32_t>(3, options.cylinderSlices);
    const float radius = object.primitiveOptions.radius;
    const float halfHeight = object.primitiveOptions.height * 0.5f;
    for (uint32_t i = 0; i < slices; ++i) {
        const float a0 = 2.0f * Pi * static_cast<float>(i) /
                         static_cast<float>(slices);
        const float a1 = 2.0f * Pi * static_cast<float>(i + 1) /
                         static_cast<float>(slices);
        const Vec3 p0 = {std::cos(a0) * radius, -halfHeight,
                         std::sin(a0) * radius};
        const Vec3 p1 = {std::cos(a1) * radius, -halfHeight,
                         std::sin(a1) * radius};
        const Vec3 p2 = {std::cos(a0) * radius, halfHeight,
                         std::sin(a0) * radius};
        const Vec3 p3 = {std::cos(a1) * radius, halfHeight,
                         std::sin(a1) * radius};
        const Vec3 sideNormal =
            pktNormalize({std::cos((a0 + a1) * 0.5f), 0.0f,
                       std::sin((a0 + a1) * 0.5f)});
        appendRawTriangle(object, p0, p1, p3, sideNormal,
                          triangles);
        appendRawTriangle(object, p0, p3, p2, sideNormal,
                          triangles);
        appendRawTriangle(object, {0.0f, halfHeight, 0.0f}, p2, p3,
                          {0.0f, 1.0f, 0.0f}, triangles);
        appendRawTriangle(object, {0.0f, -halfHeight, 0.0f}, p1, p0,
                          {0.0f, -1.0f, 0.0f}, triangles);
    }
    return true;
}

bool appendConeTriangles(const SceneObjectRecord & object,
                         const PacketTriangleExtractionOptions & options,
                         std::vector<PacketTriangle> & triangles)
{
    const uint32_t slices = std::max<uint32_t>(3, options.coneSlices);
    const float radius = object.primitiveOptions.radius;
    const float halfHeight = object.primitiveOptions.height * 0.5f;
    const Vec3 tip = {0.0f, halfHeight, 0.0f};
    for (uint32_t i = 0; i < slices; ++i) {
        const float a0 = 2.0f * Pi * static_cast<float>(i) /
                         static_cast<float>(slices);
        const float a1 = 2.0f * Pi * static_cast<float>(i + 1) /
                         static_cast<float>(slices);
        const Vec3 p0 = {std::cos(a0) * radius, -halfHeight,
                         std::sin(a0) * radius};
        const Vec3 p1 = {std::cos(a1) * radius, -halfHeight,
                         std::sin(a1) * radius};
        const Vec3 normal =
            pktNormalize({std::cos((a0 + a1) * 0.5f) * object.primitiveOptions.height,
                       radius,
                       std::sin((a0 + a1) * 0.5f) * object.primitiveOptions.height});
        appendRawTriangle(object, p0, p1, tip, normal, triangles);
        appendRawTriangle(object, {0.0f, -halfHeight, 0.0f}, p1, p0,
                          {0.0f, -1.0f, 0.0f}, triangles);
    }
    return true;
}

bool appendPrimitiveTriangles(const SceneObjectRecord & object,
                              const PacketTriangleExtractionOptions & options,
                              std::vector<PacketTriangle> & triangles,
                              std::vector<PacketGeometryDiagnostic> * diagnostics)
{
    switch (object.primitive) {
    case Primitive::Cube:
        return appendCubeTriangles(object, triangles);
    case Primitive::Sphere: {
        const Mesh mesh = primitiveSphereMesh(object, options);
        return appendMeshTriangles(object, mesh, triangles, diagnostics);
    }
    case Primitive::Cone:
        return appendConeTriangles(object, options, triangles);
    case Primitive::Cylinder:
        return appendCylinderTriangles(object, options, triangles);
    }
    return true;
}

} // namespace

PacketGeometrySupport
inspectPacketGeometrySupport(const ScenePacket & packet,
                             std::vector<PacketGeometryDiagnostic> * diagnostics)
{
    PacketGeometrySupport support;
    support.hasLegacyFallbackRoot = packet.hasLegacyFallbackRoot;
    if (packet.hasLegacyFallbackRoot) {
        addDiagnostic(diagnostics,
                      PacketGeometryDiagnosticSeverity::Warning,
                      InvalidSceneObjectId,
                      "scene packet has a legacy fallback root that cannot be lowered to portable packet triangles");
    }

    for (const SceneObjectRecord & object : packet.objects) {
        switch (object.type) {
        case SceneObjectType::Primitive:
        case SceneObjectType::Mesh:
        case SceneObjectType::Polyline:
        case SceneObjectType::PointCloud:
            ++support.portableGeometryObjects;
            break;
        case SceneObjectType::DirectionalLight:
        case SceneObjectType::PointLight:
        case SceneObjectType::SpotLight:
            ++support.lightObjects;
            break;
        case SceneObjectType::Text2D:
        case SceneObjectType::Text3D:
            ++support.textObjects;
            break;
        case SceneObjectType::CadAssembly:
            ++support.cadObjects;
            addDiagnostic(diagnostics,
                          PacketGeometryDiagnosticSeverity::Warning,
                          object.id,
                          "CAD assemblies require a CAD backend path and are not packet triangles");
            break;
        case SceneObjectType::OpenGLCallback:
            ++support.backendNativeObjects;
            addDiagnostic(diagnostics,
                          PacketGeometryDiagnosticSeverity::Warning,
                          object.id,
                          "backend-native OpenGL callbacks require an OpenGL backend path and are not packet triangles");
            break;
        case SceneObjectType::LegacySceneGraph:
            ++support.legacyObjects;
            addDiagnostic(diagnostics,
                          PacketGeometryDiagnosticSeverity::Warning,
                          object.id,
                          "legacy scene graph objects require a compatibility backend path and are not packet triangles");
            break;
        case SceneObjectType::Any:
            ++support.unsupportedObjects;
            addDiagnostic(diagnostics,
                          PacketGeometryDiagnosticSeverity::Warning,
                          object.id,
                          "unknown packet object type cannot be lowered to portable packet triangles");
            break;
        }
    }

    return support;
}

bool collectPacketTriangles(const ScenePacket & packet,
                            std::vector<PacketTriangle> & triangles,
                            std::vector<PacketGeometryDiagnostic> * diagnostics,
                            const PacketTriangleExtractionOptions & options)
{
    bool ok = true;
    for (const SceneObjectRecord & object : packet.objects) {
        if (object.type == SceneObjectType::Primitive &&
            options.includePrimitives) {
            ok = appendPrimitiveTriangles(object, options, triangles,
                                          diagnostics) && ok;
        } else if (object.type == SceneObjectType::Mesh &&
                   options.includeMeshes) {
            ok = appendMeshTriangles(object, object.mesh, triangles,
                                     diagnostics) && ok;
        }
    }
    return ok;
}

bool collectPacketLineSegments(const ScenePacket & packet,
                               std::vector<PacketLineSegment> & segments,
                               std::vector<PacketGeometryDiagnostic> * diagnostics)
{
    bool ok = true;
    for (const SceneObjectRecord & object : packet.objects) {
        if (object.type != SceneObjectType::Polyline) {
            continue;
        }
        if (object.polyline.points.size() < 2) {
            addDiagnostic(diagnostics,
                          PacketGeometryDiagnosticSeverity::Warning,
                          object.id,
                          "polyline has fewer than two points");
            ok = false;
            continue;
        }
        for (size_t i = 0; i + 1 < object.polyline.points.size(); ++i) {
            PacketLineSegment segment;
            segment.objectId = object.id;
            segment.material = object.material;
            segment.lineWidth = object.polyline.lineWidth;
            segment.vertices[0].position =
                pktTransformPoint(object.localToWorld,
                                  object.polyline.points[i]);
            segment.vertices[1].position =
                pktTransformPoint(object.localToWorld,
                                  object.polyline.points[i + 1]);
            segment.vertices[0].color = object.material.baseColor;
            segment.vertices[1].color = object.material.baseColor;
            segments.push_back(segment);
        }
    }
    return ok;
}

bool collectPacketPoints(const ScenePacket & packet,
                         std::vector<PacketPoint> & points,
                         std::vector<PacketGeometryDiagnostic> * diagnostics)
{
    bool ok = true;
    for (const SceneObjectRecord & object : packet.objects) {
        if (object.type != SceneObjectType::PointCloud) {
            continue;
        }
        if (object.pointCloud.points.empty()) {
            addDiagnostic(diagnostics,
                          PacketGeometryDiagnosticSeverity::Warning,
                          object.id,
                          "point cloud has no points");
            ok = false;
            continue;
        }
        for (Vec3 pointPosition : object.pointCloud.points) {
            PacketPoint point;
            point.objectId = object.id;
            point.material = object.material;
            point.position =
                pktTransformPoint(object.localToWorld, pointPosition);
            point.color = object.material.baseColor;
            point.pointSize = object.pointCloud.pointSize;
            points.push_back(point);
        }
    }
    return ok;
}

} // namespace obol
