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

#include "CadPicking.h"
#include "../CadProgressiveUtils.h"

#include <Inventor/SbMatrix.h>
#include <Inventor/SbBox3f.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <numeric>
#include <functional>

namespace Obol {
namespace picking {

namespace {

SbBox3f
expandedBox(const SbBox3f& box, float tolerance) noexcept
{
    if (box.isEmpty() || tolerance <= 0.0f) return box;

    SbVec3f bmin, bmax;
    box.getBounds(bmin, bmax);
    const SbVec3f pad(tolerance, tolerance, tolerance);

    SbBox3f expanded;
    expanded.setBounds(bmin - pad, bmax + pad);
    return expanded;
}

bool
rayBoxHit(const SbLine& ray, const SbBox3f& box, float* hitT) noexcept
{
    if (box.isEmpty()) return false;
    SbVec3f bmin, bmax;
    box.getBounds(bmin, bmax);

    const SbVec3f& orig = ray.getPosition();
    const SbVec3f& dir  = ray.getDirection();

    float tmin = -std::numeric_limits<float>::infinity();
    float tmax =  std::numeric_limits<float>::infinity();

    for (int a = 0; a < 3; ++a) {
        float d = dir[a];
        if (std::abs(d) < 1e-12f) {
            if (orig[a] < bmin[a] || orig[a] > bmax[a]) return false;
        } else {
            float t1 = (bmin[a] - orig[a]) / d;
            float t2 = (bmax[a] - orig[a]) / d;
            if (t1 > t2) std::swap(t1, t2);
            if (t1 > tmin) tmin = t1;
            if (t2 < tmax) tmax = t2;
            if (tmin > tmax) return false;
        }
    }

    if (tmax < 0.0f) return false;
    if (hitT) *hitT = std::max(0.0f, tmin);
    return true;
}

}  // namespace

// ===========================================================================
// CadInstanceBVH
// ===========================================================================

void
CadInstanceBVH::build(std::vector<Entry> entries)
{
    entries_ = std::move(entries);
    nodes_.clear();
    if (entries_.empty()) return;

    std::vector<int> indices(entries_.size());
    std::iota(indices.begin(), indices.end(), 0);
    buildRecursive(indices, 0, static_cast<int>(indices.size()));
}

int
CadInstanceBVH::buildRecursive(std::vector<int>& indices, int begin, int end)
{
    assert(begin < end);
    int nodeIdx = static_cast<int>(nodes_.size());
    nodes_.emplace_back();
    BvhNode& node = nodes_.back();

    // Compute combined bounds
    SbBox3f combined;
    for (int i = begin; i < end; ++i) {
        combined.extendBy(entries_[indices[i]].worldBounds);
    }
    node.bounds = combined;

    if (end - begin == 1) {
        node.itemIdx = indices[begin];
        node.left  = -1;
        node.right = -1;
        return nodeIdx;
    }

    // Split along the longest axis at the median
    SbVec3f extents = combined.getSize();
    int axis = 0;
    if (extents[1] > extents[0]) axis = 1;
    if (extents[2] > extents[axis]) axis = 2;

    int mid = begin + (end - begin) / 2;
    std::nth_element(indices.begin() + begin, indices.begin() + mid,
                     indices.begin() + end,
                     [&](int a, int b) {
                         SbVec3f ca = entries_[a].worldBounds.getCenter();
                         SbVec3f cb = entries_[b].worldBounds.getCenter();
                         return ca[axis] < cb[axis];
                     });

    int leftChild  = buildRecursive(indices, begin, mid);
    int rightChild = buildRecursive(indices, mid, end);

    // Node may have been reallocated — re-fetch by index
    nodes_[nodeIdx].left  = leftChild;
    nodes_[nodeIdx].right = rightChild;
    nodes_[nodeIdx].itemIdx = -1;
    return nodeIdx;
}

bool
CadInstanceBVH::rayIntersectsBox(const SbLine& ray, const SbBox3f& box) noexcept
{
    return rayBoxHit(ray, box, nullptr);
}

void
CadInstanceBVH::queryRecursive(int nodeIdx, const SbLine& ray, float tolerance,
                               std::vector<const Entry*>& results) const
{
    if (nodeIdx < 0 || nodeIdx >= static_cast<int>(nodes_.size())) return;
    const BvhNode& node = nodes_[nodeIdx];
    if (!rayIntersectsBox(ray, expandedBox(node.bounds, tolerance))) return;

    if (node.itemIdx >= 0) {
        results.push_back(&entries_[node.itemIdx]);
        return;
    }
    queryRecursive(node.left,  ray, tolerance, results);
    queryRecursive(node.right, ray, tolerance, results);
}

std::vector<const CadInstanceBVH::Entry*>
CadInstanceBVH::query(const SbLine& ray) const
{
    return query(ray, 0.0f);
}

std::vector<const CadInstanceBVH::Entry*>
CadInstanceBVH::query(const SbLine& ray, float tolerance) const
{
    std::vector<const Entry*> results;
    if (!nodes_.empty()) {
        queryRecursive(0, ray, tolerance, results);
    }
    return results;
}

// ===========================================================================
// CadPartEdgeBVH
// ===========================================================================

SbBox3f
CadPartEdgeBVH::segBounds(const SbVec3f& p0, const SbVec3f& p1) noexcept
{
    SbBox3f b;
    b.extendBy(p0);
    b.extendBy(p1);
    // Slightly expand to handle degenerate segments
    const float kPad = 1e-6f;
    SbVec3f bmin, bmax;
    b.getBounds(bmin, bmax);
    b.setBounds(bmin - SbVec3f(kPad, kPad, kPad),
                bmax + SbVec3f(kPad, kPad, kPad));
    return b;
}

void
CadPartEdgeBVH::build(std::vector<SegEntry> segments)
{
    segments_ = std::move(segments);
    nodes_.clear();
    if (segments_.empty()) return;

    std::vector<int> indices(segments_.size());
    std::iota(indices.begin(), indices.end(), 0);
    buildRecursive(indices, 0, static_cast<int>(indices.size()));
}

int
CadPartEdgeBVH::buildRecursive(std::vector<int>& indices, int begin, int end)
{
    assert(begin < end);
    int nodeIdx = static_cast<int>(nodes_.size());
    nodes_.emplace_back();
    BvhNode& node = nodes_.back();

    SbBox3f combined;
    for (int i = begin; i < end; ++i) {
        combined.extendBy(segBounds(segments_[indices[i]].p0,
                                    segments_[indices[i]].p1));
    }
    node.bounds = combined;

    if (end - begin == 1) {
        node.itemIdx = indices[begin];
        node.left  = -1;
        node.right = -1;
        return nodeIdx;
    }

    SbVec3f extents = combined.getSize();
    int axis = 0;
    if (extents[1] > extents[0]) axis = 1;
    if (extents[2] > extents[axis]) axis = 2;

    int mid = begin + (end - begin) / 2;
    std::nth_element(indices.begin() + begin, indices.begin() + mid,
                     indices.begin() + end,
                     [&](int a, int b) {
                         SbVec3f ca = (segments_[a].p0 + segments_[a].p1) * 0.5f;
                         SbVec3f cb = (segments_[b].p0 + segments_[b].p1) * 0.5f;
                         return ca[axis] < cb[axis];
                     });

    int leftChild  = buildRecursive(indices, begin, mid);
    int rightChild = buildRecursive(indices, mid, end);

    nodes_[nodeIdx].left  = leftChild;
    nodes_[nodeIdx].right = rightChild;
    nodes_[nodeIdx].itemIdx = -1;
    return nodeIdx;
}

float
CadPartEdgeBVH::raySegDist2(const SbLine& ray,
                             const SbVec3f& p0, const SbVec3f& p1,
                             float& outU) noexcept
{
    // Closest distance squared between an infinite ray and a finite line segment.
    // Based on Ericson "Real-Time Collision Detection" §5.1.9.
    //
    // Naming: s = parameter along SEGMENT [0,1], t_ray = parameter along RAY [0,∞)
    const SbVec3f& ro = ray.getPosition();
    const SbVec3f& rd = ray.getDirection();  // assumed unit length

    SbVec3f r  = p0 - ro;   // offset from ray origin to segment start
    SbVec3f ab = p1 - p0;   // segment direction

    float a = ab.dot(ab);   // squared length of segment
    float b = ab.dot(rd);   // segment · ray  (Ericson's b)
    float c = ab.dot(r);    // segment · offset
    float f = rd.dot(r);    // ray · offset   (Ericson's f)

    float denom = a - b * b;  // a*e - b*b where e=1 (unit ray)

    float s_seg;  // parameter along segment
    if (denom < 1e-12f) {
        // Parallel lines — use segment start
        s_seg = 0.0f;
    } else {
        s_seg = (b * f - c) / denom;
    }
    // Clamp to segment [0, 1]
    outU = s_seg < 0.0f ? 0.0f : (s_seg > 1.0f ? 1.0f : s_seg);

    // Recompute ray parameter for the (clamped) closest segment point
    float t_ray = (b * outU + f);  // = b*s + f  (with e=1)
    t_ray = t_ray < 0.0f ? 0.0f : t_ray;  // forward hits only

    SbVec3f segPt  = p0 + ab * outU;
    SbVec3f rayPt  = ro + rd * t_ray;
    SbVec3f diff   = segPt - rayPt;
    return diff.dot(diff);
}

void
CadPartEdgeBVH::queryRecursive(int nodeIdx, const SbLine& ray, float tolerance,
                               float& bestDist2, const SegEntry** bestSeg,
                               float& bestU) const
{
    if (nodeIdx < 0 || nodeIdx >= static_cast<int>(nodes_.size())) return;
    const BvhNode& node = nodes_[nodeIdx];

    if (!CadInstanceBVH::rayIntersectsBox(ray, expandedBox(node.bounds, tolerance))) return;

    if (node.itemIdx >= 0) {
        const SegEntry& seg = segments_[node.itemIdx];
        float u = 0.0f;
        float d2 = raySegDist2(ray, seg.p0, seg.p1, u);
        if (d2 < bestDist2) {
            bestDist2 = d2;
            *bestSeg  = &seg;
            bestU     = u;
        }
        return;
    }
    queryRecursive(node.left,  ray, tolerance, bestDist2, bestSeg, bestU);
    queryRecursive(node.right, ray, tolerance, bestDist2, bestSeg, bestU);
}

std::optional<CadPartEdgeBVH::QueryResult>
CadPartEdgeBVH::queryClosest(const SbLine& ray, float tolerance) const
{
    if (nodes_.empty()) return std::nullopt;

    float bestDist2 = tolerance * tolerance;
    const SegEntry* bestSeg = nullptr;
    float bestU = 0.0f;
    queryRecursive(0, ray, tolerance, bestDist2, &bestSeg, bestU);

    if (bestSeg) return QueryResult{ *bestSeg, bestU };
    return std::nullopt;
}

void
CadPartEdgeBVH::queryTransformedRecursive(
    int nodeIdx, const SbLine& localRay, float localTolerance,
    const SbLine& worldRay, const SbMatrix& localToWorld,
    float& bestWorldDist2, const SegEntry** bestSeg, float& bestU) const
{
    if (nodeIdx < 0 || nodeIdx >= static_cast<int>(nodes_.size())) return;
    const BvhNode& node = nodes_[nodeIdx];
    if (!CadInstanceBVH::rayIntersectsBox(
            localRay, expandedBox(node.bounds, localTolerance)))
        return;

    if (node.itemIdx >= 0) {
        const SegEntry& seg = segments_[node.itemIdx];
        SbVec3f worldP0;
        SbVec3f worldP1;
        localToWorld.multVecMatrix(seg.p0, worldP0);
        localToWorld.multVecMatrix(seg.p1, worldP1);
        float u = 0.0f;
        const float distance2 =
            raySegDist2(worldRay, worldP0, worldP1, u);
        if (distance2 < bestWorldDist2) {
            bestWorldDist2 = distance2;
            *bestSeg = &seg;
            bestU = u;
        }
        return;
    }
    queryTransformedRecursive(
        node.left, localRay, localTolerance, worldRay, localToWorld,
        bestWorldDist2, bestSeg, bestU);
    queryTransformedRecursive(
        node.right, localRay, localTolerance, worldRay, localToWorld,
        bestWorldDist2, bestSeg, bestU);
}

std::optional<CadPartEdgeBVH::QueryResult>
CadPartEdgeBVH::queryClosestTransformed(
    const SbLine& localRay, float localBroadphaseTolerance,
    const SbLine& worldRay, const SbMatrix& localToWorld,
    float worldTolerance) const
{
    if (nodes_.empty()) return std::nullopt;
    float bestDistance2 = worldTolerance * worldTolerance;
    const SegEntry *bestSeg = nullptr;
    float bestU = 0.0f;
    queryTransformedRecursive(
        0, localRay, localBroadphaseTolerance, worldRay, localToWorld,
        bestDistance2, &bestSeg, bestU);
    return bestSeg ? std::optional<QueryResult>(QueryResult{*bestSeg, bestU}) :
        std::nullopt;
}

// ===========================================================================
// CadPartPointBVH
// ===========================================================================

SbBox3f
CadPartPointBVH::pointBounds(const SbVec3f& point) noexcept
{
    constexpr float padding = 1e-6f;
    const SbVec3f extent(padding, padding, padding);
    return SbBox3f(point - extent, point + extent);
}

void
CadPartPointBVH::build(const std::vector<SbVec3f>& positions,
                       const std::vector<uint32_t>& pointIds)
{
    points_.clear();
    nodes_.clear();
    points_.reserve(positions.size());
    for (size_t index = 0; index < positions.size(); ++index) {
        points_.push_back({positions[index],
            index < pointIds.size() ? pointIds[index] :
                static_cast<uint32_t>(index)});
    }
    if (points_.empty())
        return;
    std::vector<int> indices(points_.size());
    std::iota(indices.begin(), indices.end(), 0);
    buildRecursive(indices, 0, static_cast<int>(indices.size()));
}

int
CadPartPointBVH::buildRecursive(
    std::vector<int>& indices, int begin, int end)
{
    assert(begin < end);
    const int nodeIndex = static_cast<int>(nodes_.size());
    nodes_.emplace_back();
    SbBox3f combined;
    for (int index = begin; index < end; ++index)
        combined.extendBy(pointBounds(points_[indices[index]].point));
    nodes_[nodeIndex].bounds = combined;

    if (end - begin == 1) {
        nodes_[nodeIndex].itemIdx = indices[begin];
        return nodeIndex;
    }

    const SbVec3f extents = combined.getSize();
    int axis = extents[1] > extents[0] ? 1 : 0;
    if (extents[2] > extents[axis])
        axis = 2;
    const int middle = begin + (end - begin) / 2;
    std::nth_element(indices.begin() + begin, indices.begin() + middle,
        indices.begin() + end, [&](int left, int right) {
            return points_[left].point[axis] < points_[right].point[axis];
        });
    const int left = buildRecursive(indices, begin, middle);
    const int right = buildRecursive(indices, middle, end);
    nodes_[nodeIndex].left = left;
    nodes_[nodeIndex].right = right;
    return nodeIndex;
}

void
CadPartPointBVH::queryTransformedRecursive(
    int nodeIdx, const SbLine& localRay, float localTolerance,
    const SbLine& worldRay, const SbMatrix& localToWorld,
    float worldTolerance2, float& bestT,
    const PointEntry** bestPoint) const
{
    if (nodeIdx < 0 || nodeIdx >= static_cast<int>(nodes_.size()))
        return;
    const BvhNode& node = nodes_[nodeIdx];
    if (!CadInstanceBVH::rayIntersectsBox(
            localRay, expandedBox(node.bounds, localTolerance)))
        return;
    if (node.itemIdx >= 0) {
        const PointEntry& point = points_[node.itemIdx];
        SbVec3f worldPoint;
        localToWorld.multVecMatrix(point.point, worldPoint);
        const float t = (worldPoint - worldRay.getPosition()).dot(
            worldRay.getDirection());
        if (t < 0.0f || t >= bestT)
            return;
        const SbVec3f closest = worldRay.getPosition() +
            worldRay.getDirection() * t;
        if ((worldPoint - closest).sqrLength() > worldTolerance2)
            return;
        bestT = t;
        *bestPoint = &point;
        return;
    }
    queryTransformedRecursive(node.left, localRay, localTolerance,
        worldRay, localToWorld, worldTolerance2, bestT, bestPoint);
    queryTransformedRecursive(node.right, localRay, localTolerance,
        worldRay, localToWorld, worldTolerance2, bestT, bestPoint);
}

std::optional<CadPartPointBVH::QueryResult>
CadPartPointBVH::queryClosestTransformed(
    const SbLine& localRay, float localBroadphaseTolerance,
    const SbLine& worldRay, const SbMatrix& localToWorld,
    float worldTolerance) const
{
    if (nodes_.empty())
        return std::nullopt;
    float bestT = std::numeric_limits<float>::infinity();
    const PointEntry *bestPoint = nullptr;
    queryTransformedRecursive(0, localRay, localBroadphaseTolerance,
        worldRay, localToWorld, worldTolerance * worldTolerance,
        bestT, &bestPoint);
    return bestPoint ? std::optional<QueryResult>(
        QueryResult{*bestPoint, bestT}) : std::nullopt;
}

// ===========================================================================
// CadPickQuery
// ===========================================================================

CadPickResult
CadPickQuery::pickPoint(
    const SbLine& ray,
    const CadInstanceBVH& instanceBvh,
    const std::unordered_map<PartId, std::shared_ptr<const Obol::PartGeometry>,
                             std::hash<Obol::PartId>>& partGeometries,
    float toleranceWS,
    CadPartPointBvhCache *partBvhCache)
{
    CadPickResult best;
    best.t = std::numeric_limits<float>::infinity();
    CadPartPointBvhCache localCache;
    CadPartPointBvhCache& cache = partBvhCache ?
        *partBvhCache : localCache;

    for (const auto* entry : instanceBvh.query(ray, toleranceWS)) {
        auto geometry = partGeometries.find(entry->partId);
        if (geometry == partGeometries.end() || !geometry->second ||
                !geometry->second->points)
            continue;
        const Obol::PointRep& points = *geometry->second->points;
        auto cached = cache.find(entry->partId);
        if (cached == cache.end()) {
            CadPartPointBVH bvh;
            bvh.build(points.positions, points.pointIds);
            cached = cache.emplace(entry->partId, std::move(bvh)).first;
        }
        if (!cached->second.isBuilt())
            continue;

        const SbMatrix worldToLocal = entry->localToWorld.inverse();
        SbVec3f localOrigin;
        SbVec3f localDirection;
        worldToLocal.multVecMatrix(ray.getPosition(), localOrigin);
        worldToLocal.multDirMatrix(ray.getDirection(), localDirection);
        if (localDirection.normalize() == 0.0f)
            continue;
        const SbLine localRay(localOrigin, localOrigin + localDirection);
        double inverseNorm2 = 0.0;
        for (int row = 0; row < 3; ++row)
            for (int column = 0; column < 3; ++column) {
                const double component = worldToLocal[row][column];
                inverseNorm2 += component * component;
            }
        const float localTolerance = static_cast<float>(
            toleranceWS * std::sqrt(inverseNorm2));
        const auto hit = cached->second.queryClosestTransformed(
            localRay, localTolerance, ray, entry->localToWorld, toleranceWS);
        if (!hit || hit->t >= best.t)
            continue;
        SbVec3f worldPoint;
        entry->localToWorld.multVecMatrix(hit->point.point, worldPoint);
        best.t = hit->t;
        best.hitPoint = worldPoint;
        best.instanceId = entry->instanceId;
        best.partId = entry->partId;
        best.primType = CadPickResult::POINT;
        best.primIndex0 = hit->point.pointId;
        best.valid = true;
    }
    return best;
}

CadPickResult
CadPickQuery::pickEdge(
    const SbLine&                                       ray,
    const CadInstanceBVH&                               instanceBvh,
    const std::unordered_map<PartId, std::shared_ptr<const Obol::PartGeometry>,
                             std::hash<Obol::PartId>>&  partGeometries,
    std::unordered_map<PartId, CadPartEdgeBVH,
                       std::hash<Obol::PartId>>&        partBvhCache,
    float                                               toleranceWS,
    uint8_t                                             lodCeiling,
    CadProgressiveEdgeBvhCache                         *progressiveBvhCache)
{
    CadPickResult best;
    best.t = std::numeric_limits<float>::infinity();
    CadProgressiveEdgeBvhCache localProgressiveCache;
    CadProgressiveEdgeBvhCache& cutCache = progressiveBvhCache ?
        *progressiveBvhCache : localProgressiveCache;

    const auto& candidates = instanceBvh.query(ray, toleranceWS);
    for (const auto* entry : candidates) {
        const PartId& pid = entry->partId;

        auto geomIt = partGeometries.find(pid);
        if (geomIt == partGeometries.end() || !geomIt->second) continue;
        const auto& geom = *geomIt->second;
        if (!geom.wire.has_value()) continue;

        const Obol::WireRep& wire = *geom.wire;
        const CadPartEdgeBVH *edgeBvh = nullptr;
        if (const Obol::TriMesh *triangleEdges = wire.triangleEdges()) {
            const Obol::TriMesh& mesh = *triangleEdges;
            const uint8_t level = mesh.isProgressive() ?
                Obol::internal::cadFullyResidentProgressiveCut(mesh,
                    std::min(entry->lodCut, lodCeiling)) :
                Obol::ProgressiveCutUnspecified;
            const CadProgressivePickKey key{pid, level};
            auto cached = cutCache.find(key);
            if (cached == cutCache.end()) {
                std::vector<CadPartEdgeBVH::SegEntry> segs;
                segs.reserve(mesh.isProgressive() ?
                    wire.segmentCountAtCut(level) : mesh.indices.size());
                const auto appendRange = [&](size_t first, size_t count) {
                    const size_t end = std::min(
                        mesh.indices.size(), first + count);
                    for (size_t index = first; index + 2 < end; index += 3) {
                        const uint32_t source[3] = {
                            mesh.indices[index], mesh.indices[index + 1],
                            mesh.indices[index + 2]
                        };
                        if (source[0] >= mesh.positions.size() ||
                                source[1] >= mesh.positions.size() ||
                                source[2] >= mesh.positions.size())
                            continue;
                        SbVec3f point[3];
                        for (int corner = 0; corner < 3; ++corner) {
                            point[corner] = mesh.isProgressive() ?
                                Obol::internal::cadProgressiveSnapPoint(
                                    mesh.positions[source[corner]],
                                    mesh.progressiveQuantizationMinimum,
                                    mesh.progressiveQuantizationMaximum,
                                    mesh.quantizationAtCut(level)) :
                                mesh.positions[source[corner]];
                        }
                        for (uint32_t edge = 0; edge < 3; ++edge) {
                            const size_t edgeIndex = index + edge;
                            if (edgeIndex > UINT32_MAX)
                                continue;
                            segs.push_back({
                                point[edge], point[(edge + 1) % 3],
                                static_cast<uint32_t>(edgeIndex), 0});
                        }
                    }
                };
                if (mesh.hasAdaptiveProgressiveClusters()) {
                    for (const Obol::ProgressiveTriangleCluster& cluster :
                            mesh.progressiveClusters) {
                        for (const Obol::ProgressiveTriangleClusterRange& range :
                                cluster.ranges) {
                            if (range.activationCut > level)
                                break;
                            appendRange(range.firstIndex, range.indexCount);
                        }
                    }
                } else {
                    appendRange(0, mesh.isProgressive() ?
                        mesh.indexCountAtCut(level) : mesh.indices.size());
                }
                CadPartEdgeBVH bvh;
                bvh.build(std::move(segs));
                cached = cutCache.emplace(key, std::move(bvh)).first;
            }
            edgeBvh = &cached->second;
        } else if (wire.isProgressive()) {
            const uint8_t level =
                Obol::internal::cadFullyResidentProgressiveCut(
                wire, std::min(entry->lodCut, lodCeiling));
            const CadProgressivePickKey key{pid, level};
            auto cached = cutCache.find(key);
            if (cached == cutCache.end()) {
                std::vector<CadPartEdgeBVH::SegEntry> segs;
                segs.reserve(wire.segmentCountAtCut(level));
                const auto appendRange = [&](size_t first, size_t count) {
                    const size_t end =
                        std::min(wire.segmentCount(), first + count);
                    for (size_t sourceSegment = first;
                            sourceSegment < end; ++sourceSegment) {
                        const uint32_t segId =
                            sourceSegment < wire.segmentIds.size() ?
                            wire.segmentIds[sourceSegment] :
                            static_cast<uint32_t>(sourceSegment);
                        segs.push_back({
                            Obol::internal::cadProgressiveSnapPoint(
                                wire.segmentPoints[2 * sourceSegment],
                                wire.progressiveQuantizationMinimum,
                                wire.progressiveQuantizationMaximum,
                                wire.quantizationAtCut(level)),
                            Obol::internal::cadProgressiveSnapPoint(
                                wire.segmentPoints[2 * sourceSegment + 1],
                                wire.progressiveQuantizationMinimum,
                                wire.progressiveQuantizationMaximum,
                                wire.quantizationAtCut(level)),
                            segId, 0 });
                    }
                };
                if (wire.hasAdaptiveProgressiveClusters()) {
                    for (const Obol::ProgressiveWireCluster& cluster :
                            wire.progressiveClusters)
                        for (const Obol::ProgressiveWireClusterRange& range :
                                cluster.ranges) {
                            if (range.activationCut > level)
                                break;
                            appendRange(
                                range.firstSegment, range.segmentCount);
                        }
                } else {
                    appendRange(wire.segmentFirstAtCut(level),
                        wire.segmentCountAtCut(level));
                }
                CadPartEdgeBVH bvh;
                bvh.build(std::move(segs));
                cached = cutCache.emplace(key, std::move(bvh)).first;
            }
            edgeBvh = &cached->second;
        } else {
            // Build non-progressive part edge BVH lazily.
            auto bvhIt = partBvhCache.find(pid);
            if (bvhIt == partBvhCache.end()) {
                CadPartEdgeBVH cached;
                std::vector<CadPartEdgeBVH::SegEntry> segs;
                segs.reserve(wire.segmentCount());
                for (size_t i = 0; i < wire.segmentCount(); ++i) {
                    const uint32_t segId =
                        (i < wire.segmentIds.size()) ?
                        wire.segmentIds[i] : static_cast<uint32_t>(i);
                    segs.push_back({ wire.segmentPoints[2 * i],
                                     wire.segmentPoints[2 * i + 1],
                                     segId,
                                     0 });
                }
                uint32_t polyIdx = 0;
                for (const auto& polyline : wire.polylines) {
                    for (size_t si = 0; si + 1 < polyline.points.size(); ++si) {
                        segs.push_back({ polyline.points[si],
                                         polyline.points[si + 1],
                                         polyIdx,
                                         static_cast<uint32_t>(si) });
                    }
                    ++polyIdx;
                }
                cached.build(std::move(segs));
                partBvhCache[pid] = std::move(cached);
                bvhIt = partBvhCache.find(pid);
            }
            edgeBvh = &bvhIt->second;
        }
        if (!edgeBvh || !edgeBvh->isBuilt()) continue;

        // Transform ray into part-local space
        SbMatrix w2l = entry->localToWorld.inverse();

        SbVec3f localOrigin, localDir;
        w2l.multVecMatrix(ray.getPosition(), localOrigin);
        w2l.multDirMatrix(ray.getDirection(), localDir);
        float dirLen = localDir.length();
        if (dirLen < 1e-12f) continue;
        localDir /= dirLen;
        SbLine localRay(localOrigin, localOrigin + localDir);

        /* The Frobenius norm is a conservative upper bound on the inverse
         * transform's distance amplification.  Use it only for BVH pruning;
         * candidate ordering and acceptance are evaluated exactly in world
         * space below. */
        double inverseNorm2 = 0.0;
        for (int row = 0; row < 3; ++row)
            for (int column = 0; column < 3; ++column) {
                const double component = w2l[row][column];
                inverseNorm2 += component * component;
            }
        const float localTol = static_cast<float>(
            toleranceWS * std::sqrt(inverseNorm2));

        auto hit = edgeBvh->queryClosestTransformed(
            localRay, localTol, ray, entry->localToWorld, toleranceWS);
        if (!hit) continue;

        // Compute world-space t for depth comparison
        // Use the closest point on the segment as an approximation
        SbVec3f segPt = hit->seg.p0 + (hit->seg.p1 - hit->seg.p0) * hit->u;
        SbVec3f worldPt;
        entry->localToWorld.multVecMatrix(segPt, worldPt);

        float t = (worldPt - ray.getPosition()).dot(ray.getDirection());
        if (t < 0.0f) continue;  // behind camera

        if (t < best.t) {
            best.t          = t;
            best.hitPoint   = worldPt;
            best.instanceId = entry->instanceId;
            best.partId     = entry->partId;
            best.primType   = CadPickResult::EDGE;
            best.primIndex0 = hit->seg.polylineIdx;
            best.primIndex1 = hit->seg.segmentIdx;
            best.u          = hit->u;
            best.valid      = true;
        }
    }
    return best;
}

CadPickResult
CadPickQuery::pickBounds(
    const SbLine&         ray,
    const CadInstanceBVH& instanceBvh,
    float                 toleranceWS)
{
    CadPickResult best;
    best.t = std::numeric_limits<float>::infinity();

    const auto& candidates = instanceBvh.query(ray, toleranceWS);
    for (const auto* entry : candidates) {
        const SbBox3f bounds = expandedBox(entry->worldBounds, toleranceWS);
        float t = std::numeric_limits<float>::infinity();
        if (!rayBoxHit(ray, bounds, &t)) continue;
        if (t < best.t) {
            best.t          = t;
            best.hitPoint   = ray.getPosition() + ray.getDirection() * t;
            best.instanceId = entry->instanceId;
            best.partId     = entry->partId;
            best.primType   = CadPickResult::BOUNDS;
            best.valid      = true;
        }
    }
    return best;
}

// ===========================================================================
// CadPartTriBVH
// ===========================================================================

SbBox3f
CadPartTriBVH::triBounds(const TriEntry& t) noexcept
{
    SbBox3f b;
    b.extendBy(t.p0);
    b.extendBy(t.p1);
    b.extendBy(t.p2);
    const float kBoundsPadding = 1e-6f;
    SbVec3f bmin, bmax;
    b.getBounds(bmin, bmax);
    b.setBounds(bmin - SbVec3f(kBoundsPadding, kBoundsPadding, kBoundsPadding),
                bmax + SbVec3f(kBoundsPadding, kBoundsPadding, kBoundsPadding));
    return b;
}

bool
CadPartTriBVH::rayTriIntersect(const SbLine& ray,
                                const SbVec3f& p0, const SbVec3f& p1,
                                const SbVec3f& p2,
                                float& t, float& u, float& v) noexcept
{
    // Möller–Trumbore algorithm.
    const SbVec3f& orig = ray.getPosition();
    const SbVec3f  dir  = ray.getDirection();

    SbVec3f e1 = p1 - p0;
    SbVec3f e2 = p2 - p0;
    SbVec3f h  = dir.cross(e2);
    float   a  = e1.dot(h);

    if (std::abs(a) < 1e-12f) return false;  // Ray is parallel to triangle

    float   f  = 1.0f / a;
    SbVec3f s  = orig - p0;
    u = f * s.dot(h);
    if (u < 0.0f || u > 1.0f) return false;

    SbVec3f q = s.cross(e1);
    v = f * dir.dot(q);
    if (v < 0.0f || u + v > 1.0f) return false;

    t = f * e2.dot(q);
    return t > 0.0f;
}

void
CadPartTriBVH::build(const std::vector<SbVec3f>& positions,
                     const std::vector<uint32_t>& indices)
{
    build(positions, indices, {});
}

void
CadPartTriBVH::build(const std::vector<SbVec3f>& positions,
                     const std::vector<uint32_t>& indices,
                     const std::vector<uint32_t>& triangleIds)
{
    triangles_.clear();
    nodes_.clear();
    if (positions.empty() || indices.size() < 3) return;

    const size_t nTri = indices.size() / 3;
    if (!triangleIds.empty() && triangleIds.size() != nTri)
        return;
    triangles_.reserve(nTri);
    for (size_t i = 0; i < nTri; ++i) {
        uint32_t i0 = indices[i * 3 + 0];
        uint32_t i1 = indices[i * 3 + 1];
        uint32_t i2 = indices[i * 3 + 2];
        if (i0 >= positions.size() || i1 >= positions.size() ||
                i2 >= positions.size()) continue;
        TriEntry e;
        e.p0       = positions[i0];
        e.p1       = positions[i1];
        e.p2       = positions[i2];
        e.triIndex = triangleIds.empty() ? static_cast<uint32_t>(i) :
            triangleIds[i];
        e.compactIndex = static_cast<uint32_t>(i);
        triangles_.push_back(e);
    }
    if (triangles_.empty()) return;

    std::vector<int> idx(triangles_.size());
    std::iota(idx.begin(), idx.end(), 0);
    buildRecursive(idx, 0, static_cast<int>(idx.size()));
}

int
CadPartTriBVH::buildRecursive(std::vector<int>& indices, int begin, int end)
{
    assert(begin < end);
    int nodeIdx = static_cast<int>(nodes_.size());
    nodes_.emplace_back();
    BvhNode& node = nodes_.back();

    SbBox3f combined;
    for (int i = begin; i < end; ++i) {
        combined.extendBy(triBounds(triangles_[indices[i]]));
    }
    node.bounds = combined;

    if (end - begin == 1) {
        node.itemIdx = indices[begin];
        node.left  = -1;
        node.right = -1;
        return nodeIdx;
    }

    SbVec3f extents = combined.getSize();
    int axis = 0;
    if (extents[1] > extents[0]) axis = 1;
    if (extents[2] > extents[axis]) axis = 2;

    int mid = begin + (end - begin) / 2;
    std::nth_element(indices.begin() + begin, indices.begin() + mid,
                     indices.begin() + end,
                     [&](int a, int b) {
                         SbVec3f ca = (triangles_[a].p0 + triangles_[a].p1 +
                                       triangles_[a].p2) * (1.0f / 3.0f);
                         SbVec3f cb = (triangles_[b].p0 + triangles_[b].p1 +
                                       triangles_[b].p2) * (1.0f / 3.0f);
                         return ca[axis] < cb[axis];
                     });

    int leftChild  = buildRecursive(indices, begin, mid);
    int rightChild = buildRecursive(indices, mid, end);

    nodes_[nodeIdx].left  = leftChild;
    nodes_[nodeIdx].right = rightChild;
    nodes_[nodeIdx].itemIdx = -1;
    return nodeIdx;
}

void
CadPartTriBVH::queryRecursive(int nodeIdx, const SbLine& ray,
                               float& bestT, const TriEntry** bestTri,
                               float& bestU, float& bestV) const
{
    if (nodeIdx < 0 || nodeIdx >= static_cast<int>(nodes_.size())) return;
    const BvhNode& node = nodes_[nodeIdx];
    if (!CadInstanceBVH::rayIntersectsBox(ray, node.bounds)) return;

    if (node.itemIdx >= 0) {
        const TriEntry& tri = triangles_[node.itemIdx];
        float t = 0.0f, u = 0.0f, v = 0.0f;
        if (rayTriIntersect(ray, tri.p0, tri.p1, tri.p2, t, u, v)) {
            if (t < bestT) {
                bestT   = t;
                *bestTri = &tri;
                bestU   = u;
                bestV   = v;
            }
        }
        return;
    }
    queryRecursive(node.left,  ray, bestT, bestTri, bestU, bestV);
    queryRecursive(node.right, ray, bestT, bestTri, bestU, bestV);
}

std::optional<CadPartTriBVH::QueryResult>
CadPartTriBVH::queryClosest(const SbLine& ray) const
{
    if (nodes_.empty()) return std::nullopt;

    float bestT = std::numeric_limits<float>::infinity();
    const TriEntry* bestTri = nullptr;
    float bestU = 0.0f, bestV = 0.0f;
    queryRecursive(0, ray, bestT, &bestTri, bestU, bestV);

    if (!bestTri) return std::nullopt;
    const SbVec3f hitPoint =
        bestTri->p0 * (1.0f - bestU - bestV) +
        bestTri->p1 * bestU + bestTri->p2 * bestV;
    return QueryResult{bestTri->triIndex, bestTri->compactIndex,
        bestT, bestU, bestV, hitPoint};
}

// ===========================================================================
// CadPickQuery::pickTriangle
// ===========================================================================

CadPickResult
CadPickQuery::pickTriangle(
    const SbLine&                                       ray,
    const CadInstanceBVH&                               instanceBvh,
    const std::unordered_map<PartId, std::shared_ptr<const Obol::PartGeometry>,
                             std::hash<Obol::PartId>>&  partGeometries,
    std::unordered_map<PartId, CadPartTriBVH,
                       std::hash<Obol::PartId>>&        partTriBvhCache,
    float                                               toleranceWS,
    uint8_t                                             lodCeiling,
    CadProgressiveTriBvhCache                          *progressiveBvhCache)
{
    CadPickResult best;
    best.t = std::numeric_limits<float>::infinity();
    CadProgressiveTriBvhCache localProgressiveCache;
    CadProgressiveTriBvhCache& cutCache = progressiveBvhCache ?
        *progressiveBvhCache : localProgressiveCache;

    const auto& candidates = instanceBvh.query(ray, toleranceWS);
    for (const auto* entry : candidates) {
        const PartId& pid = entry->partId;

        auto geomIt = partGeometries.find(pid);
        if (geomIt == partGeometries.end() || !geomIt->second) continue;
        const auto& geom = *geomIt->second;
        if (!geom.shaded.has_value()) continue;

        const Obol::TriMesh& mesh = *geom.shaded;
        const CadPartTriBVH *triBvh = nullptr;
        if (mesh.isProgressive()) {
            const uint8_t activeLevel =
                Obol::internal::cadFullyResidentProgressiveCut(
                mesh, std::min(entry->lodCut, lodCeiling));
            const CadProgressivePickKey key{pid, activeLevel};
            auto cached = cutCache.find(key);
            if (cached == cutCache.end()) {
                std::vector<SbVec3f> progressivePositions;
                progressivePositions.reserve(mesh.positions.size());
                for (const SbVec3f& point : mesh.positions) {
                    progressivePositions.push_back(
                        Obol::internal::cadProgressiveSnapPoint(
                            point, mesh.progressiveQuantizationMinimum,
                            mesh.progressiveQuantizationMaximum,
                            mesh.quantizationAtCut(activeLevel)));
                }
                std::vector<uint32_t> progressiveIndices;
                std::vector<uint32_t> progressiveTriangleIds;
                if (mesh.hasAdaptiveProgressiveClusters()) {
                    bool validRanges = true;
                    for (const ProgressiveTriangleCluster& cluster :
                            mesh.progressiveClusters) {
                        for (const ProgressiveTriangleClusterRange& range :
                                cluster.ranges) {
                            if (range.activationCut > activeLevel)
                                break;
                            const uint64_t end =
                                static_cast<uint64_t>(range.firstIndex) +
                                range.indexCount;
                            if (range.firstIndex % 3u ||
                                    range.indexCount % 3u ||
                                    end > mesh.indices.size()) {
                                progressiveIndices.clear();
                                progressiveTriangleIds.clear();
                                validRanges = false;
                                break;
                            }
                            progressiveIndices.insert(
                                progressiveIndices.end(),
                                mesh.indices.begin() + range.firstIndex,
                                mesh.indices.begin() +
                                    static_cast<size_t>(end));
                            const uint32_t firstTriangle =
                                range.firstIndex / 3u;
                            const uint32_t triangleCount =
                                range.indexCount / 3u;
                            for (uint32_t triangle = 0;
                                    triangle < triangleCount; ++triangle)
                                progressiveTriangleIds.push_back(
                                    firstTriangle + triangle);
                        }
                        if (!validRanges)
                            break;
                    }
                } else {
                    const size_t indexCount =
                        mesh.indexCountAtCut(activeLevel);
                    progressiveIndices.assign(
                        mesh.indices.begin(),
                        mesh.indices.begin() + indexCount);
                }
                CadPartTriBVH bvh;
                bvh.build(progressivePositions, progressiveIndices,
                    progressiveTriangleIds);
                cached = cutCache.emplace(key, std::move(bvh)).first;
            }
            triBvh = &cached->second;
        } else {
            // Build non-progressive part triangle BVH lazily.
            auto bvhIt = partTriBvhCache.find(pid);
            if (bvhIt == partTriBvhCache.end()) {
                CadPartTriBVH cached;
                cached.build(mesh.positions, mesh.indices);
                partTriBvhCache[pid] = std::move(cached);
                bvhIt = partTriBvhCache.find(pid);
            }
            triBvh = &bvhIt->second;
        }
        if (!triBvh || !triBvh->isBuilt()) continue;

        // Transform ray into part-local space
        SbMatrix w2l = entry->localToWorld.inverse();

        SbVec3f localOrigin, localDir;
        w2l.multVecMatrix(ray.getPosition(), localOrigin);
        w2l.multDirMatrix(ray.getDirection(), localDir);
        float dirLen = localDir.length();
        if (dirLen < 1e-12f) continue;
        localDir /= dirLen;
        SbLine localRay(localOrigin, localOrigin + localDir);

        auto hit = triBvh->queryClosest(localRay);
        if (!hit) continue;

        SbVec3f worldHit;
        entry->localToWorld.multVecMatrix(hit->hitPoint, worldHit);

        float t = (worldHit - ray.getPosition()).dot(ray.getDirection());
        if (t < 0.0f) continue;  // behind camera

        if (t < best.t) {
            best.t          = t;
            best.hitPoint   = worldHit;
            best.instanceId = entry->instanceId;
            best.partId     = entry->partId;
            best.primType   = CadPickResult::TRIANGLE;
            best.primIndex0 = hit->triIndex;
            best.valid      = true;
        }
    }
    return best;
}

} // namespace picking
} // namespace Obol
