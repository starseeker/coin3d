/* View-local subpixel proxy rendering and hysteresis regression test. */

#include "headless_utils.h"

#include <obol/cad/SoCADAssembly.h>
#include <obol/cad/CadIds.h>

#include <Inventor/SbViewportRegion.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoSeparator.h>

#include <cstdio>

namespace {

obol::WireRep
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
    obol::WireRep wire;
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

    obol::PartGeometry geometry;
    geometry.wire = unitBox();
    geometry.subpixelProxyEligible = true;
    const obol::PartId part = obol::CadIdBuilder::hash128("subpixel-proxy");
    assembly->upsertPart(part, geometry);

    obol::InstanceRecord instance;
    instance.part = part;
    instance.parent = obol::CadIdBuilder::Root();
    instance.childName = "proxy";
    instance.localToRoot.makeIdentity();
    instance.style.hasColorOverride = true;
    instance.style.color = SbColor4f(1.0f, 0.0f, 0.0f, 1.0f);
    assembly->upsertInstanceAuto(instance);

    // An incorrectly tagged payload must not collapse.  It still has twelve
    // segments, but one endpoint introduces a ninth unique corner so it is
    // not the canonical conservative proxy representation.
    obol::PartGeometry malformed = geometry;
    malformed.wire->segmentPoints[0].setValue(-0.75f, -0.5f, -0.5f);
    const obol::PartId malformedPart =
        obol::CadIdBuilder::hash128("malformed-subpixel-proxy");
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

    root->unref();
    return 0;
}
