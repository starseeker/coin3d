/**************************************************************************\\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the conditions in the Obol
 * license are met.
\\**************************************************************************/

/**
 * @file CadSoftwareWire.cpp
 * @brief Direct software-framebuffer wire rendering for CAD assemblies.
 */

#include "CadSoftwareWire.h"
#include "CadProgressiveUtils.h"

#include <Inventor/SoDB.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/elements/SoContextManagerElement.h>
#include <Inventor/elements/SoViewportRegionElement.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

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
    if (point.flags & Obol::internal::CadInstanceHidden)
        return;
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

static void
cadSoftwareProxyBox(unsigned char *pixels, unsigned int width,
                    unsigned int height, const SbVec2s& origin,
                    const SbVec2s& size, const SbMatrix& viewProj,
                    const Obol::internal::CadSubpixelProxyPoint& proxy)
{
    if (proxy.flags & Obol::internal::CadInstanceHidden)
        return;
    Obol::internal::CadVisibleInstance style;
    style.rgba = proxy.rgba;
    style.flags = proxy.flags;
    SbVec3f edgeStart;
    bool haveEdgeStart = false;
    Obol::internal::cadForEachAggregateProxyBoxVertex(
        proxy, [&](const SbVec3f& vertex) {
            if (!haveEdgeStart) {
                edgeStart = vertex;
                haveEdgeStart = true;
                return;
            }
            cadSoftwareSegment(pixels, width, height, origin, size, viewProj,
                edgeStart, vertex, style);
            haveEdgeStart = false;
        });
}

static SbVec3f
cadSoftwareSnapPoint(const SbVec3f& point, const Obol::WireRep& wire,
                     uint8_t level)
{
    if (!wire.isProgressive()) return point;
    return Obol::internal::cadProgressiveSnapPoint(
        point, wire.progressiveQuantizationMinimum,
        wire.progressiveQuantizationMaximum,
        wire.quantizationAtCut(level));
}

static SbVec3f
cadSoftwareSnapPoint(const SbVec3f& point, const Obol::TriMesh& mesh,
                     uint8_t level)
{
    if (!mesh.isProgressive()) return point;
    return Obol::internal::cadProgressiveSnapPoint(
        point, mesh.progressiveQuantizationMinimum,
        mesh.progressiveQuantizationMaximum,
        mesh.quantizationAtCut(level));
}

static bool
cadSoftwareBoxOutsideClip(const SbBox3f& bounds, const SbMatrix& transform)
{
    if (bounds.isEmpty()) return false;
    const SbVec3f minimum = bounds.getMin();
    const SbVec3f maximum = bounds.getMax();
    CadSoftwareClipPoint corners[8];
    for (unsigned int corner = 0; corner < 8; ++corner) {
        corners[corner] = cadSoftwareTransform(transform, SbVec3f(
            (corner & 1u) ? maximum[0] : minimum[0],
            (corner & 2u) ? maximum[1] : minimum[1],
            (corner & 4u) ? maximum[2] : minimum[2]));
    }
    for (int plane = 0; plane < 6; ++plane) {
        bool allOutside = true;
        for (const CadSoftwareClipPoint& corner : corners) {
            if (cadSoftwarePlaneValue(corner, plane) >= 0.0) {
                allOutside = false;
                break;
            }
        }
        if (allOutside) return true;
    }
    return false;
}


static uint64_t
cadSoftwareWorkAdd(uint64_t left, uint64_t right)
{
    return right > UINT64_MAX - left ? UINT64_MAX : left + right;
}

} // namespace

CadSoftwareWireRenderResult
cadRenderSoftwareWire(const Obol::internal::CadFramePlan& plan,
                      const SoCADAssembly& assembly,
                      const Obol::CadViewState& viewState,
                      SoState *state,
                      const SbMatrix& viewProj,
                      const std::vector<Obol::internal::CadSubpixelProxyPoint>&
                          subpixelProxyPoints)
{
    CadSoftwareWireRenderResult result;
    if (plan.wireItems.empty() || !plan.shadedItems.empty() ||
            viewState.wireframeOcclusion)
        return result;
    SoDB::ContextManager *manager = SoContextManagerElement::get(state);
    unsigned char *pixels = nullptr;
    unsigned int width = 0, height = 0, components = 0;
    if (!manager || !manager->getCurrentSoftwareFramebuffer(
            pixels, width, height, components) || components != 4)
        return result;
    const SbViewportRegion& viewport = SoViewportRegionElement::get(state);
    const SbVec2s origin = viewport.getViewportOriginPixels();
    const SbVec2s size = viewport.getViewportSizePixels();
    if (size[0] <= 0 || size[1] <= 0) return result;

    result.rendered = true;

    for (const auto& item : plan.wireItems) {
        const Obol::PartGeometry *geometry = assembly.partGeometry(item.rep.part);
        if (!geometry || !geometry->wire.has_value()) continue;
        const Obol::WireRep& wire = *geometry->wire;
        for (uint32_t i = 0; i < item.instanceCount; ++i) {
            const size_t visibleIndex = item.baseInstance + i;
            if (visibleIndex >= plan.visibleInstances.size())
                continue;
            const auto& instance = plan.visibleInstances[visibleIndex];
            if (instance.partIndex != item.partIndex ||
                    (instance.flags &
                        Obol::internal::CadInstanceHidden))
                continue;
            if (visibleIndex < plan.subpixelProxyMask.size() &&
                    plan.subpixelProxyMask[visibleIndex])
                continue;
            uint64_t instanceSegments = 0;
            SbMatrix model;
            model.setValue(instance.transform.data());
            SbMatrix transform = model;
            transform.multRight(viewProj);
            uint8_t level = Obol::cadEffectiveProgressiveCut(
                viewState, item.rep.part, instance.lodCut);
            if (wire.isProgressive()) {
                if (level == Obol::ProgressiveCutUnspecified)
                    level = wire.progressiveResidentCut;
                level = Obol::internal::cadResolvedProgressiveCut(
                    level, wire.progressiveMinimumCut,
                    wire.progressiveResidentCut);
                /* One logical occurrence must use one coherent cut.  A page
                 * that has only a coarser resident prefix constrains the
                 * entire visible mesh, while an offscreen page does not. */
                if (wire.hasAdaptiveProgressiveClusters()) {
                    for (const Obol::ProgressiveWireCluster& cluster :
                            wire.progressiveClusters) {
                        if (cluster.ranges.empty() ||
                                cadSoftwareBoxOutsideClip(
                                    cluster.bounds, transform))
                            continue;
                        const uint8_t resident =
                            cluster.residentCut ==
                                Obol::ProgressiveCutUnspecified ?
                                wire.progressiveResidentCut :
                                cluster.residentCut;
                        level = Obol::internal::cadResolvedProgressiveCut(
                            level, wire.progressiveMinimumCut, resident);
                    }
                }
            }
            if (const Obol::TriMesh *triangleEdges = wire.triangleEdges()) {
                const Obol::TriMesh& mesh = *triangleEdges;
                const size_t activePositionCount = mesh.isProgressive() ?
                    mesh.positionCountAtCut(level) : mesh.positions.size();
                if (wire.triangleEdgeSegmentCount != mesh.indices.size() ||
                        activePositionCount == 0u)
                    continue;
                const auto drawTriangleRange =
                    [&](size_t firstIndex, size_t indexCount) {
                    if (firstIndex >= mesh.indices.size()) return;
                    const size_t end = std::min(
                        mesh.indices.size(), firstIndex + indexCount);
                    for (size_t index = firstIndex;
                            index + 2 < end; index += 3) {
                        const uint32_t source[3] = {
                            mesh.indices[index],
                            mesh.indices[index + 1],
                            mesh.indices[index + 2]
                        };
                        /* A progressive cut may only reference its declared
                         * position prefix.  The richer resident vector is
                         * deliberately not the validity domain: accepting
                         * it would hide corrupt cut metadata here and permit
                         * an out-of-range VBO access in the GL executors. */
                        if (source[0] >= activePositionCount ||
                                source[1] >= activePositionCount ||
                                source[2] >= activePositionCount)
                            continue;
                        SbVec3f point[3];
                        for (int corner = 0; corner < 3; ++corner)
                            point[corner] = cadSoftwareSnapPoint(
                                mesh.positions[source[corner]], mesh, level);
                        for (int edge = 0; edge < 3; ++edge)
                            cadSoftwareSegment(
                                pixels, width, height, origin, size,
                                transform, point[edge],
                                point[(edge + 1) % 3], instance);
                        instanceSegments = cadSoftwareWorkAdd(
                            instanceSegments, 3);
                    }
                };
                if (mesh.hasAdaptiveProgressiveClusters()) {
                    for (const Obol::ProgressiveTriangleCluster& cluster :
                            mesh.progressiveClusters) {
                        if (cadSoftwareBoxOutsideClip(
                                cluster.bounds, transform))
                            continue;
                        for (const Obol::ProgressiveTriangleClusterRange& range :
                                cluster.ranges) {
                            if (range.activationCut > level) break;
                            drawTriangleRange(
                                range.firstIndex, range.indexCount);
                        }
                    }
                } else {
                    drawTriangleRange(0, mesh.isProgressive() ?
                        mesh.indexCountAtCut(level) : mesh.indices.size());
                }
                if (instanceSegments) {
                    result.work.lineCount = cadSoftwareWorkAdd(
                        result.work.lineCount, instanceSegments);
                    result.work.positionCount = cadSoftwareWorkAdd(
                        result.work.positionCount,
                        instanceSegments > UINT64_MAX / 2 ? UINT64_MAX :
                            instanceSegments * 2);
                    result.work.occurrenceCount = cadSoftwareWorkAdd(
                        result.work.occurrenceCount, 1);
                }
                continue;
            }
            const auto drawRange = [&](size_t firstSegment,
                                       size_t segmentCount) {
                if (firstSegment >= wire.segmentCount()) return;
                segmentCount = std::min(
                    segmentCount, wire.segmentCount() - firstSegment);
                const size_t end = (firstSegment + segmentCount) * 2;
                for (size_t p = firstSegment * 2; p + 1 < end; p += 2) {
                    const SbVec3f a = cadSoftwareSnapPoint(
                        wire.segmentPoints[p], wire, level);
                    const SbVec3f b = cadSoftwareSnapPoint(
                        wire.segmentPoints[p + 1], wire, level);
                    cadSoftwareSegment(pixels, width, height, origin, size,
                        transform, a, b, instance);
                }
                instanceSegments = cadSoftwareWorkAdd(
                    instanceSegments, segmentCount);
            };
            if (wire.hasAdaptiveProgressiveClusters()) {
                for (const Obol::ProgressiveWireCluster& cluster :
                        wire.progressiveClusters) {
                    if (cadSoftwareBoxOutsideClip(cluster.bounds, transform))
                        continue;
                    for (const Obol::ProgressiveWireClusterRange& range :
                            cluster.ranges) {
                        if (range.activationCut > level) break;
                        drawRange(range.firstSegment, range.segmentCount);
                    }
                }
            } else {
                drawRange(wire.segmentFirstAtCut(level),
                    wire.segmentCountAtCut(level));
            }
            for (const auto& polyline : wire.polylines) {
                for (size_t p = 1; p < polyline.points.size(); ++p) {
                    cadSoftwareSegment(pixels, width, height, origin, size,
                    transform, polyline.points[p - 1], polyline.points[p],
                        instance);
                    instanceSegments = cadSoftwareWorkAdd(
                        instanceSegments, 1);
                }
            }
            if (instanceSegments) {
                result.work.lineCount = cadSoftwareWorkAdd(
                    result.work.lineCount, instanceSegments);
                result.work.positionCount = cadSoftwareWorkAdd(
                    result.work.positionCount,
                    instanceSegments > UINT64_MAX / 2 ? UINT64_MAX :
                        instanceSegments * 2);
                result.work.occurrenceCount = cadSoftwareWorkAdd(
                    result.work.occurrenceCount, 1);
            }
        }
    }
    uint64_t submittedProxyPoints = 0;
    uint64_t submittedProxyLines = 0;
    for (const auto& point : subpixelProxyPoints) {
        if (point.shape ==
                Obol::internal::CadAggregateProxyShape::Box) {
            cadSoftwareProxyBox(
                pixels, width, height, origin, size, viewProj, point);
            if (!(point.flags & Obol::internal::CadInstanceHidden))
                submittedProxyLines = cadSoftwareWorkAdd(
                    submittedProxyLines,
                    Obol::CadAggregateProxyBoxLineCount);
        } else {
            cadSoftwarePoint(
                pixels, width, height, origin, size, viewProj, point);
        }
        if (point.shape ==
                    Obol::internal::CadAggregateProxyShape::Point &&
                !(point.flags & Obol::internal::CadInstanceHidden))
            submittedProxyPoints = cadSoftwareWorkAdd(
                submittedProxyPoints, 1);
    }
    /* These primitives are submitted by one aggregate raster channel.
     * Record their vertex work, but do not manufacture per-occurrence draw
     * cost. */
    result.work.positionCount = cadSoftwareWorkAdd(
        result.work.positionCount, cadSoftwareWorkAdd(
            submittedProxyPoints,
            submittedProxyLines > UINT64_MAX / 2 ? UINT64_MAX :
                submittedProxyLines * 2));
    result.work.lineCount = cadSoftwareWorkAdd(
        result.work.lineCount, submittedProxyLines);
    result.subpixelProxyDrawPointCount = submittedProxyPoints >
        static_cast<uint64_t>((std::numeric_limits<size_t>::max)()) ?
        (std::numeric_limits<size_t>::max)() :
        static_cast<size_t>(submittedProxyPoints);
    return result;
}
