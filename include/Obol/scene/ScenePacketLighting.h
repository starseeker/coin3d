#ifndef OBOL_SCENE_SCENE_PACKET_LIGHTING_H
#define OBOL_SCENE_SCENE_PACKET_LIGHTING_H

#include <Obol/scene/ScenePacketGeometry.h>

#include <vector>

namespace obol {

enum class PacketLightKind {
    Directional,
    Point,
    Spot
};

struct PacketLight {
    SceneObjectId objectId = InvalidSceneObjectId;
    PacketLightKind kind = PacketLightKind::Directional;
    Color color;
    float intensity = 1.0f;
    Vec3 position;
    Vec3 direction = {0.0f, 0.0f, -1.0f};
    float cutOffAngleRadians = 0.78539816339f;
    float dropOffRate = 0.0f;
};

OBOL_V2_API bool collectPacketLights(
    const ScenePacket & packet,
    std::vector<PacketLight> & lights,
    std::vector<PacketGeometryDiagnostic> * diagnostics = nullptr);

} // namespace obol

#endif // OBOL_SCENE_SCENE_PACKET_LIGHTING_H
