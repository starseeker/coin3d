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
 * - LoD (Level of Detail) is applied at render time via POP-like quantisation.
 * - The node renders entirely within its GLRender() override; it does NOT
 *   walk children.
 *
 * ### Usage example
 * @code
 *   SoCADAssembly* asm = new SoCADAssembly;
 *   asm->drawMode = SoCADAssembly::WIREFRAME;
 *
 *   // Add a part with wire geometry
 *   obol::PartId pid = obol::CadIdBuilder::hash128("wheel");
 *   obol::PartGeometry geom;
 *   geom.wire = obol::WireRep{ ... };
 *   asm->upsertPart(pid, geom);
 *
 *   // Add an instance
 *   obol::InstanceRecord rec;
 *   rec.part   = pid;
 *   rec.parent = obol::CadIdBuilder::Root();
 *   rec.localToRoot.makeIdentity();
 *   obol::InstanceId iid = asm->upsertInstanceAuto(rec);
 *
 *   root->addChild(asm);
 * @endcode
 */

#include <Inventor/nodes/SoSubNode.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/fields/SoSFEnum.h>
#include <Inventor/fields/SoSFBool.h>
#include <Inventor/fields/SoSFFloat.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbColor4f.h>
#include <Inventor/SbBox3f.h>
#include <Inventor/SbVec3f.h>

#include <obol/cad/CadIds.h>

#include <vector>
#include <optional>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

class SoDetail;

namespace obol {

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
     * This wire representation is a conservative LoD proxy rather than
     * authored geometry.  Renderers may replace it with a single depth-tested
     * point when its complete projected extent is subpixel.  The original
     * geometry remains available for bounds queries and picking.
     */
    bool subpixelProxyEligible = false;
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
};

/**
 * @brief Bulk instance update record.
 */
struct InstanceUpdate {
    InstanceId     instance;
    InstanceRecord record;
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

} // namespace obol

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

  \sa SoCADDetail, obol::CadIdBuilder, obol::PartGeometry
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
    void upsertPart(obol::PartId pid, const obol::PartGeometry& geom);

    /**
     * Insert or replace many parts as one dirty operation.
     *
     * This avoids per-part scene notifications and recomputes bounds for
     * affected instances once after all geometry updates have landed.
     */
    void upsertParts(const std::vector<obol::PartUpdate>& updates);

    /** Retain producer-owned immutable part geometry without copying it. */
    void upsertSharedParts(const std::vector<obol::SharedPartUpdate>& updates);

    /**
     * Remove a part.  Any instances referencing this part become non-renderable
     * (they remain in the instance database so they can be re-attached if the
     * part is re-inserted later).
     */
    void removePart(obol::PartId pid);

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
    obol::InstanceId upsertInstanceAuto(const obol::InstanceRecord& rec);

    /**
     * Insert or update an instance with an explicitly-supplied InstanceId.
     * Use this when you already have a stable external identifier.
     */
    void upsertInstance(obol::InstanceId iid, const obol::InstanceRecord& rec);

    /**
     * Insert or update many automatically-identified instances.
     *
     * @return Generated InstanceIds, in the same order as @p records.
     */
    std::vector<obol::InstanceId> upsertInstancesAuto(
        const std::vector<obol::InstanceRecord>& records);

    /**
     * Insert or update many explicitly-identified instances as one dirty
     * operation.
     */
    void upsertInstances(const std::vector<obol::InstanceUpdate>& updates);

    /** Remove an instance.  No-op if @p iid is not in the database. */
    void removeInstance(obol::InstanceId iid);

    /** Fast path: update only the transform for an existing instance. */
    void updateInstanceTransform(obol::InstanceId iid, const SbMatrix& localToRoot);

    /** Fast path: update only the visual style for an existing instance. */
    void updateInstanceStyle(obol::InstanceId iid, const obol::InstanceStyle& style);

    /** Update many visual styles without rebuilding bounds or the pick BVH. */
    void updateInstanceStyles(
        const std::vector<obol::InstanceStyleUpdate>& updates);

    /** Replace the selection highlight set. */
    void setSelectedInstances(const std::vector<obol::InstanceId>& ids);

    // -----------------------------------------------------------------------
    // Query
    // -----------------------------------------------------------------------

    /** Number of instances currently in the database. */
    size_t instanceCount() const;

    /** Number of parts currently in the part library. */
    size_t partCount() const;

    /**
     * Return stable instance IDs in deterministic order.
     *
     * Renderer-neutral backends use this together with getInstanceRecord()
     * and partGeometry() to consume the retained assembly without rebuilding
     * a per-instance scene graph.
     */
    std::vector<obol::InstanceId> instanceIds() const;

    /** True when an instance is hidden from rendering and generic traversal. */
    bool isInstanceHidden(obol::InstanceId iid) const;

    /** True when any part can provide progressive triangle LoD. */
    bool hasPartLod() const;

    /**
     * Return the geometry for @p pid, or nullptr if not in the part library.
     * Used by the GPU renderer to upload per-part VBOs.
     */
    const obol::PartGeometry* partGeometry(obol::PartId pid) const;

    /**
     * Return the full instance record for @p iid, or empty if not found.
     *
     * Useful for "materialising" a picked instance into a normal scene-graph
     * node: retrieve part, transform and style, then build an explicit shape.
     */
    std::optional<obol::InstanceRecord> getInstanceRecord(obol::InstanceId iid) const;

    /**
     * Return LoD-filtered triangle indices for @p pid at the given @p level.
     *
     * Builds the part's LoD structure on first demand.  Returns nullptr when
     * no shaded geometry is available for the part.
     * The returned pointer is stable until the next geometry change for
     * that part (i.e., until the next upsertPart/removePart call).
     *
     * Used internally by the renderer when LoD is enabled by SoCADViewState.
     */
    const std::vector<uint32_t>* getLodFilteredIndices(obol::PartId pid,
                                                        uint8_t level) const;

    /**
     * Exclude a set of instances from rendering.
     *
     * Hidden instances are completely skipped during GLRender() and are not
     * included in the frame plan.  They remain in the instance database and
     * can be shown again by passing an updated (smaller) set.
     *
     * Typical use: after promoting selected/edited instances to explicit
     * scene-graph nodes, hide the corresponding aggregate entries so they
     * don't double-render.
     */
    void setHiddenInstances(const std::vector<obol::InstanceId>& ids);

    /**
     * Exclude a set of instances from picking while keeping them visible.
     *
     * This is the compiled-assembly equivalent of Inventor pick style
     * suppression: instances remain in render and bounds plans, but the pick
     * BVH ignores them.  Use this for view/application state such as
     * "visible but not selectable" without promoting the instance to a full
     * scene-graph node.
     */
    void setUnpickableInstances(const std::vector<obol::InstanceId>& ids);

    /**
     * Returns the rendering tier selected during the last GLRender() call:
     *   -1 = not yet rendered
     *    0 = immediate-mode fallback (GL 1.1 fixed-function, no working GLSL+VBO)
     *    1 = retained VBO loop (fixed-function compatibility or GLSL)
     *    2 = instanced (GL 3.1+, one draw call per unique part)
     *    3 = flattened wire batch (hardware GL, one draw per style)
     */
    int lastRenderTier() const;

    /** True when the last render used the direct software wire rasterizer. */
    bool lastRenderUsedDirectSoftwareWire() const;

    /** Number of LoD proxy occurrences rendered as subpixel points last frame. */
    size_t lastSubpixelProxyCount() const;

    /** Revision of the last camera-dependent subpixel proxy presentation. */
    uint64_t lastSubpixelProxyRevision() const;

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
        const obol::CadPickDetailRecord& hit) const;

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
