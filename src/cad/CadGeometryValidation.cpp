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

#include <Obol/cad/CadGeometryValidation.h>
#include <Obol/cad/CadProjectedProxy.h>
#include <Obol/cad/CadSceneRecords.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

using Obol::CadGeometryError;
using Obol::CadGeometryValidation;

CadGeometryValidation
failure(CadGeometryError error, size_t element) noexcept
{
    CadGeometryValidation result;
    result.error = error;
    result.elementIndex = element;
    return result;
}

bool
finite(float value) noexcept
{
    return std::isfinite(value);
}

bool
finite(const SbVec3f& value) noexcept
{
    return finite(value[0]) && finite(value[1]) && finite(value[2]);
}

bool
finite(const SbColor& value) noexcept
{
    return finite(value[0]) && finite(value[1]) && finite(value[2]);
}

bool
finite(const SbBox3f& bounds) noexcept
{
    return bounds.isEmpty() ||
        (finite(bounds.getMin()) && finite(bounds.getMax()));
}

bool
contains(const SbBox3f& bounds, const SbVec3f& point) noexcept
{
    if (bounds.isEmpty())
        return false;
    const SbVec3f& minimum = bounds.getMin();
    const SbVec3f& maximum = bounds.getMax();
    return point[0] >= minimum[0] && point[0] <= maximum[0] &&
        point[1] >= minimum[1] && point[1] <= maximum[1] &&
        point[2] >= minimum[2] && point[2] <= maximum[2];
}

bool
contains(const SbBox3f& outer, const SbBox3f& inner) noexcept
{
    return inner.isEmpty() ||
        (contains(outer, inner.getMin()) && contains(outer, inner.getMax()));
}

bool
containsWithTolerance(const SbBox3f& outer, const SbBox3f& inner) noexcept
{
    if (inner.isEmpty())
        return true;
    if (outer.isEmpty())
        return false;
    const SbVec3f outerMinimum = outer.getMin();
    const SbVec3f outerMaximum = outer.getMax();
    const SbVec3f innerMinimum = inner.getMin();
    const SbVec3f innerMaximum = inner.getMax();
    for (int axis = 0; axis < 3; ++axis) {
        const float scale = std::max(1.0f,
            std::max(std::abs(outerMinimum[axis]),
                std::max(std::abs(outerMaximum[axis]),
                    outerMaximum[axis] - outerMinimum[axis])));
        const float tolerance = 64.0f *
            std::numeric_limits<float>::epsilon() * scale;
        if (innerMinimum[axis] < outerMinimum[axis] - tolerance ||
                innerMaximum[axis] > outerMaximum[axis] + tolerance)
            return false;
    }
    return true;
}

template <typename Geometry>
SbBox3f
geometryBounds(const Geometry& geometry) noexcept
{
    SbBox3f bounds;
    bounds.makeEmpty();
    if (geometry.conservativeBounds)
        bounds.extendBy(*geometry.conservativeBounds);
    if (geometry.points)
        bounds.extendBy(geometry.points->bounds);
    if (geometry.wire)
        bounds.extendBy(geometry.wire->bounds);
    if (geometry.shaded)
        bounds.extendBy(geometry.shaded->bounds);
    return bounds;
}

template <typename Geometry>
bool
validAggregateProxy(const Geometry& geometry) noexcept
{
    if (!geometry.aggregateProxyCorners)
        return true;
    const auto& corners = *geometry.aggregateProxyCorners;
    SbBox3f proxyBounds;
    proxyBounds.makeEmpty();
    for (const SbVec3f& corner : corners) {
        if (!finite(corner))
            return false;
        proxyBounds.extendBy(corner);
    }
    if (!containsWithTolerance(proxyBounds, geometryBounds(geometry)))
        return false;

    const SbVec3f axes[3] = {
        corners[1] - corners[0],
        corners[2] - corners[0],
        corners[4] - corners[0]
    };
    const float scale = std::max(1.0f,
        std::max(axes[0].length(),
            std::max(axes[1].length(), axes[2].length())));
    const float tolerance = 128.0f *
        std::numeric_limits<float>::epsilon() * scale;
    for (size_t corner = 0; corner < corners.size(); ++corner) {
        SbVec3f expected = corners[0];
        for (size_t axis = 0; axis < 3; ++axis)
            if (corner & (1u << axis))
                expected += axes[axis];
        if ((corners[corner] - expected).length() > tolerance)
            return false;
    }
    for (size_t left = 0; left < 3; ++left)
        for (size_t right = left + 1; right < 3; ++right)
            if (std::abs(axes[left].dot(axes[right])) >
                    tolerance * scale)
                return false;
    return true;
}

bool
validQuantization(Obol::ProgressiveQuantization quantization) noexcept
{
    return quantization.xBits <= 16 && quantization.yBits <= 16 &&
        quantization.zBits <= 16;
}

CadGeometryValidation
validateMesh(const Obol::TriMesh& mesh) noexcept
{
    if (!finite(mesh.bounds))
        return failure(CadGeometryError::NonFiniteValue, 0);

    for (size_t i = 0; i < mesh.positions.size(); ++i) {
        if (!finite(mesh.positions[i]))
            return failure(CadGeometryError::NonFiniteValue, i);
        if (!contains(mesh.bounds, mesh.positions[i]))
            return failure(CadGeometryError::NonConservativeBounds, i);
    }
    if (!mesh.normals.empty() && mesh.normals.size() != mesh.positions.size())
        return failure(CadGeometryError::InvalidAttributeCount,
            mesh.normals.size());
    for (size_t i = 0; i < mesh.normals.size(); ++i)
        if (!finite(mesh.normals[i]))
            return failure(CadGeometryError::NonFiniteValue, i);
    if (mesh.indices.size() % 3u != 0u)
        return failure(CadGeometryError::InvalidPrimitiveCount,
            mesh.indices.size());
    for (size_t i = 0; i < mesh.indices.size(); ++i)
        if (mesh.indices[i] >= mesh.positions.size())
            return failure(CadGeometryError::InvalidVertexIndex, i);

    if (mesh.progressiveCuts.empty()) {
        if (mesh.progressiveMinimumCut != Obol::ProgressiveCutUnspecified ||
                mesh.progressiveResidentCut !=
                    Obol::ProgressiveCutUnspecified ||
                !mesh.progressiveClusters.empty() ||
                mesh.progressiveClusterGridResolution != 0)
            return failure(CadGeometryError::InvalidProgressiveInterval, 0);
        return CadGeometryValidation();
    }
    if (!finite(mesh.progressiveQuantizationMinimum) ||
            !finite(mesh.progressiveQuantizationMaximum))
        return failure(CadGeometryError::NonFiniteValue, 0);
    if (!mesh.isProgressive())
        return failure(CadGeometryError::InvalidProgressiveInterval, 0);

    size_t previousIndexCount = 0;
    size_t previousPositionCount = 0;
    uint32_t maximumIndex = 0;
    size_t scanned = 0;
    for (size_t i = 0; i < mesh.progressiveCuts.size(); ++i) {
        const Obol::ProgressiveTriangleCut& cut = mesh.progressiveCuts[i];
        if (cut.indexCount % 3u != 0u ||
                !validQuantization(cut.quantization))
            return failure(CadGeometryError::InvalidProgressiveCut, i);
        if (cut.indexCount < previousIndexCount ||
                cut.positionCount < previousPositionCount)
            return failure(CadGeometryError::InvalidProgressiveOrder, i);
        if (i <= mesh.progressiveResidentCut) {
            if (cut.indexCount > mesh.indices.size() ||
                    cut.positionCount > mesh.positions.size())
                return failure(CadGeometryError::InvalidProgressiveCut, i);
            for (; scanned < cut.indexCount; ++scanned)
                maximumIndex = (std::max)(
                    maximumIndex, mesh.indices[scanned]);
            if (cut.indexCount && maximumIndex >= cut.positionCount)
                return failure(CadGeometryError::InvalidProgressiveCut, i);
        }
        previousIndexCount = cut.indexCount;
        previousPositionCount = cut.positionCount;
    }

    const size_t side = mesh.progressiveClusterGridResolution;
    if (side != 0 && (side <= 1 || side > 64 ||
            mesh.progressiveClusters.size() > side * side * side))
        return failure(CadGeometryError::InvalidClusterLayout, side);
    for (size_t clusterIndex = 0;
            clusterIndex < mesh.progressiveClusters.size(); ++clusterIndex) {
        const Obol::ProgressiveTriangleCluster& cluster =
            mesh.progressiveClusters[clusterIndex];
        if (!finite(cluster.bounds) ||
                cluster.residentCut >= mesh.progressiveCuts.size())
            return failure(CadGeometryError::InvalidClusterLayout,
                clusterIndex);
        uint8_t priorActivation = 0;
        bool haveActivation = false;
        for (const Obol::ProgressiveTriangleClusterRange& range :
                cluster.ranges) {
            const uint64_t end = static_cast<uint64_t>(range.firstIndex) +
                static_cast<uint64_t>(range.indexCount);
            if (range.firstIndex % 3u != 0u ||
                    range.indexCount % 3u != 0u ||
                    range.activationCut >= mesh.progressiveCuts.size())
                return failure(CadGeometryError::InvalidClusterRange,
                    clusterIndex);
            const Obol::ProgressiveTriangleCut& activation =
                mesh.progressiveCuts[range.activationCut];
            if (end > activation.indexCount)
                return failure(CadGeometryError::InvalidClusterRange,
                    clusterIndex);
            const uint8_t residentCut = (std::min)(
                mesh.progressiveResidentCut, cluster.residentCut);
            if (range.activationCut <= residentCut &&
                    end > mesh.indices.size())
                return failure(CadGeometryError::InvalidClusterRange,
                    clusterIndex);
            if (haveActivation && range.activationCut < priorActivation)
                return failure(CadGeometryError::InvalidProgressiveOrder,
                    clusterIndex);
            priorActivation = range.activationCut;
            haveActivation = true;
        }
    }
    return CadGeometryValidation();
}

CadGeometryValidation
validateWire(const Obol::WireRep& wire) noexcept
{
    if (!finite(wire.bounds))
        return failure(CadGeometryError::NonFiniteValue, 0);
    if (wire.segmentPoints.size() % 2u != 0u)
        return failure(CadGeometryError::InvalidPrimitiveCount,
            wire.segmentPoints.size());
    const Obol::TriMesh *triangleEdges = wire.triangleEdges();
    if (triangleEdges &&
            (!wire.segmentPoints.empty() || !wire.polylines.empty()))
        return failure(CadGeometryError::InvalidAttributeCount, 0);
    if (triangleEdges) {
        const CadGeometryValidation meshResult =
            validateMesh(*triangleEdges);
        if (!meshResult)
            return meshResult;
        if (wire.triangleEdgeSegmentCount !=
                triangleEdges->indices.size())
            return failure(CadGeometryError::InvalidAttributeCount,
                wire.triangleEdgeSegmentCount);
        if (!contains(wire.bounds, triangleEdges->bounds))
            return failure(CadGeometryError::NonConservativeBounds, 0);
    }
    for (size_t i = 0; i < wire.segmentPoints.size(); ++i) {
        if (!finite(wire.segmentPoints[i]))
            return failure(CadGeometryError::NonFiniteValue, i);
        if (!contains(wire.bounds, wire.segmentPoints[i]))
            return failure(CadGeometryError::NonConservativeBounds, i);
    }
    if (!wire.segmentIds.empty() &&
            wire.segmentIds.size() != wire.segmentCount())
        return failure(CadGeometryError::InvalidAttributeCount,
            wire.segmentIds.size());
    for (size_t polylineIndex = 0;
            polylineIndex < wire.polylines.size(); ++polylineIndex)
        for (const SbVec3f& point : wire.polylines[polylineIndex].points) {
            if (!finite(point))
                return failure(CadGeometryError::NonFiniteValue,
                    polylineIndex);
            if (!contains(wire.bounds, point))
                return failure(CadGeometryError::NonConservativeBounds,
                    polylineIndex);
        }

    if (wire.progressiveCuts.empty()) {
        if (wire.progressiveMinimumCut != Obol::ProgressiveCutUnspecified ||
                wire.progressiveResidentCut !=
                    Obol::ProgressiveCutUnspecified ||
                !wire.progressiveClusters.empty() ||
                wire.progressiveClusterGridResolution != 0)
            return failure(CadGeometryError::InvalidProgressiveInterval, 0);
        return CadGeometryValidation();
    }
    if (!finite(wire.progressiveQuantizationMinimum) ||
            !finite(wire.progressiveQuantizationMaximum))
        return failure(CadGeometryError::NonFiniteValue, 0);
    if (!wire.isProgressive())
        return failure(CadGeometryError::InvalidProgressiveInterval, 0);
    size_t previousEnd = 0;
    float previousError = std::numeric_limits<float>::infinity();
    for (size_t i = 0; i < wire.progressiveCuts.size(); ++i) {
        const Obol::ProgressiveWireCut& cut = wire.progressiveCuts[i];
        const uint64_t end = static_cast<uint64_t>(cut.segmentFirst) +
            static_cast<uint64_t>(cut.segmentCount);
        if (!validQuantization(cut.quantization) ||
                !std::isfinite(cut.maximumNormalizedError) ||
                cut.maximumNormalizedError < -1.0f)
            return failure(CadGeometryError::InvalidProgressiveCut, i);
        if (i <= wire.progressiveResidentCut &&
                end > wire.segmentCount())
            return failure(CadGeometryError::InvalidProgressiveCut, i);
        if (cut.segmentFirst == 0 && end < previousEnd)
            return failure(CadGeometryError::InvalidProgressiveOrder, i);
        if (cut.maximumNormalizedError >= 0.0f) {
            if (cut.maximumNormalizedError > previousError)
                return failure(CadGeometryError::InvalidProgressiveOrder, i);
            previousError = cut.maximumNormalizedError;
        } else if (previousError !=
                std::numeric_limits<float>::infinity()) {
            /* A partly certified interval is ambiguous to a view planner. */
            return failure(CadGeometryError::InvalidProgressiveOrder, i);
        }
        previousEnd = static_cast<size_t>(end);
    }

    const size_t side = wire.progressiveClusterGridResolution;
    if (side != 0 && (side <= 1 || side > 64 ||
            wire.progressiveClusters.size() > side * side * side))
        return failure(CadGeometryError::InvalidClusterLayout, side);
    for (size_t clusterIndex = 0;
            clusterIndex < wire.progressiveClusters.size(); ++clusterIndex) {
        const Obol::ProgressiveWireCluster& cluster =
            wire.progressiveClusters[clusterIndex];
        if (!finite(cluster.bounds) ||
                cluster.residentCut >= wire.progressiveCuts.size())
            return failure(CadGeometryError::InvalidClusterLayout,
                clusterIndex);
        uint8_t priorActivation = 0;
        bool haveActivation = false;
        for (const Obol::ProgressiveWireClusterRange& range :
                cluster.ranges) {
            const uint64_t end = static_cast<uint64_t>(range.firstSegment) +
                static_cast<uint64_t>(range.segmentCount);
            if (range.activationCut >= wire.progressiveCuts.size())
                return failure(CadGeometryError::InvalidClusterRange,
                    clusterIndex);
            const Obol::ProgressiveWireCut& activation =
                wire.progressiveCuts[range.activationCut];
            const uint64_t activationEnd =
                static_cast<uint64_t>(activation.segmentFirst) +
                static_cast<uint64_t>(activation.segmentCount);
            if (range.firstSegment < activation.segmentFirst ||
                    end > activationEnd)
                return failure(CadGeometryError::InvalidClusterRange,
                    clusterIndex);
            const uint8_t residentCut = (std::min)(
                wire.progressiveResidentCut, cluster.residentCut);
            if (range.activationCut <= residentCut &&
                    end > wire.segmentCount())
                return failure(CadGeometryError::InvalidClusterRange,
                    clusterIndex);
            if (haveActivation && range.activationCut < priorActivation)
                return failure(CadGeometryError::InvalidProgressiveOrder,
                    clusterIndex);
            priorActivation = range.activationCut;
            haveActivation = true;
        }
    }
    return CadGeometryValidation();
}

CadGeometryValidation
validatePoints(const Obol::PointRep& points) noexcept
{
    const size_t count = points.positions.size();
    if (!finite(points.bounds))
        return failure(CadGeometryError::NonFiniteValue, 0);
    const auto parallel = [count](size_t size) {
        return size == 0 || size == count;
    };
    if (!parallel(points.pointIds.size()) ||
            !parallel(points.colorValid.size()) ||
            !parallel(points.colors.size()) ||
            !parallel(points.scaleValid.size()) ||
            !parallel(points.scales.size()) ||
            !parallel(points.normalValid.size()) ||
            !parallel(points.normals.size()))
        return failure(CadGeometryError::InvalidAttributeCount, count);
    for (size_t i = 0; i < count; ++i) {
        if (!finite(points.positions[i]))
            return failure(CadGeometryError::NonFiniteValue, i);
        if (!contains(points.bounds, points.positions[i]))
            return failure(CadGeometryError::NonConservativeBounds, i);
    }
    for (size_t i = 0; i < points.colors.size(); ++i)
        if (!finite(points.colors[i]))
            return failure(CadGeometryError::NonFiniteValue, i);
    for (size_t i = 0; i < points.scales.size(); ++i)
        if (!finite(points.scales[i]) || points.scales[i] < 0.0f)
            return failure(CadGeometryError::NonFiniteValue, i);
    for (size_t i = 0; i < points.normals.size(); ++i)
        if (!finite(points.normals[i]))
            return failure(CadGeometryError::NonFiniteValue, i);
    return CadGeometryValidation();
}

} // namespace

namespace Obol {

PartGeometry::PartGeometry(PartGeometryBuilder&& builder) noexcept :
    points(std::move(builder.points)),
    wire(std::move(builder.wire)),
    shaded(std::move(builder.shaded)),
    conservativeBounds(std::move(builder.conservativeBounds)),
    aggregateProxyCorners(std::move(builder.aggregateProxyCorners)),
    shadedCullBackfaces(builder.shadedCullBackfaces),
    subpixelProxyEligible(builder.subpixelProxyEligible),
    structuralProxy(builder.structuralProxy)
{
}

template <typename Geometry>
CadGeometryValidation
validatePartGeometry(const Geometry& geometry) noexcept
{
    if (geometry.conservativeBounds &&
            !finite(*geometry.conservativeBounds))
        return failure(CadGeometryError::NonFiniteValue, 0);
    if (geometry.points) {
        const CadGeometryValidation result = validatePoints(*geometry.points);
        if (!result)
            return result;
    }
    if (geometry.wire) {
        const CadGeometryValidation result = validateWire(*geometry.wire);
        if (!result)
            return result;
    }
    if (geometry.shaded) {
        const CadGeometryValidation result = validateMesh(*geometry.shaded);
        if (!result)
            return result;
    }
    if (geometry.conservativeBounds) {
        const SbBox3f& bounds = *geometry.conservativeBounds;
        if ((geometry.points && !contains(bounds, geometry.points->bounds)) ||
                (geometry.wire && !contains(bounds, geometry.wire->bounds)) ||
                (geometry.shaded && !contains(bounds,
                    geometry.shaded->bounds)))
            return failure(CadGeometryError::NonConservativeBounds, 0);
    }
    if (!validAggregateProxy(geometry))
        return failure(CadGeometryError::InvalidAggregateProxy, 0);
    if (geometry.subpixelProxyEligible) {
        SbVec3f corners[8];
        if (!cadPartGeometryProxyCorners(geometry, corners))
            return failure(CadGeometryError::InvalidSubpixelProxy, 0);
    }
    return CadGeometryValidation();
}

CadGeometryValidation
cadValidatePartGeometry(const PartGeometryBuilder& geometry) noexcept
{
    return validatePartGeometry(geometry);
}

CadGeometryValidation
cadValidatePartUpdates(
    const std::vector<PartUpdate>& updates) noexcept
{
    for (size_t i = 0; i < updates.size(); ++i) {
        CadGeometryValidation validation;
        if (!updates[i].part.isValid())
            validation.error = CadGeometryError::InvalidPartId;
        else if (!updates[i].geometry)
            validation.error = CadGeometryError::NullGeometry;
        if (!validation) {
            validation.updateIndex = i;
            return validation;
        }
    }
    return CadGeometryValidation();
}

CadGeometryAdmission
cadAdmitPartGeometry(PartGeometryBuilder geometry)
{
    CadGeometryAdmission admission;
    admission.validation = cadValidatePartGeometry(geometry);
    if (!admission.validation)
        return admission;
    std::shared_ptr<const PartGeometry> snapshot(
        new PartGeometry(std::move(geometry)));
    admission.geometry = ValidatedPartGeometry(std::move(snapshot));
    return admission;
}

const char *
cadGeometryErrorName(CadGeometryError error) noexcept
{
    switch (error) {
        case CadGeometryError::Valid: return "valid";
        case CadGeometryError::InvalidPartId: return "invalid-part-id";
        case CadGeometryError::NullGeometry: return "null-geometry";
        case CadGeometryError::NonFiniteValue: return "non-finite-value";
        case CadGeometryError::InvalidAttributeCount:
            return "invalid-attribute-count";
        case CadGeometryError::InvalidPrimitiveCount:
            return "invalid-primitive-count";
        case CadGeometryError::InvalidVertexIndex:
            return "invalid-vertex-index";
        case CadGeometryError::InvalidProgressiveInterval:
            return "invalid-progressive-interval";
        case CadGeometryError::InvalidProgressiveCut:
            return "invalid-progressive-cut";
        case CadGeometryError::InvalidProgressiveOrder:
            return "invalid-progressive-order";
        case CadGeometryError::InvalidClusterLayout:
            return "invalid-cluster-layout";
        case CadGeometryError::InvalidClusterRange:
            return "invalid-cluster-range";
        case CadGeometryError::NonConservativeBounds:
            return "non-conservative-bounds";
        case CadGeometryError::InvalidSubpixelProxy:
            return "invalid-subpixel-proxy";
        case CadGeometryError::InvalidAggregateProxy:
            return "invalid-aggregate-proxy";
    }
    return "unknown";
}

} // namespace Obol
