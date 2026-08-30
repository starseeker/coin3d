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
 *   SoCADViewState* view = new SoCADViewState;
 *   view->drawMode = SoCADViewState::WIREFRAME;
 *   SoCADAssembly* asm = new SoCADAssembly;
 *
 *   // Admit immutable geometry before the owner-thread transaction.
 *   Obol::PartId pid = Obol::CadIdBuilder::partId("wheel");
 *   Obol::PartGeometryBuilder geom;
 *   geom.wire = Obol::WireRep{ ... };
 *   Obol::CadGeometryAdmission admitted = Obol::cadAdmitPartGeometry(
 *       std::move(geom));
 *   if (!admitted) { ... }
 *
 *   // Add an instance
 *   Obol::InstanceRecord rec;
 *   rec.part   = pid;
 *   rec.parent = Obol::CadIdBuilder::rootInstance();
 *   rec.localToRoot.makeIdentity();
 *   const Obol::InstanceId iid = Obol::CadIdBuilder::childInstance(
 *       rec.parent, "wheel", 0, 0);
 *   Obol::CadSceneMutation mutation;
 *   mutation.parts.push_back({pid, admitted.geometry});
 *   mutation.instances.push_back({iid, rec});
 *   if (!asm->applySceneMutation(mutation)) { ... }
 *
 *   root->addChild(view);
 *   root->addChild(asm);
 * @endcode
 */

#include <Inventor/nodes/SoSubNode.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbColor4f.h>
#include <Inventor/SbBox3f.h>
#include <Inventor/SbVec3f.h>

#include <Obol/cad/CadIds.h>
#include <Obol/cad/CadGeometry.h>
#include <Obol/cad/CadFrameReport.h>
#include <Obol/cad/CadGpuResourceSnapshot.h>
#include <Obol/cad/CadPresentationPreparation.h>
#include <Obol/cad/CadProgressive.h>
#include <Obol/cad/CadSceneRecords.h>
#include <Obol/cad/CadSceneMutation.h>
#include <Obol/cad/CadSceneReplacement.h>
#include <Obol/cad/CadSceneValidation.h>
#include <Obol/cad/CadViewState.h>

#include <vector>
#include <unordered_set>
#include <algorithm>
#include <optional>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

class SoDetail;

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
    /**
     * Move-only update window returned by batchUpdate().
     *
     * Destruction completes the batch, including every early-return path.
     * Nested windows are supported and notify the scene only when the
     * outermost window closes.
     */
    class OBOL_DLL_API UpdateScope {
    public:
        UpdateScope(UpdateScope&& other) noexcept;
        UpdateScope& operator=(UpdateScope&& other) noexcept;
        ~UpdateScope();

        UpdateScope(const UpdateScope&) = delete;
        UpdateScope& operator=(const UpdateScope&) = delete;

        void finish() noexcept;

    private:
        explicit UpdateScope(SoCADAssembly *assembly) noexcept;

        SoCADAssembly *assembly_ = nullptr;

        friend class SoCADAssembly;
    };

    // -----------------------------------------------------------------------
    // Class registration
    // -----------------------------------------------------------------------

    static void initClass();
    SoCADAssembly();

    // -----------------------------------------------------------------------
    // Update framing (optional; batch multiple inserts for efficiency)
    // -----------------------------------------------------------------------

    /** Defer scene notification until the returned scope is completed. */
    UpdateScope batchUpdate();

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

    /**
     * Replace all retained parts and occurrences as one checked operation.
     *
     * Both complete batches are validated before the preceding scene is
     * changed.  A rejection therefore leaves the assembly untouched.  Use
     * this operation for source-of-truth rebuilds; use the sparse mutation
     * transaction below for progressive publication and editing.
     */
    [[nodiscard]] Obol::CadSceneReplacementResult replaceScene(
        const std::vector<Obol::PartUpdate>& parts,
        const std::vector<Obol::InstanceUpdate>& instances);

    /**
     * Validate a sparse transaction against the current retained scene.
     * This function has no side effects and performs work proportional to the
     * mutation, not to the retained scene population.
     */
    [[nodiscard]] Obol::CadSceneMutationResult validateSceneMutation(
        const Obol::CadSceneMutation& mutation) const;

    /**
     * Validate and apply a sparse transaction under one update window.
     * Validation rejection or allocation failure leaves the preceding scene
     * unchanged.  Allocation failure reports ResourceUnavailable.
     */
    [[nodiscard]] Obol::CadSceneMutationResult applySceneMutation(
        const Obol::CadSceneMutation& mutation);

    // -----------------------------------------------------------------------
    // Narrow convenience operations
    // -----------------------------------------------------------------------

    /**
     * The operations below are useful for isolated single-domain changes and
     * interactive fast paths.  When correctness depends on two or more
     * records changing together, use applySceneMutation() instead of composing
     * these calls: an independently rejected call intentionally does not roll
     * back a preceding successful one.
     */

    /**
     * Retain producer-owned immutable part geometry without copying it.
     * Shared geometry must first be admitted with cadAdmitPartGeometry(); the
     * presentation update itself performs no array-sized validation work.
     */
    Obol::CadGeometryValidation upsertParts(
        const std::vector<Obol::PartUpdate>& updates);

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
     * @return Validation and the generated stable identifier.  The identifier
     *         is invalid when validation fails.
     */
    Obol::CadInstanceUpdateResult upsertInstanceAuto(
        const Obol::InstanceRecord& rec);

    /**
     * Insert or update an instance with an explicitly-supplied InstanceId.
     * Use this when you already have a stable external identifier.
     */
    Obol::CadSceneValidation upsertInstance(
        Obol::InstanceId iid, const Obol::InstanceRecord& rec);

    /**
     * Insert or update many automatically-identified instances.
     *
     * @return Validation and generated identifiers in record order.  The
     *         identifier vector is empty when validation fails.
     */
    Obol::CadInstanceBatchResult upsertInstancesAuto(
        const std::vector<Obol::InstanceRecord>& records);

    /**
     * Insert or update many explicitly-identified instances as one dirty
     * operation.
     */
    Obol::CadSceneValidation upsertInstances(
        const std::vector<Obol::InstanceUpdate>& updates);

    /** Update only retained progressive draw cuts. */
    Obol::CadSceneValidation updateInstanceCuts(
        const std::vector<Obol::InstanceLodUpdate>& updates);

    /** Remove an instance.  No-op if @p iid is not in the database. */
    void removeInstance(Obol::InstanceId iid);

    /** Fast path: update only the transform for an existing instance. */
    Obol::CadSceneValidation updateInstanceTransform(
        Obol::InstanceId iid, const SbMatrix& localToRoot);

    /** Fast path: update only the visual style for an existing instance. */
    Obol::CadSceneValidation updateInstanceStyle(
        Obol::InstanceId iid, const Obol::InstanceStyle& style);

    /** Update many visual styles without rebuilding bounds or the pick BVH. */
    Obol::CadSceneValidation updateInstanceStyles(
        const std::vector<Obol::InstanceStyleUpdate>& updates);

    /** Replace the selection highlight set. */
    void setSelectedInstances(const std::vector<Obol::InstanceId>& ids);

    /**
     * Exclude screen-important occurrences from adaptive point aggregation.
     * This is view-local presentation policy: it changes neither retained
     * geometry nor the scene's semantic selection state.
     */
    void setPointProxyProtectedInstances(
        const std::vector<Obol::InstanceId>& ids);

    /**
     * Return the exact current point-protection set.  Stable-view planners
     * may compare this snapshot off the presentation commit path.
     */
    std::vector<Obol::InstanceId> pointProxyProtectedInstances() const;

    /**
     * Adopt an already validated protection set without rebuilding or
     * diffing it.  This is the transaction commit path for large CAD views:
     * the caller must have compared the complete set against the snapshot
     * above under its own scene-revision witness.  The existing frame plan and
     * GPU resources remain live while the bounded point classifier prepares
     * and atomically publishes the new presentation.
     */
    void adoptPointProxyProtectedInstances(
        std::unordered_set<Obol::InstanceId,
            std::hash<Obol::InstanceId>>&& ids);

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

    /** Finite exact-target certificate for retained frame preparation. */
    Obol::CadPresentationPreparationSnapshot
        presentationPreparationSnapshot() const;

    /** Number of logical occurrences represented by aggregate points or
     * boxes in the last completed frame. */
    size_t lastSubpixelProxyCount() const;

    /**
     * Number of physical point vertices submitted for the last CAD frame.
     * Software renderers may coalesce logical subpixel occurrences into a
     * bounded screen-bin stream; use lastSubpixelProxyCount() for semantic
     * occurrence coverage.
     */
    size_t lastSubpixelProxyDrawPointCount() const;

    /**
     * Number of in-frustum unresolved LoD-leaf fallback occurrences which
     * remained visible as wire boxes after camera-local subpixel collapse
     * last frame.  Fully clipped occurrences are not convergence obligations;
     * ordinary authored structural scene geometry is intentionally excluded.
     */
    size_t lastUncollapsedStructuralProxyCount() const;

    /** Stable instance identities corresponding to
     * lastUncollapsedStructuralProxyCount().  The list is derived from the
     * same last complete camera classification, excludes clipped/hidden and
     * point-collapsed occurrences, and is sorted for deterministic clients.
     * It lets a streaming owner repair the exact visible structural frontier
     * without loading every subpixel occurrence in a large assembly. */
    std::vector<Obol::InstanceId>
        lastUncollapsedStructuralProxyInstances() const;

    /** Stable instance identities for visible structural fallbacks whose
     * cached projected extent may exceed @p pixels.  The result is derived
     * from the same exact camera classification as
     * lastStructuralProxyProjectionHistogram() and is conservative between
     * its power-of-two bucket boundaries.  Clients may preload this sparse
     * set before lowering a point-proxy threshold, avoiding a box flash. */
    std::vector<Obol::InstanceId>
        lastStructuralProxyInstancesAbovePixels(float pixels) const;

    /** Camera-local projected-size census paired with the last complete
     * structural-proxy classification.  Bucket limits are documented by
     * CadStructuralProxyProjectionHistogram. */
    Obol::CadStructuralProxyProjectionHistogram
        lastStructuralProxyProjectionHistogram() const;

    /**
     * Exact physical presentation work for unresolved structural LoD
     * occurrences in the last complete camera classification.  Aggregate
     * points and boxes are emitted by the renderer's shared proxy batches;
     * retained wire boxes are the larger fallbacks still using their normal
     * part presentation.  Authored structural geometry is excluded.
     */
    Obol::CadStructuralProxyPresentationWork
        lastStructuralProxyPresentationWork() const;

    /** Exact logical point/AABB/OBB aggregate census for the last completed
     * frame, including ordinary view-local and atlas-pressure replacements. */
    Obol::CadAggregateProxyPresentationWork
        lastAggregateProxyPresentationWork() const;

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
    void beginUpdate();
    void endUpdate();

    std::unique_ptr<SoCADAssemblyImpl> impl_;
};

#endif // OBOL_SOCADASSEMBLY_H
