/**************************************************************************\
 * Copyright (c) 2026
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
\**************************************************************************/

#include <Obol/cad/CadProjectedProxy.h>
#include <Obol/cad/SoCADAssembly.h>

#include <algorithm>
#include <cmath>

namespace {

struct ClipPoint {
    double v[4];
};

static ClipPoint
transformPoint(const SbMatrix& matrix, const SbVec3f& point)
{
    const float *m = matrix[0];
    ClipPoint result;
    for (int column = 0; column < 4; ++column) {
        result.v[column] = static_cast<double>(point[0]) * m[column] +
            static_cast<double>(point[1]) * m[4 + column] +
            static_cast<double>(point[2]) * m[8 + column] + m[12 + column];
    }
    return result;
}

static double
planeValue(const ClipPoint& point, int plane)
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

} // namespace

bool
Obol::cadPartGeometryProxyCorners(const Obol::PartGeometry& geometry,
                                  SbVec3f corners[8])
{
    if (!corners)
        return false;

    SbBox3f bounds;
    bounds.makeEmpty();
    if (geometry.conservativeBounds &&
            !geometry.conservativeBounds->isEmpty())
        bounds.extendBy(*geometry.conservativeBounds);
    if (geometry.shaded && !geometry.shaded->bounds.isEmpty())
        bounds.extendBy(geometry.shaded->bounds);
    if (geometry.points && !geometry.points->bounds.isEmpty())
        bounds.extendBy(geometry.points->bounds);
    if (!bounds.isEmpty()) {
        const SbVec3f minimum = bounds.getMin();
        const SbVec3f maximum = bounds.getMax();
        size_t index = 0;
        for (int z = 0; z < 2; ++z)
            for (int y = 0; y < 2; ++y)
                for (int x = 0; x < 2; ++x)
                    corners[index++] = SbVec3f(
                        x ? maximum[0] : minimum[0],
                        y ? maximum[1] : minimum[1],
                        z ? maximum[2] : minimum[2]);
        return true;
    }

    if (!geometry.wire ||
            geometry.wire->segmentPoints.size() != 24u ||
            !geometry.wire->polylines.empty())
        return false;
    /* A valid AABB wire may be planar, linear, or point-like.  Such boxes
     * have four, two, or one distinct endpoints rather than eight.  Validate
     * the stronger invariant that every endpoint is on an extremum in each
     * axis, then publish the ordinary eight AABB corners (with duplicates in
     * degenerate axes).  Merely accepting fewer unique endpoints would also
     * accept arbitrary twelve-segment wire geometry; checking per-axis
     * extrema preserves the structural-proxy contract. */
    SbBox3f wireBounds;
    wireBounds.makeEmpty();
    for (const SbVec3f& candidate : geometry.wire->segmentPoints) {
        wireBounds.extendBy(candidate);
    }
    if (wireBounds.isEmpty())
        return false;
    const SbVec3f minimum = wireBounds.getMin();
    const SbVec3f maximum = wireBounds.getMax();
    for (const SbVec3f& candidate : geometry.wire->segmentPoints) {
        for (int axis = 0; axis < 3; ++axis) {
            if (candidate[axis] != minimum[axis] &&
                    candidate[axis] != maximum[axis])
                return false;
        }
    }
    size_t index = 0;
    for (int z = 0; z < 2; ++z)
        for (int y = 0; y < 2; ++y)
            for (int x = 0; x < 2; ++x)
                corners[index++] = SbVec3f(
                    x ? maximum[0] : minimum[0],
                    y ? maximum[1] : minimum[1],
                    z ? maximum[2] : minimum[2]);
    return true;
}

Obol::CadProjectedProxy
Obol::classifyCadProjectedProxy(const SbVec3f corners[8],
                                const SbMatrix& localToWorld,
                                const SbMatrix& viewProjection,
                                const SbVec2s& viewportSize,
                                float pixelLimit)
{
    Obol::CadProjectedProxy result;
    if (!corners || viewportSize[0] <= 1 || viewportSize[1] <= 1 ||
            !std::isfinite(pixelLimit) || pixelLimit <= 0.0f)
        return result;

    SbMatrix modelViewProjection = localToWorld;
    modelViewProjection.multRight(viewProjection);

    double minimumX = 0.0;
    double minimumY = 0.0;
    double maximumX = 0.0;
    double maximumY = 0.0;
    double nearestDepth = 0.0;
    bool first = true;
    bool allOutside[6] = { true, true, true, true, true, true };
    result.fullyContained = true;
    for (int corner = 0; corner < 8; ++corner) {
        const ClipPoint clip = transformPoint(modelViewProjection,
            corners[corner]);
        if (std::abs(clip.v[3]) < 1.0e-20) {
            result.fullyContained = false;
            continue;
        }
        bool inside = true;
        for (int plane = 0; plane < 6; ++plane) {
            const bool planeInside = planeValue(clip, plane) >= 0.0;
            allOutside[plane] = allOutside[plane] && !planeInside;
            if (!planeInside) {
                inside = false;
                result.fullyContained = false;
            }
        }
        if (!inside)
            continue;

        const double x = clip.v[0] / clip.v[3];
        const double y = clip.v[1] / clip.v[3];
        const double z = clip.v[2] / clip.v[3];
        if (first) {
            minimumX = maximumX = x;
            minimumY = maximumY = y;
            nearestDepth = z;
            localToWorld.multVecMatrix(corners[corner], result.point);
            first = false;
        } else {
            minimumX = std::min(minimumX, x);
            maximumX = std::max(maximumX, x);
            minimumY = std::min(minimumY, y);
            maximumY = std::max(maximumY, y);
            if (z < nearestDepth) {
                nearestDepth = z;
                localToWorld.multVecMatrix(corners[corner], result.point);
            }
        }
    }

    /* The renderer's point path is intentionally stricter than visibility:
     * all corners must be projectable and inside.  A partly clipped proxy is
     * visible through its intersecting edges, but is never one-point safe. */
    result.visible = true;
    for (int plane = 0; plane < 6; ++plane)
        if (allOutside[plane])
            result.visible = false;
    if (!result.fullyContained || first)
        return result;

    result.pixelWidth = static_cast<float>(
        (maximumX - minimumX) * 0.5 * (viewportSize[0] - 1));
    result.pixelHeight = static_cast<float>(
        (maximumY - minimumY) * 0.5 * (viewportSize[1] - 1));
    result.pointEligible = result.pixelWidth <= pixelLimit &&
        result.pixelHeight <= pixelLimit;
    return result;
}
