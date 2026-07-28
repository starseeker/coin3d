/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
\**************************************************************************/

/**
 * @file SoCADAssembly.cpp
 * @brief SoCADAssembly Inventor node – compiled CAD assembly renderer.
 *
 * Implementation notes:
 *  - GLRender: iterates visible instances, draws wire segments/polylines and
 *    optionally triangle meshes.
 *  - rayPick: delegates to CadPickQuery using the current pickMode.
 *  - getBoundingBox: returns the union of all instance world bounds.
 *  - The Pimpl (SoCADAssemblyImpl) holds the mutable instance/part databases
 *    and lazily-built acceleration structures.
 */

#include <Obol/cad/SoCADAssembly.h>
#include <Obol/cad/SoCADDetail.h>
#include <Obol/cad/SoCADViewState.h>
#include "CadFramePlan.h"
#include "CadRendererGL.h"
#include "picking/CadPicking.h"

#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/actions/SoRayPickAction.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/actions/SoGetPrimitiveCountAction.h>
#include <Inventor/SoPickedPoint.h>
#include <Inventor/SoFullPath.h>
#include <Inventor/SoDB.h>
#include <Inventor/SbLine.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbBox3f.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbViewVolume.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/elements/SoViewVolumeElement.h>
#include <Inventor/elements/SoViewportRegionElement.h>
#include <Inventor/elements/SoViewingMatrixElement.h>
#include <Inventor/elements/SoProjectionMatrixElement.h>
#include <Inventor/elements/SoContextManagerElement.h>
#include <Inventor/elements/SoGLLazyElement.h>
#include <Inventor/elements/SoLightElement.h>
#include <Inventor/elements/SoShapeHintsElement.h>
#include <Inventor/nodes/SoLight.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoPointLight.h>
#include <Inventor/nodes/SoSpotLight.h>
#include <Inventor/SbColor.h>

#include <Inventor/system/gl.h>
#include "rendering/SoGL.h"

#include <unordered_map>
#include <unordered_set>
#include <array>
#include <vector>
#include <memory>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cmath>

// ---------------------------------------------------------------------------
// SoCADAssemblyImpl – private implementation (Pimpl pattern)
// ---------------------------------------------------------------------------

static bool
cadDebugEnabled()
{
    const char *env = std::getenv("OBOL_CAD_DEBUG");
    return env && env[0] && env[0] != '0';
}

static bool
cadLightDebugEnabled()
{
    const char *env = std::getenv("OBOL_CAD_LIGHT_DEBUG");
    return env && env[0] && env[0] != '0';
}

static bool
cadTransformPreservesOrientation(const std::array<float, 16>& transform)
{
    const double determinant =
        static_cast<double>(transform[0]) *
            (static_cast<double>(transform[5]) * transform[10] -
             static_cast<double>(transform[6]) * transform[9]) -
        static_cast<double>(transform[1]) *
            (static_cast<double>(transform[4]) * transform[10] -
             static_cast<double>(transform[6]) * transform[8]) +
        static_cast<double>(transform[2]) *
            (static_cast<double>(transform[4]) * transform[9] -
             static_cast<double>(transform[5]) * transform[8]);
    return std::isfinite(determinant) && determinant > 1.0e-12;
}

namespace {

struct CadSoftwareClipPoint {
    double v[4];
};

static double cadSoftwarePlaneValue(const CadSoftwareClipPoint& point,
                                    int plane);

static CadSoftwareClipPoint
cadSoftwareTransform(const SbMatrix& matrix, const SbVec3f& point)
{
    const float *m = matrix[0];
    CadSoftwareClipPoint result;
    for (int col = 0; col < 4; ++col) {
        result.v[col] = static_cast<double>(point[0]) * m[col] +
            static_cast<double>(point[1]) * m[4 + col] +
            static_cast<double>(point[2]) * m[8 + col] + m[12 + col];
    }
    return result;
}

static bool
cadSubpixelProxyCorners(const Obol::WireRep& wire,
                        std::array<SbVec3f, 8>& corners)
{
    // Only the canonical 12-segment proxy representation is eligible.  Do
    // this validation once when a shared part is published rather than once
    // per occurrence on every camera update.
    if (wire.segmentPoints.size() != 24u || !wire.polylines.empty())
        return false;

    size_t cornerCount = 0;
    const auto addCorner = [&](const SbVec3f& candidate) -> bool {
        for (size_t i = 0; i < cornerCount; ++i) {
            const SbVec3f& existing = corners[i];
            if (candidate[0] == existing[0] &&
                    candidate[1] == existing[1] &&
                    candidate[2] == existing[2])
                return true;
        }
        if (cornerCount == corners.size())
            return false;
        corners[cornerCount++] = candidate;
        return true;
    };

    for (const SbVec3f& point : wire.segmentPoints)
        if (!addCorner(point))
            return false;

    return cornerCount == corners.size();
}

static bool
cadSubpixelProxyPoint(const std::array<SbVec3f, 8>& corners,
                      const Obol::internal::CadVisibleInstance& instance,
                      const SbMatrix& viewProj, const SbVec2s& viewportSize,
                      float pixelLimit, SbVec3f& point)
{
    if (viewportSize[0] <= 1 || viewportSize[1] <= 1 ||
            pixelLimit <= 0.0f)
        return false;

    SbMatrix model;
    model.setValue(instance.transform.data());
    SbMatrix modelViewProj = model;
    modelViewProj.multRight(viewProj);

    double minX = 0.0, minY = 0.0, maxX = 0.0, maxY = 0.0;
    double nearestDepth = 0.0;
    bool havePoint = false;
    const auto project = [&](const SbVec3f& localPoint) -> bool {
        const CadSoftwareClipPoint clip =
            cadSoftwareTransform(modelViewProj, localPoint);
        if (std::abs(clip.v[3]) < 1.0e-20)
            return false;
        for (int plane = 0; plane < 6; ++plane) {
            if (cadSoftwarePlaneValue(clip, plane) < 0.0)
                return false;
        }
        const double x = clip.v[0] / clip.v[3];
        const double y = clip.v[1] / clip.v[3];
        const double z = clip.v[2] / clip.v[3];
        if (!havePoint) {
            minX = maxX = x;
            minY = maxY = y;
            nearestDepth = z;
            model.multVecMatrix(localPoint, point);
            havePoint = true;
        } else {
            minX = std::min(minX, x);
            maxX = std::max(maxX, x);
            minY = std::min(minY, y);
            maxY = std::max(maxY, y);
            // The closest actual proxy vertex preserves the front-most AABB
            // or OBB depth when the proxy collapses to one raster point.
            if (z < nearestDepth) {
                nearestDepth = z;
                model.multVecMatrix(localPoint, point);
            }
        }
        return true;
    };

    for (const SbVec3f& corner : corners)
        if (!project(corner))
            return false;

    if (!havePoint)
        return false;
    const double width = (maxX - minX) * 0.5 * (viewportSize[0] - 1);
    const double height = (maxY - minY) * 0.5 * (viewportSize[1] - 1);
    return width <= pixelLimit && height <= pixelLimit;
}

static bool
cadSameSubpixelProxyPoints(
        const std::vector<Obol::internal::CadSubpixelProxyPoint>& left,
        const std::vector<Obol::internal::CadSubpixelProxyPoint>& right)
{
    if (left.size() != right.size())
        return false;
    for (size_t i = 0; i < left.size(); ++i) {
        const Obol::internal::CadSubpixelProxyPoint& a = left[i];
        const Obol::internal::CadSubpixelProxyPoint& b = right[i];
        if (a.instanceId != b.instanceId || a.rgba != b.rgba ||
                a.position[0] != b.position[0] ||
                a.position[1] != b.position[1] ||
                a.position[2] != b.position[2])
            return false;
    }
    return true;
}

static bool
cadSoftwareClipScreen(double& x0, double& y0, double& x1, double& y1,
                      double left, double bottom, double right, double top)
{
    auto code = [left, bottom, right, top](double x, double y) {
        unsigned int result = 0;
        if (x < left) result |= 1u;
        else if (x > right) result |= 2u;
        if (y < bottom) result |= 4u;
        else if (y > top) result |= 8u;
        return result;
    };
    unsigned int c0 = code(x0, y0);
    unsigned int c1 = code(x1, y1);
    for (;;) {
        if (!(c0 | c1)) return true;
        if (c0 & c1) return false;
        const unsigned int c = c0 ? c0 : c1;
        double x = 0.0, y = 0.0;
        if (c & 8u) {
            y = top;
            x = x0 + (x1 - x0) * (top - y0) / (y1 - y0);
        } else if (c & 4u) {
            y = bottom;
            x = x0 + (x1 - x0) * (bottom - y0) / (y1 - y0);
        } else if (c & 2u) {
            x = right;
            y = y0 + (y1 - y0) * (right - x0) / (x1 - x0);
        } else {
            x = left;
            y = y0 + (y1 - y0) * (left - x0) / (x1 - x0);
        }
        if (c == c0) { x0 = x; y0 = y; c0 = code(x0, y0); }
        else { x1 = x; y1 = y; c1 = code(x1, y1); }
    }
}

static double
cadSoftwarePlaneValue(const CadSoftwareClipPoint& point, int plane)
{
    switch (plane) {
        case 0: return point.v[3] + point.v[0];
        case 1: return point.v[3] - point.v[0];
        case 2: return point.v[3] + point.v[1];
        case 3: return point.v[3] - point.v[1];
        case 4: return point.v[3] + point.v[2];
        default: return point.v[3] - point.v[2];
    }
}

static bool
cadSoftwareClip(CadSoftwareClipPoint& a, CadSoftwareClipPoint& b)
{
    for (int plane = 0; plane < 6; ++plane) {
        const double da = cadSoftwarePlaneValue(a, plane);
        const double db = cadSoftwarePlaneValue(b, plane);
        if (da < 0.0 && db < 0.0) return false;
        if (da >= 0.0 && db >= 0.0) continue;
        const double denominator = da - db;
        if (std::abs(denominator) < 1.0e-20) return false;
        const double t = da / denominator;
        CadSoftwareClipPoint clipped;
        for (int i = 0; i < 4; ++i)
            clipped.v[i] = a.v[i] + t * (b.v[i] - a.v[i]);
        if (da < 0.0) a = clipped;
        else b = clipped;
    }
    return std::abs(a.v[3]) > 1.0e-20 && std::abs(b.v[3]) > 1.0e-20;
}

static void
cadSoftwarePutPixel(unsigned char *pixels, unsigned int width,
                    unsigned int height, int x, int y,
                    const std::array<uint8_t, 4>& color)
{
    if (x < 0 || y < 0 || static_cast<unsigned int>(x) >= width ||
            static_cast<unsigned int>(y) >= height)
        return;
    unsigned char *pixel = pixels +
        (static_cast<size_t>(y) * width + static_cast<unsigned int>(x)) * 4;
    if (color[3] == 255) {
        std::memcpy(pixel, color.data(), 4);
        return;
    }
    const unsigned int alpha = color[3];
    const unsigned int inverse = 255 - alpha;
    for (int i = 0; i < 3; ++i)
        pixel[i] = static_cast<unsigned char>(
            (color[i] * alpha + pixel[i] * inverse + 127) / 255);
    pixel[3] = 255;
}

static void
cadSoftwareLine(unsigned char *pixels, unsigned int width,
                unsigned int height, int x0, int y0, int x1, int y1,
                const Obol::internal::CadVisibleInstance& instance)
{
    std::array<uint8_t, 4> color = instance.rgba;
    const int pixelWidth = std::max(1, static_cast<int>(
        std::lround(instance.lineWidth)));
    const int lowOffset = -(pixelWidth - 1) / 2;
    const int highOffset = pixelWidth / 2;
    const unsigned int factor = std::max<unsigned int>(
        1u, instance.linePatternFactor);
    int dx = std::abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    unsigned int step = 0;
    for (;;) {
        const unsigned int patternBit = (step / factor) & 15u;
        if (instance.linePattern & (1u << patternBit)) {
            for (int oy = lowOffset; oy <= highOffset; ++oy)
                for (int ox = lowOffset; ox <= highOffset; ++ox)
                    cadSoftwarePutPixel(pixels, width, height,
                                        x0 + ox, y0 + oy, color);
        }
        if (x0 == x1 && y0 == y1) break;
        const int twiceError = 2 * error;
        if (twiceError >= dy) { error += dy; x0 += sx; }
        if (twiceError <= dx) { error += dx; y0 += sy; }
        ++step;
    }
}

static void
cadSoftwareSegment(unsigned char *pixels, unsigned int width,
                   unsigned int height, const SbVec2s& origin,
                   const SbVec2s& size, const SbMatrix& transform,
                   const SbVec3f& p0, const SbVec3f& p1,
                   const Obol::internal::CadVisibleInstance& instance)
{
    const float *m = transform[0];
    if (m[3] == 0.0f && m[7] == 0.0f && m[11] == 0.0f && m[15] != 0.0f) {
        const double inverseW = 1.0 / m[15];
        double x0 = origin[0] + ((p0[0] * m[0] + p0[1] * m[4] +
            p0[2] * m[8] + m[12]) * inverseW * 0.5 + 0.5) * (size[0] - 1);
        double y0 = origin[1] + ((p0[0] * m[1] + p0[1] * m[5] +
            p0[2] * m[9] + m[13]) * inverseW * 0.5 + 0.5) * (size[1] - 1);
        double x1 = origin[0] + ((p1[0] * m[0] + p1[1] * m[4] +
            p1[2] * m[8] + m[12]) * inverseW * 0.5 + 0.5) * (size[0] - 1);
        double y1 = origin[1] + ((p1[0] * m[1] + p1[1] * m[5] +
            p1[2] * m[9] + m[13]) * inverseW * 0.5 + 0.5) * (size[1] - 1);
        if (!cadSoftwareClipScreen(x0, y0, x1, y1, origin[0], origin[1],
                origin[0] + size[0] - 1, origin[1] + size[1] - 1))
            return;
        cadSoftwareLine(pixels, width, height,
            static_cast<int>(x0 + 0.5), static_cast<int>(y0 + 0.5),
            static_cast<int>(x1 + 0.5), static_cast<int>(y1 + 0.5), instance);
        return;
    }

    CadSoftwareClipPoint a = cadSoftwareTransform(transform, p0);
    CadSoftwareClipPoint b = cadSoftwareTransform(transform, p1);
    if (!cadSoftwareClip(a, b)) return;
    const int x0 = origin[0] + static_cast<int>(std::lround(
        (a.v[0] / a.v[3] * 0.5 + 0.5) * (size[0] - 1)));
    const int y0 = origin[1] + static_cast<int>(std::lround(
        (a.v[1] / a.v[3] * 0.5 + 0.5) * (size[1] - 1)));
    const int x1 = origin[0] + static_cast<int>(std::lround(
        (b.v[0] / b.v[3] * 0.5 + 0.5) * (size[0] - 1)));
    const int y1 = origin[1] + static_cast<int>(std::lround(
        (b.v[1] / b.v[3] * 0.5 + 0.5) * (size[1] - 1)));
    cadSoftwareLine(pixels, width, height, x0, y0, x1, y1, instance);
}

static void
cadSoftwarePoint(unsigned char *pixels, unsigned int width,
                 unsigned int height, const SbVec2s& origin,
                 const SbVec2s& size, const SbMatrix& viewProj,
                 const Obol::internal::CadSubpixelProxyPoint& point)
{
    const CadSoftwareClipPoint clip =
        cadSoftwareTransform(viewProj, point.position);
    if (std::abs(clip.v[3]) < 1.0e-20)
        return;
    for (int plane = 0; plane < 6; ++plane)
        if (cadSoftwarePlaneValue(clip, plane) < 0.0)
            return;
    const int x = origin[0] + static_cast<int>(std::lround(
        (clip.v[0] / clip.v[3] * 0.5 + 0.5) * (size[0] - 1)));
    const int y = origin[1] + static_cast<int>(std::lround(
        (clip.v[1] / clip.v[3] * 0.5 + 0.5) * (size[1] - 1)));
    cadSoftwarePutPixel(pixels, width, height, x, y, point.rgba);
}

static bool
cadRenderSoftwareWire(const Obol::internal::CadFramePlan& plan,
                      const SoCADAssembly& assembly, SoState *state,
                      const SbMatrix& viewProj)
{
    if (plan.wireItems.empty() || !plan.shadedItems.empty() ||
            assembly.wireframeOcclusion.getValue())
        return false;
    SoDB::ContextManager *manager = SoContextManagerElement::get(state);
    unsigned char *pixels = nullptr;
    unsigned int width = 0, height = 0, components = 0;
    if (!manager || !manager->getCurrentSoftwareFramebuffer(
            pixels, width, height, components) || components != 4)
        return false;
    const SbViewportRegion& viewport = SoViewportRegionElement::get(state);
    const SbVec2s origin = viewport.getViewportOriginPixels();
    const SbVec2s size = viewport.getViewportSizePixels();
    if (size[0] <= 0 || size[1] <= 0) return false;

    for (const auto& item : plan.wireItems) {
        const Obol::PartGeometry *geometry = assembly.partGeometry(item.rep.part);
        if (!geometry || !geometry->wire.has_value()) continue;
        const Obol::WireRep& wire = *geometry->wire;
        for (uint32_t i = 0; i < item.instanceCount; ++i) {
            const size_t visibleIndex = item.baseInstance + i;
            if (visibleIndex < plan.subpixelProxyMask.size() &&
                    plan.subpixelProxyMask[visibleIndex])
                continue;
            const auto& instance = plan.visibleInstances[visibleIndex];
            SbMatrix model;
            model.setValue(instance.transform.data());
            SbMatrix transform = model;
            transform.multRight(viewProj);
            for (size_t p = 0; p + 1 < wire.segmentPoints.size(); p += 2)
                cadSoftwareSegment(pixels, width, height, origin, size,
                    transform, wire.segmentPoints[p], wire.segmentPoints[p + 1],
                    instance);
            for (const auto& polyline : wire.polylines)
                for (size_t p = 1; p < polyline.points.size(); ++p)
                    cadSoftwareSegment(pixels, width, height, origin, size,
                    transform, polyline.points[p - 1], polyline.points[p],
                        instance);
        }
    }
    for (const auto& point : plan.subpixelProxyPoints)
        cadSoftwarePoint(pixels, width, height, origin, size, viewProj, point);
    return true;
}

} // namespace

struct InstanceData {
    Obol::PartId          partId;
    SbMatrix              localToRoot;
    Obol::InstanceStyle   style;
    Obol::InstanceId      parent;
    std::string           childName;
    uint32_t              occurrenceIndex = 0;
    uint8_t               boolOp = 0;
    uint8_t               lodLevel = 255;
    SbBox3f               worldBounds;   // cached; recomputed on transform change
};

struct SoCADAssemblyImpl {
    static constexpr size_t ProgressiveLevelBinCount = 17;

    struct ProgressiveShadedPlanGroup {
        Obol::PartId part;
        uint32_t baseInstance = 0;
        uint32_t instanceCount = 0;
        std::array<uint32_t, ProgressiveLevelBinCount> levelCounts = {};
        size_t shadedItemBegin = 0;
        size_t shadedItemCount = 0;
    };

    // Part library
    std::unordered_map<Obol::PartId, std::shared_ptr<const Obol::PartGeometry>,
                       std::hash<Obol::PartId>> parts_;
    // Cached, validated corners for the small subset of shared parts that
    // are conservative AABB/OBB display proxies.  This avoids rediscovering
    // eight corners from 24 wire endpoints for every occurrence per view.
    std::unordered_map<Obol::PartId, std::array<SbVec3f, 8>,
                       std::hash<Obol::PartId>> subpixelProxyCorners_;

    // Instance database
    std::unordered_map<Obol::InstanceId, InstanceData,
                       std::hash<Obol::InstanceId>> instances_;

    // Selection set
    std::unordered_set<Obol::InstanceId,
                       std::hash<Obol::InstanceId>> selected_;

    // Hidden-instance set (excluded from rendering and frame plan)
    std::unordered_set<Obol::InstanceId,
                       std::hash<Obol::InstanceId>> hidden_;

    // Unpickable-instance set (visible, but excluded from pick BVH)
    std::unordered_set<Obol::InstanceId,
                       std::hash<Obol::InstanceId>> unpickable_;

    // Picking acceleration structures
    Obol::picking::CadInstanceBVH instanceBvh_;
    bool bvhDirty_   = true;
    bool inUpdate_   = false;

    // Per-part edge BVH cache (lazily built during picking)
    std::unordered_map<Obol::PartId, Obol::picking::CadPartEdgeBVH,
                       std::hash<Obol::PartId>> partEdgeBvhCache_;

    // Per-part triangle BVH cache (lazily built during picking)
    std::unordered_map<Obol::PartId, Obol::picking::CadPartTriBVH,
                       std::hash<Obol::PartId>> partTriBvhCache_;

    // Parts whose producer supplied retained progressive prefixes.
    std::unordered_set<Obol::PartId,
                       std::hash<Obol::PartId>> progressiveParts_;

    // Per-part generation counter – incremented when geometry changes so the
    // renderer knows to re-upload VBOs.
    std::unordered_map<Obol::PartId, uint64_t,
                       std::hash<Obol::PartId>> partGeneration_;
    uint64_t nextGeneration_ = 1;

    // Frame plan cache.  Rebuilt lazily when planDirty_ is true.
    Obol::internal::CadFramePlan cachedPlan_;
    // A shaded progressive plan is ordered into at most 17 level buckets
    // (PoP levels 0..15 plus the producer-default sentinel).  These indices
    // let a sparse LoD journal move an occurrence between buckets with at
    // most 16 swaps instead of rebuilding and sorting the entire assembly.
    std::vector<ProgressiveShadedPlanGroup> progressiveShadedPlanGroups_;
    std::unordered_map<Obol::PartId, size_t, std::hash<Obol::PartId>>
        progressiveShadedPlanGroupByPart_;
    std::unordered_map<Obol::InstanceId, uint32_t,
                       std::hash<Obol::InstanceId>>
        progressivePlanIndexByInstance_;
    uint64_t nextPlanRevision_ = 1;
    uint64_t nextGeometryRevision_ = 1;
    uint64_t nextSubpixelProxyRevision_ = 1;
    uint64_t geometryRevision_ = 0;
    uint64_t subpixelProxyStatePlanRevision_ = 0;
    uint64_t subpixelProxyViewPlanRevision_ = 0;
    SbMatrix subpixelProxyViewProj_;
    SbVec2s subpixelProxyViewportSize_ = SbVec2s(0, 0);
    bool subpixelProxyViewValid_ = false;
    // The presentation plan fixes visible-instance order for its lifetime.
    // Keep hysteresis state by that index rather than hashing every proxy on
    // camera-only frames.  All three vectors are reset when the plan changes.
    std::vector<uint8_t> subpixelProxyState_;
    std::vector<uint8_t> subpixelProxyScratchMask_;
    std::vector<Obol::internal::CadSubpixelProxyPoint>
        subpixelProxyScratchPoints_;
    bool planDirty_    = true;   ///< Plan must be rebuilt before next render
    bool geometryDirty_ = true;  ///< Flattened geometry must be rebuilt
    int  cachedDM_     = -1;     ///< Draw mode used for the cached plan

    // VBO + shader renderer (lazy-created on first GLRender call)
    std::unique_ptr<Obol::internal::CadRendererGL> renderer_;
    bool lastDirectSoftwareWire_ = false;

    // Rebuild instance BVH if dirty
    void rebuildBvhIfNeeded() {
        if (!bvhDirty_) return;
        std::vector<Obol::picking::CadInstanceBVH::Entry> entries;
        entries.reserve(instances_.size());
        for (const auto& [iid, idata] : instances_) {
            if (hidden_.count(iid) || unpickable_.count(iid))
                continue;
            Obol::picking::CadInstanceBVH::Entry e;
            e.worldBounds  = idata.worldBounds;
            e.instanceId   = iid;
            e.partId       = idata.partId;
            e.localToWorld = idata.localToRoot;
            e.lodLevel     = idata.lodLevel;
            entries.push_back(e);
        }
        instanceBvh_.build(std::move(entries));
        bvhDirty_ = false;
    }

    // Compute world bounds for an instance from part geometry
    SbBox3f computeWorldBounds(const Obol::PartGeometry& geom,
                               const SbMatrix& m) const {
        SbBox3f local;
        if (geom.points) { local.extendBy(geom.points->bounds); }
        if (geom.wire)   { local.extendBy(geom.wire->bounds);   }
        if (geom.shaded) { local.extendBy(geom.shaded->bounds); }
        if (local.isEmpty()) {
            // Fall back to a unit cube at the origin
            local.extendBy(SbVec3f(-0.5f,-0.5f,-0.5f));
            local.extendBy(SbVec3f( 0.5f, 0.5f, 0.5f));
        }
        // Transform all 8 corners
        SbBox3f world;
        SbVec3f mn, mx;
        local.getBounds(mn, mx);
        const float xs[2] = {mn[0], mx[0]};
        const float ys[2] = {mn[1], mx[1]};
        const float zs[2] = {mn[2], mx[2]};
        for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2; ++j)
        for (int k = 0; k < 2; ++k) {
            SbVec3f corner(xs[i], ys[j], zs[k]);
            SbVec3f wc;
            m.multVecMatrix(corner, wc);
            world.extendBy(wc);
        }
        return world;
    }

    void updatePartGeometry(
            Obol::PartId pid,
            const std::shared_ptr<const Obol::PartGeometry>& geom) {
        if (!geom) return;
        parts_[pid] = geom;
        if (geom->subpixelProxyEligible && geom->wire.has_value()) {
            std::array<SbVec3f, 8> corners;
            if (cadSubpixelProxyCorners(*geom->wire, corners))
                subpixelProxyCorners_[pid] = std::move(corners);
            else
                subpixelProxyCorners_.erase(pid);
        } else {
            subpixelProxyCorners_.erase(pid);
        }
        partGeneration_[pid] = nextGeneration_++;
        partEdgeBvhCache_.erase(pid);
        partTriBvhCache_.erase(pid);
        const bool progressive =
            (geom->shaded.has_value() && geom->shaded->isProgressive()) ||
            (geom->wire.has_value() && geom->wire->isProgressive());
        if (progressive)
            progressiveParts_.insert(pid);
        else
            progressiveParts_.erase(pid);
    }

    void recomputeWorldBoundsForPart(Obol::PartId pid) {
        auto geomIt = parts_.find(pid);
        for (auto& [iid, idata] : instances_) {
            if (idata.partId != pid)
                continue;
            idata.worldBounds = geomIt != parts_.end() && geomIt->second ?
                computeWorldBounds(*geomIt->second, idata.localToRoot) :
                SbBox3f();
        }
    }

    void recomputeWorldBoundsForParts(
        const std::unordered_set<Obol::PartId,
                                 std::hash<Obol::PartId>>& pids) {
        if (pids.empty())
            return;
        for (auto& [iid, idata] : instances_) {
            if (!pids.count(idata.partId))
                continue;
            auto geomIt = parts_.find(idata.partId);
            idata.worldBounds = geomIt != parts_.end() && geomIt->second ?
                computeWorldBounds(*geomIt->second, idata.localToRoot) :
                SbBox3f();
        }
    }

    void updateInstance(Obol::InstanceId iid,
                        const Obol::InstanceRecord& rec) {
        InstanceData& idata  = instances_[iid];
        idata.partId         = rec.part;
        idata.localToRoot    = rec.localToRoot;
        idata.style          = rec.style;
        idata.parent         = rec.parent;
        idata.childName      = rec.childName;
        idata.occurrenceIndex = rec.occurrenceIndex;
        idata.boolOp         = rec.boolOp;
        idata.lodLevel       = rec.lodLevel;

        auto geomIt = parts_.find(rec.part);
        idata.worldBounds = geomIt != parts_.end() && geomIt->second ?
            computeWorldBounds(*geomIt->second, rec.localToRoot) : SbBox3f();
    }

    void markDirty() {
        bvhDirty_ = true;
        planDirty_ = true;
        geometryDirty_ = true;
        progressiveShadedPlanGroups_.clear();
        progressiveShadedPlanGroupByPart_.clear();
        progressivePlanIndexByInstance_.clear();
    }

    static size_t progressiveLevelBin(uint8_t level) {
        return level < 16 ? static_cast<size_t>(level) : 16u;
    }

    /**
     * Build a CadFramePlan from the current instance and part databases.
     *
     * Groups instances by part to maximise batching in the renderer.
     * The instance list is sorted by part so each CadDrawItem covers a
     * contiguous run of visibleInstances.
     *
     * @param dm       Draw mode (WIREFRAME / SHADED / SHADED_WITH_EDGES).
     * @param selected Set of selected instance IDs (for colour override).
     * @param hidden   Set of hidden instance IDs (excluded from the plan).
     */
    Obol::internal::CadFramePlan buildFramePlan(
            int dm,
            const std::unordered_set<Obol::InstanceId,
                                     std::hash<Obol::InstanceId>>& selected,
            const std::unordered_set<Obol::InstanceId,
                                     std::hash<Obol::InstanceId>>& hidden) const
    {
        using namespace Obol::internal;

        CadFramePlan plan;
        if (instances_.empty()) return plan;

        // Collect (partId → list of CadVisibleInstance) grouped by part.
        // We use a stable insertion-order map so every run is contiguous.
        std::unordered_map<Obol::PartId,
                           std::vector<CadVisibleInstance>,
                           std::hash<Obol::PartId>> byPart;

        for (const auto& [iid, idata] : instances_) {
            // Skip hidden instances
            if (hidden.count(iid)) continue;

            CadVisibleInstance vi;
            vi.instanceId = iid;
            vi.partIndex  = 0; // filled in below

            // Copy transform: raw OI float[16] (row-major).
            // The renderer uploads with GL_FALSE so GL sees it as column-major
            // (= transpose of OI matrix), which is the correct GL convention.
            std::memcpy(vi.transform.data(), idata.localToRoot[0],
                        16 * sizeof(float));

            // Instance style already encodes application-specific selection
            // colors.  Keep the selected bit separate for view policies such
            // as selected-full-detail LoD, but do not impose a fixed CAD-node
            // highlight color here.
            const bool isSel = selected.count(iid) != 0;
            float r = 0.8f, g = 0.8f, b = 0.8f, a = 1.0f;
            if (idata.style.hasColorOverride) {
                r = idata.style.color[0];
                g = idata.style.color[1];
                b = idata.style.color[2];
                a = idata.style.color[3];
            }

            vi.rgba[0] = static_cast<uint8_t>(std::min(255.0f, r * 255.0f));
            vi.rgba[1] = static_cast<uint8_t>(std::min(255.0f, g * 255.0f));
            vi.rgba[2] = static_cast<uint8_t>(std::min(255.0f, b * 255.0f));
            vi.rgba[3] = static_cast<uint8_t>(std::min(255.0f, a * 255.0f));
            vi.lineWidth = std::max(1.0f, idata.style.lineWidth);
            vi.linePattern = idata.style.linePattern;
            vi.linePatternFactor = std::max<uint16_t>(
                1u, idata.style.linePatternFactor);
            if (vi.lineWidth != 1.0f || vi.linePattern != 0xffffu)
                plan.hasCustomWireStyle = true;
            vi.flags = (isSel ? 1u : 0u) |
                       (idata.style.hasColorOverride ? 4u : 0u);
            vi.lodLevel = idata.lodLevel;

            if (cadDebugEnabled()) {
                std::fprintf(stderr,
                             "SoCADAssembly plan instance=%016llx:%016llx "
                             "styleOverride=%d style=(%.9g %.9g %.9g %.9g) "
                             "rgba=(%u %u %u %u) selected=%d\n",
                             static_cast<unsigned long long>(iid.w0),
                             static_cast<unsigned long long>(iid.w1),
                             idata.style.hasColorOverride ? 1 : 0,
                             idata.style.color[0], idata.style.color[1],
                             idata.style.color[2], idata.style.color[3],
                             static_cast<unsigned>(vi.rgba[0]),
                             static_cast<unsigned>(vi.rgba[1]),
                             static_cast<unsigned>(vi.rgba[2]),
                             static_cast<unsigned>(vi.rgba[3]),
                             isSel ? 1 : 0);
            }

            // Store world bounding box for per-instance frustum culling.
            SbVec3f wbMn, wbMx;
            idata.worldBounds.getBounds(wbMn, wbMx);
            vi.wbMin[0] = wbMn[0]; vi.wbMin[1] = wbMn[1]; vi.wbMin[2] = wbMn[2];
            vi.wbMax[0] = wbMx[0]; vi.wbMax[1] = wbMx[1]; vi.wbMax[2] = wbMx[2];

            if (!idata.worldBounds.isEmpty())
                plan.worldBounds.extendBy(idata.worldBounds);

            byPart[idata.partId].push_back(vi);
        }

        const bool needWire   = (dm == SoCADAssembly::WIREFRAME ||
                                 dm == SoCADAssembly::SHADED_WITH_EDGES ||
                                 dm == SoCADAssembly::HIDDEN_LINE);
        const bool needShaded = (dm == SoCADAssembly::SHADED ||
                                 dm == SoCADAssembly::SHADED_WITH_EDGES ||
                                 dm == SoCADAssembly::HIDDEN_LINE);

        // Track which (part, type) pairs have already been added to requiredReps
        // to avoid duplicates (a part with both wire and shaded gets one entry each).
        std::unordered_set<Obol::PartId, std::hash<Obol::PartId>>
            requiredPointParts, requiredWireParts, requiredShadedParts;

        uint32_t baseInst = 0;
        for (auto& [pid, vis] : byPart) {
            auto partIt = parts_.find(pid);
            if (partIt == parts_.end() || !partIt->second) continue;
            const auto& geom = *partIt->second;

            /*
             * A retained PoP part is one shared resident buffer, but each
             * occurrence may select a different prefix.  Keep occurrences
             * with the same active level contiguous so the renderer can issue
             * at most one instanced draw per (part, level, wire-style), rather
             * than falling back to one draw per occurrence.
             *
             * The instance id tie-breaker also makes the frame plan stable
             * across unordered-map iteration order.
             */
            const bool progressiveWire =
                geom.wire.has_value() && geom.wire->isProgressive();
            const bool progressiveShaded =
                geom.shaded.has_value() && geom.shaded->isProgressive();
            std::sort(vis.begin(), vis.end(),
                [](const CadVisibleInstance& a,
                   const CadVisibleInstance& b) {
                    if (a.lodLevel != b.lodLevel)
                        return a.lodLevel < b.lodLevel;
                    if (a.lineWidth != b.lineWidth)
                        return a.lineWidth < b.lineWidth;
                    if (a.linePattern != b.linePattern)
                        return a.linePattern < b.linePattern;
                    if (a.linePatternFactor != b.linePatternFactor)
                        return a.linePatternFactor < b.linePatternFactor;
                    if (a.instanceId.w1 != b.instanceId.w1)
                        return a.instanceId.w1 < b.instanceId.w1;
                    return a.instanceId.w0 < b.instanceId.w0;
                });

            const uint32_t count = static_cast<uint32_t>(vis.size());
            if (count > 0 &&
                    ((needWire && geom.wire.has_value()) ||
                     (needShaded && geom.shaded.has_value()))) {
                uint8_t maximumLod = 0;
                for (const auto& instance : vis)
                    maximumLod = std::max(maximumLod, instance.lodLevel);
                plan.maximumRequestedLodByPart[pid] = maximumLod;
            }

            // Fill partIndex (index into the upcoming visibleInstances block)
            for (auto& vi : vis) vi.partIndex = baseInst;

            // Append instances to flat list
            for (const auto& vi : vis) plan.visibleInstances.push_back(vi);

            // Wire draw item
            if (needWire && geom.wire.has_value()) {
                CadDrawItem item;
                item.rep.part  = pid;
                item.rep.type  = CadRepType::WireSegments;
                uint32_t runStart = 0;
                while (runStart < count) {
                    uint32_t runEnd = runStart + 1;
                    while (runEnd < count &&
                           vis[runEnd].lineWidth == vis[runStart].lineWidth &&
                           vis[runEnd].linePattern == vis[runStart].linePattern &&
                           vis[runEnd].linePatternFactor ==
                               vis[runStart].linePatternFactor &&
                           (!progressiveWire ||
                            vis[runEnd].lodLevel ==
                                vis[runStart].lodLevel))
                        ++runEnd;
                    item.baseInstance = baseInst + runStart;
                    item.instanceCount = runEnd - runStart;
                    item.customWireStyle =
                        vis[runStart].lineWidth != 1.0f ||
                        vis[runStart].linePattern != 0xffffu;
                    plan.wireItems.push_back(item);
                    runStart = runEnd;
                }
                if (!requiredWireParts.count(pid)) {
                    plan.requiredReps.push_back(item.rep);
                    requiredWireParts.insert(pid);
                    plan.wirePartsWithUncollapsedInstances.insert(pid);
                }
            }

            if (geom.points.has_value() && !geom.points->positions.empty()) {
                CadDrawItem item;
                item.rep.part = pid;
                item.rep.type = CadRepType::Points;
                item.baseInstance = baseInst;
                item.instanceCount = count;
                plan.pointItems.push_back(item);
                if (!requiredPointParts.count(pid)) {
                    plan.requiredReps.push_back(item.rep);
                    requiredPointParts.insert(pid);
                }
            }

            // Shaded draw item
            if (needShaded && geom.shaded.has_value()) {
                const size_t itemBegin = plan.shadedItems.size();
                uint32_t runStart = 0;
                while (runStart < count) {
                    uint32_t runEnd = runStart + 1;
                    while (runEnd < count &&
                           (!progressiveShaded ||
                            vis[runEnd].lodLevel ==
                                vis[runStart].lodLevel))
                        ++runEnd;
                    CadDrawItem item;
                    item.rep.part  = pid;
                    item.rep.type  = CadRepType::Triangles;
                    item.baseInstance = baseInst + runStart;
                    item.instanceCount = runEnd - runStart;
                    item.cullBackfaces = geom.shadedCullBackfaces &&
                        std::all_of(vis.begin() + runStart,
                            vis.begin() + runEnd,
                            [](const CadVisibleInstance& instance) {
                                return instance.rgba[3] == 255 &&
                                    cadTransformPreservesOrientation(
                                        instance.transform);
                            });
                    plan.shadedItems.push_back(item);
                    runStart = runEnd;
                }
                /*
                 * A progressive part can occupy no more than one run per PoP
                 * level, and no more runs than it has occurrences.  Reserve
                 * that fixed number of draw-item slots in the cached plan.
                 * Sparse level changes can then alter run boundaries in place
                 * without inserting into the global draw-item vector.
                 */
                if (progressiveShaded) {
                    const size_t slotCount =
                        std::min<size_t>(count, ProgressiveLevelBinCount);
                    while (plan.shadedItems.size() - itemBegin < slotCount) {
                        CadDrawItem item;
                        item.rep.part = pid;
                        item.rep.type = CadRepType::Triangles;
                        item.baseInstance = baseInst;
                        item.instanceCount = 0;
                        item.cullBackfaces = geom.shadedCullBackfaces &&
                            std::all_of(vis.begin(), vis.end(),
                                [](const CadVisibleInstance& instance) {
                                    return instance.rgba[3] == 255 &&
                                        cadTransformPreservesOrientation(
                                            instance.transform);
                                });
                        plan.shadedItems.push_back(item);
                    }
                }
                if (!requiredShadedParts.count(pid)) {
                    CadRepKey rep;
                    rep.part = pid;
                    rep.type = CadRepType::Triangles;
                    plan.requiredReps.push_back(rep);
                    requiredShadedParts.insert(pid);
                }
            }

            baseInst += count;
        }

        return plan;
    }

    void rebuildProgressiveShadedPlanIndex() {
        progressiveShadedPlanGroups_.clear();
        progressiveShadedPlanGroupByPart_.clear();
        progressivePlanIndexByInstance_.clear();
        if (cachedDM_ != SoCADAssembly::SHADED ||
                cachedPlan_.visibleInstances.empty())
            return;

        size_t base = 0;
        while (base < cachedPlan_.visibleInstances.size()) {
            const uint32_t partBase =
                cachedPlan_.visibleInstances[base].partIndex;
            size_t end = base + 1;
            while (end < cachedPlan_.visibleInstances.size() &&
                    cachedPlan_.visibleInstances[end].partIndex == partBase)
                ++end;

            const auto instanceFound = instances_.find(
                cachedPlan_.visibleInstances[base].instanceId);
            if (instanceFound == instances_.end()) {
                base = end;
                continue;
            }
            const Obol::PartId part = instanceFound->second.partId;
            const auto geometryFound = parts_.find(part);
            if (geometryFound == parts_.end() || !geometryFound->second ||
                    !geometryFound->second->shaded.has_value() ||
                    !geometryFound->second->shaded->isProgressive()) {
                base = end;
                continue;
            }

            ProgressiveShadedPlanGroup group;
            group.part = part;
            group.baseInstance = static_cast<uint32_t>(base);
            group.instanceCount = static_cast<uint32_t>(end - base);
            for (size_t i = base; i < end; ++i) {
                const auto& instance = cachedPlan_.visibleInstances[i];
                ++group.levelCounts[progressiveLevelBin(instance.lodLevel)];
                progressivePlanIndexByInstance_[instance.instanceId] =
                    static_cast<uint32_t>(i);
            }
            for (size_t i = 0; i < cachedPlan_.shadedItems.size(); ++i) {
                if (!(cachedPlan_.shadedItems[i].rep.part == part))
                    continue;
                if (group.shadedItemCount == 0)
                    group.shadedItemBegin = i;
                ++group.shadedItemCount;
            }
            if (group.shadedItemCount == 0) {
                base = end;
                continue;
            }
            progressiveShadedPlanGroupByPart_[part] =
                progressiveShadedPlanGroups_.size();
            progressiveShadedPlanGroups_.push_back(group);
            base = end;
        }
    }

    bool patchProgressiveShadedPlanLod(
            Obol::InstanceId instance, uint8_t lodLevel,
            std::unordered_set<size_t>& changedGroups) {
        if (planDirty_ || geometryDirty_ ||
                cachedDM_ != SoCADAssembly::SHADED)
            return false;
        const auto indexFound =
            progressivePlanIndexByInstance_.find(instance);
        if (indexFound == progressivePlanIndexByInstance_.end())
            return false;
        uint32_t index = indexFound->second;
        if (index >= cachedPlan_.visibleInstances.size())
            return false;
        auto& visible = cachedPlan_.visibleInstances;
        const auto instanceFound = instances_.find(instance);
        if (instanceFound == instances_.end())
            return false;
        const auto groupFound =
            progressiveShadedPlanGroupByPart_.find(
                instanceFound->second.partId);
        if (groupFound == progressiveShadedPlanGroupByPart_.end())
            return false;
        const size_t groupIndex = groupFound->second;
        ProgressiveShadedPlanGroup& group =
            progressiveShadedPlanGroups_[groupIndex];
        const size_t oldBin = progressiveLevelBin(visible[index].lodLevel);
        const size_t newBin = progressiveLevelBin(lodLevel);

        const auto swapVisible = [&](uint32_t left, uint32_t right) {
            if (left == right)
                return;
            std::swap(visible[left], visible[right]);
            progressivePlanIndexByInstance_[visible[left].instanceId] = left;
            progressivePlanIndexByInstance_[visible[right].instanceId] = right;
        };

        if (oldBin < newBin) {
            uint32_t boundary = group.baseInstance;
            for (size_t bin = 0; bin <= oldBin; ++bin)
                boundary += group.levelCounts[bin];
            swapVisible(index, boundary - 1);
            index = boundary - 1;
            for (size_t bin = oldBin + 1; bin <= newBin; ++bin) {
                boundary += group.levelCounts[bin];
                swapVisible(index, boundary - 1);
                index = boundary - 1;
            }
        } else if (oldBin > newBin) {
            uint32_t boundary = group.baseInstance;
            for (size_t bin = 0; bin < oldBin; ++bin)
                boundary += group.levelCounts[bin];
            swapVisible(index, boundary);
            index = boundary;
            for (size_t bin = oldBin; bin-- > newBin; ) {
                uint32_t previousBoundary = group.baseInstance;
                for (size_t prior = 0; prior < bin; ++prior)
                    previousBoundary += group.levelCounts[prior];
                swapVisible(index, previousBoundary);
                index = previousBoundary;
            }
        }
        visible[index].lodLevel = lodLevel;
        progressivePlanIndexByInstance_[instance] = index;
        if (oldBin != newBin) {
            --group.levelCounts[oldBin];
            ++group.levelCounts[newBin];
        }
        changedGroups.insert(groupIndex);
        return true;
    }

    void finishProgressiveShadedPlanPatch(
            const std::unordered_set<size_t>& changedGroups) {
        for (size_t groupIndex : changedGroups) {
            if (groupIndex >= progressiveShadedPlanGroups_.size())
                continue;
            ProgressiveShadedPlanGroup& group =
                progressiveShadedPlanGroups_[groupIndex];
            if (group.shadedItemBegin + group.shadedItemCount >
                    cachedPlan_.shadedItems.size())
                continue;
            const bool cullBackfaces =
                cachedPlan_.shadedItems[group.shadedItemBegin].cullBackfaces;
            size_t slot = 0;
            uint32_t base = group.baseInstance;
            for (size_t bin = 0;
                    bin < ProgressiveLevelBinCount &&
                    slot < group.shadedItemCount; ++bin) {
                const uint32_t count = group.levelCounts[bin];
                if (!count)
                    continue;
                auto& item =
                    cachedPlan_.shadedItems[group.shadedItemBegin + slot++];
                item.rep.part = group.part;
                item.rep.type =
                    Obol::internal::CadRepType::Triangles;
                item.baseInstance = base;
                item.instanceCount = count;
                item.cullBackfaces = cullBackfaces;
                base += count;
            }
            while (slot < group.shadedItemCount) {
                auto& item =
                    cachedPlan_.shadedItems[group.shadedItemBegin + slot++];
                item.rep.part = group.part;
                item.rep.type =
                    Obol::internal::CadRepType::Triangles;
                item.baseInstance = group.baseInstance;
                item.instanceCount = 0;
                item.cullBackfaces = cullBackfaces;
            }
            uint8_t maximumLod = 0;
            for (size_t bin = 0; bin < ProgressiveLevelBinCount; ++bin) {
                if (group.levelCounts[bin])
                    maximumLod = static_cast<uint8_t>(
                        std::min<size_t>(bin, 15u));
            }
            cachedPlan_.maximumRequestedLodByPart[group.part] = maximumLod;
        }
        cachedPlan_.revision = nextPlanRevision_++;
        if (nextPlanRevision_ == 0)
            nextPlanRevision_ = 1;
        subpixelProxyStatePlanRevision_ = 0;
    }

    void updateSubpixelProxyPlan(const SbMatrix& viewProj,
                                 const SbVec2s& viewportSize)
    {
        using namespace Obol::internal;
        CadFramePlan& plan = cachedPlan_;
        if (subpixelProxyViewValid_ &&
                subpixelProxyViewPlanRevision_ == plan.revision &&
                subpixelProxyViewportSize_[0] == viewportSize[0] &&
                subpixelProxyViewportSize_[1] == viewportSize[1] &&
                subpixelProxyViewProj_ == viewProj)
            return;

        if (subpixelProxyStatePlanRevision_ != plan.revision) {
            subpixelProxyState_.assign(plan.visibleInstances.size(), 0u);
            subpixelProxyStatePlanRevision_ = plan.revision;
        }

        std::vector<uint8_t>& mask = subpixelProxyScratchMask_;
        std::vector<CadSubpixelProxyPoint>& points =
            subpixelProxyScratchPoints_;
        mask.assign(plan.visibleInstances.size(), 0u);
        points.clear();
        plan.wirePartsWithUncollapsedInstances.clear();
        for (const CadDrawItem& item : plan.wireItems) {
            const auto partIt = parts_.find(item.rep.part);
            const auto cornersIt = subpixelProxyCorners_.find(item.rep.part);
            if (partIt == parts_.end() || !partIt->second ||
                    cornersIt == subpixelProxyCorners_.end()) {
                if (item.instanceCount > 0)
                    plan.wirePartsWithUncollapsedInstances.insert(
                        item.rep.part);
                continue;
            }

            for (uint32_t i = 0; i < item.instanceCount; ++i) {
                const size_t visibleIndex = item.baseInstance + i;
                if (visibleIndex >= plan.visibleInstances.size()) {
                    plan.wirePartsWithUncollapsedInstances.insert(
                        item.rep.part);
                    continue;
                }
                const CadVisibleInstance& instance =
                    plan.visibleInstances[visibleIndex];
                const float threshold =
                    subpixelProxyState_[visibleIndex] ?
                    1.25f : 0.75f;
                SbVec3f point;
                if (!cadSubpixelProxyPoint(cornersIt->second, instance, viewProj,
                        viewportSize, threshold, point)) {
                    subpixelProxyState_[visibleIndex] = 0u;
                    plan.wirePartsWithUncollapsedInstances.insert(
                        item.rep.part);
                    continue;
                }

                subpixelProxyState_[visibleIndex] = 1u;
                mask[visibleIndex] = 1u;
                CadSubpixelProxyPoint replacement;
                replacement.position = point;
                replacement.rgba = instance.rgba;
                replacement.instanceId = instance.instanceId;
                points.push_back(replacement);
            }
        }

        const bool changed = plan.subpixelProxySourcePlanRevision !=
                plan.revision || mask != plan.subpixelProxyMask ||
                !cadSameSubpixelProxyPoints(points,
                    plan.subpixelProxyPoints);
        if (changed) {
            // Swapping preserves previous storage in the scratch vectors for
            // the next camera update instead of allocating frame-local data.
            plan.subpixelProxyMask.swap(mask);
            plan.subpixelProxyPoints.swap(points);
            plan.subpixelProxySourcePlanRevision = plan.revision;
            plan.subpixelProxyRevision = nextSubpixelProxyRevision_++;
            if (nextSubpixelProxyRevision_ == 0)
                nextSubpixelProxyRevision_ = 1;
        }
        subpixelProxyViewProj_ = viewProj;
        subpixelProxyViewportSize_ = viewportSize;
        subpixelProxyViewPlanRevision_ = plan.revision;
        subpixelProxyViewValid_ = true;
    }
};

// ---------------------------------------------------------------------------
// SoCADAssembly
// ---------------------------------------------------------------------------

SO_NODE_SOURCE(SoCADAssembly);

void
SoCADAssembly::initClass()
{
    SO_NODE_INIT_CLASS(SoCADAssembly, SoNode, "Node");
    SoCADDetail::initClass();
    SoCADViewState::initClass();
}

SoCADAssembly::SoCADAssembly()
    : impl_(new SoCADAssemblyImpl)
{
    SO_NODE_CONSTRUCTOR(SoCADAssembly);

    SO_NODE_DEFINE_ENUM_VALUE(DrawMode, SHADED);
    SO_NODE_DEFINE_ENUM_VALUE(DrawMode, WIREFRAME);
    SO_NODE_DEFINE_ENUM_VALUE(DrawMode, SHADED_WITH_EDGES);
    SO_NODE_DEFINE_ENUM_VALUE(DrawMode, HIDDEN_LINE);
    SO_NODE_SET_SF_ENUM_TYPE(drawMode, DrawMode);
    SO_NODE_ADD_FIELD(drawMode, (WIREFRAME));

    SO_NODE_DEFINE_ENUM_VALUE(PickMode, PICK_AUTO);
    SO_NODE_DEFINE_ENUM_VALUE(PickMode, PICK_EDGE);
    SO_NODE_DEFINE_ENUM_VALUE(PickMode, PICK_TRIANGLE);
    SO_NODE_DEFINE_ENUM_VALUE(PickMode, PICK_BOUNDS);
    SO_NODE_DEFINE_ENUM_VALUE(PickMode, PICK_HYBRID);
    SO_NODE_SET_SF_ENUM_TYPE(pickMode, PickMode);
    SO_NODE_ADD_FIELD(pickMode, (PICK_AUTO));

    SO_NODE_ADD_FIELD(edgePickTolerancePx, (5.0f));
    SO_NODE_ADD_FIELD(wireframeOcclusion,  (FALSE));
    SO_NODE_ADD_FIELD(progressiveLodCeiling, (-1));
}

SoCADAssembly::~SoCADAssembly() = default;

SoDetail*
SoCADAssembly::createPickDetail(
    const Obol::CadPickDetailRecord& hit) const
{
    SoCADDetail* detail = new SoCADDetail;
    detail->setInstanceId(hit.instance);
    detail->setPartId(hit.part);
    switch (hit.primitiveKind) {
        case Obol::CadPickDetailRecord::EDGE:
            detail->setPrimType(SoCADDetail::EDGE);
            detail->setPrimIndex0(hit.primIndex0);
            detail->setPrimIndex1(hit.primIndex1);
            detail->setU(hit.u);
            break;
        case Obol::CadPickDetailRecord::TRIANGLE:
            detail->setPrimType(SoCADDetail::TRIANGLE);
            detail->setPrimIndex0(hit.primIndex0);
            break;
        case Obol::CadPickDetailRecord::POINT:
            detail->setPrimType(SoCADDetail::POINT);
            detail->setPrimIndex0(hit.primIndex0);
            break;
        case Obol::CadPickDetailRecord::BOUNDS:
        default:
            detail->setPrimType(SoCADDetail::BOUNDS);
            break;
    }
    return detail;
}

// --- Update framing --------------------------------------------------------

void SoCADAssembly::beginUpdate() { impl_->inUpdate_ = true; }

void SoCADAssembly::endUpdate()
{
    impl_->inUpdate_ = false;
    /* Public mutations record the exact caches they invalidate even while
     * notifications are batched.  Do not turn a selection/style-only batch
     * into a geometry and BVH rebuild merely because it was framed by
     * beginUpdate()/endUpdate(). */
    touch();
}

void
SoCADAssembly::clear()
{
    impl_->parts_.clear();
    impl_->subpixelProxyCorners_.clear();
    impl_->partGeneration_.clear();
    impl_->instances_.clear();
    impl_->selected_.clear();
    impl_->hidden_.clear();
    impl_->unpickable_.clear();
    impl_->partEdgeBvhCache_.clear();
    impl_->partTriBvhCache_.clear();
    impl_->progressiveParts_.clear();
    impl_->instanceBvh_ = Obol::picking::CadInstanceBVH();
    impl_->bvhDirty_ = true;
    impl_->planDirty_ = true;
    impl_->geometryDirty_ = true;
    if (!impl_->inUpdate_)
        touch();
}

// --- Part library ----------------------------------------------------------

void
SoCADAssembly::upsertPart(Obol::PartId pid, const Obol::PartGeometry& geom)
{
    impl_->updatePartGeometry(pid,
        std::make_shared<const Obol::PartGeometry>(geom));
    impl_->recomputeWorldBoundsForPart(pid);
    impl_->markDirty();
    if (!impl_->inUpdate_) touch();
}

void
SoCADAssembly::upsertParts(const std::vector<Obol::PartUpdate>& updates)
{
    if (updates.empty())
        return;

    std::unordered_set<Obol::PartId, std::hash<Obol::PartId>> changedParts;
    changedParts.reserve(updates.size());
    for (const auto& update : updates) {
        impl_->updatePartGeometry(update.part,
            std::make_shared<const Obol::PartGeometry>(update.geometry));
        changedParts.insert(update.part);
    }
    impl_->recomputeWorldBoundsForParts(changedParts);
    impl_->markDirty();
    if (!impl_->inUpdate_)
        touch();
}

void
SoCADAssembly::upsertSharedParts(
    const std::vector<Obol::SharedPartUpdate>& updates)
{
    if (updates.empty()) return;
    std::unordered_set<Obol::PartId, std::hash<Obol::PartId>> changedParts;
    changedParts.reserve(updates.size());
    for (const auto& update : updates) {
        if (!update.geometry) continue;
        impl_->updatePartGeometry(update.part, update.geometry);
        changedParts.insert(update.part);
    }
    if (changedParts.empty()) return;
    impl_->recomputeWorldBoundsForParts(changedParts);
    impl_->markDirty();
    if (!impl_->inUpdate_) touch();
}

void
SoCADAssembly::removePart(Obol::PartId pid)
{
    impl_->parts_.erase(pid);
    impl_->subpixelProxyCorners_.erase(pid);
    impl_->partGeneration_.erase(pid);
    impl_->partEdgeBvhCache_.erase(pid);
    impl_->partTriBvhCache_.erase(pid);
    impl_->progressiveParts_.erase(pid);
    impl_->recomputeWorldBoundsForPart(pid);
    impl_->markDirty();
    if (!impl_->inUpdate_) touch();
}

void
SoCADAssembly::removeParts(const std::vector<Obol::PartId>& pids)
{
    if (pids.empty()) return;
    std::unordered_set<Obol::PartId, std::hash<Obol::PartId>> changedParts;
    changedParts.reserve(pids.size());
    for (const Obol::PartId pid : pids) {
        if (!impl_->parts_.erase(pid))
            continue;
        changedParts.insert(pid);
        impl_->subpixelProxyCorners_.erase(pid);
        impl_->partGeneration_.erase(pid);
        impl_->partEdgeBvhCache_.erase(pid);
        impl_->partTriBvhCache_.erase(pid);
        impl_->progressiveParts_.erase(pid);
    }
    if (changedParts.empty()) return;
    impl_->recomputeWorldBoundsForParts(changedParts);
    impl_->markDirty();
    if (!impl_->inUpdate_) touch();
}

// --- Instance management ---------------------------------------------------

Obol::InstanceId
SoCADAssembly::upsertInstanceAuto(const Obol::InstanceRecord& rec)
{
    Obol::InstanceId iid = Obol::CadIdBuilder::extendNameOccBool(
        rec.parent, rec.childName, rec.occurrenceIndex, rec.boolOp);
    upsertInstance(iid, rec);
    return iid;
}

void
SoCADAssembly::upsertInstance(Obol::InstanceId iid, const Obol::InstanceRecord& rec)
{
    impl_->updateInstance(iid, rec);
    impl_->markDirty();
    if (!impl_->inUpdate_) touch();
}

std::vector<Obol::InstanceId>
SoCADAssembly::upsertInstancesAuto(
    const std::vector<Obol::InstanceRecord>& records)
{
    std::vector<Obol::InstanceId> ids;
    ids.reserve(records.size());
    if (records.empty())
        return ids;

    for (const auto& rec : records) {
        Obol::InstanceId iid = Obol::CadIdBuilder::extendNameOccBool(
            rec.parent, rec.childName, rec.occurrenceIndex, rec.boolOp);
        ids.push_back(iid);
        impl_->updateInstance(iid, rec);
    }
    impl_->markDirty();
    if (!impl_->inUpdate_)
        touch();
    return ids;
}

void
SoCADAssembly::upsertInstances(
    const std::vector<Obol::InstanceUpdate>& updates)
{
    if (updates.empty())
        return;

    for (const auto& update : updates)
        impl_->updateInstance(update.instance, update.record);
    impl_->markDirty();
    if (!impl_->inUpdate_)
        touch();
}

void
SoCADAssembly::updateInstanceLodLevels(
    const std::vector<Obol::InstanceLodUpdate>& updates)
{
    bool changed = false;
    bool sparsePlanPatch = !impl_->planDirty_ && !impl_->geometryDirty_ &&
        impl_->cachedDM_ == SoCADAssembly::SHADED;
    std::unordered_set<size_t> changedPlanGroups;
    for (const auto& update : updates) {
        auto found = impl_->instances_.find(update.instance);
        if (found == impl_->instances_.end() ||
                found->second.lodLevel == update.lodLevel)
            continue;
        if (sparsePlanPatch &&
                !impl_->patchProgressiveShadedPlanLod(
                    update.instance, update.lodLevel, changedPlanGroups))
            sparsePlanPatch = false;
        found->second.lodLevel = update.lodLevel;
        changed = true;
    }
    if (!changed) return;
    if (sparsePlanPatch)
        impl_->finishProgressiveShadedPlanPatch(changedPlanGroups);
    else
        impl_->planDirty_ = true;
    /* The instance BVH also carries the active cut used by exact picking.
     * Rebuilding remains lazy and therefore does not add work to rendering. */
    impl_->bvhDirty_ = true;
    if (!impl_->inUpdate_) touch();
}

void
SoCADAssembly::removeInstance(Obol::InstanceId iid)
{
    impl_->instances_.erase(iid);
    impl_->selected_.erase(iid);
    impl_->hidden_.erase(iid);
    impl_->unpickable_.erase(iid);
    impl_->bvhDirty_  = true;
    impl_->planDirty_ = true;
    impl_->geometryDirty_ = true;
    if (!impl_->inUpdate_) touch();
}

void
SoCADAssembly::updateInstanceTransform(Obol::InstanceId iid, const SbMatrix& m)
{
    auto it = impl_->instances_.find(iid);
    if (it == impl_->instances_.end()) return;
    it->second.localToRoot = m;
    auto geomIt = impl_->parts_.find(it->second.partId);
    if (geomIt != impl_->parts_.end() && geomIt->second) {
        it->second.worldBounds = impl_->computeWorldBounds(*geomIt->second, m);
    }
    impl_->bvhDirty_  = true;
    impl_->planDirty_ = true;
    impl_->geometryDirty_ = true;
    if (!impl_->inUpdate_) touch();
}

void
SoCADAssembly::updateInstanceStyle(Obol::InstanceId iid, const Obol::InstanceStyle& style)
{
    auto it = impl_->instances_.find(iid);
    if (it == impl_->instances_.end()) return;
    it->second.style = style;
    impl_->planDirty_ = true;
    if (!impl_->inUpdate_) touch();
}

void
SoCADAssembly::updateInstanceStyles(
    const std::vector<Obol::InstanceStyleUpdate>& updates)
{
    if (updates.empty()) return;
    bool changed = false;
    for (const auto& update : updates) {
        auto it = impl_->instances_.find(update.instance);
        if (it == impl_->instances_.end()) continue;
        it->second.style = update.style;
        changed = true;
    }
    if (!changed) return;
    impl_->planDirty_ = true;
    if (!impl_->inUpdate_) touch();
}

void
SoCADAssembly::setSelectedInstances(const std::vector<Obol::InstanceId>& ids)
{
    impl_->selected_.clear();
    impl_->selected_.insert(ids.begin(), ids.end());
    impl_->planDirty_ = true;
    if (!impl_->inUpdate_) touch();
}

// --- Query -----------------------------------------------------------------

size_t SoCADAssembly::instanceCount() const { return impl_->instances_.size(); }
size_t SoCADAssembly::partCount()     const { return impl_->parts_.size();     }

std::vector<Obol::InstanceId>
SoCADAssembly::instanceIds() const
{
    std::vector<Obol::InstanceId> ids;
    ids.reserve(impl_->instances_.size());
    for (const auto &entry : impl_->instances_)
        ids.push_back(entry.first);
    std::sort(ids.begin(), ids.end(),
        [](const Obol::InstanceId &a, const Obol::InstanceId &b) {
            return a.w0 != b.w0 ? a.w0 < b.w0 : a.w1 < b.w1;
        });
    return ids;
}

bool
SoCADAssembly::isInstanceHidden(Obol::InstanceId iid) const
{
    return impl_->hidden_.find(iid) != impl_->hidden_.end();
}

bool SoCADAssembly::hasProgressivePartLod() const
{
    return !impl_->progressiveParts_.empty();
}

uint8_t SoCADAssembly::effectiveProgressiveLodLevel(
    uint8_t requested) const
{
    const int ceiling = progressiveLodCeiling.getValue();
    if (ceiling < 0 || ceiling > 15)
        return requested;
    return std::min(requested, static_cast<uint8_t>(ceiling));
}

const Obol::PartGeometry*
SoCADAssembly::partGeometry(Obol::PartId pid) const
{
    auto it = impl_->parts_.find(pid);
    if (it == impl_->parts_.end()) return nullptr;
    return it->second.get();
}

std::optional<Obol::InstanceRecord>
SoCADAssembly::getInstanceRecord(Obol::InstanceId iid) const
{
    auto it = impl_->instances_.find(iid);
    if (it == impl_->instances_.end()) return std::nullopt;
    const InstanceData& d = it->second;
    Obol::InstanceRecord rec;
    rec.part        = d.partId;
    rec.localToRoot = d.localToRoot;
    rec.style       = d.style;
    rec.parent      = d.parent;
    rec.childName  = d.childName;
    rec.occurrenceIndex = d.occurrenceIndex;
    rec.boolOp      = d.boolOp;
    rec.lodLevel    = d.lodLevel;
    return rec;
}

void
SoCADAssembly::setHiddenInstances(const std::vector<Obol::InstanceId>& ids)
{
    impl_->hidden_.clear();
    impl_->hidden_.insert(ids.begin(), ids.end());
    impl_->bvhDirty_ = true;
    impl_->planDirty_ = true;
    impl_->geometryDirty_ = true;
    if (!impl_->inUpdate_) touch();
}

void
SoCADAssembly::setUnpickableInstances(const std::vector<Obol::InstanceId>& ids)
{
    impl_->unpickable_.clear();
    impl_->unpickable_.insert(ids.begin(), ids.end());
    impl_->bvhDirty_ = true;
    if (!impl_->inUpdate_) touch();
}

// ---------------------------------------------------------------------------
// GLRender
// ---------------------------------------------------------------------------

void
SoCADAssembly::GLRender(SoGLRenderAction* action)
{
    if (impl_->instances_.empty()) return;

    SoState* state = action->getState();

    // Obtain the GL dispatch context for the active rendering backend.
    // This routes calls correctly to either the system OpenGL or OSMesa.
    const SoGLContext* glue = sogl_glue_from_state(state);
    if (!glue) return;

    // SoCADAssembly issues GL calls directly instead of going through
    // SoShape.  Synchronize Coin's lazy shape state before doing so: the raw
    // GL cull bit may legitimately still describe a sibling whose separator
    // has already popped, while the element state already says this node is
    // two-sided.  Establish counter-clockwise/two-sided as the conservative
    // assembly baseline; the renderer locally enables culling only for parts
    // carrying a verified closed/oriented guarantee and restores this raw
    // state before returning.  Keeping this in a local state frame lets the
    // next node restore its own semantics through the normal lazy-element
    // path.
    state->push();
    SoShapeHintsElement::set(state, this,
        SoShapeHintsElement::COUNTERCLOCKWISE,
        SoShapeHintsElement::UNKNOWN_SHAPE_TYPE,
        SoShapeHintsElement::CONVEX);
    SoGLLazyElement::getInstance(state)->send(state,
        SoLazyElement::VERTEXORDERING_MASK |
        SoLazyElement::CULLING_MASK |
        SoLazyElement::TWOSIDE_MASK);

    // Lazy-create the renderer the first time we have a GL context.
    if (!impl_->renderer_) {
        impl_->renderer_ = std::make_unique<Obol::internal::CadRendererGL>();
    }

    // Build the combined view-projection matrix from the state stack.
    // Both matrices are OI row-major SbMatrix values.
    const SbMatrix viewMat = SoViewingMatrixElement::get(state);
    const SbMatrix projMat = SoProjectionMatrixElement::get(state);
    // OI post-multiply convention: VP = view * proj
    SbMatrix viewProj = viewMat;
    viewProj.multRight(projMat);

    const int dm = drawMode.getValue();

    // Rebuild the frame plan only when geometry, instances, styles, selection,
    // hidden set, or draw mode have changed.  Camera moves do NOT invalidate
    // the plan, so it is reused every frame during interactive orbit.
    if (impl_->planDirty_ || impl_->cachedDM_ != dm) {
        const bool geometryChanged = impl_->geometryDirty_ ||
                                     impl_->cachedDM_ != dm;
        impl_->cachedPlan_  = impl_->buildFramePlan(dm, impl_->selected_,
                                                     impl_->hidden_);
        impl_->cachedPlan_.revision = impl_->nextPlanRevision_++;
        if (impl_->nextPlanRevision_ == 0)
            impl_->nextPlanRevision_ = 1;
        if (geometryChanged) {
            impl_->geometryRevision_ = impl_->nextGeometryRevision_++;
            if (impl_->nextGeometryRevision_ == 0)
                impl_->nextGeometryRevision_ = 1;
        }
        impl_->cachedPlan_.geometryRevision = impl_->geometryRevision_;
        impl_->planDirty_   = false;
        impl_->geometryDirty_ = false;
        impl_->cachedDM_    = dm;
        impl_->rebuildProgressiveShadedPlanIndex();
    }

    const SbViewportRegion& viewport = SoViewportRegionElement::get(state);
    impl_->updateSubpixelProxyPlan(viewProj,
        viewport.getViewportSizePixels());

    const Obol::CadRenderState renderState =
        Obol::resolveCadRenderState(SoCADViewStateElement::get(state));

    const GLboolean lightingEnabled = glue->glIsEnabled(GL_LIGHTING);
    const GLboolean light0Enabled = glue->glIsEnabled(GL_LIGHT0);
    const bool hasTransparency = std::any_of(
        impl_->cachedPlan_.visibleInstances.begin(),
        impl_->cachedPlan_.visibleInstances.end(),
        [](const Obol::internal::CadVisibleInstance& instance) {
            return instance.rgba[3] < 255;
        });
    const GLboolean blendEnabled = glue->glIsEnabled(GL_BLEND);
    GLint blendSource = GL_ONE;
    GLint blendDestination = GL_ZERO;
    if (hasTransparency) {
        glue->glGetIntegerv(GL_BLEND_SRC, &blendSource);
        glue->glGetIntegerv(GL_BLEND_DST, &blendDestination);
        glue->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glue->glEnable(GL_BLEND);
    }

    // Explicit FAST mode allows ordinary software wireframes to bypass Mesa's
    // fixed-function interpreter.  AUTO is deliberately quality-first because
    // direct CPU rasterization is workload-dependent and can be slower.
    const bool softwareWire = dm == WIREFRAME &&
        renderState.softwareWireMode == Obol::CadSoftwareWireMode::FAST &&
        cadRenderSoftwareWire(impl_->cachedPlan_, *this, state, viewProj);
    impl_->lastDirectSoftwareWire_ = softwareWire;
    if (!softwareWire) {
        // Feed the shaded GLSL pass ALL enabled scene lights (the camera-tracked
        // headlight plus any in-scene database lights), so the hardware view
        // lights consistently with the fixed-function path instead of using a
        // single hardcoded world-fixed direction.  Directional, point, and spot
        // lights are all supported (up to CadRendererGL::kMaxLights), each with
        // its own RGB colour x intensity.
        //
        // The shaded GLSL pass lights in WORLD space (v_worldPos/v_norm use the
        // model matrix without the view).  Obol authors all its lights directly
        // in world coordinates -- the headlight direction is rewritten in world
        // space each frame, and DB light positions are world bbox centers -- and
        // places them without transform nodes above them, so the SoLight field
        // values ARE world space.  We therefore read the raw fields rather than
        // SoLightElement::getMatrix(), whose accumulated matrix for the
        // post-camera scene-lights group is contaminated with the view transform
        // in this custom render batch (which would put point/spot positions in
        // eye space and make them drift with the camera).
        const SoNodeList& lights = SoLightElement::getLights(state);
        std::vector<Obol::internal::CadRendererGL::GlLight> glLights;
        for (int li = 0; li < lights.getLength(); ++li) {
            SoLight* l = static_cast<SoLight*>(lights[li]);
            if (!l || !l->on.getValue())
                continue;
            const SbColor c = l->color.getValue();
            const float inten = l->intensity.getValue();
            Obol::internal::CadRendererGL::GlLight gl;
            gl.color[0] = c[0] * inten;
            gl.color[1] = c[1] * inten;
            gl.color[2] = c[2] * inten;
            if (l->isOfType(SoDirectionalLight::getClassTypeId())) {
                SoDirectionalLight* dl = static_cast<SoDirectionalLight*>(l);
                SbVec3f travel = dl->direction.getValue();
                if (travel.length() <= 0.0f)
                    continue;
                travel.normalize();
                gl.type = 0;  // directional; shader wants direction toward light
                gl.vec[0] = -travel[0]; gl.vec[1] = -travel[1];
                gl.vec[2] = -travel[2];
            } else if (l->isOfType(SoSpotLight::getClassTypeId())) {
                SoSpotLight* sl = static_cast<SoSpotLight*>(l);
                SbVec3f pos = sl->location.getValue();
                SbVec3f axis = sl->direction.getValue();
                if (axis.length() > 0.0f) axis.normalize();
                gl.type = 2;  // spot
                gl.vec[0] = pos[0];  gl.vec[1] = pos[1];  gl.vec[2] = pos[2];
                gl.axis[0] = axis[0]; gl.axis[1] = axis[1]; gl.axis[2] = axis[2];
                gl.cosCutoff =
                    static_cast<float>(std::cos(sl->cutOffAngle.getValue()));
            } else if (l->isOfType(SoPointLight::getClassTypeId())) {
                SoPointLight* pl = static_cast<SoPointLight*>(l);
                SbVec3f pos = pl->location.getValue();
                gl.type = 1;  // point
                gl.vec[0] = pos[0]; gl.vec[1] = pos[1]; gl.vec[2] = pos[2];
            } else {
                continue;
            }
            glLights.push_back(gl);
        }
        // Empty list => renderer falls back to its default fixed light.
        impl_->renderer_->setLights(glLights);
        if (cadLightDebugEnabled()) {
            static unsigned int reportCount = 0;
            if (reportCount++ < 32) {
                std::fprintf(stderr,
                    "SoCADAssembly lights count=%zu stateCount=%d",
                    glLights.size(), lights.getLength());
                for (size_t i = 0;
                     i < std::min<size_t>(glLights.size(), 2); ++i) {
                    const auto& gl = glLights[i];
                    std::fprintf(stderr,
                        " l%zu={type=%d vec=(%.9g,%.9g,%.9g) "
                        "axis=(%.9g,%.9g,%.9g) "
                        "color=(%.9g,%.9g,%.9g) cos=%.9g}",
                        i, gl.type,
                        gl.vec[0], gl.vec[1], gl.vec[2],
                        gl.axis[0], gl.axis[1], gl.axis[2],
                        gl.color[0], gl.color[1], gl.color[2],
                        gl.cosCutoff);
                }
                std::fprintf(stderr, "\n");
            }
        }

        // Delegate to the VBO + shader renderer (GL 2.0 minimum; optional GL
        // 3.1+ instanced path selected automatically when available).
        impl_->renderer_->render(impl_->cachedPlan_, *this, glue, viewProj,
                                 viewMat, projMat,
                                 impl_->partGeneration_);
    }
    if (hasTransparency) {
        glue->glBlendFunc(static_cast<GLenum>(blendSource),
                          static_cast<GLenum>(blendDestination));
        if (!blendEnabled) glue->glDisable(GL_BLEND);
    }
    if (cadDebugEnabled()) {
        std::fprintf(stderr,
                     "SoCADAssembly render tier=%d visible=%zu wireItems=%zu "
                     "shadedItems=%zu parts=%zu instances=%zu "
                     "softwareWireMode=%d direct=%d lighting=%d light0=%d\n",
                     impl_->renderer_->lastRenderTier(),
                     impl_->cachedPlan_.visibleInstances.size(),
                     impl_->cachedPlan_.wireItems.size(),
                     impl_->cachedPlan_.shadedItems.size(),
                     impl_->parts_.size(),
                     impl_->instances_.size(),
                     static_cast<int>(renderState.softwareWireMode),
                     softwareWire ? 1 : 0,
                     lightingEnabled ? 1 : 0, light0Enabled ? 1 : 0);
    }
    // The CAD renderer deliberately bypasses Coin's normal SoShape path and
    // issues raw GL calls.  Even though it restores the raw raster state it
    // borrows, material, lighting, blending, and shader transitions may no
    // longer match SoGLLazyElement's cached belief.  Invalidate that cache
    // before popping our local state frame so the next Coin node resends its
    // own state instead of inheriting a stale CAD frame.  This is the proper
    // renderer boundary; hosts must not compensate with extra clears or
    // presentation timing workarounds.
    SoGLLazyElement::getInstance(state)->reset(state,
        SoLazyElement::ALL_MASK);
    state->pop();
}

// ---------------------------------------------------------------------------
// rayPick
// ---------------------------------------------------------------------------

void
SoCADAssembly::rayPick(SoRayPickAction* action)
{
    if (impl_->instances_.empty()) return;

    impl_->rebuildBvhIfNeeded();

    // Match SoShape picking semantics: SoRayPickAction stores the active
    // ray in object space only after setObjectSpace() updates it from the
    // current traversal state.
    action->setObjectSpace();
    SbLine pickRay = action->getLine();

    // Determine effective pick mode
    int pm = pickMode.getValue();
    const bool automaticPick = pm == PICK_AUTO;
    if (pm == PICK_AUTO) {
        pm = (drawMode.getValue() == WIREFRAME) ? PICK_EDGE : PICK_TRIANGLE;
    }

    // Derive an assembly-space edge-pick tolerance from the screen-space field.
    // Approximate: use the view volume to find how large one pixel is in world
    // coordinates at the assembly centre, then scale by the user-specified tolerance.
    float toleranceWS = edgePickTolerancePx.getValue() * 0.01f;
    {
        SoState* state = action->getState();
        if (state) {
            const SbViewportRegion& vpr =
                SoViewportRegionElement::get(state);
            const SbViewVolume vv = SoViewVolumeElement::get(state);
            const float vpH = static_cast<float>(
                vpr.getViewportSizePixels()[1]);
            if (vpH > 0.0f && vv.getNearDist() > 0.0f) {
                // Pick ray distance to assembly centre (or fallback to near*10)
                SbBox3f bbox;
                for (const auto& [iid, idata] : impl_->instances_) {
                    if (impl_->hidden_.count(iid) ||
                            impl_->unpickable_.count(iid))
                        continue;
                    if (!idata.worldBounds.isEmpty())
                        bbox.extendBy(idata.worldBounds);
                }
                float dist = vv.getNearDist() * 10.0f;
                if (!bbox.isEmpty()) {
                    dist = (bbox.getCenter() - pickRay.getPosition())
                               .dot(pickRay.getDirection());
                    dist = std::max(vv.getNearDist(), dist);
                }
                // Height of the view volume at that distance (perspective or ortho)
                float nearH  = vv.getHeight();          // at nearDist for persp
                float nearD  = vv.getNearDist();
                float pixelH = (nearH / vpH) * (dist / nearD);
                toleranceWS = std::max(toleranceWS,
                    edgePickTolerancePx.getValue() * pixelH);
            }
        }
    }

    Obol::picking::CadPickResult result;
    const int configuredLodCeiling = progressiveLodCeiling.getValue();
    const uint8_t pickLodCeiling =
        configuredLodCeiling >= 0 && configuredLodCeiling <= 15 ?
        static_cast<uint8_t>(configuredLodCeiling) : 255;

    if (automaticPick || pm == PICK_EDGE || pm == PICK_HYBRID) {
        result = Obol::picking::CadPickQuery::pickPoint(
            pickRay, impl_->instanceBvh_, impl_->parts_, toleranceWS);
    }

    if (!result.valid && (pm == PICK_EDGE || pm == PICK_HYBRID)) {
        result = Obol::picking::CadPickQuery::pickEdge(
            pickRay,
            impl_->instanceBvh_,
            impl_->parts_,
            impl_->partEdgeBvhCache_,
            toleranceWS,
            pickLodCeiling);
    }

    if (!result.valid && (pm == PICK_TRIANGLE || pm == PICK_HYBRID)) {
        result = Obol::picking::CadPickQuery::pickTriangle(
            pickRay,
            impl_->instanceBvh_,
            impl_->parts_,
            impl_->partTriBvhCache_,
            toleranceWS,
            pickLodCeiling);
    }

    if (!result.valid && pm == PICK_BOUNDS) {
        result = Obol::picking::CadPickQuery::pickBounds(
            pickRay,
            impl_->instanceBvh_,
            toleranceWS);
    }

    // For PICK_HYBRID: also try bounds if triangle picking returned nothing.
    if (!result.valid && pm == PICK_HYBRID) {
        result = Obol::picking::CadPickQuery::pickBounds(
            pickRay,
            impl_->instanceBvh_,
            toleranceWS);
    }

    if (!result.valid) return;

    // Register the hit with the pick action
    SoPickedPoint* pp = action->addIntersection(result.hitPoint);
    if (!pp) return;

    Obol::CadPickDetailRecord hit;
    hit.instance = result.instanceId;
    hit.part = result.partId;
    hit.point = result.hitPoint;
    switch (result.primType) {
        case Obol::picking::CadPickResult::EDGE:
            hit.primitiveKind = Obol::CadPickDetailRecord::EDGE;
            hit.primIndex0 = result.primIndex0;
            hit.primIndex1 = result.primIndex1;
            hit.u = result.u;
            break;
        case Obol::picking::CadPickResult::TRIANGLE:
            hit.primitiveKind = Obol::CadPickDetailRecord::TRIANGLE;
            hit.primIndex0 = result.primIndex0;
            break;
        case Obol::picking::CadPickResult::POINT:
            hit.primitiveKind = Obol::CadPickDetailRecord::POINT;
            hit.primIndex0 = result.primIndex0;
            break;
        default:
            hit.primitiveKind = Obol::CadPickDetailRecord::BOUNDS;
            break;
    }

    SoDetail* detail = this->createPickDetail(hit);
    if (detail) {
        SoNode* detailNode = this;
        SoPath* path = pp->getPath();
        if (!path || path->findNode(this) < 0) {
            SoFullPath* fullPath = static_cast<SoFullPath*>(path);
            if (fullPath && fullPath->getLength() > 0)
                detailNode = fullPath->getTail();
        }
        pp->setDetail(detail, detailNode);
    }
}

// ---------------------------------------------------------------------------
// getBoundingBox
// ---------------------------------------------------------------------------

void
SoCADAssembly::getBoundingBox(SoGetBoundingBoxAction* action)
{
    SbBox3f worldBox;
    for (const auto& [iid, idata] : impl_->instances_) {
        if (impl_->hidden_.count(iid))
            continue;
        if (!idata.worldBounds.isEmpty()) {
            worldBox.extendBy(idata.worldBounds);
        }
    }
    if (!worldBox.isEmpty()) {
        action->extendBy(worldBox);
        action->setCenter(worldBox.getCenter(), TRUE);
    }
}

// ---------------------------------------------------------------------------
// getPrimitiveCount
// ---------------------------------------------------------------------------

void
SoCADAssembly::getPrimitiveCount(SoGetPrimitiveCountAction* action)
{
    // Count total segments and triangles across all visible instances
    int totalLines = 0;
    int totalTris  = 0;
    for (const auto& [iid, idata] : impl_->instances_) {
        if (impl_->hidden_.count(iid))
            continue;
        auto geomIt = impl_->parts_.find(idata.partId);
        if (geomIt == impl_->parts_.end() || !geomIt->second) continue;
        const auto& geom = *geomIt->second;
        if (geom.points)
            action->addNumPoints(static_cast<int>(geom.points->positions.size()));
        if (geom.wire) {
            totalLines += static_cast<int>(geom.wire->segmentCount());
            for (const auto& poly : geom.wire->polylines) {
                if (poly.points.size() >= 2) {
                    totalLines += static_cast<int>(poly.points.size() - 1);
                }
            }
        }
        if (geom.shaded) {
            totalTris += static_cast<int>(geom.shaded->indices.size() / 3);
        }
    }
    action->addNumLines(totalLines);
    action->addNumTriangles(totalTris);
}

// ---------------------------------------------------------------------------
// lastRenderTier
// ---------------------------------------------------------------------------

int
SoCADAssembly::lastRenderTier() const
{
    if (!impl_->renderer_) return -1;
    return impl_->renderer_->lastRenderTier();
}

bool
SoCADAssembly::lastRenderUsedDirectSoftwareWire() const
{
    return impl_->lastDirectSoftwareWire_;
}

size_t
SoCADAssembly::lastSubpixelProxyCount() const
{
    return impl_->cachedPlan_.subpixelProxyPoints.size();
}

uint64_t
SoCADAssembly::lastSubpixelProxyRevision() const
{
    return impl_->cachedPlan_.subpixelProxyRevision;
}
