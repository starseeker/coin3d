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
 * - Wire polylines are the primary wireframe primitive; triangle meshes are
 *   optional and used for shaded rendering.
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
#include <cstdint>
#include <memory>
#include <string>

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
 * @brief Collection of polylines representing the wireframe of a part.
 */
struct WireRep {
    std::vector<WirePolyline> polylines;
    /** Tight axis-aligned bounding box enclosing all polylines. */
    SbBox3f bounds;
};

/**
 * @brief Optional shaded triangle mesh for a part.
 *
 * normals may be empty; in that case flat normals are computed at render time.
 */
struct TriMesh {
    std::vector<SbVec3f>  positions;
    std::vector<SbVec3f>  normals;    ///< optional; may be empty
    std::vector<uint32_t> indices;    ///< triangle list (3 indices per tri)
    SbBox3f               bounds;
};

/**
 * @brief Combined geometry payload for a single part.
 *
 * Either or both channels may be absent:
 * - @c wire : needed for wireframe rendering and edge picking.
 * - @c shaded : needed for shaded rendering and triangle picking.
 *
 * A part with neither channel is non-renderable but still participates in
 * hierarchy / bounds queries.
 */
struct PartGeometry {
    std::optional<WireRep> wire;    ///< Feature-edge polylines (no tessellation needed)
    std::optional<TriMesh> shaded;  ///< Optional triangle mesh for shading
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

} // namespace obol

// ---------------------------------------------------------------------------
// SoCADAssembly node
// ---------------------------------------------------------------------------

class SoCADAssemblyImpl;

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
        WIREFRAME        = 1,  ///< Wireframe polylines only
        SHADED_WITH_EDGES = 2, ///< Shaded triangles + wire overlay
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
    SoSFBool  lodEnabled;           ///< Apply POP LoD to triangle meshes (default: FALSE)

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

    /** Remove an instance.  No-op if @p iid is not in the database. */
    void removeInstance(obol::InstanceId iid);

    /** Fast path: update only the transform for an existing instance. */
    void updateInstanceTransform(obol::InstanceId iid, const SbMatrix& localToRoot);

    /** Fast path: update only the visual style for an existing instance. */
    void updateInstanceStyle(obol::InstanceId iid, const obol::InstanceStyle& style);

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
     * Returns nullptr when no LoD structure is available for the part.
     * The returned pointer is stable until the next geometry change for
     * that part (i.e., until the next upsertPart/removePart call).
     *
     * Used internally by the renderer when @c lodEnabled is TRUE.
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
     * Returns the rendering tier selected during the last GLRender() call:
     *   -1 = not yet rendered
     *    0 = immediate-mode fallback (GL 1.1 fixed-function, no working GLSL+VBO)
     *    1 = VBO-loop (GL 2.0, GLSL 1.10)
     *    2 = instanced (GL 3.1+, one draw call per unique part)
     */
    int lastRenderTier() const;

protected:
    ~SoCADAssembly() override;

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
