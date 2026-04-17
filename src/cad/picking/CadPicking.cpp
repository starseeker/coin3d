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

#include <Inventor/SbMatrix.h>
#include <Inventor/SbBox3f.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <numeric>
#include <functional>

namespace obol {
namespace picking {

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
            // Ray parallel to slab — check if origin is inside
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
    return tmax >= 0.0f;
}

void
CadInstanceBVH::queryRecursive(int nodeIdx, const SbLine& ray,
                               std::vector<const Entry*>& results) const
{
    if (nodeIdx < 0 || nodeIdx >= static_cast<int>(nodes_.size())) return;
    const BvhNode& node = nodes_[nodeIdx];
    if (!rayIntersectsBox(ray, node.bounds)) return;

    if (node.itemIdx >= 0) {
        results.push_back(&entries_[node.itemIdx]);
        return;
    }
    queryRecursive(node.left,  ray, results);
    queryRecursive(node.right, ray, results);
}

std::vector<const CadInstanceBVH::Entry*>
CadInstanceBVH::query(const SbLine& ray) const
{
    std::vector<const Entry*> results;
    if (!nodes_.empty()) {
        queryRecursive(0, ray, results);
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

    // Cull if ray doesn't intersect the expanded AABB
    if (!CadInstanceBVH::rayIntersectsBox(ray, node.bounds)) return;

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

// ===========================================================================
// CadPickQuery
// ===========================================================================

CadPickResult
CadPickQuery::pickEdge(
    const SbLine&                                       ray,
    const CadInstanceBVH&                               instanceBvh,
    const std::unordered_map<PartId, obol::PartGeometry,
                             std::hash<obol::PartId>>&  partGeometries,
    std::unordered_map<PartId, CadPartEdgeBVH,
                       std::hash<obol::PartId>>&        partBvhCache,
    float                                               toleranceWS)
{
    CadPickResult best;
    best.t = std::numeric_limits<float>::infinity();

    const auto& candidates = instanceBvh.query(ray);
    for (const auto* entry : candidates) {
        const PartId& pid = entry->partId;

        auto geomIt = partGeometries.find(pid);
        if (geomIt == partGeometries.end()) continue;
        const auto& geom = geomIt->second;
        if (!geom.wire.has_value()) continue;

        // Build part edge BVH lazily
        auto bvhIt = partBvhCache.find(pid);
        if (bvhIt == partBvhCache.end()) {
            CadPartEdgeBVH edgeBvh;
            std::vector<CadPartEdgeBVH::SegEntry> segs;
            uint32_t polyIdx = 0;
            for (const auto& polyline : geom.wire->polylines) {
                for (size_t si = 0; si + 1 < polyline.points.size(); ++si) {
                    segs.push_back({ polyline.points[si],
                                     polyline.points[si + 1],
                                     polyIdx,
                                     static_cast<uint32_t>(si) });
                }
                ++polyIdx;
            }
            edgeBvh.build(std::move(segs));
            partBvhCache[pid] = std::move(edgeBvh);
            bvhIt = partBvhCache.find(pid);
        }

        // Transform ray into part-local space
        SbMatrix w2l = entry->localToWorld.inverse();

        SbVec3f localOrigin, localDir;
        w2l.multVecMatrix(ray.getPosition(), localOrigin);
        w2l.multDirMatrix(ray.getDirection(), localDir);
        float dirLen = localDir.length();
        if (dirLen < 1e-12f) continue;
        localDir /= dirLen;
        SbLine localRay(localOrigin, localOrigin + localDir);

        // Scale tolerance from world to local space (approximate: use max scale)
        // Simple heuristic: use the inverse of the max column magnitude
        float scaleApprox = 1.0f / (dirLen > 1e-6f ? dirLen : 1.0f);
        float localTol = toleranceWS * scaleApprox;

        auto hit = bvhIt->second.queryClosest(localRay, localTol);
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
    const CadInstanceBVH& instanceBvh)
{
    CadPickResult best;
    best.t = std::numeric_limits<float>::infinity();

    const auto& candidates = instanceBvh.query(ray);
    for (const auto* entry : candidates) {
        SbVec3f bmin, bmax;
        entry->worldBounds.getBounds(bmin, bmax);
        SbVec3f center = entry->worldBounds.getCenter();
        float t = (center - ray.getPosition()).dot(ray.getDirection());
        if (t < 0.0f) continue;
        if (t < best.t) {
            best.t          = t;
            best.hitPoint   = center;
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
    triangles_.clear();
    nodes_.clear();
    if (positions.empty() || indices.size() < 3) return;

    const size_t nTri = indices.size() / 3;
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
        e.triIndex = static_cast<uint32_t>(i);
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
    return QueryResult{ bestTri->triIndex, bestT, bestU, bestV };
}

// ===========================================================================
// CadPickQuery::pickTriangle
// ===========================================================================

CadPickResult
CadPickQuery::pickTriangle(
    const SbLine&                                       ray,
    const CadInstanceBVH&                               instanceBvh,
    const std::unordered_map<PartId, obol::PartGeometry,
                             std::hash<obol::PartId>>&  partGeometries,
    std::unordered_map<PartId, CadPartTriBVH,
                       std::hash<obol::PartId>>&        partTriBvhCache)
{
    CadPickResult best;
    best.t = std::numeric_limits<float>::infinity();

    const auto& candidates = instanceBvh.query(ray);
    for (const auto* entry : candidates) {
        const PartId& pid = entry->partId;

        auto geomIt = partGeometries.find(pid);
        if (geomIt == partGeometries.end()) continue;
        const auto& geom = geomIt->second;
        if (!geom.shaded.has_value()) continue;

        // Build part triangle BVH lazily
        auto bvhIt = partTriBvhCache.find(pid);
        if (bvhIt == partTriBvhCache.end()) {
            CadPartTriBVH triBvh;
            triBvh.build(geom.shaded->positions, geom.shaded->indices);
            partTriBvhCache[pid] = std::move(triBvh);
            bvhIt = partTriBvhCache.find(pid);
        }
        if (!bvhIt->second.isBuilt()) continue;

        // Transform ray into part-local space
        SbMatrix w2l = entry->localToWorld.inverse();

        SbVec3f localOrigin, localDir;
        w2l.multVecMatrix(ray.getPosition(), localOrigin);
        w2l.multDirMatrix(ray.getDirection(), localDir);
        float dirLen = localDir.length();
        if (dirLen < 1e-12f) continue;
        localDir /= dirLen;
        SbLine localRay(localOrigin, localOrigin + localDir);

        auto hit = bvhIt->second.queryClosest(localRay);
        if (!hit) continue;

        // Compute the world-space hit point from the local-space barycentric
        // intersection coordinates.
        const auto& mesh = *geom.shaded;
        uint32_t i0 = mesh.indices[hit->triIndex * 3 + 0];
        uint32_t i1 = mesh.indices[hit->triIndex * 3 + 1];
        uint32_t i2 = mesh.indices[hit->triIndex * 3 + 2];
        SbVec3f localHit = mesh.positions[i0] * (1.0f - hit->u - hit->v)
                         + mesh.positions[i1] * hit->u
                         + mesh.positions[i2] * hit->v;
        SbVec3f worldHit;
        entry->localToWorld.multVecMatrix(localHit, worldHit);

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
} // namespace obol
