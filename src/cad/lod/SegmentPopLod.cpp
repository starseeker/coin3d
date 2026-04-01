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

#include "SegmentPopLod.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>

namespace obol {

// ---------------------------------------------------------------------------
// snapNorm – snap a normalised [0,1] coordinate to a POP grid cell centre
// ---------------------------------------------------------------------------
//
// Level convention:
//   level kMaxLevel (255) = full detail: no snapping (grid has 2^8 = 256 cells)
//   level 0              = coarsest: single cell (everything maps to 0.5)
//
// At level L the grid has (2^(L+1)) cells of width 1/(2^(L+1)).
// The snapped value is the centre of the cell containing v.
//
// Implementation detail: we compute floor(v * 2^(L+1)) / 2^(L+1) + 0.5 / 2^(L+1).

float
SegmentPopLod::snapNorm(float v, uint8_t level) noexcept
{
    if (level == kMaxLevel) return v;  // full detail: no snapping

    // Number of grid cells along each axis at this level: 2^(level+1).
    // Cap the shift at 23 to stay within float mantissa precision
    // (2^23 = 8388608 cells ≈ sub-micrometre precision for metre-scale parts).
    // Using a plain shift without modulo ensures cell count is strictly
    // non-decreasing with level (monotone property required for LoD).
    const uint32_t shift = static_cast<uint32_t>(level) + 1u;
    const uint32_t cells = (shift >= 24u) ? (1u << 23u) : (1u << shift);
    const float    invCells = 1.0f / static_cast<float>(cells);

    // Clamp v to [0, 1 - epsilon].  The upper clamp ensures v=1.0 snaps to
    // the centre of the last cell instead of overflowing into a non-existent
    // cell beyond the bounds.
    float vc = v < 0.0f ? 0.0f : (v >= 1.0f ? 1.0f - invCells * 0.5f : v);

    uint32_t cell = static_cast<uint32_t>(vc * static_cast<float>(cells));
    return (static_cast<float>(cell) + 0.5f) * invCells;
}

// ---------------------------------------------------------------------------
// build
// ---------------------------------------------------------------------------

void
SegmentPopLod::build(const std::vector<SbVec3f>& points, const SbBox3f& bounds)
{
    built_ = false;
    minLevel_.clear();

    if (points.size() < 2) {
        built_ = true;
        return;
    }

    SbVec3f bmin, bmax;
    bounds.getBounds(bmin, bmax);
    SbVec3f bsize = bmax - bmin;

    // Guard against degenerate bounds
    const float kEps = 1e-10f;
    if (bsize[0] < kEps) bsize[0] = kEps;
    if (bsize[1] < kEps) bsize[1] = kEps;
    if (bsize[2] < kEps) bsize[2] = kEps;

    const size_t numSegs = points.size() / 2;
    minLevel_.resize(numSegs);

    for (size_t i = 0; i < numSegs; ++i) {
        const SbVec3f& p0 = points[2 * i];
        const SbVec3f& p1 = points[2 * i + 1];

        // Normalise to [0,1]
        float n0x = (p0[0] - bmin[0]) / bsize[0];
        float n0y = (p0[1] - bmin[1]) / bsize[1];
        float n0z = (p0[2] - bmin[2]) / bsize[2];
        float n1x = (p1[0] - bmin[0]) / bsize[0];
        float n1y = (p1[1] - bmin[1]) / bsize[1];
        float n1z = (p1[2] - bmin[2]) / bsize[2];

        // Binary search for the minimum level at which the two endpoints
        // snap to different grid cells (i.e. the segment is non-degenerate).
        uint8_t lo = 0, hi = kMaxLevel, found = kMaxLevel;
        while (lo <= hi) {
            uint8_t mid = static_cast<uint8_t>(lo + (hi - lo) / 2);
            float s0x = snapNorm(n0x, mid);
            float s0y = snapNorm(n0y, mid);
            float s0z = snapNorm(n0z, mid);
            float s1x = snapNorm(n1x, mid);
            float s1y = snapNorm(n1y, mid);
            float s1z = snapNorm(n1z, mid);
            bool degenerate = (s0x == s1x && s0y == s1y && s0z == s1z);
            if (!degenerate) {
                found = mid;
                if (mid == 0) break;
                hi = mid - 1;
            } else {
                if (mid == 255) break;
                lo = mid + 1;
            }
        }
        minLevel_[i] = found;
    }

    built_ = true;
}

// ---------------------------------------------------------------------------
// segmentsAtLevel
// ---------------------------------------------------------------------------

std::vector<uint32_t>
SegmentPopLod::segmentsAtLevel(uint8_t level) const
{
    std::vector<uint32_t> result;
    result.reserve(minLevel_.size());
    for (uint32_t i = 0; i < static_cast<uint32_t>(minLevel_.size()); ++i) {
        if (minLevel_[i] <= level) {
            result.push_back(i);
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// minLevelForSegment
// ---------------------------------------------------------------------------

uint8_t
SegmentPopLod::minLevelForSegment(uint32_t idx) const
{
    if (idx >= static_cast<uint32_t>(minLevel_.size())) return kMaxLevel;
    return minLevel_[idx];
}

} // namespace obol
