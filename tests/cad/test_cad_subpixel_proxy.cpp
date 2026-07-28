/* View-local subpixel proxy rendering and hysteresis regression test. */

#include "headless_utils.h"

#include <Obol/cad/SoCADAssembly.h>
#include <Obol/cad/CadIds.h>

#include <Inventor/SbViewportRegion.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoSeparator.h>

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>

namespace {

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
     * Two disjoint, coplanar triangles deliberately use opposite winding and
     * have no authored normal stream.  The retained progressive declaration
     * makes the fixed VBO reference compute flat normals from the geometry,
     * matching the normal-free PoP payloads used by BRL-CAD meshes such as
     * Lucy.  With two-sided lighting, both visible triangles must receive the
     * same headlight contribution.
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
    mesh.progressiveMinimumLevel = 15;
    mesh.progressiveResidentLevel = 15;
    mesh.progressiveIndexCount[15] =
        static_cast<uint32_t>(mesh.indices.size());
    mesh.progressivePositionCount[15] =
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
    instance.lodLevel = 15;
    instance.style.hasColorOverride = true;
    instance.style.color = SbColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    assembly->upsertInstanceAuto(instance);

    const SbViewportRegion viewport(256, 192);
    SoOffscreenRenderer renderer(viewport);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    const char *previousGlsl = std::getenv("OBOL_CAD_SOFTWARE_GLSL");
    const bool hadPreviousGlsl = previousGlsl != nullptr;
    const std::string previousGlslValue =
        previousGlsl ? std::string(previousGlsl) : std::string();
    const auto restoreGlslEnvironment = [&]() {
        if (hadPreviousGlsl)
            setenv("OBOL_CAD_SOFTWARE_GLSL",
                   previousGlslValue.c_str(), 1);
        else
            unsetenv("OBOL_CAD_SOFTWARE_GLSL");
    };

    unsetenv("OBOL_CAD_SOFTWARE_GLSL");
    if (!render(renderer, root)) {
        restoreGlslEnvironment();
        root->unref();
        return false;
    }
    const HalfImageStats fixed = foregroundHalfStats(renderer);

    setenv("OBOL_CAD_SOFTWARE_GLSL", "1", 1);
    const bool rendered = render(renderer, root);
    const HalfImageStats glsl = foregroundHalfStats(renderer);
    restoreGlslEnvironment();
    root->unref();

    if (!rendered || fixed.leftPixels < 100 || fixed.rightPixels < 100 ||
            glsl.leftPixels < 100 || glsl.rightPixels < 100)
        return false;
    const double minimumLit = 0.75 *
        (std::min)(fixed.leftMean, fixed.rightMean);
    const double glslLow = (std::min)(glsl.leftMean, glsl.rightMean);
    const double glslHigh = (std::max)(glsl.leftMean, glsl.rightMean);
    const bool matched = glslLow >= minimumLit && glslHigh > 0.0 &&
        glslLow / glslHigh >= 0.9;
    if (!matched) {
        std::fprintf(stderr,
            "two-sided stats fixed={left=%.3f/%zu right=%.3f/%zu} "
            "glsl={left=%.3f/%zu right=%.3f/%zu}\n",
            fixed.leftMean, fixed.leftPixels,
            fixed.rightMean, fixed.rightPixels,
            glsl.leftMean, glsl.leftPixels,
            glsl.rightMean, glsl.rightPixels);
    }
    return matched;
}

} // namespace

int
main()
{
    initCoinHeadless();
    SoCADAssembly::initClass();

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
    const Obol::PartId part = Obol::CadIdBuilder::hash128("subpixel-proxy");
    assembly->upsertPart(part, geometry);

    Obol::InstanceRecord instance;
    instance.part = part;
    instance.parent = Obol::CadIdBuilder::Root();
    instance.childName = "proxy";
    instance.localToRoot.makeIdentity();
    instance.style.hasColorOverride = true;
    instance.style.color = SbColor4f(1.0f, 0.0f, 0.0f, 1.0f);
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
    const uint64_t initialPresentation =
        assembly->lastSubpixelProxyRevision();
    if (!initialPresentation || !render(renderer, root) ||
        assembly->lastSubpixelProxyRevision() != initialPresentation) {
        std::fprintf(stderr,
            "unchanged subpixel proxy view rebuilt presentation state\n");
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

    // Stay collapsed inside the hysteresis band.  The proxy now projects to
    // about one pixel, above the entry threshold but below the leave limit.
    camera->height.setValue(250.0f);
    if (!render(renderer, root) || assembly->lastSubpixelProxyCount() != 1u) {
        std::fprintf(stderr, "subpixel proxy did not retain hysteresis state\n");
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
    triangle.progressiveMinimumLevel = 15;
    triangle.progressiveResidentLevel = 15;
    triangle.progressiveIndexCount[15] = 3;
    triangle.progressivePositionCount[15] = 3;
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
    setenv("OBOL_CAD_FLAT_WIRE", "0", 1);
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

    setenv("OBOL_CAD_FLAT_WIRE", "1", 1);
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

    root->unref();
    if (!normalFreeTwoSidedGlslMatchesFixed()) {
        std::fprintf(stderr,
            "normal-free two-sided GLSL shading diverged from fixed VBO\n");
        return 1;
    }
    return 0;
}
