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

#ifndef OBOL_CAD_ASSEMBLY_IMPL_H
#define OBOL_CAD_ASSEMBLY_IMPL_H

#include <Obol/cad/SoCADAssembly.h>
#include <Obol/cad/CadProjectedProxy.h>
#include <Obol/cad/SoCADViewState.h>

#include "CadAssemblyState.h"
#include "CadFramePlan.h"

#include <Inventor/actions/SoGLRenderAction.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

bool cadDebugEnabled();
bool cadLightDebugEnabled();
bool cadPlanDebugEnabled();
uint64_t cadPlanDebugMessageLimit();

using InstanceData = Obol::internal::CadAssemblyInstanceData;

enum class CadProxyPresentation : uint8_t {
    Geometry = 0u,
    Point = 1u,
    Box = 2u,
    Offscreen = 3u
};

struct CadStructuralProjectionSample {
    bool structural = false;
    bool visible = false;
    bool collapsible = false;
    float maximumPixels = 0.0f;
};

struct SoCADAssemblyImpl :
    Obol::internal::CadSceneDatabase,
    Obol::internal::CadPickingIndex,
    Obol::internal::CadPlanCache,
    Obol::internal::CadSubpixelClassifier,
    Obol::internal::CadRendererState
{

    /* Transaction framing and producer hints describe the live assembly,
     * not the retained scene payload.  Keeping them outside CadSceneDatabase
     * prevents complete-scene publication from swapping away an open update
     * scope or a capacity hint. */
    size_t updateDepth_ = 0;
    size_t streamingOccurrenceCapacityHint_ = 0;

    // Rebuild instance BVH if dirty
    void rebuildBvhIfNeeded();

    static SbBox3f partGeometryBounds(
            const Obol::PartGeometry& geom);

    static bool partGeometryBoundsEqual(
            const Obol::PartGeometry& left,
            const Obol::PartGeometry& right);

    // Compute world bounds for an instance from part geometry
    SbBox3f computeWorldBounds(const Obol::PartGeometry& geom,
                               const SbMatrix& m) const ;

    void updatePartGeometry(
            Obol::PartId pid,
            const std::shared_ptr<const Obol::PartGeometry>& geom);

    void recomputeWorldBoundsForPart(Obol::PartId pid);

    void recomputeWorldBoundsForParts(
        const std::unordered_set<Obol::PartId,
                                 std::hash<Obol::PartId>>& pids);

    void removeInstanceFromPartIndex(
            Obol::InstanceId iid, Obol::PartId pid);

    void addInstanceToPartIndex(
            Obol::InstanceId iid, Obol::PartId pid,
            InstanceData *idata);

    void updateKnownInstance(Obol::InstanceId iid,
                             const Obol::InstanceRecord& rec,
                             InstanceData *prior);

    void updateInstance(Obol::InstanceId iid,
                        const Obol::InstanceRecord& rec);

    void markDirty(const char *reason = "geometry-or-structure");

    static size_t progressiveCutBin(uint8_t cut);

    bool patchCachedInstanceFlags(Obol::InstanceId instance);

    bool patchCachedInstanceStyle(Obol::InstanceId instance);

    void finishSparsePresentationPatch(bool visibilityChanged = false);

    /**
     * Build a CadFramePlan from the current instance and part databases.
     *
     * Groups instances by part to maximise batching in the renderer.
     * The instance list is sorted by part so each CadDrawItem covers a
     * contiguous run of visibleInstances.
     *
     * @param dm       Draw mode (WIREFRAME / SHADED / SHADED_WITH_EDGES).
     * @param selected Set of selected instance IDs (for colour override).
     * @param hidden   Set of hidden instance IDs (flagged in the plan).
     */
    Obol::internal::CadFramePlan buildFramePlan(
            Obol::CadDrawMode dm,
            const std::unordered_set<Obol::InstanceId,
                                     std::hash<Obol::InstanceId>>& selected,
            const std::unordered_set<Obol::InstanceId,
                                     std::hash<Obol::InstanceId>>& hidden,
            const std::map<Obol::PartId, InstancePartBucket> *buckets =
                nullptr,
            SoGLRenderAction *renderAction = nullptr) const;

    bool rebuildProgressiveShadedPlanIndex(
            SoGLRenderAction *renderAction = nullptr);

    static bool partGeometryPlanCompatible(
            const Obol::PartGeometry& oldGeometry,
            const Obol::PartGeometry& newGeometry);

    /*
     * Append a streamed batch without moving the compiled prefix.  A batch
     * may contain new occurrences and box-to-mesh presentation replacements.
     * Replacements tombstone their old presentation record and redirect the
     * stable InstanceId to a new tail record.  This also handles a shared
     * structural-box part: its draw range stays intact while the replaced
     * occurrence is discarded by the normal hidden-instance path.
     *
     * A later draw-mode change or explicit compaction naturally removes the
     * tombstones and restores global part order.
     */
    bool appendCachedInstances(
            const std::vector<Obol::InstanceId>& instanceIds,
            bool allowReplacements = false);

    /*
     * Replace the part behind one uniquely owned presentation slot without
     * recompiling the complete assembly.
     *
     * Cold progressive population commonly changes a leaf from its structural
     * box part to a newly arrived mesh part.  In a large vehicle most such
     * parts have one occurrence.  The old implementation treated every wave
     * as arbitrary topology and rebuilt the growing N-entry plan, making
     * streamed realization quadratic.  A unique slot can instead retain its
     * visible-instance index and part-binding index, tombstone its old draw
     * items, and append the new channel items.  The next draw-mode transition
     * or genuinely shared-part edit performs a normal compact rebuild.
     */
    bool canPatchCachedInstancePartRebind(
            Obol::InstanceId instance, Obol::PartId oldPart) const ;

    bool patchCachedInstancePartRebind(
            Obol::InstanceId instance, Obol::PartId oldPart,
            bool prevalidated = false);

    /*
     * Apply a wave of independent unique-slot presentation rebinds without
     * appending duplicate visible-instance records.  Validate the complete
     * wave before mutating the cached plan so a shared structural range or a
     * repeated target part can fall back to the append representation
     * atomically.
     */
    bool patchCachedInstancePartRebinds(
            const std::vector<std::pair<Obol::InstanceId, Obol::PartId>>&
                rebinds);

    void finishSparseStructuralPatch();

    /*
     * Replace the immutable arrays behind an existing retained part without
     * recompiling the assembly-wide instance plan.  Progressive population
     * growth changes a part generation and its resident prefix capacity, but
     * not the occurrence topology, styles, or level buckets.
     */
    bool patchCachedPartGeometry(
            Obol::PartId part,
            Obol::internal::CadPartGeometryDelta& geometryDelta,
            bool preservesBounds = false);

    void finishPartGeometryPatch(
            Obol::internal::CadPartGeometryDelta geometryDelta);

    bool patchCachedInstanceCut(
            Obol::InstanceId instance, uint8_t lodCut,
            std::unordered_set<size_t>& changedGroups);

    bool patchProgressiveShadedPlanCut(
            Obol::InstanceId instance, uint8_t lodCut,
            std::unordered_set<size_t>& changedGroups);

    void finishProgressiveShadedPlanPatch(
            const std::unordered_set<size_t>& changedGroups);

    CadProxyPresentation subpixelProxyPresentationForOccurrence(
            const Obol::internal::CadFramePlan& plan,
            size_t visibleIndex,
            const SbMatrix& viewProj,
            const SbVec2s& viewportSize,
            float pixelThreshold,
            CadProxyPresentation previousPresentation,
            Obol::internal::CadSubpixelProxyPoint& replacement,
            CadStructuralProjectionSample *structuralSample = nullptr) const;

    void updateStructuralProjectionForVisible(size_t visibleIndex);

    bool patchSubpixelProxyGeometryForVisible(
            size_t visibleIndex, uint64_t priorInputRevision);

    /* Keep selected geometry visually inspectable even when its conservative
     * bounds are below the ordinary small-part threshold.  Selection is a
     * sparse presentation property, so promote/demote just this occurrence
     * and preserve the camera-local classification for every other record. */
    bool updateProtectedSubpixelProxy(
            size_t visibleIndex, bool protectedInstance);

    void refreshWireProxyParts(
            const std::unordered_set<Obol::PartId,
                                     std::hash<Obol::PartId>>& parts);

    bool patchSubpixelProxyAppendPlan(
            const SbMatrix& viewProj,
            const SbVec2s& viewportSize,
            float pixelThreshold);

    /*
     * Preserve an unpublished classifier cursor while a cold source appends
     * more occurrences.  The ordinary append patch above operates on a
     * complete, published classification.  Before the first publication,
     * repeatedly invalidating the scratch scan made a fast producer able to
     * keep a large assembly at boxes forever: every render revisited the
     * prefix and the next append discarded it.
     *
     * Pure tail growth is safe to fold into the active transaction.  A
     * replacement may alter a record the cursor has already visited, so it
     * deliberately takes the conservative reset path instead.
     */
    bool extendSubpixelProxyAppendBuild(
            uint64_t viewId, const SbMatrix& viewProj,
            const SbVec2s& viewportSize,
            float pixelThreshold);

    Obol::CadPresentationPreparationTarget subpixelPreparationTarget(
            uint64_t viewId, const SbMatrix& viewProj,
            const SbVec2s& viewportSize,
            float pixelThreshold);

    uint64_t subpixelPreparationReservedBytes() const;

    uint64_t subpixelPreparationCompletedUnits() const;

    void publishSubpixelPreparation(
            const Obol::CadPresentationPreparationTarget& target,
            Obol::CadPresentationPreparationState state,
            uint64_t completedUnits);

    bool updateSubpixelProxyPlan(uint64_t viewId,
                                 const SbMatrix& viewProj,
                                 const SbVec2s& viewportSize,
                                 float pixelThreshold,
                                 bool cameraMotionReuse,
                                 SoGLRenderAction *renderAction = nullptr,
                                 bool *preparationPerformed = nullptr);
};

#endif // OBOL_CAD_ASSEMBLY_IMPL_H
