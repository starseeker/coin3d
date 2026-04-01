#ifndef OBOL_CAD_PICKING_CADPICKING_H
#define OBOL_CAD_PICKING_CADPICKING_H

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
 * @file CadPicking.h
 * @brief CPU-side BVH ray-picking for SoCADAssembly.
 *
 * Provides:
 *   - CadInstanceBVH  : AABB tree over world-space instance bounds for fast
 *                       candidate rejection.
 *   - CadPartEdgeBVH  : AABB tree over the segments of a single part's
 *                       WireRep, built lazily per part.
 *   - CadPickQuery    : executes a pick ray through these structures and
 *                       returns a CadPickResult.
 *
 * All structures are independent of BRL-CAD.
 */

#include <obol/cad/CadIds.h>
#include <obol/cad/SoCADAssembly.h>  // obol::PartGeometry, obol::InstanceRecord

#include <Inventor/SbVec3f.h>
#include <Inventor/SbBox3f.h>
#include <Inventor/SbLine.h>

#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>
#include <cstdint>
#include <cmath>

namespace obol {
namespace picking {

// ---------------------------------------------------------------------------
// Pick result
// ---------------------------------------------------------------------------

/**
 * @brief Result of a single ray pick against the CAD assembly.
 */
struct CadPickResult {
    InstanceId  instanceId;          ///< Picked instance
    PartId      partId;              ///< Part used by the picked instance

    /** Primitive type matching SoCADDetail::PrimType. */
    enum PrimType { EDGE = 0, TRIANGLE = 1, BOUNDS = 2 } primType = BOUNDS;

    uint32_t    primIndex0 = 0;  ///< Polyline index (EDGE) or tri index (TRIANGLE)
    uint32_t    primIndex1 = 0;  ///< Segment index within polyline (EDGE only)
    float       u          = 0.0f; ///< Param along segment [0,1] (EDGE only)

    float       t          = 0.0f; ///< Ray parameter of the closest hit
    SbVec3f     hitPoint;          ///< World-space hit point

    bool        valid = false;     ///< false if no hit was found
};

// ---------------------------------------------------------------------------
// BVH node (AABB tree)
// ---------------------------------------------------------------------------

/** Internal AABB tree node (flat array representation). */
struct BvhNode {
    SbBox3f bounds;
    int     left  = -1;   ///< Child index, or -1 if leaf
    int     right = -1;   ///< Child index, or -1 if leaf
    int     itemIdx = -1; ///< Payload index for leaves (-1 for internals)
};

// ---------------------------------------------------------------------------
// CadInstanceBVH – world-space AABB tree over all instances
// ---------------------------------------------------------------------------

/**
 * @brief AABB tree over all assembly instances for fast pick culling.
 *
 * Each leaf holds one instance's world-space bounding box.  During picking
 * the tree prunes the candidate set from O(N) to O(log N + hits).
 */
class CadInstanceBVH {
public:
    struct Entry {
        SbBox3f    worldBounds;
        InstanceId instanceId;
        PartId     partId;
        SbMatrix   localToWorld;  ///< For transforming pick ray into part space
    };

    /** Build the BVH from a flat list of instance entries.  O(N log N). */
    void build(std::vector<Entry> entries);

    /**
     * @brief Collect all instances whose world AABB intersects @p ray.
     *
     * Returned hits are unsorted; the caller should refine with per-part
     * geometry tests.
     */
    std::vector<const Entry*> query(const SbLine& ray) const;

    /** true after build() has been called with at least one entry. */
    bool isBuilt() const noexcept { return !nodes_.empty(); }

    /** Number of instances in the BVH. */
    size_t size() const noexcept { return entries_.size(); }

    /** Ray-AABB intersection test (public so other BVH classes can reuse it). */
    static bool rayIntersectsBox(const SbLine& ray, const SbBox3f& box) noexcept;

private:
    std::vector<Entry>   entries_;
    std::vector<BvhNode> nodes_;

    int buildRecursive(std::vector<int>& indices, int begin, int end);

    void queryRecursive(int nodeIdx, const SbLine& ray,
                        std::vector<const Entry*>& results) const;
};

// ---------------------------------------------------------------------------
// CadPartEdgeBVH – AABB tree over segments of one part's WireRep
// ---------------------------------------------------------------------------

/**
 * @brief Segment-level AABB tree for a single part's wireframe geometry.
 *
 * Built once per part (lazily) and reused across frames.
 */
class CadPartEdgeBVH {
public:
    struct SegEntry {
        SbVec3f  p0, p1;        ///< Segment endpoints in part-local space
        uint32_t polylineIdx;   ///< Index of the owning polyline
        uint32_t segmentIdx;    ///< Index within the polyline (0 = first seg)
    };

    /** Build from a flat list of segments. */
    void build(std::vector<SegEntry> segments);

    /** Combined result from queryClosest. */
    struct QueryResult {
        SegEntry seg;
        float    u = 0.0f;  ///< Parametric position along the hit segment [0,1]
    };

    /**
     * @brief Find the closest segment within world-space tolerance to @p ray.
     *
     * @param ray        Ray in part-local coordinates.
     * @param tolerance  Maximum world-space distance to accept a hit.
     * @return Closest hit segment + u parameter, or empty if none within tolerance.
     */
    std::optional<QueryResult> queryClosest(const SbLine& ray, float tolerance) const;

    /** true after build() with at least one segment. */
    bool isBuilt() const noexcept { return !nodes_.empty(); }

private:
    std::vector<SegEntry> segments_;
    std::vector<BvhNode>  nodes_;

    int  buildRecursive(std::vector<int>& indices, int begin, int end);
    void queryRecursive(int nodeIdx, const SbLine& ray, float tolerance,
                        float& bestDist2, const SegEntry** bestSeg,
                        float& bestU) const;

    /** Closest distance squared from a ray to a line segment. */
    static float raySegDist2(const SbLine& ray,
                             const SbVec3f& p0, const SbVec3f& p1,
                             float& outU) noexcept;

    static SbBox3f segBounds(const SbVec3f& p0, const SbVec3f& p1) noexcept;
};

// ---------------------------------------------------------------------------
// CadPickQuery – orchestrates picking using the above BVH structures
// ---------------------------------------------------------------------------

/**
 * @brief Orchestrates a complete pick operation against an assembly.
 *
 * Instantiate one per rayPick() call; reuse the BVH structures across calls.
 */
class CadPickQuery {
public:
    /**
     * @brief Perform edge (wire) picking.
     *
     * @param ray            World-space pick ray.
     * @param instanceBvh    Pre-built instance BVH.
     * @param partGeometries Map from PartId to PartGeometry (for seg BVHs).
     * @param partBvhCache   Lazily built per-part segment BVH cache.
     * @param toleranceWS    World-space tolerance for segment picking.
     * @return Best hit or invalid result.
     */
    static CadPickResult pickEdge(
        const SbLine&                                       ray,
        const CadInstanceBVH&                               instanceBvh,
        const std::unordered_map<PartId, obol::PartGeometry,
                                 std::hash<obol::PartId>>&  partGeometries,
        std::unordered_map<PartId, CadPartEdgeBVH,
                           std::hash<obol::PartId>>&        partBvhCache,
        float                                               toleranceWS);

    /**
     * @brief Perform bounding-box picking (bounds proxy).
     *
     * Intersects the pick ray with instance world bounding boxes.  Always
     * returns a result as long as any instance exists.
     */
    static CadPickResult pickBounds(
        const SbLine&        ray,
        const CadInstanceBVH& instanceBvh);
};

} // namespace picking
} // namespace obol

#endif // OBOL_CAD_PICKING_CADPICKING_H
