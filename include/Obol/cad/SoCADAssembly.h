#ifndef OBOL_SOCADASSEMBLY_H
#define OBOL_SOCADASSEMBLY_H

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
 * @file SoCADAssembly.h
 * @brief Compiled CAD assembly node for large-scale CAD scene rendering.
 *
 * SoCADAssembly is a compiled-packet Inventor node that efficiently renders
 * CAD assemblies of 20k–10M instances without per-node scene-graph traversal.
 *
 * ### Key design decisions
 * - Geometry is ingested via an explicit API (not by populating child nodes).
 * - Wire geometry may be supplied either as flat segments or polylines;
 *   triangle meshes are optional and used for shaded rendering.
 * - Picking returns SoCADDetail with stable InstanceId.
 * - Progressive cuts are supplied explicitly per instance by the producer;
 *   this node never derives LoD from the camera or rebuilds a hierarchy.
 * - The node renders entirely within its GLRender() override; it does NOT
 *   walk children.
 *
 * ### Usage example
 * @code
 *   SoCADAssembly* asm = new SoCADAssembly;
 *   asm->drawMode = SoCADAssembly::WIREFRAME;
 *
 *   // Add a part with wire geometry
 *   Obol::PartId pid = Obol::CadIdBuilder::hash128("wheel");
 *   Obol::PartGeometry geom;
 *   geom.wire = Obol::WireRep{ ... };
 *   asm->upsertPart(pid, geom);
 *
 *   // Add an instance
 *   Obol::InstanceRecord rec;
 *   rec.part   = pid;
 *   rec.parent = Obol::CadIdBuilder::Root();
 *   rec.localToRoot.makeIdentity();
 *   Obol::InstanceId iid = asm->upsertInstanceAuto(rec);
 *
 *   root->addChild(asm);
 * @endcode
 */

#include <Inventor/nodes/SoSubNode.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/fields/SoSFEnum.h>
#include <Inventor/fields/SoSFBool.h>
#include <Inventor/fields/SoSFFloat.h>
#include <Inventor/fields/SoSFInt32.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbColor4f.h>
#include <Inventor/SbBox3f.h>
#include <Inventor/SbVec3f.h>

#include <Obol/cad/CadIds.h>
#include <Obol/cad/CadGpuResourceSnapshot.h>

#include <vector>
#include <array>
#include <algorithm>
#include <optional>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

class SoDetail;

namespace Obol {

/**
 * Logical work submitted by one completed CAD render.
 *
 * Counts include every visible occurrence and the active progressive cut,
 * rather than the richer resident geometry which may remain behind a
 * renderer-side LoD ceiling.  Consumers can therefore translate this record
 * into their own calibrated cost model without guessing from triangle ratios.
 */
struct CadRenderedWork {
    uint64_t triangleCount = 0;
    uint64_t lineCount = 0;
    uint64_t positionCount = 0;
    uint64_t normalCount = 0;
    uint64_t occurrenceCount = 0;
    bool exact = false;
};

// ---------------------------------------------------------------------------
// Geometry primitives ingested via the SoCADAssembly API
// ---------------------------------------------------------------------------

/**
 * @brief A single polyline of 3-D points in part-local space.
 *
 * Polylines represent feature edges (e.g. boundary edges of a CSG solid)
 * without requiring surface tessellation.
 */
struct WirePolyline {
    /** Polyline vertices in part-local coordinates. */
    std::vector<SbVec3f> points;

    /**
     * Optional per-polyline stable edge ID within this part.
     * Set to 0 if no stable edge identity is available.
     */
    uint32_t edgeId = 0;
};

/**
 * @brief Collection of line geometry representing the wireframe of a part.
 */
struct WireRep {
    /**
     * Flat segment endpoint list in part-local coordinates.
     *
     * Each consecutive pair (segmentPoints[2i], segmentPoints[2i+1])
     * defines one independent line segment.  This is the preferred storage
     * for CAD wire data that is naturally segment-oriented.
     */
    std::vector<SbVec3f> segmentPoints;

    /**
     * Optional stable ID per flat segment.  When present, segmentIds[i]
     * identifies the segment defined by segmentPoints[2i..2i+1].
     */
    std::vector<uint32_t> segmentIds;

    /** Return the number of complete flat segments. */
    size_t segmentCount() const noexcept { return segmentPoints.size() / 2; }

    /** Polyline storage for curves or callers that need connected strips. */
    std::vector<WirePolyline> polylines;

    /** Tight axis-aligned bounding box enclosing all wire geometry. */
    SbBox3f bounds;

    /**
     * Segment ranges for retained levels 0..15.  Prefix producers leave
     * progressiveSegmentFirst zeroed.  Native curve producers may pack an
     * independent approximation for each level and select it without
     * rebuilding or copying the part.
     */
    std::array<uint32_t, 16> progressiveSegmentFirst = {};
    std::array<uint32_t, 16> progressiveSegmentCount = {};
    uint8_t progressiveMinimumLevel = 255;
    uint8_t progressiveResidentLevel = 255;
    SbVec3f progressiveQuantizationMinimum;
    SbVec3f progressiveQuantizationMaximum;

    /**
     * Producer-certified identity of one append-only flat-segment stream.
     *
     * A nonzero value promises that later immutable WireRep records carrying
     * the same token retain every preceding segmentPoints value as an exact
     * prefix.  Renderers may preserve that GPU prefix and upload only the
     * newly resident suffix.  Independent per-level curve approximations and
     * other non-prefix representations must leave this value zero.
     */
    uint64_t progressiveLineage = 0;

    bool isProgressive() const noexcept {
        return progressiveResidentLevel < 16;
    }

    size_t segmentCountAtLevel(uint8_t level) const noexcept {
        if (!isProgressive()) return segmentCount();
        level = (std::max)(progressiveMinimumLevel,
                         (std::min)(progressiveResidentLevel, level));
        const size_t first = (std::min<size_t>)(
            progressiveSegmentFirst[level], segmentCount());
        return (std::min<size_t>)(progressiveSegmentCount[level],
                                  segmentCount() - first);
    }

    size_t segmentFirstAtLevel(uint8_t level) const noexcept {
        if (!isProgressive()) return 0;
        level = (std::max)(progressiveMinimumLevel,
                         (std::min)(progressiveResidentLevel, level));
        return (std::min<size_t>)(progressiveSegmentFirst[level],
                                  segmentCount());
    }
};

/**
 * @brief Optional shaded triangle mesh for a part.
 *
 * normals may be empty; in that case flat normals are computed at render time.
 * When present, normals has one entry per position and is addressed by the
 * same triangle indices as positions.
 */
struct TriMesh {
    std::vector<SbVec3f>  positions;
    std::vector<SbVec3f>  normals;    ///< optional; empty or positions.size()
    std::vector<uint32_t> indices;    ///< triangle list (3 indices per tri)
    SbBox3f               bounds;
    /** Cumulative triangle index counts for retained PoP levels 0..15. */
    std::array<uint32_t, 16> progressiveIndexCount = {};
    /**
     * Cumulative position counts addressed by each retained PoP index
     * prefix.  Producers should compute this once while constructing the
     * geometry; renderers use it to avoid rescanning the index prefix on
     * every frame.
     */
    std::array<uint32_t, 16> progressivePositionCount = {};
    uint8_t progressiveMinimumLevel = 255;
    uint8_t progressiveResidentLevel = 255;
    SbVec3f progressiveQuantizationMinimum;
    SbVec3f progressiveQuantizationMaximum;

    /**
     * Producer-certified identity of one append-only progressive stream.
     *
     * A nonzero value promises that every later immutable PartGeometry with
     * the same token preserves all position, normal, and index values in its
     * preceding cumulative prefix.  Renderers may therefore retain an
     * already uploaded prefix across PartGeometry generation changes and
     * upload only the newly resident suffix.  Zero makes no such promise and
     * retains the conservative full-replacement behavior.
     *
     * Producers must allocate a different token whenever topology, authored
     * values, vertex splitting, activation order, progressive level tables,
     * or the quantization domain changes.  The token is process-local
     * identity, not serialized asset or cache identity.
     */
    uint64_t progressiveLineage = 0;

    bool isProgressive() const noexcept {
        return progressiveResidentLevel < 16;
    }

    size_t indexCountAtLevel(uint8_t level) const noexcept {
        if (!isProgressive()) return indices.size();
        level = (std::max)(progressiveMinimumLevel,
                         (std::min)(progressiveResidentLevel, level));
        return std::min<size_t>(progressiveIndexCount[level],
                                indices.size());
    }

    size_t positionCountAtLevel(uint8_t level) const noexcept {
        if (!isProgressive()) return positions.size();
        level = (std::max)(progressiveMinimumLevel,
                         (std::min)(progressiveResidentLevel, level));
        return std::min<size_t>(progressivePositionCount[level],
                                positions.size());
    }
};

/**
 * @brief Collection of point primitives representing a part.
 *
 * Optional attribute arrays are either empty or parallel to @c positions.
 * Scales are model-space characteristic radii used by bounds and export;
 * interactive raster size remains an instance presentation property.
 */
struct PointRep {
    std::vector<SbVec3f> positions;
    std::vector<uint32_t> pointIds;
    std::vector<uint8_t> colorValid;
    std::vector<SbColor> colors;
    std::vector<uint8_t> scaleValid;
    std::vector<float> scales;
    std::vector<uint8_t> normalValid;
    std::vector<SbVec3f> normals;
    SbBox3f bounds;
};

/**
 * @brief Combined geometry payload for a single part.
 *
 * Any channel may be absent:
 * - @c points : needed for point rendering and point picking.
 * - @c wire : needed for wireframe rendering and edge picking.
 * - @c shaded : needed for shaded rendering and triangle picking.
 *
 * A part with neither channel is non-renderable but still participates in
 * hierarchy / bounds queries.
 */
struct PartGeometry {
    std::optional<PointRep> points; ///< Point primitives and optional attributes
    std::optional<WireRep> wire;    ///< Feature edges (no tessellation needed)
    std::optional<TriMesh> shaded;  ///< Optional triangle mesh for shading

    /**
     * Optional producer-supplied local-space extent for geometry which is not
     * yet renderable, or whose retained channels are only a partial
     * presentation of the source.  When present, this bound is combined with
     * all channel bounds and therefore must conservatively enclose the source
     * represented by this part.
     *
     * An empty part without this value has empty bounds.  SoCADAssembly never
     * invents placeholder geometry at the origin.
     */
    std::optional<SbBox3f> conservativeBounds;

    /**
     * The shaded triangles form a verified closed, manifold, consistently
     * oriented surface whose exterior winding has been normalized to
     * counter-clockwise.  The renderer may cull back faces only when this
     * guarantee is true and the occurrence transform preserves orientation.
     *
     * This is deliberately producer-supplied rather than inferred by the
     * renderer: validating topology belongs in geometry preparation and may
     * be persisted with an LoD asset.  Open, unoriented, transparent, or
     * otherwise unverified meshes must leave this false.
     */
    bool shadedCullBackfaces = false;

    /**
     * This presentation may be replaced by one depth-tested point when its
     * complete producer-validated bounds are subpixel.  The immutable
     * geometry remains available for bounds queries, picking, and immediate
     * promotion when the view makes it significant again.  Producers should
     * enable this for conservative LoD boxes and retained view-LoD meshes,
     * not for arbitrary annotations whose authored strokes must remain
     * individually visible.
     */
    bool subpixelProxyEligible = false;

    /**
     * This geometry is a temporary structural AABB/OBB presentation rather
     * than authored wire geometry or a retained mesh LoD.  Producers must
     * set this explicitly: channel shape alone is not enough to distinguish
     * a box from a legitimate wire-only mesh.  The renderer uses the marker
     * only for presentation accounting and diagnostics; subpixel aggregation
     * remains controlled independently by @c subpixelProxyEligible.
     */
    bool structuralProxy = false;
};

// ---------------------------------------------------------------------------
// Per-instance data
// ---------------------------------------------------------------------------

/**
 * @brief Visual style overrides applied to a single instance.
 */
struct InstanceStyle {
    bool     hasColorOverride = false;
    SbColor4f color           = SbColor4f(0.8f, 0.8f, 0.8f, 1.0f);
    float    lineWidth        = 1.0f;
    uint16_t linePattern      = 0xffffu;
    uint16_t linePatternFactor = 1u;
};

/**
 * @brief Full record describing an instance in the assembly.
 *
 * When no stable per-node GUID is available (the common case in BRL-CAD
 * comb trees), the InstanceId is generated deterministically from
 * (parent, childName, occurrenceIndex, boolOp) via CadIdBuilder.
 */
struct InstanceRecord {
    PartId        part;
    SbMatrix      localToRoot;    ///< Transform placing the part at its world position

    // --- ID generation inputs (ignored when using upsertInstance) ---
    InstanceId    parent;          ///< Parent InstanceId (Root() at top level)
    std::string   childName;       ///< Name string from the comb tree node
    uint32_t      occurrenceIndex = 0; ///< Sibling disambiguator (0-based)
    uint8_t       boolOp          = 0; ///< Boolean operation (0=union,1=sub,2=inter)
    /** Per-occurrence retained PoP draw cut.  255 selects full resident data. */
    uint8_t       lodLevel        = 255;

    InstanceStyle style;
};

/**
 * @brief Bulk part update record.
 */
struct PartUpdate {
    PartId       part;
    PartGeometry geometry;
};

/** Bulk update retaining immutable geometry owned by the producer. */
struct SharedPartUpdate {
    PartId part;
    std::shared_ptr<const PartGeometry> geometry;
    /**
     * Producer hint for immutable-generation replacement.  When both the
     * preceding and replacement geometry have exactly the same conservative
     * local bounds, SoCADAssembly may retain occurrence world bounds and the
     * instance BVH instead of revisiting every instance of this part.  The
     * assembly validates the invariant and falls back to ordinary bounds
     * recomputation if it is not satisfied.
     */
    bool preservesBounds = false;
};

/**
 * @brief Bulk instance update record.
 */
struct InstanceUpdate {
    InstanceId     instance;
    InstanceRecord record;
};

struct InstanceLodUpdate {
    InstanceId instance;
    uint8_t lodLevel = 255;
};

/**
 * @brief Bulk presentation-only instance update record.
 */
struct InstanceStyleUpdate {
    InstanceId    instance;
    InstanceStyle style;
};

/**
 * @brief Domain-neutral pick hit record produced by SoCADAssembly.
 *
 * Applications that need richer pick details can subclass SoCADAssembly and
 * override createPickDetail() to translate this stable CAD hit identity into
 * application-specific detail data.
 */
struct CadPickDetailRecord {
    enum PrimitiveKind {
        EDGE     = 0,
        TRIANGLE = 1,
        BOUNDS   = 2,
        POINT    = 3,
    };

    InstanceId    instance;
    PartId        part;
    SbVec3f       point         = SbVec3f(0.0f, 0.0f, 0.0f);
    PrimitiveKind primitiveKind = BOUNDS;
    uint32_t      primIndex0    = 0;
    uint32_t      primIndex1    = 0;
    float         u             = 0.0f;
};

} // namespace Obol

// ---------------------------------------------------------------------------
// SoCADAssembly node
// ---------------------------------------------------------------------------

struct SoCADAssemblyImpl;

/*!
  \class SoCADAssembly SoCADAssembly.h obol/cad/SoCADAssembly.h
  \brief Compiled CAD assembly node for scalable large-scene rendering.

  \ingroup obol_cad

  SoCADAssembly renders up to millions of part instances efficiently by
  bypassing per-node scene-graph traversal and instead building a GPU-ready
  frame plan (transform buffer + indirect draw commands).

  See SoCADAssembly.h for a complete usage example.

  \sa SoCADDetail, Obol::CadIdBuilder, Obol::PartGeometry
*/
class OBOL_DLL_API SoCADAssembly : public SoNode {
    typedef SoNode inherited;
    SO_NODE_HEADER(SoCADAssembly);

public:
    // -----------------------------------------------------------------------
    // Inventor-style fields
    // -----------------------------------------------------------------------

    /** Rendering mode. */
    enum DrawMode {
        SHADED           = 0,  ///< Shaded triangles only
        WIREFRAME        = 1,  ///< Wireframe segments/polylines only
        SHADED_WITH_EDGES = 2, ///< Shaded triangles + wire overlay
        HIDDEN_LINE      = 3,  ///< Triangle depth prepass + visible wire edges
    };

    /** Picking mode. */
    enum PickMode {
        PICK_AUTO     = 0, ///< Automatically select based on drawMode
        PICK_EDGE     = 1, ///< Always use edge/wire picking
        PICK_TRIANGLE = 2, ///< Always use triangle picking
        PICK_BOUNDS   = 3, ///< Use bounding-box proxy only
        PICK_HYBRID   = 4, ///< Try triangles; fall back to edges then bounds
    };

    SoSFEnum  drawMode;             ///< Default: WIREFRAME
    SoSFEnum  pickMode;             ///< Default: PICK_AUTO
    SoSFFloat edgePickTolerancePx;  ///< Screen-space edge pick tolerance (pixels)
    SoSFBool  wireframeOcclusion;   ///< Run depth-only triangle pass in wireframe mode
    /** Interactive render-only PoP ceiling, or -1 when disabled.
     *
     * This does not mutate producer-authored per-instance cuts or rebuild the
     * frame plan.  It lets a view controller shed already-retained draw work
     * in O(1) at interaction start while its precise per-instance allocator
     * catches up. */
    SoSFInt32 progressiveLodCeiling;
    /** Screen-space size below which eligible occurrences enter one point
     * batch.  The default 1 px is pixel-exact; an interactive controller may
     * temporarily raise it to its measured screen-error tolerance.  Geometry
     * and instance records remain retained and are restored in place when the
     * threshold returns to 1 px. */
    SoSFFloat pointProxyPixelThreshold;
    /** Reuse the last camera-dependent submission during interactive camera
     * motion.
     *
     * This is a presentation hint, not a visibility contract.  The renderer
     * Callers must clear it for the first quiet frame or when a coverage pass
     * discovers missing newly visible geometry.  During zoom the retained cut
     * may temporarily overdraw or omit newly exposed peripheral occurrences,
     * but it provides an immediate coherent frame while view admission
     * catches up. */
    SoSFBool cameraMotionFrameReuse;

    // -----------------------------------------------------------------------
    // Class registration
    // -----------------------------------------------------------------------

    static void initClass();
    SoCADAssembly();

    // -----------------------------------------------------------------------
    // Update framing (optional; batch multiple inserts for efficiency)
    // -----------------------------------------------------------------------

    /** Begin a batch update.  Defers internal rebuilds until endUpdate(). */
    void beginUpdate();

    /** End a batch update and rebuild acceleration structures as needed. */
    void endUpdate();

    /**
     * Reserve retained and streamed presentation storage for a known
     * occurrence population.
     *
     * This is a capacity hint only: it does not create instances, invalidate
     * the current plan, or notify scene auditors.  Supplying a manifest count
     * before the first streamed update prevents unordered-table rehashes and
     * large vector relocations from landing in later GUI render frames.
     */
    void reserveStreamingCapacity(size_t expectedOccurrences);

    /** Remove all parts, instances, selection and hidden-state records. */
    void clear();

    // -----------------------------------------------------------------------
    // Part library
    // -----------------------------------------------------------------------

    /**
     * Insert or replace the geometry for a part.
     * @param pid  Stable part identifier (use CadIdBuilder::hash128 to create).
     * @param geom Part geometry (wire and/or shaded).
     */
    void upsertPart(Obol::PartId pid, const Obol::PartGeometry& geom);

    /**
     * Insert or replace many parts as one dirty operation.
     *
     * This avoids per-part scene notifications and recomputes bounds for
     * affected instances once after all geometry updates have landed.
     */
    void upsertParts(const std::vector<Obol::PartUpdate>& updates);

    /** Retain producer-owned immutable part geometry without copying it. */
    void upsertSharedParts(const std::vector<Obol::SharedPartUpdate>& updates);

    /**
     * Remove a part.  Any instances referencing this part become non-renderable
     * (they remain in the instance database so they can be re-attached if the
     * part is re-inserted later).
     */
    void removePart(Obol::PartId pid);

    /**
     * Remove many parts as one dirty operation.
     *
     * Instance bounds are recomputed in one pass after all removals.  Use
     * this for a scene-wide LoD cut change; calling removePart() thousands
     * of times would otherwise rescan the complete instance population once
     * per retired part.
     */
    void removeParts(const std::vector<Obol::PartId>& pids);

    // -----------------------------------------------------------------------
    // Instance management
    // -----------------------------------------------------------------------

    /**
     * Insert or update an instance, generating its InstanceId automatically
     * from (parent, childName, occurrenceIndex, boolOp) in @p rec.
     *
     * @return The generated InstanceId (stable within the session as long as
     *         the same traversal path is used).
     */
    Obol::InstanceId upsertInstanceAuto(const Obol::InstanceRecord& rec);

    /**
     * Insert or update an instance with an explicitly-supplied InstanceId.
     * Use this when you already have a stable external identifier.
     */
    void upsertInstance(Obol::InstanceId iid, const Obol::InstanceRecord& rec);

    /**
     * Insert or update many automatically-identified instances.
     *
     * @return Generated InstanceIds, in the same order as @p records.
     */
    std::vector<Obol::InstanceId> upsertInstancesAuto(
        const std::vector<Obol::InstanceRecord>& records);

    /**
     * Insert or update many explicitly-identified instances as one dirty
     * operation.
     */
    void upsertInstances(const std::vector<Obol::InstanceUpdate>& updates);

    /** Update only retained progressive draw cuts. */
    void updateInstanceLodLevels(
        const std::vector<Obol::InstanceLodUpdate>& updates);

    /** Remove an instance.  No-op if @p iid is not in the database. */
    void removeInstance(Obol::InstanceId iid);

    /** Fast path: update only the transform for an existing instance. */
    void updateInstanceTransform(Obol::InstanceId iid, const SbMatrix& localToRoot);

    /** Fast path: update only the visual style for an existing instance. */
    void updateInstanceStyle(Obol::InstanceId iid, const Obol::InstanceStyle& style);

    /** Update many visual styles without rebuilding bounds or the pick BVH. */
    void updateInstanceStyles(
        const std::vector<Obol::InstanceStyleUpdate>& updates);

    /** Replace the selection highlight set. */
    void setSelectedInstances(const std::vector<Obol::InstanceId>& ids);

    // -----------------------------------------------------------------------
    // Query
    // -----------------------------------------------------------------------

    /** Number of instances currently in the database. */
    size_t instanceCount() const;

    /** Number of parts currently in the part library. */
    size_t partCount() const;

    /** Number of retained instances in the selected presentation set. */
    size_t selectedInstanceCount() const;

    /**
     * Return stable instance IDs in deterministic order.
     *
     * Renderer-neutral backends use this together with getInstanceRecord()
     * and partGeometry() to consume the retained assembly without rebuilding
     * a per-instance scene graph.
     */
    std::vector<Obol::InstanceId> instanceIds() const;

    /** True when an instance is hidden from rendering and generic traversal. */
    bool isInstanceHidden(Obol::InstanceId iid) const;

    /** True when any retained part contains producer-authored PoP prefixes. */
    bool hasProgressivePartLod() const;

    /** Apply the render-only progressive ceiling to one requested level. */
    uint8_t effectiveProgressiveLodLevel(uint8_t requested) const;

    /**
     * Return the geometry for @p pid, or nullptr if not in the part library.
     * Used by the GPU renderer to upload per-part VBOs.
     */
    const Obol::PartGeometry* partGeometry(Obol::PartId pid) const;

    /**
     * Return the full instance record for @p iid, or empty if not found.
     *
     * Useful for "materialising" a picked instance into a normal scene-graph
     * node: retrieve part, transform and style, then build an explicit shape.
     */
    std::optional<Obol::InstanceRecord> getInstanceRecord(Obol::InstanceId iid) const;

    /**
     * Exclude a set of instances from rendering.
     *
     * Hidden instances are skipped during GLRender() but retained in the
     * compiled frame plan.  Changing this set is therefore a sparse
     * presentation update: existing part bindings, LoD payloads, atlas
     * allocations, and unrelated instance records remain intact.  Instances
     * can be shown again by passing an updated (smaller) set.
     *
     * Typical use: after promoting selected/edited instances to explicit
     * scene-graph nodes, hide the corresponding aggregate entries so they
     * don't double-render.
     */
    void setHiddenInstances(const std::vector<Obol::InstanceId>& ids);

    /**
     * Exclude a set of instances from picking while keeping them visible.
     *
     * This is the compiled-assembly equivalent of Inventor pick style
     * suppression: instances remain in render and bounds plans, but the pick
     * BVH ignores them.  Use this for view/application state such as
     * "visible but not selectable" without promoting the instance to a full
     * scene-graph node.
     */
    void setUnpickableInstances(const std::vector<Obol::InstanceId>& ids);

    /**
     * Returns the rendering tier selected during the last GLRender() call:
     *   -1 = not yet rendered
     *    0 = immediate-mode fallback (GL 1.1 fixed-function, no working GLSL+VBO)
     *    1 = retained VBO loop (fixed-function compatibility or GLSL)
     *    2 = instanced (GL 3.1+, one draw call per unique part)
     *    3 = flattened wire batch (hardware GL, one draw per style)
     */
    int lastRenderTier() const;

    /** Diagnostic status of the last retained indirect-shaded attempt. */
    int lastIndirectStatus() const;

    /** Triangles actually submitted by the last shaded rendering pass. */
    uint64_t lastRenderedTriangleCount() const;

    /** Exact logical work for the last completed rendering pass.
     * Renderers publish this at their actual draw sites, including shaded
     * triangles and wire segments.  @c exact is false when rendering was
     * interrupted or the selected tier could not publish a complete record.
     */
    Obol::CadRenderedWork lastRenderedWork() const;

    /** Duration and triangle count from the newest completed asynchronous
     * GPU timer sample.  A zero serial means timer queries are unavailable or
     * no result has completed yet. */
    uint64_t lastGpuRenderNanoseconds() const;
    uint64_t lastGpuRenderedTriangleCount() const;
    /** Point-aggregation threshold paired with the newest completed GPU
     * timer submission. */
    float lastGpuPointProxyPixelThreshold() const;
    uint64_t gpuTimerSampleSerial() const;

    /** Last complete-frame snapshot of renderer-owned GPU buffer resources. */
    Obol::CadGpuResourceSnapshot gpuResourceSnapshot() const;

    /** True when the last retained indirect pass replayed an already prepared
     * camera-dependent frame rather than rebuilding its submission record. */
    bool lastRenderUsedPreparedReplay() const;

    /** True when the last render used the direct software wire rasterizer. */
    bool lastRenderUsedDirectSoftwareWire() const;

    /** Monotonic token advanced immediately before CAD drawing begins.
     * A deadline-bounded host may compare this value around a traversal to
     * distinguish resumable presentation preparation from rendering load. */
    uint64_t renderExecutionSerial() const;

    /** Monotonic token advanced by non-steady presentation work: assembly
     * plan/classifier preparation, retained renderer record construction, or
     * geometry upload.  Comparing this around a deadline-bounded traversal
     * prevents one-time preparation cost from being learned as steady draw
     * capacity. */
    uint64_t renderPreparationSerial() const;

    /** Number of LoD proxy occurrences rendered as subpixel points last frame. */
    size_t lastSubpixelProxyCount() const;

    /**
     * Number of structural proxy occurrences which remained visible as
     * wire boxes after camera-local subpixel collapse last frame.
     */
    size_t lastUncollapsedStructuralProxyCount() const;

    /** Revision of the last camera-dependent subpixel proxy presentation. */
    uint64_t lastSubpixelProxyRevision() const;

    /** Number of full retained frame-plan compilations performed. */
    uint64_t framePlanBuildCount() const;

    /**
     * Number of source occurrence records in the retained frame plan.
     *
     * This diagnostic distinguishes logical instances from append-only
     * presentation records while profiling progressive streams.
     */
    size_t framePlanInstanceRecordCount() const;

protected:
    ~SoCADAssembly() override;

    /**
     * Create the detail stored on SoPickedPoint for ray picks.
     *
     * The default implementation returns an SoCADDetail.  Subclasses can
     * override this to expose richer application details while still using
     * SoCADAssembly's accelerated picking implementation.
     */
    virtual SoDetail* createPickDetail(
        const Obol::CadPickDetailRecord& hit) const;

    // -----------------------------------------------------------------------
    // Inventor action overrides
    // -----------------------------------------------------------------------

    void GLRender         (SoGLRenderAction*         action) override;
    void rayPick          (SoRayPickAction*           action) override;
    void getBoundingBox   (SoGetBoundingBoxAction*    action) override;
    void getPrimitiveCount(SoGetPrimitiveCountAction* action) override;

private:
    std::unique_ptr<SoCADAssemblyImpl> impl_;
};

#endif // OBOL_SOCADASSEMBLY_H
