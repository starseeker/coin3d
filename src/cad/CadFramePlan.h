#ifndef OBOL_CAD_CADFRAMEPLAN_H
#define OBOL_CAD_CADFRAMEPLAN_H

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
 * @file CadFramePlan.h
 * @brief Internal per-frame rendering plan built by SoCADAssembly.
 *
 * These structs are NOT public API.  They are the internal representation
 * that SoCADAssembly builds each frame (or when dirty) to describe what
 * must be rendered and what GPU resources are needed.
 */

#include <obol/cad/CadIds.h>
#include <Inventor/SbBox3f.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbColor4f.h>

#include <vector>
#include <cstdint>
#include <array>

namespace obol {
namespace internal {

// ---------------------------------------------------------------------------
// Rep key: identifies a (Part, type, LoD level) GPU resource slot
// ---------------------------------------------------------------------------

enum class CadRepType : uint8_t {
    WireSegments = 0,
    Triangles    = 1,
};

/**
 * @brief Key for a GPU geometry representation.
 *
 * A representation is keyed by (part ID, geometry type, LoD level).
 * Level 0 is the coarsest; higher levels add detail.
 */
struct CadRepKey {
    PartId       part;
    CadRepType   type  = CadRepType::WireSegments;
    uint8_t      level = 255; ///< 255 = full detail (no LoD reduction)

    bool operator==(const CadRepKey& o) const noexcept {
        return part == o.part && type == o.type && level == o.level;
    }
};

// ---------------------------------------------------------------------------
// Visible instance record (compact, GPU-uploadable)
// ---------------------------------------------------------------------------

/**
 * @brief Per-instance data packed for GPU upload.
 *
 * Laid out to be uploadable as a vertex attribute or SSBO element:
 *   - 16 floats for the 4×4 local-to-world transform (column-major)
 *   - 4 bytes RGBA color
 *   - 1 uint32 part index within the frame plan's part list
 *   - 1 uint32 flags (bit 0 = selected)
 *   - 2 uint64 InstanceId (for CPU-side picking round-trip)
 *   - 2 × float[3] world bounding box (for per-instance frustum culling)
 */
struct CadVisibleInstance {
    std::array<float, 16> transform = {};   ///< Column-major 4×4
    std::array<uint8_t, 4> rgba     = {204, 204, 204, 255}; ///< RGBA8
    uint32_t partIndex = 0;
    uint32_t flags     = 0;   ///< bit 0: selected; bit 1: hovered
    InstanceId instanceId;
    /// World-space bounding box min/max.  Used by the renderer for per-instance
    /// frustum culling (avoids drawing instances completely outside the view).
    float wbMin[3] = {0.0f, 0.0f, 0.0f};
    float wbMax[3] = {0.0f, 0.0f, 0.0f};
};

// ---------------------------------------------------------------------------
// Draw item: one batched draw call
// ---------------------------------------------------------------------------

/**
 * @brief One entry in the indirect draw command list.
 *
 * Maps to a single glMultiDrawElementsIndirect sub-command (or a single
 * glDrawElementsInstancedBaseInstance call in the fallback path).
 */
struct CadDrawItem {
    CadRepKey rep;
    uint32_t  baseInstance   = 0; ///< First CadVisibleInstance index for this item
    uint32_t  instanceCount  = 0; ///< Number of instances sharing this part rep
    // GPU-resolved at render time:
    uint32_t  vertexOffset   = 0; ///< Byte offset into vertex buffer
    uint32_t  indexOffset    = 0; ///< Byte offset into index buffer
    uint32_t  indexCount     = 0; ///< Number of indices to draw
    int       baseVertex     = 0; ///< Base vertex for indexed drawing
};

// ---------------------------------------------------------------------------
// CadFramePlan: the complete per-frame rendering work order
// ---------------------------------------------------------------------------

/**
 * @brief Complete work order for one rendered frame.
 *
 * SoCADAssembly::GLRender() builds this struct from the current instance
 * database and LoD choices, then hands it to CadRendererGL to execute.
 */
struct CadFramePlan {
    /** All visible instances, sorted by (partIndex, repLevel) for batching. */
    std::vector<CadVisibleInstance> visibleInstances;

    /**
     * Draw items for the wire pass.  Each item refers to a contiguous run
     * of visibleInstances that share the same CadRepKey.
     */
    std::vector<CadDrawItem> wireItems;

    /**
     * Draw items for the shaded pass.  May be empty in WIREFRAME mode unless
     * wireframeOcclusion is enabled (depth-only prepass).
     */
    std::vector<CadDrawItem> shadedItems;

    /**
     * GPU representations required to execute this plan.  The renderer uses
     * this list to page in any missing GPU resources before drawing.
     */
    std::vector<CadRepKey> requiredReps;

    /** Aggregate world bounding box of all visible instances. */
    SbBox3f worldBounds;
};

} // namespace internal
} // namespace obol

#endif // OBOL_CAD_CADFRAMEPLAN_H
