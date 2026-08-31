/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES ARE DISCLAIMED.
\**************************************************************************/

#ifndef OBOL_CAD_ASSEMBLY_STATE_H
#define OBOL_CAD_ASSEMBLY_STATE_H

#include "CadFramePlan.h"
#include "CadRendererGL.h"
#include "picking/CadPicking.h"

#include <Obol/cad/SoCADAssembly.h>

#include <Inventor/SbBox3f.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbVec2s.h>

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Obol {
namespace internal {

struct CadAssemblyInstanceData {
    Obol::PartId partId;
    SbMatrix localToRoot;
    Obol::InstanceStyle style;
    Obol::InstanceId parent;
    std::string childName;
    uint32_t occurrenceIndex = 0;
    uint8_t boolOp = 0;
    uint8_t lodCut = Obol::ProgressiveCutUnspecified;
    bool lodStructuralProxy = false;
    SbBox3f worldBounds;
};

/*
 * Authoritative retained parts and occurrences.  The one-instance/part case
 * stays inline; shared assets allocate a secondary vector only when needed.
 */
struct CadSceneDatabase {
    struct InstancePartBucket {
        Obol::InstanceId first;
        CadAssemblyInstanceData *firstData = nullptr;
        bool hasFirst = false;
        std::vector<Obol::InstanceId> additional;
        std::vector<CadAssemblyInstanceData *> additionalData;

        size_t size() const noexcept {
            return hasFirst ? additional.size() + 1u : 0u;
        }
        Obol::InstanceId at(size_t slot) const {
            return slot ? additional[slot - 1u] : first;
        }
        CadAssemblyInstanceData *dataAt(size_t slot) const {
            return slot ? additionalData[slot - 1u] : firstData;
        }
    };

    std::unordered_map<Obol::PartId,
        std::shared_ptr<const Obol::PartGeometry>,
        std::hash<Obol::PartId>> parts_;
    std::unordered_map<Obol::InstanceId, CadAssemblyInstanceData,
        std::hash<Obol::InstanceId>> instances_;
    std::map<Obol::PartId, InstancePartBucket> instanceIdsByPart_;
    std::unordered_map<Obol::InstanceId, size_t,
        std::hash<Obol::InstanceId>> instancePartSlot_;
    std::unordered_set<Obol::InstanceId,
        std::hash<Obol::InstanceId>> selected_;
    std::unordered_set<Obol::InstanceId,
        std::hash<Obol::InstanceId>> hidden_;
    std::unordered_set<Obol::InstanceId,
        std::hash<Obol::InstanceId>> unpickable_;
    std::unordered_set<Obol::InstanceId,
        std::hash<Obol::InstanceId>> pointProxyProtected_;
    std::unordered_set<Obol::PartId,
        std::hash<Obol::PartId>> progressiveParts_;
    std::unordered_map<Obol::PartId, uint64_t,
        std::hash<Obol::PartId>> partGeneration_;
    uint64_t nextGeneration_ = 1;

    /*
     * The generic aggregate swap is expressed in terms of move assignment.
     * MSVC consequently cannot prove that it is non-throwing, despite every
     * retained container using an always-equal default allocator.  Exchange
     * the containers directly so complete-scene publication remains a
     * constant-time, allocation-free commit on every supported STL.
     */
    friend void swap(CadSceneDatabase& left,
                     CadSceneDatabase& right) noexcept
    {
        left.parts_.swap(right.parts_);
        left.instances_.swap(right.instances_);
        left.instanceIdsByPart_.swap(right.instanceIdsByPart_);
        left.instancePartSlot_.swap(right.instancePartSlot_);
        left.selected_.swap(right.selected_);
        left.hidden_.swap(right.hidden_);
        left.unpickable_.swap(right.unpickable_);
        left.pointProxyProtected_.swap(right.pointProxyProtected_);
        left.progressiveParts_.swap(right.progressiveParts_);
        left.partGeneration_.swap(right.partGeneration_);
        using std::swap;
        swap(left.nextGeneration_, right.nextGeneration_);
    }
};

struct CadPickingIndex {
    Obol::picking::CadInstanceBVH instanceBvh_;
    bool bvhDirty_ = true;
    std::unordered_map<Obol::PartId, Obol::picking::CadPartEdgeBVH,
        std::hash<Obol::PartId>> partEdgeBvhCache_;
    std::unordered_map<Obol::PartId, Obol::picking::CadPartTriBVH,
        std::hash<Obol::PartId>> partTriBvhCache_;
};

/*
 * Retained frame-plan and sparse-journal state.  Plan compilation and replay
 * remain non-virtual direct operations; this type is an ownership boundary,
 * not an abstraction layer in the hot path.
 */
struct CadPlanCache {
    static constexpr size_t ProgressiveCutBinCount =
        Obol::ProgressiveCutLimit + 1;

    struct ProgressiveShadedPlanGroup {
        Obol::PartId part;
        uint32_t baseInstance = 0;
        uint32_t instanceCount = 0;
        std::array<uint32_t, ProgressiveCutBinCount> cutCounts = {};
        size_t shadedItemBegin = 0;
        size_t shadedItemCount = 0;
    };

    struct CachedPlanPartSpan {
        uint32_t partIndex = 0;
        uint32_t baseInstance = 0;
        uint32_t instanceCount = 0;
        size_t wireItemBegin = 0;
        size_t wireItemCount = 0;
        size_t pointItemBegin = 0;
        size_t pointItemCount = 0;
        size_t shadedItemBegin = 0;
        size_t shadedItemCount = 0;
    };

    Obol::internal::CadFramePlan cachedPlan_;
    std::vector<ProgressiveShadedPlanGroup> progressiveShadedPlanGroups_;
    std::unordered_map<Obol::InstanceId, size_t,
        std::hash<Obol::InstanceId>>
        progressiveShadedPlanGroupByInstance_;
    std::unordered_map<Obol::InstanceId, uint32_t,
        std::hash<Obol::InstanceId>> progressivePlanIndexByInstance_;
    std::unordered_map<Obol::PartId, std::vector<CachedPlanPartSpan>,
        std::hash<Obol::PartId>> cachedPlanPartSpansByPart_;
    uint64_t nextPlanRevision_ = 1;
    uint64_t nextGeometryRevision_ = 1;
    uint64_t nextShadedLayoutRevision_ = 1;
    uint64_t nextShadedLodRevision_ = 1;
    uint64_t nextAppendRevision_ = 1;
    uint64_t nextPartGeometryRevision_ = 1;
    uint64_t nextInstanceAttributeRevision_ = 1;
    uint64_t geometryRevision_ = 0;
    bool planDirty_ = true;
    bool geometryDirty_ = true;
    std::optional<Obol::CadDrawMode> cachedDrawMode_;

    int cachedDrawModeDiagnostic() const noexcept
    {
        return cachedDrawMode_ ? static_cast<int>(*cachedDrawMode_) : -1;
    }

    bool cachedDrawModeHasWire() const noexcept
    {
        return cachedDrawMode_ &&
            Obol::cadDrawModeHasWire(*cachedDrawMode_);
    }

    bool cachedDrawModeHasShaded() const noexcept
    {
        return cachedDrawMode_ &&
            Obol::cadDrawModeHasShaded(*cachedDrawMode_);
    }
    const char *planDirtyReason_ = "initial";
    uint64_t framePlanBuildCount_ = 0;
    uint64_t progressivePlanPatchFailureCount_ = 0;
    uint64_t planDebugBuildMessageCount_ = 0;
    uint64_t planDebugPatchMessageCount_ = 0;
    size_t cachedPlanTombstoneCount_ = 0;
    bool streamTombstoneCompactionPerformed_ = false;
    std::vector<uint32_t> pendingInstanceAttributeIndices_;
};

struct CadSubpixelClassifier {
    std::unordered_map<Obol::PartId, std::array<SbVec3f, 8>,
        std::hash<Obol::PartId>> subpixelProxyCorners_;
    uint64_t pointProxyProtectionRevision_ = 1;
    uint64_t classifiedPointProxyProtectionRevision_ = 0;
    uint64_t nextSubpixelProxyRevision_ = 1;
    uint64_t nextSubpixelProxyInputRevision_ = 1;
    uint64_t subpixelProxyStateInputRevision_ = 0;
    uint64_t subpixelProxyViewInputRevision_ = 0;
    uint64_t subpixelProxyViewId_ = 0;
    SbMatrix subpixelProxyViewProj_;
    SbVec2s subpixelProxyViewportSize_ = SbVec2s(0, 0);
    float subpixelProxyPixelThreshold_ = 1.0f;
    bool subpixelProxyViewValid_ = false;
    uint32_t subpixelProxyCameraMotionReuseCount_ = 0;
    std::vector<uint8_t> subpixelProxyState_;
    std::vector<uint8_t> subpixelProxyScratchMask_;
    std::vector<Obol::internal::CadSubpixelProxyPoint>
        subpixelProxyScratchPoints_;
    std::vector<uint32_t> subpixelProxyVisibleByPoint_;
    std::vector<uint32_t> subpixelProxyScratchVisibleByPoint_;
    std::vector<uint32_t> subpixelProxyPointByVisible_;
    std::vector<uint32_t> subpixelProxyScratchPointByVisible_;
    /* Lowest 1/2/4/8/16/32/64-pixel cumulative bucket for each structural
     * occurrence.  -1 is not a visible structural fallback; BucketCount is a
     * visible occurrence which is clipped, protected, or larger than 64 px. */
    std::vector<int8_t> structuralProjectionBucketByVisible_;
    std::vector<int8_t> structuralProjectionScratchBucketByVisible_;
    Obol::CadStructuralProxyProjectionHistogram
        structuralProjectionHistogram_;
    Obol::CadStructuralProxyProjectionHistogram
        structuralProjectionScratchHistogram_;
    uint64_t nextStructuralProjectionRevision_ = 1;
    /* A sparse selection change may promote/demote a classified occurrence
     * without invalidating the camera-local classification for the rest of
     * the assembly.  Publish those edits with one point-stream revision at
     * the end of the presentation transaction. */
    bool pendingSubpixelProxyChange_ = false;
    /*
     * A camera-local classification may exceed one presentation deadline in
     * a scene with hundreds of thousands of occurrences.  Retain the exact
     * scratch cursor across retries instead of restarting the O(N) scan on
     * every frame.  These values are valid only while all recorded inputs
     * still match the cached plan and current view.
     */
    bool subpixelProxyBuildActive_ = false;
    uint64_t subpixelProxyBuildInputRevision_ = 0;
    uint64_t subpixelProxyBuildAppendRevision_ = 0;
    uint64_t subpixelProxyBuildViewId_ = 0;
    SbMatrix subpixelProxyBuildViewProj_;
    SbVec2s subpixelProxyBuildViewportSize_ = SbVec2s(0, 0);
    float subpixelProxyBuildPixelThreshold_ = 1.0f;
    size_t subpixelProxyBuildVisibleCursor_ = 0;
    size_t subpixelProxyBuildWireItemCursor_ = 0;
    uint32_t subpixelProxyBuildWireOffset_ = 0;
    bool subpixelProxyBuildWireHasUncollapsed_ = false;
    size_t subpixelProxyBuildWireStructuralCount_ = 0;
    uint64_t subpixelProxyBuildTotalUnits_ = 0;
    uint64_t subpixelProxyBuildCompletedWireUnits_ = 0;
    /* Dense plan-indexed scratch keeps the preparation reservation exact;
     * node-based hash allocation is neither portable nor tightly bounded. */
    std::vector<uint8_t> subpixelProxyScratchWireByPart_;
    std::vector<size_t> subpixelProxyScratchStructuralCountByPart_;
    size_t subpixelProxyScratchStructuralCount_ = 0;
    std::vector<Obol::InstanceId>
        subpixelProxyScratchStructuralInstances_;
    uint64_t subpixelProxyClassifiedAppendRevision_ = 0;
    std::unordered_map<Obol::PartId, size_t, std::hash<Obol::PartId>>
        uncollapsedStructuralProxyCountByPart_;
    size_t uncollapsedStructuralProxyCount_ = 0;
    std::unordered_set<Obol::InstanceId, std::hash<Obol::InstanceId>>
        uncollapsedStructuralProxyInstances_;
};

struct CadRendererState {
    /*
     * A retained renderer owns mutable frame-planning scratch, per-context
     * GPU tables and last-frame diagnostics.  The scene-graph contract lets
     * independent actions traverse the same read-only node concurrently, so
     * serialize the complete renderer transaction for one assembly.  Separate
     * assemblies remain fully concurrent.
     */
    std::mutex renderMutex_;
    std::unique_ptr<Obol::internal::CadRendererGL> renderer_;
    bool lastDirectSoftwareWire_ = false;
    /* Increment immediately before retained/direct CAD drawing begins.  A
     * host compares this token around a deadline-bounded traversal to
     * distinguish one-time plan/classifier preparation from an expensive
     * draw attempt. */
    uint64_t renderExecutionSerial_ = 0;
    /* Increment whenever GLRender builds or advances an assembly-owned frame
     * plan/classification.  SoCADAssembly::renderPreparationSerial() combines
     * this with renderer-owned record/upload preparation. */
    uint64_t renderPreparationSerial_ = 0;
    Obol::CadPresentationPreparationSnapshot presentationPreparation_;
    uint64_t nextPresentationPreparationRevision_ = 1;
};

} // namespace internal
} // namespace Obol

#endif // OBOL_CAD_ASSEMBLY_STATE_H
