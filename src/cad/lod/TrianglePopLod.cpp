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

#include "TrianglePopLod.h"

#include <algorithm>
#include <cmath>
#include <cassert>
#include <cstdint>

namespace obol {

// ---------------------------------------------------------------------------
// snapNorm – identical convention to SegmentPopLod::snapNorm
// ---------------------------------------------------------------------------

float
TrianglePopLod::snapNorm(float v, uint8_t level) noexcept
{
    if (level == kMaxLevel) return v;

    // Cap the shift at 23 to stay within float mantissa precision and ensure
    // the cell count is strictly non-decreasing with level (monotone LoD).
    const uint32_t shift    = static_cast<uint32_t>(level) + 1u;
    const uint32_t cells    = (shift >= 24u) ? (1u << 23u) : (1u << shift);
    const float    invCells = 1.0f / static_cast<float>(cells);

    // Upper clamp ensures v=1.0 snaps to the last cell centre, not beyond.
    float vc = v < 0.0f ? 0.0f : (v >= 1.0f ? 1.0f - invCells * 0.5f : v);
    uint32_t cell = static_cast<uint32_t>(vc * static_cast<float>(cells));
    return (static_cast<float>(cell) + 0.5f) * invCells;
}

// ---------------------------------------------------------------------------
// build
// ---------------------------------------------------------------------------

void
TrianglePopLod::build(const std::vector<SbVec3f>&  positions,
                      const std::vector<uint32_t>& indices,
                      const SbBox3f&               bounds)
{
    built_ = false;
    minLevel_.clear();

    if (positions.empty() || indices.size() < 3) {
        built_ = true;
        return;
    }

    const size_t numTris = indices.size() / 3;
    minLevel_.resize(numTris);

    SbVec3f bmin, bmax;
    bounds.getBounds(bmin, bmax);
    SbVec3f bsize = bmax - bmin;

    const float kEps = 1e-10f;
    if (bsize[0] < kEps) bsize[0] = kEps;
    if (bsize[1] < kEps) bsize[1] = kEps;
    if (bsize[2] < kEps) bsize[2] = kEps;

    for (size_t t = 0; t < numTris; ++t) {
        const uint32_t i0 = indices[3 * t];
        const uint32_t i1 = indices[3 * t + 1];
        const uint32_t i2 = indices[3 * t + 2];

        if (i0 >= positions.size() || i1 >= positions.size() || i2 >= positions.size()) {
            minLevel_[t] = kMaxLevel;
            continue;
        }

        const SbVec3f& v0 = positions[i0];
        const SbVec3f& v1 = positions[i1];
        const SbVec3f& v2 = positions[i2];

        // Normalise all three vertices to [0,1]
        float n0[3], n1[3], n2[3];
        for (int a = 0; a < 3; ++a) {
            n0[a] = (v0[a] - bmin[a]) / bsize[a];
            n1[a] = (v1[a] - bmin[a]) / bsize[a];
            n2[a] = (v2[a] - bmin[a]) / bsize[a];
        }

        // Binary search for minimum non-degenerate level
        uint8_t lo = 0, hi = kMaxLevel, found = kMaxLevel;
        while (lo <= hi) {
            uint8_t mid = static_cast<uint8_t>(lo + (hi - lo) / 2);

            float s0[3], s1[3], s2[3];
            for (int a = 0; a < 3; ++a) {
                s0[a] = snapNorm(n0[a], mid);
                s1[a] = snapNorm(n1[a], mid);
                s2[a] = snapNorm(n2[a], mid);
            }

            // Degenerate if all three vertices snap to the same cell
            bool same01 = (s0[0] == s1[0] && s0[1] == s1[1] && s0[2] == s1[2]);
            bool same02 = (s0[0] == s2[0] && s0[1] == s2[1] && s0[2] == s2[2]);
            bool degenerate = same01 && same02;

            if (!degenerate) {
                found = mid;
                if (mid == 0) break;
                hi = mid - 1;
            } else {
                if (mid == 255) break;
                lo = mid + 1;
            }
        }
        minLevel_[t] = found;
    }

    built_ = true;
}

// ---------------------------------------------------------------------------
// trianglesAtLevel
// ---------------------------------------------------------------------------

std::vector<uint32_t>
TrianglePopLod::trianglesAtLevel(uint8_t level) const
{
    std::vector<uint32_t> result;
    result.reserve(minLevel_.size());
    for (uint32_t t = 0; t < static_cast<uint32_t>(minLevel_.size()); ++t) {
        if (minLevel_[t] <= level) {
            result.push_back(t);
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// minLevelForTriangle
// ---------------------------------------------------------------------------

uint8_t
TrianglePopLod::minLevelForTriangle(uint32_t idx) const
{
    if (idx >= static_cast<uint32_t>(minLevel_.size())) return kMaxLevel;
    return minLevel_[idx];
}

} // namespace obol
