#include <Obol/scene/ScenePacketText.h>

namespace obol {
namespace {

Vec3 textTransformPoint(const Matrix4 & matrix, Vec3 point)
{
    return {
        matrix.values[0] * point.x + matrix.values[4] * point.y +
            matrix.values[8] * point.z + matrix.values[12],
        matrix.values[1] * point.x + matrix.values[5] * point.y +
            matrix.values[9] * point.z + matrix.values[13],
        matrix.values[2] * point.x + matrix.values[6] * point.y +
            matrix.values[10] * point.z + matrix.values[14]};
}

void addTextDiagnostic(std::vector<PacketGeometryDiagnostic> * diagnostics,
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

bool collectPacketText(const ScenePacket & packet,
                       std::vector<PacketText> & text,
                       std::vector<PacketGeometryDiagnostic> * diagnostics)
{
    bool ok = true;
    for (const SceneObjectRecord & object : packet.objects) {
        if (object.type == SceneObjectType::Text2D) {
            PacketText record;
            record.objectId = object.id;
            record.kind = PacketTextKind::Text2D;
            record.material = object.material;
            record.localToWorld = object.localToWorld;
            record.origin = textTransformPoint(object.localToWorld,
                                               {0.0f, 0.0f, 0.0f});
            record.text = object.text2D.text;
            record.fontName = object.text2D.fontName;
            record.fontSize = object.text2D.fontSize;
            record.spacing = object.text2D.spacing;
            record.justification = object.text2D.justification;
            record.depthTest = object.text2D.depthTest;
            if (record.text.empty()) {
                addTextDiagnostic(diagnostics,
                                  PacketGeometryDiagnosticSeverity::Warning,
                                  object.id,
                                  "Text2D object has an empty string");
                ok = false;
            }
            text.push_back(record);
        } else if (object.type == SceneObjectType::Text3D) {
            PacketText record;
            record.objectId = object.id;
            record.kind = PacketTextKind::Text3D;
            record.material = object.material;
            record.localToWorld = object.localToWorld;
            record.origin = textTransformPoint(object.localToWorld,
                                               {0.0f, 0.0f, 0.0f});
            record.text = object.text3D.text;
            record.fontName = object.text3D.fontName;
            record.fontSize = object.text3D.fontSize;
            record.spacing = object.text3D.spacing;
            record.justification = object.text3D.justification;
            record.parts = object.text3D.parts;
            record.partColors = object.text3D.partColors;
            record.profile = object.text3D.profile;
            if (record.text.empty()) {
                addTextDiagnostic(diagnostics,
                                  PacketGeometryDiagnosticSeverity::Warning,
                                  object.id,
                                  "Text3D object has an empty string");
                ok = false;
            }
            text.push_back(record);
        }
    }
    return ok;
}

} // namespace obol
