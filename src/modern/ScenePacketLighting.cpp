#include <Obol/scene/ScenePacketLighting.h>

#include <cmath>

namespace obol {
namespace {

float lightLength(Vec3 v)
{
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

Vec3 lightNormalize(Vec3 v, Vec3 fallback = {0.0f, 0.0f, -1.0f})
{
    const float len = lightLength(v);
    if (len <= 1.0e-8f) {
        return fallback;
    }
    return {v.x / len, v.y / len, v.z / len};
}

Vec3 lightTransformPoint(const Matrix4 & matrix, Vec3 point)
{
    return {
        matrix.values[0] * point.x + matrix.values[4] * point.y +
            matrix.values[8] * point.z + matrix.values[12],
        matrix.values[1] * point.x + matrix.values[5] * point.y +
            matrix.values[9] * point.z + matrix.values[13],
        matrix.values[2] * point.x + matrix.values[6] * point.y +
            matrix.values[10] * point.z + matrix.values[14]};
}

Vec3 lightTransformDirection(const Matrix4 & matrix, Vec3 direction)
{
    return {
        matrix.values[0] * direction.x + matrix.values[4] * direction.y +
            matrix.values[8] * direction.z,
        matrix.values[1] * direction.x + matrix.values[5] * direction.y +
            matrix.values[9] * direction.z,
        matrix.values[2] * direction.x + matrix.values[6] * direction.y +
            matrix.values[10] * direction.z};
}

void addLightDiagnostic(std::vector<PacketGeometryDiagnostic> * diagnostics,
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

bool collectPacketLights(const ScenePacket & packet,
                         std::vector<PacketLight> & lights,
                         std::vector<PacketGeometryDiagnostic> * diagnostics)
{
    bool ok = true;
    for (const SceneObjectRecord & object : packet.objects) {
        if (object.type == SceneObjectType::DirectionalLight) {
            PacketLight light;
            light.objectId = object.id;
            light.kind = PacketLightKind::Directional;
            light.color = object.directionalLight.color;
            light.intensity = object.directionalLight.intensity;
            light.direction = lightNormalize(
                lightTransformDirection(object.localToWorld,
                                        object.directionalLight.direction));
            if (light.intensity < 0.0f) {
                addLightDiagnostic(diagnostics,
                                   PacketGeometryDiagnosticSeverity::Warning,
                                   object.id,
                                   "directional light has negative intensity");
                ok = false;
            }
            lights.push_back(light);
        } else if (object.type == SceneObjectType::PointLight) {
            PacketLight light;
            light.objectId = object.id;
            light.kind = PacketLightKind::Point;
            light.color = object.pointLight.color;
            light.intensity = object.pointLight.intensity;
            light.position =
                lightTransformPoint(object.localToWorld,
                                    object.pointLight.location);
            if (light.intensity < 0.0f) {
                addLightDiagnostic(diagnostics,
                                   PacketGeometryDiagnosticSeverity::Warning,
                                   object.id,
                                   "point light has negative intensity");
                ok = false;
            }
            lights.push_back(light);
        } else if (object.type == SceneObjectType::SpotLight) {
            PacketLight light;
            light.objectId = object.id;
            light.kind = PacketLightKind::Spot;
            light.color = object.spotLight.color;
            light.intensity = object.spotLight.intensity;
            light.position =
                lightTransformPoint(object.localToWorld,
                                    object.spotLight.location);
            light.direction = lightNormalize(
                lightTransformDirection(object.localToWorld,
                                        object.spotLight.direction));
            light.cutOffAngleRadians = object.spotLight.cutOffAngleRadians;
            light.dropOffRate = object.spotLight.dropOffRate;
            if (light.intensity < 0.0f) {
                addLightDiagnostic(diagnostics,
                                   PacketGeometryDiagnosticSeverity::Warning,
                                   object.id,
                                   "spot light has negative intensity");
                ok = false;
            }
            lights.push_back(light);
        }
    }
    return ok;
}

} // namespace obol
