#ifndef OBOL_SCENE_SCENE_PACKET_GEOMETRY_H
#define OBOL_SCENE_SCENE_PACKET_GEOMETRY_H

#include <Obol/scene/Scene.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace obol {

enum class PacketGeometryDiagnosticSeverity {
    Warning,
    Error
};

struct PacketGeometryDiagnostic {
    PacketGeometryDiagnosticSeverity severity =
        PacketGeometryDiagnosticSeverity::Warning;
    SceneObjectId objectId = InvalidSceneObjectId;
    std::string message;
};

struct PacketTriangleVertex {
    Vec3 position;
    Vec3 normal;
    Vec2 texCoord;
    Color color;
};

struct PacketTriangle {
    SceneObjectId objectId = InvalidSceneObjectId;
    Material material;
    PacketTriangleVertex vertices[3];
};

struct PacketLineVertex {
    Vec3 position;
    Color color;
};

struct PacketLineSegment {
    SceneObjectId objectId = InvalidSceneObjectId;
    Material material;
    float lineWidth = 1.0f;
    PacketLineVertex vertices[2];
};

struct PacketPoint {
    SceneObjectId objectId = InvalidSceneObjectId;
    Material material;
    Vec3 position;
    Color color;
    float pointSize = 1.0f;
};

struct PacketTriangleExtractionOptions {
    bool includePrimitives = true;
    bool includeMeshes = true;
    uint32_t sphereSlices = 32;
    uint32_t sphereStacks = 16;
    uint32_t cylinderSlices = 32;
    uint32_t coneSlices = 32;
};

struct PacketGeometrySupport {
    size_t portableGeometryObjects = 0;
    size_t lightObjects = 0;
    size_t textObjects = 0;
    size_t cadObjects = 0;
    size_t backendNativeObjects = 0;
    size_t legacyObjects = 0;
    size_t unsupportedObjects = 0;
    bool hasLegacyFallbackRoot = false;
};

OBOL_V2_API PacketGeometrySupport inspectPacketGeometrySupport(
    const ScenePacket & packet,
    std::vector<PacketGeometryDiagnostic> * diagnostics = nullptr);

OBOL_V2_API bool collectPacketTriangles(
    const ScenePacket & packet,
    std::vector<PacketTriangle> & triangles,
    std::vector<PacketGeometryDiagnostic> * diagnostics = nullptr,
    const PacketTriangleExtractionOptions & options =
        PacketTriangleExtractionOptions{});

OBOL_V2_API bool collectPacketLineSegments(
    const ScenePacket & packet,
    std::vector<PacketLineSegment> & segments,
    std::vector<PacketGeometryDiagnostic> * diagnostics = nullptr);

OBOL_V2_API bool collectPacketPoints(
    const ScenePacket & packet,
    std::vector<PacketPoint> & points,
    std::vector<PacketGeometryDiagnostic> * diagnostics = nullptr);

} // namespace obol

#endif // OBOL_SCENE_SCENE_PACKET_GEOMETRY_H
