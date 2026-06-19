#ifndef OBOL_SCENE_SCENE_PACKET_TEXT_H
#define OBOL_SCENE_SCENE_PACKET_TEXT_H

#include <Obol/scene/ScenePacketGeometry.h>

#include <cstdint>
#include <string>
#include <vector>

namespace obol {

enum class PacketTextKind {
    Text2D,
    Text3D
};

struct PacketText {
    SceneObjectId objectId = InvalidSceneObjectId;
    PacketTextKind kind = PacketTextKind::Text2D;
    Material material;
    Matrix4 localToWorld;
    Vec3 origin;
    std::string text;
    std::string fontName;
    float fontSize = 1.0f;
    float spacing = 1.0f;
    TextJustification justification = TextJustification::Left;
    bool depthTest = true;
    uint32_t parts = static_cast<uint32_t>(Text3DParts::All);
    std::vector<Color> partColors;
    std::vector<Vec2> profile;
};

OBOL_V2_API bool collectPacketText(
    const ScenePacket & packet,
    std::vector<PacketText> & text,
    std::vector<PacketGeometryDiagnostic> * diagnostics = nullptr);

} // namespace obol

#endif // OBOL_SCENE_SCENE_PACKET_TEXT_H
