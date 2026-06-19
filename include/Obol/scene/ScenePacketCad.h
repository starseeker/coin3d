#ifndef OBOL_SCENE_SCENE_PACKET_CAD_H
#define OBOL_SCENE_SCENE_PACKET_CAD_H

#include <Obol/cad/CadAssembly.h>
#include <Obol/scene/ScenePacketGeometry.h>

#include <string>
#include <vector>

namespace obol {

struct PacketCadPart {
    SceneObjectId objectId = InvalidSceneObjectId;
    PartId partId;
    CadPartGeometry geometry;
};

struct PacketCadInstance {
    SceneObjectId objectId = InvalidSceneObjectId;
    InstanceId instanceId;
    PartId partId;
    InstanceId parent;
    std::string childName;
    uint32_t occurrenceIndex = 0;
    uint8_t boolOp = 0;
    CadInstanceStyle style;
    Matrix4 localToWorld;
    bool selected = false;
    bool hidden = false;
};

struct PacketCadAssembly {
    SceneObjectId objectId = InvalidSceneObjectId;
    Matrix4 localToWorld;
    CadDrawMode drawMode = CadDrawMode::Wireframe;
    CadPickMode pickMode = CadPickMode::Auto;
    float edgePickTolerance = 5.0f;
    bool wireframeOcclusion = true;
    bool lodEnabled = false;
    std::vector<PacketCadPart> parts;
    std::vector<PacketCadInstance> instances;
};

OBOL_V2_API bool collectPacketCadAssemblies(
    const ScenePacket & packet,
    std::vector<PacketCadAssembly> & assemblies,
    std::vector<PacketGeometryDiagnostic> * diagnostics = nullptr);

} // namespace obol

#endif // OBOL_SCENE_SCENE_PACKET_CAD_H
