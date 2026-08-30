#ifndef OBOL_CAD_GEOMETRY_H
#define OBOL_CAD_GEOMETRY_H

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
 * @file CadGeometry.h
 * @brief Immutable renderer geometry and progressive spatial metadata.
 */

#include <Inventor/SbBox3f.h>
#include <Inventor/SbColor.h>
#include <Inventor/SbVec3f.h>

#include <Obol/cad/CadProgressive.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace Obol {

struct TriMesh;
class PartGeometry;
struct CadGeometryAdmission;

struct ProgressiveWireCut {
    uint32_t segmentFirst = 0;
    uint32_t segmentCount = 0;
    ProgressiveQuantization quantization;
    /**
     * Producer-certified maximum geometric deviation divided by the
     * complete part-bounds diagonal.  A negative value means that the
     * producer did not supply an error bound.  Consumers must not infer
     * geometric quality from a cut ordinal.
     */
    float maximumNormalizedError = -1.0f;
};

struct ProgressiveTriangleCut {
    uint32_t indexCount = 0;
    uint32_t positionCount = 0;
    ProgressiveQuantization quantization;
};

struct ProgressiveTriangleClusterRange {
    uint32_t firstIndex = 0;
    uint32_t indexCount = 0;
    /* The complete range must be present in this cut's index/position
     * prefixes.  Once activated, a range remains active at richer cuts. */
    uint8_t activationCut = ProgressiveCutUnspecified;
};

/** Private spatial page of a logical triangle part, without CAD identity. */
struct ProgressiveTriangleCluster {
    /* Conservative bounds for every currently resident primitive referenced
     * by ranges in this cluster. */
    SbBox3f bounds;
    std::vector<ProgressiveTriangleClusterRange> ranges;
    uint8_t residentCut = ProgressiveCutUnspecified;
};

struct ProgressiveWireClusterRange {
    uint32_t firstSegment = 0;
    uint32_t segmentCount = 0;
    /* The complete range must be present in the selected segment interval at
     * this cut.  Once activated, a range remains active at richer cuts. */
    uint8_t activationCut = ProgressiveCutUnspecified;
};

/** Private spatial page of a logical wire part, without CAD identity. */
struct ProgressiveWireCluster {
    /* Conservative bounds for every currently resident segment referenced
     * by ranges in this cluster. */
    SbBox3f bounds;
    std::vector<ProgressiveWireClusterRange> ranges;
    uint8_t residentCut = ProgressiveCutUnspecified;
};

struct WirePolyline {
    std::vector<SbVec3f> points;
    uint32_t edgeId = 0;
};

struct WireRep {
    std::vector<SbVec3f> segmentPoints;

    /* Optional admitted shaded snapshot whose indexed triangles define this
     * wire channel without duplicating six positions per triangle.  It is
     * mutually exclusive with segments and polylines.  Referencing the owning
     * snapshot, rather than a bare TriMesh, prevents a mutable mesh alias. */
    std::shared_ptr<const PartGeometry> triangleEdgeGeometry;
    size_t triangleEdgeSegmentCount = 0;
    std::vector<uint32_t> segmentIds;
    std::vector<WirePolyline> polylines;
    /* Must conservatively contain every resident point in this channel. */
    SbBox3f bounds;

    std::vector<ProgressiveWireCut> progressiveCuts;
    uint8_t progressiveMinimumCut = ProgressiveCutUnspecified;
    uint8_t progressiveResidentCut = ProgressiveCutUnspecified;
    /* Ordered domain containing every stored coordinate on an axis for which
     * a resident cut requests lossy (1..15 bit) quantization. */
    SbVec3f progressiveQuantizationMinimum = SbVec3f(0.0f, 0.0f, 0.0f);
    SbVec3f progressiveQuantizationMaximum = SbVec3f(0.0f, 0.0f, 0.0f);

    /* A nonzero lineage certifies an append-only segment prefix across
     * immutable generations.  Independent approximations must leave it 0. */
    uint64_t progressiveLineage = 0;
    std::vector<ProgressiveWireCluster> progressiveClusters;
    uint16_t progressiveClusterGridResolution = 0;

    const TriMesh *triangleEdges() const noexcept;
    size_t segmentCount() const noexcept;
    bool derivesTriangleEdges() const noexcept;

    bool isProgressive() const noexcept
    {
        return !progressiveCuts.empty() &&
            progressiveCuts.size() <= ProgressiveCutLimit &&
            progressiveMinimumCut < progressiveCuts.size() &&
            progressiveResidentCut >= progressiveMinimumCut &&
            progressiveResidentCut < progressiveCuts.size();
    }

    bool hasProgressiveClusters() const noexcept
    {
        const size_t side = progressiveClusterGridResolution;
        return isProgressive() && !progressiveClusters.empty() &&
            (side == 0 || (side > 1 && side <= 64 &&
             progressiveClusters.size() <= side * side * side));
    }

    bool hasAdaptiveProgressiveClusters() const noexcept
    {
        return hasProgressiveClusters() &&
            progressiveClusterGridResolution == 0;
    }

    size_t segmentCountAtCut(uint8_t cut) const noexcept
    {
        if (!isProgressive())
            return segmentCount();
        cut = (std::max)(progressiveMinimumCut,
            (std::min)(progressiveResidentCut, cut));
        const size_t first = (std::min<size_t>)(
            progressiveCuts[cut].segmentFirst, segmentCount());
        return (std::min<size_t>)(progressiveCuts[cut].segmentCount,
            segmentCount() - first);
    }

    size_t segmentFirstAtCut(uint8_t cut) const noexcept
    {
        if (!isProgressive())
            return 0;
        cut = (std::max)(progressiveMinimumCut,
            (std::min)(progressiveResidentCut, cut));
        return (std::min<size_t>)(progressiveCuts[cut].segmentFirst,
            segmentCount());
    }

    ProgressiveQuantization quantizationAtCut(uint8_t cut) const noexcept
    {
        if (!isProgressive())
            return ProgressiveQuantization();
        cut = (std::max)(progressiveMinimumCut,
            (std::min)(progressiveResidentCut, cut));
        return progressiveCuts[cut].quantization;
    }

    bool hasProgressiveErrorBounds() const noexcept
    {
        if (!isProgressive())
            return false;
        for (size_t cut = progressiveMinimumCut;
                cut <= progressiveResidentCut; ++cut)
            if (progressiveCuts[cut].maximumNormalizedError < 0.0f)
                return false;
        return true;
    }

    float normalizedErrorAtCut(uint8_t cut) const noexcept
    {
        if (!hasProgressiveErrorBounds())
            return -1.0f;
        cut = (std::max)(progressiveMinimumCut,
            (std::min)(progressiveResidentCut, cut));
        return progressiveCuts[cut].maximumNormalizedError;
    }
};

struct TriMesh {
    std::vector<SbVec3f> positions;
    std::vector<SbVec3f> normals;
    std::vector<uint32_t> indices;
    /* Must conservatively contain every resident position. */
    SbBox3f bounds;

    std::vector<ProgressiveTriangleCut> progressiveCuts;
    uint8_t progressiveMinimumCut = ProgressiveCutUnspecified;
    uint8_t progressiveResidentCut = ProgressiveCutUnspecified;
    /* Ordered domain containing every stored coordinate on an axis for which
     * a resident cut requests lossy (1..15 bit) quantization. */
    SbVec3f progressiveQuantizationMinimum = SbVec3f(0.0f, 0.0f, 0.0f);
    SbVec3f progressiveQuantizationMaximum = SbVec3f(0.0f, 0.0f, 0.0f);
    std::vector<ProgressiveTriangleCluster> progressiveClusters;
    uint16_t progressiveClusterGridResolution = 0;

    /* A nonzero lineage certifies identical append-only position, normal,
     * and index prefixes across immutable generations. */
    uint64_t progressiveLineage = 0;

    bool isProgressive() const noexcept
    {
        return !progressiveCuts.empty() &&
            progressiveCuts.size() <= ProgressiveCutLimit &&
            progressiveMinimumCut < progressiveCuts.size() &&
            progressiveResidentCut >= progressiveMinimumCut &&
            progressiveResidentCut < progressiveCuts.size();
    }

    bool hasProgressiveClusters() const noexcept
    {
        const size_t side = progressiveClusterGridResolution;
        return isProgressive() && !progressiveClusters.empty() &&
            (side == 0 || (side > 1 && side <= 64 &&
             progressiveClusters.size() <= side * side * side));
    }

    bool hasAdaptiveProgressiveClusters() const noexcept
    {
        return hasProgressiveClusters() &&
            progressiveClusterGridResolution == 0;
    }

    size_t indexCountAtCut(uint8_t cut) const noexcept
    {
        if (!isProgressive())
            return indices.size();
        cut = (std::max)(progressiveMinimumCut,
            (std::min)(progressiveResidentCut, cut));
        return std::min<size_t>(progressiveCuts[cut].indexCount,
            indices.size());
    }

    size_t positionCountAtCut(uint8_t cut) const noexcept
    {
        if (!isProgressive())
            return positions.size();
        cut = (std::max)(progressiveMinimumCut,
            (std::min)(progressiveResidentCut, cut));
        return std::min<size_t>(progressiveCuts[cut].positionCount,
            positions.size());
    }

    ProgressiveQuantization quantizationAtCut(uint8_t cut) const noexcept
    {
        if (!isProgressive())
            return ProgressiveQuantization();
        cut = (std::max)(progressiveMinimumCut,
            (std::min)(progressiveResidentCut, cut));
        return progressiveCuts[cut].quantization;
    }
};

struct PointRep {
    std::vector<SbVec3f> positions;
    std::vector<uint32_t> pointIds;
    std::vector<uint8_t> colorValid;
    std::vector<SbColor> colors;
    std::vector<uint8_t> scaleValid;
    std::vector<float> scales;
    std::vector<uint8_t> normalValid;
    std::vector<SbVec3f> normals;
    /* Must conservatively contain every position. */
    SbBox3f bounds;
};

/**
 * Mutable producer payload for one logical part.
 *
 * A missing channel is intentional.  conservativeBounds covers unresolved
 * or partially resident source geometry; an empty part without it has no
 * bounds and never invents a placeholder at the origin.
 */
struct PartGeometryBuilder {
    std::optional<PointRep> points;
    std::optional<WireRep> wire;
    std::optional<TriMesh> shaded;
    /* When present, this must contain every channel bound as well as any
     * unresolved source geometry it represents. */
    std::optional<SbBox3f> conservativeBounds;

    /* Optional producer-certified oriented bounds used only when this whole
     * part is represented by an aggregate proxy.  Corners use binary XYZ
     * order.  Geometry arrays and authored bounds remain authoritative for
     * ordinary drawing and picking. */
    std::optional<std::array<SbVec3f, 8>> aggregateProxyCorners;

    /* Back-face culling is legal only for producer-verified, closed,
     * consistently oriented shaded topology. */
    bool shadedCullBackfaces = false;

    /* Whole-occurrence aggregation may replace this presentation by one
     * depth-tested point while its complete conservative, shaded, point, or
     * wire bounds are subpixel. */
    bool subpixelProxyEligible = false;

    /* Explicitly distinguishes temporary structural bounds from authored
     * wire geometry with the same shape.  Structural proxies remain visible
     * in shaded modes.  Point-collapse eligibility is the independent flag
     * above; a whole-scene overview is deliberately structural but not
     * subpixel eligible. */
    bool structuralProxy = false;
};

/**
 * Renderer-visible geometry snapshot admitted from a mutable builder.
 *
 * Instances cannot be constructed or modified by clients.  Consequently a
 * shared PartGeometry pointer is a durable proof that the complete snapshot
 * was validated and that no producer retains a mutable alias to its owned
 * arrays.  Building and admission remain zero-copy when the builder is moved.
 */
class PartGeometry {
public:
    PartGeometry(const PartGeometry&) = delete;
    PartGeometry& operator=(const PartGeometry&) = delete;

    const std::optional<PointRep> points;
    const std::optional<WireRep> wire;
    const std::optional<TriMesh> shaded;
    const std::optional<SbBox3f> conservativeBounds;
    const std::optional<std::array<SbVec3f, 8>> aggregateProxyCorners;
    const bool shadedCullBackfaces;
    const bool subpixelProxyEligible;
    const bool structuralProxy;

private:
    explicit PartGeometry(PartGeometryBuilder&& builder) noexcept;

    friend OBOL_DLL_API CadGeometryAdmission cadAdmitPartGeometry(
        PartGeometryBuilder geometry);
};

inline const TriMesh *
WireRep::triangleEdges() const noexcept
{
    return triangleEdgeGeometry && triangleEdgeGeometry->shaded ?
        &*triangleEdgeGeometry->shaded : nullptr;
}

inline size_t
WireRep::segmentCount() const noexcept
{
    return triangleEdges() ? triangleEdgeSegmentCount :
        segmentPoints.size() / 2;
}

inline bool
WireRep::derivesTriangleEdges() const noexcept
{
    return triangleEdges() != nullptr;
}

} // namespace Obol

#endif // OBOL_CAD_GEOMETRY_H
