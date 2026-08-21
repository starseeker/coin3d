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

#include <Obol/cad/CadIds.h>
#include <Obol/cad/CadProgressive.h>
#include <Inventor/SbBox3f.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbColor4f.h>

#include <vector>
#include <deque>
#include <cstdint>
#include <array>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace Obol {

struct PartGeometry;

namespace internal {

enum CadInstanceFlag : uint32_t {
    CadInstanceSelected      = 1u << 0,
    CadInstanceHovered       = 1u << 1,
    CadInstanceColorOverride = 1u << 2,
    /*
     * Visibility is retained presentation state, not topology.  Keeping a
     * hidden occurrence in the compiled plan lets a sparse erase/show delta
     * preserve part bindings, level buckets, GPU atlases, and every unrelated
     * instance record.
     */
    CadInstanceHidden        = 1u << 3,
    /* Screen-important instances remain ordinary geometry even when the
     * scene-wide small-object threshold rises under render pressure. */
    CadInstancePointProxyProtected = 1u << 4
};

// ---------------------------------------------------------------------------
// Rep key: identifies one stable (Part, representation type) GPU resource.
// ---------------------------------------------------------------------------

enum class CadRepType : uint8_t {
    Points       = 0,
    WireSegments = 1,
    Triangles    = 2,
};

/**
 * @brief Key for a GPU geometry representation.
 *
 * The resident PoP prefix is one appendable resource.  Per-instance active
 * cuts select a draw count and quantization level without creating another
 * GPU representation.
 */
struct CadRepKey {
    PartId       part;
    CadRepType   type  = CadRepType::WireSegments;

    bool operator==(const CadRepKey& o) const noexcept {
        return part == o.part && type == o.type;
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
 *   - 1 uint32 flags (CadInstanceFlag)
 *   - 2 uint64 InstanceId (for CPU-side picking round-trip)
 *   - 2 × float[3] world bounding box (for per-instance frustum culling)
 */
struct CadVisibleInstance {
    std::array<float, 16> transform = {};   ///< Column-major 4×4
    std::array<uint8_t, 4> rgba     = {204, 204, 204, 255}; ///< RGBA8
    float lineWidth = 1.0f;
    uint16_t linePattern = 0xffffu;
    uint16_t linePatternFactor = 1u;
    uint32_t partIndex = 0;
    uint32_t flags     = 0;   ///< CadInstanceFlag bit set
    uint8_t lodCut = Obol::ProgressiveCutUnspecified; ///< producer-authored retained PoP draw cut
    InstanceId instanceId;
    /// World-space bounding box min/max.  Used by the renderer for per-instance
    /// frustum culling (avoids drawing instances completely outside the view).
    float wbMin[3] = {0.0f, 0.0f, 0.0f};
    float wbMax[3] = {0.0f, 0.0f, 0.0f};
};

// ---------------------------------------------------------------------------
// Stable part binding: geometry and generation resolved while compiling plan
// ---------------------------------------------------------------------------

/**
 * @brief Direct, lifetime-safe binding for one part used by this plan.
 *
 * A frame plan owns the immutable geometry shared pointer.  Camera-only
 * frames and sparse LoD-cut changes can therefore use a compact array index
 * instead of resolving the same PartId through assembly hash tables for
 * every draw item.  A topology-compatible retained-array replacement patches
 * this binding and generation in place; only channel/topology changes
 * recompile the plan.
 */
struct CadPartBinding {
    PartId part;
    std::shared_ptr<const PartGeometry> geometry;
    uint64_t generation = 0;
    bool subpixelProxyEligible = false;
    bool structuralProxy = false;
    bool lodStructuralProxy = false;
    std::array<SbVec3f, 8> subpixelProxyCorners = {};
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
    uint32_t  partIndex      = 0; ///< Index into CadFramePlan::partBindings
    uint32_t  baseInstance   = 0; ///< First CadVisibleInstance index for this item
    uint32_t  instanceCount  = 0; ///< Number of instances sharing this part rep
    bool      customWireStyle = false; ///< Wire run requires width/stipple state
    bool      cullBackfaces = false; ///< Safe CCW closed-surface shaded draw
    // GPU-resolved at render time:
    uint32_t  vertexOffset   = 0; ///< Byte offset into vertex buffer
    uint32_t  indexOffset    = 0; ///< Byte offset into index buffer
    uint32_t  indexCount     = 0; ///< Number of indices to draw
    int       baseVertex     = 0; ///< Base vertex for indexed drawing
};

/**
 * A view-local point replacing one eligible wire proxy for this frame.
 * Positions are world-space, so the batch has no per-instance transform.
 */
struct CadSubpixelProxyPoint {
    SbVec3f position;
    std::array<uint8_t, 4> rgba = {204, 204, 204, 255};
    InstanceId instanceId;
    uint32_t flags = 0;
};

/**
 * One non-consuming batch of in-place instance attribute changes.
 *
 * Revision-stamped indices let renderers in independent GL contexts patch
 * only the instance records they have not yet observed.
 */
struct CadInstanceAttributeDelta {
    uint64_t revision = 0;
    std::vector<uint32_t> visibleIndices;
    bool visibilityChanged = false;
};

/**
 * One fixed progressive-part span whose per-occurrence PoP cuts changed.
 *
 * The occurrence and draw-item slots are structural and remain allocated in
 * the plan.  A renderer which has prepared the preceding revision can update
 * just these ranges instead of repeating whole-scene visibility, atlas-touch,
 * instance-packing, and indirect-command passes.
 */
struct CadShadedLodRange {
    uint32_t partIndex = 0;
    uint32_t baseInstance = 0;
    uint32_t instanceCount = 0;
    uint32_t shadedItemBegin = 0;
    uint32_t shadedItemCount = 0;
};

struct CadShadedLodDelta {
    uint64_t revision = 0;
    std::vector<CadShadedLodRange> ranges;
};

/**
 * One append-only structural publication.
 *
 * Streaming realization keeps the compiled prefix fixed, hides superseded
 * box records, and appends new part/instance/draw-item ranges.  Retained
 * renderers can therefore admit and append only the shaded tail.  Any
 * structural mutation which cannot honor this contract does not publish a
 * delta and continues to invalidate the exact layout normally.
 */
struct CadPlanAppendDelta {
    uint64_t revision = 0;
    uint64_t subpixelProxyInputRevision = 0;
    uint32_t visibleBegin = 0;
    uint32_t visibleCount = 0;
    uint32_t partBegin = 0;
    uint32_t partCount = 0;
    uint32_t shadedItemBegin = 0;
    uint32_t shadedItemCount = 0;
    std::vector<uint32_t> retiredVisibleIndices;
};

/**
 * One topology-compatible replacement of immutable arrays behind fixed part
 * and occurrence slots.  Progressive resident-prefix growth uses this path.
 */
struct CadPartGeometryRange {
    uint32_t partIndex = 0;
    uint32_t baseInstance = 0;
    uint32_t instanceCount = 0;
    uint32_t shadedItemBegin = 0;
    uint32_t shadedItemCount = 0;
};

struct CadPartGeometryDelta {
    uint64_t revision = 0;
    /**
     * True when one or more replacement parts changed conservative bounds.
     * Bounds-preserving immutable generation swaps leave the instance BVH
     * valid and need only patch renderer geometry bindings.
     */
    bool boundsChanged = false;
    /**
     * True when producer-authorized proxy eligibility or conservative proxy
     * corners changed.  Most progressive resident-prefix updates preserve
     * these inputs and therefore must not trigger a scene-wide reprojection.
     */
    bool subpixelProxyInputChanged = false;
    std::vector<CadPartGeometryRange> ranges;
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
    /** Changes whenever immutable presentation data in this plan changes. */
    uint64_t revision = 0;

    /** Changes only when flattened geometry or transforms change. */
    uint64_t geometryRevision = 0;

    /**
     * Changes when shaded draw membership, order, or per-occurrence LoD runs
     * change.  A color/selection-only update deliberately leaves this stable
     * so a prepared indirect command stream can be retained.
     */
    uint64_t shadedLayoutRevision = 0;

    /**
     * Changes when in-place per-instance attributes (currently color and
     * presentation flags) change without changing shaded draw membership.
     */
    uint64_t instanceAttributeRevision = 0;
    uint64_t instanceAttributeDeltaFloorRevision = 0;
    size_t instanceAttributeDeltaEntryCount = 0;
    std::deque<CadInstanceAttributeDelta> instanceAttributeDeltas;

    /**
     * Changes only when active PoP cuts move within fixed progressive-part
     * spans.  Structural shadedLayoutRevision deliberately remains stable,
     * allowing retained renderers to consume this bounded delta journal.
     */
    uint64_t shadedLodRevision = 0;
    uint64_t shadedLodDeltaFloorRevision = 0;
    size_t shadedLodDeltaEntryCount = 0;
    std::deque<CadShadedLodDelta> shadedLodDeltas;

    uint64_t appendRevision = 0;
    uint64_t appendDeltaFloorRevision = 0;
    size_t appendDeltaEntryCount = 0;
    std::deque<CadPlanAppendDelta> appendDeltas;

    uint64_t partGeometryRevision = 0;
    uint64_t partGeometryDeltaFloorRevision = 0;
    size_t partGeometryDeltaEntryCount = 0;
    std::deque<CadPartGeometryDelta> partGeometryDeltas;

    /**
     * All retained instances, sorted by (partIndex, repLevel) for batching.
     * CadInstanceHidden records remain here and are skipped at execution.
     */
    std::vector<CadVisibleInstance> visibleInstances;

    /**
     * Direct part bindings referenced by visibleInstances and draw items.
     * The shared ownership makes payload access valid for the complete plan
     * lifetime, including camera-only frames with no scene traversal.
     */
    std::vector<CadPartBinding> partBindings;

    bool hasCustomWireStyle = false;

    /**
     * Draw items for the wire pass.  Each item refers to a contiguous run
     * of visibleInstances that share the same CadRepKey.
     */
    std::vector<CadDrawItem> wireItems;

    /** Draw items for point primitives. */
    std::vector<CadDrawItem> pointItems;

    /**
     * Camera-dependent replacements for wire proxy instances.  The mask is
     * parallel to visibleInstances; set entries are omitted from wire draws
     * and emitted by the single point batch instead.
     */
    std::vector<uint8_t> subpixelProxyMask;
    std::vector<CadSubpixelProxyPoint> subpixelProxyPoints;
    uint64_t subpixelProxyRevision = 0;
    /**
     * Changes only when the occurrence topology or conservative proxy inputs
     * change.  It deliberately remains stable while immutable PoP buffers
     * grow behind fixed part and occurrence slots.
     */
    uint64_t subpixelProxyInputRevision = 0;
    uint64_t subpixelProxySourceInputRevision = 0;

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

    /**
     * Per-part summaries used while paging GPU representations.  Keeping
     * these in the plan avoids rescanning every draw item for every required
     * part (quadratic in a scene with many distinct meshes).
     */
    std::unordered_map<PartId, uint8_t, std::hash<PartId>>
        maximumRequestedCutByPart;
    std::unordered_set<PartId, std::hash<PartId>>
        wirePartsWithUncollapsedInstances;

    /** Conservative aggregate world bounding box of visible instances. */
    SbBox3f worldBounds;
};

} // namespace internal
} // namespace Obol

#endif // OBOL_CAD_CADFRAMEPLAN_H
