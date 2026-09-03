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
 * @file SoCADAssembly.cpp
 * @brief SoCADAssembly Inventor node – compiled CAD assembly renderer.
 *
 * Implementation notes:
 *  - GLRender: iterates visible instances, draws wire segments/polylines and
 *    optionally triangle meshes.
 *  - rayPick: delegates to CadPickQuery using the current pickMode.
 *  - getBoundingBox: returns the union of all instance world bounds.
 *  - The Pimpl (SoCADAssemblyImpl) holds the mutable instance/part databases
 *    and lazily-built acceleration structures.
 */

#include "CadAssemblyImpl.h"
#include "CadWireSource.h"
#include <Obol/cad/CadProjectedProxy.h>
#include <Obol/cad/SoCADDetail.h>
#include <Obol/cad/SoCADViewState.h>
#include "CadAssemblyState.h"
#include "CadFramePlan.h"
#include "CadIdentityCounter.h"
#include "CadRendererGL.h"
#include "CadSoftwareWire.h"
#include "picking/CadPicking.h"

#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/actions/SoRayPickAction.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/actions/SoGetPrimitiveCountAction.h>
#include <Inventor/SoPickedPoint.h>
#include <Inventor/SoFullPath.h>
#include <Inventor/SoDB.h>
#include <Inventor/SbLine.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbBox3f.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbViewVolume.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/elements/SoViewVolumeElement.h>
#include <Inventor/elements/SoViewportRegionElement.h>
#include <Inventor/elements/SoViewingMatrixElement.h>
#include <Inventor/elements/SoProjectionMatrixElement.h>
#include <Inventor/elements/SoContextManagerElement.h>
#include <Inventor/elements/SoGLLazyElement.h>
#include <Inventor/elements/SoLightElement.h>
#include <Inventor/elements/SoEnvironmentElement.h>
#include <Inventor/elements/SoClipPlaneElement.h>
#include <Inventor/elements/SoShapeHintsElement.h>
#include <Inventor/nodes/SoLight.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoPointLight.h>
#include <Inventor/nodes/SoSpotLight.h>
#include <Inventor/SbColor.h>

#include <Inventor/system/gl.h>
#include "rendering/SoGL.h"

#include <unordered_map>
#include <unordered_set>
#include <map>
#include <array>
#include <chrono>
#include <vector>
#include <memory>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <limits>
#include <type_traits>
#include <utility>

// ---------------------------------------------------------------------------
// SoCADAssemblyImpl – private implementation (Pimpl pattern)
// ---------------------------------------------------------------------------

bool
cadDebugEnabled()
{
    static const bool enabled = []() {
        const char *env = std::getenv("OBOL_CAD_DEBUG");
        return env && env[0] && env[0] != '0';
    }();
    return enabled;
}

bool
cadLightDebugEnabled()
{
    static const bool enabled = []() {
        const char *env = std::getenv("OBOL_CAD_LIGHT_DEBUG");
        return env && env[0] && env[0] != '0';
    }();
    return enabled;
}

bool
cadPlanDebugEnabled()
{
    static const bool enabled = []() {
        const char *env = std::getenv("OBOL_CAD_PLAN_DEBUG");
        return env && env[0] && env[0] != '0';
    }();
    return enabled;
}

uint64_t
cadPlanDebugMessageLimit()
{
    static const uint64_t limit = []() {
        const char *env = std::getenv("OBOL_CAD_PLAN_DEBUG_LIMIT");
        if (!env || !env[0])
            return uint64_t{64};
        char *end = nullptr;
        const unsigned long long value = std::strtoull(env, &end, 10);
        return end != env && value > 0 ?
            static_cast<uint64_t>(value) : uint64_t{64};
    }();
    return limit;
}

static bool
cadTransformPreservesOrientation(const std::array<float, 16>& transform)
{
    const double determinant =
        static_cast<double>(transform[0]) *
            (static_cast<double>(transform[5]) * transform[10] -
             static_cast<double>(transform[6]) * transform[9]) -
        static_cast<double>(transform[1]) *
            (static_cast<double>(transform[4]) * transform[10] -
             static_cast<double>(transform[6]) * transform[8]) +
        static_cast<double>(transform[2]) *
            (static_cast<double>(transform[4]) * transform[9] -
             static_cast<double>(transform[5]) * transform[8]);
    return std::isfinite(determinant) && determinant > 1.0e-12;
}

namespace {

static bool
cadSubpixelGeometryCorners(const Obol::PartGeometry& geometry,
                           std::array<SbVec3f, 8>& corners)
{
    return Obol::cadPartGeometryProxyCorners(geometry, corners.data());
}

static uint8_t
cadPackNormalizedColor(float component) noexcept
{
    /* Retained style validation supplies [0, 1] values.  Keep this conversion
     * defensive as it is the final boundary before floating-to-integer
     * conversion, which is undefined for an out-of-range destination. */
    if (!(component > 0.0f))
        return 0u;
    if (component >= 1.0f)
        return 255u;
    return static_cast<uint8_t>(component * 255.0f);
}

static std::array<uint8_t, 4>
cadPackInstanceStyleColor(const Obol::InstanceStyle& style) noexcept
{
    if (!style.hasColorOverride)
        return {{204u, 204u, 204u, 255u}};
    return {{
        cadPackNormalizedColor(style.color[0]),
        cadPackNormalizedColor(style.color[1]),
        cadPackNormalizedColor(style.color[2]),
        cadPackNormalizedColor(style.color[3])}};
}

} // namespace

using InstanceData = Obol::internal::CadAssemblyInstanceData;

// Rebuild instance BVH if dirty
void SoCADAssemblyImpl::rebuildBvhIfNeeded() {
        if (!bvhDirty_) return;
        std::vector<Obol::picking::CadInstanceBVH::Entry> entries;
        entries.reserve(instances_.size());
        for (const auto& [iid, idata] : instances_) {
            if (hidden_.count(iid) || unpickable_.count(iid))
                continue;
            Obol::picking::CadInstanceBVH::Entry e;
            e.worldBounds  = idata.worldBounds;
            e.instanceId   = iid;
            e.partId       = idata.partId;
            e.localToWorld = idata.localToRoot;
            e.lodCut     = idata.lodCut;
            entries.push_back(e);
        }
        instanceBvh_.build(std::move(entries));
        bvhDirty_ = false;
    }

SbBox3f SoCADAssemblyImpl::partGeometryBounds(
            const Obol::PartGeometry& geom) {
        SbBox3f local;
        local.makeEmpty();
        if (geom.conservativeBounds && !geom.conservativeBounds->isEmpty())
            local.extendBy(*geom.conservativeBounds);
        if (geom.points) { local.extendBy(geom.points->bounds); }
        if (geom.wire)   { local.extendBy(geom.wire->bounds);   }
        if (geom.shaded) { local.extendBy(geom.shaded->bounds); }
        return local;
    }

bool SoCADAssemblyImpl::partGeometryBoundsEqual(
            const Obol::PartGeometry& left,
            const Obol::PartGeometry& right) {
        const SbBox3f leftBounds = partGeometryBounds(left);
        const SbBox3f rightBounds = partGeometryBounds(right);
        if (leftBounds.isEmpty() || rightBounds.isEmpty())
            return leftBounds.isEmpty() && rightBounds.isEmpty();
        return leftBounds.getMin() == rightBounds.getMin() &&
            leftBounds.getMax() == rightBounds.getMax();
    }



    // Compute world bounds for an instance from part geometry
SbBox3f SoCADAssemblyImpl::computeWorldBounds(const Obol::PartGeometry& geom,
                               const SbMatrix& m) const {
        const SbBox3f local = partGeometryBounds(geom);
        if (local.isEmpty())
            return SbBox3f();
        // Transform all 8 corners
        SbBox3f world;
        world.makeEmpty();
        SbVec3f mn, mx;
        local.getBounds(mn, mx);
        const float xs[2] = {mn[0], mx[0]};
        const float ys[2] = {mn[1], mx[1]};
        const float zs[2] = {mn[2], mx[2]};
        for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2; ++j)
        for (int k = 0; k < 2; ++k) {
            SbVec3f corner(xs[i], ys[j], zs[k]);
            SbVec3f wc;
            m.multVecMatrix(corner, wc);
            world.extendBy(wc);
        }
        return world;
    }

void SoCADAssemblyImpl::updatePartGeometry(
            Obol::PartId pid,
            const std::shared_ptr<const Obol::PartGeometry>& geom) {
        if (!geom) return;
        const bool replacing = parts_.find(pid) != parts_.end();
        parts_[pid] = geom;
        if (geom->subpixelProxyEligible) {
            std::array<SbVec3f, 8> corners;
            if (cadSubpixelGeometryCorners(*geom, corners))
                subpixelProxyCorners_[pid] = std::move(corners);
            else if (replacing)
                subpixelProxyCorners_.erase(pid);
        } else if (replacing) {
            subpixelProxyCorners_.erase(pid);
        }
        partGeneration_[pid] =
            Obol::internal::cadTakeNonzeroIdentity(nextGeneration_);
        if (replacing) {
            erasePartBvhCaches(pid);
        }
        const bool progressive =
            (geom->shaded.has_value() && geom->shaded->isProgressive()) ||
            (geom->wire.has_value() &&
                Obol::internal::cadWireSourceIsProgressive(*geom->wire));
        if (progressive)
            progressiveParts_.insert(pid);
        else if (replacing)
            progressiveParts_.erase(pid);
    }

void SoCADAssemblyImpl::recomputeWorldBoundsForPart(Obol::PartId pid) {
        auto geomIt = parts_.find(pid);
        const auto indexed = instanceIdsByPart_.find(pid);
        if (indexed == instanceIdsByPart_.end())
            return;
        for (size_t slot = 0; slot < indexed->second.size(); ++slot) {
            InstanceData *idata = indexed->second.dataAt(slot);
            if (!idata)
                continue;
            idata->worldBounds = geomIt != parts_.end() && geomIt->second ?
                computeWorldBounds(*geomIt->second, idata->localToRoot) :
                SbBox3f();
        }
    }

void SoCADAssemblyImpl::recomputeWorldBoundsForParts(
        const std::unordered_set<Obol::PartId,
                                 std::hash<Obol::PartId>>& pids) {
        if (pids.empty())
            return;
        for (const Obol::PartId pid : pids) {
            const auto indexed = instanceIdsByPart_.find(pid);
            if (indexed == instanceIdsByPart_.end())
                continue;
            const auto geometry = parts_.find(pid);
            for (size_t slot = 0; slot < indexed->second.size(); ++slot) {
                InstanceData *idata = indexed->second.dataAt(slot);
                if (!idata)
                    continue;
                idata->worldBounds =
                    geometry != parts_.end() && geometry->second ?
                    computeWorldBounds(
                        *geometry->second, idata->localToRoot) :
                    SbBox3f();
            }
        }
    }

void SoCADAssemblyImpl::removeInstanceFromPartIndex(
            Obol::InstanceId iid, Obol::PartId pid,
            InstanceData *idata) {
        const auto indexed = instanceIdsByPart_.find(pid);
        if (indexed == instanceIdsByPart_.end() || !idata)
            return;
        InstancePartBucket& ids = indexed->second;
        size_t offset = idata->partSlot;
        if (offset >= ids.size() || !(ids.at(offset) == iid)) {
            offset = ids.size();
            for (size_t candidate = 0; candidate < ids.size(); ++candidate) {
                if (ids.at(candidate) == iid) {
                    offset = candidate;
                    break;
                }
            }
            if (offset == ids.size())
                return;
        }
        const size_t last = ids.size() - 1u;
        if (offset < last) {
            const Obol::InstanceId replacement = ids.at(last);
            InstanceData *replacementData = ids.dataAt(last);
            if (offset == 0)
                ids.first = replacement;
            else
                ids.additional[offset - 1u] = replacement;
            if (offset == 0)
                ids.firstData = replacementData;
            else
                ids.additionalData[offset - 1u] = replacementData;
            if (replacementData)
                replacementData->partSlot = offset;
        }
        if (last == 0) {
            ids.hasFirst = false;
            ids.firstData = nullptr;
        } else {
            ids.additional.pop_back();
            ids.additionalData.pop_back();
        }
        if (!ids.hasFirst)
            instanceIdsByPart_.erase(indexed);
    }

void SoCADAssemblyImpl::addInstanceToPartIndex(
            Obol::InstanceId iid, Obol::PartId pid,
            InstanceData *idata) {
        InstancePartBucket& ids = instanceIdsByPart_[pid];
        const size_t slot = ids.size();
        idata->partSlot = slot;
        if (!ids.hasFirst) {
            ids.first = iid;
            ids.firstData = idata;
            ids.hasFirst = true;
        } else {
            ids.additional.push_back(iid);
            ids.additionalData.push_back(idata);
        }
    }

void SoCADAssemblyImpl::updateKnownInstance(Obol::InstanceId iid,
                             const Obol::InstanceRecord& rec,
                             InstanceData *prior) {
        InstanceData *idata = prior;
        if (!prior) {
            const auto inserted =
                instances_.try_emplace(iid, InstanceData());
            idata = &inserted.first->second;
            addInstanceToPartIndex(iid, rec.part, idata);
        } else if (!(prior->partId == rec.part)) {
            removeInstanceFromPartIndex(iid, prior->partId, prior);
            addInstanceToPartIndex(iid, rec.part, prior);
        }
        idata->partId         = rec.part;
        idata->localToRoot    = rec.localToRoot;
        idata->style          = rec.style;
        idata->parent         = rec.parent;
        idata->childName      = rec.childName;
        idata->occurrenceIndex = rec.occurrenceIndex;
        idata->boolOp         = rec.boolOp;
        idata->lodCut       = rec.lodCut;
        idata->lodStructuralProxy = rec.lodStructuralProxy;

        auto geomIt = parts_.find(rec.part);
        idata->worldBounds = geomIt != parts_.end() && geomIt->second ?
            computeWorldBounds(*geomIt->second, rec.localToRoot) : SbBox3f();
    }

void SoCADAssemblyImpl::updateInstance(Obol::InstanceId iid,
                        const Obol::InstanceRecord& rec) {
        const auto prior = instances_.find(iid);
        updateKnownInstance(iid, rec,
            prior == instances_.end() ? nullptr : &prior->second);
    }

void SoCADAssemblyImpl::markDirty(const char *reason) {
        bvhDirty_ = true;
        planDirty_ = true;
        geometryDirty_ = true;
        planDirtyReason_ = reason;
        progressiveShadedPlanGroups_.clear();
        progressiveShadedPlanGroupByInstance_.clear();
        progressivePlanIndexByInstance_.clear();
        cachedPlanPartSpansByPart_.clear();
        pendingInstanceAttributeIndices_.clear();
    }

size_t SoCADAssemblyImpl::progressiveCutBin(uint8_t cut) {
        return cut < ProgressiveCutBinCount - 1 ?
            static_cast<size_t>(cut) : ProgressiveCutBinCount - 1;
    }

bool SoCADAssemblyImpl::patchCachedInstanceFlags(Obol::InstanceId instance) {
        const auto fail = [&](const char *reason) {
            if (cadPlanDebugEnabled() &&
                    planDebugPatchMessageCount_++ <
                        cadPlanDebugMessageLimit())
                std::fprintf(stderr,
                    "SoCADAssembly sparse instance-flag patch rejected "
                    "reason=%s instance=%016llx:%016llx "
                    "plan_dirty=%d geometry_dirty=%d draw_mode=%d "
                    "visible=%zu instances=%zu parts=%zu selected=%zu "
                    "hidden=%zu\n",
                    reason ? reason : "unknown",
                    static_cast<unsigned long long>(instance.w0),
                    static_cast<unsigned long long>(instance.w1),
                    planDirty_ ? 1 : 0, geometryDirty_ ? 1 : 0,
                    cachedDrawModeDiagnostic(),
                    cachedPlan_.visibleInstances.size(), instances_.size(),
                    parts_.size(), selected_.size(), hidden_.size());
            return false;
        };
        if (planDirty_ || geometryDirty_)
            return fail("plan-state");
        const auto indexFound =
            progressivePlanIndexByInstance_.find(instance);
        if (indexFound == progressivePlanIndexByInstance_.end()) {
            /*
             * A retained occurrence with no compiled presentation record has
             * no attribute stream entry to patch.  Keep its selected/hidden
             * state in the authoritative instance set; a later append or
             * rebuild will consume that state.  Rebuilding every visible
             * record because one currently unrenderable occurrence changed a
             * flag turns a sparse tree selection into an unbounded operation
             * in 150k-part scenes.
             */
            if (instances_.find(instance) != instances_.end())
                return true;
            return fail("instance-not-retained");
        }
        if (indexFound->second >= cachedPlan_.visibleInstances.size())
            return fail("invalid-visible-index");
        const auto instanceFound = instances_.find(instance);
        if (instanceFound == instances_.end())
            return fail("instance-not-retained");
        const uint32_t visibleIndex = indexFound->second;
        auto& record = cachedPlan_.visibleInstances[visibleIndex];
        const bool wasTransparent =
            !(record.flags & Obol::internal::CadInstanceHidden) &&
            record.rgba[3] < 255;
        const bool wasProxyProtected =
            (record.flags &
                Obol::internal::CadInstancePointProxyProtected) != 0;
        uint32_t flags = record.flags &
            ~(Obol::internal::CadInstanceSelected |
              Obol::internal::CadInstanceHidden |
              Obol::internal::CadInstancePointProxyProtected |
              Obol::internal::CadInstanceLodStructuralProxy);
        if (selected_.count(instance))
            flags |= Obol::internal::CadInstanceSelected;
        if (hidden_.count(instance))
            flags |= Obol::internal::CadInstanceHidden;
        if (pointProxyProtected_.count(instance))
            flags |= Obol::internal::CadInstancePointProxyProtected;
        if (instanceFound->second.lodStructuralProxy)
            flags |= Obol::internal::CadInstanceLodStructuralProxy;
        record.flags = flags;
        const bool isTransparent =
            !(record.flags & Obol::internal::CadInstanceHidden) &&
            record.rgba[3] < 255;
        if (wasTransparent != isTransparent) {
            if (isTransparent)
                ++cachedPlan_.transparentVisibleInstanceCount;
            else if (cachedPlan_.transparentVisibleInstanceCount)
                --cachedPlan_.transparentVisibleInstanceCount;
        }
        updateStructuralProjectionForVisible(visibleIndex);
        if (cadDebugEnabled()) {
            std::fprintf(stderr,
                "SoCADAssembly patch flags instance=%016llx:%016llx "
                "selected=%d hidden=%d flags=%u\n",
                static_cast<unsigned long long>(instance.w0),
                static_cast<unsigned long long>(instance.w1),
                selected_.count(instance) ? 1 : 0,
                hidden_.count(instance) ? 1 : 0,
                static_cast<unsigned>(record.flags));
        }
        const bool isProxyProtected =
            (record.flags &
                Obol::internal::CadInstancePointProxyProtected) != 0;
        if (wasProxyProtected != isProxyProtected &&
                updateProtectedSubpixelProxy(
                    visibleIndex, isProxyProtected))
            pendingSubpixelProxyChange_ = true;
        if (visibleIndex < subpixelProxyPointByVisible_.size()) {
            const uint32_t pointIndex =
                subpixelProxyPointByVisible_[visibleIndex];
            if (pointIndex != std::numeric_limits<uint32_t>::max() &&
                    pointIndex < cachedPlan_.subpixelProxyPoints.size()) {
                cachedPlan_.subpixelProxyPoints[pointIndex].flags =
                    record.flags;
            }
        }
        pendingInstanceAttributeIndices_.push_back(visibleIndex);
        return true;
    }

bool SoCADAssemblyImpl::patchCachedInstanceStyle(Obol::InstanceId instance) {
        if (planDirty_ || geometryDirty_ ||
                !cachedDrawModeHasShaded())
            return false;
        const auto indexFound =
            progressivePlanIndexByInstance_.find(instance);
        const auto instanceFound = instances_.find(instance);
        if (indexFound == progressivePlanIndexByInstance_.end() ||
                indexFound->second >= cachedPlan_.visibleInstances.size() ||
                instanceFound == instances_.end())
            return false;
        auto& record = cachedPlan_.visibleInstances[indexFound->second];
        const bool wasOpaque = record.rgba[3] == 255;
        const float oldLineWidth = record.lineWidth;
        const uint16_t oldLinePattern = record.linePattern;
        const uint16_t oldLinePatternFactor = record.linePatternFactor;
        const Obol::InstanceStyle& style = instanceFound->second.style;
        record.rgba = cadPackInstanceStyleColor(style);
        if (cadDebugEnabled()) {
            std::fprintf(stderr,
                "SoCADAssembly patch style instance=%016llx:%016llx "
                "rgba=(%u %u %u %u)\n",
                static_cast<unsigned long long>(instance.w0),
                static_cast<unsigned long long>(instance.w1),
                static_cast<unsigned>(record.rgba[0]),
                static_cast<unsigned>(record.rgba[1]),
                static_cast<unsigned>(record.rgba[2]),
                static_cast<unsigned>(record.rgba[3]));
        }
        record.lineWidth = std::max(1.0f, style.lineWidth);
        record.linePattern = style.linePattern;
        record.linePatternFactor =
            std::max<uint16_t>(1u, style.linePatternFactor);
        if (style.hasColorOverride)
            record.flags |= Obol::internal::CadInstanceColorOverride;
        else
            record.flags &= ~Obol::internal::CadInstanceColorOverride;
        /*
         * Opacity participates in shaded cull-run construction, while line
         * width and stipple participate in wire draw-run construction.  A
         * color-only style change is an instance-stream update in every draw
         * mode, but changes to either grouping property need a new plan.
         */
        if (wasOpaque != (record.rgba[3] == 255))
            return false;
        const bool hasWirePlan = cachedDrawModeHasWire();
        if (hasWirePlan &&
                (record.lineWidth != oldLineWidth ||
                 record.linePattern != oldLinePattern ||
                 record.linePatternFactor != oldLinePatternFactor))
            return false;
        const uint32_t visibleIndex = indexFound->second;
        if (visibleIndex < subpixelProxyPointByVisible_.size()) {
            const uint32_t pointIndex =
                subpixelProxyPointByVisible_[visibleIndex];
            if (pointIndex != std::numeric_limits<uint32_t>::max() &&
                    pointIndex < cachedPlan_.subpixelProxyPoints.size())
                cachedPlan_.subpixelProxyPoints[pointIndex].rgba =
                    record.rgba;
        }
        pendingInstanceAttributeIndices_.push_back(visibleIndex);
        return true;
    }

void SoCADAssemblyImpl::finishSparsePresentationPatch(bool visibilityChanged) {
        /* A partially classified point-proxy scratch result contains copied
         * flags.  Discard its cursor when a sparse attribute change lands so
         * the next bounded scan cannot publish stale selection/visibility. */
        subpixelProxyBuildActive_ = false;
        if (pendingSubpixelProxyChange_) {
            cachedPlan_.subpixelProxyRevision =
                Obol::internal::cadTakeNonzeroIdentity(
                    nextSubpixelProxyRevision_);
            pendingSubpixelProxyChange_ = false;
        }
        cachedPlan_.revision = Obol::internal::cadTakeNonzeroIdentity(
            nextPlanRevision_);
        cachedPlan_.instanceAttributeRevision =
            Obol::internal::cadTakeNonzeroIdentity(
                nextInstanceAttributeRevision_);
        std::sort(pendingInstanceAttributeIndices_.begin(),
            pendingInstanceAttributeIndices_.end());
        pendingInstanceAttributeIndices_.erase(
            std::unique(pendingInstanceAttributeIndices_.begin(),
                pendingInstanceAttributeIndices_.end()),
            pendingInstanceAttributeIndices_.end());
        if (!pendingInstanceAttributeIndices_.empty()) {
            Obol::internal::CadInstanceAttributeDelta delta;
            delta.revision =
                cachedPlan_.instanceAttributeRevision;
            delta.visibleIndices =
                pendingInstanceAttributeIndices_;
            delta.visibilityChanged = visibilityChanged;
            cachedPlan_.instanceAttributeDeltaEntryCount +=
                delta.visibleIndices.size();
            cachedPlan_.instanceAttributeDeltas.push_back(
                std::move(delta));
        }
        static constexpr size_t maxDeltaBatches = 256u;
        static constexpr size_t maxDeltaEntries = 65536u;
        while (cachedPlan_.instanceAttributeDeltas.size() >
                    maxDeltaBatches ||
                cachedPlan_.instanceAttributeDeltaEntryCount >
                    maxDeltaEntries) {
            const auto& front =
                cachedPlan_.instanceAttributeDeltas.front();
            cachedPlan_.instanceAttributeDeltaEntryCount -=
                front.visibleIndices.size();
            cachedPlan_.instanceAttributeDeltaFloorRevision =
                std::max(
                    cachedPlan_.instanceAttributeDeltaFloorRevision,
                    front.revision);
            cachedPlan_.instanceAttributeDeltas.pop_front();
        }
        pendingInstanceAttributeIndices_.clear();
        /*
         * Visibility does not alter screen-size classification at an
         * unchanged camera.  Existing aggregate points retain their stable
         * slots and receive the hidden flag above; non-proxied instances use
         * the same attribute journal in the mesh stream.  Camera or threshold
         * changes remain responsible for recomputing proxy membership.
         */
    }



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
    Obol::internal::CadFramePlan SoCADAssemblyImpl::buildFramePlan(
            Obol::CadDrawMode dm,
            const std::unordered_set<Obol::InstanceId,
                                     std::hash<Obol::InstanceId>>& selected,
            const std::unordered_set<Obol::InstanceId,
                                     std::hash<Obol::InstanceId>>& hidden,
            const std::map<Obol::PartId, InstancePartBucket> *buckets,
            SoGLRenderAction *renderAction) const {
        using namespace Obol::internal;

        CadFramePlan plan;
        if (instances_.empty()) return plan;
        size_t workSinceAbortCheck = 256u;
        const auto abortRequested = [&]() {
            if (!renderAction)
                return false;
            if (++workSinceAbortCheck < 256u)
                return false;
            workSinceAbortCheck = 0;
            return renderAction->abortNow();
        };

        /*
         * Write directly into the final flat allocation.  Retaining a second
         * array of (PartId, CadVisibleInstance) records doubled the memory
         * traffic of large streamed plan rebuilds even after the global sort
         * had been removed.
         */
        struct VisiblePartSpan {
            Obol::PartId part;
            size_t begin = 0;
            size_t end = 0;
        };
        std::vector<VisiblePartSpan> visiblePartSpans;
        const auto& sourceBuckets =
            buckets ? *buckets : instanceIdsByPart_;
        visiblePartSpans.reserve(sourceBuckets.size());
        size_t sourceInstanceCount = instances_.size();
        if (buckets) {
            sourceInstanceCount = 0;
            for (const auto& indexed : sourceBuckets) {
                if (abortRequested())
                    return CadFramePlan();
                sourceInstanceCount += indexed.second.size();
            }
        }
        /*
         * A full compact source initially presents structural fallback boxes,
         * then appends at most one richer presentation record per occurrence.
         * Reserve that known upper bound before publishing the initial plan.
         * Otherwise vector growth near convergence can copy tens of
         * thousands of records in one GUI frame even though the logical
         * mutation batch is small.
         *
         * Delta plans are transient and should stay exactly sized.
         */
        const size_t expectedSourceCount =
            buckets ? sourceInstanceCount :
            std::max(sourceInstanceCount,
                     streamingOccurrenceCapacityHint_);
        const size_t streamedTailCapacity =
            buckets ? 0u : expectedSourceCount;
        plan.visibleInstances.reserve(
            expectedSourceCount + streamedTailCapacity);
        plan.partBindings.reserve(
            std::min(parts_.size(), sourceBuckets.size()) +
            streamedTailCapacity);
        if (!buckets) {
            plan.wireItems.reserve(expectedSourceCount);
            plan.pointItems.reserve(expectedSourceCount);
            plan.shadedItems.reserve(expectedSourceCount);
            plan.requiredReps.reserve(expectedSourceCount * 2u);
        }

        const auto appendInstance =
            [&](Obol::InstanceId iid, const InstanceData& idata) {
            CadVisibleInstance vi;
            vi.instanceId = iid;
            vi.partIndex  = 0; // filled in below

            // Copy transform: raw OI float[16] (row-major).
            // The renderer uploads with GL_FALSE so GL sees it as column-major
            // (= transpose of OI matrix), which is the correct GL convention.
            std::memcpy(vi.transform.data(), idata.localToRoot[0],
                        16 * sizeof(float));

            // Instance style already encodes application-specific selection
            // colors.  Keep the selected bit separate for view policies such
            // as selected-full-detail LoD, but do not impose a fixed CAD-node
            // highlight color here.
            const bool isSel = selected.count(iid) != 0;
            vi.rgba = cadPackInstanceStyleColor(idata.style);
            vi.lineWidth = std::max(1.0f, idata.style.lineWidth);
            vi.linePattern = idata.style.linePattern;
            vi.linePatternFactor = std::max<uint16_t>(
                1u, idata.style.linePatternFactor);
            if (vi.lineWidth != 1.0f || vi.linePattern != 0xffffu)
                plan.hasCustomWireStyle = true;
            const bool isHidden = hidden.count(iid) != 0;
            vi.flags =
                (isSel ? CadInstanceSelected : 0u) |
                (idata.style.hasColorOverride ?
                    CadInstanceColorOverride : 0u) |
                (isHidden ? CadInstanceHidden : 0u) |
                (pointProxyProtected_.count(iid) ?
                    CadInstancePointProxyProtected : 0u) |
                (idata.lodStructuralProxy ?
                    CadInstanceLodStructuralProxy : 0u);
            vi.lodCut = idata.lodCut;

            if (cadDebugEnabled()) {
                std::fprintf(stderr,
                             "SoCADAssembly plan instance=%016llx:%016llx "
                             "styleOverride=%d style=(%.9g %.9g %.9g %.9g) "
                             "rgba=(%u %u %u %u) selected=%d\n",
                             static_cast<unsigned long long>(iid.w0),
                             static_cast<unsigned long long>(iid.w1),
                             idata.style.hasColorOverride ? 1 : 0,
                             idata.style.color[0], idata.style.color[1],
                             idata.style.color[2], idata.style.color[3],
                             static_cast<unsigned>(vi.rgba[0]),
                             static_cast<unsigned>(vi.rgba[1]),
                             static_cast<unsigned>(vi.rgba[2]),
                             static_cast<unsigned>(vi.rgba[3]),
                             isSel ? 1 : 0);
            }

            // Store world bounding box for per-instance frustum culling.
            SbVec3f wbMn, wbMx;
            idata.worldBounds.getBounds(wbMn, wbMx);
            vi.wbMin[0] = wbMn[0]; vi.wbMin[1] = wbMn[1]; vi.wbMin[2] = wbMn[2];
            vi.wbMax[0] = wbMx[0]; vi.wbMax[1] = wbMx[1]; vi.wbMax[2] = wbMx[2];

            if (!isHidden && !idata.worldBounds.isEmpty())
                plan.worldBounds.extendBy(idata.worldBounds);

            if (!isHidden && vi.rgba[3] < 255)
                ++plan.transparentVisibleInstanceCount;

            plan.visibleInstances.push_back(std::move(vi));
        };

        /*
         * The maintained part index is already in the renderer's primary
         * order.  Only occurrences which genuinely share a part need the
         * secondary (LoD/style/id) sort.  A 50k-unique scene therefore avoids
         * sorting and moving 50k large instance records on every streamed
         * publication wave.
         */
        for (const auto& indexed : sourceBuckets) {
            if (abortRequested())
                return CadFramePlan();
            /*
             * Do not retain unrenderable occurrences in the compiled plan.
             * Once geometry arrives, the normal structural/geometry
             * publication invalidates the plan and adds this part.
             */
            const auto part = parts_.find(indexed.first);
            if (part == parts_.end() || !part->second)
                continue;
            const size_t groupBegin = plan.visibleInstances.size();
            for (size_t slot = 0; slot < indexed.second.size(); ++slot) {
                if (abortRequested())
                    return CadFramePlan();
                const Obol::InstanceId iid = indexed.second.at(slot);
                const InstanceData *idata = indexed.second.dataAt(slot);
                if (idata)
                    appendInstance(iid, *idata);
            }
            const size_t groupEnd = plan.visibleInstances.size();
            if (groupBegin == groupEnd)
                continue;
            std::sort(
                plan.visibleInstances.begin() + groupBegin,
                plan.visibleInstances.end(),
                [](const CadVisibleInstance& av,
                   const CadVisibleInstance& bv) {
                    if (av.lodCut != bv.lodCut)
                        return av.lodCut < bv.lodCut;
                    if (av.lineWidth != bv.lineWidth)
                        return av.lineWidth < bv.lineWidth;
                    if (av.linePattern != bv.linePattern)
                        return av.linePattern < bv.linePattern;
                    if (av.linePatternFactor != bv.linePatternFactor)
                        return av.linePatternFactor <
                            bv.linePatternFactor;
                    return av.instanceId < bv.instanceId;
                });
            if (abortRequested())
                return CadFramePlan();
            visiblePartSpans.push_back(
                VisiblePartSpan{indexed.first, groupBegin, groupEnd});
        }

        const bool needWire = Obol::cadDrawModeHasWire(dm);
        const bool needShaded = Obol::cadDrawModeHasShaded(dm);

        for (const VisiblePartSpan& visibleSpan : visiblePartSpans) {
            if (abortRequested())
                return CadFramePlan();
            const Obol::PartId pid = visibleSpan.part;
            const size_t groupBegin = visibleSpan.begin;
            const size_t groupEnd = visibleSpan.end;
            auto partIt = parts_.find(pid);
            if (partIt == parts_.end() || !partIt->second) {
                continue;
            }
            const auto& geom = *partIt->second;
            /* Structural fallbacks are deliberately wire boxes even while the
             * requested model representation is shaded.  Omitting their wire
             * item until a shaded mesh exists leaves a cold large scene blank
             * despite a valid, visible coverage proxy. */
            const bool needWireForPart = needWire ||
                (needShaded && geom.structuralProxy &&
                 geom.wire.has_value());
            CadPartBinding binding;
            binding.part = pid;
            binding.geometry = partIt->second;
            binding.structuralProxy = geom.structuralProxy;
            binding.subpixelProxyOriented =
                geom.aggregateProxyCorners.has_value();
            const auto generationIt = partGeneration_.find(pid);
            if (generationIt != partGeneration_.end())
                binding.generation = generationIt->second;
            const uint32_t partIndex =
                static_cast<uint32_t>(plan.partBindings.size());
            const auto subpixelCorners =
                subpixelProxyCorners_.find(pid);
            if (subpixelCorners != subpixelProxyCorners_.end()) {
                binding.subpixelProxyEligible = true;
                binding.subpixelProxyCorners =
                    subpixelCorners->second;
            }
            plan.partBindings.push_back(std::move(binding));

            /*
             * The global flat sort already keeps a part's occurrences in
             * (level, wire-style, instance-id) order.  It both defines stable
             * draw runs and avoids a separately allocated vector and sort for
             * every unique part.
             */
            const bool progressiveShaded =
                geom.shaded.has_value() && geom.shaded->isProgressive();
            const size_t groupCount = groupEnd - groupBegin;
            if (groupCount > std::numeric_limits<uint32_t>::max())
                return CadFramePlan();
            const uint32_t count = static_cast<uint32_t>(groupCount);
            const auto visAt =
                [&](size_t offset) -> CadVisibleInstance& {
                    return plan.visibleInstances[groupBegin + offset];
                };
            const auto cullSafe =
                [&](size_t begin, size_t end) {
                    for (size_t i = begin; i < end; ++i) {
                        if (abortRequested())
                            return false;
                        const CadVisibleInstance& instance = visAt(i);
                        if (instance.rgba[3] != 255 ||
                                !cadTransformPreservesOrientation(
                                    instance.transform))
                            return false;
                    }
                    return true;
                };
            if (count > 0 &&
                    ((needWireForPart && geom.wire.has_value()) ||
                     (needShaded && geom.shaded.has_value()))) {
                uint8_t maximumCut = 0;
                for (size_t i = 0; i < groupCount; ++i) {
                    if (abortRequested())
                        return CadFramePlan();
                    maximumCut =
                        std::max(maximumCut, visAt(i).lodCut);
                }
                plan.partPresentation[pid].maximumRequestedCut =
                    maximumCut;
            }

            // Bind each occurrence directly to this plan-owned part payload.
            for (size_t i = 0; i < groupCount; ++i) {
                if (abortRequested())
                    return CadFramePlan();
                visAt(i).partIndex = partIndex;
            }

            // Wire draw item
            if (needWireForPart && geom.wire.has_value()) {
                CadDrawItem item;
                item.rep.part  = pid;
                item.rep.type  = geom.wire->derivesTriangleEdges() ?
                    CadRepType::Triangles : CadRepType::WireSegments;
                item.partIndex = partIndex;
                uint32_t runStart = 0;
                while (runStart < count) {
                    if (abortRequested())
                        return CadFramePlan();
                    uint32_t runEnd = runStart + 1;
                    while (runEnd < count &&
                           visAt(runEnd).lineWidth ==
                               visAt(runStart).lineWidth &&
                           visAt(runEnd).linePattern ==
                               visAt(runStart).linePattern &&
                           visAt(runEnd).linePatternFactor ==
                               visAt(runStart).linePatternFactor)
                    {
                        if (abortRequested())
                            return CadFramePlan();
                        ++runEnd;
                    }
                    item.baseInstance =
                        static_cast<uint32_t>(groupBegin) + runStart;
                    item.instanceCount = runEnd - runStart;
                    item.customWireStyle =
                        visAt(runStart).lineWidth != 1.0f ||
                        visAt(runStart).linePattern != 0xffffu;
                    plan.wireItems.push_back(item);
                    runStart = runEnd;
                }
                // Each part is visited exactly once by the flat grouped walk.
                plan.requiredReps.push_back(item.rep);
                plan.partPresentation[pid].
                    wireHasUncollapsedInstances = true;
            }

            if (geom.points.has_value() && !geom.points->positions.empty()) {
                CadDrawItem item;
                item.rep.part = pid;
                item.rep.type = CadRepType::Points;
                item.partIndex = partIndex;
                item.baseInstance = static_cast<uint32_t>(groupBegin);
                item.instanceCount = count;
                plan.pointItems.push_back(item);
                plan.requiredReps.push_back(item.rep);
            }

            // Shaded draw item
            if (needShaded && geom.shaded.has_value()) {
                const size_t itemBegin = plan.shadedItems.size();
                uint32_t runStart = 0;
                while (runStart < count) {
                    if (abortRequested())
                        return CadFramePlan();
                    uint32_t runEnd = runStart + 1;
                    while (runEnd < count &&
                           (!progressiveShaded ||
                            visAt(runEnd).lodCut ==
                                visAt(runStart).lodCut)) {
                        if (abortRequested())
                            return CadFramePlan();
                        ++runEnd;
                    }
                    CadDrawItem item;
                    item.rep.part  = pid;
                    item.rep.type  = CadRepType::Triangles;
                    item.partIndex = partIndex;
                    item.baseInstance =
                        static_cast<uint32_t>(groupBegin) + runStart;
                    item.instanceCount = runEnd - runStart;
                    item.cullBackfaces = geom.shadedCullBackfaces &&
                        cullSafe(runStart, runEnd);
                    if (renderAction && renderAction->hasTerminated())
                        return CadFramePlan();
                    plan.shadedItems.push_back(item);
                    runStart = runEnd;
                }
                /*
                 * A progressive part can occupy no more than one run per PoP
                 * level, and no more runs than it has occurrences.  Reserve
                 * that fixed number of draw-item slots in the cached plan.
                 * Sparse level changes can then alter run boundaries in place
                 * without inserting into the global draw-item vector.
                 */
                if (progressiveShaded) {
                    const size_t slotCount =
                        std::min<size_t>(count, ProgressiveCutBinCount);
                    while (plan.shadedItems.size() - itemBegin < slotCount) {
                        CadDrawItem item;
                        item.rep.part = pid;
                        item.rep.type = CadRepType::Triangles;
                        item.partIndex = partIndex;
                        item.baseInstance =
                            static_cast<uint32_t>(groupBegin);
                        item.instanceCount = 0;
                        item.cullBackfaces = geom.shadedCullBackfaces &&
                            cullSafe(0, groupCount);
                        if (renderAction && renderAction->hasTerminated())
                            return CadFramePlan();
                        plan.shadedItems.push_back(item);
                    }
                }
                CadRepKey rep;
                rep.part = pid;
                rep.type = CadRepType::Triangles;
                plan.requiredReps.push_back(rep);
            }

        }

        return plan;
    }

bool SoCADAssemblyImpl::rebuildProgressiveShadedPlanIndex(
            SoGLRenderAction *renderAction) {
        size_t workSinceAbortCheck = 256u;
        const auto abortRequested = [&]() {
            if (!renderAction)
                return false;
            if (++workSinceAbortCheck < 256u)
                return false;
            workSinceAbortCheck = 0;
            return renderAction->abortNow();
        };
        progressiveShadedPlanGroups_.clear();
        progressiveShadedPlanGroupByInstance_.clear();
        progressivePlanIndexByInstance_.clear();
        cachedPlanPartSpansByPart_.clear();
        if (cachedPlan_.visibleInstances.empty())
            return true;
        progressivePlanIndexByInstance_.reserve(
            cachedPlan_.visibleInstances.size());
        for (size_t i = 0; i < cachedPlan_.visibleInstances.size(); ++i) {
            if (abortRequested())
                return false;
            progressivePlanIndexByInstance_[
                cachedPlan_.visibleInstances[i].instanceId] =
                static_cast<uint32_t>(i);
        }

        /*
         * Draw items are emitted contiguously per part.  Index those ranges
         * once.  Scanning the complete shaded-item vector for every part made
         * a plan rebuild O(parts * draw-items)—2.5 billion comparisons for a
         * 50k unique-part scene—and dominated the qged GUI thread.
         */
        const size_t noItem = std::numeric_limits<size_t>::max();
        std::vector<size_t> wireItemBegin(
            cachedPlan_.partBindings.size(), noItem);
        std::vector<size_t> wireItemCount(
            cachedPlan_.partBindings.size(), 0);
        for (size_t i = 0; i < cachedPlan_.wireItems.size(); ++i) {
            if (abortRequested())
                return false;
            const size_t partIndex =
                cachedPlan_.wireItems[i].partIndex;
            if (partIndex >= wireItemBegin.size())
                continue;
            if (wireItemBegin[partIndex] == noItem)
                wireItemBegin[partIndex] = i;
            ++wireItemCount[partIndex];
        }
        std::vector<size_t> pointItemBegin(
            cachedPlan_.partBindings.size(), noItem);
        std::vector<size_t> pointItemCount(
            cachedPlan_.partBindings.size(), 0);
        for (size_t i = 0; i < cachedPlan_.pointItems.size(); ++i) {
            if (abortRequested())
                return false;
            const size_t partIndex =
                cachedPlan_.pointItems[i].partIndex;
            if (partIndex >= pointItemBegin.size())
                continue;
            if (pointItemBegin[partIndex] == noItem)
                pointItemBegin[partIndex] = i;
            ++pointItemCount[partIndex];
        }
        std::vector<size_t> shadedItemBegin(
            cachedPlan_.partBindings.size(), noItem);
        std::vector<size_t> shadedItemCount(
            cachedPlan_.partBindings.size(), 0);
        for (size_t i = 0; i < cachedPlan_.shadedItems.size(); ++i) {
            if (abortRequested())
                return false;
            const size_t partIndex =
                cachedPlan_.shadedItems[i].partIndex;
            if (partIndex >= shadedItemBegin.size())
                continue;
            if (shadedItemBegin[partIndex] == noItem)
                shadedItemBegin[partIndex] = i;
            ++shadedItemCount[partIndex];
        }

        size_t base = 0;
        while (base < cachedPlan_.visibleInstances.size()) {
            if (abortRequested())
                return false;
            const uint32_t partIndex =
                cachedPlan_.visibleInstances[base].partIndex;
            size_t end = base + 1;
            while (end < cachedPlan_.visibleInstances.size() &&
                    cachedPlan_.visibleInstances[end].partIndex == partIndex) {
                if (abortRequested())
                    return false;
                ++end;
            }

            if (partIndex >= cachedPlan_.partBindings.size()) {
                base = end;
                continue;
            }
            const auto& binding = cachedPlan_.partBindings[partIndex];
            const Obol::PartId part = binding.part;
            CachedPlanPartSpan span;
            span.partIndex = partIndex;
            span.baseInstance = static_cast<uint32_t>(base);
            span.instanceCount = static_cast<uint32_t>(end - base);
            if (wireItemBegin[partIndex] != noItem) {
                span.wireItemBegin = wireItemBegin[partIndex];
                span.wireItemCount = wireItemCount[partIndex];
            }
            if (pointItemBegin[partIndex] != noItem) {
                span.pointItemBegin = pointItemBegin[partIndex];
                span.pointItemCount = pointItemCount[partIndex];
            }
            if (shadedItemBegin[partIndex] != noItem) {
                span.shadedItemBegin = shadedItemBegin[partIndex];
                span.shadedItemCount = shadedItemCount[partIndex];
            }
            cachedPlanPartSpansByPart_[part].push_back(span);
            if (!cachedDrawModeHasShaded()) {
                base = end;
                continue;
            }
            if (!binding.geometry ||
                    !binding.geometry->shaded.has_value() ||
                    !binding.geometry->shaded->isProgressive()) {
                base = end;
                continue;
            }

            ProgressiveShadedPlanGroup group;
            group.part = part;
            group.baseInstance = static_cast<uint32_t>(base);
            group.instanceCount = static_cast<uint32_t>(end - base);
            for (size_t i = base; i < end; ++i) {
                if (abortRequested())
                    return false;
                const auto& instance = cachedPlan_.visibleInstances[i];
                ++group.cutCounts[progressiveCutBin(instance.lodCut)];
            }
            group.shadedItemBegin = shadedItemBegin[partIndex];
            group.shadedItemCount = shadedItemCount[partIndex];
            if (group.shadedItemCount == 0) {
                base = end;
                continue;
            }
            const size_t groupIndex =
                progressiveShadedPlanGroups_.size();
            for (size_t i = base; i < end; ++i) {
                if (abortRequested())
                    return false;
                progressiveShadedPlanGroupByInstance_[
                    cachedPlan_.visibleInstances[i].instanceId] =
                        groupIndex;
            }
            progressiveShadedPlanGroups_.push_back(group);
            base = end;
        }
        return true;
    }

bool SoCADAssemblyImpl::partGeometryPlanCompatible(
            const Obol::PartGeometry& oldGeometry,
            const Obol::PartGeometry& newGeometry) {
        if (oldGeometry.points.has_value() !=
                newGeometry.points.has_value() ||
                oldGeometry.wire.has_value() !=
                newGeometry.wire.has_value() ||
                oldGeometry.shaded.has_value() !=
                newGeometry.shaded.has_value())
            return false;
        if (oldGeometry.points &&
                oldGeometry.points->positions.empty() !=
                    newGeometry.points->positions.empty())
            return false;
        if (oldGeometry.wire &&
                (Obol::internal::cadWireSourceIsProgressive(
                     *oldGeometry.wire) !=
                     Obol::internal::cadWireSourceIsProgressive(
                         *newGeometry.wire) ||
                 oldGeometry.wire->derivesTriangleEdges() !=
                     newGeometry.wire->derivesTriangleEdges()))
            return false;
        if (oldGeometry.shaded &&
                oldGeometry.shaded->isProgressive() !=
                    newGeometry.shaded->isProgressive())
            return false;
        return true;
    }



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
    bool SoCADAssemblyImpl::appendCachedInstances(
            const std::vector<Obol::InstanceId>& instanceIds,
            bool allowReplacements) {
        using namespace Obol::internal;

        const auto fail = [&](const char *reason, size_t actual = 0,
                              size_t expected = 0) {
            if (cadPlanDebugEnabled() &&
                    planDebugPatchMessageCount_++ <
                        cadPlanDebugMessageLimit())
                std::fprintf(stderr,
                    "SoCADAssembly sparse append rejected reason=%s "
                    "batch=%zu actual=%zu expected=%zu "
                    "plan_dirty=%d geometry_dirty=%d draw_mode=%d "
                    "visible=%zu instances=%zu parts=%zu\n",
                    reason ? reason : "unknown", instanceIds.size(),
                    actual, expected, planDirty_ ? 1 : 0,
                    geometryDirty_ ? 1 : 0, cachedDrawModeDiagnostic(),
                    cachedPlan_.visibleInstances.size(),
                    instances_.size(), parts_.size());
            return false;
        };
        if (instanceIds.empty() || planDirty_ || geometryDirty_ ||
                !cachedDrawMode_)
            return fail("plan-state");
        std::map<Obol::PartId, InstancePartBucket> appendedBuckets;
        std::vector<uint32_t> replacedIndices;
        replacedIndices.reserve(instanceIds.size());
        std::unordered_set<Obol::InstanceId, std::hash<Obol::InstanceId>>
            uniqueInstances;
        uniqueInstances.reserve(instanceIds.size());
        for (const Obol::InstanceId instance : instanceIds) {
            if (!uniqueInstances.insert(instance).second)
                return fail("duplicate-instance");
            const auto retained = instances_.find(instance);
            if (retained == instances_.end())
                return fail("instance-not-retained");
            const auto compiled =
                progressivePlanIndexByInstance_.find(instance);
            if (compiled != progressivePlanIndexByInstance_.end()) {
                if (!allowReplacements)
                    return fail("instance-already-compiled");
                if (compiled->second >=
                        cachedPlan_.visibleInstances.size())
                    return fail("invalid-replacement-index");
                const CadVisibleInstance& oldVisible =
                    cachedPlan_.visibleInstances[compiled->second];
                if (oldVisible.partIndex >=
                        cachedPlan_.partBindings.size())
                    return fail("invalid-replacement-part");
                if (cachedPlan_.partBindings[oldVisible.partIndex].part ==
                        retained->second.partId)
                    return fail("replacement-part-unchanged");
                replacedIndices.push_back(compiled->second);
            }
            const auto geometry = parts_.find(retained->second.partId);
            if (geometry == parts_.end() || !geometry->second)
                return fail("part-not-retained");
            InstancePartBucket& bucket =
                appendedBuckets[retained->second.partId];
            if (!bucket.hasFirst) {
                bucket.first = instance;
                bucket.firstData = &retained->second;
                bucket.hasFirst = true;
            } else {
                bucket.additional.push_back(instance);
                bucket.additionalData.push_back(&retained->second);
            }
        }

        CadFramePlan delta = buildFramePlan(
            *cachedDrawMode_, selected_, hidden_, &appendedBuckets);
        if (delta.visibleInstances.size() != instanceIds.size())
            return fail("visible-count", delta.visibleInstances.size(),
                        instanceIds.size());
        if (delta.partBindings.size() != appendedBuckets.size())
            return fail("binding-count", delta.partBindings.size(),
                        appendedBuckets.size());
        const size_t visibleBase =
            cachedPlan_.visibleInstances.size();
        const size_t partBase = cachedPlan_.partBindings.size();
        const size_t wireItemBase = cachedPlan_.wireItems.size();
        const size_t pointItemBase = cachedPlan_.pointItems.size();
        const size_t shadedItemBase = cachedPlan_.shadedItems.size();
        if (visibleBase + delta.visibleInstances.size() >
                    std::numeric_limits<uint32_t>::max() ||
                partBase + delta.partBindings.size() >
                    std::numeric_limits<uint32_t>::max())
            return fail("index-overflow");

        /*
         * All validation which can reject the operation has completed.
         * Retire replacement presentations only now, so a failed sparse
         * attempt leaves the cached plan untouched for the fallback rebuild.
         */
        for (const uint32_t oldIndex : replacedIndices) {
            auto& oldVisible = cachedPlan_.visibleInstances[oldIndex];
            if (!(oldVisible.flags & CadInstanceHidden) &&
                    oldVisible.rgba[3] < 255 &&
                    cachedPlan_.transparentVisibleInstanceCount)
                --cachedPlan_.transparentVisibleInstanceCount;
            oldVisible.flags |= CadInstanceHidden;
            progressiveShadedPlanGroupByInstance_.erase(
                oldVisible.instanceId);
        }
        cachedPlanTombstoneCount_ += replacedIndices.size();

        const size_t noItem = std::numeric_limits<size_t>::max();
        std::vector<size_t> wireBegin(delta.partBindings.size(), noItem);
        std::vector<size_t> wireCount(delta.partBindings.size(), 0);
        for (size_t i = 0; i < delta.wireItems.size(); ++i) {
            const size_t partIndex = delta.wireItems[i].partIndex;
            if (partIndex >= wireBegin.size())
                return fail("wire-item-part-index");
            if (wireBegin[partIndex] == noItem)
                wireBegin[partIndex] = i;
            ++wireCount[partIndex];
        }
        std::vector<size_t> pointBegin(delta.partBindings.size(), noItem);
        std::vector<size_t> pointCount(delta.partBindings.size(), 0);
        for (size_t i = 0; i < delta.pointItems.size(); ++i) {
            const size_t partIndex = delta.pointItems[i].partIndex;
            if (partIndex >= pointBegin.size())
                return fail("point-item-part-index");
            if (pointBegin[partIndex] == noItem)
                pointBegin[partIndex] = i;
            ++pointCount[partIndex];
        }
        std::vector<size_t> shadedBegin(delta.partBindings.size(), noItem);
        std::vector<size_t> shadedCount(delta.partBindings.size(), 0);
        for (size_t i = 0; i < delta.shadedItems.size(); ++i) {
            const size_t partIndex = delta.shadedItems[i].partIndex;
            if (partIndex >= shadedBegin.size())
                return fail("shaded-item-part-index");
            if (shadedBegin[partIndex] == noItem)
                shadedBegin[partIndex] = i;
            ++shadedCount[partIndex];
        }

        size_t localBase = 0;
        while (localBase < delta.visibleInstances.size()) {
            const uint32_t localPart =
                delta.visibleInstances[localBase].partIndex;
            size_t localEnd = localBase + 1u;
            while (localEnd < delta.visibleInstances.size() &&
                    delta.visibleInstances[localEnd].partIndex == localPart)
                ++localEnd;
            if (localPart >= delta.partBindings.size())
                return fail("visible-part-index");
            const Obol::PartId part =
                delta.partBindings[localPart].part;
            CachedPlanPartSpan span;
            span.partIndex = static_cast<uint32_t>(
                partBase + localPart);
            span.baseInstance = static_cast<uint32_t>(
                visibleBase + localBase);
            span.instanceCount = static_cast<uint32_t>(
                localEnd - localBase);
            if (wireBegin[localPart] != noItem) {
                span.wireItemBegin =
                    wireItemBase + wireBegin[localPart];
                span.wireItemCount = wireCount[localPart];
            }
            if (pointBegin[localPart] != noItem) {
                span.pointItemBegin =
                    pointItemBase + pointBegin[localPart];
                span.pointItemCount = pointCount[localPart];
            }
            if (shadedBegin[localPart] != noItem) {
                span.shadedItemBegin =
                    shadedItemBase + shadedBegin[localPart];
                span.shadedItemCount = shadedCount[localPart];
            }
            cachedPlanPartSpansByPart_[part].push_back(span);

            const CadPartBinding& binding =
                delta.partBindings[localPart];
            if (cachedDrawModeHasShaded() &&
                    binding.geometry && binding.geometry->shaded &&
                    binding.geometry->shaded->isProgressive()) {
                ProgressiveShadedPlanGroup group;
                group.part = part;
                group.baseInstance = span.baseInstance;
                group.instanceCount = span.instanceCount;
                for (size_t i = localBase; i < localEnd; ++i)
                    ++group.cutCounts[progressiveCutBin(
                        delta.visibleInstances[i].lodCut)];
                group.shadedItemBegin = span.shadedItemBegin;
                group.shadedItemCount = span.shadedItemCount;
                if (!group.shadedItemCount)
                    return fail("missing-progressive-draw-item");
                const size_t groupIndex =
                    progressiveShadedPlanGroups_.size();
                for (size_t i = localBase; i < localEnd; ++i)
                    progressiveShadedPlanGroupByInstance_[
                        delta.visibleInstances[i].instanceId] =
                            groupIndex;
                progressiveShadedPlanGroups_.push_back(group);
            }
            for (size_t i = localBase; i < localEnd; ++i)
                progressivePlanIndexByInstance_[
                    delta.visibleInstances[i].instanceId] =
                        static_cast<uint32_t>(visibleBase + i);
            localBase = localEnd;
        }

        for (CadVisibleInstance& visible : delta.visibleInstances) {
            visible.partIndex += static_cast<uint32_t>(partBase);
            cachedPlan_.visibleInstances.push_back(std::move(visible));
        }
        cachedPlan_.transparentVisibleInstanceCount +=
            delta.transparentVisibleInstanceCount;
        for (CadPartBinding& binding : delta.partBindings)
            cachedPlan_.partBindings.push_back(std::move(binding));
        const auto appendItems = [visibleBase, partBase](
                auto& destination, auto& source) {
            for (auto& item : source) {
                item.baseInstance += static_cast<uint32_t>(visibleBase);
                item.partIndex += static_cast<uint32_t>(partBase);
                destination.push_back(std::move(item));
            }
        };
        appendItems(cachedPlan_.wireItems, delta.wireItems);
        appendItems(cachedPlan_.pointItems, delta.pointItems);
        appendItems(cachedPlan_.shadedItems, delta.shadedItems);
        for (CadRepKey& rep : delta.requiredReps)
            cachedPlan_.requiredReps.push_back(std::move(rep));
        for (const auto& item : delta.partPresentation) {
            auto inserted = cachedPlan_.partPresentation.emplace(
                item.first, item.second);
            if (!inserted.second) {
                inserted.first->second.maximumRequestedCut = std::max(
                    inserted.first->second.maximumRequestedCut,
                    item.second.maximumRequestedCut);
                inserted.first->second.wireHasUncollapsedInstances =
                    inserted.first->second.
                        wireHasUncollapsedInstances ||
                    item.second.wireHasUncollapsedInstances;
            }
        }
        cachedPlan_.hasCustomWireStyle =
            cachedPlan_.hasCustomWireStyle || delta.hasCustomWireStyle;
        if (!delta.worldBounds.isEmpty())
            cachedPlan_.worldBounds.extendBy(delta.worldBounds);

        CadPlanAppendDelta appendDelta;
        appendDelta.revision = Obol::internal::cadTakeNonzeroIdentity(
            nextAppendRevision_);
        appendDelta.subpixelProxyInputRevision =
            Obol::internal::cadTakeNonzeroIdentity(
                nextSubpixelProxyInputRevision_);
        cachedPlan_.subpixelProxyInputRevision =
            appendDelta.subpixelProxyInputRevision;
        appendDelta.visibleBegin =
            static_cast<uint32_t>(visibleBase);
        appendDelta.visibleCount =
            static_cast<uint32_t>(
                cachedPlan_.visibleInstances.size() - visibleBase);
        appendDelta.partBegin =
            static_cast<uint32_t>(partBase);
        appendDelta.partCount =
            static_cast<uint32_t>(
                cachedPlan_.partBindings.size() - partBase);
        appendDelta.shadedItemBegin =
            static_cast<uint32_t>(shadedItemBase);
        appendDelta.shadedItemCount =
            static_cast<uint32_t>(
                cachedPlan_.shadedItems.size() - shadedItemBase);
        appendDelta.retiredVisibleIndices =
            std::move(replacedIndices);
        cachedPlan_.appendRevision = appendDelta.revision;
        cachedPlan_.appendDeltaEntryCount +=
            1u + appendDelta.retiredVisibleIndices.size();
        cachedPlan_.appendDeltas.push_back(
            std::move(appendDelta));
        static constexpr size_t maxAppendDeltaBatches = 256u;
        static constexpr size_t maxAppendDeltaEntries = 65536u;
        while (cachedPlan_.appendDeltas.size() >
                    maxAppendDeltaBatches ||
                cachedPlan_.appendDeltaEntryCount >
                    maxAppendDeltaEntries) {
            const auto& front =
                cachedPlan_.appendDeltas.front();
            cachedPlan_.appendDeltaEntryCount -=
                1u + front.retiredVisibleIndices.size();
            cachedPlan_.appendDeltaFloorRevision =
                std::max(
                    cachedPlan_.appendDeltaFloorRevision,
                    front.revision);
            cachedPlan_.appendDeltas.pop_front();
        }

        bvhDirty_ = true;
        return true;
    }



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
    bool SoCADAssemblyImpl::canPatchCachedInstancePartRebind(
            Obol::InstanceId instance, Obol::PartId oldPart) const {
        if (planDirty_ || geometryDirty_ || !cachedDrawMode_)
            return false;
        const auto retained = instances_.find(instance);
        if (retained == instances_.end())
            return false;
        const Obol::PartId newPart = retained->second.partId;
        if (newPart == oldPart)
            return false;
        const auto oldSpanFound = cachedPlanPartSpansByPart_.find(oldPart);
        if (oldSpanFound == cachedPlanPartSpansByPart_.end() ||
                oldSpanFound->second.size() != 1u ||
                oldSpanFound->second.front().instanceCount != 1u ||
                cachedPlanPartSpansByPart_.count(newPart))
            return false;
        const CachedPlanPartSpan oldSpan =
            oldSpanFound->second.front();
        if (oldSpan.partIndex >= cachedPlan_.partBindings.size() ||
                oldSpan.baseInstance >= cachedPlan_.visibleInstances.size())
            return false;
        const Obol::internal::CadVisibleInstance& visible =
            cachedPlan_.visibleInstances[oldSpan.baseInstance];
        if (!(visible.instanceId == instance) ||
                visible.partIndex != oldSpan.partIndex)
            return false;
        if (!(cachedPlan_.partBindings[oldSpan.partIndex].part == oldPart))
            return false;
        const auto newGeometryFound = parts_.find(newPart);
        if (newGeometryFound == parts_.end() || !newGeometryFound->second)
            return false;
        const auto rangeIsValid = [](size_t size, size_t begin, size_t count) {
            return begin <= size && count <= size - begin;
        };
        return rangeIsValid(
                   cachedPlan_.wireItems.size(),
                   oldSpan.wireItemBegin, oldSpan.wireItemCount) &&
            rangeIsValid(
                   cachedPlan_.pointItems.size(),
                   oldSpan.pointItemBegin, oldSpan.pointItemCount) &&
            rangeIsValid(
                   cachedPlan_.shadedItems.size(),
                   oldSpan.shadedItemBegin, oldSpan.shadedItemCount);
    }

bool SoCADAssemblyImpl::patchCachedInstancePartRebind(
            Obol::InstanceId instance, Obol::PartId oldPart,
            bool prevalidated) {
        using namespace Obol::internal;

        if (!prevalidated &&
                !canPatchCachedInstancePartRebind(instance, oldPart))
            return false;
        const auto retained = instances_.find(instance);
        const Obol::PartId newPart = retained->second.partId;
        const auto oldSpanFound = cachedPlanPartSpansByPart_.find(oldPart);
        const CachedPlanPartSpan oldSpan =
            oldSpanFound->second.front();
        CadVisibleInstance& visible =
            cachedPlan_.visibleInstances[oldSpan.baseInstance];
        const auto newGeometryFound = parts_.find(newPart);
        const Obol::PartGeometry& geometry = *newGeometryFound->second;

        const auto disableItems = [](auto& items, size_t begin, size_t count) {
            if (begin > items.size() || count > items.size() - begin)
                return false;
            for (size_t i = begin; i < begin + count; ++i)
                items[i].instanceCount = 0;
            return true;
        };
        if (!disableItems(
                cachedPlan_.wireItems, oldSpan.wireItemBegin,
                oldSpan.wireItemCount) ||
                !disableItems(
                    cachedPlan_.pointItems, oldSpan.pointItemBegin,
                    oldSpan.pointItemCount) ||
                !disableItems(
                    cachedPlan_.shadedItems, oldSpan.shadedItemBegin,
                    oldSpan.shadedItemCount))
            return false;

        const InstanceData& data = retained->second;
        const bool wasTransparent =
            !(visible.flags & CadInstanceHidden) &&
            visible.rgba[3] < 255;
        std::memcpy(
            visible.transform.data(), data.localToRoot[0],
            16 * sizeof(float));
        visible.rgba = cadPackInstanceStyleColor(data.style);
        visible.lineWidth = std::max(1.0f, data.style.lineWidth);
        visible.linePattern = data.style.linePattern;
        visible.linePatternFactor = std::max<uint16_t>(
            1u, data.style.linePatternFactor);
        if (visible.lineWidth != 1.0f ||
                visible.linePattern != 0xffffu)
            cachedPlan_.hasCustomWireStyle = true;
        visible.flags =
            (selected_.count(instance) ? CadInstanceSelected : 0u) |
            (data.style.hasColorOverride ?
                CadInstanceColorOverride : 0u) |
            (hidden_.count(instance) ? CadInstanceHidden : 0u) |
            (pointProxyProtected_.count(instance) ?
                CadInstancePointProxyProtected : 0u) |
            (data.lodStructuralProxy ?
                CadInstanceLodStructuralProxy : 0u);
        visible.lodCut = data.lodCut;
        const bool isTransparent =
            !(visible.flags & CadInstanceHidden) &&
            visible.rgba[3] < 255;
        if (wasTransparent != isTransparent) {
            if (isTransparent)
                ++cachedPlan_.transparentVisibleInstanceCount;
            else if (cachedPlan_.transparentVisibleInstanceCount)
                --cachedPlan_.transparentVisibleInstanceCount;
        }
        if (!data.worldBounds.isEmpty()) {
            SbVec3f minimum, maximum;
            data.worldBounds.getBounds(minimum, maximum);
            for (int axis = 0; axis < 3; ++axis) {
                visible.wbMin[axis] = minimum[axis];
                visible.wbMax[axis] = maximum[axis];
            }
            if (!(visible.flags & CadInstanceHidden))
                cachedPlan_.worldBounds.extendBy(data.worldBounds);
        }

        CadPartBinding& binding =
            cachedPlan_.partBindings[oldSpan.partIndex];
        binding.part = newPart;
        binding.geometry = newGeometryFound->second;
        const auto generation = partGeneration_.find(newPart);
        binding.generation = generation == partGeneration_.end() ?
            0 : generation->second;
        binding.structuralProxy = newGeometryFound->second->structuralProxy;
        binding.subpixelProxyOriented =
            newGeometryFound->second->aggregateProxyCorners.has_value();
        const auto proxy = subpixelProxyCorners_.find(newPart);
        binding.subpixelProxyEligible =
            proxy != subpixelProxyCorners_.end();
        if (binding.subpixelProxyEligible)
            binding.subpixelProxyCorners = proxy->second;
        else
            binding.subpixelProxyCorners = {};

        cachedPlan_.partPresentation.erase(oldPart);
        progressiveShadedPlanGroupByInstance_.erase(instance);

        CachedPlanPartSpan newSpan;
        newSpan.partIndex = oldSpan.partIndex;
        newSpan.baseInstance = oldSpan.baseInstance;
        newSpan.instanceCount = 1u;
        const bool needWire = cachedDrawModeHasWire();
        const bool needShaded = cachedDrawModeHasShaded();
        /* Match the full plan path: a temporary structural proxy remains a
         * wire extent in shaded mode until a shaded mesh supersedes it. */
        const bool needWireForPart = needWire ||
            (needShaded && geometry.structuralProxy && geometry.wire);
        if ((needWireForPart && geometry.wire) ||
                (needShaded && geometry.shaded) ||
                (geometry.points && !geometry.points->positions.empty()))
            cachedPlan_.partPresentation[newPart].maximumRequestedCut =
                visible.lodCut;

        if (needWireForPart && geometry.wire) {
            CadDrawItem item;
            item.rep.part = newPart;
            item.rep.type = geometry.wire->derivesTriangleEdges() ?
                CadRepType::Triangles : CadRepType::WireSegments;
            item.partIndex = oldSpan.partIndex;
            item.baseInstance = oldSpan.baseInstance;
            item.instanceCount = 1u;
            item.customWireStyle =
                visible.lineWidth != 1.0f ||
                visible.linePattern != 0xffffu;
            newSpan.wireItemBegin = cachedPlan_.wireItems.size();
            newSpan.wireItemCount = 1u;
            cachedPlan_.wireItems.push_back(item);
            cachedPlan_.requiredReps.push_back(item.rep);
            cachedPlan_.partPresentation[newPart].
                wireHasUncollapsedInstances = true;
        }
        if (geometry.points && !geometry.points->positions.empty()) {
            CadDrawItem item;
            item.rep.part = newPart;
            item.rep.type = CadRepType::Points;
            item.partIndex = oldSpan.partIndex;
            item.baseInstance = oldSpan.baseInstance;
            item.instanceCount = 1u;
            newSpan.pointItemBegin = cachedPlan_.pointItems.size();
            newSpan.pointItemCount = 1u;
            cachedPlan_.pointItems.push_back(item);
            cachedPlan_.requiredReps.push_back(item.rep);
        }
        if (needShaded && geometry.shaded) {
            CadDrawItem item;
            item.rep.part = newPart;
            item.rep.type = CadRepType::Triangles;
            item.partIndex = oldSpan.partIndex;
            item.baseInstance = oldSpan.baseInstance;
            item.instanceCount = 1u;
            item.cullBackfaces = geometry.shadedCullBackfaces &&
                visible.rgba[3] == 255 &&
                cadTransformPreservesOrientation(visible.transform);
            newSpan.shadedItemBegin = cachedPlan_.shadedItems.size();
            newSpan.shadedItemCount = 1u;
            cachedPlan_.shadedItems.push_back(item);
            cachedPlan_.requiredReps.push_back(item.rep);
            if (geometry.shaded->isProgressive()) {
                ProgressiveShadedPlanGroup group;
                group.part = newPart;
                group.baseInstance = oldSpan.baseInstance;
                group.instanceCount = 1u;
                group.cutCounts[
                    progressiveCutBin(visible.lodCut)] = 1u;
                group.shadedItemBegin = newSpan.shadedItemBegin;
                group.shadedItemCount = 1u;
                progressiveShadedPlanGroupByInstance_[instance] =
                    progressiveShadedPlanGroups_.size();
                progressiveShadedPlanGroups_.push_back(group);
            }
        }

        cachedPlanPartSpansByPart_.erase(oldSpanFound);
        cachedPlanPartSpansByPart_[newPart].push_back(newSpan);
        const uint64_t priorSubpixelInputRevision =
            cachedPlan_.subpixelProxyInputRevision;
        cachedPlan_.subpixelProxyInputRevision =
            Obol::internal::cadTakeNonzeroIdentity(
                nextSubpixelProxyInputRevision_);
        const bool patchedSubpixelOccurrence =
            patchSubpixelProxyGeometryForVisible(
                oldSpan.baseInstance, priorSubpixelInputRevision);
        if (patchedSubpixelOccurrence) {
            subpixelProxyStateInputRevision_ =
                cachedPlan_.subpixelProxyInputRevision;
            subpixelProxyViewInputRevision_ =
                cachedPlan_.subpixelProxyInputRevision;
            cachedPlan_.subpixelProxySourceInputRevision =
                cachedPlan_.subpixelProxyInputRevision;
            std::unordered_set<Obol::PartId,
                std::hash<Obol::PartId>> affectedParts;
            /* The old unique part span has already been retired, so the
             * per-part refresh cannot discover this occurrence through that
             * span to erase its former structural-frontier identity. */
            uncollapsedStructuralProxyInstances_.erase(instance);
            affectedParts.insert(oldPart);
            affectedParts.insert(newPart);
            refreshWireProxyParts(affectedParts);
        } else {
            subpixelProxyStateInputRevision_ = 0;
            subpixelProxyViewValid_ = false;
            structuralProjectionHistogram_.exact = false;
        }
        bvhDirty_ = true;
        return true;
    }



    /*
     * Apply a wave of independent unique-slot presentation rebinds without
     * appending duplicate visible-instance records.  Validate the complete
     * wave before mutating the cached plan so a shared structural range or a
     * repeated target part can fall back to the append representation
     * atomically.
     */
    bool SoCADAssemblyImpl::patchCachedInstancePartRebinds(
            const std::vector<std::pair<Obol::InstanceId, Obol::PartId>>&
                rebinds) {
        if (rebinds.empty())
            return false;
        std::unordered_set<Obol::PartId, std::hash<Obol::PartId>>
            targetParts;
        targetParts.reserve(rebinds.size());
        for (const auto& rebind : rebinds) {
            if (!canPatchCachedInstancePartRebind(
                    rebind.first, rebind.second))
                return false;
            const auto retained = instances_.find(rebind.first);
            if (retained == instances_.end() ||
                    !targetParts.insert(retained->second.partId).second)
                return false;
        }
        /*
         * The preflight above makes the batch atomic.  Do not repeat the
         * same part/span/instance hash lookups inside every patch, and reserve
         * the exact worst-case append capacity before the first mutation.
         * Cold box-to-mesh waves otherwise spend more owner-thread time in
         * allocator growth and duplicate validation than in renderer work.
         */
        cachedPlan_.wireItems.reserve(
            cachedPlan_.wireItems.size() + rebinds.size());
        cachedPlan_.pointItems.reserve(
            cachedPlan_.pointItems.size() + rebinds.size());
        cachedPlan_.shadedItems.reserve(
            cachedPlan_.shadedItems.size() + rebinds.size());
        cachedPlan_.requiredReps.reserve(
            cachedPlan_.requiredReps.size() + 3u * rebinds.size());
        progressiveShadedPlanGroups_.reserve(
            progressiveShadedPlanGroups_.size() + rebinds.size());
        progressiveShadedPlanGroupByInstance_.reserve(
            progressiveShadedPlanGroupByInstance_.size() +
                rebinds.size());
        cachedPlanPartSpansByPart_.reserve(
            cachedPlanPartSpansByPart_.size() + rebinds.size());
        cachedPlan_.partPresentation.reserve(
            cachedPlan_.partPresentation.size() +
                rebinds.size());
        for (const auto& rebind : rebinds) {
            if (!patchCachedInstancePartRebind(
                    rebind.first, rebind.second, true))
                return false;
        }
        return true;
    }

void SoCADAssemblyImpl::finishSparseStructuralPatch() {
        geometryRevision_ = Obol::internal::cadTakeNonzeroIdentity(
            nextGeometryRevision_);
        cachedPlan_.geometryRevision = geometryRevision_;
        cachedPlan_.revision = Obol::internal::cadTakeNonzeroIdentity(
            nextPlanRevision_);
        cachedPlan_.shadedLayoutRevision =
            Obol::internal::cadTakeNonzeroIdentity(
                nextShadedLayoutRevision_);
    }



    /*
     * Replace the immutable arrays behind an existing retained part without
     * recompiling the assembly-wide instance plan.  Progressive population
     * growth changes a part generation and its resident prefix capacity, but
     * not the occurrence topology, styles, or level buckets.
     */
    bool SoCADAssemblyImpl::patchCachedPartGeometry(
            Obol::PartId part,
            Obol::internal::CadPartGeometryDelta& geometryDelta,
            bool preservesBounds) {
        if (planDirty_ || geometryDirty_)
            return false;
        const auto geometryFound = parts_.find(part);
        if (geometryFound == parts_.end() || !geometryFound->second)
            return false;
        const auto spansFound = cachedPlanPartSpansByPart_.find(part);
        if (spansFound == cachedPlanPartSpansByPart_.end()) {
            /*
             * An unreferenced retained part has no current frame binding.
             * Hidden occurrences remain indexed, so a referenced part must
             * always have a span even while every occurrence is hidden.
             */
            const auto referenced = instanceIdsByPart_.find(part);
            return referenced == instanceIdsByPart_.end() ||
                referenced->second.size() == 0;
        }
        const auto generationFound = partGeneration_.find(part);
        const uint64_t generation =
            generationFound == partGeneration_.end() ?
            0 : generationFound->second;
        const auto proxyFound = subpixelProxyCorners_.find(part);
        auto& visible = cachedPlan_.visibleInstances;
        for (const CachedPlanPartSpan& span : spansFound->second) {
            if (span.partIndex >= cachedPlan_.partBindings.size() ||
                    span.baseInstance > visible.size() ||
                    span.instanceCount >
                        visible.size() - span.baseInstance ||
                    span.shadedItemBegin >
                        cachedPlan_.shadedItems.size() ||
                    span.shadedItemCount >
                        cachedPlan_.shadedItems.size() -
                            span.shadedItemBegin)
                return false;
            const auto& binding =
                cachedPlan_.partBindings[span.partIndex];
            if (!binding.geometry ||
                    !partGeometryPlanCompatible(
                        *binding.geometry, *geometryFound->second))
                return false;
        }
        for (const CachedPlanPartSpan& span : spansFound->second) {
            auto& binding = cachedPlan_.partBindings[span.partIndex];
            const bool newProxyEligible =
                proxyFound != subpixelProxyCorners_.end();
            if (binding.subpixelProxyEligible != newProxyEligible ||
                    binding.subpixelProxyOriented !=
                        geometryFound->second->aggregateProxyCorners.has_value() ||
                    (newProxyEligible &&
                     binding.subpixelProxyCorners != proxyFound->second) ||
                    binding.structuralProxy !=
                        geometryFound->second->structuralProxy)
                geometryDelta.subpixelProxyInputChanged = true;
            binding.geometry = geometryFound->second;
            binding.generation = generation;
            binding.subpixelProxyEligible = newProxyEligible;
            binding.structuralProxy =
                geometryFound->second->structuralProxy;
            binding.subpixelProxyOriented =
                geometryFound->second->aggregateProxyCorners.has_value();
            if (binding.subpixelProxyEligible)
                binding.subpixelProxyCorners = proxyFound->second;
            else
                binding.subpixelProxyCorners = {};

            if (!preservesBounds) {
                geometryDelta.boundsChanged = true;
                for (uint32_t offset = 0;
                        offset < span.instanceCount; ++offset) {
                    auto& occurrence =
                        visible[span.baseInstance + offset];
                    const auto instanceFound =
                        instances_.find(occurrence.instanceId);
                    if (instanceFound == instances_.end())
                        return false;
                    const SbBox3f& bounds =
                        instanceFound->second.worldBounds;
                    if (bounds.isEmpty())
                        continue;
                    SbVec3f minimum, maximum;
                    bounds.getBounds(minimum, maximum);
                    for (int axis = 0; axis < 3; ++axis) {
                        occurrence.wbMin[axis] = minimum[axis];
                        occurrence.wbMax[axis] = maximum[axis];
                    }
                    cachedPlan_.worldBounds.extendBy(bounds);
                }
            }

            const size_t shadedEnd =
                span.shadedItemBegin + span.shadedItemCount;
            for (size_t itemIndex = span.shadedItemBegin;
                    itemIndex < shadedEnd; ++itemIndex) {
                auto& item = cachedPlan_.shadedItems[itemIndex];
                if (!(item.rep.part == part))
                    return false;
                item.cullBackfaces =
                    geometryFound->second->shadedCullBackfaces &&
                    std::all_of(
                        visible.begin() + item.baseInstance,
                        visible.begin() + item.baseInstance +
                            item.instanceCount,
                        [](const Obol::internal::CadVisibleInstance&
                                occurrence) {
                            return occurrence.rgba[3] == 255 &&
                                cadTransformPreservesOrientation(
                                    occurrence.transform);
                        });
            }
        }

        geometryDelta.ranges.reserve(
            geometryDelta.ranges.size() +
            spansFound->second.size());
        for (const CachedPlanPartSpan& span :
                spansFound->second) {
            Obol::internal::CadPartGeometryRange range;
            range.partIndex = span.partIndex;
            range.baseInstance = span.baseInstance;
            range.instanceCount = span.instanceCount;
            range.shadedItemBegin = static_cast<uint32_t>(
                span.shadedItemBegin);
            range.shadedItemCount = static_cast<uint32_t>(
                span.shadedItemCount);
            geometryDelta.ranges.push_back(range);
        }
        return true;
    }

void SoCADAssemblyImpl::finishPartGeometryPatch(
            Obol::internal::CadPartGeometryDelta geometryDelta) {
        if (geometryDelta.ranges.empty())
            return;
        const bool subpixelProxyInputChanged =
            geometryDelta.subpixelProxyInputChanged;
        const bool boundsChanged =
            geometryDelta.boundsChanged;
        geometryDelta.revision =
            Obol::internal::cadTakeNonzeroIdentity(
                nextPartGeometryRevision_);
        cachedPlan_.partGeometryRevision =
            geometryDelta.revision;
        cachedPlan_.partGeometryDeltaEntryCount +=
            geometryDelta.ranges.size();
        cachedPlan_.partGeometryDeltas.push_back(
            std::move(geometryDelta));
        static constexpr size_t maxGeometryDeltaBatches = 256u;
        static constexpr size_t maxGeometryDeltaRanges = 65536u;
        while (cachedPlan_.partGeometryDeltas.size() >
                    maxGeometryDeltaBatches ||
                cachedPlan_.partGeometryDeltaEntryCount >
                    maxGeometryDeltaRanges) {
            const auto& front =
                cachedPlan_.partGeometryDeltas.front();
            cachedPlan_.partGeometryDeltaEntryCount -=
                front.ranges.size();
            cachedPlan_.partGeometryDeltaFloorRevision =
                std::max(
                    cachedPlan_.partGeometryDeltaFloorRevision,
                    front.revision);
            cachedPlan_.partGeometryDeltas.pop_front();
        }

        geometryRevision_ = Obol::internal::cadTakeNonzeroIdentity(
            nextGeometryRevision_);
        cachedPlan_.geometryRevision = geometryRevision_;
        cachedPlan_.revision = Obol::internal::cadTakeNonzeroIdentity(
            nextPlanRevision_);
        /*
         * Fixed channel/occurrence slots do not change shaded layout.  The
         * renderer consumes partGeometryRevision to patch the changed atlas
         * records.  Only a conservative proxy-input change requires the
         * assembly to reclassify all affected occurrences for this view.
         */
        if (subpixelProxyInputChanged) {
            cachedPlan_.subpixelProxyInputRevision =
                Obol::internal::cadTakeNonzeroIdentity(
                    nextSubpixelProxyInputRevision_);
            subpixelProxyStateInputRevision_ = 0;
            subpixelProxyViewValid_ = false;
        }
        if (boundsChanged)
            bvhDirty_ = true;
    }

bool SoCADAssemblyImpl::patchCachedInstanceCut(
            Obol::InstanceId instance, uint8_t lodCut,
            std::unordered_set<size_t>& changedGroups) {
        const auto fail = [&](const char *reason) {
            if (cadPlanDebugEnabled() &&
                    planDebugPatchMessageCount_++ <
                        cadPlanDebugMessageLimit())
                std::fprintf(stderr,
                    "SoCADAssembly sparse LoD classification rejected "
                    "reason=%s instance=%016llx:%016llx level=%u "
                    "plan_dirty=%d geometry_dirty=%d draw_mode=%d\n",
                    reason ? reason : "unknown",
                    static_cast<unsigned long long>(instance.w0),
                    static_cast<unsigned long long>(instance.w1),
                    static_cast<unsigned>(lodCut),
                    planDirty_ ? 1 : 0, geometryDirty_ ? 1 : 0,
                    cachedDrawModeDiagnostic());
            return false;
        };
        if (planDirty_ || geometryDirty_ || !cachedDrawMode_)
            return fail("plan-state");
        const auto retained = instances_.find(instance);
        if (retained == instances_.end())
            return fail("instance-not-retained");
        const auto indexed =
            progressivePlanIndexByInstance_.find(instance);
        if (indexed == progressivePlanIndexByInstance_.end()) {
            /*
             * A retained fallback with no active representation in this draw
             * mode has no compiled level to patch.  Updating its source record
             * is sufficient.
             */
            const bool inactive =
                cachedPlanPartSpansByPart_.find(
                retained->second.partId) ==
                    cachedPlanPartSpansByPart_.end();
            return inactive ? true : fail("active-instance-not-indexed");
        }
        if (indexed->second >= cachedPlan_.visibleInstances.size())
            return fail("invalid-visible-index");
        const auto& visible =
            cachedPlan_.visibleInstances[indexed->second];
        if (visible.partIndex >= cachedPlan_.partBindings.size())
            return fail("invalid-part-index");
        const auto& binding =
            cachedPlan_.partBindings[visible.partIndex];
        if (!binding.geometry)
            return fail("missing-binding-geometry");
        const bool progressiveShaded =
            binding.geometry->shaded &&
            binding.geometry->shaded->isProgressive();
        if (progressiveShaded)
            return patchProgressiveShadedPlanCut(
                instance, lodCut, changedGroups);

        /*
         * Wire ranges are selected per occurrence by every executor.  Their
         * immutable part payload and style batches therefore do not change
         * when an occurrence selects another range.  Ordinary meshes and
         * structural fallbacks likewise draw the same arrays at every level.
         */
        cachedPlan_.visibleInstances[indexed->second].lodCut =
            lodCut;
        return true;
    }

bool SoCADAssemblyImpl::patchProgressiveShadedPlanCut(
            Obol::InstanceId instance, uint8_t lodCut,
            std::unordered_set<size_t>& changedGroups) {
        const auto fail = [&](const char *reason) {
            ++progressivePlanPatchFailureCount_;
            if (cadPlanDebugEnabled() &&
                    planDebugPatchMessageCount_++ <
                        cadPlanDebugMessageLimit())
                std::fprintf(stderr,
                    "SoCADAssembly sparse LoD patch rejected reason=%s "
                    "instance=%016llx:%016llx level=%u "
                    "plan_dirty=%d geometry_dirty=%d draw_mode=%d "
                    "visible=%zu instances=%zu parts=%zu\n",
                    reason ? reason : "unknown",
                    static_cast<unsigned long long>(instance.w0),
                    static_cast<unsigned long long>(instance.w1),
                    static_cast<unsigned>(lodCut),
                    planDirty_ ? 1 : 0, geometryDirty_ ? 1 : 0,
                    cachedDrawModeDiagnostic(),
                    cachedPlan_.visibleInstances.size(), instances_.size(),
                    parts_.size());
            return false;
        };
        if (planDirty_ || geometryDirty_ ||
                !cachedDrawModeHasShaded())
            return fail("plan-state");
        const auto indexFound =
            progressivePlanIndexByInstance_.find(instance);
        if (indexFound == progressivePlanIndexByInstance_.end())
            return fail("instance-not-indexed");
        uint32_t index = indexFound->second;
        if (index >= cachedPlan_.visibleInstances.size())
            return fail("invalid-visible-index");
        auto& visible = cachedPlan_.visibleInstances;
        const auto instanceFound = instances_.find(instance);
        if (instanceFound == instances_.end())
            return fail("instance-not-retained");
        const auto groupFound =
            progressiveShadedPlanGroupByInstance_.find(instance);
        if (groupFound == progressiveShadedPlanGroupByInstance_.end())
            return fail("part-group-not-indexed");
        const size_t groupIndex = groupFound->second;
        if (groupIndex >= progressiveShadedPlanGroups_.size())
            return fail("invalid-group-index");
        ProgressiveShadedPlanGroup& group =
            progressiveShadedPlanGroups_[groupIndex];
        const size_t oldBin = progressiveCutBin(visible[index].lodCut);
        const size_t newBin = progressiveCutBin(lodCut);

        /* A sparse level patch is safe only while its retained bin
         * certificate still describes the compiled occurrence span.  An
         * underflow here turns one empty bin into UINT32_MAX draw
         * occurrences, which is both semantically invalid and an unbounded
         * renderer traversal.  Reject stale bookkeeping before the first
         * swap; the caller will rebuild the plan from authoritative instance
         * records. */
        uint64_t certifiedCount = 0;
        uint64_t oldBinBegin = group.baseInstance;
        for (size_t bin = 0; bin < group.cutCounts.size(); ++bin) {
            certifiedCount += group.cutCounts[bin];
            if (bin < oldBin)
                oldBinBegin += group.cutCounts[bin];
        }
        const size_t groupBase = group.baseInstance;
        const size_t groupCount = group.instanceCount;
        if (certifiedCount != groupCount)
            return fail("group-count-certificate");
        if (groupBase > visible.size() ||
                groupCount > visible.size() - groupBase)
            return fail("group-visible-span");
        if (index < groupBase || index >= groupBase + groupCount)
            return fail("instance-outside-group");
        if (group.cutCounts[oldBin] == 0u)
            return fail("empty-source-bin");
        if (oldBin != newBin &&
                group.cutCounts[newBin] ==
                    std::numeric_limits<uint32_t>::max())
            return fail("destination-bin-overflow");
        if (index < oldBinBegin ||
                index >= oldBinBegin + group.cutCounts[oldBin])
            return fail("instance-bin-mismatch");

        const auto swapVisible = [&](uint32_t left, uint32_t right) {
            if (left == right)
                return;
            const Obol::InstanceId leftInstance =
                visible[left].instanceId;
            const Obol::InstanceId rightInstance =
                visible[right].instanceId;
            std::swap(visible[left], visible[right]);
            /*
             * Screen-size proxy membership is independent of the PoP cut.
             * Keep all index-parallel classification state attached to the
             * occurrence when level bucketing swaps two records, avoiding a
             * complete scene reprojection at an unchanged camera.
             */
            if (subpixelProxyState_.size() == visible.size())
                std::swap(subpixelProxyState_[left],
                          subpixelProxyState_[right]);
            if (cachedPlan_.subpixelProxyMask.size() == visible.size())
                std::swap(cachedPlan_.subpixelProxyMask[left],
                          cachedPlan_.subpixelProxyMask[right]);
            if (subpixelProxyPointByVisible_.size() == visible.size())
                std::swap(subpixelProxyPointByVisible_[left],
                          subpixelProxyPointByVisible_[right]);
            if (subpixelProxyPointByVisible_.size() == visible.size()) {
                const uint32_t noPoint =
                    std::numeric_limits<uint32_t>::max();
                const uint32_t leftPoint =
                    subpixelProxyPointByVisible_[left];
                const uint32_t rightPoint =
                    subpixelProxyPointByVisible_[right];
                if (leftPoint != noPoint &&
                        leftPoint <
                            subpixelProxyVisibleByPoint_.size())
                    subpixelProxyVisibleByPoint_[leftPoint] = left;
                if (rightPoint != noPoint &&
                        rightPoint <
                            subpixelProxyVisibleByPoint_.size())
                    subpixelProxyVisibleByPoint_[rightPoint] = right;
            }
            /* A sparse box-to-mesh replacement retains its hidden compiled
             * tombstone and appends a new active slot with the same stable
             * InstanceId.  Moving that tombstone inside its old cut group
             * must not steal the active replacement's global index.  Update
             * only mappings which still owned the swapped source slot. */
            const auto updateOwnedIndex = [&](Obol::InstanceId moved,
                                               uint32_t source,
                                               uint32_t destination) {
                const auto found =
                    progressivePlanIndexByInstance_.find(moved);
                if (found != progressivePlanIndexByInstance_.end() &&
                        found->second == source)
                    found->second = destination;
            };
            updateOwnedIndex(leftInstance, left, right);
            updateOwnedIndex(rightInstance, right, left);
        };

        if (oldBin < newBin) {
            uint32_t boundary = group.baseInstance;
            for (size_t bin = 0; bin <= oldBin; ++bin)
                boundary += group.cutCounts[bin];
            swapVisible(index, boundary - 1);
            index = boundary - 1;
            for (size_t bin = oldBin + 1; bin <= newBin; ++bin) {
                boundary += group.cutCounts[bin];
                swapVisible(index, boundary - 1);
                index = boundary - 1;
            }
        } else if (oldBin > newBin) {
            uint32_t boundary = group.baseInstance;
            for (size_t bin = 0; bin < oldBin; ++bin)
                boundary += group.cutCounts[bin];
            swapVisible(index, boundary);
            index = boundary;
            for (size_t bin = oldBin; bin-- > newBin; ) {
                uint32_t previousBoundary = group.baseInstance;
                for (size_t prior = 0; prior < bin; ++prior)
                    previousBoundary += group.cutCounts[prior];
                swapVisible(index, previousBoundary);
                index = previousBoundary;
            }
        }
        visible[index].lodCut = lodCut;
        progressivePlanIndexByInstance_[instance] = index;
        if (oldBin != newBin) {
            --group.cutCounts[oldBin];
            ++group.cutCounts[newBin];
        }
        changedGroups.insert(groupIndex);
        return true;
    }

void SoCADAssemblyImpl::finishProgressiveShadedPlanPatch(
            const std::unordered_set<size_t>& changedGroups) {
        Obol::internal::CadShadedLodDelta delta;
        for (size_t groupIndex : changedGroups) {
            if (groupIndex >= progressiveShadedPlanGroups_.size())
                continue;
            ProgressiveShadedPlanGroup& group =
                progressiveShadedPlanGroups_[groupIndex];
            if (group.shadedItemBegin + group.shadedItemCount >
                    cachedPlan_.shadedItems.size())
                continue;
            const bool cullBackfaces =
                cachedPlan_.shadedItems[group.shadedItemBegin].cullBackfaces;
            size_t slot = 0;
            uint32_t base = group.baseInstance;
            for (size_t bin = 0;
                    bin < ProgressiveCutBinCount &&
                    slot < group.shadedItemCount; ++bin) {
                const uint32_t count = group.cutCounts[bin];
                if (!count)
                    continue;
                auto& item =
                    cachedPlan_.shadedItems[group.shadedItemBegin + slot++];
                item.rep.part = group.part;
                item.rep.type =
                    Obol::internal::CadRepType::Triangles;
                item.baseInstance = base;
                item.instanceCount = count;
                item.cullBackfaces = cullBackfaces;
                base += count;
            }
            while (slot < group.shadedItemCount) {
                auto& item =
                    cachedPlan_.shadedItems[group.shadedItemBegin + slot++];
                item.rep.part = group.part;
                item.rep.type =
                    Obol::internal::CadRepType::Triangles;
                item.baseInstance = group.baseInstance;
                item.instanceCount = 0;
                item.cullBackfaces = cullBackfaces;
            }
            uint8_t maximumCut = 0;
            for (size_t bin = 0; bin < ProgressiveCutBinCount; ++bin) {
                if (group.cutCounts[bin])
                    maximumCut = static_cast<uint8_t>(
                        std::min<size_t>(bin,
                            Obol::ProgressiveCutLimit - 1));
            }
            cachedPlan_.partPresentation[group.part].maximumRequestedCut =
                maximumCut;
            if (group.baseInstance <
                    cachedPlan_.visibleInstances.size() &&
                    group.shadedItemBegin <=
                        std::numeric_limits<uint32_t>::max() &&
                    group.shadedItemCount <=
                        std::numeric_limits<uint32_t>::max()) {
                Obol::internal::CadShadedLodRange range;
                range.partIndex =
                    cachedPlan_.visibleInstances[
                        group.baseInstance].partIndex;
                range.baseInstance = group.baseInstance;
                range.instanceCount = group.instanceCount;
                range.shadedItemBegin = static_cast<uint32_t>(
                    group.shadedItemBegin);
                range.shadedItemCount = static_cast<uint32_t>(
                    group.shadedItemCount);
                delta.ranges.push_back(range);
            }
        }
        cachedPlan_.revision = Obol::internal::cadTakeNonzeroIdentity(
            nextPlanRevision_);
        if (!changedGroups.empty()) {
            /*
             * The fixed group/item slots did not change structurally.
             * Publish a bounded LoD journal while leaving the layout stamp
             * stable, so retained renderers can patch only the affected
             * atlas demands, instance levels, and commands.
             */
            std::sort(delta.ranges.begin(), delta.ranges.end(),
                [](const auto& left, const auto& right) {
                    return left.baseInstance < right.baseInstance;
                });
            delta.ranges.erase(
                std::unique(delta.ranges.begin(), delta.ranges.end(),
                    [](const auto& left, const auto& right) {
                        return left.partIndex == right.partIndex &&
                            left.baseInstance == right.baseInstance;
                    }),
                delta.ranges.end());
            cachedPlan_.shadedLodRevision =
                Obol::internal::cadTakeNonzeroIdentity(
                    nextShadedLodRevision_);
            delta.revision = cachedPlan_.shadedLodRevision;
            cachedPlan_.shadedLodDeltaEntryCount +=
                delta.ranges.size();
            cachedPlan_.shadedLodDeltas.push_back(std::move(delta));
            static constexpr size_t maxDeltaBatches = 256u;
            static constexpr size_t maxDeltaRanges = 65536u;
            while (cachedPlan_.shadedLodDeltas.size() >
                        maxDeltaBatches ||
                    cachedPlan_.shadedLodDeltaEntryCount >
                        maxDeltaRanges) {
                const auto& front =
                    cachedPlan_.shadedLodDeltas.front();
                cachedPlan_.shadedLodDeltaEntryCount -=
                    front.ranges.size();
                cachedPlan_.shadedLodDeltaFloorRevision =
                    std::max(
                        cachedPlan_.shadedLodDeltaFloorRevision,
                        front.revision);
                cachedPlan_.shadedLodDeltas.pop_front();
            }
        }
    }
