/* View-local subpixel proxy rendering and hysteresis regression test. */

#include "headless_utils.h"
#include "cad/CadFramePlan.h"

#include <Obol/cad/CadProjectedProxy.h>
#include <Obol/cad/SoCADAssembly.h>
#include <Obol/cad/SoCADViewState.h>
#include <Obol/cad/CadIds.h>

#include <Inventor/SbViewportRegion.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoSeparator.h>

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

template <typename Result>
void
requireCadMutation(const Result& result, const char *operation)
{
    if (!result)
        throw std::runtime_error(std::string("CAD mutation failed: ") +
            operation);
}

template <typename Result>
Result
requireCadValue(Result result, const char *operation)
{
    requireCadMutation(result, operation);
    return result;
}

Obol::CadGeometryValidation
admitAndUpsertPart(SoCADAssembly *assembly, Obol::PartId part,
    Obol::PartGeometryBuilder geometry)
{
    const Obol::CadGeometryAdmission admission =
        Obol::cadAdmitPartGeometry(std::move(geometry));
    if (!admission)
        return admission.validation;
    return assembly->upsertParts({{part, admission.geometry}});
}

SoCADViewState *
cadViewState(SoSeparator *root)
{
    if (!root)
        throw std::invalid_argument("CAD test scene root is null");
    for (int index = 0; index < root->getNumChildren(); ++index) {
        SoNode *child = root->getChild(index);
        if (child && child->isOfType(SoCADViewState::getClassTypeId()))
            return static_cast<SoCADViewState *>(child);
    }
    SoCADViewState *viewState = new SoCADViewState;
    viewState->viewIdLow.setValue(1);
    root->addChild(viewState);
    return viewState;
}

void
setCadDrawMode(SoSeparator *root, SoCADViewState::DrawMode mode)
{
    cadViewState(root)->drawMode.setValue(mode);
}

void
setTestEnvironment(const char *name, const char *value, int overwrite)
{
#ifdef _WIN32
    if (!overwrite && std::getenv(name))
        return;
    const int result = _putenv_s(name, value);
#else
    const int result = ::setenv(name, value, overwrite);
#endif
    if (result != 0)
        throw std::runtime_error(
            std::string("cannot set test environment variable ") + name);
}

void
unsetTestEnvironment(const char *name)
{
#ifdef _WIN32
    const int result = _putenv_s(name, "");
#else
    const int result = ::unsetenv(name);
#endif
    if (result != 0)
        throw std::runtime_error(
            std::string("cannot unset test environment variable ") + name);
}

Obol::WireRep unitBox();
bool render(SoOffscreenRenderer& renderer, SoSeparator *root);
size_t nonBlackPixels(const SoOffscreenRenderer& renderer);

bool
sharedProjectedProxyContract()
{
    const SbVec3f corners[8] = {
        SbVec3f(-0.002f, -0.002f, -0.002f),
        SbVec3f( 0.002f, -0.002f, -0.002f),
        SbVec3f(-0.002f,  0.002f, -0.002f),
        SbVec3f( 0.002f,  0.002f, -0.002f),
        SbVec3f(-0.002f, -0.002f,  0.002f),
        SbVec3f( 0.002f, -0.002f,  0.002f),
        SbVec3f(-0.002f,  0.002f,  0.002f),
        SbVec3f( 0.002f,  0.002f,  0.002f)
    };
    SbMatrix identity;
    identity.makeIdentity();
    const SbVec2s viewport(256, 256);

    const Obol::CadProjectedProxy centered =
        Obol::classifyCadProjectedProxy(
            corners, identity, identity, viewport, 1.0f);
    if (!centered.visible || !centered.fullyContained ||
            !centered.pointEligible || centered.pixelWidth <= 0.0f ||
            centered.pixelHeight <= 0.0f)
        return false;

    /* A subpixel proxy straddling a clip plane remains visible but must not
     * collapse to a point.  This is the planner/renderer edge contract which
     * prevents a point request from leaving the renderer's structural box in
     * place indefinitely. */
    SbMatrix edge;
    edge.setTranslate(SbVec3f(1.0f, 0.0f, 0.0f));
    const Obol::CadProjectedProxy clipped =
        Obol::classifyCadProjectedProxy(
            corners, edge, identity, viewport, 1.0f);
    if (!clipped.visible || clipped.fullyContained || clipped.pointEligible)
        return false;

    SbMatrix outside;
    outside.setTranslate(SbVec3f(1.01f, 0.0f, 0.0f));
    const Obol::CadProjectedProxy rejected =
        Obol::classifyCadProjectedProxy(
            corners, outside, identity, viewport, 1.0f);
    if (rejected.visible || rejected.pointEligible)
        return false;

    const Obol::CadProjectedProxy invalidViewport =
        Obol::classifyCadProjectedProxy(
            corners, identity, identity, SbVec2s(1, 256), 1.0f);
    return !invalidViewport.visible && !invalidViewport.pointEligible;
}

bool
degenerateStructuralProxyContract()
{
    Obol::PartGeometryBuilder geometry;
    geometry.wire = unitBox();
    for (SbVec3f& point : geometry.wire->segmentPoints)
        point[2] = 0.0f;
    geometry.wire->bounds = SbBox3f(
        SbVec3f(-0.5f, -0.5f, 0.0f),
        SbVec3f(0.5f, 0.5f, 0.0f));
    geometry.subpixelProxyEligible = true;
    geometry.structuralProxy = true;

    SbVec3f corners[8];
    if (!Obol::cadPartGeometryProxyCorners(geometry, corners))
        return false;
    for (const SbVec3f& corner : corners) {
        if (corner[2] != 0.0f)
            return false;
    }
    return true;
}

std::array<SbVec3f, 8>
orientedProxyCorners()
{
    constexpr float cosine = 0.8660254038f;
    constexpr float sine = 0.5f;
    constexpr float halfLength = 2.0f;
    constexpr float halfWidth = 0.25f;
    constexpr float halfHeight = 0.2f;
    std::array<SbVec3f, 8> corners;
    for (size_t corner = 0; corner < corners.size(); ++corner) {
        const float localX = (corner & 1u) ? halfLength : -halfLength;
        const float localY = (corner & 2u) ? halfWidth : -halfWidth;
        corners[corner].setValue(
            cosine * localX - sine * localY,
            sine * localX + cosine * localY,
            (corner & 4u) ? halfHeight : -halfHeight);
    }
    return corners;
}

Obol::WireRep
orientedProxyWire(const std::array<SbVec3f, 8>& corners)
{
    Obol::internal::CadSubpixelProxyPoint proxy;
    proxy.boxCorners = corners;
    proxy.boxCornersValid = true;
    Obol::WireRep wire;
    wire.bounds.makeEmpty();
    Obol::internal::cadForEachAggregateProxyBoxVertex(
        proxy, [&wire](const SbVec3f& point) {
            wire.segmentPoints.push_back(point);
            wire.bounds.extendBy(point);
        });
    return wire;
}

bool
orientedAggregateProxyContract()
{
    const std::array<SbVec3f, 8> corners = orientedProxyCorners();
    Obol::PartGeometryBuilder geometry;
    geometry.wire = orientedProxyWire(corners);
    Obol::TriMesh shaded;
    shaded.positions = {
        corners[0], corners[1], corners[2], corners[4]
    };
    shaded.indices = {
        0u, 2u, 1u,
        0u, 1u, 3u,
        0u, 3u, 2u,
        1u, 2u, 3u
    };
    shaded.bounds.makeEmpty();
    for (const SbVec3f& point : shaded.positions)
        shaded.bounds.extendBy(point);
    geometry.shaded = std::move(shaded);
    geometry.shadedCullBackfaces = false;
    geometry.aggregateProxyCorners = corners;
    geometry.subpixelProxyEligible = true;

    const Obol::CadGeometryAdmission admitted =
        Obol::cadAdmitPartGeometry(geometry);
    if (!admitted || !admitted.geometry ||
            !admitted.geometry.get()->aggregateProxyCorners) {
        std::fprintf(stderr, "oriented aggregate proxy admission failed: %s\n",
            Obol::cadGeometryErrorName(admitted.validation.error));
        return false;
    }
    SbVec3f admittedCorners[8];
    if (!Obol::cadPartGeometryProxyCorners(
            *admitted.geometry.get(), admittedCorners)) {
        std::fprintf(stderr, "oriented aggregate proxy corners unavailable\n");
        return false;
    }
    for (size_t corner = 0; corner < corners.size(); ++corner)
        if (!admittedCorners[corner].equals(corners[corner], 1.0e-6f)) {
            std::fprintf(stderr,
                "oriented aggregate proxy corner changed during admission\n");
            return false;
        }

    Obol::PartGeometryBuilder malformed = geometry;
    (*malformed.aggregateProxyCorners)[7] += SbVec3f(0.25f, 0.0f, 0.0f);
    const Obol::CadGeometryAdmission rejected =
        Obol::cadAdmitPartGeometry(std::move(malformed));
    if (rejected || rejected.validation.error !=
            Obol::CadGeometryError::InvalidAggregateProxy) {
        std::fprintf(stderr,
            "malformed oriented aggregate proxy had result %s\n",
            Obol::cadGeometryErrorName(rejected.validation.error));
        return false;
    }

    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 5.0f);
    camera->nearDistance.setValue(0.1f);
    camera->farDistance.setValue(100.0f);
    camera->height.setValue(20.0f);
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);
    SoCADAssembly *assembly = new SoCADAssembly;
    setCadDrawMode(root, SoCADViewState::SHADED);
    cadViewState(root)->pointProxyPixelThreshold.setValue(64.0f);
    root->addChild(assembly);

    const Obol::PartId part =
        Obol::CadIdBuilder::partId("oriented-aggregate-proxy");
    requireCadMutation(admitAndUpsertPart(assembly, part, geometry),
        "oriented aggregate proxy part");
    Obol::InstanceRecord instance;
    instance.part = part;
    instance.parent = Obol::CadIdBuilder::rootInstance();
    instance.childName = "oriented-aggregate-proxy";
    instance.localToRoot.setTranslate(SbVec3f(0.5f, -0.25f, 0.0f));
    requireCadMutation(assembly->upsertInstanceAuto(instance),
        "oriented aggregate proxy instance");

    const SbViewportRegion viewport(256, 256);
    SoOffscreenRenderer renderer(viewport);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
    bool passed = render(renderer, root);
    const Obol::CadRenderedWork shadedWork =
        assembly->lastRenderedWork();
    const Obol::CadAggregateProxyPresentationWork shadedProxies =
        assembly->lastAggregateProxyPresentationWork();
    passed = passed &&
        assembly->lastSubpixelProxyCount() == 1u &&
        assembly->lastSubpixelProxyDrawPointCount() == 0u &&
        shadedProxies.exact && shadedProxies.pointCount == 0u &&
        shadedProxies.axisAlignedBoxCount == 0u &&
        shadedProxies.orientedBoxCount == 1u &&
        shadedWork.triangleCount ==
            Obol::CadAggregateProxyBoxTriangleCount &&
        shadedWork.lineCount == 0u &&
        nonBlackPixels(renderer) > 0u;
    setCadDrawMode(root, SoCADViewState::WIREFRAME);
    const bool wireRendered = render(renderer, root);
    const Obol::CadRenderedWork wireWork =
        assembly->lastRenderedWork();
    const Obol::CadAggregateProxyPresentationWork wireProxies =
        assembly->lastAggregateProxyPresentationWork();
    passed = passed && wireRendered &&
        assembly->lastSubpixelProxyCount() == 1u &&
        assembly->lastSubpixelProxyDrawPointCount() == 0u &&
        wireProxies.exact && wireProxies.pointCount == 0u &&
        wireProxies.axisAlignedBoxCount == 0u &&
        wireProxies.orientedBoxCount == 1u &&
        wireWork.triangleCount == 0u &&
        wireWork.lineCount ==
            Obol::CadAggregateProxyBoxLineCount &&
        nonBlackPixels(renderer) > 0u;
    if (!passed) {
        const Obol::CadRenderedWork work = assembly->lastRenderedWork();
        std::fprintf(stderr,
            "oriented aggregate proxy render failed "
            "(logical=%zu points=%zu triangles=%llu lines=%llu pixels=%zu)\n",
            assembly->lastSubpixelProxyCount(),
            assembly->lastSubpixelProxyDrawPointCount(),
            static_cast<unsigned long long>(work.triangleCount),
            static_cast<unsigned long long>(work.lineCount),
            nonBlackPixels(renderer));
    }
    root->unref();
    return passed;
}

void
setProgressiveCuts(Obol::TriMesh& mesh, size_t cutCount,
                   uint32_t indexCount, uint32_t positionCount)
{
    mesh.progressiveCuts.resize(cutCount);
    for (Obol::ProgressiveTriangleCut& cut : mesh.progressiveCuts) {
        cut.indexCount = indexCount;
        cut.positionCount = positionCount;
    }
}

bool
sparseUniformClusterContract()
{
    Obol::TriMesh mesh;
    mesh.positions = {
        SbVec3f(0.0f, 0.0f, 0.0f),
        SbVec3f(1.0f, 0.0f, 0.0f),
        SbVec3f(0.0f, 1.0f, 0.0f)};
    mesh.indices = {0, 1, 2};
    mesh.progressiveMinimumCut = 0;
    mesh.progressiveResidentCut = 0;
    setProgressiveCuts(mesh, 1, 3, 3);
    mesh.progressiveClusterGridResolution = 8;
    mesh.progressiveClusters.resize(2);
    if (!mesh.hasProgressiveClusters() ||
            mesh.hasAdaptiveProgressiveClusters())
        return false;
    mesh.progressiveClusters.resize(513);
    if (mesh.hasProgressiveClusters())
        return false;

    Obol::WireRep wire;
    wire.segmentPoints = {
        SbVec3f(0.0f, 0.0f, 0.0f),
        SbVec3f(1.0f, 0.0f, 0.0f)};
    wire.progressiveCuts.resize(1);
    wire.progressiveCuts[0].segmentCount = 1;
    wire.progressiveMinimumCut = 0;
    wire.progressiveResidentCut = 0;
    wire.progressiveClusterGridResolution = 8;
    wire.progressiveClusters.resize(2);
    return wire.hasProgressiveClusters() &&
        !wire.hasAdaptiveProgressiveClusters();
}

Obol::WireRep
unitBox()
{
    static const int edges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };
    const SbVec3f corners[8] = {
        SbVec3f(-0.5f, -0.5f, -0.5f), SbVec3f(0.5f, -0.5f, -0.5f),
        SbVec3f(0.5f, 0.5f, -0.5f), SbVec3f(-0.5f, 0.5f, -0.5f),
        SbVec3f(-0.5f, -0.5f, 0.5f), SbVec3f(0.5f, -0.5f, 0.5f),
        SbVec3f(0.5f, 0.5f, 0.5f), SbVec3f(-0.5f, 0.5f, 0.5f)
    };
    Obol::WireRep wire;
    for (const auto &edge : edges) {
        wire.segmentPoints.push_back(corners[edge[0]]);
        wire.segmentPoints.push_back(corners[edge[1]]);
    }
    wire.bounds = SbBox3f(corners[0], corners[6]);
    return wire;
}

bool
render(SoOffscreenRenderer &renderer, SoSeparator *root)
{
    return renderer.render(root) == TRUE && renderer.getBuffer() != nullptr;
}

size_t
nonBlackPixels(const SoOffscreenRenderer &renderer)
{
    const unsigned char *buffer = renderer.getBuffer();
    const SbVec2s size = renderer.getViewportRegion().getViewportSizePixels();
    if (!buffer || size[0] <= 0 || size[1] <= 0)
        return 0;

    size_t count = 0;
    const size_t pixelCount = static_cast<size_t>(size[0]) *
        static_cast<size_t>(size[1]);
    for (size_t i = 0; i < pixelCount; ++i) {
        const unsigned char *pixel = buffer + (i * 3u);
        if (pixel[0] || pixel[1] || pixel[2])
            ++count;
    }
    return count;
}

bool
softwareSubpixelProxyAggregationContract()
{
    /*
     * This intentionally uses many independently retained occurrences of
     * one tiny structural part.  Logical coverage must remain one proxy per
     * occurrence, while the OSMesa executor is allowed only a bounded
     * camera-local point stream.  Keep this outside the broader rendering
     * scenario below so a failure cannot be hidden by its mesh work.
     */
    static constexpr uint32_t occurrenceCount = 8192u;
    static constexpr uint32_t gridWidth = 128u;
    static constexpr float gridSpacing = 4.0f;
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 5.0f);
    camera->nearDistance.setValue(0.1f);
    camera->farDistance.setValue(100.0f);
    camera->height.setValue(1000.0f);
    root->addChild(camera);

    SoCADAssembly *assembly = new SoCADAssembly;
    setCadDrawMode(root, SoCADViewState::WIREFRAME);
    root->addChild(assembly);

    Obol::PartGeometryBuilder geometry;
    geometry.wire = unitBox();
    geometry.subpixelProxyEligible = true;
    geometry.structuralProxy = true;
    const Obol::PartId part =
        Obol::CadIdBuilder::partId("software-subpixel-proxy-part");
    requireCadMutation(admitAndUpsertPart(assembly, part, geometry),
        "subpixel proxy part");

    std::vector<Obol::InstanceUpdate> updates;
    updates.reserve(occurrenceCount);
    for (uint32_t index = 0; index < occurrenceCount; ++index) {
        Obol::InstanceRecord instance;
        instance.part = part;
        instance.parent = Obol::CadIdBuilder::rootInstance();
        instance.childName = "software-subpixel-proxy";
        instance.occurrenceIndex = index;
        instance.lodStructuralProxy = true;
        instance.localToRoot.setTranslate(SbVec3f(
            (static_cast<float>(index % gridWidth) -
                static_cast<float>(gridWidth) * 0.5f) * gridSpacing,
            (static_cast<float>(index / gridWidth) -
                static_cast<float>(occurrenceCount / gridWidth) * 0.5f) *
                gridSpacing,
            0.0f));
        Obol::InstanceUpdate update;
        update.instance = Obol::CadIdBuilder::childInstance(
            instance.parent, instance.childName, instance.occurrenceIndex,
            instance.boolOp);
        update.record = instance;
        updates.push_back(std::move(update));
    }
    requireCadMutation(assembly->upsertInstances(updates),
        "subpixel proxy instances");

    const SbViewportRegion viewport(256, 256);
    SoOffscreenRenderer renderer(viewport);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
    const bool rendered = render(renderer, root);
    const size_t logicalCount = assembly->lastSubpixelProxyCount();
    const size_t drawCount = assembly->lastSubpixelProxyDrawPointCount();
    root->unref();
    if (!rendered || logicalCount != occurrenceCount || !drawCount ||
            drawCount > logicalCount) {
        std::fprintf(stderr,
            "software subpixel aggregation did not preserve logical "
            "coverage or bound point submission (%zu logical, %zu draw)\n",
            logicalCount, drawCount);
        return false;
    }
    return true;
}

struct HalfImageStats {
    double leftMean = 0.0;
    double rightMean = 0.0;
    size_t leftPixels = 0;
    size_t rightPixels = 0;
};

HalfImageStats
foregroundHalfStats(const SoOffscreenRenderer &renderer)
{
    HalfImageStats result;
    const unsigned char *buffer = renderer.getBuffer();
    const SbVec2s size = renderer.getViewportRegion().getViewportSizePixels();
    if (!buffer || size[0] <= 0 || size[1] <= 0)
        return result;

    double leftSum = 0.0;
    double rightSum = 0.0;
    for (int y = 0; y < size[1]; ++y) {
        for (int x = 0; x < size[0]; ++x) {
            const unsigned char *pixel =
                buffer + (static_cast<size_t>(y) * size[0] + x) * 3u;
            if (!pixel[0] && !pixel[1] && !pixel[2])
                continue;
            const double luma =
                0.2126 * pixel[0] + 0.7152 * pixel[1] + 0.0722 * pixel[2];
            if (x < size[0] / 2) {
                leftSum += luma;
                ++result.leftPixels;
            } else {
                rightSum += luma;
                ++result.rightPixels;
            }
        }
    }
    if (result.leftPixels)
        result.leftMean = leftSum / result.leftPixels;
    if (result.rightPixels)
        result.rightMean = rightSum / result.rightPixels;
    return result;
}

bool
normalFreeTwoSidedGlslMatchesFixed()
{
    const char *previousGlsl = std::getenv("OBOL_CAD_SOFTWARE_GLSL");
    const bool hadPreviousGlsl = previousGlsl != nullptr;
    const std::string previousGlslValue =
        previousGlsl ? std::string(previousGlsl) : std::string();
    const auto restoreGlslEnvironment = [&]() {
        if (hadPreviousGlsl)
            setTestEnvironment("OBOL_CAD_SOFTWARE_GLSL",
                   previousGlslValue.c_str(), 1);
        else
            unsetTestEnvironment("OBOL_CAD_SOFTWARE_GLSL");
    };

    struct RouteResult {
        bool rendered = false;
        HalfImageStats image;
    };
    const auto renderRoute = [](bool softwareGlsl) {
        if (softwareGlsl)
            setTestEnvironment("OBOL_CAD_SOFTWARE_GLSL", "1", 1);
        else
            unsetTestEnvironment("OBOL_CAD_SOFTWARE_GLSL");

        /*
         * Renderer configuration is immutable per assembly.  Construct a
         * distinct scene after selecting each route; changing the environment
         * around repeated renders of one node only retests the cached route.
         */
        SoSeparator *root = new SoSeparator;
        root->ref();
        SoOrthographicCamera *camera = new SoOrthographicCamera;
        camera->position.setValue(0.0f, 0.0f, 5.0f);
        camera->nearDistance.setValue(0.1f);
        camera->farDistance.setValue(100.0f);
        camera->height.setValue(2.4f);
        root->addChild(camera);

        SoDirectionalLight *light = new SoDirectionalLight;
        light->direction.setValue(0.0f, 0.0f, -1.0f);
        root->addChild(light);

        SoCADAssembly *assembly = new SoCADAssembly;
        setCadDrawMode(root, SoCADViewState::SHADED);
        root->addChild(assembly);

        /*
         * Two disjoint, coplanar triangles deliberately use opposite winding
         * and have no authored normal stream, matching normal-free PoP
         * payloads such as Lucy.  Both routes must light both faces equally.
         */
        Obol::TriMesh mesh;
        mesh.positions = {
            SbVec3f(-1.05f, -0.8f, 0.0f),
            SbVec3f(-0.05f, -0.8f, 0.0f),
            SbVec3f(-0.55f, 0.8f, 0.0f),
            SbVec3f(0.05f, -0.8f, 0.0f),
            SbVec3f(1.05f, -0.8f, 0.0f),
            SbVec3f(0.55f, 0.8f, 0.0f)
        };
        mesh.indices = {0, 1, 2, 3, 5, 4};
        mesh.bounds.makeEmpty();
        for (const SbVec3f& point : mesh.positions)
            mesh.bounds.extendBy(point);
        mesh.progressiveMinimumCut = 15;
        mesh.progressiveResidentCut = 15;
        setProgressiveCuts(mesh, 16, 0, 0);
        mesh.progressiveCuts[15].indexCount =
            static_cast<uint32_t>(mesh.indices.size());
        mesh.progressiveCuts[15].positionCount =
            static_cast<uint32_t>(mesh.positions.size());
        mesh.progressiveQuantizationMinimum = mesh.bounds.getMin();
        mesh.progressiveQuantizationMaximum = mesh.bounds.getMax();

        Obol::PartGeometryBuilder geometry;
        geometry.shaded = std::move(mesh);
        geometry.shadedCullBackfaces = false;
        const Obol::PartId part =
            Obol::CadIdBuilder::partId("normal-free-two-sided");
        requireCadMutation(admitAndUpsertPart(assembly, part, geometry),
            "retained proxy part");

        Obol::InstanceRecord instance;
        instance.part = part;
        instance.parent = Obol::CadIdBuilder::rootInstance();
        instance.childName = "normal-free-two-sided";
        instance.localToRoot.makeIdentity();
        instance.lodCut = 15;
        instance.style.hasColorOverride = true;
        instance.style.color = SbColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        requireCadMutation(assembly->upsertInstanceAuto(instance),
            "retained proxy instance");

        const SbViewportRegion viewport(256, 192);
        SoOffscreenRenderer renderer(viewport);
        renderer.setComponents(SoOffscreenRenderer::RGB);
        renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

        RouteResult result;
        result.rendered = render(renderer, root);
        if (result.rendered)
            result.image = foregroundHalfStats(renderer);
        root->unref();
        return result;
    };

    const RouteResult fixed = renderRoute(false);
    const RouteResult glsl = renderRoute(true);
    restoreGlslEnvironment();

    if (!fixed.rendered || !glsl.rendered ||
            fixed.image.leftPixels < 100 ||
            fixed.image.rightPixels < 100 ||
            glsl.image.leftPixels < 100 ||
            glsl.image.rightPixels < 100)
        return false;
    const double minimumLit = 0.75 *
        (std::min)(fixed.image.leftMean, fixed.image.rightMean);
    const double glslLow =
        (std::min)(glsl.image.leftMean, glsl.image.rightMean);
    const double glslHigh =
        (std::max)(glsl.image.leftMean, glsl.image.rightMean);
    const bool matched = glslLow >= minimumLit && glslHigh > 0.0 &&
        glslLow / glslHigh >= 0.9;
    if (!matched) {
        std::fprintf(stderr,
            "two-sided stats fixed={left=%.3f/%zu right=%.3f/%zu} "
            "glsl={left=%.3f/%zu right=%.3f/%zu}\n",
            fixed.image.leftMean, fixed.image.leftPixels,
            fixed.image.rightMean, fixed.image.rightPixels,
            glsl.image.leftMean, glsl.image.leftPixels,
            glsl.image.rightMean, glsl.image.rightPixels);
    }
    return matched;
}

bool
nonUniformNormalTransformMatchesFixed()
{
    const char *previousGlsl = std::getenv("OBOL_CAD_SOFTWARE_GLSL");
    const bool hadPreviousGlsl = previousGlsl != nullptr;
    const std::string previousGlslValue =
        previousGlsl ? std::string(previousGlsl) : std::string();
    const auto restoreGlslEnvironment = [&]() {
        if (hadPreviousGlsl)
            setTestEnvironment("OBOL_CAD_SOFTWARE_GLSL",
                previousGlslValue.c_str(), 1);
        else
            unsetTestEnvironment("OBOL_CAD_SOFTWARE_GLSL");
    };

    struct RouteResult {
        bool rendered = false;
        size_t pixels = 0;
        double mean = 0.0;
    };
    const auto renderRoute = [](bool softwareGlsl) {
        if (softwareGlsl)
            setTestEnvironment("OBOL_CAD_SOFTWARE_GLSL", "1", 1);
        else
            unsetTestEnvironment("OBOL_CAD_SOFTWARE_GLSL");

        SoSeparator *root = new SoSeparator;
        root->ref();
        SoOrthographicCamera *camera = new SoOrthographicCamera;
        camera->position.setValue(0.0f, 0.0f, 5.0f);
        camera->nearDistance.setValue(0.1f);
        camera->farDistance.setValue(100.0f);
        camera->height.setValue(2.4f);
        root->addChild(camera);
        SoDirectionalLight *light = new SoDirectionalLight;
        light->direction.setValue(0.0f, -1.0f, 0.0f);
        root->addChild(light);

        SoCADAssembly *assembly = new SoCADAssembly;
        setCadDrawMode(root, SoCADViewState::SHADED);
        root->addChild(assembly);

        Obol::TriMesh mesh;
        mesh.positions = {
            SbVec3f(-0.25f, -0.8f, 0.0f),
            SbVec3f(0.25f, -0.8f, 0.0f),
            SbVec3f(0.0f, 0.8f, 0.0f)};
        mesh.normals.assign(3, SbVec3f(
            0.70710678f, 0.70710678f, 0.0f));
        mesh.indices = {0, 1, 2};
        mesh.bounds = SbBox3f(
            SbVec3f(-0.25f, -0.8f, 0.0f),
            SbVec3f(0.25f, 0.8f, 0.0f));
        Obol::PartGeometryBuilder geometry;
        geometry.shaded = std::move(mesh);
        const Obol::PartId part =
            Obol::CadIdBuilder::partId("non-uniform-normal-transform");
        requireCadMutation(admitAndUpsertPart(assembly, part, geometry),
            "non-uniform normal part");

        Obol::InstanceRecord instance;
        instance.part = part;
        instance.parent = Obol::CadIdBuilder::rootInstance();
        instance.childName = "non-uniform-normal-transform";
        instance.style.hasColorOverride = true;
        instance.style.color = SbColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        instance.localToRoot.makeIdentity();
        instance.localToRoot[0][0] = 4.0f;
        requireCadMutation(assembly->upsertInstanceAuto(instance),
            "non-uniform normal instance");

        const SbViewportRegion viewport(256, 192);
        SoOffscreenRenderer renderer(viewport);
        renderer.setComponents(SoOffscreenRenderer::RGB);
        renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
        RouteResult result;
        result.rendered = render(renderer, root);
        if (result.rendered) {
            const HalfImageStats stats = foregroundHalfStats(renderer);
            result.pixels = stats.leftPixels + stats.rightPixels;
            if (result.pixels)
                result.mean =
                    (stats.leftMean * stats.leftPixels +
                     stats.rightMean * stats.rightPixels) / result.pixels;
        }
        root->unref();
        return result;
    };

    const RouteResult fixed = renderRoute(false);
    const RouteResult glsl = renderRoute(true);
    restoreGlslEnvironment();
    const bool matched = fixed.rendered && glsl.rendered &&
        fixed.pixels > 500 && glsl.pixels > 500 && fixed.mean > 1.0 &&
        glsl.mean >= fixed.mean * 0.8 && glsl.mean <= fixed.mean * 1.2;
    if (!matched)
        std::fprintf(stderr,
            "non-uniform normal stats fixed={mean=%.3f pixels=%zu} "
            "glsl={mean=%.3f pixels=%zu}\n",
            fixed.mean, fixed.pixels, glsl.mean, glsl.pixels);
    return matched;
}

bool
indirectProgressiveAtlasGrows()
{
    constexpr int partCount = 128;
    constexpr int trianglesPerPart = 300;

    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 10.0f);
    camera->nearDistance.setValue(0.1f);
    camera->farDistance.setValue(100.0f);
    camera->height.setValue(110.0f);
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);

    SoCADAssembly *assembly = new SoCADAssembly;
    setCadDrawMode(root, SoCADViewState::SHADED);
    root->addChild(assembly);

    Obol::TriMesh mesh;
    mesh.positions.reserve(trianglesPerPart * 3u);
    mesh.indices.reserve(trianglesPerPart * 3u);
    mesh.bounds.makeEmpty();
    for (int triangle = 0; triangle < trianglesPerPart; ++triangle) {
        const float x = -0.45f +
            0.06f * static_cast<float>(triangle % 15);
        const float y = -0.45f +
            0.045f * static_cast<float>(triangle / 15);
        const SbVec3f points[3] = {
            SbVec3f(x, y, 0.0f),
            SbVec3f(x + 0.05f, y, 0.0f),
            SbVec3f(x + 0.025f, y + 0.04f, 0.0f)
        };
        for (const SbVec3f& point : points) {
            mesh.indices.push_back(
                static_cast<uint32_t>(mesh.positions.size()));
            mesh.positions.push_back(point);
            mesh.bounds.extendBy(point);
        }
    }
    mesh.progressiveMinimumCut = 0;
    mesh.progressiveResidentCut = 15;
    setProgressiveCuts(mesh, 16, 3u, 3u);
    mesh.progressiveCuts[15].indexCount =
        static_cast<uint32_t>(mesh.indices.size());
    mesh.progressiveCuts[15].positionCount =
        static_cast<uint32_t>(mesh.positions.size());
    mesh.progressiveQuantizationMinimum = mesh.bounds.getMin();
    mesh.progressiveQuantizationMaximum = mesh.bounds.getMax();

    std::vector<Obol::InstanceLodUpdate> richCuts;
    richCuts.reserve(partCount);
    for (int i = 0; i < partCount; ++i) {
        char name[64] = {};
        std::snprintf(name, sizeof(name),
            "progressive-atlas-growth-%03d", i);
        Obol::PartGeometryBuilder geometry;
        geometry.shaded = mesh;
        const Obol::PartId part = Obol::CadIdBuilder::partId(name);
        requireCadMutation(admitAndUpsertPart(assembly, part, geometry),
            "progressive atlas part");

        Obol::InstanceRecord instance;
        instance.part = part;
        instance.parent = Obol::CadIdBuilder::rootInstance();
        instance.childName = name;
        instance.occurrenceIndex = static_cast<uint32_t>(i);
        instance.localToRoot.setTranslate(SbVec3f(
            -44.0f + 8.0f * static_cast<float>(i % 12),
            -44.0f + 8.0f * static_cast<float>(i / 12),
            0.0f));
        instance.lodCut = 0;
        const Obol::InstanceId id =
            requireCadValue(assembly->upsertInstanceAuto(instance),
                "progressive atlas instance").instance;
        richCuts.push_back({id, 15});
    }

    const SbViewportRegion viewport(256, 256);
    SoOffscreenRenderer renderer(viewport);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    const char *previousIndirect = std::getenv("OBOL_CAD_INDIRECT");
    const bool hadPreviousIndirect = previousIndirect != nullptr;
    const std::string previousIndirectValue =
        previousIndirect ? previousIndirect : "";
    setTestEnvironment("OBOL_CAD_INDIRECT", "1", 1);
    const bool coarseRendered = render(renderer, root);
    const int coarseTier = assembly->lastRenderTier();
    const uint64_t coarseTriangles =
        assembly->lastRenderedTriangleCount();

    /*
     * A backend without MDI cannot exercise this GPU-residency contract.
     * Its ordinary progressive path is covered separately.
     */
    bool passed = coarseRendered;
    if (passed && coarseTier == 6) {
        requireCadMutation(assembly->updateInstanceCuts(richCuts),
            "progressive atlas cuts");
        const bool richRendered = render(renderer, root);
        const uint64_t richTriangles =
            assembly->lastRenderedTriangleCount();
        const uint64_t expectedRich =
            static_cast<uint64_t>(partCount) * trianglesPerPart;
        const Obol::CadGpuResourceSnapshot resources =
            assembly->gpuResourceSnapshot();
        passed = richRendered && assembly->lastRenderTier() == 6 &&
            coarseTriangles == static_cast<uint64_t>(partCount) &&
            richTriangles == expectedRich && resources.frameSerial > 0 &&
            resources.triangleAtlasAllocatedBytes > 0 &&
            resources.triangleAtlasLiveBytes > 0 &&
            resources.triangleAtlasLiveBytes <=
                resources.triangleAtlasAllocatedBytes &&
            resources.triangleAtlasPartCount == partCount &&
            resources.triangleAtlasPageCount > 0 &&
            resources.trackedBufferBytes >=
                resources.triangleAtlasAllocatedBytes;
        if (!passed) {
            std::fprintf(stderr,
                "indirect progressive atlas did not grow "
                "(tier=%d coarse=%llu rich=%llu expected=%llu)\n",
                assembly->lastRenderTier(),
                static_cast<unsigned long long>(coarseTriangles),
                static_cast<unsigned long long>(richTriangles),
                static_cast<unsigned long long>(expectedRich));
        }
    }

    if (hadPreviousIndirect)
        setTestEnvironment("OBOL_CAD_INDIRECT",
            previousIndirectValue.c_str(), 1);
    else
        unsetTestEnvironment("OBOL_CAD_INDIRECT");
    root->unref();
    return passed;
}

bool
indirectProgressiveGenerationAppendsSuffix()
{
    const char *previousIndirect = std::getenv("OBOL_CAD_INDIRECT");
    const bool hadPreviousIndirect = previousIndirect != nullptr;
    const std::string previousIndirectValue =
        previousIndirect ? previousIndirect : "";
    setTestEnvironment("OBOL_CAD_INDIRECT", "1", 1);

    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 10.0f);
    camera->nearDistance.setValue(0.1f);
    camera->farDistance.setValue(100.0f);
    camera->height.setValue(4.0f);
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);
    SoCADAssembly *assembly = new SoCADAssembly;
    setCadDrawMode(root, SoCADViewState::SHADED);
    root->addChild(assembly);

    constexpr uint32_t triangleCount = 128u;
    constexpr uint64_t lineage = UINT64_C(0x7155465847454e31);
    Obol::TriMesh rich;
    rich.positions.reserve(triangleCount * 3u);
    rich.indices.reserve(triangleCount * 3u);
    rich.bounds.makeEmpty();
    for (uint32_t triangle = 0; triangle < triangleCount; ++triangle) {
        const float x = -1.6f +
            0.2f * static_cast<float>(triangle % 16u);
        const float y = -1.6f +
            0.2f * static_cast<float>(triangle / 16u);
        const SbVec3f points[3] = {
            SbVec3f(x, y, 0.0f),
            SbVec3f(x + 0.16f, y, 0.0f),
            SbVec3f(x + 0.08f, y + 0.16f, 0.0f)
        };
        for (const SbVec3f& point : points) {
            rich.indices.push_back(
                static_cast<uint32_t>(rich.positions.size()));
            rich.positions.push_back(point);
            rich.bounds.extendBy(point);
        }
    }
    rich.progressiveMinimumCut = 0;
    rich.progressiveResidentCut = 15;
    setProgressiveCuts(rich, 16, 3u, 3u);
    rich.progressiveCuts[15].indexCount =
        static_cast<uint32_t>(rich.indices.size());
    rich.progressiveCuts[15].positionCount =
        static_cast<uint32_t>(rich.positions.size());
    rich.progressiveQuantizationMinimum = rich.bounds.getMin();
    rich.progressiveQuantizationMaximum = rich.bounds.getMax();
    rich.progressiveLineage = lineage;

    Obol::TriMesh coarse = rich;
    coarse.positions.resize(3u);
    coarse.indices.resize(3u);
    coarse.progressiveResidentCut = 0;

    const Obol::PartId part =
        Obol::CadIdBuilder::partId("progressive-generation-suffix");
    std::shared_ptr<Obol::PartGeometryBuilder> coarseGeometry(
        new Obol::PartGeometryBuilder);
    coarseGeometry->shaded = std::move(coarse);
    coarseGeometry->conservativeBounds = rich.bounds;
    requireCadMutation(assembly->upsertParts({{part,
        requireCadValue(Obol::cadAdmitPartGeometry(*coarseGeometry),
            "progressive suffix admission").geometry, false}}),
        "progressive suffix part");

    Obol::InstanceRecord instance;
    instance.part = part;
    instance.parent = Obol::CadIdBuilder::rootInstance();
    instance.childName = "progressive-generation-suffix";
    instance.localToRoot.makeIdentity();
    instance.lodCut = 15;
    requireCadMutation(assembly->upsertInstanceAuto(instance),
        "progressive suffix instance");

    const SbViewportRegion viewport(192, 192);
    SoOffscreenRenderer renderer(viewport);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
    bool passed = render(renderer, root);
    const int initialTier = assembly->lastRenderTier();
    const Obol::CadGpuResourceSnapshot coarseResources =
        assembly->gpuResourceSnapshot();

    if (passed && initialTier == 6) {
        std::shared_ptr<Obol::PartGeometryBuilder> richGeometry(
            new Obol::PartGeometryBuilder);
        richGeometry->shaded = std::move(rich);
        richGeometry->conservativeBounds =
            coarseGeometry->conservativeBounds;
        requireCadMutation(assembly->upsertParts({{part,
            requireCadValue(Obol::cadAdmitPartGeometry(*richGeometry),
                "rich suffix admission").geometry, true}}),
            "rich suffix part");
        passed = render(renderer, root);
        const Obol::CadGpuResourceSnapshot richResources =
            assembly->gpuResourceSnapshot();
        const uint64_t expectedSuffixBytes =
            static_cast<uint64_t>(triangleCount * 3u - 3u) *
                (3u * sizeof(float) + sizeof(uint32_t));
        passed = passed && assembly->lastRenderTier() == 6 &&
            assembly->lastRenderedTriangleCount() == triangleCount &&
            richResources.triangleAtlasFullUploadBytes ==
                coarseResources.triangleAtlasFullUploadBytes &&
            richResources.triangleAtlasSuffixUploadBytes >=
                coarseResources.triangleAtlasSuffixUploadBytes +
                    expectedSuffixBytes &&
            richResources.triangleAtlasLineageReuseCount >
                coarseResources.triangleAtlasLineageReuseCount;
        if (!passed) {
            std::fprintf(stderr,
                "progressive immutable generation did not reuse atlas "
                "prefix (tier=%d triangles=%llu full=%llu/%llu "
                "suffix=%llu/%llu reuse=%llu/%llu)\n",
                assembly->lastRenderTier(),
                static_cast<unsigned long long>(
                    assembly->lastRenderedTriangleCount()),
                static_cast<unsigned long long>(
                    coarseResources.triangleAtlasFullUploadBytes),
                static_cast<unsigned long long>(
                    richResources.triangleAtlasFullUploadBytes),
                static_cast<unsigned long long>(
                    coarseResources.triangleAtlasSuffixUploadBytes),
                static_cast<unsigned long long>(
                    richResources.triangleAtlasSuffixUploadBytes),
                static_cast<unsigned long long>(
                    coarseResources.triangleAtlasLineageReuseCount),
                static_cast<unsigned long long>(
                    richResources.triangleAtlasLineageReuseCount));
        }
    }

    root->unref();
    if (hadPreviousIndirect)
        setTestEnvironment("OBOL_CAD_INDIRECT",
            previousIndirectValue.c_str(), 1);
    else
        unsetTestEnvironment("OBOL_CAD_INDIRECT");
    return passed;
}

bool
ordinaryProgressiveGenerationAppendsSuffix()
{
    const char *previousIndirect = std::getenv("OBOL_CAD_INDIRECT");
    const bool hadPreviousIndirect = previousIndirect != nullptr;
    const std::string previousIndirectValue =
        previousIndirect ? previousIndirect : "";
    setTestEnvironment("OBOL_CAD_INDIRECT", "0", 1);

    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 10.0f);
    camera->nearDistance.setValue(0.1f);
    camera->farDistance.setValue(100.0f);
    camera->height.setValue(4.0f);
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);
    SoCADAssembly *assembly = new SoCADAssembly;
    setCadDrawMode(root, SoCADViewState::SHADED);
    root->addChild(assembly);

    constexpr uint32_t triangleCount = 128u;
    constexpr uint64_t lineage = UINT64_C(0x4f5244494e415259);
    Obol::TriMesh rich;
    rich.positions.reserve(triangleCount * 3u);
    rich.indices.reserve(triangleCount * 3u);
    rich.bounds.makeEmpty();
    for (uint32_t triangle = 0; triangle < triangleCount; ++triangle) {
        const float x = -1.6f +
            0.2f * static_cast<float>(triangle % 16u);
        const float y = -1.6f +
            0.2f * static_cast<float>(triangle / 16u);
        const SbVec3f points[3] = {
            SbVec3f(x, y, 0.0f),
            SbVec3f(x + 0.16f, y, 0.0f),
            SbVec3f(x + 0.08f, y + 0.16f, 0.0f)
        };
        for (const SbVec3f& point : points) {
            rich.indices.push_back(
                static_cast<uint32_t>(rich.positions.size()));
            rich.positions.push_back(point);
            rich.bounds.extendBy(point);
        }
    }
    rich.progressiveMinimumCut = 0;
    rich.progressiveResidentCut = 15;
    setProgressiveCuts(rich, 16, 3u, 3u);
    rich.progressiveCuts[15].indexCount =
        static_cast<uint32_t>(rich.indices.size());
    rich.progressiveCuts[15].positionCount =
        static_cast<uint32_t>(rich.positions.size());
    rich.progressiveQuantizationMinimum = rich.bounds.getMin();
    rich.progressiveQuantizationMaximum = rich.bounds.getMax();
    rich.progressiveLineage = lineage;

    Obol::TriMesh coarse = rich;
    coarse.positions.resize(3u);
    coarse.indices.resize(3u);
    coarse.progressiveResidentCut = 0;
    Obol::TriMesh contracted = coarse;

    const Obol::PartId part =
        Obol::CadIdBuilder::partId("ordinary-progressive-suffix");
    std::shared_ptr<Obol::PartGeometryBuilder> coarseGeometry(
        new Obol::PartGeometryBuilder);
    coarseGeometry->shaded = std::move(coarse);
    coarseGeometry->conservativeBounds = rich.bounds;
    requireCadMutation(assembly->upsertParts({{part,
        requireCadValue(Obol::cadAdmitPartGeometry(*coarseGeometry),
            "ordinary suffix admission").geometry, false}}),
        "ordinary suffix part");

    Obol::InstanceRecord instance;
    instance.part = part;
    instance.parent = Obol::CadIdBuilder::rootInstance();
    instance.childName = "ordinary-progressive-suffix";
    instance.localToRoot.makeIdentity();
    instance.lodCut = 15;
    requireCadMutation(assembly->upsertInstanceAuto(instance),
        "ordinary suffix instance");

    const SbViewportRegion viewport(192, 192);
    SoOffscreenRenderer renderer(viewport);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
    bool passed = render(renderer, root);
    const Obol::CadGpuResourceSnapshot coarseResources =
        assembly->gpuResourceSnapshot();

    if (passed) {
        std::shared_ptr<Obol::PartGeometryBuilder> richGeometry(
            new Obol::PartGeometryBuilder);
        richGeometry->shaded = std::move(rich);
        richGeometry->conservativeBounds =
            coarseGeometry->conservativeBounds;
        requireCadMutation(assembly->upsertParts({{part,
            requireCadValue(Obol::cadAdmitPartGeometry(*richGeometry),
                "ordinary rich admission").geometry, true}}),
            "ordinary rich part");
        passed = render(renderer, root);
        const Obol::CadGpuResourceSnapshot richResources =
            assembly->gpuResourceSnapshot();
        const uint64_t expectedSuffixBytes =
            static_cast<uint64_t>(triangleCount * 3u - 3u) *
                (3u * sizeof(float) + sizeof(uint32_t));
        const uint64_t expectedCopiedBytes =
            3u * (3u * sizeof(float) + sizeof(uint32_t));
        const uint64_t expectedCompleteBytes =
            static_cast<uint64_t>(triangleCount) * 3u *
                (3u * sizeof(float) + sizeof(uint32_t));
        const bool copiedPrefix =
            richResources.ordinaryPartFullUploadBytes ==
                coarseResources.ordinaryPartFullUploadBytes &&
            richResources.ordinaryPartSuffixUploadBytes >=
                coarseResources.ordinaryPartSuffixUploadBytes +
                    expectedSuffixBytes &&
            richResources.ordinaryPartGpuCopyBytes >=
                coarseResources.ordinaryPartGpuCopyBytes +
                    expectedCopiedBytes;
        /* GL 3.1/ARB_copy_buffer preserves the old device prefix and uploads
         * only the suffix.  Legacy software contexts do not expose that
         * operation; their conservative, defined fallback is one complete
         * upload.  Lineage reuse must still be recognized in both cases so a
         * capable later generation/context may take the fast path. */
        const bool completeUploadFallback =
            richResources.ordinaryPartFullUploadBytes >=
                coarseResources.ordinaryPartFullUploadBytes +
                    expectedCompleteBytes &&
            richResources.ordinaryPartSuffixUploadBytes ==
                coarseResources.ordinaryPartSuffixUploadBytes &&
            richResources.ordinaryPartGpuCopyBytes ==
                coarseResources.ordinaryPartGpuCopyBytes;
        passed = passed && assembly->lastRenderTier() != 6 &&
            assembly->lastRenderedTriangleCount() == triangleCount &&
            (copiedPrefix || completeUploadFallback) &&
            richResources.ordinaryPartLineageReuseCount >
                coarseResources.ordinaryPartLineageReuseCount;
        if (!passed) {
            std::fprintf(stderr,
                "ordinary progressive generation did not copy/reuse prefix "
                "(tier=%d triangles=%llu full=%llu/%llu suffix=%llu/%llu "
                "copy=%llu/%llu reuse=%llu/%llu)\n",
                assembly->lastRenderTier(),
                static_cast<unsigned long long>(
                    assembly->lastRenderedTriangleCount()),
                static_cast<unsigned long long>(
                    coarseResources.ordinaryPartFullUploadBytes),
                static_cast<unsigned long long>(
                    richResources.ordinaryPartFullUploadBytes),
                static_cast<unsigned long long>(
                    coarseResources.ordinaryPartSuffixUploadBytes),
                static_cast<unsigned long long>(
                    richResources.ordinaryPartSuffixUploadBytes),
                static_cast<unsigned long long>(
                    coarseResources.ordinaryPartGpuCopyBytes),
                static_cast<unsigned long long>(
                    richResources.ordinaryPartGpuCopyBytes),
                static_cast<unsigned long long>(
                    coarseResources.ordinaryPartLineageReuseCount),
                static_cast<unsigned long long>(
                    richResources.ordinaryPartLineageReuseCount));
        }
        if (passed) {
            std::shared_ptr<Obol::PartGeometryBuilder> contractedGeometry(
                new Obol::PartGeometryBuilder);
            contractedGeometry->shaded = std::move(contracted);
            contractedGeometry->conservativeBounds =
                coarseGeometry->conservativeBounds;
            requireCadMutation(assembly->upsertParts({{part,
                requireCadValue(
                    Obol::cadAdmitPartGeometry(*contractedGeometry),
                    "ordinary contraction admission").geometry, true}}),
                "ordinary contraction part");
            passed = render(renderer, root);
            const Obol::CadGpuResourceSnapshot contractedResources =
                assembly->gpuResourceSnapshot();
            passed = passed &&
                assembly->lastRenderedTriangleCount() == 1u &&
                contractedResources.ordinaryPartBufferBytes ==
                    richResources.ordinaryPartBufferBytes &&
                contractedResources.ordinaryPartFullUploadBytes ==
                    richResources.ordinaryPartFullUploadBytes &&
                contractedResources.ordinaryPartSuffixUploadBytes ==
                    richResources.ordinaryPartSuffixUploadBytes &&
                contractedResources.ordinaryPartGpuCopyBytes ==
                    richResources.ordinaryPartGpuCopyBytes &&
                contractedResources.ordinaryPartLineageReuseCount >
                    richResources.ordinaryPartLineageReuseCount;
            if (!passed) {
                std::fprintf(stderr,
                    "ordinary progressive contraction did not retain its "
                    "certified GPU superset (triangles=%llu bytes=%zu/%zu "
                    "full=%llu/%llu suffix=%llu/%llu copy=%llu/%llu "
                    "reuse=%llu/%llu)\n",
                    static_cast<unsigned long long>(
                        assembly->lastRenderedTriangleCount()),
                    richResources.ordinaryPartBufferBytes,
                    contractedResources.ordinaryPartBufferBytes,
                    static_cast<unsigned long long>(
                        richResources.ordinaryPartFullUploadBytes),
                    static_cast<unsigned long long>(
                        contractedResources.ordinaryPartFullUploadBytes),
                    static_cast<unsigned long long>(
                        richResources.ordinaryPartSuffixUploadBytes),
                    static_cast<unsigned long long>(
                        contractedResources.ordinaryPartSuffixUploadBytes),
                    static_cast<unsigned long long>(
                        richResources.ordinaryPartGpuCopyBytes),
                    static_cast<unsigned long long>(
                        contractedResources.ordinaryPartGpuCopyBytes),
                    static_cast<unsigned long long>(
                        richResources.ordinaryPartLineageReuseCount),
                    static_cast<unsigned long long>(
                        contractedResources.ordinaryPartLineageReuseCount));
            }
            if (passed) {
                Obol::TriMesh replacement =
                    *contractedGeometry->shaded;
                replacement.progressiveLineage = lineage + 1u;
                std::shared_ptr<Obol::PartGeometryBuilder> replacementGeometry(
                    new Obol::PartGeometryBuilder);
                replacementGeometry->shaded = std::move(replacement);
                replacementGeometry->conservativeBounds =
                    contractedGeometry->conservativeBounds;
                requireCadMutation(assembly->upsertParts({{part,
                    requireCadValue(
                        Obol::cadAdmitPartGeometry(*replacementGeometry),
                        "ordinary replacement admission").geometry, true}}),
                    "ordinary replacement part");
                passed = render(renderer, root);
                const Obol::CadGpuResourceSnapshot replacementResources =
                    assembly->gpuResourceSnapshot();
                const uint64_t oneTriangleBytes =
                    3u * (3u * sizeof(float) + sizeof(uint32_t));
                passed = passed &&
                    assembly->lastRenderedTriangleCount() == 1u &&
                    replacementResources.ordinaryPartFullUploadBytes ==
                        contractedResources.ordinaryPartFullUploadBytes +
                            oneTriangleBytes &&
                    replacementResources.
                        ordinaryPartLineageReplacementCount >
                    contractedResources.
                        ordinaryPartLineageReplacementCount;
                if (!passed) {
                    std::fprintf(stderr,
                        "ordinary progressive lineage replacement was not "
                        "explicitly accounted (triangles=%llu full=%llu/%llu "
                        "replacement=%llu/%llu)\n",
                        static_cast<unsigned long long>(
                            assembly->lastRenderedTriangleCount()),
                        static_cast<unsigned long long>(
                            contractedResources.
                                ordinaryPartFullUploadBytes),
                        static_cast<unsigned long long>(
                            replacementResources.
                                ordinaryPartFullUploadBytes),
                        static_cast<unsigned long long>(
                            contractedResources.
                                ordinaryPartLineageReplacementCount),
                        static_cast<unsigned long long>(
                            replacementResources.
                                ordinaryPartLineageReplacementCount));
                }
            }
        }
    }

    root->unref();
    if (hadPreviousIndirect)
        setTestEnvironment("OBOL_CAD_INDIRECT",
            previousIndirectValue.c_str(), 1);
    else
        unsetTestEnvironment("OBOL_CAD_INDIRECT");
    return passed;
}

bool
ordinaryProgressiveZeroLineageReplacesWithoutOverread()
{
    const char *previousIndirect = std::getenv("OBOL_CAD_INDIRECT");
    const bool hadPreviousIndirect = previousIndirect != nullptr;
    const std::string previousIndirectValue =
        previousIndirect ? previousIndirect : "";
    setTestEnvironment("OBOL_CAD_INDIRECT", "0", 1);

    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 10.0f);
    camera->nearDistance.setValue(0.1f);
    camera->farDistance.setValue(100.0f);
    camera->height.setValue(4.0f);
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);
    SoCADAssembly *assembly = new SoCADAssembly;
    setCadDrawMode(root, SoCADViewState::SHADED);
    root->addChild(assembly);

    Obol::TriMesh rich;
    rich.bounds.makeEmpty();
    for (uint32_t triangle = 0; triangle < 64u; ++triangle) {
        const float x = -1.5f + 0.35f * static_cast<float>(triangle % 8u);
        const float y = -1.5f + 0.35f * static_cast<float>(triangle / 8u);
        const SbVec3f points[3] = {
            SbVec3f(x, y, 0.0f),
            SbVec3f(x + 0.25f, y, 0.0f),
            SbVec3f(x + 0.12f, y + 0.25f, 0.0f)
        };
        for (const SbVec3f& point : points) {
            rich.indices.push_back(
                static_cast<uint32_t>(rich.positions.size()));
            rich.positions.push_back(point);
            rich.bounds.extendBy(point);
        }
    }
    rich.progressiveMinimumCut = 0;
    rich.progressiveResidentCut = 15;
    setProgressiveCuts(rich, 16, 3u, 3u);
    rich.progressiveCuts[15].indexCount =
        static_cast<uint32_t>(rich.indices.size());
    rich.progressiveCuts[15].positionCount =
        static_cast<uint32_t>(rich.positions.size());
    rich.progressiveQuantizationMinimum = rich.bounds.getMin();
    rich.progressiveQuantizationMaximum = rich.bounds.getMax();
    /* Zero explicitly means no cross-generation prefix identity. */
    rich.progressiveLineage = 0;

    Obol::TriMesh coarse = rich;
    coarse.positions.resize(3u);
    coarse.indices.resize(3u);
    coarse.progressiveResidentCut = 0;

    const Obol::PartId part =
        Obol::CadIdBuilder::partId("ordinary-zero-lineage-replacement");
    std::shared_ptr<Obol::PartGeometryBuilder> richGeometry(
        new Obol::PartGeometryBuilder);
    richGeometry->shaded = std::move(rich);
    richGeometry->conservativeBounds = richGeometry->shaded->bounds;
    requireCadMutation(assembly->upsertParts({{part,
        requireCadValue(Obol::cadAdmitPartGeometry(*richGeometry),
            "zero-lineage rich admission").geometry, false}}),
        "zero-lineage rich part");

    Obol::InstanceRecord instance;
    instance.part = part;
    instance.parent = Obol::CadIdBuilder::rootInstance();
    instance.childName = "ordinary-zero-lineage-replacement";
    instance.localToRoot.makeIdentity();
    instance.lodCut = 15;
    requireCadMutation(assembly->upsertInstanceAuto(instance),
        "zero-lineage instance");

    const SbViewportRegion viewport(192, 192);
    SoOffscreenRenderer renderer(viewport);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
    bool passed = render(renderer, root) &&
        assembly->lastRenderedTriangleCount() == 64u;
    const Obol::CadGpuResourceSnapshot richResources =
        assembly->gpuResourceSnapshot();

    if (passed) {
        std::shared_ptr<Obol::PartGeometryBuilder> coarseGeometry(
            new Obol::PartGeometryBuilder);
        coarseGeometry->shaded = std::move(coarse);
        coarseGeometry->conservativeBounds =
            richGeometry->conservativeBounds;
        requireCadMutation(assembly->upsertParts({{part,
            requireCadValue(Obol::cadAdmitPartGeometry(*coarseGeometry),
                "zero-lineage coarse admission").geometry, true}}),
            "zero-lineage coarse part");
        passed = render(renderer, root);
        const Obol::CadGpuResourceSnapshot coarseResources =
            assembly->gpuResourceSnapshot();
        const uint64_t oneTriangleBytes =
            3u * (3u * sizeof(float) + sizeof(uint32_t));
        passed = passed && assembly->lastRenderedTriangleCount() == 1u &&
            coarseResources.ordinaryPartFullUploadBytes ==
                richResources.ordinaryPartFullUploadBytes +
                    oneTriangleBytes &&
            coarseResources.ordinaryPartLineageReuseCount ==
                richResources.ordinaryPartLineageReuseCount;
        if (!passed) {
            std::fprintf(stderr,
                "zero-lineage progressive replacement retained stale "
                "CPU counts (triangles=%llu full=%llu/%llu reuse=%llu/%llu)\n",
                static_cast<unsigned long long>(
                    assembly->lastRenderedTriangleCount()),
                static_cast<unsigned long long>(
                    richResources.ordinaryPartFullUploadBytes),
                static_cast<unsigned long long>(
                    coarseResources.ordinaryPartFullUploadBytes),
                static_cast<unsigned long long>(
                    richResources.ordinaryPartLineageReuseCount),
                static_cast<unsigned long long>(
                    coarseResources.ordinaryPartLineageReuseCount));
        }
    }

    root->unref();
    if (hadPreviousIndirect)
        setTestEnvironment("OBOL_CAD_INDIRECT", previousIndirectValue.c_str(), 1);
    else
        unsetTestEnvironment("OBOL_CAD_INDIRECT");
    return passed;
}

struct DeadlineAbortCounter {
    size_t calls = 0;
    size_t abortAt = static_cast<size_t>(-1);
};

SoGLRenderAction::AbortCode
deadlineAbortCounter(void *userData)
{
    DeadlineAbortCounter *counter =
        static_cast<DeadlineAbortCounter *>(userData);
    if (!counter)
        return SoGLRenderAction::CONTINUE;
    ++counter->calls;
    return counter->calls >= counter->abortAt ?
        SoGLRenderAction::ABORT : SoGLRenderAction::CONTINUE;
}

struct DeadlineAssemblyAbort {
    SoCADAssembly *assembly = nullptr;
    uint64_t executionSerial = 0;
    size_t calls = 0;
    size_t abortAt = 10;
};

struct DeadlinePreparationAbort {
    SoCADAssembly *assembly = nullptr;
    size_t calls = 0u;
};

SoGLRenderAction::AbortCode
deadlineAbortPreparation(void *userData)
{
    DeadlinePreparationAbort *counter =
        static_cast<DeadlinePreparationAbort *>(userData);
    if (!counter || !counter->assembly ||
            counter->assembly->presentationPreparationSnapshot().state !=
                Obol::CadPresentationPreparationState::Preparing)
        return SoGLRenderAction::CONTINUE;
    ++counter->calls;
    return SoGLRenderAction::ABORT;
}

bool
flatShadedPlanningResumesAcrossAborts()
{
    struct EnvironmentSnapshot {
        const char *name = nullptr;
        bool present = false;
        std::string value;
    } settings[] = {
        {"OBOL_CAD_INDIRECT"},
        {"OBOL_CAD_FLAT_SHADED"}
    };
    for (EnvironmentSnapshot& setting : settings) {
        const char *value = std::getenv(setting.name);
        setting.present = value != nullptr;
        if (value)
            setting.value = value;
    }
    setTestEnvironment("OBOL_CAD_INDIRECT", "0", 1);
    setTestEnvironment("OBOL_CAD_FLAT_SHADED", "1", 1);
    const auto restoreEnvironment = [&]() {
        for (const EnvironmentSnapshot& setting : settings) {
            if (setting.present)
                setTestEnvironment(
                    setting.name, setting.value.c_str(), 1);
            else
                unsetTestEnvironment(setting.name);
        }
    };

    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 10.0f);
    camera->nearDistance.setValue(0.1f);
    camera->farDistance.setValue(100.0f);
    camera->height.setValue(4.0f);
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);
    SoCADAssembly *assembly = new SoCADAssembly;
    setCadDrawMode(root, SoCADViewState::SHADED);
    root->addChild(assembly);

    constexpr uint32_t occurrenceCount = 2048u;
    constexpr uint32_t partCount = 128u;
    constexpr uint32_t instancesPerPart = occurrenceCount / partCount;
    Obol::TriMesh mesh;
    mesh.positions = {
        SbVec3f(-0.01f, -0.01f, 0.0f),
        SbVec3f(0.01f, -0.01f, 0.0f),
        SbVec3f(0.0f, 0.01f, 0.0f)
    };
    mesh.indices = {0u, 1u, 2u};
    mesh.bounds = SbBox3f(
        SbVec3f(-0.01f, -0.01f, 0.0f),
        SbVec3f(0.01f, 0.01f, 0.0f));

    for (uint32_t partIndex = 0; partIndex < partCount; ++partIndex) {
        char name[64] = {};
        std::snprintf(name, sizeof(name),
            "flat-planning-resume-%03u", partIndex);
        Obol::PartGeometryBuilder geometry;
        geometry.shaded = mesh;
        geometry.shadedCullBackfaces = false;
        const Obol::PartId part = Obol::CadIdBuilder::partId(name);
        requireCadMutation(admitAndUpsertPart(assembly, part, geometry),
            "flat planning part");
        for (uint32_t offset = 0;
                offset < instancesPerPart; ++offset) {
            const uint32_t occurrence =
                partIndex * instancesPerPart + offset;
            Obol::InstanceRecord instance;
            instance.part = part;
            instance.parent = Obol::CadIdBuilder::rootInstance();
            instance.childName = name;
            instance.occurrenceIndex = occurrence;
            instance.localToRoot.setTranslate(SbVec3f(
                -1.5f + 0.05f * static_cast<float>(occurrence % 64u),
                -0.75f + 0.05f * static_cast<float>(occurrence / 64u),
                0.0f));
            requireCadMutation(assembly->upsertInstanceAuto(instance),
                "flat planning instance");
        }
    }

    SoOffscreenRenderer renderer(SbViewportRegion(192, 192));
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
    /* Compile and classify the occurrence stream without touching the
     * shaded executor.  Subsequent abort samples then belong to flat-shaded
     * preparation, not first-traversal scene setup. */
    setCadDrawMode(root, SoCADViewState::WIREFRAME);
    const bool planWarmed = render(renderer, root);
    setCadDrawMode(root, SoCADViewState::SHADED);
    SoGLRenderAction *action = renderer.getGLRenderAction();
    SoGLRenderAction::SoGLRenderAbortCB *previousCallback = nullptr;
    void *previousData = nullptr;
    if (action)
        action->getAbortCallback(previousCallback, previousData);

    bool passed = planWarmed && action != nullptr;
    Obol::CadPresentationPreparationTarget target;
    uint64_t priorCompleted = 0;
    constexpr size_t interruptedSlices = 3u;
    constexpr size_t initialPlanningSafePoint = 12u;
    constexpr size_t resumedPlanningSafePoint = 10u;
    for (size_t slice = 0; passed && slice < interruptedSlices; ++slice) {
        DeadlineAbortCounter interrupted;
        /* The first shaded traversal finishes the already-bounded
         * occurrence classifier before entering planning.  Later slices
         * start from its retained result and reach the renderer sooner. */
        interrupted.abortAt = slice == 0 ?
            initialPlanningSafePoint : resumedPlanningSafePoint;
        action->setAbortCallback(deadlineAbortCounter, &interrupted);
        (void)renderer.render(root);
        const Obol::CadPresentationPreparationSnapshot snapshot =
            assembly->presentationPreparationSnapshot();
        passed = action->hasTerminated() &&
            interrupted.calls == interrupted.abortAt &&
            snapshot.target.kind ==
                Obol::CadPresentationPreparationKind::FlatShadedPlanning &&
            snapshot.state ==
                Obol::CadPresentationPreparationState::Preparing &&
            snapshot.totalUnits == occurrenceCount &&
            snapshot.completedUnits > priorCompleted &&
            snapshot.completedUnits < snapshot.totalUnits;
        if (slice == 0)
            target = snapshot.target;
        else
            passed = passed && snapshot.target == target;
        priorCompleted = snapshot.completedUnits;
    }
    if (action)
        action->setAbortCallback(previousCallback, previousData);
    passed = passed && render(renderer, root) &&
        assembly->lastRenderTier() == 4 &&
        assembly->lastRenderedTriangleCount() == occurrenceCount;
    const Obol::CadPresentationPreparationSnapshot completed =
        assembly->presentationPreparationSnapshot();
    passed = passed && completed.target.kind ==
            Obol::CadPresentationPreparationKind::FlatShadedAtlas &&
        completed.state ==
            Obol::CadPresentationPreparationState::Complete &&
        completed.totalUnits == occurrenceCount &&
        completed.completedUnits == completed.totalUnits;
    if (!passed) {
        std::fprintf(stderr,
            "flat planning did not resume with an immutable certificate "
            "(tier=%d planning=%llu/%llu final-kind=%u final=%llu/%llu)\n",
            assembly->lastRenderTier(),
            static_cast<unsigned long long>(priorCompleted),
            static_cast<unsigned long long>(occurrenceCount),
            static_cast<unsigned>(completed.target.kind),
            static_cast<unsigned long long>(completed.completedUnits),
            static_cast<unsigned long long>(completed.totalUnits));
    }

    root->unref();
    restoreEnvironment();
    return passed;
}

bool
flatShadedAtlasMakesProgressInsideLargeSourceRange()
{
    struct EnvironmentSnapshot {
        const char *name = nullptr;
        bool present = false;
        std::string value;
    } settings[] = {
        {"OBOL_CAD_INDIRECT"},
        {"OBOL_CAD_FLAT_SHADED"}
    };
    for (EnvironmentSnapshot& setting : settings) {
        const char *value = std::getenv(setting.name);
        setting.present = value != nullptr;
        if (value)
            setting.value = value;
    }
    setTestEnvironment("OBOL_CAD_INDIRECT", "0", 1);
    setTestEnvironment("OBOL_CAD_FLAT_SHADED", "1", 1);
    const auto restoreEnvironment = [&]() {
        for (const EnvironmentSnapshot& setting : settings) {
            if (setting.present)
                setTestEnvironment(
                    setting.name, setting.value.c_str(), 1);
            else
                unsetTestEnvironment(setting.name);
        }
    };

    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 10.0f);
    camera->nearDistance.setValue(0.1f);
    camera->farDistance.setValue(100.0f);
    camera->height.setValue(4.0f);
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);
    SoCADAssembly *assembly = new SoCADAssembly;
    setCadDrawMode(root, SoCADViewState::WIREFRAME);
    root->addChild(assembly);

    constexpr uint32_t triangleCount = 32u * 1024u;
    Obol::TriMesh mesh;
    mesh.positions.reserve(triangleCount * 3u);
    mesh.indices.reserve(triangleCount * 3u);
    mesh.bounds.makeEmpty();
    for (uint32_t triangle = 0; triangle < triangleCount; ++triangle) {
        const float x = -1.5f +
            0.012f * static_cast<float>(triangle % 256u);
        const float y = -0.75f +
            0.012f * static_cast<float>((triangle / 256u) % 128u);
        const SbVec3f points[3] = {
            SbVec3f(x, y, 0.0f),
            SbVec3f(x + 0.008f, y, 0.0f),
            SbVec3f(x + 0.004f, y + 0.007f, 0.0f)
        };
        for (const SbVec3f& point : points) {
            mesh.indices.push_back(
                static_cast<uint32_t>(mesh.positions.size()));
            mesh.positions.push_back(point);
            mesh.bounds.extendBy(point);
        }
    }
    Obol::PartGeometryBuilder geometry;
    geometry.shaded = mesh;
    geometry.shadedCullBackfaces = false;
    const Obol::PartId part =
        Obol::CadIdBuilder::partId("flat-large-range");
    requireCadMutation(admitAndUpsertPart(assembly, part, geometry),
        "flat large-range part");
    Obol::InstanceRecord instance;
    instance.part = part;
    instance.parent = Obol::CadIdBuilder::rootInstance();
    instance.childName = "flat-large-range";
    requireCadMutation(assembly->upsertInstanceAuto(instance),
        "flat large-range instance");

    SoOffscreenRenderer renderer(SbViewportRegion(192, 192));
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
    bool passed = render(renderer, root);
    setCadDrawMode(root, SoCADViewState::HIDDEN_LINE);
    SoGLRenderAction *action = renderer.getGLRenderAction();
    SoGLRenderAction::SoGLRenderAbortCB *previousCallback = nullptr;
    void *previousData = nullptr;
    if (action)
        action->getAbortCallback(previousCallback, previousData);
    else
        passed = false;

    DeadlineAbortCounter interrupted;
    constexpr size_t atlasPartialProgressSafePoint = 64u;
    interrupted.abortAt = atlasPartialProgressSafePoint;
    if (passed) {
        action->setAbortCallback(deadlineAbortCounter, &interrupted);
        (void)renderer.render(root);
        const Obol::CadPresentationPreparationSnapshot partial =
            assembly->presentationPreparationSnapshot();
        passed = action->hasTerminated() &&
            partial.target.kind ==
                Obol::CadPresentationPreparationKind::FlatShadedAtlas &&
            partial.state ==
                Obol::CadPresentationPreparationState::Preparing &&
            partial.totalUnits > 1u && partial.completedUnits > 0u &&
            partial.completedUnits < partial.totalUnits;
    }
    if (action)
        action->setAbortCallback(previousCallback, previousData);
    passed = passed && render(renderer, root) &&
        assembly->lastRenderTier() == 3 &&
        assembly->lastRenderedTriangleCount() == triangleCount;
    const Obol::CadPresentationPreparationSnapshot completed =
        assembly->presentationPreparationSnapshot();
    passed = passed && completed.target.kind ==
            Obol::CadPresentationPreparationKind::FlatShadedAtlas &&
        completed.state ==
            Obol::CadPresentationPreparationState::Complete &&
        completed.completedUnits == completed.totalUnits;
    if (!passed) {
        std::fprintf(stderr,
            "flat atlas did not commit progress inside a large source "
            "range (tier=%d abort-calls=%zu state=%u units=%llu/%llu)\n",
            assembly->lastRenderTier(), interrupted.calls,
            static_cast<unsigned>(completed.state),
            static_cast<unsigned long long>(completed.completedUnits),
            static_cast<unsigned long long>(completed.totalUnits));
    }

    root->unref();
    restoreEnvironment();
    return passed;
}

bool
progressiveReplacementTombstoneKeepsActiveIndex()
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 10.0f);
    camera->nearDistance.setValue(0.1f);
    camera->farDistance.setValue(100.0f);
    camera->height.setValue(8.0f);
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);
    SoCADAssembly *assembly = new SoCADAssembly;
    setCadDrawMode(root, SoCADViewState::SHADED);
    root->addChild(assembly);

    Obol::TriMesh mesh;
    mesh.positions = {
        SbVec3f(-0.4f, -0.3f, 0.0f),
        SbVec3f(0.4f, -0.3f, 0.0f),
        SbVec3f(0.0f, 0.4f, 0.0f)
    };
    mesh.indices = {0u, 1u, 2u};
    mesh.bounds = SbBox3f(
        SbVec3f(-0.4f, -0.3f, 0.0f),
        SbVec3f(0.4f, 0.4f, 0.0f));
    mesh.progressiveMinimumCut = 0;
    mesh.progressiveResidentCut = 15;
    setProgressiveCuts(mesh, 16, 3u, 3u);
    mesh.progressiveQuantizationMinimum = mesh.bounds.getMin();
    mesh.progressiveQuantizationMaximum = mesh.bounds.getMax();

    Obol::PartGeometryBuilder geometry;
    geometry.shaded = mesh;
    geometry.shadedCullBackfaces = false;
    const Obol::PartId sharedPart =
        Obol::CadIdBuilder::partId("tombstone-shared-progressive");
    const Obol::PartId replacementPart =
        Obol::CadIdBuilder::partId("tombstone-replacement-progressive");
    requireCadMutation(admitAndUpsertPart(assembly, sharedPart, geometry),
        "shared progressive part");
    requireCadMutation(
        admitAndUpsertPart(assembly, replacementPart, geometry),
        "replacement progressive part");

    constexpr size_t occurrenceCount = 4u;
    std::array<Obol::InstanceId, occurrenceCount> instances;
    std::array<Obol::InstanceRecord, occurrenceCount> records;
    for (size_t index = 0; index < occurrenceCount; ++index) {
        Obol::InstanceRecord& record = records[index];
        record.part = sharedPart;
        record.parent = Obol::CadIdBuilder::rootInstance();
        record.childName = "tombstone-shared-occurrence";
        record.occurrenceIndex = static_cast<uint32_t>(index);
        record.lodCut = 0;
        record.localToRoot.setTranslate(SbVec3f(
            -2.25f + 1.5f * static_cast<float>(index), 0.0f, 0.0f));
        instances[index] = requireCadValue(
            assembly->upsertInstanceAuto(record),
            "shared progressive instance").instance;
    }

    SoOffscreenRenderer renderer(SbViewportRegion(192, 192));
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
    bool passed = render(renderer, root);
    const uint64_t compiledPlans = assembly->framePlanBuildCount();

    /* Rebinding the final shared occurrence cannot reuse its old part slot,
     * so the sparse path hides that compiled occurrence and appends its
     * active replacement.  Moving the first peer to another cut then swaps
     * it with the hidden final tombstone.  The tombstone and replacement
     * share an InstanceId; this is the exact sequence which previously made
     * the stale slot overwrite the active ID-to-index mapping. */
    records.back().part = replacementPart;
    Obol::InstanceUpdate rebind;
    rebind.instance = instances.back();
    rebind.record = records.back();
    passed = passed && assembly->upsertInstances({rebind});

    Obol::InstanceLodUpdate firstCut;
    firstCut.instance = instances.front();
    firstCut.lodCut = 15;
    passed = passed && assembly->updateInstanceCuts({firstCut});
    Obol::InstanceLodUpdate replacementCut;
    replacementCut.instance = instances.back();
    replacementCut.lodCut = 15;
    passed = passed && assembly->updateInstanceCuts({replacementCut});
    passed = passed && render(renderer, root) &&
        assembly->framePlanBuildCount() == compiledPlans &&
        assembly->lastRenderedTriangleCount() == occurrenceCount;
    if (!passed) {
        std::fprintf(stderr,
            "progressive replacement tombstone stole the active index "
            "(plan-builds=%llu/%llu triangles=%llu)\n",
            static_cast<unsigned long long>(compiledPlans),
            static_cast<unsigned long long>(
                assembly->framePlanBuildCount()),
            static_cast<unsigned long long>(
                assembly->lastRenderedTriangleCount()));
    }

    root->unref();
    return passed;
}

SoGLRenderAction::AbortCode
deadlineAbortAssemblyWork(void *userData)
{
    DeadlineAssemblyAbort *counter =
        static_cast<DeadlineAssemblyAbort *>(userData);
    if (!counter || !counter->assembly ||
            counter->assembly->renderExecutionSerial() ==
                counter->executionSerial)
        return SoGLRenderAction::CONTINUE;
    ++counter->calls;
    return counter->calls >= counter->abortAt ?
        SoGLRenderAction::ABORT : SoGLRenderAction::CONTINUE;
}

bool
subpixelPreparationReservationCoversBoundedScratch()
{
    constexpr uint32_t occurrenceCount = 131072u;
    constexpr uint32_t gridWidth = 512u;
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 5.0f);
    camera->nearDistance.setValue(0.1f);
    camera->farDistance.setValue(100.0f);
    camera->height.setValue(2048.0f);
    root->addChild(camera);

    SoCADAssembly *assembly = new SoCADAssembly;
    setCadDrawMode(root, SoCADViewState::WIREFRAME);
    root->addChild(assembly);

    Obol::PartGeometryBuilder geometry;
    geometry.wire = unitBox();
    geometry.subpixelProxyEligible = true;
    geometry.structuralProxy = true;
    const Obol::PartId part =
        Obol::CadIdBuilder::partId("bounded-subpixel-preparation");
    requireCadMutation(admitAndUpsertPart(assembly, part, geometry),
        "flattened wire part");

    std::vector<Obol::InstanceUpdate> updates;
    updates.reserve(occurrenceCount);
    for (uint32_t index = 0; index < occurrenceCount; ++index) {
        Obol::InstanceRecord instance;
        instance.part = part;
        instance.parent = Obol::CadIdBuilder::rootInstance();
        instance.childName = "bounded-subpixel-preparation";
        instance.occurrenceIndex = index;
        instance.lodStructuralProxy = true;
        instance.localToRoot.setTranslate(SbVec3f(
            static_cast<float>(index % gridWidth) -
                static_cast<float>(gridWidth) * 0.5f,
            static_cast<float>(index / gridWidth) -
                static_cast<float>(occurrenceCount / gridWidth) * 0.5f,
            0.0f));
        Obol::InstanceUpdate update;
        update.instance = Obol::CadIdBuilder::childInstance(
            instance.parent, instance.childName,
            instance.occurrenceIndex, instance.boolOp);
        update.record = instance;
        updates.push_back(std::move(update));
    }
    requireCadMutation(assembly->upsertInstances(updates),
        "flattened wire instances");

    SoOffscreenRenderer renderer(SbViewportRegion(128, 128));
    renderer.setComponents(SoOffscreenRenderer::RGB);
    SoGLRenderAction *action = renderer.getGLRenderAction();
    SoGLRenderAction::SoGLRenderAbortCB *previousCallback = nullptr;
    void *previousData = nullptr;
    if (action)
        action->getAbortCallback(previousCallback, previousData);

    DeadlinePreparationAbort interrupted;
    interrupted.assembly = assembly;
    if (action)
        action->setAbortCallback(deadlineAbortPreparation, &interrupted);
    (void)renderer.render(root);
    const Obol::CadPresentationPreparationSnapshot preparing =
        assembly->presentationPreparationSnapshot();
    if (action)
        action->setAbortCallback(previousCallback, previousData);

    /* These are only the fixed occurrence-indexed buffers.  Point records
     * and their reverse index make the real reservation larger.  Requiring
     * this lower bound catches any return to unaccounted node-based scratch
     * while remaining independent of std::vector growth policy. */
    const uint64_t fixedOccurrenceBytes =
        sizeof(uint8_t) + sizeof(uint32_t) + sizeof(int8_t) +
        sizeof(Obol::InstanceId);
    const uint64_t minimumReservation =
        static_cast<uint64_t>(occurrenceCount) * fixedOccurrenceBytes +
        sizeof(uint8_t) + sizeof(size_t);
    const bool preparationInterrupted = action &&
        action->hasTerminated() && interrupted.calls == 1u;
    bool passed = preparationInterrupted &&
        preparing.target.kind == Obol::CadPresentationPreparationKind::
            SubpixelClassification &&
        preparing.state == Obol::CadPresentationPreparationState::Preparing &&
        preparing.completedUnits > 0u &&
        preparing.completedUnits < preparing.totalUnits &&
        preparing.reservedBytes >= minimumReservation;

    SoOffscreenRenderer recoveryRenderer(SbViewportRegion(128, 128));
    recoveryRenderer.setComponents(SoOffscreenRenderer::RGB);
    passed = passed && render(recoveryRenderer, root) &&
        assembly->lastSubpixelProxyCount() == occurrenceCount &&
        assembly->lastUncollapsedStructuralProxyCount() == 0u;
    if (!passed) {
        std::fprintf(stderr,
            "bounded classifier reservation failed "
            "(aborted=%d state=%u completed=%llu/%llu reserved=%llu "
            "minimum=%llu proxies=%zu boxes=%zu)\n",
            preparationInterrupted ? 1 : 0,
            static_cast<unsigned>(preparing.state),
            static_cast<unsigned long long>(preparing.completedUnits),
            static_cast<unsigned long long>(preparing.totalUnits),
            static_cast<unsigned long long>(preparing.reservedBytes),
            static_cast<unsigned long long>(minimumReservation),
            assembly->lastSubpixelProxyCount(),
            assembly->lastUncollapsedStructuralProxyCount());
    }
    root->unref();
    return passed;
}

bool
ordinaryExecutorHonorsAbortSafePoints()
{
    struct EnvironmentSnapshot {
        const char *name = nullptr;
        bool present = false;
        std::string value;
    };
    EnvironmentSnapshot settings[] = {
        {"OBOL_CAD_FLAT_WIRE"},
        {"OBOL_CAD_FLAT_SHADED"},
        {"OBOL_CAD_INDIRECT"},
        {"OBOL_CAD_REPLAY"}
    };
    for (EnvironmentSnapshot& setting : settings) {
        const char *value = std::getenv(setting.name);
        setting.present = value != nullptr;
        if (value)
            setting.value = value;
        setTestEnvironment(setting.name, "0", 1);
    }
    const auto restoreEnvironment = [&]() {
        for (const EnvironmentSnapshot& setting : settings) {
            if (setting.present)
                setTestEnvironment(setting.name, setting.value.c_str(), 1);
            else
                unsetTestEnvironment(setting.name);
        }
    };

    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 10.0f);
    camera->nearDistance.setValue(0.1f);
    camera->farDistance.setValue(100.0f);
    camera->height.setValue(72.0f);
    root->addChild(camera);

    SoCADAssembly *assembly = new SoCADAssembly;
    setCadDrawMode(root, SoCADViewState::WIREFRAME);
    root->addChild(assembly);

    Obol::PartGeometryBuilder geometry;
    geometry.wire = unitBox();
    geometry.subpixelProxyEligible = false;
    geometry.structuralProxy = false;
    const Obol::PartId part =
        Obol::CadIdBuilder::partId("deadline-shared-wire-part");
    requireCadMutation(admitAndUpsertPart(assembly, part, geometry),
        "software wire part");
    constexpr uint32_t instanceCount = 4096u;
    for (uint32_t index = 0; index < instanceCount; ++index) {
        Obol::InstanceRecord instance;
        instance.part = part;
        instance.parent = Obol::CadIdBuilder::rootInstance();
        instance.childName = "deadline-shared-wire-instance";
        instance.occurrenceIndex = index;
        instance.localToRoot.setTranslate(SbVec3f(
            -31.5f + static_cast<float>(index % 64u),
            -31.5f + static_cast<float>(index / 64u), 0.0f));
        requireCadMutation(assembly->upsertInstanceAuto(instance),
            "software wire instance");
    }

    const SbViewportRegion viewport(128, 128);
    SoOffscreenRenderer renderer(viewport);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    /* First build and then replay the retained plan so callback sampling in
     * the measurement below belongs to the renderer executor, not plan
     * construction. */
    bool passed = render(renderer, root) && render(renderer, root);
    const uint64_t planBuilds = assembly->framePlanBuildCount();
    SoGLRenderAction *action = renderer.getGLRenderAction();
    SoGLRenderAction::SoGLRenderAbortCB *previousCallback = nullptr;
    void *previousData = nullptr;
    if (action)
        action->getAbortCallback(previousCallback, previousData);

    DeadlineAbortCounter observed;
    if (passed && action) {
        action->setAbortCallback(deadlineAbortCounter, &observed);
        passed = render(renderer, root) && !action->hasTerminated() &&
            assembly->framePlanBuildCount() == planBuilds &&
            observed.calls >= 10u;
    } else {
        passed = false;
    }

    DeadlineAbortCounter interrupted;
    interrupted.abortAt = (std::max)(size_t(4), observed.calls / 2u);
    const Obol::CadGpuResourceSnapshot beforeInterrupted =
        assembly->gpuResourceSnapshot();
    passed = passed && beforeInterrupted.frameSerial > 0 &&
        beforeInterrupted.trackedBufferBytes > 0 &&
        beforeInterrupted.ordinaryPartBufferBytes > 0;
    if (passed) {
        action->setAbortCallback(deadlineAbortCounter, &interrupted);
        (void)renderer.render(root);
        passed = action->hasTerminated() &&
            interrupted.calls == interrupted.abortAt &&
            assembly->gpuResourceSnapshot().frameSerial ==
                beforeInterrupted.frameSerial;
    }

    if (action)
        action->setAbortCallback(previousCallback, previousData);
    if (passed) {
        passed = render(renderer, root) && nonBlackPixels(renderer) != 0u &&
            assembly->framePlanBuildCount() == planBuilds &&
            assembly->gpuResourceSnapshot().frameSerial >
                beforeInterrupted.frameSerial;
    }
    if (!passed) {
        std::fprintf(stderr,
            "ordinary CAD executor deadline contract failed "
            "(tier=%d observed=%zu abortAt=%zu abortedCalls=%zu)\n",
            assembly->lastRenderTier(), observed.calls,
            interrupted.abortAt, interrupted.calls);
    }

    root->unref();
    restoreEnvironment();
    return passed;
}

bool
indirectAtlasValidationResumesAcrossAborts()
{
    struct EnvironmentSnapshot {
        const char *name = nullptr;
        bool present = false;
        std::string value;
    } settings[] = {
        {"OBOL_CAD_INDIRECT"},
        {"OBOL_CAD_FLAT_SHADED"},
        {"OBOL_CAD_ATLAS_VALIDATION_FRAMES"}
    };
    for (EnvironmentSnapshot& setting : settings) {
        const char *value = std::getenv(setting.name);
        setting.present = value != nullptr;
        if (value)
            setting.value = value;
    }
    setTestEnvironment("OBOL_CAD_INDIRECT", "1", 1);
    setTestEnvironment("OBOL_CAD_FLAT_SHADED", "0", 1);
    setTestEnvironment("OBOL_CAD_ATLAS_VALIDATION_FRAMES", "1", 1);
    const auto restoreEnvironment = [&]() {
        for (const EnvironmentSnapshot& setting : settings) {
            if (setting.present)
                setTestEnvironment(setting.name, setting.value.c_str(), 1);
            else
                unsetTestEnvironment(setting.name);
        }
    };

    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 10.0f);
    camera->nearDistance.setValue(0.1f);
    camera->farDistance.setValue(100.0f);
    camera->height.setValue(40.0f);
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);

    SoCADAssembly *assembly = new SoCADAssembly;
    setCadDrawMode(root, SoCADViewState::SHADED);
    root->addChild(assembly);

    Obol::TriMesh triangle;
    triangle.positions = {
        SbVec3f(-0.4f, -0.4f, 0.0f),
        SbVec3f( 0.4f, -0.4f, 0.0f),
        SbVec3f( 0.0f,  0.4f, 0.0f)};
    triangle.indices = {0u, 1u, 2u};
    triangle.bounds = SbBox3f(
        SbVec3f(-0.4f, -0.4f, 0.0f),
        SbVec3f( 0.4f,  0.4f, 0.0f));

    /* More parts than one executor safe-point span ensures validation needs
     * several deadline-bounded traversals when the callback below aborts at
     * its second sample. */
    constexpr uint32_t partCount = 1024u;
    for (uint32_t index = 0; index < partCount; ++index) {
        char name[64] = {};
        std::snprintf(name, sizeof(name),
            "validation-resume-%04u", index);
        Obol::PartGeometryBuilder geometry;
        geometry.shaded = triangle;
        geometry.subpixelProxyEligible = false;
        const Obol::PartId part = Obol::CadIdBuilder::partId(name);
        requireCadMutation(admitAndUpsertPart(assembly, part, geometry),
            "atlas pressure part");

        Obol::InstanceRecord instance;
        instance.part = part;
        instance.parent = Obol::CadIdBuilder::rootInstance();
        instance.childName = name;
        instance.occurrenceIndex = index;
        instance.localToRoot.setTranslate(SbVec3f(
            -15.5f + static_cast<float>(index % 32u),
            -15.5f + static_cast<float>(index / 32u), 0.0f));
        requireCadMutation(assembly->upsertInstanceAuto(instance),
            "atlas pressure instance");
    }

    const SbViewportRegion viewport(256, 256);
    SoOffscreenRenderer renderer(viewport);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
    bool passed = render(renderer, root);
    if (passed && assembly->lastRenderTier() == 6) {
        SoGLRenderAction *action = renderer.getGLRenderAction();
        SoGLRenderAction::SoGLRenderAbortCB *previousCallback = nullptr;
        void *previousData = nullptr;
        if (action)
            action->getAbortCallback(previousCallback, previousData);
        else
            passed = false;

        size_t preparationSlices = 0u;
        size_t interruptedAttempts = 0u;
        bool reachedSteadyDraw = false;
        constexpr size_t maximumAttempts = 12u;
        for (size_t attempt = 0;
                passed && attempt < maximumAttempts; ++attempt) {
            DeadlineAssemblyAbort interrupted;
            interrupted.assembly = assembly;
            interrupted.executionSerial =
                assembly->renderExecutionSerial();
            action->setAbortCallback(
                deadlineAbortAssemblyWork, &interrupted);
            const uint64_t preparationBefore =
                assembly->renderPreparationSerial();
            (void)renderer.render(root);
            if (action->hasTerminated())
                ++interruptedAttempts;
            if (assembly->renderPreparationSerial() ==
                    preparationBefore) {
                if (preparationSlices > 0u) {
                    reachedSteadyDraw = true;
                    break;
                }
                /* The one-frame audit countdown is itself steady replay.
                 * Continue once to enter the validation transaction. */
                continue;
            }
            ++preparationSlices;
        }
        if (action)
            action->setAbortCallback(previousCallback, previousData);
        passed = passed && interruptedAttempts >= 1u &&
            preparationSlices >= 2u && reachedSteadyDraw &&
            render(renderer, root) &&
            assembly->lastRenderTier() == 6 &&
            assembly->lastRenderedWork().exact;
        if (!passed)
            std::fprintf(stderr,
                "retained atlas validation did not converge across "
                "deadline slices (tier=%d aborts=%zu slices=%zu "
                "steady=%d)\n",
                assembly->lastRenderTier(), interruptedAttempts,
                preparationSlices, reachedSteadyDraw ? 1 : 0);
    }

    root->unref();
    restoreEnvironment();
    return passed;
}

bool
indirectProgressiveAtlasPreservesCoverageUnderPressure()
{
    constexpr int partCount = 192;
    constexpr int trianglesPerPart = 2000;

    struct EnvironmentSnapshot {
        const char *name;
        bool present;
        std::string value;
    } settings[] = {
        {"OBOL_CAD_INDIRECT", false, std::string()},
        {"OBOL_CAD_ATLAS_MB", false, std::string()}
    };
    for (EnvironmentSnapshot& setting : settings) {
        const char *value = std::getenv(setting.name);
        setting.present = value != nullptr;
        if (value)
            setting.value = value;
    }
    setTestEnvironment("OBOL_CAD_INDIRECT", "1", 1);
    /* One ordinary atlas page fits, two do not.  Every minimum prefix fits
     * comfortably, while all requested rich prefixes require the second
     * page. */
    setTestEnvironment("OBOL_CAD_ATLAS_MB", "20", 1);

    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 10.0f);
    camera->nearDistance.setValue(0.1f);
    camera->farDistance.setValue(100.0f);
    camera->height.setValue(150.0f);
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);
    SoCADAssembly *assembly = new SoCADAssembly;
    setCadDrawMode(root, SoCADViewState::SHADED);
    root->addChild(assembly);

    Obol::TriMesh mesh;
    mesh.positions.reserve(trianglesPerPart * 3u);
    mesh.indices.reserve(trianglesPerPart * 3u);
    mesh.bounds.makeEmpty();
    for (int triangle = 0; triangle < trianglesPerPart; ++triangle) {
        const float x = -0.48f +
            0.015f * static_cast<float>(triangle % 64);
        const float y = -0.48f +
            0.015f * static_cast<float>(triangle / 64);
        const SbVec3f points[3] = {
            SbVec3f(x, y, 0.0f),
            SbVec3f(x + 0.012f, y, 0.0f),
            SbVec3f(x + 0.006f, y + 0.010f, 0.0f)
        };
        for (const SbVec3f& point : points) {
            mesh.indices.push_back(
                static_cast<uint32_t>(mesh.positions.size()));
            mesh.positions.push_back(point);
            mesh.bounds.extendBy(point);
        }
    }
    mesh.progressiveMinimumCut = 0;
    mesh.progressiveResidentCut = 15;
    setProgressiveCuts(mesh, 16, 3u, 3u);
    mesh.progressiveCuts[15].indexCount =
        static_cast<uint32_t>(mesh.indices.size());
    mesh.progressiveCuts[15].positionCount =
        static_cast<uint32_t>(mesh.positions.size());
    mesh.progressiveQuantizationMinimum = mesh.bounds.getMin();
    mesh.progressiveQuantizationMaximum = mesh.bounds.getMax();

    std::vector<Obol::InstanceLodUpdate> richCuts;
    richCuts.reserve(partCount);
    for (int index = 0; index < partCount; ++index) {
        char name[64] = {};
        std::snprintf(name, sizeof(name),
            "progressive-pressure-%03d", index);
        Obol::PartGeometryBuilder geometry;
        geometry.shaded = mesh;
        geometry.subpixelProxyEligible = true;
        const Obol::PartId part = Obol::CadIdBuilder::partId(name);
        requireCadMutation(admitAndUpsertPart(assembly, part, geometry),
            "coverage pressure part");
        Obol::InstanceRecord instance;
        instance.part = part;
        instance.parent = Obol::CadIdBuilder::rootInstance();
        instance.childName = name;
        instance.occurrenceIndex = static_cast<uint32_t>(index);
        instance.localToRoot.setTranslate(SbVec3f(
            -60.0f + 7.0f * static_cast<float>(index % 18),
            -35.0f + 7.0f * static_cast<float>(index / 18), 0.0f));
        instance.lodCut = 0;
        const Obol::InstanceId id =
            requireCadValue(assembly->upsertInstanceAuto(instance),
                "coverage pressure instance").instance;
        richCuts.push_back({id, 15});
    }

    const SbViewportRegion viewport(256, 256);
    SoOffscreenRenderer renderer(viewport);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
    bool passed = render(renderer, root);
    const int tier = assembly->lastRenderTier();
    if (passed && tier == 6) {
        requireCadMutation(assembly->updateInstanceCuts(richCuts),
            "coverage pressure cuts");
        passed = render(renderer, root);
        const uint64_t pressuredTriangles =
            assembly->lastRenderedTriangleCount();
        const uint64_t requestedTriangles =
            static_cast<uint64_t>(partCount) * trianglesPerPart;
        const Obol::CadGpuResourceSnapshot pressured =
            assembly->gpuResourceSnapshot();
        const uint64_t pressuredSerial = pressured.frameSerial;
        passed = passed && pressured.atlasAdmissionPressure &&
            pressured.pressureProxyCount == 0u &&
            pressured.triangleAtlasPartCount == partCount &&
            pressuredTriangles >= static_cast<uint64_t>(partCount) &&
            pressuredTriangles < requestedTriangles &&
            pressured.triangleAtlasAllocatedBytes <=
                pressured.triangleAtlasBudgetBytes;
        if (passed) {
            passed = render(renderer, root) &&
                assembly->lastRenderedTriangleCount() ==
                    pressuredTriangles;
            const Obol::CadGpuResourceSnapshot replayed =
                assembly->gpuResourceSnapshot();
            passed = passed && replayed.frameSerial > pressuredSerial &&
                replayed.atlasAdmissionPressure &&
                replayed.pressureProxyCount == 0u;
        }
        if (!passed) {
            std::fprintf(stderr,
                "coverage-first atlas pressure contract failed "
                "(tier=%d rendered=%llu requested=%llu parts=%zu "
                "proxies=%zu pressure=%d allocated=%zu budget=%zu)\n",
                assembly->lastRenderTier(),
                static_cast<unsigned long long>(pressuredTriangles),
                static_cast<unsigned long long>(requestedTriangles),
                pressured.triangleAtlasPartCount,
                pressured.pressureProxyCount,
                pressured.atlasAdmissionPressure ? 1 : 0,
                pressured.triangleAtlasAllocatedBytes,
                pressured.triangleAtlasBudgetBytes);
        }
    }

    root->unref();
    for (const EnvironmentSnapshot& setting : settings) {
        if (setting.present)
            setTestEnvironment(setting.name, setting.value.c_str(), 1);
        else
            unsetTestEnvironment(setting.name);
    }
    return passed;
}

bool
indirectPressureProxyPreservesProjectedExtent()
{
    constexpr uint32_t partCount = 128u;
    constexpr uint64_t boxTriangles =
        partCount * Obol::CadAggregateProxyBoxTriangleCount;
    constexpr uint64_t boxLines =
        partCount * Obol::CadAggregateProxyBoxLineCount;

    struct EnvironmentSnapshot {
        const char *name;
        bool present;
        std::string value;
    } settings[] = {
        {"OBOL_CAD_INDIRECT", false, std::string()},
        {"OBOL_CAD_ATLAS_MB", false, std::string()},
        {"OBOL_CAD_FLAT_SHADED", false, std::string()}
    };
    for (EnvironmentSnapshot& setting : settings) {
        const char *value = std::getenv(setting.name);
        setting.present = value != nullptr;
        if (value)
            setting.value = value;
    }
    setTestEnvironment("OBOL_CAD_INDIRECT", "1", 1);
    /* An ordinary atlas page is 16 MiB.  This limit deliberately prevents
     * even the minimum representation from being admitted. */
    setTestEnvironment("OBOL_CAD_ATLAS_MB", "1", 1);
    setTestEnvironment("OBOL_CAD_FLAT_SHADED", "0", 1);

    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 10.0f);
    camera->nearDistance.setValue(0.1f);
    camera->farDistance.setValue(100.0f);
    camera->height.setValue(24.0f);
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);
    SoCADAssembly *assembly = new SoCADAssembly;
    setCadDrawMode(root, SoCADViewState::SHADED);
    root->addChild(assembly);

    Obol::TriMesh tetrahedron;
    tetrahedron.positions = {
        SbVec3f(-0.5f, -0.5f, -0.5f),
        SbVec3f( 0.5f, -0.5f, -0.5f),
        SbVec3f( 0.0f,  0.5f, -0.5f),
        SbVec3f( 0.0f,  0.0f,  0.5f)
    };
    tetrahedron.indices = {
        0u, 2u, 1u,
        0u, 1u, 3u,
        1u, 2u, 3u,
        2u, 0u, 3u
    };
    tetrahedron.bounds = SbBox3f(
        SbVec3f(-0.5f, -0.5f, -0.5f),
        SbVec3f( 0.5f,  0.5f,  0.5f));

    for (uint32_t index = 0; index < partCount; ++index) {
        char name[64] = {};
        std::snprintf(name, sizeof(name),
            "pressure-proxy-%03u", index);
        Obol::PartGeometryBuilder geometry;
        geometry.shaded = tetrahedron;
        geometry.shadedCullBackfaces = false;
        geometry.subpixelProxyEligible = true;
        const Obol::PartId part = Obol::CadIdBuilder::partId(name);
        requireCadMutation(admitAndUpsertPart(assembly, part, geometry),
            "pressure proxy part");

        Obol::InstanceRecord instance;
        instance.part = part;
        instance.parent = Obol::CadIdBuilder::rootInstance();
        instance.childName = name;
        instance.occurrenceIndex = index;
        instance.localToRoot.setTranslate(SbVec3f(
            -15.0f + 2.0f * static_cast<float>(index % 16u),
            -7.0f + 2.0f * static_cast<float>(index / 16u), 0.0f));
        requireCadMutation(assembly->upsertInstanceAuto(instance),
            "pressure proxy instance");
    }

    const SbViewportRegion viewport(384, 256);
    SoOffscreenRenderer renderer(viewport);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
    bool passed = render(renderer, root);
    if (passed && assembly->lastRenderTier() == 6) {
        const Obol::CadGpuResourceSnapshot shaded =
            assembly->gpuResourceSnapshot();
        const Obol::CadRenderedWork shadedWork =
            assembly->lastRenderedWork();
        const Obol::CadAggregateProxyPresentationWork shadedProxies =
            assembly->lastAggregateProxyPresentationWork();
        const size_t shadedPixels = nonBlackPixels(renderer);
        const bool shadedPassed = shaded.atlasAdmissionPressure &&
            shaded.pressureProxyCount == partCount &&
            shadedProxies.exact && shadedProxies.pointCount == 0u &&
            shadedProxies.axisAlignedBoxCount == partCount &&
            shadedProxies.orientedBoxCount == 0u &&
            assembly->lastSubpixelProxyDrawPointCount() == 0u &&
            shadedWork.triangleCount == boxTriangles &&
            shadedWork.lineCount == 0u &&
            shadedPixels > 0u;

        setCadDrawMode(root, SoCADViewState::SHADED_WITH_EDGES);
        const bool edgedRendered = render(renderer, root);
        const Obol::CadRenderedWork edgedWork =
            assembly->lastRenderedWork();
        const bool edgedPassed = edgedRendered &&
            assembly->gpuResourceSnapshot().pressureProxyCount == partCount &&
            assembly->lastSubpixelProxyDrawPointCount() == 0u &&
            edgedWork.triangleCount == boxTriangles &&
            edgedWork.lineCount == boxLines;

        camera->height.setValue(1024.0f);
        setCadDrawMode(root, SoCADViewState::SHADED);
        const bool pointRendered = render(renderer, root);
        const Obol::CadRenderedWork pointWork =
            assembly->lastRenderedWork();
        const Obol::CadGpuResourceSnapshot pointSnapshot =
            assembly->gpuResourceSnapshot();
        /* Once every occurrence is genuinely point-sized, ordinary
         * view-local aggregation precedes atlas admission.  The pressure
         * fallback therefore has no remaining occurrences to replace. */
        const bool pointPassed = pointRendered &&
            pointSnapshot.pressureProxyCount == 0u &&
            assembly->lastSubpixelProxyDrawPointCount() == partCount &&
            pointWork.triangleCount == 0u && pointWork.lineCount == 0u;
        passed = shadedPassed && edgedPassed && pointPassed;

        if (!passed) {
            std::fprintf(stderr,
                "atlas-pressure proxy extent contract failed "
                "(pressure=%zu shaded=%llu/%llu edged=%llu/%llu "
                "points=%zu point-work=%llu/%llu "
                "stage=%d/%d/%d point-render=%d point-pressure=%zu "
                "pixels=%zu)\n",
                shaded.pressureProxyCount,
                static_cast<unsigned long long>(shadedWork.triangleCount),
                static_cast<unsigned long long>(shadedWork.lineCount),
                static_cast<unsigned long long>(edgedWork.triangleCount),
                static_cast<unsigned long long>(edgedWork.lineCount),
                assembly->lastSubpixelProxyDrawPointCount(),
                static_cast<unsigned long long>(pointWork.triangleCount),
                static_cast<unsigned long long>(pointWork.lineCount),
                shadedPassed ? 1 : 0, edgedPassed ? 1 : 0,
                pointPassed ? 1 : 0, pointRendered ? 1 : 0,
                pointSnapshot.pressureProxyCount, shadedPixels);
        }
    }

    root->unref();
    for (const EnvironmentSnapshot& setting : settings) {
        if (setting.present)
            setTestEnvironment(setting.name, setting.value.c_str(), 1);
        else
            unsetTestEnvironment(setting.name);
    }
    return passed;
}

} // namespace

int
runCadSubpixelProxyLifecycleContract()
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 5.0f);
    camera->nearDistance.setValue(0.1f);
    camera->farDistance.setValue(100.0f);
    root->addChild(camera);

    SoCADAssembly *assembly = new SoCADAssembly;
    setCadDrawMode(root, SoCADViewState::WIREFRAME);
    root->addChild(assembly);

    Obol::PartGeometryBuilder geometry;
    geometry.wire = unitBox();
    geometry.subpixelProxyEligible = true;
    geometry.structuralProxy = true;
    const Obol::PartId part = Obol::CadIdBuilder::partId("subpixel-proxy");
    requireCadMutation(admitAndUpsertPart(assembly, part, geometry),
        "proxy lifecycle part");

    Obol::InstanceRecord instance;
    instance.part = part;
    instance.parent = Obol::CadIdBuilder::rootInstance();
    instance.childName = "proxy";
    instance.localToRoot.makeIdentity();
    instance.lodStructuralProxy = true;
    instance.style.hasColorOverride = true;
    instance.style.color = SbColor4f(1.0f, 0.0f, 0.0f, 1.0f);
    const Obol::InstanceId proxyInstance =
        requireCadValue(assembly->upsertInstanceAuto(instance),
            "proxy lifecycle instance").instance;

    // An incorrectly bounded payload must not collapse: doing so could hide
    // visible geometry outside the point proxy's projected extent.
    Obol::PartGeometryBuilder malformed = geometry;
    malformed.wire->segmentPoints[0].setValue(-0.75f, -0.5f, -0.5f);
    const Obol::PartId malformedPart =
        Obol::CadIdBuilder::partId("malformed-subpixel-proxy");
    const Obol::CadGeometryValidation malformedResult =
        admitAndUpsertPart(assembly, malformedPart, malformed);
    if (malformedResult)
        throw std::runtime_error("malformed CAD geometry was accepted");
    if (malformedResult.error !=
            Obol::CadGeometryError::NonConservativeBounds)
        throw std::runtime_error("malformed CAD geometry had wrong error");
    instance.part = malformedPart;
    instance.childName = "malformed-proxy";
    instance.occurrenceIndex = 1;
    requireCadMutation(assembly->upsertInstanceAuto(instance),
        "malformed-part reference instance");

    const SbViewportRegion viewport(256, 256);
    SoOffscreenRenderer renderer(viewport);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    /* A coalesced occurrence which still covers a recognizable screen area
     * must retain its extent.  It remains one logical aggregate and one
     * batched line stream; it is not converted into twelve scene draw calls. */
    cadViewState(root)->pointProxyPixelThreshold.setValue(64.0f);
    camera->height.setValue(20.0f);
    if (!render(renderer, root) ||
            assembly->lastSubpixelProxyCount() != 1u ||
            assembly->lastSubpixelProxyDrawPointCount() != 0u ||
            assembly->lastRenderedWork().lineCount != 12u ||
            assembly->lastRenderedWork().positionCount != 24u ||
            nonBlackPixels(renderer) == 0u) {
        std::fprintf(stderr,
            "screen-significant aggregate did not use one batched box\n");
        root->unref();
        return 1;
    }

    cadViewState(root)->pointProxyPixelThreshold.setValue(1.0f);
    camera->height.setValue(1000.0f);
    if (!render(renderer, root) || assembly->lastSubpixelProxyCount() != 1u ||
        nonBlackPixels(renderer) == 0u) {
        std::fprintf(stderr, "subpixel proxy did not collapse to a point\n");
        root->unref();
        return 1;
    }
    const Obol::CadRenderedWork collapsedWork =
        assembly->lastRenderedWork();
    if (!collapsedWork.exact ||
            collapsedWork.positionCount <= collapsedWork.lineCount * 2u) {
        std::fprintf(stderr,
            "aggregate proxy point was omitted from exact rendered work\n");
        root->unref();
        return 1;
    }
    /* A point which grows past the hard five-pixel ceiling must become a box
     * immediately.  Anti-flicker hysteresis is permitted only when shrinking
     * a box back to a point. */
    cadViewState(root)->pointProxyPixelThreshold.setValue(64.0f);
    camera->height.setValue(45.0f);
    if (!render(renderer, root) ||
            assembly->lastSubpixelProxyCount() != 1u ||
            assembly->lastSubpixelProxyDrawPointCount() != 0u ||
            assembly->lastRenderedWork().lineCount !=
                Obol::CadAggregateProxyBoxLineCount) {
        std::fprintf(stderr,
            "aggregate point exceeded the hard five-pixel ceiling\n");
        root->unref();
        return 1;
    }
    setCadDrawMode(root, SoCADViewState::SHADED);
    if (!render(renderer, root)) {
        std::fprintf(stderr,
            "shaded aggregate proxy frame did not render\n");
        root->unref();
        return 1;
    }
    const Obol::CadStructuralProxyPresentationWork shadedProxyWork =
        assembly->lastStructuralProxyPresentationWork();
    if (assembly->lastRenderedWork().lineCount != 0u ||
            assembly->lastRenderedWork().triangleCount !=
                Obol::CadAggregateProxyBoxTriangleCount ||
            !shadedProxyWork.exact ||
            shadedProxyWork.aggregatePointCount != 0u ||
            shadedProxyWork.aggregateBoxCount != 1u ||
            shadedProxyWork.retainedWireBoxCount != 0u ||
            nonBlackPixels(renderer) == 0u) {
        std::fprintf(stderr,
            "shaded aggregate proxy was not rendered as a solid box\n");
        root->unref();
        return 1;
    }
    cadViewState(root)->pointProxyPixelThreshold.setValue(1.0f);
    camera->height.setValue(1000.0f);
    /* A shaded cold view must aggregate a subpixel replacement directly,
     * rather than loading a shaded mesh merely to reach the same point. */
    if (!render(renderer, root) || assembly->lastSubpixelProxyCount() != 1u ||
            nonBlackPixels(renderer) == 0u) {
        std::fprintf(stderr,
            "shaded structural fallback did not collapse directly\n");
        root->unref();
        return 1;
    }
    const Obol::CadStructuralProxyPresentationWork pointProxyWork =
        assembly->lastStructuralProxyPresentationWork();
    if (!pointProxyWork.exact ||
            pointProxyWork.aggregatePointCount != 1u ||
            pointProxyWork.aggregateBoxCount != 0u ||
            pointProxyWork.retainedWireBoxCount != 0u) {
        std::fprintf(stderr,
            "structural aggregate point work report was not exact\n");
        root->unref();
        return 1;
    }
    setCadDrawMode(root, SoCADViewState::WIREFRAME);
    if (!render(renderer, root) || assembly->lastSubpixelProxyCount() != 1u) {
        std::fprintf(stderr,
            "wire structural fallback did not survive draw-mode restore\n");
        root->unref();
        return 1;
    }
    camera->height.setValue(320.0f);
    setCadDrawMode(root, SoCADViewState::SHADED);
    if (!render(renderer, root) || assembly->lastSubpixelProxyCount() != 1u) {
        std::fprintf(stderr,
            "pixel-sized shaded structural fallback used mesh hysteresis\n");
        root->unref();
        return 1;
    }
    camera->height.setValue(1000.0f);
    setCadDrawMode(root, SoCADViewState::WIREFRAME);
    if (!render(renderer, root) || assembly->lastSubpixelProxyCount() != 1u) {
        std::fprintf(stderr,
            "structural fallback did not restore after boundary test\n");
        root->unref();
        return 1;
    }
    const uint64_t initialPresentation =
        assembly->lastSubpixelProxyRevision();
    const uint64_t initialPlanBuilds =
        assembly->framePlanBuildCount();
    if (!initialPresentation || !render(renderer, root) ||
        assembly->lastSubpixelProxyRevision() != initialPresentation) {
        std::fprintf(stderr,
            "unchanged subpixel proxy view rebuilt presentation state\n");
        root->unref();
        return 1;
    }

    /*
     * Visibility is a retained per-instance presentation delta.  Hiding and
     * restoring one occurrence must update the aggregate point channel
     * without recompiling unrelated part/instance topology.
     */
    assembly->setHiddenInstances({proxyInstance});
    if (!render(renderer, root) || !assembly->isInstanceHidden(proxyInstance) ||
            assembly->lastSubpixelProxyCount() != 0u ||
            assembly->framePlanBuildCount() != initialPlanBuilds) {
        std::fprintf(stderr,
            "sparse hide rebuilt the frame plan or remained visible\n");
        root->unref();
        return 1;
    }
    assembly->setHiddenInstances({});
    if (!render(renderer, root) || assembly->isInstanceHidden(proxyInstance) ||
            assembly->lastSubpixelProxyCount() != 1u ||
            assembly->framePlanBuildCount() != initialPlanBuilds) {
        std::fprintf(stderr,
            "sparse visibility restore rebuilt the frame plan or stayed hidden\n");
        root->unref();
        return 1;
    }
    assembly->setSelectedInstances({proxyInstance});
    if (!render(renderer, root) || assembly->selectedInstanceCount() != 1u ||
            assembly->lastSubpixelProxyCount() != 1u ||
            assembly->framePlanBuildCount() != initialPlanBuilds) {
        std::fprintf(stderr,
            "sparse selection did not retain the styled point proxy in place "
            "(selected=%zu proxies=%zu plans=%llu/%llu)\n",
            assembly->selectedInstanceCount(),
            assembly->lastSubpixelProxyCount(),
            static_cast<unsigned long long>(assembly->framePlanBuildCount()),
            static_cast<unsigned long long>(initialPlanBuilds));
        root->unref();
        return 1;
    }
    assembly->setSelectedInstances({});
    if (!render(renderer, root) || assembly->selectedInstanceCount() != 0u ||
            assembly->lastSubpixelProxyCount() != 1u ||
            assembly->framePlanBuildCount() != initialPlanBuilds) {
        std::fprintf(stderr,
            "sparse selection clear did not restore the point proxy in place\n");
        root->unref();
        return 1;
    }

    // View importance is presentation policy, not semantic selection.  It
    // must promote/demote the same retained occurrence through the sparse
    // proxy channel without rebuilding the assembly-wide frame plan.
    assembly->setPointProxyProtectedInstances({proxyInstance});
    const uint64_t protectedRevision =
        assembly->pointProxyProtectionRevision();
    const std::vector<Obol::InstanceId> protectedSnapshot =
        assembly->pointProxyProtectedInstances();
    if (protectedSnapshot.size() != 1u ||
            protectedSnapshot[0] != proxyInstance ||
            protectedRevision == 0 ||
            assembly->lastClassifiedPointProxyProtectionRevision() !=
                protectedRevision ||
            !render(renderer, root) ||
            assembly->selectedInstanceCount() != 0u ||
            assembly->lastSubpixelProxyCount() != 0u ||
            assembly->framePlanBuildCount() != initialPlanBuilds) {
        std::fprintf(stderr,
            "point-proxy protection did not promote the retained instance\n");
        root->unref();
        return 1;
    }
    std::unordered_set<Obol::InstanceId,
        std::hash<Obol::InstanceId>> adoptedProtection;
    assembly->adoptPointProxyProtectedInstances(std::move(adoptedProtection));
    const uint64_t adoptedRevision =
        assembly->pointProxyProtectionRevision();
    if (adoptedRevision == protectedRevision ||
            assembly->lastClassifiedPointProxyProtectionRevision() ==
                adoptedRevision ||
            !assembly->pointProxyProtectedInstances().empty() ||
            !render(renderer, root) ||
            assembly->lastSubpixelProxyCount() != 1u ||
            assembly->lastClassifiedPointProxyProtectionRevision() !=
                adoptedRevision ||
            assembly->framePlanBuildCount() != initialPlanBuilds) {
        std::fprintf(stderr,
            "adopting point-proxy protection did not restore aggregation\n");
        root->unref();
        return 1;
    }

    // The box remains subpixel from the opposite side, but its nearest
    // depth-preserving corner changes.  This must advance the presentation
    // revision even though the collapsed-instance mask does not change.
    camera->position.setValue(0.0f, 0.0f, -5.0f);
    camera->orientation.setValue(SbRotation(SbVec3f(0.0f, 1.0f, 0.0f),
        3.14159265f));
    if (!render(renderer, root) || assembly->lastSubpixelProxyCount() != 1u ||
        assembly->lastSubpixelProxyRevision() == initialPresentation) {
        std::fprintf(stderr,
            "camera movement did not update subpixel proxy presentation\n");
        root->unref();
        return 1;
    }

    // Structural fallbacks use the exact declared pixel boundary rather than
    // mesh hysteresis.  The proxy now projects just above one pixel and must
    // remain a box until its mesh is available.
    camera->height.setValue(250.0f);
    if (!render(renderer, root) || assembly->lastSubpixelProxyCount() != 0u) {
        std::fprintf(stderr,
            "structural proxy remained collapsed above the pixel boundary\n");
        root->unref();
        return 1;
    }

    // Above the leave threshold, the same persistent AABB returns to its
    // normal wire representation without rebuilding scene geometry.
    camera->height.setValue(200.0f);
    if (!render(renderer, root) || assembly->lastSubpixelProxyCount() != 0u) {
        std::fprintf(stderr, "subpixel proxy did not expand back to wire\n");
        root->unref();
        return 1;
    }

    // Under measured interaction pressure the same retained occurrence may
    // enter the aggregate batch at a larger screen-error threshold.  Returning
    // to the pixel-exact threshold must restore its ordinary representation
    // without a geometry update.
    cadViewState(root)->pointProxyPixelThreshold.setValue(2.0f);
    if (!render(renderer, root) || assembly->lastSubpixelProxyCount() != 1u) {
        std::fprintf(stderr,
            "interactive point threshold did not aggregate retained proxy\n");
        root->unref();
        return 1;
    }
    cadViewState(root)->pointProxyPixelThreshold.setValue(1.0f);
    if (!render(renderer, root) || assembly->lastSubpixelProxyCount() != 0u) {
        std::fprintf(stderr,
            "pixel-exact point threshold did not restore retained proxy\n");
        root->unref();
        return 1;
    }

    /*
     * Retained shaded LoD uses the same camera-local replacement.  The
     * triangles and instance identity remain in the assembly; only the draw
     * channel changes, so a near view promotes the mesh without an update or
     * reload.
     */
    Obol::PartGeometryBuilder shadedSubpixel;
    Obol::TriMesh shadedMesh;
    shadedMesh.positions = {
        SbVec3f(-0.5f, -0.5f, 0.0f),
        SbVec3f( 0.5f, -0.5f, 0.0f),
        SbVec3f( 0.5f,  0.5f, 0.0f),
        SbVec3f(-0.5f,  0.5f, 0.0f)
    };
    shadedMesh.indices = {0, 1, 2, 0, 2, 3};
    shadedMesh.bounds.makeEmpty();
    for (const SbVec3f& point : shadedMesh.positions)
        shadedMesh.bounds.extendBy(point);
    shadedSubpixel.shaded = std::move(shadedMesh);
    shadedSubpixel.subpixelProxyEligible = true;
    const Obol::PartId shadedSubpixelPart =
        Obol::CadIdBuilder::partId("shaded-subpixel");
    requireCadMutation(
        admitAndUpsertPart(assembly, shadedSubpixelPart, shadedSubpixel),
        "shaded subpixel part");
    Obol::InstanceRecord shadedInstance;
    shadedInstance.part = shadedSubpixelPart;
    shadedInstance.parent = Obol::CadIdBuilder::rootInstance();
    shadedInstance.childName = "shaded-subpixel";
    shadedInstance.localToRoot.makeIdentity();
    const Obol::InstanceId shadedSubpixelInstance =
        requireCadValue(assembly->upsertInstanceAuto(shadedInstance),
            "shaded subpixel instance").instance;
    setCadDrawMode(root, SoCADViewState::SHADED);
    camera->height.setValue(1000.0f);
    if (!render(renderer, root) ||
            assembly->lastSubpixelProxyCount() != 2u) {
        std::fprintf(stderr,
            "subpixel shaded LoD did not enter the aggregate point batch\n");
        root->unref();
        return 1;
    }
    /* Ordinary retained meshes keep the 0.75/1.25 Schmitt band.  At this
     * view the structural fallback expands at the exact one-pixel boundary,
     * while the shaded occurrence remains collapsed until it crosses the
     * wider leave threshold. */
    camera->height.setValue(250.0f);
    if (!render(renderer, root) ||
            assembly->lastSubpixelProxyCount() != 1u) {
        std::fprintf(stderr,
            "retained mesh did not preserve subpixel hysteresis\n");
        root->unref();
        return 1;
    }
    camera->height.setValue(200.0f);
    if (!render(renderer, root) ||
            assembly->lastSubpixelProxyCount() != 0u) {
        std::fprintf(stderr,
            "retained mesh did not leave subpixel hysteresis band\n");
        root->unref();
        return 1;
    }
    camera->height.setValue(2.0f);
    if (!render(renderer, root) ||
            assembly->lastSubpixelProxyCount() != 0u) {
        std::fprintf(stderr,
            "near shaded LoD did not promote its retained mesh in place\n");
        root->unref();
        return 1;
    }
    assembly->removeInstance(shadedSubpixelInstance);
    assembly->removePart(shadedSubpixelPart);

    /*
     * More than 128 independently authored wire parts select the flat batch.
     * Keep one retained progressive shaded part in the same assembly: this is
     * the mixed cold-start state that must not disable box batching.  Compare
     * it with the ordinary per-part renderer so batching cannot silently
     * change authored coordinates or occurrence placement.
     */
    setCadDrawMode(root, SoCADViewState::SHADED_WITH_EDGES);
    for (int i = 0; i < 144; ++i) {
        Obol::PartGeometryBuilder box;
        box.wire = unitBox();
        box.subpixelProxyEligible = true;
        box.structuralProxy = true;
        const float sx = 0.65f + 0.07f * static_cast<float>(i % 5);
        const float sy = 0.55f + 0.05f * static_cast<float>((i / 5) % 7);
        const float sz = 0.45f + 0.03f * static_cast<float>((i / 11) % 9);
        box.wire->bounds.makeEmpty();
        for (SbVec3f& point : box.wire->segmentPoints) {
            point.setValue(point[0] * sx + 0.11f * static_cast<float>(i % 3),
                           point[1] * sy - 0.09f * static_cast<float>(i % 4),
                           point[2] * sz);
            box.wire->bounds.extendBy(point);
        }
        char partName[64] = {};
        std::snprintf(partName, sizeof(partName), "distinct-proxy-%03d", i);
        const Obol::PartId boxPart = Obol::CadIdBuilder::partId(partName);
        requireCadMutation(admitAndUpsertPart(assembly, boxPart, box),
            "streamed box part");

        Obol::InstanceRecord boxInstance;
        boxInstance.part = boxPart;
        boxInstance.parent = Obol::CadIdBuilder::rootInstance();
        boxInstance.childName = partName;
        boxInstance.occurrenceIndex = static_cast<uint32_t>(i + 2);
        boxInstance.lodStructuralProxy = true;
        boxInstance.localToRoot.setTranslate(SbVec3f(
            -27.5f + 5.0f * static_cast<float>(i % 12),
            -27.5f + 5.0f * static_cast<float>(i / 12), 0.0f));
        boxInstance.style.hasColorOverride = true;
        boxInstance.style.color = SbColor4f(
            (i & 1) ? 1.0f : 0.2f,
            (i & 2) ? 0.8f : 0.25f,
            (i & 4) ? 0.9f : 0.3f, 1.0f);
        requireCadMutation(assembly->upsertInstanceAuto(boxInstance),
            "streamed box instance");
    }

    Obol::PartGeometryBuilder progressiveGeometry;
    Obol::TriMesh triangle;
    triangle.positions = {
        SbVec3f(-1.5f, -1.0f, -0.25f),
        SbVec3f(1.5f, -1.0f, -0.25f),
        SbVec3f(0.0f, 1.5f, -0.25f)
    };
    triangle.indices = {0, 1, 2};
    triangle.bounds = SbBox3f(
        SbVec3f(-1.5f, -1.0f, -0.25f),
        SbVec3f(1.5f, 1.5f, -0.25f));
    triangle.progressiveMinimumCut = 15;
    triangle.progressiveResidentCut = 15;
    setProgressiveCuts(triangle, 16, 0, 0);
    triangle.progressiveCuts[15].indexCount = 3;
    triangle.progressiveCuts[15].positionCount = 3;
    triangle.progressiveQuantizationMinimum = triangle.bounds.getMin();
    triangle.progressiveQuantizationMaximum = triangle.bounds.getMax();
    progressiveGeometry.shaded = std::move(triangle);
    const Obol::PartId progressivePart =
        Obol::CadIdBuilder::partId("mixed-progressive-triangle");
    requireCadMutation(
        admitAndUpsertPart(assembly, progressivePart, progressiveGeometry),
        "streamed progressive part");
    Obol::InstanceRecord progressiveInstance;
    progressiveInstance.part = progressivePart;
    progressiveInstance.parent = Obol::CadIdBuilder::rootInstance();
    progressiveInstance.childName = "mixed-progressive-triangle";
    progressiveInstance.localToRoot.makeIdentity();
    requireCadMutation(assembly->upsertInstanceAuto(progressiveInstance),
        "streamed progressive instance");

    camera->height.setValue(70.0f);
    setTestEnvironment("OBOL_CAD_FLAT_WIRE", "0", 1);
    if (!render(renderer, root)) {
        std::fprintf(stderr, "per-part mixed proxy reference did not render\n");
        root->unref();
        return 1;
    }
    const SbVec2s imageSize = renderer.getViewportRegion().getViewportSizePixels();
    const size_t imageBytes = static_cast<size_t>(imageSize[0]) *
        static_cast<size_t>(imageSize[1]) * 3u;
    std::vector<unsigned char> reference(
        renderer.getBuffer(), renderer.getBuffer() + imageBytes);

    setTestEnvironment("OBOL_CAD_FLAT_WIRE", "1", 1);
    if (!render(renderer, root) || assembly->lastRenderTier() != 5) {
        std::fprintf(stderr,
            "mixed progressive scene did not retain the flat wire batch\n");
        root->unref();
        return 1;
    }
    const unsigned char *batched = renderer.getBuffer();
    size_t changedPixels = 0;
    size_t coveredPixels = 0;
    for (size_t p = 0; p < imageBytes; p += 3) {
        const bool referenceCovered =
            reference[p] || reference[p + 1] || reference[p + 2];
        const bool batchedCovered =
            batched[p] || batched[p + 1] || batched[p + 2];
        if (referenceCovered || batchedCovered)
            ++coveredPixels;
        if (std::memcmp(reference.data() + p, batched + p, 3) != 0)
            ++changedPixels;
    }
    if (!coveredPixels || changedPixels > coveredPixels / 50u) {
        std::fprintf(stderr,
            "flat wire batch changed mixed-scene placement (%zu/%zu pixels)\n",
            changedPixels, coveredPixels);
        root->unref();
        return 1;
    }

    /*
     * qged selection changes both the retained style color and the selected
     * flag.  In a mixed shaded-with-edges assembly, a color-only style change
     * must patch the instance stream just like the flag change; recompiling
     * the complete frame plan makes selection latency scale with scene size.
     */
    const uint64_t mixedPlanBuilds = assembly->framePlanBuildCount();
    Obol::InstanceStyle selectedStyle;
    selectedStyle.hasColorOverride = true;
    selectedStyle.color = SbColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    Obol::InstanceStyleUpdate selectedStyleUpdate;
    selectedStyleUpdate.instance = proxyInstance;
    selectedStyleUpdate.style = selectedStyle;
    requireCadMutation(assembly->updateInstanceStyles({selectedStyleUpdate}),
        "streamed selected style");
    assembly->setSelectedInstances({proxyInstance});
    if (!render(renderer, root) ||
            assembly->selectedInstanceCount() != 1u ||
            assembly->framePlanBuildCount() != mixedPlanBuilds) {
        std::fprintf(stderr,
            "mixed-mode sparse style/selection rebuilt the frame plan\n");
        root->unref();
        return 1;
    }
    assembly->setSelectedInstances({});
    if (!render(renderer, root) ||
            assembly->selectedInstanceCount() != 0u ||
            assembly->framePlanBuildCount() != mixedPlanBuilds) {
        std::fprintf(stderr,
            "mixed-mode sparse selection clear rebuilt the frame plan\n");
        root->unref();
        return 1;
    }

    /*
     * A streamed leaf normally changes from a unique structural box part to a
     * newly available progressive mesh part.  That is a presentation-slot
     * rebind, not arbitrary assembly topology: adding the unreferenced mesh
     * and rebinding the unique occurrence must retain the compiled plan.
     */
    Obol::PartGeometryBuilder rebindBox;
    rebindBox.wire = unitBox();
    rebindBox.subpixelProxyEligible = true;
    rebindBox.structuralProxy = true;
    const Obol::PartId rebindBoxPart =
        Obol::CadIdBuilder::partId("stream-rebind-box");
    requireCadMutation(admitAndUpsertPart(assembly, rebindBoxPart, rebindBox),
        "rebind box part");
    Obol::InstanceRecord rebindRecord;
    rebindRecord.part = rebindBoxPart;
    rebindRecord.parent = Obol::CadIdBuilder::rootInstance();
    rebindRecord.childName = "stream-rebind";
    rebindRecord.occurrenceIndex = 9001;
    rebindRecord.lodStructuralProxy = true;
    rebindRecord.localToRoot.setTranslate(SbVec3f(0.0f, 20.0f, 0.0f));
    const Obol::InstanceId rebindInstance =
        requireCadValue(assembly->upsertInstanceAuto(rebindRecord),
            "rebind instance").instance;
    if (!render(renderer, root)) {
        std::fprintf(stderr, "stream rebind setup did not render\n");
        root->unref();
        return 1;
    }
    const uint64_t rebindPlanBuilds = assembly->framePlanBuildCount();
    const size_t rebindPlanInstances =
        assembly->framePlanInstanceRecordCount();
    const Obol::PartId rebindMeshPart =
        Obol::CadIdBuilder::partId("stream-rebind-mesh");
    requireCadMutation(
        admitAndUpsertPart(assembly, rebindMeshPart, progressiveGeometry),
        "rebind mesh part");
    rebindRecord.part = rebindMeshPart;
    rebindRecord.lodStructuralProxy = false;
    Obol::InstanceUpdate rebindUpdate;
    rebindUpdate.instance = rebindInstance;
    rebindUpdate.record = rebindRecord;
    requireCadMutation(assembly->upsertInstances({rebindUpdate}),
        "part rebind");
    if (!render(renderer, root) ||
            assembly->framePlanBuildCount() != rebindPlanBuilds ||
            assembly->framePlanInstanceRecordCount() !=
                rebindPlanInstances) {
        std::fprintf(stderr,
            "unique box-to-mesh rebind rebuilt or duplicated the frame plan\n");
        root->unref();
        return 1;
    }

    /*
     * A retained LoD publication is not category-homogeneous: a wave can
     * promote structural boxes while advancing cuts on meshes published by
     * an earlier wave.  Each operation is sparsely patchable and their
     * combination must remain sparsely patchable as well.  This mirrors the
     * large-scene batch which used to trigger one complete plan rebuild per
     * 256-512 occurrence publication.
     */
    const Obol::PartId mixedRebindBoxPart =
        Obol::CadIdBuilder::partId("mixed-wave-rebind-box");
    requireCadMutation(admitAndUpsertPart(assembly, mixedRebindBoxPart, rebindBox),
        "mixed rebind box part");
    Obol::InstanceRecord mixedRebindRecord = rebindRecord;
    mixedRebindRecord.part = mixedRebindBoxPart;
    mixedRebindRecord.childName = "mixed-wave-rebind";
    mixedRebindRecord.occurrenceIndex = 9006;
    mixedRebindRecord.lodStructuralProxy = true;
    mixedRebindRecord.localToRoot.setTranslate(
        SbVec3f(-8.0f, 20.0f, 0.0f));
    const Obol::InstanceId mixedRebindInstance =
        requireCadValue(assembly->upsertInstanceAuto(mixedRebindRecord),
            "mixed rebind instance").instance;

    Obol::InstanceRecord mixedCutRecord = progressiveInstance;
    mixedCutRecord.childName = "mixed-wave-cut";
    mixedCutRecord.occurrenceIndex = 9007;
    mixedCutRecord.lodCut = 15;
    mixedCutRecord.localToRoot.setTranslate(
        SbVec3f(8.0f, 20.0f, 0.0f));
    const Obol::InstanceId mixedCutInstance =
        requireCadValue(assembly->upsertInstanceAuto(mixedCutRecord),
            "mixed cut instance").instance;
    if (!render(renderer, root)) {
        std::fprintf(stderr, "mixed rebind/cut setup did not render\n");
        root->unref();
        return 1;
    }
    const uint64_t mixedRebindPlanBuilds =
        assembly->framePlanBuildCount();
    const size_t mixedRebindPlanInstances =
        assembly->framePlanInstanceRecordCount();

    const Obol::PartId mixedRebindMeshPart =
        Obol::CadIdBuilder::partId("mixed-wave-rebind-mesh");
    requireCadMutation(
        admitAndUpsertPart(assembly, mixedRebindMeshPart, progressiveGeometry),
        "mixed rebind mesh part");
    mixedRebindRecord.part = mixedRebindMeshPart;
    mixedRebindRecord.lodStructuralProxy = false;
    Obol::InstanceUpdate mixedRebindUpdate;
    mixedRebindUpdate.instance = mixedRebindInstance;
    mixedRebindUpdate.record = mixedRebindRecord;
    mixedCutRecord.lodCut = 14;
    Obol::InstanceUpdate mixedCutUpdate;
    mixedCutUpdate.instance = mixedCutInstance;
    mixedCutUpdate.record = mixedCutRecord;
    requireCadMutation(
        assembly->upsertInstances({mixedRebindUpdate, mixedCutUpdate}),
        "mixed rebind instances");
    const std::optional<Obol::InstanceRecord> retainedMixedRebind =
        assembly->getInstanceRecord(mixedRebindInstance);
    const std::optional<Obol::InstanceRecord> retainedMixedCut =
        assembly->getInstanceRecord(mixedCutInstance);
    if (!render(renderer, root) ||
            assembly->framePlanBuildCount() != mixedRebindPlanBuilds ||
            assembly->framePlanInstanceRecordCount() !=
                mixedRebindPlanInstances ||
            !retainedMixedRebind ||
            !(retainedMixedRebind->part == mixedRebindMeshPart) ||
            retainedMixedRebind->lodStructuralProxy ||
            !retainedMixedCut || retainedMixedCut->lodCut != 14) {
        std::fprintf(stderr,
            "mixed sparse rebind/cut rebuilt or corrupted the frame plan\n");
        root->unref();
        return 1;
    }
    /*
     * The retired box library entry is now referenced only by the hidden
     * compiled tombstone.  Its plan binding owns the immutable payload until
     * compaction, so removing the library entry must not invalidate the live
     * mesh plan.
     */
    assembly->removePart(rebindBoxPart);
    if (!render(renderer, root) ||
            assembly->framePlanBuildCount() != rebindPlanBuilds) {
        std::fprintf(stderr,
            "retired tombstone part removal rebuilt the frame plan\n");
        root->unref();
        return 1;
    }
    const size_t sharedProxyBaseline =
        assembly->lastUncollapsedStructuralProxyCount();

    /*
     * Real cold delivery waves mix newly discovered occurrences with older
     * occurrences promoted out of one deduplicated structural-box part.  The
     * shared box range must stay compiled while the promoted occurrence is
     * internally tombstoned and redirected to its mesh tail record.
     */
    const Obol::PartId sharedBoxPart =
        Obol::CadIdBuilder::partId("stream-shared-box");
    requireCadMutation(admitAndUpsertPart(assembly, sharedBoxPart, rebindBox),
        "shared box part");
    Obol::InstanceRecord sharedBoxA = rebindRecord;
    sharedBoxA.part = sharedBoxPart;
    sharedBoxA.lodStructuralProxy = true;
    sharedBoxA.childName = "stream-shared-box-a";
    sharedBoxA.occurrenceIndex = 9002;
    sharedBoxA.localToRoot.setTranslate(SbVec3f(-4.0f, 24.0f, 0.0f));
    const Obol::InstanceId sharedBoxAId =
        requireCadValue(assembly->upsertInstanceAuto(sharedBoxA),
            "shared box instance A").instance;
    Obol::InstanceRecord sharedBoxB = sharedBoxA;
    sharedBoxB.childName = "stream-shared-box-b";
    sharedBoxB.occurrenceIndex = 9003;
    sharedBoxB.localToRoot.setTranslate(SbVec3f(4.0f, 24.0f, 0.0f));
    const Obol::InstanceId sharedBoxBId =
        requireCadValue(assembly->upsertInstanceAuto(sharedBoxB),
            "shared box instance B").instance;
    if (!render(renderer, root) ||
            assembly->lastUncollapsedStructuralProxyCount() !=
                sharedProxyBaseline + 2u) {
        std::fprintf(stderr, "shared stream rebind setup did not render\n");
        root->unref();
        return 1;
    }
    const auto sharedStructural =
        assembly->lastUncollapsedStructuralProxyInstances();
    if (sharedStructural.size() !=
            assembly->lastUncollapsedStructuralProxyCount() ||
            std::find(sharedStructural.begin(), sharedStructural.end(),
                sharedBoxAId) == sharedStructural.end() ||
            std::find(sharedStructural.begin(), sharedStructural.end(),
                sharedBoxBId) == sharedStructural.end()) {
        std::fprintf(stderr,
            "structural proxy occurrence frontier did not match count\n");
        root->unref();
        return 1;
    }
    const Obol::CadStructuralProxyProjectionHistogram sharedProjection =
        assembly->lastStructuralProxyProjectionHistogram();
    bool cumulativeProjectionValid = sharedProjection.exact &&
        sharedProjection.revision != 0 &&
        sharedProjection.visibleCount >= sharedStructural.size();
    uint64_t previousProjectionCount = 0;
    for (const uint64_t count : sharedProjection.cumulativeCount) {
        cumulativeProjectionValid = cumulativeProjectionValid &&
            count >= previousProjectionCount &&
            count <= sharedProjection.visibleCount;
        previousProjectionCount = count;
    }
    if (!cumulativeProjectionValid) {
        std::fprintf(stderr,
            "structural projected-size histogram was not exact/cumulative "
            "exact=%d revision=%llu visible=%llu buckets=%llu,%llu,%llu,"
            "%llu,%llu,%llu,%llu frontier=%zu\n",
            sharedProjection.exact ? 1 : 0,
            static_cast<unsigned long long>(sharedProjection.revision),
            static_cast<unsigned long long>(sharedProjection.visibleCount),
            static_cast<unsigned long long>(
                sharedProjection.cumulativeCount[0]),
            static_cast<unsigned long long>(
                sharedProjection.cumulativeCount[1]),
            static_cast<unsigned long long>(
                sharedProjection.cumulativeCount[2]),
            static_cast<unsigned long long>(
                sharedProjection.cumulativeCount[3]),
            static_cast<unsigned long long>(
                sharedProjection.cumulativeCount[4]),
            static_cast<unsigned long long>(
                sharedProjection.cumulativeCount[5]),
            static_cast<unsigned long long>(
                sharedProjection.cumulativeCount[6]),
            sharedStructural.size());
        root->unref();
        return 1;
    }
    const std::array<float,
        Obol::CadStructuralProxyProjectionHistogram::BucketCount>
        structuralBucketLimits = {1.0f, 2.0f, 4.0f, 8.0f,
            16.0f, 32.0f, 64.0f};
    for (size_t bucket = 0; bucket < structuralBucketLimits.size();
            ++bucket) {
        const std::vector<Obol::InstanceId> above =
            assembly->lastStructuralProxyInstancesAbovePixels(
                structuralBucketLimits[bucket]);
        const uint64_t expected = sharedProjection.visibleCount -
            sharedProjection.cumulativeCount[bucket];
        bool sortedUnique = true;
        for (size_t instance = 1; instance < above.size(); ++instance) {
            const Obol::InstanceId &previous = above[instance - 1];
            const Obol::InstanceId &current = above[instance];
            sortedUnique = sortedUnique &&
                (previous.w0 < current.w0 ||
                 (previous.w0 == current.w0 && previous.w1 < current.w1));
        }
        if (above.size() != expected || !sortedUnique) {
            std::fprintf(stderr,
                "structural projected-size frontier mismatch "
                "limit=%.0f expected=%llu actual=%zu sorted=%d\n",
                structuralBucketLimits[bucket],
                static_cast<unsigned long long>(expected), above.size(),
                sortedUnique ? 1 : 0);
            root->unref();
            return 1;
        }
    }
    const uint64_t mixedStreamPlanBuilds =
        assembly->framePlanBuildCount();
    assembly->setHiddenInstances({sharedBoxAId});
    if (!render(renderer, root) ||
            assembly->lastUncollapsedStructuralProxyCount() !=
                sharedProxyBaseline + 1u ||
            assembly->framePlanBuildCount() != mixedStreamPlanBuilds ||
            !assembly->lastStructuralProxyProjectionHistogram().exact ||
            assembly->lastStructuralProxyProjectionHistogram().visibleCount +
                1u != sharedProjection.visibleCount) {
        std::fprintf(stderr,
            "sparse hide retained an uncollapsed structural proxy or "
            "rebuilt the complete frame plan\n");
        root->unref();
        return 1;
    }
    assembly->setHiddenInstances({});
    if (!render(renderer, root) ||
            assembly->lastUncollapsedStructuralProxyCount() !=
                sharedProxyBaseline + 2u ||
            assembly->framePlanBuildCount() != mixedStreamPlanBuilds ||
            !assembly->lastStructuralProxyProjectionHistogram().exact ||
            assembly->lastStructuralProxyProjectionHistogram().visibleCount !=
                sharedProjection.visibleCount) {
        std::fprintf(stderr,
            "sparse restore lost an uncollapsed structural proxy or "
            "rebuilt the complete frame plan\n");
        root->unref();
        return 1;
    }

    /* A structural occurrence wholly outside the camera frustum is not a
     * visible fallback and must not make a view-convergence client attempt to
     * realize geometry for it.  It shares the live box part so this also
     * exercises the batched range accounting used by large CAD assemblies. */
    Obol::InstanceRecord offscreenBox = sharedBoxB;
    offscreenBox.childName = "stream-offscreen-box";
    offscreenBox.occurrenceIndex = 9005;
    offscreenBox.localToRoot.setTranslate(
        SbVec3f(100000.0f, 24.0f, 0.0f));
    const Obol::InstanceId offscreenBoxId =
        requireCadValue(assembly->upsertInstanceAuto(offscreenBox),
            "offscreen box instance").instance;
    if (!render(renderer, root) ||
            assembly->lastUncollapsedStructuralProxyCount() !=
                sharedProxyBaseline + 2u ||
            !assembly->lastStructuralProxyProjectionHistogram().exact ||
            assembly->lastStructuralProxyProjectionHistogram().visibleCount !=
                sharedProjection.visibleCount) {
        std::fprintf(stderr,
            "off-frustum box was counted as a visible structural proxy\n");
        root->unref();
        return 1;
    }
    const auto offscreenStructural =
        assembly->lastUncollapsedStructuralProxyInstances();
    if (offscreenStructural.size() !=
            assembly->lastUncollapsedStructuralProxyCount() ||
            std::find(offscreenStructural.begin(), offscreenStructural.end(),
                offscreenBoxId) != offscreenStructural.end()) {
        std::fprintf(stderr,
            "off-frustum occurrence entered structural repair frontier\n");
        root->unref();
        return 1;
    }
    Obol::InstanceRecord promotedShared = sharedBoxA;
    promotedShared.part = rebindMeshPart;
    promotedShared.lodStructuralProxy = false;
    Obol::InstanceUpdate promotedUpdate;
    promotedUpdate.instance = sharedBoxAId;
    promotedUpdate.record = promotedShared;

    Obol::InstanceRecord newStreamed = promotedShared;
    newStreamed.childName = "stream-new-mesh";
    newStreamed.occurrenceIndex = 9004;
    newStreamed.localToRoot.setTranslate(SbVec3f(0.0f, 28.0f, 0.0f));
    const Obol::InstanceId newStreamedId =
        Obol::CadIdBuilder::childInstance(
            newStreamed.parent, newStreamed.childName,
            newStreamed.occurrenceIndex, newStreamed.boolOp);
    Obol::InstanceUpdate newStreamedUpdate;
    newStreamedUpdate.instance = newStreamedId;
    newStreamedUpdate.record = newStreamed;
    requireCadMutation(
        assembly->upsertInstances({promotedUpdate, newStreamedUpdate}),
        "promoted streamed instances");
    if (!render(renderer, root) ||
            assembly->framePlanBuildCount() != mixedStreamPlanBuilds ||
            assembly->lastUncollapsedStructuralProxyCount() !=
                sharedProxyBaseline + 1u ||
            !assembly->lastStructuralProxyProjectionHistogram().exact ||
            assembly->lastStructuralProxyProjectionHistogram().visibleCount +
                1u != sharedProjection.visibleCount) {
        std::fprintf(stderr,
            "mixed shared-box promotion rebuilt the complete frame plan "
            "or retained its stale box presentation\n");
        root->unref();
        return 1;
    }
    const auto promotedStructural =
        assembly->lastUncollapsedStructuralProxyInstances();
    if (promotedStructural.size() !=
            assembly->lastUncollapsedStructuralProxyCount() ||
            std::find(promotedStructural.begin(), promotedStructural.end(),
                sharedBoxAId) != promotedStructural.end() ||
            std::find(promotedStructural.begin(), promotedStructural.end(),
                sharedBoxBId) == promotedStructural.end()) {
        std::fprintf(stderr,
            "box-to-mesh promotion left stale structural frontier entry\n");
        root->unref();
        return 1;
    }
    assembly->setHiddenInstances({offscreenBoxId});

    /*
     * Subsequent cold-delivery waves may append more occurrences of the
     * existing structural-box part after unrelated mesh records.  Extending
     * the old compiled range across those records is valid sparse storage,
     * but those intervening records are holes, not box instances.  Every
     * renderer and proxy-accounting consumer must honor current partIndex
     * membership rather than treating range containment as ownership.
     */
    for (uint32_t wave = 0; wave < 8u; ++wave) {
        char boxName[64] = {};
        std::snprintf(
            boxName, sizeof(boxName), "stream-late-box-%u", wave);
        Obol::InstanceRecord lateBox = sharedBoxB;
        lateBox.childName = boxName;
        lateBox.occurrenceIndex = 9100u + wave;
        lateBox.localToRoot.setTranslate(SbVec3f(
            -14.0f + 4.0f * static_cast<float>(wave),
            30.0f, 0.0f));
        Obol::InstanceUpdate lateBoxUpdate;
        lateBoxUpdate.instance =
            Obol::CadIdBuilder::childInstance(
                lateBox.parent, lateBox.childName,
                lateBox.occurrenceIndex, lateBox.boolOp);
        lateBoxUpdate.record = lateBox;

        char meshName[64] = {};
        std::snprintf(
            meshName, sizeof(meshName), "stream-late-mesh-%u", wave);
        Obol::InstanceRecord lateMesh = newStreamed;
        lateMesh.childName = meshName;
        lateMesh.occurrenceIndex = 9200u + wave;
        lateMesh.localToRoot.setTranslate(SbVec3f(
            -14.0f + 4.0f * static_cast<float>(wave),
            34.0f, 0.0f));
        Obol::InstanceUpdate lateMeshUpdate;
        lateMeshUpdate.instance =
            Obol::CadIdBuilder::childInstance(
                lateMesh.parent, lateMesh.childName,
                lateMesh.occurrenceIndex, lateMesh.boolOp);
        lateMeshUpdate.record = lateMesh;

        requireCadMutation(assembly->upsertInstances(
            {lateMeshUpdate, lateBoxUpdate}), "late streamed instances");
        if (!render(renderer, root) ||
                assembly->framePlanBuildCount() !=
                    mixedStreamPlanBuilds ||
                assembly->lastUncollapsedStructuralProxyCount() !=
                    sharedProxyBaseline + 2u + wave) {
            std::fprintf(stderr,
                "interleaved sparse delivery wave %u retained stale "
                "draw-range members\n", wave);
            root->unref();
            return 1;
        }
    }

    // Scene-wide LoD admission may retire thousands of distinct mesh parts
    // in one camera epoch.  The bulk API must remove the complete set without
    // changing unrelated parts or requiring one full instance scan per part.
    std::vector<Obol::PartId> removalParts;
    for (int i = 0; i < 3; ++i) {
        char name[64] = {};
        std::snprintf(name, sizeof(name), "bulk-remove-part-%d", i);
        const Obol::PartId id = Obol::CadIdBuilder::partId(name);
        requireCadMutation(admitAndUpsertPart(assembly, id, geometry),
            "stream compaction part");
        removalParts.push_back(id);
    }
    const size_t beforeBulkRemove = assembly->partCount();
    assembly->removeParts(removalParts);
    if (assembly->partCount() + removalParts.size() != beforeBulkRemove) {
        std::fprintf(stderr, "bulk part removal did not remove exact set\n");
        root->unref();
        return 1;
    }

    /* Hosts use this token to distinguish a one-time retained preparation
     * deadline from steady draw overload.  An unchanged prepared replay must
     * eventually leave it stable, while a camera-classification input change
     * must advance it. */
    bool observedSteadyReplay = false;
    for (int attempt = 0; attempt < 3 && !observedSteadyReplay; ++attempt) {
        const uint64_t before = assembly->renderPreparationSerial();
        if (!render(renderer, root)) {
            std::fprintf(stderr,
                "preparation-serial replay did not render\n");
            root->unref();
            return 1;
        }
        observedSteadyReplay =
            assembly->renderPreparationSerial() == before;
    }
    const uint64_t beforeClassification =
        assembly->renderPreparationSerial();
    const float threshold = cadViewState(root)->pointProxyPixelThreshold.getValue();
    cadViewState(root)->pointProxyPixelThreshold.setValue(
        threshold < 64.0f ? threshold + 1.0f : threshold - 1.0f);
    if (!observedSteadyReplay || !render(renderer, root) ||
            assembly->renderPreparationSerial() == beforeClassification) {
        std::fprintf(stderr,
            "preparation serial did not separate replay from "
            "classification work\n");
        root->unref();
        return 1;
    }

    root->unref();
    return 0;
}

bool
clearReleasesCompiledPlanGeometry()
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoOrthographicCamera *camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 5.0f);
    camera->nearDistance.setValue(0.1f);
    camera->farDistance.setValue(100.0f);
    camera->height.setValue(4.0f);
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);
    setCadDrawMode(root, SoCADViewState::SHADED);
    SoCADAssembly *assembly = new SoCADAssembly;
    root->addChild(assembly);

    const Obol::PartId part =
        Obol::CadIdBuilder::partId("clear-plan-retention-part");
    const Obol::InstanceId instance =
        Obol::CadIdBuilder::instanceId("clear-plan-retention-instance");
    std::weak_ptr<const Obol::PartGeometry> geometryLifetime;
    {
        Obol::PartGeometryBuilder builder;
        builder.shaded.emplace();
        builder.shaded->positions = {
            SbVec3f(-0.5f, -0.5f, 0.0f),
            SbVec3f(0.5f, -0.5f, 0.0f),
            SbVec3f(0.0f, 0.5f, 0.0f)};
        builder.shaded->indices = {0u, 1u, 2u};
        builder.shaded->bounds = SbBox3f(
            SbVec3f(-0.5f, -0.5f, 0.0f),
            SbVec3f(0.5f, 0.5f, 0.0f));
        const Obol::CadGeometryAdmission admitted =
            Obol::cadAdmitPartGeometry(std::move(builder));
        if (!admitted) {
            root->unref();
            return false;
        }
        geometryLifetime = admitted.geometry.shared();
        requireCadMutation(assembly->upsertParts(
            {{part, admitted.geometry, false}}),
            "clear retention part");
        Obol::InstanceRecord record;
        record.part = part;
        requireCadMutation(assembly->upsertInstance(instance, record),
            "clear retention instance");
    }

    SoOffscreenRenderer renderer(SbViewportRegion(128, 128));
    renderer.setComponents(SoOffscreenRenderer::RGB);
    const bool warmed = render(renderer, root) &&
        assembly->framePlanBuildCount() != 0u &&
        !geometryLifetime.expired();
    assembly->clear();
    const bool released = geometryLifetime.expired() &&
        assembly->partCount() == 0u && assembly->instanceCount() == 0u;
    root->unref();
    return warmed && released;
}

#include "render_test_registration.h"

class CadSubpixelProxyContracts : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        SoCADAssembly::initClass();
    }
};

TEST_F(CadSubpixelProxyContracts, SparseUniformClustersRemainValid)
{
    EXPECT_TRUE(sparseUniformClusterContract());
}

TEST_F(CadSubpixelProxyContracts, ProjectedProxyClassificationHandlesClipEdges)
{
    EXPECT_TRUE(sharedProjectedProxyContract());
}

TEST_F(CadSubpixelProxyContracts, DegenerateStructuralProxyKeepsItsPlane)
{
    EXPECT_TRUE(degenerateStructuralProxyContract());
}

TEST_F(CadSubpixelProxyContracts, OrientedAggregateProxyIsRetainedAndRendered)
{
    EXPECT_TRUE(orientedAggregateProxyContract());
}

TEST_F(CadSubpixelProxyContracts, SoftwareAggregationPreservesLogicalCoverage)
{
    EXPECT_TRUE(softwareSubpixelProxyAggregationContract());
}

TEST_F(CadSubpixelProxyContracts, LifecycleAndStreamingStateRemainCoherent)
{
    EXPECT_EQ(runCadSubpixelProxyLifecycleContract(), 0);
}

TEST_F(CadSubpixelProxyContracts, NormalFreeTwoSidedGlslMatchesFixedPipeline)
{
    EXPECT_TRUE(normalFreeTwoSidedGlslMatchesFixed());
}

TEST_F(CadSubpixelProxyContracts, NonUniformNormalTransformMatchesFixedPipeline)
{
    EXPECT_TRUE(nonUniformNormalTransformMatchesFixed());
}

TEST_F(CadSubpixelProxyContracts, IndirectProgressiveAtlasGrows)
{
    EXPECT_TRUE(indirectProgressiveAtlasGrows());
}

TEST_F(CadSubpixelProxyContracts, IndirectGenerationAppendsOnlyItsSuffix)
{
    EXPECT_TRUE(indirectProgressiveGenerationAppendsSuffix());
}

TEST_F(CadSubpixelProxyContracts, OrdinaryGenerationAppendsOnlyItsSuffix)
{
    EXPECT_TRUE(ordinaryProgressiveGenerationAppendsSuffix());
}

TEST_F(CadSubpixelProxyContracts, ZeroLineageReplacementDoesNotOverread)
{
    EXPECT_TRUE(ordinaryProgressiveZeroLineageReplacesWithoutOverread());
}

TEST_F(CadSubpixelProxyContracts, OrdinaryExecutorHonorsAbortSafePoints)
{
    EXPECT_TRUE(ordinaryExecutorHonorsAbortSafePoints());
}

TEST_F(CadSubpixelProxyContracts, IndirectValidationResumesAcrossAborts)
{
    EXPECT_TRUE(indirectAtlasValidationResumesAcrossAborts());
}

TEST_F(CadSubpixelProxyContracts, IndirectAtlasPreservesCoverageUnderPressure)
{
    EXPECT_TRUE(indirectProgressiveAtlasPreservesCoverageUnderPressure());
}

TEST_F(CadSubpixelProxyContracts, IndirectPressureProxyPreservesProjectedExtent)
{
    EXPECT_TRUE(indirectPressureProxyPreservesProjectedExtent());
}

TEST_F(CadSubpixelProxyContracts, PreparationReservationCoversBoundedScratch)
{
    EXPECT_TRUE(subpixelPreparationReservationCoversBoundedScratch());
}

TEST_F(CadSubpixelProxyContracts, FlatShadedPlanningResumesAcrossAborts)
{
    EXPECT_TRUE(flatShadedPlanningResumesAcrossAborts());
}

TEST_F(CadSubpixelProxyContracts, FlatShadedAtlasMakesProgressInsideLargeSourceRange)
{
    EXPECT_TRUE(flatShadedAtlasMakesProgressInsideLargeSourceRange());
}

TEST_F(CadSubpixelProxyContracts, ProgressiveReplacementTombstoneKeepsActiveIndex)
{
    EXPECT_TRUE(progressiveReplacementTombstoneKeepsActiveIndex());
}

TEST_F(CadSubpixelProxyContracts, ClearReleasesCompiledPlanGeometry)
{
    EXPECT_TRUE(clearReleasesCompiledPlanGeometry());
}
