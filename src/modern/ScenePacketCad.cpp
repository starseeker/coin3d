#include <Obol/scene/ScenePacketCad.h>

#include <algorithm>

namespace obol {
namespace {

Matrix4 cadToMatrix4(const CadMatrix4 & matrix)
{
    Matrix4 result;
    result.values = matrix.values;
    return result;
}

Matrix4 multiplyMatrix4(const Matrix4 & lhs, const Matrix4 & rhs)
{
    Matrix4 result;
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            float value = 0.0f;
            for (int k = 0; k < 4; ++k) {
                value += lhs.values[static_cast<size_t>(k) * 4 +
                                    static_cast<size_t>(row)] *
                         rhs.values[static_cast<size_t>(column) * 4 +
                                    static_cast<size_t>(k)];
            }
            result.values[static_cast<size_t>(column) * 4 +
                          static_cast<size_t>(row)] = value;
        }
    }
    return result;
}

bool containsId(const std::vector<InstanceId> & ids, InstanceId id)
{
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

void addCadDiagnostic(std::vector<PacketGeometryDiagnostic> * diagnostics,
                      PacketGeometryDiagnosticSeverity severity,
                      SceneObjectId objectId,
                      const char * message)
{
    if (!diagnostics) {
        return;
    }
    PacketGeometryDiagnostic diagnostic;
    diagnostic.severity = severity;
    diagnostic.objectId = objectId;
    diagnostic.message = message ? message : "";
    diagnostics->push_back(diagnostic);
}

} // namespace

bool collectPacketCadAssemblies(
    const ScenePacket & packet,
    std::vector<PacketCadAssembly> & assemblies,
    std::vector<PacketGeometryDiagnostic> * diagnostics)
{
    bool ok = true;
    for (const SceneObjectRecord & object : packet.objects) {
        if (object.type != SceneObjectType::CadAssembly) {
            continue;
        }
        if (!object.cadAssembly) {
            addCadDiagnostic(diagnostics,
                             PacketGeometryDiagnosticSeverity::Error,
                             object.id,
                             "CAD packet object has no assembly payload");
            ok = false;
            continue;
        }

        const CadAssembly & cad = *object.cadAssembly;
        PacketCadAssembly assembly;
        assembly.objectId = object.id;
        assembly.localToWorld = object.localToWorld;
        assembly.drawMode = cad.drawMode();
        assembly.pickMode = cad.pickMode();
        assembly.edgePickTolerance = cad.edgePickTolerance();
        assembly.wireframeOcclusion = cad.wireframeOcclusion();
        assembly.lodEnabled = cad.lodEnabled();

        for (PartId partId : cad.partIds()) {
            const CadPartGeometry * geometry = cad.partGeometry(partId);
            if (!geometry) {
                addCadDiagnostic(diagnostics,
                                 PacketGeometryDiagnosticSeverity::Warning,
                                 object.id,
                                 "CAD part id was listed but no geometry was available");
                ok = false;
                continue;
            }
            PacketCadPart part;
            part.objectId = object.id;
            part.partId = partId;
            part.geometry = *geometry;
            assembly.parts.push_back(part);
        }

        const std::vector<InstanceId> & selected = cad.selectedInstances();
        const std::vector<InstanceId> & hidden = cad.hiddenInstances();
        for (InstanceId instanceId : cad.instanceIds()) {
            const std::optional<CadInstanceRecord> record =
                cad.getInstanceRecord(instanceId);
            if (!record) {
                addCadDiagnostic(diagnostics,
                                 PacketGeometryDiagnosticSeverity::Warning,
                                 object.id,
                                 "CAD instance id was listed but no record was available");
                ok = false;
                continue;
            }
            if (!cad.partGeometry(record->part)) {
                addCadDiagnostic(diagnostics,
                                 PacketGeometryDiagnosticSeverity::Warning,
                                 object.id,
                                 "CAD instance references a missing part");
                ok = false;
            }

            PacketCadInstance instance;
            instance.objectId = object.id;
            instance.instanceId = instanceId;
            instance.partId = record->part;
            instance.parent = record->parent;
            instance.childName = record->childName;
            instance.occurrenceIndex = record->occurrenceIndex;
            instance.boolOp = record->boolOp;
            instance.style = record->style;
            instance.localToWorld =
                multiplyMatrix4(object.localToWorld,
                                cadToMatrix4(record->localToRoot));
            instance.selected = containsId(selected, instanceId);
            instance.hidden = containsId(hidden, instanceId);
            assembly.instances.push_back(instance);
        }

        assemblies.push_back(assembly);
    }
    return ok;
}

} // namespace obol
