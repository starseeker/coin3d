#ifndef OBOL_SCENE_SCENE_PACKET_EXTRACTION_H
#define OBOL_SCENE_SCENE_PACKET_EXTRACTION_H

#include <Obol/scene/ScenePacketCad.h>
#include <Obol/scene/ScenePacketGeometry.h>
#include <Obol/scene/ScenePacketLighting.h>
#include <Obol/scene/ScenePacketText.h>

#include <vector>

namespace obol {

struct PacketSceneExtractionOptions {
    bool includeTriangles = true;
    bool includeLineSegments = true;
    bool includePoints = true;
    bool includeLights = true;
    bool includeText = true;
    bool includeCadAssemblies = true;
    PacketTriangleExtractionOptions triangleOptions;
};

struct ExtractedPacketScene {
    PacketGeometrySupport support;
    bool complete = true;
    std::vector<PacketTriangle> triangles;
    std::vector<PacketLineSegment> lineSegments;
    std::vector<PacketPoint> points;
    std::vector<PacketLight> lights;
    std::vector<PacketText> text;
    std::vector<PacketCadAssembly> cadAssemblies;
    std::vector<PacketGeometryDiagnostic> diagnostics;
};

OBOL_V2_API bool extractPacketScene(
    const ScenePacket & packet,
    ExtractedPacketScene & extracted,
    const PacketSceneExtractionOptions & options =
        PacketSceneExtractionOptions{});

} // namespace obol

#endif // OBOL_SCENE_SCENE_PACKET_EXTRACTION_H
