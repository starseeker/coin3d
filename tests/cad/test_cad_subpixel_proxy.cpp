/* View-local subpixel proxy rendering and hysteresis regression test. */

#include "headless_utils.h"

#include <Obol/cad/CadProjectedProxy.h>
#include <Obol/cad/SoCADAssembly.h>
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
#include <stdexcept>
#include <string>
#include <vector>

namespace {

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
    Obol::PartGeometry geometry;
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
    assembly->drawMode.setValue(SoCADAssembly::WIREFRAME);
    root->addChild(assembly);

    Obol::PartGeometry geometry;
    geometry.wire = unitBox();
    geometry.subpixelProxyEligible = true;
    geometry.structuralProxy = true;
    const Obol::PartId part =
        Obol::CadIdBuilder::hash128("software-subpixel-proxy-part");
    assembly->upsertPart(part, geometry);

    std::vector<Obol::InstanceUpdate> updates;
    updates.reserve(occurrenceCount);
    for (uint32_t index = 0; index < occurrenceCount; ++index) {
        Obol::InstanceRecord instance;
        instance.part = part;
        instance.parent = Obol::CadIdBuilder::Root();
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
        update.instance = Obol::CadIdBuilder::extendNameOccBool(
            instance.parent, instance.childName, instance.occurrenceIndex,
            instance.boolOp);
        update.record = instance;
        updates.push_back(std::move(update));
    }
    assembly->upsertInstances(updates);

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
        assembly->drawMode.setValue(SoCADAssembly::SHADED);
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

        Obol::PartGeometry geometry;
        geometry.shaded = std::move(mesh);
        geometry.shadedCullBackfaces = false;
        const Obol::PartId part =
            Obol::CadIdBuilder::hash128("normal-free-two-sided");
        assembly->upsertPart(part, geometry);

        Obol::InstanceRecord instance;
        instance.part = part;
        instance.parent = Obol::CadIdBuilder::Root();
        instance.childName = "normal-free-two-sided";
        instance.localToRoot.makeIdentity();
        instance.lodCut = 15;
        instance.style.hasColorOverride = true;
        instance.style.color = SbColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        assembly->upsertInstanceAuto(instance);

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
    assembly->drawMode.setValue(SoCADAssembly::SHADED);
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
        Obol::PartGeometry geometry;
        geometry.shaded = mesh;
        const Obol::PartId part = Obol::CadIdBuilder::hash128(name);
        assembly->upsertPart(part, geometry);

        Obol::InstanceRecord instance;
        instance.part = part;
        instance.parent = Obol::CadIdBuilder::Root();
        instance.childName = name;
        instance.occurrenceIndex = static_cast<uint32_t>(i);
        instance.localToRoot.setTranslate(SbVec3f(
            -44.0f + 8.0f * static_cast<float>(i % 12),
            -44.0f + 8.0f * static_cast<float>(i / 12),
            0.0f));
        instance.lodCut = 0;
        const Obol::InstanceId id =
            assembly->upsertInstanceAuto(instance);
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
        assembly->updateInstanceCuts(richCuts);
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
    assembly->drawMode.setValue(SoCADAssembly::SHADED);
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
        Obol::CadIdBuilder::hash128("progressive-generation-suffix");
    std::shared_ptr<Obol::PartGeometry> coarseGeometry(
        new Obol::PartGeometry);
    coarseGeometry->shaded = std::move(coarse);
    coarseGeometry->conservativeBounds = rich.bounds;
    assembly->upsertSharedParts({{part, coarseGeometry, false}});

    Obol::InstanceRecord instance;
    instance.part = part;
    instance.parent = Obol::CadIdBuilder::Root();
    instance.childName = "progressive-generation-suffix";
    instance.localToRoot.makeIdentity();
    instance.lodCut = 15;
    assembly->upsertInstanceAuto(instance);

    const SbViewportRegion viewport(192, 192);
    SoOffscreenRenderer renderer(viewport);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
    bool passed = render(renderer, root);
    const int initialTier = assembly->lastRenderTier();
    const Obol::CadGpuResourceSnapshot coarseResources =
        assembly->gpuResourceSnapshot();

    if (passed && initialTier == 6) {
        std::shared_ptr<Obol::PartGeometry> richGeometry(
            new Obol::PartGeometry);
        richGeometry->shaded = std::move(rich);
        richGeometry->conservativeBounds =
            coarseGeometry->conservativeBounds;
        assembly->upsertSharedParts({{part, richGeometry, true}});
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
    assembly->drawMode.setValue(SoCADAssembly::SHADED);
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
        Obol::CadIdBuilder::hash128("ordinary-progressive-suffix");
    std::shared_ptr<Obol::PartGeometry> coarseGeometry(
        new Obol::PartGeometry);
    coarseGeometry->shaded = std::move(coarse);
    coarseGeometry->conservativeBounds = rich.bounds;
    assembly->upsertSharedParts({{part, coarseGeometry, false}});

    Obol::InstanceRecord instance;
    instance.part = part;
    instance.parent = Obol::CadIdBuilder::Root();
    instance.childName = "ordinary-progressive-suffix";
    instance.localToRoot.makeIdentity();
    instance.lodCut = 15;
    assembly->upsertInstanceAuto(instance);

    const SbViewportRegion viewport(192, 192);
    SoOffscreenRenderer renderer(viewport);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
    bool passed = render(renderer, root);
    const Obol::CadGpuResourceSnapshot coarseResources =
        assembly->gpuResourceSnapshot();

    if (passed) {
        std::shared_ptr<Obol::PartGeometry> richGeometry(
            new Obol::PartGeometry);
        richGeometry->shaded = std::move(rich);
        richGeometry->conservativeBounds =
            coarseGeometry->conservativeBounds;
        assembly->upsertSharedParts({{part, richGeometry, true}});
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
            std::shared_ptr<Obol::PartGeometry> contractedGeometry(
                new Obol::PartGeometry);
            contractedGeometry->shaded = std::move(contracted);
            contractedGeometry->conservativeBounds =
                coarseGeometry->conservativeBounds;
            assembly->upsertSharedParts(
                {{part, contractedGeometry, true}});
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
                std::shared_ptr<Obol::PartGeometry> replacementGeometry(
                    new Obol::PartGeometry);
                replacementGeometry->shaded = std::move(replacement);
                replacementGeometry->conservativeBounds =
                    contractedGeometry->conservativeBounds;
                assembly->upsertSharedParts(
                    {{part, replacementGeometry, true}});
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
    assembly->drawMode.setValue(SoCADAssembly::SHADED);
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
        Obol::CadIdBuilder::hash128("ordinary-zero-lineage-replacement");
    std::shared_ptr<Obol::PartGeometry> richGeometry(
        new Obol::PartGeometry);
    richGeometry->shaded = std::move(rich);
    richGeometry->conservativeBounds = richGeometry->shaded->bounds;
    assembly->upsertSharedParts({{part, richGeometry, false}});

    Obol::InstanceRecord instance;
    instance.part = part;
    instance.parent = Obol::CadIdBuilder::Root();
    instance.childName = "ordinary-zero-lineage-replacement";
    instance.localToRoot.makeIdentity();
    instance.lodCut = 15;
    assembly->upsertInstanceAuto(instance);

    const SbViewportRegion viewport(192, 192);
    SoOffscreenRenderer renderer(viewport);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
    bool passed = render(renderer, root) &&
        assembly->lastRenderedTriangleCount() == 64u;
    const Obol::CadGpuResourceSnapshot richResources =
        assembly->gpuResourceSnapshot();

    if (passed) {
        std::shared_ptr<Obol::PartGeometry> coarseGeometry(
            new Obol::PartGeometry);
        coarseGeometry->shaded = std::move(coarse);
        coarseGeometry->conservativeBounds =
            richGeometry->conservativeBounds;
        assembly->upsertSharedParts({{part, coarseGeometry, true}});
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
    assembly->drawMode.setValue(SoCADAssembly::WIREFRAME);
    root->addChild(assembly);

    Obol::PartGeometry geometry;
    geometry.wire = unitBox();
    geometry.subpixelProxyEligible = true;
    geometry.structuralProxy = true;
    const Obol::PartId part =
        Obol::CadIdBuilder::hash128("bounded-subpixel-preparation");
    assembly->upsertPart(part, geometry);

    std::vector<Obol::InstanceUpdate> updates;
    updates.reserve(occurrenceCount);
    for (uint32_t index = 0; index < occurrenceCount; ++index) {
        Obol::InstanceRecord instance;
        instance.part = part;
        instance.parent = Obol::CadIdBuilder::Root();
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
        update.instance = Obol::CadIdBuilder::extendNameOccBool(
            instance.parent, instance.childName,
            instance.occurrenceIndex, instance.boolOp);
        update.record = instance;
        updates.push_back(std::move(update));
    }
    assembly->upsertInstances(updates);

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
    assembly->drawMode.setValue(SoCADAssembly::WIREFRAME);
    root->addChild(assembly);

    Obol::PartGeometry geometry;
    geometry.wire = unitBox();
    geometry.subpixelProxyEligible = false;
    geometry.structuralProxy = false;
    const Obol::PartId part =
        Obol::CadIdBuilder::hash128("deadline-shared-wire-part");
    assembly->upsertPart(part, geometry);
    constexpr uint32_t instanceCount = 4096u;
    for (uint32_t index = 0; index < instanceCount; ++index) {
        Obol::InstanceRecord instance;
        instance.part = part;
        instance.parent = Obol::CadIdBuilder::Root();
        instance.childName = "deadline-shared-wire-instance";
        instance.occurrenceIndex = index;
        instance.localToRoot.setTranslate(SbVec3f(
            -31.5f + static_cast<float>(index % 64u),
            -31.5f + static_cast<float>(index / 64u), 0.0f));
        assembly->upsertInstanceAuto(instance);
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
    assembly->drawMode.setValue(SoCADAssembly::SHADED);
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
        Obol::PartGeometry geometry;
        geometry.shaded = triangle;
        geometry.subpixelProxyEligible = false;
        const Obol::PartId part = Obol::CadIdBuilder::hash128(name);
        assembly->upsertPart(part, geometry);

        Obol::InstanceRecord instance;
        instance.part = part;
        instance.parent = Obol::CadIdBuilder::Root();
        instance.childName = name;
        instance.occurrenceIndex = index;
        instance.localToRoot.setTranslate(SbVec3f(
            -15.5f + static_cast<float>(index % 32u),
            -15.5f + static_cast<float>(index / 32u), 0.0f));
        assembly->upsertInstanceAuto(instance);
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
    assembly->drawMode.setValue(SoCADAssembly::SHADED);
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
        Obol::PartGeometry geometry;
        geometry.shaded = mesh;
        geometry.subpixelProxyEligible = true;
        const Obol::PartId part = Obol::CadIdBuilder::hash128(name);
        assembly->upsertPart(part, geometry);
        Obol::InstanceRecord instance;
        instance.part = part;
        instance.parent = Obol::CadIdBuilder::Root();
        instance.childName = name;
        instance.occurrenceIndex = static_cast<uint32_t>(index);
        instance.localToRoot.setTranslate(SbVec3f(
            -60.0f + 7.0f * static_cast<float>(index % 18),
            -35.0f + 7.0f * static_cast<float>(index / 18), 0.0f));
        instance.lodCut = 0;
        const Obol::InstanceId id =
            assembly->upsertInstanceAuto(instance);
        richCuts.push_back({id, 15});
    }

    const SbViewportRegion viewport(256, 256);
    SoOffscreenRenderer renderer(viewport);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
    bool passed = render(renderer, root);
    const int tier = assembly->lastRenderTier();
    if (passed && tier == 6) {
        assembly->updateInstanceCuts(richCuts);
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
    assembly->drawMode.setValue(SoCADAssembly::WIREFRAME);
    root->addChild(assembly);

    Obol::PartGeometry geometry;
    geometry.wire = unitBox();
    geometry.subpixelProxyEligible = true;
    geometry.structuralProxy = true;
    const Obol::PartId part = Obol::CadIdBuilder::hash128("subpixel-proxy");
    assembly->upsertPart(part, geometry);

    Obol::InstanceRecord instance;
    instance.part = part;
    instance.parent = Obol::CadIdBuilder::Root();
    instance.childName = "proxy";
    instance.localToRoot.makeIdentity();
    instance.lodStructuralProxy = true;
    instance.style.hasColorOverride = true;
    instance.style.color = SbColor4f(1.0f, 0.0f, 0.0f, 1.0f);
    const Obol::InstanceId proxyInstance =
        assembly->upsertInstanceAuto(instance);

    // An incorrectly tagged payload must not collapse.  It still has twelve
    // segments, but one endpoint introduces a ninth unique corner so it is
    // not the canonical conservative proxy representation.
    Obol::PartGeometry malformed = geometry;
    malformed.wire->segmentPoints[0].setValue(-0.75f, -0.5f, -0.5f);
    const Obol::PartId malformedPart =
        Obol::CadIdBuilder::hash128("malformed-subpixel-proxy");
    assembly->upsertPart(malformedPart, malformed);
    instance.part = malformedPart;
    instance.childName = "malformed-proxy";
    instance.occurrenceIndex = 1;
    assembly->upsertInstanceAuto(instance);

    const SbViewportRegion viewport(256, 256);
    SoOffscreenRenderer renderer(viewport);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

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
    /* Structural fallbacks are wire boxes regardless of the requested final
     * mesh mode.  A shaded cold view must aggregate a subpixel box directly,
     * rather than loading a shaded mesh merely to reach the same point. */
    assembly->drawMode.setValue(SoCADAssembly::SHADED);
    if (!render(renderer, root) || assembly->lastSubpixelProxyCount() != 1u ||
            nonBlackPixels(renderer) == 0u) {
        std::fprintf(stderr,
            "shaded structural fallback did not collapse directly\n");
        root->unref();
        return 1;
    }
    assembly->drawMode.setValue(SoCADAssembly::WIREFRAME);
    if (!render(renderer, root) || assembly->lastSubpixelProxyCount() != 1u) {
        std::fprintf(stderr,
            "wire structural fallback did not survive draw-mode restore\n");
        root->unref();
        return 1;
    }
    camera->height.setValue(320.0f);
    assembly->drawMode.setValue(SoCADAssembly::SHADED);
    if (!render(renderer, root) || assembly->lastSubpixelProxyCount() != 1u) {
        std::fprintf(stderr,
            "pixel-sized shaded structural fallback used mesh hysteresis\n");
        root->unref();
        return 1;
    }
    camera->height.setValue(1000.0f);
    assembly->drawMode.setValue(SoCADAssembly::WIREFRAME);
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
    const std::vector<Obol::InstanceId> protectedSnapshot =
        assembly->pointProxyProtectedInstances();
    if (protectedSnapshot.size() != 1u ||
            protectedSnapshot[0] != proxyInstance ||
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
    if (!assembly->pointProxyProtectedInstances().empty() ||
            !render(renderer, root) ||
            assembly->lastSubpixelProxyCount() != 1u ||
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
    assembly->pointProxyPixelThreshold.setValue(2.0f);
    if (!render(renderer, root) || assembly->lastSubpixelProxyCount() != 1u) {
        std::fprintf(stderr,
            "interactive point threshold did not aggregate retained proxy\n");
        root->unref();
        return 1;
    }
    assembly->pointProxyPixelThreshold.setValue(1.0f);
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
    Obol::PartGeometry shadedSubpixel;
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
        Obol::CadIdBuilder::hash128("shaded-subpixel");
    assembly->upsertPart(shadedSubpixelPart, shadedSubpixel);
    Obol::InstanceRecord shadedInstance;
    shadedInstance.part = shadedSubpixelPart;
    shadedInstance.parent = Obol::CadIdBuilder::Root();
    shadedInstance.childName = "shaded-subpixel";
    shadedInstance.localToRoot.makeIdentity();
    const Obol::InstanceId shadedSubpixelInstance =
        assembly->upsertInstanceAuto(shadedInstance);
    assembly->drawMode.setValue(SoCADAssembly::SHADED);
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
    assembly->drawMode.setValue(SoCADAssembly::SHADED_WITH_EDGES);
    for (int i = 0; i < 144; ++i) {
        Obol::PartGeometry box;
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
        const Obol::PartId boxPart = Obol::CadIdBuilder::hash128(partName);
        assembly->upsertPart(boxPart, box);

        Obol::InstanceRecord boxInstance;
        boxInstance.part = boxPart;
        boxInstance.parent = Obol::CadIdBuilder::Root();
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
        assembly->upsertInstanceAuto(boxInstance);
    }

    Obol::PartGeometry progressiveGeometry;
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
        Obol::CadIdBuilder::hash128("mixed-progressive-triangle");
    assembly->upsertPart(progressivePart, progressiveGeometry);
    Obol::InstanceRecord progressiveInstance;
    progressiveInstance.part = progressivePart;
    progressiveInstance.parent = Obol::CadIdBuilder::Root();
    progressiveInstance.childName = "mixed-progressive-triangle";
    progressiveInstance.localToRoot.makeIdentity();
    assembly->upsertInstanceAuto(progressiveInstance);

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
    assembly->updateInstanceStyles({selectedStyleUpdate});
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
    Obol::PartGeometry rebindBox;
    rebindBox.wire = unitBox();
    rebindBox.subpixelProxyEligible = true;
    rebindBox.structuralProxy = true;
    const Obol::PartId rebindBoxPart =
        Obol::CadIdBuilder::hash128("stream-rebind-box");
    assembly->upsertPart(rebindBoxPart, rebindBox);
    Obol::InstanceRecord rebindRecord;
    rebindRecord.part = rebindBoxPart;
    rebindRecord.parent = Obol::CadIdBuilder::Root();
    rebindRecord.childName = "stream-rebind";
    rebindRecord.occurrenceIndex = 9001;
    rebindRecord.lodStructuralProxy = true;
    rebindRecord.localToRoot.setTranslate(SbVec3f(0.0f, 20.0f, 0.0f));
    const Obol::InstanceId rebindInstance =
        assembly->upsertInstanceAuto(rebindRecord);
    if (!render(renderer, root)) {
        std::fprintf(stderr, "stream rebind setup did not render\n");
        root->unref();
        return 1;
    }
    const uint64_t rebindPlanBuilds = assembly->framePlanBuildCount();
    const size_t rebindPlanInstances =
        assembly->framePlanInstanceRecordCount();
    const Obol::PartId rebindMeshPart =
        Obol::CadIdBuilder::hash128("stream-rebind-mesh");
    assembly->upsertPart(rebindMeshPart, progressiveGeometry);
    rebindRecord.part = rebindMeshPart;
    rebindRecord.lodStructuralProxy = false;
    Obol::InstanceUpdate rebindUpdate;
    rebindUpdate.instance = rebindInstance;
    rebindUpdate.record = rebindRecord;
    assembly->upsertInstances({rebindUpdate});
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
        Obol::CadIdBuilder::hash128("mixed-wave-rebind-box");
    assembly->upsertPart(mixedRebindBoxPart, rebindBox);
    Obol::InstanceRecord mixedRebindRecord = rebindRecord;
    mixedRebindRecord.part = mixedRebindBoxPart;
    mixedRebindRecord.childName = "mixed-wave-rebind";
    mixedRebindRecord.occurrenceIndex = 9006;
    mixedRebindRecord.lodStructuralProxy = true;
    mixedRebindRecord.localToRoot.setTranslate(
        SbVec3f(-8.0f, 20.0f, 0.0f));
    const Obol::InstanceId mixedRebindInstance =
        assembly->upsertInstanceAuto(mixedRebindRecord);

    Obol::InstanceRecord mixedCutRecord = progressiveInstance;
    mixedCutRecord.childName = "mixed-wave-cut";
    mixedCutRecord.occurrenceIndex = 9007;
    mixedCutRecord.lodCut = 15;
    mixedCutRecord.localToRoot.setTranslate(
        SbVec3f(8.0f, 20.0f, 0.0f));
    const Obol::InstanceId mixedCutInstance =
        assembly->upsertInstanceAuto(mixedCutRecord);
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
        Obol::CadIdBuilder::hash128("mixed-wave-rebind-mesh");
    assembly->upsertPart(mixedRebindMeshPart, progressiveGeometry);
    mixedRebindRecord.part = mixedRebindMeshPart;
    mixedRebindRecord.lodStructuralProxy = false;
    Obol::InstanceUpdate mixedRebindUpdate;
    mixedRebindUpdate.instance = mixedRebindInstance;
    mixedRebindUpdate.record = mixedRebindRecord;
    mixedCutRecord.lodCut = 14;
    Obol::InstanceUpdate mixedCutUpdate;
    mixedCutUpdate.instance = mixedCutInstance;
    mixedCutUpdate.record = mixedCutRecord;
    assembly->upsertInstances({mixedRebindUpdate, mixedCutUpdate});
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
        Obol::CadIdBuilder::hash128("stream-shared-box");
    assembly->upsertPart(sharedBoxPart, rebindBox);
    Obol::InstanceRecord sharedBoxA = rebindRecord;
    sharedBoxA.part = sharedBoxPart;
    sharedBoxA.lodStructuralProxy = true;
    sharedBoxA.childName = "stream-shared-box-a";
    sharedBoxA.occurrenceIndex = 9002;
    sharedBoxA.localToRoot.setTranslate(SbVec3f(-4.0f, 24.0f, 0.0f));
    const Obol::InstanceId sharedBoxAId =
        assembly->upsertInstanceAuto(sharedBoxA);
    Obol::InstanceRecord sharedBoxB = sharedBoxA;
    sharedBoxB.childName = "stream-shared-box-b";
    sharedBoxB.occurrenceIndex = 9003;
    sharedBoxB.localToRoot.setTranslate(SbVec3f(4.0f, 24.0f, 0.0f));
    const Obol::InstanceId sharedBoxBId =
        assembly->upsertInstanceAuto(sharedBoxB);
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
        assembly->upsertInstanceAuto(offscreenBox);
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
        Obol::CadIdBuilder::extendNameOccBool(
            newStreamed.parent, newStreamed.childName,
            newStreamed.occurrenceIndex, newStreamed.boolOp);
    Obol::InstanceUpdate newStreamedUpdate;
    newStreamedUpdate.instance = newStreamedId;
    newStreamedUpdate.record = newStreamed;
    assembly->upsertInstances({promotedUpdate, newStreamedUpdate});
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
            Obol::CadIdBuilder::extendNameOccBool(
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
            Obol::CadIdBuilder::extendNameOccBool(
                lateMesh.parent, lateMesh.childName,
                lateMesh.occurrenceIndex, lateMesh.boolOp);
        lateMeshUpdate.record = lateMesh;

        assembly->upsertInstances(
            {lateMeshUpdate, lateBoxUpdate});
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
        const Obol::PartId id = Obol::CadIdBuilder::hash128(name);
        assembly->upsertPart(id, geometry);
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
    const float threshold = assembly->pointProxyPixelThreshold.getValue();
    assembly->pointProxyPixelThreshold.setValue(
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

TEST_F(CadSubpixelProxyContracts, PreparationReservationCoversBoundedScratch)
{
    EXPECT_TRUE(subpixelPreparationReservationCoversBoundedScratch());
}
