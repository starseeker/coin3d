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
#include <string>
#include <unordered_map>
#include <unordered_set>
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
    bool inUpdate_ = false;
    size_t streamingOccurrenceCapacityHint_ = 0;
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
    int cachedDM_ = -1;
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
    uint64_t nextSubpixelProxyRevision_ = 1;
    uint64_t nextSubpixelProxyInputRevision_ = 1;
    uint64_t subpixelProxyStateInputRevision_ = 0;
    uint64_t subpixelProxyViewInputRevision_ = 0;
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
    SbMatrix subpixelProxyBuildViewProj_;
    SbVec2s subpixelProxyBuildViewportSize_ = SbVec2s(0, 0);
    float subpixelProxyBuildPixelThreshold_ = 1.0f;
    size_t subpixelProxyBuildVisibleCursor_ = 0;
    size_t subpixelProxyBuildWireItemCursor_ = 0;
    uint32_t subpixelProxyBuildWireOffset_ = 0;
    bool subpixelProxyBuildWireHasUncollapsed_ = false;
    size_t subpixelProxyBuildWireStructuralCount_ = 0;
    std::unordered_set<Obol::PartId, std::hash<Obol::PartId>>
        subpixelProxyScratchWireParts_;
    std::unordered_map<Obol::PartId, size_t, std::hash<Obol::PartId>>
        subpixelProxyScratchStructuralCountByPart_;
    size_t subpixelProxyScratchStructuralCount_ = 0;
    uint64_t subpixelProxyClassifiedAppendRevision_ = 0;
    std::unordered_map<Obol::PartId, size_t, std::hash<Obol::PartId>>
        uncollapsedStructuralProxyCountByPart_;
    size_t uncollapsedStructuralProxyCount_ = 0;
};

struct CadRendererState {
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
};

} // namespace internal
} // namespace Obol

#endif // OBOL_CAD_ASSEMBLY_STATE_H
