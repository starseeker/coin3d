#include <Obol/scene/ScenePacketExtraction.h>

namespace obol {
namespace {

void addExtractionDiagnostic(std::vector<PacketGeometryDiagnostic> & diagnostics,
                             PacketGeometryDiagnosticSeverity severity,
                             SceneObjectId objectId,
                             const char * message)
{
    PacketGeometryDiagnostic diagnostic;
    diagnostic.severity = severity;
    diagnostic.objectId = objectId;
    diagnostic.message = message ? message : "";
    diagnostics.push_back(diagnostic);
}

bool appendContentDiagnostics(const ScenePacket & packet,
                              const PacketSceneExtractionOptions & options,
                              std::vector<PacketGeometryDiagnostic> & diagnostics)
{
    bool complete = true;

    if (packet.hasLegacyFallbackRoot) {
        addExtractionDiagnostic(
            diagnostics,
            PacketGeometryDiagnosticSeverity::Warning,
            InvalidSceneObjectId,
            "scene packet has a legacy fallback root that requires a compatibility backend path");
        complete = false;
    }

    for (const SceneObjectRecord & object : packet.objects) {
        switch (object.type) {
        case SceneObjectType::Primitive:
        case SceneObjectType::Mesh:
            if (!options.includeTriangles) {
                addExtractionDiagnostic(
                    diagnostics,
                    PacketGeometryDiagnosticSeverity::Warning,
                    object.id,
                    "portable triangle geometry was skipped by extraction options");
                complete = false;
            }
            break;
        case SceneObjectType::Polyline:
            if (!options.includeLineSegments) {
                addExtractionDiagnostic(
                    diagnostics,
                    PacketGeometryDiagnosticSeverity::Warning,
                    object.id,
                    "portable polyline geometry was skipped by extraction options");
                complete = false;
            }
            break;
        case SceneObjectType::PointCloud:
            if (!options.includePoints) {
                addExtractionDiagnostic(
                    diagnostics,
                    PacketGeometryDiagnosticSeverity::Warning,
                    object.id,
                    "portable point geometry was skipped by extraction options");
                complete = false;
            }
            break;
        case SceneObjectType::DirectionalLight:
        case SceneObjectType::PointLight:
        case SceneObjectType::SpotLight:
            if (!options.includeLights) {
                addExtractionDiagnostic(
                    diagnostics,
                    PacketGeometryDiagnosticSeverity::Warning,
                    object.id,
                    "light object was skipped by extraction options");
                complete = false;
            }
            break;
        case SceneObjectType::Text2D:
        case SceneObjectType::Text3D:
            if (!options.includeText) {
                addExtractionDiagnostic(
                    diagnostics,
                    PacketGeometryDiagnosticSeverity::Warning,
                    object.id,
                    "text object was skipped by extraction options");
                complete = false;
            }
            break;
        case SceneObjectType::CadAssembly:
            if (!options.includeCadAssemblies) {
                addExtractionDiagnostic(
                    diagnostics,
                    PacketGeometryDiagnosticSeverity::Warning,
                    object.id,
                    "CAD assembly was skipped by extraction options");
                complete = false;
            }
            break;
        case SceneObjectType::OpenGLCallback:
            addExtractionDiagnostic(
                diagnostics,
                PacketGeometryDiagnosticSeverity::Warning,
                object.id,
                "OpenGL callback objects require an OpenGL backend path");
            complete = false;
            break;
        case SceneObjectType::LegacySceneGraph:
            addExtractionDiagnostic(
                diagnostics,
                PacketGeometryDiagnosticSeverity::Warning,
                object.id,
                "legacy scene graph objects require a compatibility backend path");
            complete = false;
            break;
        case SceneObjectType::Any:
            addExtractionDiagnostic(
                diagnostics,
                PacketGeometryDiagnosticSeverity::Warning,
                object.id,
                "unknown packet object type cannot be extracted");
            complete = false;
            break;
        }
    }

    return complete;
}

} // namespace

bool extractPacketScene(const ScenePacket & packet,
                        ExtractedPacketScene & extracted,
                        const PacketSceneExtractionOptions & options)
{
    extracted = ExtractedPacketScene{};
    extracted.support = inspectPacketGeometrySupport(packet, nullptr);

    bool complete =
        appendContentDiagnostics(packet, options, extracted.diagnostics);

    if (options.includeTriangles) {
        complete = collectPacketTriangles(packet,
                                          extracted.triangles,
                                          &extracted.diagnostics,
                                          options.triangleOptions) &&
                   complete;
    }
    if (options.includeLineSegments) {
        complete = collectPacketLineSegments(packet,
                                             extracted.lineSegments,
                                             &extracted.diagnostics) &&
                   complete;
    }
    if (options.includePoints) {
        complete = collectPacketPoints(packet,
                                       extracted.points,
                                       &extracted.diagnostics) &&
                   complete;
    }
    if (options.includeLights) {
        complete = collectPacketLights(packet,
                                       extracted.lights,
                                       &extracted.diagnostics) &&
                   complete;
    }
    if (options.includeText) {
        complete = collectPacketText(packet,
                                     extracted.text,
                                     &extracted.diagnostics) &&
                   complete;
    }
    if (options.includeCadAssemblies) {
        complete = collectPacketCadAssemblies(packet,
                                              extracted.cadAssemblies,
                                              &extracted.diagnostics) &&
                   complete;
    }

    extracted.complete = complete;
    return complete;
}

} // namespace obol
