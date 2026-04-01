#ifndef OBOL_CAD_LOD_SEGMENTPOPLOD_H
#define OBOL_CAD_LOD_SEGMENTPOPLOD_H

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
 * @file SegmentPopLod.h
 * @brief POP-quantisation LoD for line segments (standalone, no BRL-CAD deps).
 *
 * Inspired by the POP buffer technique: at each LoD level the endpoint
 * coordinates are snapped to a power-of-two grid.  Segments that become
 * degenerate (both endpoints snap to the same grid cell) are discarded.
 *
 * ### Level convention
 * Levels are integers in [0, kMaxLevel]:
 *   - Level kMaxLevel  (255 by default) = full detail (no snapping, all segs)
 *   - Level 0          = coarsest (all coords snapped to 1 cell → nearly all
 *                        short segments degenerate and are dropped)
 *
 * This matches the "LOD = distance from camera" intuition: a far-away part
 * gets a small level value and therefore fewer segments.
 *
 * ### Thread safety
 * All const methods are safe to call from multiple threads simultaneously
 * after the object is built.  build() must be called from a single thread.
 */

#include <Inventor/SbVec3f.h>
#include <Inventor/SbBox3f.h>

#include <vector>
#include <cstdint>

namespace obol {

/**
 * @brief POP-quantisation LoD for a collection of line segments.
 *
 * @code
 *   std::vector<SbVec3f> pts = { ... }; // flat list: seg0p0, seg0p1, seg1p0, ...
 *   SbBox3f bounds = ...;
 *   SegmentPopLod lod;
 *   lod.build(pts, bounds);
 *   auto segs = lod.segmentsAtLevel(5); // get level-5 segment list
 * @endcode
 */
class SegmentPopLod {
public:
    /** Maximum representable LoD level (full detail). */
    static constexpr uint8_t kMaxLevel = 255;

    SegmentPopLod() = default;

    /**
     * @brief Build the LoD structure from a flat segment list.
     *
     * @param points  Flat array of 3-D points.  Each consecutive pair
     *                (points[2i], points[2i+1]) defines one segment.
     *                Must have an even number of elements.
     * @param bounds  Tight bounding box of all points; used to normalise
     *                coordinates for grid snapping.
     */
    void build(const std::vector<SbVec3f>& points, const SbBox3f& bounds);

    /**
     * @brief Return indices of segments that are non-degenerate at @p level.
     *
     * The returned vector contains indices into the original segment list
     * (0-based).  Caller can map index i → original points[2i], points[2i+1].
     *
     * Higher levels include more segments (strictly non-decreasing with level).
     *
     * @param level  LoD level in [0, kMaxLevel].
     * @return Sorted vector of non-degenerate segment indices.
     */
    std::vector<uint32_t> segmentsAtLevel(uint8_t level) const;

    /**
     * @brief Minimum level at which segment @p idx becomes non-degenerate.
     *
     * Segments with a high minLevel are very short (they snap to a degenerate
     * segment at coarse levels) and should be shown only at fine LoD.
     */
    uint8_t minLevelForSegment(uint32_t idx) const;

    /** Total number of segments in the input. */
    size_t segmentCount() const noexcept { return minLevel_.size(); }

    /** true if build() has been called successfully. */
    bool isBuilt() const noexcept { return built_; }

    /**
     * @brief Snap a single coordinate component to the grid at @p level.
     *
     * Exposed primarily for unit testing.
     *
     * @param v     Normalised coordinate in [0, 1].
     * @param level LoD level in [0, kMaxLevel].
     */
    static float snapNorm(float v, uint8_t level) noexcept;

private:
    /** Per-segment minimum non-degenerate level. */
    std::vector<uint8_t> minLevel_;
    bool                 built_  = false;
};

} // namespace obol

#endif // OBOL_CAD_LOD_SEGMENTPOPLOD_H
