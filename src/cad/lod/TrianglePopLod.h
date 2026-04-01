#ifndef OBOL_CAD_LOD_TRIANGLEPOPLOD_H
#define OBOL_CAD_LOD_TRIANGLEPOPLOD_H

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
 * @file TrianglePopLod.h
 * @brief POP-quantisation LoD for triangle meshes (standalone, no BRL-CAD deps).
 *
 * Applies the same POP-buffer quantisation principle as SegmentPopLod but
 * to triangle meshes.  At each level the vertex coordinates are snapped to a
 * power-of-two grid; triangles whose three snapped vertices are all in the
 * same grid cell (degenerate) are discarded.
 *
 * ### Level convention
 * Same as SegmentPopLod:
 *   - Level kMaxLevel = full detail
 *   - Level 0         = coarsest (nearly all small triangles degenerate)
 *
 * ### Thread safety
 * All const methods are safe to call from multiple threads after build().
 */

#include <Inventor/SbVec3f.h>
#include <Inventor/SbBox3f.h>

#include <vector>
#include <cstdint>

namespace obol {

/**
 * @brief POP-quantisation LoD for a triangle mesh.
 *
 * @code
 *   TrianglePopLod lod;
 *   lod.build(positions, indices, bounds);
 *   auto tris = lod.trianglesAtLevel(5);  // list of non-degenerate tri indices
 * @endcode
 */
class TrianglePopLod {
public:
    /** Maximum representable LoD level (full detail). */
    static constexpr uint8_t kMaxLevel = 255;

    TrianglePopLod() = default;

    /**
     * @brief Build the LoD structure from a triangle mesh.
     *
     * @param positions  Vertex position array.
     * @param indices    Index array (triangle list; length must be divisible by 3).
     * @param bounds     Tight bounding box used for coordinate normalisation.
     */
    void build(const std::vector<SbVec3f>&  positions,
               const std::vector<uint32_t>& indices,
               const SbBox3f&               bounds);

    /**
     * @brief Return indices of triangles that are non-degenerate at @p level.
     *
     * Each returned value @c i corresponds to the i-th triangle in the
     * original mesh (vertices at indices[3i], indices[3i+1], indices[3i+2]).
     *
     * Triangle count is non-decreasing with level.
     *
     * @param level  LoD level in [0, kMaxLevel].
     * @return Sorted vector of non-degenerate triangle indices.
     */
    std::vector<uint32_t> trianglesAtLevel(uint8_t level) const;

    /** Minimum level at which triangle @p idx is non-degenerate. */
    uint8_t minLevelForTriangle(uint32_t idx) const;

    /** Total number of triangles in the input mesh. */
    size_t triangleCount() const noexcept { return minLevel_.size(); }

    /** true if build() has been called successfully. */
    bool isBuilt() const noexcept { return built_; }

    /**
     * @brief Snap a single normalised coordinate to the grid at @p level.
     * Exposed for unit tests.
     */
    static float snapNorm(float v, uint8_t level) noexcept;

private:
    std::vector<uint8_t> minLevel_;
    bool                 built_ = false;
};

} // namespace obol

#endif // OBOL_CAD_LOD_TRIANGLEPOPLOD_H
