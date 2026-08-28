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

#include <Obol/cad/SoCADAssembly.h>
#include <Obol/cad/CadProjectedProxy.h>
#include <Obol/cad/SoCADDetail.h>
#include <Obol/cad/SoCADViewState.h>
#include "CadAssemblyState.h"
#include "CadFramePlan.h"
#include "CadRendererGL.h"
#include "CadSoftwareWire.h"
#include "picking/CadPicking.h"

#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/actions/SoRayPickAction.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/actions/SoGetPrimitiveCountAction.h>
#include <Inventor/SoPickedPoint.h>
#include <Inventor/SoPath.h>
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

// ---------------------------------------------------------------------------
// SoCADAssemblyImpl – private implementation (Pimpl pattern)
// ---------------------------------------------------------------------------

static bool
cadDebugEnabled()
{
    static const bool enabled = []() {
        const char *env = std::getenv("OBOL_CAD_DEBUG");
        return env && env[0] && env[0] != '0';
    }();
    return enabled;
}

static bool
cadLightDebugEnabled()
{
    static const bool enabled = []() {
        const char *env = std::getenv("OBOL_CAD_LIGHT_DEBUG");
        return env && env[0] && env[0] != '0';
    }();
    return enabled;
}

static bool
cadPlanDebugEnabled()
{
    static const bool enabled = []() {
        const char *env = std::getenv("OBOL_CAD_PLAN_DEBUG");
        return env && env[0] && env[0] != '0';
    }();
    return enabled;
}

static uint64_t
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

static bool
cadSubpixelProxyProjection(
        const std::array<SbVec3f, 8>& corners,
        const Obol::internal::CadVisibleInstance& instance,
        const SbMatrix& viewProj, const SbVec2s& viewportSize,
        float pixelLimit, Obol::CadProjectedProxy& projected)
{
    if (viewportSize[0] <= 1 || viewportSize[1] <= 1 ||
            pixelLimit <= 0.0f)
        return false;

    SbMatrix model;
    model.setValue(instance.transform.data());
    projected = Obol::classifyCadProjectedProxy(
        corners.data(), model, viewProj, viewportSize, pixelLimit);
    return true;
}

enum class CadProxyPresentation : uint8_t {
    Geometry = 0u,
    Point = 1u,
    Offscreen = 2u
};

struct CadStructuralProjectionSample {
    bool structural = false;
    bool visible = false;
    bool collapsible = false;
    float maximumPixels = 0.0f;
};

static int8_t
cadStructuralProjectionBucket(const CadStructuralProjectionSample& sample)
{
    if (!sample.structural || !sample.visible)
        return -1;
    if (!sample.collapsible || !std::isfinite(sample.maximumPixels))
        return static_cast<int8_t>(
            Obol::CadStructuralProxyProjectionHistogram::BucketCount);
    float limit = 1.0f;
    for (size_t bucket = 0;
            bucket < Obol::CadStructuralProxyProjectionHistogram::BucketCount;
            ++bucket, limit *= 2.0f)
        if (sample.maximumPixels <= limit)
            return static_cast<int8_t>(bucket);
    return static_cast<int8_t>(
        Obol::CadStructuralProxyProjectionHistogram::BucketCount);
}

static void
cadUpdateStructuralProjectionHistogram(
        Obol::CadStructuralProxyProjectionHistogram& histogram,
        int8_t bucket, bool add)
{
    if (bucket < 0)
        return;
    if (add) {
        if (histogram.visibleCount != UINT64_MAX)
            ++histogram.visibleCount;
    } else if (histogram.visibleCount) {
        --histogram.visibleCount;
    }
    const size_t first = static_cast<size_t>(bucket);
    if (first >= histogram.cumulativeCount.size())
        return;
    for (size_t i = first; i < histogram.cumulativeCount.size(); ++i) {
        if (add) {
            if (histogram.cumulativeCount[i] != UINT64_MAX)
                ++histogram.cumulativeCount[i];
        } else if (histogram.cumulativeCount[i]) {
            --histogram.cumulativeCount[i];
        }
    }
}

static bool
cadSameSubpixelProxyPoints(
        const std::vector<Obol::internal::CadSubpixelProxyPoint>& left,
        const std::vector<Obol::internal::CadSubpixelProxyPoint>& right)
{
    if (left.size() != right.size())
        return false;
    for (size_t i = 0; i < left.size(); ++i) {
        const Obol::internal::CadSubpixelProxyPoint& a = left[i];
        const Obol::internal::CadSubpixelProxyPoint& b = right[i];
        if (a.instanceId != b.instanceId || a.rgba != b.rgba ||
                a.flags != b.flags ||
                a.position[0] != b.position[0] ||
                a.position[1] != b.position[1] ||
                a.position[2] != b.position[2])
            return false;
    }
    return true;
}

} // namespace

using InstanceData = Obol::internal::CadAssemblyInstanceData;

struct SoCADAssemblyImpl :
    Obol::internal::CadSceneDatabase,
    Obol::internal::CadPickingIndex,
    Obol::internal::CadPlanCache,
    Obol::internal::CadSubpixelClassifier,
    Obol::internal::CadRendererState
{

    // Rebuild instance BVH if dirty
    void rebuildBvhIfNeeded() {
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

    static SbBox3f partGeometryBounds(
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

    static bool partGeometryBoundsEqual(
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
    SbBox3f computeWorldBounds(const Obol::PartGeometry& geom,
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

    void updatePartGeometry(
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
        partGeneration_[pid] = nextGeneration_++;
        if (replacing) {
            partEdgeBvhCache_.erase(pid);
            partTriBvhCache_.erase(pid);
        }
        const bool progressive =
            (geom->shaded.has_value() && geom->shaded->isProgressive()) ||
            (geom->wire.has_value() && geom->wire->isProgressive());
        if (progressive)
            progressiveParts_.insert(pid);
        else if (replacing)
            progressiveParts_.erase(pid);
    }

    void recomputeWorldBoundsForPart(Obol::PartId pid) {
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

    void recomputeWorldBoundsForParts(
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

    void removeInstanceFromPartIndex(
            Obol::InstanceId iid, Obol::PartId pid) {
        const auto indexed = instanceIdsByPart_.find(pid);
        const auto slot = instancePartSlot_.find(iid);
        if (indexed == instanceIdsByPart_.end() ||
                slot == instancePartSlot_.end())
            return;
        InstancePartBucket& ids = indexed->second;
        size_t offset = slot->second;
        if (offset >= ids.size() || !(ids.at(offset) == iid)) {
            offset = ids.size();
            for (size_t candidate = 0; candidate < ids.size(); ++candidate) {
                if (ids.at(candidate) == iid) {
                    offset = candidate;
                    break;
                }
            }
            if (offset == ids.size()) {
                instancePartSlot_.erase(slot);
                return;
            }
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
            instancePartSlot_[replacement] = offset;
        }
        if (last == 0) {
            ids.hasFirst = false;
            ids.firstData = nullptr;
        } else {
            ids.additional.pop_back();
            ids.additionalData.pop_back();
        }
        instancePartSlot_.erase(iid);
        if (!ids.hasFirst)
            instanceIdsByPart_.erase(indexed);
    }

    void addInstanceToPartIndex(
            Obol::InstanceId iid, Obol::PartId pid,
            InstanceData *idata) {
        InstancePartBucket& ids = instanceIdsByPart_[pid];
        const size_t slot = ids.size();
        instancePartSlot_[iid] = slot;
        if (!ids.hasFirst) {
            ids.first = iid;
            ids.firstData = idata;
            ids.hasFirst = true;
        } else {
            ids.additional.push_back(iid);
            ids.additionalData.push_back(idata);
        }
    }

    void updateKnownInstance(Obol::InstanceId iid,
                             const Obol::InstanceRecord& rec,
                             InstanceData *prior) {
        InstanceData *idata = prior;
        if (!prior) {
            const auto inserted =
                instances_.try_emplace(iid, InstanceData());
            idata = &inserted.first->second;
            addInstanceToPartIndex(iid, rec.part, idata);
        } else if (!(prior->partId == rec.part)) {
            removeInstanceFromPartIndex(iid, prior->partId);
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

    void updateInstance(Obol::InstanceId iid,
                        const Obol::InstanceRecord& rec) {
        const auto prior = instances_.find(iid);
        updateKnownInstance(iid, rec,
            prior == instances_.end() ? nullptr : &prior->second);
    }

    void markDirty(const char *reason = "geometry-or-structure") {
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

    static size_t progressiveCutBin(uint8_t cut) {
        return cut < ProgressiveCutBinCount - 1 ?
            static_cast<size_t>(cut) : ProgressiveCutBinCount - 1;
    }

    static bool drawModeHasShadedPlan(int drawMode) {
        return drawMode == SoCADAssembly::SHADED ||
            drawMode == SoCADAssembly::SHADED_WITH_EDGES ||
            drawMode == SoCADAssembly::HIDDEN_LINE;
    }

    bool patchCachedInstanceFlags(Obol::InstanceId instance) {
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
                    planDirty_ ? 1 : 0, geometryDirty_ ? 1 : 0, cachedDM_,
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

    bool patchCachedInstanceStyle(Obol::InstanceId instance) {
        if (planDirty_ || geometryDirty_ ||
                cachedDM_ < SoCADAssembly::SHADED ||
                cachedDM_ > SoCADAssembly::HIDDEN_LINE)
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
        float r = 0.8f;
        float g = 0.8f;
        float b = 0.8f;
        float a = 1.0f;
        if (style.hasColorOverride) {
            r = style.color[0];
            g = style.color[1];
            b = style.color[2];
            a = style.color[3];
        }
        record.rgba[0] =
            static_cast<uint8_t>(std::min(255.0f, r * 255.0f));
        record.rgba[1] =
            static_cast<uint8_t>(std::min(255.0f, g * 255.0f));
        record.rgba[2] =
            static_cast<uint8_t>(std::min(255.0f, b * 255.0f));
        record.rgba[3] =
            static_cast<uint8_t>(std::min(255.0f, a * 255.0f));
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
        const bool hasWirePlan =
            cachedDM_ == SoCADAssembly::WIREFRAME ||
            cachedDM_ == SoCADAssembly::SHADED_WITH_EDGES ||
            cachedDM_ == SoCADAssembly::HIDDEN_LINE;
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

    void finishSparsePresentationPatch(bool visibilityChanged = false) {
        /* A partially classified point-proxy scratch result contains copied
         * flags.  Discard its cursor when a sparse attribute change lands so
         * the next bounded scan cannot publish stale selection/visibility. */
        subpixelProxyBuildActive_ = false;
        if (pendingSubpixelProxyChange_) {
            cachedPlan_.subpixelProxyRevision =
                nextSubpixelProxyRevision_++;
            if (nextSubpixelProxyRevision_ == 0)
                nextSubpixelProxyRevision_ = 1;
            pendingSubpixelProxyChange_ = false;
        }
        cachedPlan_.revision = nextPlanRevision_++;
        if (nextPlanRevision_ == 0)
            nextPlanRevision_ = 1;
        cachedPlan_.instanceAttributeRevision =
            nextInstanceAttributeRevision_++;
        if (nextInstanceAttributeRevision_ == 0)
            nextInstanceAttributeRevision_ = 1;
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
    Obol::internal::CadFramePlan buildFramePlan(
            int dm,
            const std::unordered_set<Obol::InstanceId,
                                     std::hash<Obol::InstanceId>>& selected,
            const std::unordered_set<Obol::InstanceId,
                                     std::hash<Obol::InstanceId>>& hidden,
            const std::map<Obol::PartId, InstancePartBucket> *buckets =
                nullptr,
            SoGLRenderAction *renderAction = nullptr) const
    {
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
            float r = 0.8f, g = 0.8f, b = 0.8f, a = 1.0f;
            if (idata.style.hasColorOverride) {
                r = idata.style.color[0];
                g = idata.style.color[1];
                b = idata.style.color[2];
                a = idata.style.color[3];
            }

            vi.rgba[0] = static_cast<uint8_t>(std::min(255.0f, r * 255.0f));
            vi.rgba[1] = static_cast<uint8_t>(std::min(255.0f, g * 255.0f));
            vi.rgba[2] = static_cast<uint8_t>(std::min(255.0f, b * 255.0f));
            vi.rgba[3] = static_cast<uint8_t>(std::min(255.0f, a * 255.0f));
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

        const bool needWire   = (dm == SoCADAssembly::WIREFRAME ||
                                 dm == SoCADAssembly::SHADED_WITH_EDGES ||
                                 dm == SoCADAssembly::HIDDEN_LINE);
        const bool needShaded = (dm == SoCADAssembly::SHADED ||
                                 dm == SoCADAssembly::SHADED_WITH_EDGES ||
                                 dm == SoCADAssembly::HIDDEN_LINE);

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

    bool rebuildProgressiveShadedPlanIndex(
            SoGLRenderAction *renderAction = nullptr) {
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
            if (!drawModeHasShadedPlan(cachedDM_)) {
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

    static bool partGeometryPlanCompatible(
            const Obol::PartGeometry& oldGeometry,
            const Obol::PartGeometry& newGeometry) {
        if (oldGeometry.points.has_value() !=
                newGeometry.points.has_value() ||
                oldGeometry.wire.has_value() !=
                newGeometry.wire.has_value() ||
                oldGeometry.shaded.has_value() !=
                newGeometry.shaded.has_value())
            return false;
        if (oldGeometry.wire &&
                oldGeometry.wire->isProgressive() !=
                    newGeometry.wire->isProgressive())
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
    bool appendCachedInstances(
            const std::vector<Obol::InstanceId>& instanceIds,
            bool allowReplacements = false) {
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
                    geometryDirty_ ? 1 : 0, cachedDM_,
                    cachedPlan_.visibleInstances.size(),
                    instances_.size(), parts_.size());
            return false;
        };
        if (instanceIds.empty() || planDirty_ || geometryDirty_ ||
                cachedDM_ < 0)
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
            cachedDM_, selected_, hidden_, &appendedBuckets);
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
            if (drawModeHasShadedPlan(cachedDM_) &&
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
        appendDelta.revision = nextAppendRevision_++;
        if (nextAppendRevision_ == 0)
            nextAppendRevision_ = 1;
        appendDelta.subpixelProxyInputRevision =
            nextSubpixelProxyInputRevision_++;
        if (nextSubpixelProxyInputRevision_ == 0)
            nextSubpixelProxyInputRevision_ = 1;
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
    bool canPatchCachedInstancePartRebind(
            Obol::InstanceId instance, Obol::PartId oldPart) const {
        if (planDirty_ || geometryDirty_ || cachedDM_ < 0)
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

    bool patchCachedInstancePartRebind(
            Obol::InstanceId instance, Obol::PartId oldPart,
            bool prevalidated = false) {
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
        std::memcpy(
            visible.transform.data(), data.localToRoot[0],
            16 * sizeof(float));
        float r = 0.8f, g = 0.8f, b = 0.8f, a = 1.0f;
        if (data.style.hasColorOverride) {
            r = data.style.color[0];
            g = data.style.color[1];
            b = data.style.color[2];
            a = data.style.color[3];
        }
        visible.rgba[0] = static_cast<uint8_t>(
            std::min(255.0f, r * 255.0f));
        visible.rgba[1] = static_cast<uint8_t>(
            std::min(255.0f, g * 255.0f));
        visible.rgba[2] = static_cast<uint8_t>(
            std::min(255.0f, b * 255.0f));
        visible.rgba[3] = static_cast<uint8_t>(
            std::min(255.0f, a * 255.0f));
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
        const bool needWire =
            cachedDM_ == SoCADAssembly::WIREFRAME ||
            cachedDM_ == SoCADAssembly::SHADED_WITH_EDGES ||
            cachedDM_ == SoCADAssembly::HIDDEN_LINE;
        const bool needShaded =
            cachedDM_ == SoCADAssembly::SHADED ||
            cachedDM_ == SoCADAssembly::SHADED_WITH_EDGES ||
            cachedDM_ == SoCADAssembly::HIDDEN_LINE;
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
            nextSubpixelProxyInputRevision_++;
        if (nextSubpixelProxyInputRevision_ == 0)
            nextSubpixelProxyInputRevision_ = 1;
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
    bool patchCachedInstancePartRebinds(
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

    void finishSparseStructuralPatch() {
        geometryRevision_ = nextGeometryRevision_++;
        if (nextGeometryRevision_ == 0)
            nextGeometryRevision_ = 1;
        cachedPlan_.geometryRevision = geometryRevision_;
        cachedPlan_.revision = nextPlanRevision_++;
        if (nextPlanRevision_ == 0)
            nextPlanRevision_ = 1;
        cachedPlan_.shadedLayoutRevision =
            nextShadedLayoutRevision_++;
        if (nextShadedLayoutRevision_ == 0)
            nextShadedLayoutRevision_ = 1;
    }

    /*
     * Replace the immutable arrays behind an existing retained part without
     * recompiling the assembly-wide instance plan.  Progressive population
     * growth changes a part generation and its resident prefix capacity, but
     * not the occurrence topology, styles, or level buckets.
     */
    bool patchCachedPartGeometry(
            Obol::PartId part,
            Obol::internal::CadPartGeometryDelta& geometryDelta,
            bool preservesBounds = false) {
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

    void finishPartGeometryPatch(
            Obol::internal::CadPartGeometryDelta geometryDelta) {
        if (geometryDelta.ranges.empty())
            return;
        const bool subpixelProxyInputChanged =
            geometryDelta.subpixelProxyInputChanged;
        const bool boundsChanged =
            geometryDelta.boundsChanged;
        geometryDelta.revision =
            nextPartGeometryRevision_++;
        if (nextPartGeometryRevision_ == 0)
            nextPartGeometryRevision_ = 1;
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

        geometryRevision_ = nextGeometryRevision_++;
        if (nextGeometryRevision_ == 0)
            nextGeometryRevision_ = 1;
        cachedPlan_.geometryRevision = geometryRevision_;
        cachedPlan_.revision = nextPlanRevision_++;
        if (nextPlanRevision_ == 0)
            nextPlanRevision_ = 1;
        /*
         * Fixed channel/occurrence slots do not change shaded layout.  The
         * renderer consumes partGeometryRevision to patch the changed atlas
         * records.  Only a conservative proxy-input change requires the
         * assembly to reclassify all affected occurrences for this view.
         */
        if (subpixelProxyInputChanged) {
            cachedPlan_.subpixelProxyInputRevision =
                nextSubpixelProxyInputRevision_++;
            if (nextSubpixelProxyInputRevision_ == 0)
                nextSubpixelProxyInputRevision_ = 1;
            subpixelProxyStateInputRevision_ = 0;
            subpixelProxyViewValid_ = false;
        }
        if (boundsChanged)
            bvhDirty_ = true;
    }

    bool patchCachedInstanceCut(
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
                    cachedDM_);
            return false;
        };
        if (planDirty_ || geometryDirty_ ||
                cachedDM_ < SoCADAssembly::WIREFRAME ||
                cachedDM_ > SoCADAssembly::HIDDEN_LINE)
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

    bool patchProgressiveShadedPlanCut(
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
                    planDirty_ ? 1 : 0, geometryDirty_ ? 1 : 0, cachedDM_,
                    cachedPlan_.visibleInstances.size(), instances_.size(),
                    parts_.size());
            return false;
        };
        if (planDirty_ || geometryDirty_ ||
                !drawModeHasShadedPlan(cachedDM_))
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
        ProgressiveShadedPlanGroup& group =
            progressiveShadedPlanGroups_[groupIndex];
        const size_t oldBin = progressiveCutBin(visible[index].lodCut);
        const size_t newBin = progressiveCutBin(lodCut);

        const auto swapVisible = [&](uint32_t left, uint32_t right) {
            if (left == right)
                return;
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
            progressivePlanIndexByInstance_[visible[left].instanceId] = left;
            progressivePlanIndexByInstance_[visible[right].instanceId] = right;
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

    void finishProgressiveShadedPlanPatch(
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
        cachedPlan_.revision = nextPlanRevision_++;
        if (nextPlanRevision_ == 0)
            nextPlanRevision_ = 1;
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
                nextShadedLodRevision_++;
            if (nextShadedLodRevision_ == 0)
                nextShadedLodRevision_ = 1;
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

    CadProxyPresentation subpixelProxyPresentationForOccurrence(
            const Obol::internal::CadFramePlan& plan,
            size_t visibleIndex,
            const SbMatrix& viewProj,
            const SbVec2s& viewportSize,
            float pixelThreshold,
            bool wasCollapsed,
            Obol::internal::CadSubpixelProxyPoint& replacement,
            CadStructuralProjectionSample *structuralSample = nullptr) const
    {
        using namespace Obol::internal;
        if (structuralSample)
            *structuralSample = CadStructuralProjectionSample();
        if (visibleIndex >= plan.visibleInstances.size())
            return CadProxyPresentation::Geometry;
        const CadVisibleInstance& instance =
            plan.visibleInstances[visibleIndex];
        if (instance.flags & CadInstanceHidden)
            return CadProxyPresentation::Offscreen;
        if (instance.partIndex >= plan.partBindings.size())
            return CadProxyPresentation::Geometry;
        const CadPartBinding& binding =
            plan.partBindings[instance.partIndex];
        /* A malformed structural fallback without validated projection
         * corners cannot enter the point channel, but it is still submitted
         * as geometry and therefore remains a conservative visible loader
         * obligation.  A valid projection below may prove it off-screen. */
        if (structuralSample &&
                (instance.flags & CadInstanceLodStructuralProxy) &&
                binding.geometry) {
            structuralSample->structural = true;
            structuralSample->visible = true;
        }
        if (!binding.subpixelProxyEligible)
            return CadProxyPresentation::Geometry;
        const bool wireActive =
            cachedDM_ == SoCADAssembly::WIREFRAME ||
            cachedDM_ == SoCADAssembly::SHADED_WITH_EDGES ||
            cachedDM_ == SoCADAssembly::HIDDEN_LINE;
        const bool shadedActive =
            cachedDM_ == SoCADAssembly::SHADED ||
            cachedDM_ == SoCADAssembly::SHADED_WITH_EDGES ||
            cachedDM_ == SoCADAssembly::HIDDEN_LINE;
        /* A structural fallback is intentionally a wire box in every scene
         * draw mode.  Its replacement eligibility therefore follows that
         * fallback channel, not the user's eventual mesh mode.  Requiring a
         * shaded representation here forced shaded large-model views to load
         * thousands of minimum meshes before the same screen-space extent
         * could enter the aggregate point batch. */
        const bool structuralFallbackActive =
            (instance.flags & CadInstanceLodStructuralProxy) &&
            binding.geometry && binding.geometry->wire.has_value();
        if (!binding.geometry ||
                !((wireActive && binding.geometry->wire) ||
                  (shadedActive && binding.geometry->shaded) ||
                  structuralFallbackActive))
            return CadProxyPresentation::Geometry;
        /* A structural fallback has no stable geometry presentation worth
         * protecting with a Schmitt band: it either supplies the exact leaf
         * extent or is atomically replaced by its mesh.  Classify that box at
         * the declared pixel boundary so a 0.8-pixel leaf does not trigger a
         * cache load merely because ordinary retained meshes use a 0.75/1.25
         * anti-flicker band. */
        const bool structuralFallback =
            (instance.flags & CadInstanceLodStructuralProxy) != 0;
        const float threshold = structuralFallback ? pixelThreshold :
            pixelThreshold * (wasCollapsed ? 1.25f : 0.75f);
        Obol::CadProjectedProxy projected;
        if (!cadSubpixelProxyProjection(
                binding.subpixelProxyCorners, instance, viewProj,
                viewportSize, threshold, projected))
            return CadProxyPresentation::Geometry;
        if (structuralSample && structuralFallback) {
            structuralSample->structural = true;
            structuralSample->visible = projected.visible;
            structuralSample->collapsible = projected.fullyContained &&
                !pointProxyProtected_.count(instance.instanceId);
            structuralSample->maximumPixels = std::max(
                projected.pixelWidth, projected.pixelHeight);
        }
        /* A fully clipped occurrence is neither an uncollapsed structural
         * fallback nor a convergence obligation.  Mark it in the same
         * per-occurrence suppression mask used by point replacement, but do
         * not emit a point.  The previous binary state treated every
         * off-frustum structural box as visible geometry; the view-aware
         * loader would correctly retire its mesh and the controller would
         * then schedule an endless box-repair pass which could never make
         * progress. */
        if (!projected.visible)
            return CadProxyPresentation::Offscreen;
        /* Point-proxy protection is a view-allocation policy which may be
         * atomically adopted after the immutable frame plan was built.  Read
         * its authoritative set here rather than requiring an O(instances)
         * flag rewrite at transaction commit.  Selection remains an ordinary
         * sparse style/identity attribute: applications which need a selected
         * occurrence promoted for editing put it in the explicit protection
         * set.  This lets bulk selection retain bounded point presentation. */
        if (pointProxyProtected_.count(instance.instanceId))
            return CadProxyPresentation::Geometry;
        if (!projected.pointEligible)
            return CadProxyPresentation::Geometry;

        replacement.position = projected.point;
        replacement.rgba = instance.rgba;
        replacement.instanceId = instance.instanceId;
        replacement.flags = instance.flags;
        return CadProxyPresentation::Point;
    }

    void updateStructuralProjectionForVisible(size_t visibleIndex)
    {
        using namespace Obol::internal;
        CadFramePlan& plan = cachedPlan_;
        if (!structuralProjectionHistogram_.exact)
            return;
        if (!subpixelProxyViewValid_ ||
                subpixelProxyViewInputRevision_ !=
                    plan.subpixelProxyInputRevision ||
                visibleIndex >= plan.visibleInstances.size() ||
                visibleIndex >=
                    structuralProjectionBucketByVisible_.size()) {
            structuralProjectionHistogram_.exact = false;
            return;
        }
        const int8_t oldBucket =
            structuralProjectionBucketByVisible_[visibleIndex];
        CadSubpixelProxyPoint ignored;
        CadStructuralProjectionSample sample;
        (void)subpixelProxyPresentationForOccurrence(
            plan, visibleIndex, subpixelProxyViewProj_,
            subpixelProxyViewportSize_, subpixelProxyPixelThreshold_,
            subpixelProxyState_[visibleIndex] != 0u, ignored, &sample);
        const int8_t newBucket = cadStructuralProjectionBucket(sample);
        if (oldBucket == newBucket)
            return;
        cadUpdateStructuralProjectionHistogram(
            structuralProjectionHistogram_, oldBucket, false);
        cadUpdateStructuralProjectionHistogram(
            structuralProjectionHistogram_, newBucket, true);
        structuralProjectionBucketByVisible_[visibleIndex] = newBucket;
        structuralProjectionHistogram_.revision =
            nextStructuralProjectionRevision_++;
        if (nextStructuralProjectionRevision_ == 0)
            nextStructuralProjectionRevision_ = 1;
    }

    bool patchSubpixelProxyGeometryForVisible(
            size_t visibleIndex, uint64_t priorInputRevision)
    {
        using namespace Obol::internal;
        CadFramePlan& plan = cachedPlan_;
        const uint32_t noPoint = std::numeric_limits<uint32_t>::max();
        if (!subpixelProxyViewValid_ || !priorInputRevision ||
                subpixelProxyStateInputRevision_ != priorInputRevision ||
                subpixelProxyViewInputRevision_ != priorInputRevision ||
                visibleIndex >= plan.visibleInstances.size() ||
                visibleIndex >= plan.subpixelProxyMask.size() ||
                visibleIndex >= subpixelProxyState_.size() ||
                visibleIndex >= subpixelProxyPointByVisible_.size())
            return false;

        const uint32_t currentPoint =
            subpixelProxyPointByVisible_[visibleIndex];
        if (currentPoint != noPoint &&
                (currentPoint >= plan.subpixelProxyPoints.size() ||
                 currentPoint >= subpixelProxyVisibleByPoint_.size()))
            return false;
        if (currentPoint != noPoint) {
            const uint32_t last = static_cast<uint32_t>(
                plan.subpixelProxyPoints.size() - 1u);
            if (currentPoint != last) {
                plan.subpixelProxyPoints[currentPoint] =
                    std::move(plan.subpixelProxyPoints[last]);
                const uint32_t movedVisible =
                    subpixelProxyVisibleByPoint_[last];
                if (movedVisible >=
                        subpixelProxyPointByVisible_.size())
                    return false;
                subpixelProxyVisibleByPoint_[currentPoint] = movedVisible;
                subpixelProxyPointByVisible_[movedVisible] = currentPoint;
            }
            plan.subpixelProxyPoints.pop_back();
            subpixelProxyVisibleByPoint_.pop_back();
        }
        subpixelProxyPointByVisible_[visibleIndex] = noPoint;
        plan.subpixelProxyMask[visibleIndex] = 0u;
        subpixelProxyState_[visibleIndex] = 0u;

        if (visibleIndex <
                structuralProjectionBucketByVisible_.size()) {
            cadUpdateStructuralProjectionHistogram(
                structuralProjectionHistogram_,
                structuralProjectionBucketByVisible_[visibleIndex], false);
            structuralProjectionBucketByVisible_[visibleIndex] = -1;
        } else if (structuralProjectionHistogram_.exact) {
            structuralProjectionHistogram_.exact = false;
        }

        CadSubpixelProxyPoint replacement;
        CadStructuralProjectionSample structuralSample;
        const CadProxyPresentation presentation =
            subpixelProxyPresentationForOccurrence(
                plan, visibleIndex, subpixelProxyViewProj_,
                subpixelProxyViewportSize_, subpixelProxyPixelThreshold_,
                false, replacement, &structuralSample);
        const int8_t structuralBucket =
            cadStructuralProjectionBucket(structuralSample);
        if (visibleIndex <
                structuralProjectionBucketByVisible_.size()) {
            structuralProjectionBucketByVisible_[visibleIndex] =
                structuralBucket;
            cadUpdateStructuralProjectionHistogram(
                structuralProjectionHistogram_, structuralBucket, true);
        }
        subpixelProxyState_[visibleIndex] =
            static_cast<uint8_t>(presentation);
        if (presentation != CadProxyPresentation::Geometry)
            plan.subpixelProxyMask[visibleIndex] = 1u;
        if (presentation == CadProxyPresentation::Point) {
            subpixelProxyPointByVisible_[visibleIndex] =
                static_cast<uint32_t>(plan.subpixelProxyPoints.size());
            plan.subpixelProxyPoints.push_back(std::move(replacement));
            subpixelProxyVisibleByPoint_.push_back(
                static_cast<uint32_t>(visibleIndex));
        }

        plan.subpixelProxyRevision = nextSubpixelProxyRevision_++;
        if (nextSubpixelProxyRevision_ == 0)
            nextSubpixelProxyRevision_ = 1;
        if (structuralProjectionHistogram_.exact) {
            structuralProjectionHistogram_.revision =
                nextStructuralProjectionRevision_++;
            if (nextStructuralProjectionRevision_ == 0)
                nextStructuralProjectionRevision_ = 1;
        }
        return true;
    }

    /* Keep selected geometry visually inspectable even when its conservative
     * bounds are below the ordinary small-part threshold.  Selection is a
     * sparse presentation property, so promote/demote just this occurrence
     * and preserve the camera-local classification for every other record. */
    bool updateProtectedSubpixelProxy(
            size_t visibleIndex, bool protectedInstance)
    {
        using namespace Obol::internal;
        CadFramePlan& plan = cachedPlan_;
        const uint32_t noPoint =
            std::numeric_limits<uint32_t>::max();
        if (visibleIndex >= plan.visibleInstances.size() ||
                visibleIndex >= plan.subpixelProxyMask.size() ||
                visibleIndex >= subpixelProxyState_.size() ||
                visibleIndex >= subpixelProxyPointByVisible_.size())
            return false;

        const uint32_t currentPoint =
            subpixelProxyPointByVisible_[visibleIndex];
        if (protectedInstance) {
            if (!plan.subpixelProxyMask[visibleIndex])
                return false;
            if (currentPoint == noPoint ||
                    currentPoint >= plan.subpixelProxyPoints.size() ||
                    currentPoint >= subpixelProxyVisibleByPoint_.size())
                return false;
            const uint32_t last = static_cast<uint32_t>(
                plan.subpixelProxyPoints.size() - 1u);
            if (currentPoint != last) {
                plan.subpixelProxyPoints[currentPoint] =
                    std::move(plan.subpixelProxyPoints[last]);
                const uint32_t movedVisible =
                    subpixelProxyVisibleByPoint_[last];
                subpixelProxyVisibleByPoint_[currentPoint] =
                    movedVisible;
                if (movedVisible <
                        subpixelProxyPointByVisible_.size())
                    subpixelProxyPointByVisible_[movedVisible] =
                        currentPoint;
            }
            plan.subpixelProxyPoints.pop_back();
            subpixelProxyVisibleByPoint_.pop_back();
            subpixelProxyPointByVisible_[visibleIndex] = noPoint;
            plan.subpixelProxyMask[visibleIndex] = 0u;
            subpixelProxyState_[visibleIndex] = 0u;
            return true;
        }

        if (plan.subpixelProxyMask[visibleIndex] ||
                !subpixelProxyViewValid_ ||
                subpixelProxyViewInputRevision_ !=
                    plan.subpixelProxyInputRevision)
            return false;
        CadSubpixelProxyPoint replacement;
        if (subpixelProxyPresentationForOccurrence(
                plan, visibleIndex, subpixelProxyViewProj_,
                subpixelProxyViewportSize_,
                subpixelProxyPixelThreshold_, false, replacement) !=
                CadProxyPresentation::Point)
            return false;
        subpixelProxyPointByVisible_[visibleIndex] =
            static_cast<uint32_t>(plan.subpixelProxyPoints.size());
        plan.subpixelProxyPoints.push_back(std::move(replacement));
        subpixelProxyVisibleByPoint_.push_back(
            static_cast<uint32_t>(visibleIndex));
        plan.subpixelProxyMask[visibleIndex] = 1u;
        subpixelProxyState_[visibleIndex] = 1u;
        return true;
    }

    void refreshWireProxyParts(
            const std::unordered_set<Obol::PartId,
                                     std::hash<Obol::PartId>>& parts)
    {
        using namespace Obol::internal;
        CadFramePlan& plan = cachedPlan_;
        for (const Obol::PartId part : parts) {
            const auto previous =
                uncollapsedStructuralProxyCountByPart_.find(part);
            if (previous !=
                    uncollapsedStructuralProxyCountByPart_.end()) {
                uncollapsedStructuralProxyCount_ = previous->second <=
                        uncollapsedStructuralProxyCount_ ?
                    uncollapsedStructuralProxyCount_ - previous->second :
                    0u;
                uncollapsedStructuralProxyCountByPart_.erase(previous);
            }
            auto presentation = plan.partPresentation.find(part);
            if (presentation != plan.partPresentation.end())
                presentation->second.wireHasUncollapsedInstances = false;
            bool hasUncollapsed = false;
            size_t structuralCount = 0u;
            const auto spans = cachedPlanPartSpansByPart_.find(part);
            if (spans == cachedPlanPartSpansByPart_.end())
                continue;
            for (const CachedPlanPartSpan& span : spans->second) {
                /* The compiled plan, not the requested display-mode field,
                 * is authoritative during a sparse transition.  A mixed
                 * box-to-mesh plan may still own wire fallback items while a
                 * pending mode update already says SHADED. */
                if (!span.wireItemCount ||
                        span.partIndex >= plan.partBindings.size())
                    continue;
                const CadPartBinding& binding =
                    plan.partBindings[span.partIndex];
                if (!binding.geometry || !binding.geometry->wire)
                    continue;
                for (uint32_t offset = 0;
                        offset < span.instanceCount; ++offset) {
                    const size_t visibleIndex =
                        span.baseInstance + offset;
                    if (visibleIndex >= plan.visibleInstances.size())
                        continue;
                    const CadVisibleInstance& occurrence =
                        plan.visibleInstances[visibleIndex];
                    /* The published structural frontier is an occurrence
                     * set.  Sparse box-to-mesh/selection/visibility changes
                     * update only the affected occurrence records. */
                    uncollapsedStructuralProxyInstances_.erase(
                        occurrence.instanceId);
                    if (occurrence.partIndex != span.partIndex ||
                            (occurrence.flags & CadInstanceHidden) ||
                            (visibleIndex <
                                 plan.subpixelProxyMask.size() &&
                             plan.subpixelProxyMask[visibleIndex]))
                        continue;
                    hasUncollapsed = true;
                    if (occurrence.flags &
                            CadInstanceLodStructuralProxy) {
                        ++structuralCount;
                        uncollapsedStructuralProxyInstances_.insert(
                            occurrence.instanceId);
                    }
                }
            }
            if (presentation != plan.partPresentation.end())
                presentation->second.wireHasUncollapsedInstances =
                    hasUncollapsed;
            if (structuralCount) {
                uncollapsedStructuralProxyCountByPart_[part] =
                    structuralCount;
                uncollapsedStructuralProxyCount_ += structuralCount;
            }
        }
    }

    bool patchSubpixelProxyAppendPlan(
            const SbMatrix& viewProj,
            const SbVec2s& viewportSize,
            float pixelThreshold)
    {
        using namespace Obol::internal;
        CadFramePlan& plan = cachedPlan_;
        if (!subpixelProxyViewValid_ ||
                subpixelProxyViewInputRevision_ !=
                    subpixelProxyStateInputRevision_ ||
                subpixelProxyStateInputRevision_ == 0 ||
                subpixelProxyViewportSize_[0] != viewportSize[0] ||
                subpixelProxyViewportSize_[1] != viewportSize[1] ||
                subpixelProxyPixelThreshold_ != pixelThreshold ||
                !(subpixelProxyViewProj_ == viewProj) ||
                subpixelProxyClassifiedAppendRevision_ <
                    plan.appendDeltaFloorRevision ||
                !structuralProjectionHistogram_.exact ||
                structuralProjectionBucketByVisible_.size() !=
                    subpixelProxyState_.size())
            return false;

        std::vector<const CadPlanAppendDelta *> deltas;
        size_t expectedVisibleExtent = subpixelProxyState_.size();
        for (const CadPlanAppendDelta& delta : plan.appendDeltas) {
            if (delta.revision <=
                    subpixelProxyClassifiedAppendRevision_)
                continue;
            if (delta.visibleBegin != expectedVisibleExtent ||
                    delta.visibleBegin >
                        plan.visibleInstances.size() ||
                    delta.visibleCount >
                        plan.visibleInstances.size() -
                            delta.visibleBegin ||
                    !delta.subpixelProxyInputRevision)
                return false;
            for (const uint32_t retired :
                    delta.retiredVisibleIndices)
                if (retired >= delta.visibleBegin)
                    return false;
            expectedVisibleExtent += delta.visibleCount;
            deltas.push_back(&delta);
        }
        if (deltas.empty() ||
                expectedVisibleExtent !=
                    plan.visibleInstances.size() ||
                deltas.back()->subpixelProxyInputRevision !=
                    plan.subpixelProxyInputRevision)
            return false;

        const uint32_t noPoint =
            std::numeric_limits<uint32_t>::max();
        std::unordered_set<Obol::PartId,
                           std::hash<Obol::PartId>> affectedParts;
        affectedParts.reserve(deltas.size() * 2u);
        const auto removePointForVisible =
            [&](uint32_t visibleIndex) {
                if (visibleIndex >=
                        subpixelProxyPointByVisible_.size())
                    return false;
                const uint32_t pointIndex =
                    subpixelProxyPointByVisible_[visibleIndex];
                if (pointIndex == noPoint)
                    return true;
                if (pointIndex >= plan.subpixelProxyPoints.size() ||
                        pointIndex >=
                            subpixelProxyVisibleByPoint_.size())
                    return false;
                const size_t last =
                    plan.subpixelProxyPoints.size() - 1u;
                if (pointIndex != last) {
                    const uint32_t movedVisible =
                        subpixelProxyVisibleByPoint_[last];
                    if (movedVisible >=
                            subpixelProxyPointByVisible_.size())
                        return false;
                    plan.subpixelProxyPoints[pointIndex] =
                        std::move(plan.subpixelProxyPoints[last]);
                    subpixelProxyVisibleByPoint_[pointIndex] =
                        movedVisible;
                    subpixelProxyPointByVisible_[movedVisible] =
                        pointIndex;
                }
                plan.subpixelProxyPoints.pop_back();
                subpixelProxyVisibleByPoint_.pop_back();
                subpixelProxyPointByVisible_[visibleIndex] =
                    noPoint;
                return true;
            };

        for (const CadPlanAppendDelta *delta : deltas) {
            for (const uint32_t retired :
                    delta->retiredVisibleIndices) {
                if (retired >= plan.visibleInstances.size() ||
                        !removePointForVisible(retired))
                    return false;
                if (retired <
                        structuralProjectionBucketByVisible_.size()) {
                    cadUpdateStructuralProjectionHistogram(
                        structuralProjectionHistogram_,
                        structuralProjectionBucketByVisible_[retired], false);
                    structuralProjectionBucketByVisible_[retired] = -1;
                }
                const CadVisibleInstance& old =
                    plan.visibleInstances[retired];
                if (old.partIndex < plan.partBindings.size())
                    affectedParts.insert(
                        plan.partBindings[old.partIndex].part);
                if (retired < subpixelProxyState_.size())
                    subpixelProxyState_[retired] = 0u;
                if (retired < plan.subpixelProxyMask.size())
                    plan.subpixelProxyMask[retired] = 0u;
            }

            const size_t newExtent =
                static_cast<size_t>(delta->visibleBegin) +
                delta->visibleCount;
            subpixelProxyState_.resize(newExtent, 0u);
            plan.subpixelProxyMask.resize(newExtent, 0u);
            subpixelProxyPointByVisible_.resize(
                newExtent, noPoint);
            structuralProjectionBucketByVisible_.resize(newExtent, -1);
            for (size_t visibleIndex = delta->visibleBegin;
                    visibleIndex < newExtent; ++visibleIndex) {
                const CadVisibleInstance& occurrence =
                    plan.visibleInstances[visibleIndex];
                if (occurrence.partIndex < plan.partBindings.size())
                    affectedParts.insert(
                        plan.partBindings[
                            occurrence.partIndex].part);
                CadSubpixelProxyPoint replacement;
                CadStructuralProjectionSample structuralSample;
                const CadProxyPresentation presentation =
                    subpixelProxyPresentationForOccurrence(
                        plan, visibleIndex, viewProj, viewportSize,
                        pixelThreshold, false, replacement,
                        &structuralSample);
                const int8_t structuralBucket =
                    cadStructuralProjectionBucket(structuralSample);
                structuralProjectionBucketByVisible_[visibleIndex] =
                    structuralBucket;
                cadUpdateStructuralProjectionHistogram(
                    structuralProjectionHistogram_, structuralBucket, true);
                const bool collapsed =
                    presentation == CadProxyPresentation::Point;
                const bool suppressed =
                    presentation != CadProxyPresentation::Geometry;
                subpixelProxyState_[visibleIndex] =
                    static_cast<uint8_t>(presentation);
                plan.subpixelProxyMask[visibleIndex] =
                    suppressed ? 1u : 0u;
                if (!collapsed)
                    continue;
                subpixelProxyPointByVisible_[visibleIndex] =
                    static_cast<uint32_t>(
                        plan.subpixelProxyPoints.size());
                plan.subpixelProxyPoints.push_back(
                    std::move(replacement));
                subpixelProxyVisibleByPoint_.push_back(
                    static_cast<uint32_t>(visibleIndex));
            }
        }

        refreshWireProxyParts(affectedParts);
        plan.subpixelProxySourceInputRevision =
            plan.subpixelProxyInputRevision;
        plan.subpixelProxyRevision = nextSubpixelProxyRevision_++;
        if (nextSubpixelProxyRevision_ == 0)
            nextSubpixelProxyRevision_ = 1;
        structuralProjectionHistogram_.revision =
            nextStructuralProjectionRevision_++;
        if (nextStructuralProjectionRevision_ == 0)
            nextStructuralProjectionRevision_ = 1;
        structuralProjectionHistogram_.exact = true;
        subpixelProxyClassifiedAppendRevision_ =
            plan.appendRevision;
        subpixelProxyStateInputRevision_ =
            plan.subpixelProxyInputRevision;
        subpixelProxyViewInputRevision_ =
            plan.subpixelProxyInputRevision;
        return true;
    }

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
            const SbMatrix& viewProj,
            const SbVec2s& viewportSize,
            float pixelThreshold)
    {
        using namespace Obol::internal;
        CadFramePlan& plan = cachedPlan_;
        if (!subpixelProxyBuildActive_ ||
                subpixelProxyBuildAppendRevision_ == 0 ||
                subpixelProxyBuildAppendRevision_ <
                    plan.appendDeltaFloorRevision ||
                subpixelProxyBuildViewportSize_[0] != viewportSize[0] ||
                subpixelProxyBuildViewportSize_[1] != viewportSize[1] ||
                subpixelProxyBuildPixelThreshold_ != pixelThreshold ||
                !(subpixelProxyBuildViewProj_ == viewProj))
            return false;

        size_t expectedVisibleExtent = subpixelProxyScratchMask_.size();
        uint64_t newestAppendRevision =
            subpixelProxyBuildAppendRevision_;
        uint64_t newestInputRevision =
            subpixelProxyBuildInputRevision_;
        bool found = false;
        for (const CadPlanAppendDelta& delta : plan.appendDeltas) {
            if (delta.revision <=
                    subpixelProxyBuildAppendRevision_)
                continue;
            if (!delta.retiredVisibleIndices.empty() ||
                    delta.visibleBegin != expectedVisibleExtent ||
                    delta.visibleBegin >
                        plan.visibleInstances.size() ||
                    delta.visibleCount >
                        plan.visibleInstances.size() -
                            delta.visibleBegin ||
                    !delta.subpixelProxyInputRevision)
                return false;
            expectedVisibleExtent += delta.visibleCount;
            newestAppendRevision = delta.revision;
            newestInputRevision =
                delta.subpixelProxyInputRevision;
            found = true;
        }
        if (!found ||
                expectedVisibleExtent !=
                    plan.visibleInstances.size() ||
                newestAppendRevision != plan.appendRevision ||
                newestInputRevision !=
                    plan.subpixelProxyInputRevision)
            return false;

        const uint32_t noPoint =
            std::numeric_limits<uint32_t>::max();
        subpixelProxyState_.resize(expectedVisibleExtent, 0u);
        subpixelProxyScratchMask_.resize(expectedVisibleExtent, 0u);
        subpixelProxyScratchPoints_.reserve(expectedVisibleExtent);
        subpixelProxyScratchVisibleByPoint_.reserve(expectedVisibleExtent);
        subpixelProxyScratchPointByVisible_.resize(
            expectedVisibleExtent, noPoint);
        structuralProjectionScratchBucketByVisible_.resize(
            expectedVisibleExtent, -1);
        subpixelProxyScratchWireByPart_.resize(
            plan.partBindings.size(), 0u);
        subpixelProxyScratchStructuralCountByPart_.resize(
            plan.partBindings.size(), 0u);
        subpixelProxyScratchStructuralInstances_.reserve(
            expectedVisibleExtent);
        subpixelProxyStateInputRevision_ = newestInputRevision;
        subpixelProxyBuildInputRevision_ = newestInputRevision;
        subpixelProxyBuildAppendRevision_ = newestAppendRevision;
        return true;
    }

    Obol::CadPresentationPreparationTarget subpixelPreparationTarget(
            const SbMatrix& viewProj, const SbVec2s& viewportSize,
            float pixelThreshold)
    {
        Obol::CadPresentationPreparationTarget target;
        target.kind =
            Obol::CadPresentationPreparationKind::SubpixelClassification;
        target.planRevision = cachedPlan_.revision;
        target.geometryRevision = cachedPlan_.geometryRevision;
        target.classifierInputRevision =
            cachedPlan_.subpixelProxyInputRevision;
        target.classifierAppendRevision = cachedPlan_.appendRevision;
        target.viewportWidth = viewportSize[0];
        target.viewportHeight = viewportSize[1];
        std::memcpy(&target.pointProxyPixelThresholdBits,
            &pixelThreshold,
            sizeof(target.pointProxyPixelThresholdBits));
        for (size_t i = 0; i < target.viewProjectionBits.size(); ++i)
            std::memcpy(&target.viewProjectionBits[i],
                viewProj[0] + i, sizeof(target.viewProjectionBits[i]));

        if (presentationPreparation_.state ==
                Obol::CadPresentationPreparationState::Preparing) {
            Obol::CadPresentationPreparationTarget active =
                presentationPreparation_.target;
            active.obligationRevision = 0;
            if (active == target)
                return presentationPreparation_.target;
        }
        target.obligationRevision =
            nextPresentationPreparationRevision_++;
        if (!nextPresentationPreparationRevision_)
            nextPresentationPreparationRevision_ = 1;
        return target;
    }

    uint64_t subpixelPreparationReservedBytes() const
    {
        uint64_t total = 0;
        const auto add = [&total](size_t count, size_t elementSize) {
            const uint64_t bytes = count &&
                    elementSize > UINT64_MAX / count ?
                UINT64_MAX :
                static_cast<uint64_t>(count) * elementSize;
            total = bytes > UINT64_MAX - total ?
                UINT64_MAX : total + bytes;
        };
        add(subpixelProxyScratchMask_.capacity(), sizeof(uint8_t));
        add(subpixelProxyScratchPoints_.capacity(),
            sizeof(Obol::internal::CadSubpixelProxyPoint));
        add(subpixelProxyScratchVisibleByPoint_.capacity(),
            sizeof(uint32_t));
        add(subpixelProxyScratchPointByVisible_.capacity(),
            sizeof(uint32_t));
        add(structuralProjectionScratchBucketByVisible_.capacity(),
            sizeof(int8_t));
        add(subpixelProxyScratchWireByPart_.capacity(), sizeof(uint8_t));
        add(subpixelProxyScratchStructuralCountByPart_.capacity(),
            sizeof(size_t));
        add(subpixelProxyScratchStructuralInstances_.capacity(),
            sizeof(Obol::InstanceId));
        return total;
    }

    uint64_t subpixelPreparationCompletedUnits() const
    {
        const uint64_t visible = std::min<uint64_t>(
            subpixelProxyBuildVisibleCursor_,
            cachedPlan_.visibleInstances.size());
        const uint64_t wire =
            subpixelProxyBuildWireOffset_ >
                    UINT64_MAX - subpixelProxyBuildCompletedWireUnits_ ?
                UINT64_MAX : subpixelProxyBuildCompletedWireUnits_ +
                    subpixelProxyBuildWireOffset_;
        const uint64_t completed = wire > UINT64_MAX - visible ?
            UINT64_MAX : visible + wire;
        return std::min(completed, subpixelProxyBuildTotalUnits_);
    }

    void publishSubpixelPreparation(
            const Obol::CadPresentationPreparationTarget& target,
            Obol::CadPresentationPreparationState state,
            uint64_t completedUnits)
    {
        Obol::CadPresentationPreparationSnapshot snapshot;
        snapshot.target = target;
        snapshot.state = state;
        snapshot.totalUnits = subpixelProxyBuildTotalUnits_;
        snapshot.completedUnits = std::min(
            completedUnits, snapshot.totalUnits);
        snapshot.reservedBytes = subpixelPreparationReservedBytes();
        if (presentationPreparation_.target == target &&
                presentationPreparation_.state ==
                    Obol::CadPresentationPreparationState::Preparing)
            snapshot.completedUnits = std::max(
                snapshot.completedUnits,
                presentationPreparation_.completedUnits);
        presentationPreparation_ = snapshot;
    }

    bool updateSubpixelProxyPlan(const SbMatrix& viewProj,
                                 const SbVec2s& viewportSize,
                                 float pixelThreshold,
                                 bool cameraMotionReuse,
                                 SoGLRenderAction *renderAction = nullptr,
                                 bool *preparationPerformed = nullptr)
    {
        using namespace Obol::internal;
        CadFramePlan& plan = cachedPlan_;
        /*
         * A large assembly may be reached after an earlier scene node has
         * already consumed the presentation deadline.  Every retry must
         * nevertheless advance a bounded amount of retained classifier work
         * or an expired deadline can starve the same cursor forever.  Four
         * thousand inexpensive occurrence tests is a small owner-thread
         * quantum, yet bounds a 300k occurrence+wire scan to tens rather than
         * thousands of recovery frames.
         */
        static constexpr size_t guaranteedWorkPerRetry = 4096u;
        static constexpr auto guaranteedPreparationTime =
            std::chrono::milliseconds(4);
        const auto preparationStarted =
            std::chrono::steady_clock::now();
        size_t workSinceAbortCheck = 0u;
        size_t workThisRetry = 0u;
        const auto abortRequested = [&]() {
            if (!renderAction)
                return false;
            ++workThisRetry;
            if (++workSinceAbortCheck < 256u)
                return false;
            workSinceAbortCheck = 0;
            if (workThisRetry < guaranteedWorkPerRetry ||
                    std::chrono::steady_clock::now() -
                        preparationStarted <
                            guaranteedPreparationTime)
                return false;
            return renderAction->abortNow();
        };
        if (preparationPerformed)
            *preparationPerformed = false;
        pixelThreshold = std::isfinite(pixelThreshold) ?
            std::max(1.0f, std::min(64.0f, pixelThreshold)) : 1.0f;
        if (plan.subpixelProxyInputRevision !=
                subpixelProxyViewInputRevision_) {
            if (preparationPerformed)
                *preparationPerformed = true;
            if (patchSubpixelProxyAppendPlan(
                    viewProj, viewportSize, pixelThreshold)) {
                subpixelProxyBuildActive_ = false;
                subpixelProxyBuildTotalUnits_ = 1;
                const Obol::CadPresentationPreparationTarget target =
                    subpixelPreparationTarget(
                        viewProj, viewportSize, pixelThreshold);
                publishSubpixelPreparation(
                    target,
                    Obol::CadPresentationPreparationState::Complete,
                    1);
                return !renderAction || !renderAction->abortNow();
            }
        }
        /*
         * Reproject the already classified point/mesh cut for the input burst
         * instead of rescanning every occurrence on the GUI thread.
         * Rotation and translation preserve the desired PoP prefix.  Zoom
         * may change it, but the retained cut is still the fastest coherent
         * first response and avoids blocking input on exact admission.  The
         * view controller clears this hint on the first quiet frame or when a
         * coverage pass proves that newly visible geometry is missing.
         */
        if (cameraMotionReuse && subpixelProxyViewValid_ &&
                subpixelProxyViewInputRevision_ ==
                    plan.subpixelProxyInputRevision &&
                subpixelProxyViewportSize_[0] == viewportSize[0] &&
                subpixelProxyViewportSize_[1] == viewportSize[1] &&
                subpixelProxyPixelThreshold_ == pixelThreshold &&
                !(subpixelProxyViewProj_ == viewProj)) {
            ++subpixelProxyCameraMotionReuseCount_;
            subpixelProxyViewProj_ = viewProj;
            subpixelProxyBuildActive_ = false;
            structuralProjectionHistogram_.exact = false;
            return true;
        }
        if (subpixelProxyViewValid_ &&
                subpixelProxyViewInputRevision_ ==
                    plan.subpixelProxyInputRevision &&
                subpixelProxyViewportSize_[0] == viewportSize[0] &&
                subpixelProxyViewportSize_[1] == viewportSize[1] &&
                subpixelProxyPixelThreshold_ == pixelThreshold &&
                subpixelProxyViewProj_ == viewProj) {
            subpixelProxyBuildActive_ = false;
            return true;
        }

        subpixelProxyCameraMotionReuseCount_ = 0;
        std::vector<uint8_t>& mask = subpixelProxyScratchMask_;
        std::vector<CadSubpixelProxyPoint>& points =
            subpixelProxyScratchPoints_;
        std::vector<uint32_t>& visibleByPoint =
            subpixelProxyScratchVisibleByPoint_;
        std::vector<uint32_t>& pointByVisible =
            subpixelProxyScratchPointByVisible_;
        const bool matchingBuild = subpixelProxyBuildActive_ &&
            subpixelProxyBuildInputRevision_ ==
                plan.subpixelProxyInputRevision &&
            subpixelProxyBuildViewportSize_[0] == viewportSize[0] &&
            subpixelProxyBuildViewportSize_[1] == viewportSize[1] &&
            subpixelProxyBuildPixelThreshold_ == pixelThreshold &&
            subpixelProxyBuildViewProj_ == viewProj;
        const bool extendedBuild = !matchingBuild &&
            subpixelProxyBuildActive_ &&
            extendSubpixelProxyAppendBuild(
                viewProj, viewportSize, pixelThreshold);
        if (extendedBuild) {
            subpixelProxyBuildTotalUnits_ =
                static_cast<uint64_t>(plan.visibleInstances.size());
            for (const CadDrawItem& item : plan.wireItems) {
                subpixelProxyBuildTotalUnits_ = item.instanceCount >
                        UINT64_MAX - subpixelProxyBuildTotalUnits_ ?
                    UINT64_MAX : subpixelProxyBuildTotalUnits_ +
                        item.instanceCount;
            }
        }
        if (!matchingBuild && !extendedBuild) {
            if (preparationPerformed)
                *preparationPerformed = true;
            if (cadPlanDebugEnabled()) {
                static unsigned int resetMessageCount = 0;
                if (resetMessageCount++ < 256)
                    std::fprintf(stderr,
                        "SoCADAssembly subpixel classifier reset "
                        "active=%d cursor=%zu input=%llu/%llu "
                        "viewport=%d,%d/%d,%d threshold=%.9g/%.9g "
                        "view_match=%d visible=%zu wire=%zu\n",
                        subpixelProxyBuildActive_ ? 1 : 0,
                        subpixelProxyBuildVisibleCursor_,
                        static_cast<unsigned long long>(
                            subpixelProxyBuildInputRevision_),
                        static_cast<unsigned long long>(
                            plan.subpixelProxyInputRevision),
                        subpixelProxyBuildViewportSize_[0],
                        subpixelProxyBuildViewportSize_[1],
                        viewportSize[0], viewportSize[1],
                        subpixelProxyBuildPixelThreshold_, pixelThreshold,
                        subpixelProxyBuildViewProj_ == viewProj ? 1 : 0,
                        plan.visibleInstances.size(), plan.wireItems.size());
            }
            if (subpixelProxyStateInputRevision_ !=
                    plan.subpixelProxyInputRevision) {
                subpixelProxyState_.assign(
                    plan.visibleInstances.size(), 0u);
                subpixelProxyStateInputRevision_ =
                    plan.subpixelProxyInputRevision;
            }
            mask.assign(plan.visibleInstances.size(), 0u);
            pointByVisible.assign(
                plan.visibleInstances.size(),
                std::numeric_limits<uint32_t>::max());
            structuralProjectionScratchBucketByVisible_.assign(
                plan.visibleInstances.size(), -1);
            structuralProjectionScratchHistogram_ =
                Obol::CadStructuralProxyProjectionHistogram();
            points.clear();
            points.reserve(plan.visibleInstances.size());
            visibleByPoint.clear();
            visibleByPoint.reserve(plan.visibleInstances.size());
            subpixelProxyScratchWireByPart_.assign(
                plan.partBindings.size(), 0u);
            subpixelProxyScratchStructuralCountByPart_.assign(
                plan.partBindings.size(), 0u);
            subpixelProxyScratchStructuralCount_ = 0;
            subpixelProxyScratchStructuralInstances_.clear();
            subpixelProxyScratchStructuralInstances_.reserve(
                plan.visibleInstances.size());
            subpixelProxyBuildVisibleCursor_ = 0;
            subpixelProxyBuildWireItemCursor_ = 0;
            subpixelProxyBuildWireOffset_ = 0;
            subpixelProxyBuildWireHasUncollapsed_ = false;
            subpixelProxyBuildWireStructuralCount_ = 0;
            subpixelProxyBuildCompletedWireUnits_ = 0;
            subpixelProxyBuildTotalUnits_ =
                static_cast<uint64_t>(plan.visibleInstances.size());
            for (const CadDrawItem& item : plan.wireItems) {
                subpixelProxyBuildTotalUnits_ = item.instanceCount >
                        UINT64_MAX - subpixelProxyBuildTotalUnits_ ?
                    UINT64_MAX : subpixelProxyBuildTotalUnits_ +
                        item.instanceCount;
            }
            subpixelProxyBuildInputRevision_ =
                plan.subpixelProxyInputRevision;
            subpixelProxyBuildAppendRevision_ =
                plan.appendRevision;
            subpixelProxyBuildViewProj_ = viewProj;
            subpixelProxyBuildViewportSize_ = viewportSize;
            subpixelProxyBuildPixelThreshold_ = pixelThreshold;
            subpixelProxyBuildActive_ = true;
        }
        const Obol::CadPresentationPreparationTarget preparationTarget =
            subpixelPreparationTarget(
                viewProj, viewportSize, pixelThreshold);
        publishSubpixelPreparation(
            preparationTarget,
            Obol::CadPresentationPreparationState::Preparing,
            subpixelPreparationCompletedUnits());
        if (subpixelProxyBuildVisibleCursor_ <
                plan.visibleInstances.size() ||
                subpixelProxyBuildWireItemCursor_ <
                plan.wireItems.size()) {
            if (preparationPerformed)
                *preparationPerformed = true;
        }
        /*
         * Classify each occurrence once, independently of its active draw
         * channels.  In particular a shaded-only PoP mesh must be able to
         * enter the same one-call point batch as a wire AABB.  Iterating draw
         * items duplicated work in SHADED_WITH_EDGES and left SHADED with no
         * subpixel escape path at all.  The retained cursor is part of the
         * same atomic scratch result: an aborted frame keeps presenting the
         * previous complete classification until every occurrence and wire
         * summary has been visited.
         */
        for (; subpixelProxyBuildVisibleCursor_ <
                plan.visibleInstances.size();
                ++subpixelProxyBuildVisibleCursor_) {
            if (abortRequested()) {
                if (cadPlanDebugEnabled()) {
                    static unsigned int abortMessageCount = 0;
                    if (abortMessageCount++ < 256)
                        std::fprintf(stderr,
                            "SoCADAssembly subpixel classifier defer "
                            "visible=%zu/%zu wire=%zu/%zu\n",
                            subpixelProxyBuildVisibleCursor_,
                            plan.visibleInstances.size(),
                            subpixelProxyBuildWireItemCursor_,
                            plan.wireItems.size());
                }
                publishSubpixelPreparation(
                    preparationTarget,
                    Obol::CadPresentationPreparationState::Preparing,
                    subpixelPreparationCompletedUnits());
                return false;
            }
            const size_t visibleIndex =
                subpixelProxyBuildVisibleCursor_;
            const CadVisibleInstance& instance =
                plan.visibleInstances[visibleIndex];
            CadSubpixelProxyPoint replacement;
            CadStructuralProjectionSample structuralSample;
            const CadProxyPresentation presentation =
                subpixelProxyPresentationForOccurrence(
                    plan, visibleIndex, viewProj, viewportSize,
                    pixelThreshold,
                    subpixelProxyState_[visibleIndex] != 0u,
                    replacement, &structuralSample);
            const int8_t structuralBucket =
                cadStructuralProjectionBucket(structuralSample);
            structuralProjectionScratchBucketByVisible_[visibleIndex] =
                structuralBucket;
            cadUpdateStructuralProjectionHistogram(
                structuralProjectionScratchHistogram_,
                structuralBucket, true);
            subpixelProxyState_[visibleIndex] =
                static_cast<uint8_t>(presentation);
            if (presentation == CadProxyPresentation::Geometry) {
                if (instance.flags &
                        CadInstanceLodStructuralProxy)
                    subpixelProxyScratchStructuralInstances_.push_back(
                        instance.instanceId);
                continue;
            }

            mask[visibleIndex] = 1u;
            if (presentation == CadProxyPresentation::Offscreen)
                continue;
            pointByVisible[visibleIndex] =
                static_cast<uint32_t>(points.size());
            points.push_back(std::move(replacement));
            visibleByPoint.push_back(
                static_cast<uint32_t>(visibleIndex));
        }

        for (; subpixelProxyBuildWireItemCursor_ < plan.wireItems.size();
                ++subpixelProxyBuildWireItemCursor_) {
            if (abortRequested()) {
                publishSubpixelPreparation(
                    preparationTarget,
                    Obol::CadPresentationPreparationState::Preparing,
                    subpixelPreparationCompletedUnits());
                return false;
            }
            const CadDrawItem& item =
                plan.wireItems[subpixelProxyBuildWireItemCursor_];
            for (; subpixelProxyBuildWireOffset_ < item.instanceCount;
                    ++subpixelProxyBuildWireOffset_) {
                if (abortRequested()) {
                    publishSubpixelPreparation(
                        preparationTarget,
                        Obol::CadPresentationPreparationState::Preparing,
                        subpixelPreparationCompletedUnits());
                    return false;
                }
                const size_t visibleIndex = item.baseInstance +
                    subpixelProxyBuildWireOffset_;
                if (visibleIndex >= mask.size())
                    continue;
                const CadVisibleInstance& instance =
                    plan.visibleInstances[visibleIndex];
                if (instance.partIndex != item.partIndex ||
                        (instance.flags & CadInstanceHidden))
                    continue;
                if (!mask[visibleIndex]) {
                    subpixelProxyBuildWireHasUncollapsed_ = true;
                    if (instance.flags &
                            CadInstanceLodStructuralProxy) {
                        ++subpixelProxyScratchStructuralCount_;
                        ++subpixelProxyBuildWireStructuralCount_;
                    }
                }
            }
            if (item.partIndex < subpixelProxyScratchWireByPart_.size()) {
                if (subpixelProxyBuildWireHasUncollapsed_)
                    subpixelProxyScratchWireByPart_[item.partIndex] = 1u;
                if (subpixelProxyBuildWireStructuralCount_)
                    subpixelProxyScratchStructuralCountByPart_[
                        item.partIndex] +=
                            subpixelProxyBuildWireStructuralCount_;
            }
            subpixelProxyBuildCompletedWireUnits_ =
                item.instanceCount >
                        UINT64_MAX -
                            subpixelProxyBuildCompletedWireUnits_ ?
                    UINT64_MAX :
                    subpixelProxyBuildCompletedWireUnits_ +
                        item.instanceCount;
            subpixelProxyBuildWireOffset_ = 0;
            subpixelProxyBuildWireHasUncollapsed_ = false;
            subpixelProxyBuildWireStructuralCount_ = 0;
        }
        if (cadPlanDebugEnabled() &&
                subpixelProxyScratchStructuralCount_) {
            size_t structuralItemReferences = 0;
            size_t structuralVisibleReferences = 0;
            size_t structuralHiddenReferences = 0;
            size_t structuralStaleReferences = 0;
            size_t structuralPartCount = 0;
            for (const CadDrawItem& item : plan.wireItems) {
                const CadPartBinding *binding =
                    item.partIndex < plan.partBindings.size() ?
                        &plan.partBindings[item.partIndex] : nullptr;
                const bool structuralProxy =
                    binding && binding->structuralProxy;
                if (!structuralProxy)
                    continue;
                ++structuralPartCount;
                for (uint32_t i = 0; i < item.instanceCount; ++i) {
                    const size_t visibleIndex = item.baseInstance + i;
                    if (visibleIndex >= plan.visibleInstances.size())
                        continue;
                    ++structuralItemReferences;
                    const CadVisibleInstance& instance =
                        plan.visibleInstances[visibleIndex];
                    if (instance.partIndex != item.partIndex) {
                        ++structuralStaleReferences;
                        continue;
                    }
                    if (instance.flags & CadInstanceHidden) {
                        ++structuralHiddenReferences;
                        continue;
                    }
                    ++structuralVisibleReferences;
                }
            }
            std::fprintf(stderr,
                "SoCADAssembly structural proxy debug "
                "uncollapsed=%zu item_refs=%zu hidden_refs=%zu "
                "stale_refs=%zu "
                "visible_refs=%zu classified_instances=%zu "
                "wire_items=%zu structural_items=%zu visible_records=%zu\n",
                subpixelProxyScratchStructuralCount_,
                structuralItemReferences, structuralHiddenReferences,
                structuralStaleReferences,
                structuralVisibleReferences,
                subpixelProxyScratchStructuralInstances_.size(),
                plan.wireItems.size(), structuralPartCount,
                plan.visibleInstances.size());
        }

        /*
         * Publish every classifier product as one transaction.  Diagnostics
         * above deliberately do not consult the presentation deadline:
         * debug instrumentation must not make the resumable production
         * algorithm fail to converge or expose half of a new classification.
         */
        for (auto& entry : plan.partPresentation)
            entry.second.wireHasUncollapsedInstances = false;
        for (size_t partIndex = 0;
                partIndex < subpixelProxyScratchWireByPart_.size() &&
                partIndex < plan.partBindings.size(); ++partIndex) {
            if (!subpixelProxyScratchWireByPart_[partIndex])
                continue;
            const auto presentation = plan.partPresentation.find(
                plan.partBindings[partIndex].part);
            if (presentation != plan.partPresentation.end())
                presentation->second.wireHasUncollapsedInstances = true;
        }
        size_t structuralPartCount = 0u;
        for (const size_t count :
                subpixelProxyScratchStructuralCountByPart_)
            if (count)
                ++structuralPartCount;
        uncollapsedStructuralProxyCountByPart_.clear();
        uncollapsedStructuralProxyCountByPart_.reserve(
            structuralPartCount);
        for (size_t partIndex = 0;
                partIndex <
                    subpixelProxyScratchStructuralCountByPart_.size() &&
                partIndex < plan.partBindings.size(); ++partIndex) {
            const size_t count =
                subpixelProxyScratchStructuralCountByPart_[partIndex];
            if (count)
                uncollapsedStructuralProxyCountByPart_[
                    plan.partBindings[partIndex].part] += count;
        }
        uncollapsedStructuralProxyCount_ =
            subpixelProxyScratchStructuralCount_;
        uncollapsedStructuralProxyInstances_.clear();
        uncollapsedStructuralProxyInstances_.reserve(
            subpixelProxyScratchStructuralInstances_.size());
        uncollapsedStructuralProxyInstances_.insert(
            subpixelProxyScratchStructuralInstances_.begin(),
            subpixelProxyScratchStructuralInstances_.end());
        structuralProjectionBucketByVisible_.swap(
            structuralProjectionScratchBucketByVisible_);
        structuralProjectionHistogram_ =
            structuralProjectionScratchHistogram_;
        structuralProjectionHistogram_.revision =
            nextStructuralProjectionRevision_++;
        if (nextStructuralProjectionRevision_ == 0)
            nextStructuralProjectionRevision_ = 1;
        structuralProjectionHistogram_.exact = true;

        const bool changed = plan.subpixelProxySourceInputRevision !=
                plan.subpixelProxyInputRevision ||
                mask != plan.subpixelProxyMask ||
                !cadSameSubpixelProxyPoints(points,
                    plan.subpixelProxyPoints);
        if (changed) {
            // Swapping preserves previous storage in the scratch vectors for
            // the next camera update instead of allocating frame-local data.
            plan.subpixelProxyMask.swap(mask);
            plan.subpixelProxyPoints.swap(points);
            subpixelProxyVisibleByPoint_.swap(visibleByPoint);
            subpixelProxyPointByVisible_.swap(pointByVisible);
            plan.subpixelProxySourceInputRevision =
                plan.subpixelProxyInputRevision;
            plan.subpixelProxyRevision = nextSubpixelProxyRevision_++;
            if (nextSubpixelProxyRevision_ == 0)
                nextSubpixelProxyRevision_ = 1;
        }
        subpixelProxyViewProj_ = viewProj;
        subpixelProxyViewportSize_ = viewportSize;
        subpixelProxyPixelThreshold_ = pixelThreshold;
        subpixelProxyViewInputRevision_ =
            plan.subpixelProxyInputRevision;
        subpixelProxyClassifiedAppendRevision_ =
            plan.appendRevision;
        subpixelProxyViewValid_ = true;
        subpixelProxyBuildActive_ = false;
        publishSubpixelPreparation(
            preparationTarget,
            Obol::CadPresentationPreparationState::Complete,
            subpixelProxyBuildTotalUnits_);
        if (cadPlanDebugEnabled()) {
            static unsigned int completeMessageCount = 0;
            if (completeMessageCount++ < 256)
                std::fprintf(stderr,
                    "SoCADAssembly subpixel classifier complete "
                    "visible=%zu wire=%zu points=%zu threshold=%.9g\n",
                    plan.visibleInstances.size(), plan.wireItems.size(),
                    plan.subpixelProxyPoints.size(), pixelThreshold);
        }
        return true;
    }
};

// ---------------------------------------------------------------------------
// SoCADAssembly
// ---------------------------------------------------------------------------

SO_NODE_SOURCE(SoCADAssembly);

void
SoCADAssembly::initClass()
{
    SO_NODE_INIT_CLASS(SoCADAssembly, SoNode, "Node");
    SoCADDetail::initClass();
    SoCADViewState::initClass();
}

SoCADAssembly::SoCADAssembly()
    : impl_(new SoCADAssemblyImpl)
{
    SO_NODE_CONSTRUCTOR(SoCADAssembly);

    SO_NODE_DEFINE_ENUM_VALUE(DrawMode, SHADED);
    SO_NODE_DEFINE_ENUM_VALUE(DrawMode, WIREFRAME);
    SO_NODE_DEFINE_ENUM_VALUE(DrawMode, SHADED_WITH_EDGES);
    SO_NODE_DEFINE_ENUM_VALUE(DrawMode, HIDDEN_LINE);
    SO_NODE_SET_SF_ENUM_TYPE(drawMode, DrawMode);
    SO_NODE_ADD_FIELD(drawMode, (WIREFRAME));

    SO_NODE_DEFINE_ENUM_VALUE(PickMode, PICK_AUTO);
    SO_NODE_DEFINE_ENUM_VALUE(PickMode, PICK_EDGE);
    SO_NODE_DEFINE_ENUM_VALUE(PickMode, PICK_TRIANGLE);
    SO_NODE_DEFINE_ENUM_VALUE(PickMode, PICK_BOUNDS);
    SO_NODE_DEFINE_ENUM_VALUE(PickMode, PICK_HYBRID);
    SO_NODE_SET_SF_ENUM_TYPE(pickMode, PickMode);
    SO_NODE_ADD_FIELD(pickMode, (PICK_AUTO));

    SO_NODE_ADD_FIELD(edgePickTolerancePx, (5.0f));
    SO_NODE_ADD_FIELD(wireframeOcclusion,  (FALSE));
    SO_NODE_ADD_FIELD(progressiveCutCeiling, (-1));
    SO_NODE_ADD_FIELD(progressiveCutNextFraction, (0.0f));
    SO_NODE_ADD_FIELD(pointProxyPixelThreshold, (1.0f));
    SO_NODE_ADD_FIELD(cameraMotionFrameReuse, (FALSE));
}

SoCADAssembly::~SoCADAssembly() = default;

SoDetail*
SoCADAssembly::createPickDetail(
    const Obol::CadPickDetailRecord& hit) const
{
    SoCADDetail* detail = new SoCADDetail;
    detail->setInstanceId(hit.instance);
    detail->setPartId(hit.part);
    switch (hit.primitiveKind) {
        case Obol::CadPickDetailRecord::EDGE:
            detail->setPrimType(SoCADDetail::EDGE);
            detail->setPrimIndex0(hit.primIndex0);
            detail->setPrimIndex1(hit.primIndex1);
            detail->setU(hit.u);
            break;
        case Obol::CadPickDetailRecord::TRIANGLE:
            detail->setPrimType(SoCADDetail::TRIANGLE);
            detail->setPrimIndex0(hit.primIndex0);
            break;
        case Obol::CadPickDetailRecord::POINT:
            detail->setPrimType(SoCADDetail::POINT);
            detail->setPrimIndex0(hit.primIndex0);
            break;
        case Obol::CadPickDetailRecord::BOUNDS:
        default:
            detail->setPrimType(SoCADDetail::BOUNDS);
            break;
    }
    return detail;
}

// --- Update framing --------------------------------------------------------

void SoCADAssembly::beginUpdate() { impl_->inUpdate_ = true; }

void SoCADAssembly::reserveStreamingCapacity(size_t expectedOccurrences)
{
    if (!expectedOccurrences)
        return;
    impl_->streamingOccurrenceCapacityHint_ =
        std::max(impl_->streamingOccurrenceCapacityHint_,
                 expectedOccurrences);

    /*
     * These tables retain one logical record per occurrence/part in the
     * distinct-part worst case.  reserve() does not construct records, so the
     * memory is committed only as table/vector capacity rather than as
     * populated mesh data.
     */
    impl_->parts_.reserve(expectedOccurrences);
    impl_->instances_.reserve(expectedOccurrences);
    impl_->instancePartSlot_.reserve(expectedOccurrences);
    impl_->progressivePlanIndexByInstance_.reserve(
        expectedOccurrences);
    impl_->cachedPlanPartSpansByPart_.reserve(
        expectedOccurrences);

    /*
     * A plan may already exist if the structural stream disclosed its
     * manifest after publishing the overall/root proxy.  Grow its buffers
     * while that population is still small instead of allowing geometric
     * vector growth near convergence to copy the mature plan.
     */
    const size_t visibleCapacity =
        expectedOccurrences >
                std::numeric_limits<size_t>::max() / 2u ?
            std::numeric_limits<size_t>::max() :
            expectedOccurrences * 2u;
    impl_->cachedPlan_.visibleInstances.reserve(visibleCapacity);
    impl_->cachedPlan_.partBindings.reserve(expectedOccurrences + 1u);
    impl_->cachedPlan_.wireItems.reserve(expectedOccurrences);
    impl_->cachedPlan_.pointItems.reserve(expectedOccurrences);
    impl_->cachedPlan_.shadedItems.reserve(expectedOccurrences);
    impl_->cachedPlan_.requiredReps.reserve(visibleCapacity);
}

void SoCADAssembly::endUpdate()
{
    impl_->inUpdate_ = false;
    /* Public mutations record the exact caches they invalidate even while
     * notifications are batched.  Do not turn a selection/style-only batch
     * into a geometry and BVH rebuild merely because it was framed by
     * beginUpdate()/endUpdate(). */
    touch();
}

void
SoCADAssembly::clear()
{
    impl_->parts_.clear();
    impl_->subpixelProxyCorners_.clear();
    impl_->partGeneration_.clear();
    impl_->instances_.clear();
    impl_->instanceIdsByPart_.clear();
    impl_->instancePartSlot_.clear();
    impl_->cachedPlanTombstoneCount_ = 0;
    impl_->streamTombstoneCompactionPerformed_ = false;
    impl_->subpixelProxyPointByVisible_.clear();
    impl_->subpixelProxyVisibleByPoint_.clear();
    impl_->subpixelProxyScratchVisibleByPoint_.clear();
    impl_->subpixelProxyClassifiedAppendRevision_ = 0;
    impl_->subpixelProxyScratchWireByPart_.clear();
    impl_->subpixelProxyScratchStructuralCountByPart_.clear();
    impl_->uncollapsedStructuralProxyCountByPart_.clear();
    impl_->uncollapsedStructuralProxyCount_ = 0;
    impl_->subpixelProxyScratchStructuralInstances_.clear();
    impl_->uncollapsedStructuralProxyInstances_.clear();
    impl_->selected_.clear();
    impl_->hidden_.clear();
    impl_->unpickable_.clear();
    impl_->pointProxyProtected_.clear();
    impl_->partEdgeBvhCache_.clear();
    impl_->partTriBvhCache_.clear();
    impl_->progressiveParts_.clear();
    impl_->instanceBvh_ = Obol::picking::CadInstanceBVH();
    impl_->bvhDirty_ = true;
    impl_->planDirty_ = true;
    impl_->geometryDirty_ = true;
    if (!impl_->inUpdate_)
        touch();
}

// --- Part library ----------------------------------------------------------

void
SoCADAssembly::upsertPart(Obol::PartId pid, const Obol::PartGeometry& geom)
{
    impl_->updatePartGeometry(pid,
        std::make_shared<const Obol::PartGeometry>(geom));
    impl_->recomputeWorldBoundsForPart(pid);
    Obol::internal::CadPartGeometryDelta geometryDelta;
    if (!impl_->patchCachedPartGeometry(pid, geometryDelta))
        impl_->markDirty("part-geometry-unpatchable");
    else
        impl_->finishPartGeometryPatch(std::move(geometryDelta));
    if (!impl_->inUpdate_) touch();
}

void
SoCADAssembly::upsertParts(const std::vector<Obol::PartUpdate>& updates)
{
    if (updates.empty())
        return;

    std::unordered_set<Obol::PartId, std::hash<Obol::PartId>> changedParts;
    bool topologyDirty = false;
    changedParts.reserve(updates.size());
    for (const auto& update : updates) {
        impl_->updatePartGeometry(update.part,
            std::make_shared<const Obol::PartGeometry>(update.geometry));
        changedParts.insert(update.part);
    }
    impl_->recomputeWorldBoundsForParts(changedParts);
    Obol::internal::CadPartGeometryDelta geometryDelta;
    for (const Obol::PartId part : changedParts) {
        if (!impl_->patchCachedPartGeometry(
                part, geometryDelta)) {
            topologyDirty = true;
            break;
        }
    }
    if (topologyDirty)
        impl_->markDirty("part-geometry-batch-unpatchable");
    else
        impl_->finishPartGeometryPatch(
            std::move(geometryDelta));
    if (!impl_->inUpdate_)
        touch();
}

void
SoCADAssembly::upsertSharedParts(
    const std::vector<Obol::SharedPartUpdate>& updates)
{
    if (updates.empty()) return;
    std::unordered_set<Obol::PartId, std::hash<Obol::PartId>> changedParts;
    std::unordered_set<Obol::PartId, std::hash<Obol::PartId>>
        boundsPreservingParts;
    bool topologyDirty = false;
    changedParts.reserve(updates.size());
    boundsPreservingParts.reserve(updates.size());
    for (const auto& update : updates) {
        if (!update.geometry) continue;
        const auto preceding = impl_->parts_.find(update.part);
        /*
         * Shared immutable geometry is its own publication identity.  Large
         * structural streams may mention one retained fallback part in many
         * append batches; reinstalling the exact pointer used to increment
         * its generation, recompute every referencing occurrence's bounds,
         * and patch every compiled span on each batch.  That turned a cheap
         * append journal into a growing-scene rescan.
         */
        if (preceding != impl_->parts_.end() &&
                preceding->second == update.geometry)
            continue;
        const bool preservesBounds =
            update.preservesBounds &&
            preceding != impl_->parts_.end() &&
            preceding->second &&
            SoCADAssemblyImpl::partGeometryBoundsEqual(
                *preceding->second, *update.geometry);
        const bool firstUpdate =
            changedParts.insert(update.part).second;
        if (firstUpdate) {
            if (preservesBounds)
                boundsPreservingParts.insert(update.part);
        } else if (!preservesBounds) {
            boundsPreservingParts.erase(update.part);
        }
        impl_->updatePartGeometry(update.part, update.geometry);
    }
    if (changedParts.empty()) return;
    if (boundsPreservingParts.size() != changedParts.size()) {
        std::unordered_set<Obol::PartId, std::hash<Obol::PartId>>
            boundsChangedParts;
        boundsChangedParts.reserve(
            changedParts.size() - boundsPreservingParts.size());
        for (const Obol::PartId part : changedParts)
            if (boundsPreservingParts.find(part) ==
                    boundsPreservingParts.end())
                boundsChangedParts.insert(part);
        impl_->recomputeWorldBoundsForParts(boundsChangedParts);
    }
    Obol::internal::CadPartGeometryDelta geometryDelta;
    for (const Obol::PartId part : changedParts) {
        if (!impl_->patchCachedPartGeometry(
                part, geometryDelta,
                boundsPreservingParts.find(part) !=
                    boundsPreservingParts.end())) {
            topologyDirty = true;
            break;
        }
    }
    if (topologyDirty)
        impl_->markDirty("shared-part-geometry-batch-unpatchable");
    else
        impl_->finishPartGeometryPatch(
            std::move(geometryDelta));
    if (!impl_->inUpdate_) touch();
}

void
SoCADAssembly::removePart(Obol::PartId pid)
{
    const auto referenced = impl_->instanceIdsByPart_.find(pid);
    const bool affectsPlan =
        referenced != impl_->instanceIdsByPart_.end() &&
        referenced->second.size() != 0;
    /*
     * A sparse part replacement leaves a hidden compiled tombstone whose
     * CadPartBinding owns the immutable geometry through a shared pointer.
     * Removing the retired library entry is therefore not a live topology
     * change.  Rebuilding the whole plan merely because that tombstone still
     * has a span defeated append-only streaming; normal late compaction
     * removes the retained binding.
     */
    impl_->parts_.erase(pid);
    impl_->subpixelProxyCorners_.erase(pid);
    impl_->partGeneration_.erase(pid);
    impl_->partEdgeBvhCache_.erase(pid);
    impl_->partTriBvhCache_.erase(pid);
    impl_->progressiveParts_.erase(pid);
    impl_->recomputeWorldBoundsForPart(pid);
    if (affectsPlan)
        impl_->markDirty("part-remove");
    if (!impl_->inUpdate_) touch();
}

void
SoCADAssembly::removeParts(const std::vector<Obol::PartId>& pids)
{
    if (pids.empty()) return;
    std::unordered_set<Obol::PartId, std::hash<Obol::PartId>> changedParts;
    bool affectsPlan = false;
    changedParts.reserve(pids.size());
    for (const Obol::PartId pid : pids) {
        if (!impl_->parts_.erase(pid))
            continue;
        const auto referenced =
            impl_->instanceIdsByPart_.find(pid);
        affectsPlan = affectsPlan ||
            (referenced != impl_->instanceIdsByPart_.end() &&
             referenced->second.size() != 0);
        changedParts.insert(pid);
        impl_->subpixelProxyCorners_.erase(pid);
        impl_->partGeneration_.erase(pid);
        impl_->partEdgeBvhCache_.erase(pid);
        impl_->partTriBvhCache_.erase(pid);
        impl_->progressiveParts_.erase(pid);
    }
    if (changedParts.empty()) return;
    impl_->recomputeWorldBoundsForParts(changedParts);
    if (affectsPlan)
        impl_->markDirty("parts-remove");
    if (!impl_->inUpdate_) touch();
}

// --- Instance management ---------------------------------------------------

Obol::InstanceId
SoCADAssembly::upsertInstanceAuto(const Obol::InstanceRecord& rec)
{
    Obol::InstanceId iid = Obol::CadIdBuilder::extendNameOccBool(
        rec.parent, rec.childName, rec.occurrenceIndex, rec.boolOp);
    upsertInstance(iid, rec);
    return iid;
}

void
SoCADAssembly::upsertInstance(Obol::InstanceId iid, const Obol::InstanceRecord& rec)
{
    const auto prior = impl_->instances_.find(iid);
    const bool sparseAppend =
        !impl_->planDirty_ && !impl_->geometryDirty_ &&
        prior == impl_->instances_.end();
    const bool sparseRebind =
        !impl_->planDirty_ && !impl_->geometryDirty_ &&
        prior != impl_->instances_.end() &&
        !(prior->second.partId == rec.part);
    const Obol::PartId oldPart = prior != impl_->instances_.end() ?
        prior->second.partId : Obol::PartId();
    impl_->updateKnownInstance(iid, rec,
        prior == impl_->instances_.end() ? nullptr : &prior->second);
    if (sparseAppend &&
            impl_->appendCachedInstances({iid}))
        impl_->finishSparseStructuralPatch();
    else if (sparseRebind &&
            impl_->patchCachedInstancePartRebind(iid, oldPart))
        impl_->finishSparseStructuralPatch();
    else
        impl_->markDirty("instance-upsert-unpatchable");
    if (!impl_->inUpdate_) touch();
}

std::vector<Obol::InstanceId>
SoCADAssembly::upsertInstancesAuto(
    const std::vector<Obol::InstanceRecord>& records)
{
    std::vector<Obol::InstanceId> ids;
    ids.reserve(records.size());
    if (records.empty())
        return ids;

    for (const auto& rec : records) {
        Obol::InstanceId iid = Obol::CadIdBuilder::extendNameOccBool(
            rec.parent, rec.childName, rec.occurrenceIndex, rec.boolOp);
        ids.push_back(iid);
        impl_->updateInstance(iid, rec);
    }
    if (!impl_->appendCachedInstances(ids))
        impl_->markDirty("instance-auto-batch-unpatchable");
    else
        impl_->finishSparseStructuralPatch();
    if (!impl_->inUpdate_)
        touch();
    return ids;
}

void
SoCADAssembly::upsertInstances(
    const std::vector<Obol::InstanceUpdate>& updates)
{
    if (updates.empty())
        return;

    const bool planPatchable =
        !impl_->planDirty_ && !impl_->geometryDirty_;
    bool allNew = planPatchable;
    bool allRebind = planPatchable;
    size_t newCount = 0;
    size_t samePartCount = 0;
    size_t rebindCount = 0;
    size_t noOpCount = 0;
    size_t lodOnlyCount = 0;
    size_t metadataOnlyCount = 0;
    size_t lodStructuralRoleChangeCount = 0;
    size_t styleChangeCount = 0;
    size_t transformChangeCount = 0;
    bool samePartPlanNeutral = planPatchable;
    std::vector<Obol::InstanceId> changedIds;
    std::vector<Obol::InstanceId> structuralIds;
    std::vector<std::pair<Obol::InstanceId, Obol::PartId>>
        rebinds;
    std::vector<Obol::InstanceLodUpdate> samePartLodUpdates;
    std::vector<Obol::InstanceId> lodStructuralRoleChanges;
    changedIds.reserve(updates.size());
    structuralIds.reserve(updates.size());
    rebinds.reserve(updates.size());
    samePartLodUpdates.reserve(updates.size());
    lodStructuralRoleChanges.reserve(updates.size());
    bool sourceChanged = false;
    for (const auto& update : updates) {
        const auto prior = impl_->instances_.find(update.instance);
        bool lightweightSamePart = false;
        if (prior == impl_->instances_.end()) {
            ++newCount;
            structuralIds.push_back(update.instance);
            samePartPlanNeutral = false;
            sourceChanged = true;
        } else if (prior->second.partId == update.record.part) {
            ++samePartCount;
            const bool transformChanged =
                !(prior->second.localToRoot == update.record.localToRoot);
            const bool styleChanged =
                prior->second.style.hasColorOverride !=
                    update.record.style.hasColorOverride ||
                !(prior->second.style.color == update.record.style.color) ||
                prior->second.style.lineWidth !=
                    update.record.style.lineWidth ||
                prior->second.style.linePattern !=
                    update.record.style.linePattern ||
                prior->second.style.linePatternFactor !=
                    update.record.style.linePatternFactor;
            const bool metadataChanged =
                !(prior->second.parent == update.record.parent) ||
                prior->second.childName != update.record.childName ||
                prior->second.occurrenceIndex !=
                    update.record.occurrenceIndex ||
                prior->second.boolOp != update.record.boolOp;
            const bool lodChanged =
                prior->second.lodCut != update.record.lodCut;
            const bool lodStructuralRoleChanged =
                prior->second.lodStructuralProxy !=
                    update.record.lodStructuralProxy;
            lightweightSamePart = !transformChanged && !styleChanged;
            sourceChanged = sourceChanged || transformChanged ||
                styleChanged || metadataChanged || lodChanged ||
                lodStructuralRoleChanged;
            if (transformChanged)
                ++transformChangeCount;
            if (styleChanged)
                ++styleChangeCount;
            if (!transformChanged && !styleChanged &&
                    !metadataChanged && !lodChanged &&
                    !lodStructuralRoleChanged)
                ++noOpCount;
            else if (!transformChanged && !styleChanged &&
                    !metadataChanged && lodChanged &&
                    !lodStructuralRoleChanged)
                ++lodOnlyCount;
            else if (!transformChanged && !styleChanged &&
                    metadataChanged && !lodChanged &&
                    !lodStructuralRoleChanged)
                ++metadataOnlyCount;
            if (lodChanged)
                samePartLodUpdates.push_back(
                    {update.instance, update.record.lodCut});
            if (lodStructuralRoleChanged) {
                ++lodStructuralRoleChangeCount;
                lodStructuralRoleChanges.push_back(update.instance);
            }
            samePartPlanNeutral =
                samePartPlanNeutral &&
                !transformChanged && !styleChanged;
        } else {
            ++rebindCount;
            structuralIds.push_back(update.instance);
            rebinds.emplace_back(
                update.instance, prior->second.partId);
            samePartPlanNeutral = false;
            sourceChanged = true;
        }
        allNew = allNew && prior == impl_->instances_.end();
        allRebind = allRebind &&
            prior != impl_->instances_.end() &&
            !(prior->second.partId == update.record.part);
        changedIds.push_back(update.instance);
        if (lightweightSamePart) {
            /*
             * LoD policy waves commonly repeat thousands of otherwise
             * identical records.  Avoid copying every child-name string and
             * recomputing unchanged world bounds on the GUI thread.
             */
            InstanceData& retained = prior->second;
            if (!(retained.parent == update.record.parent))
                retained.parent = update.record.parent;
            if (retained.childName != update.record.childName)
                retained.childName = update.record.childName;
            retained.occurrenceIndex = update.record.occurrenceIndex;
            retained.boolOp = update.record.boolOp;
            retained.lodCut = update.record.lodCut;
            retained.lodStructuralProxy =
                update.record.lodStructuralProxy;
        } else {
            impl_->updateKnownInstance(update.instance, update.record,
                prior == impl_->instances_.end() ?
                    nullptr : &prior->second);
        }
    }
    if (!sourceChanged)
        return;
    const auto patchPlanNeutralSamePart = [&]() {
        std::unordered_set<size_t> changedPlanGroups;
        bool samePartPatched = true;
        for (const auto& update : samePartLodUpdates) {
            if (!impl_->patchCachedInstanceCut(
                    update.instance, update.lodCut,
                    changedPlanGroups)) {
                samePartPatched = false;
                break;
            }
        }
        if (samePartPatched && !samePartLodUpdates.empty())
            impl_->finishProgressiveShadedPlanPatch(
                changedPlanGroups);
        if (samePartPatched && !samePartLodUpdates.empty())
            impl_->bvhDirty_ = true;
        if (samePartPatched && !lodStructuralRoleChanges.empty()) {
            std::unordered_set<Obol::PartId,
                std::hash<Obol::PartId>> affectedParts;
            affectedParts.reserve(lodStructuralRoleChanges.size());
            for (const Obol::InstanceId instance :
                    lodStructuralRoleChanges) {
                if (!impl_->patchCachedInstanceFlags(instance)) {
                    samePartPatched = false;
                    break;
                }
                const auto retained = impl_->instances_.find(instance);
                if (retained != impl_->instances_.end())
                    affectedParts.insert(retained->second.partId);
            }
            if (samePartPatched) {
                impl_->refreshWireProxyParts(affectedParts);
                impl_->finishSparsePresentationPatch();
            }
        }
        return samePartPatched;
    };
    bool patched = allNew &&
        impl_->appendCachedInstances(changedIds);
    bool structuralPatched = patched;
    const bool mixedAppend = planPatchable && samePartCount == 0u &&
        newCount != 0u && rebindCount != 0u;
    if (!patched && mixedAppend) {
        patched = impl_->appendCachedInstances(changedIds, true);
        structuralPatched = patched;
    }
    if (!patched && allRebind) {
        patched = impl_->patchCachedInstancePartRebinds(rebinds);
        structuralPatched = patched;
    }
    if (!patched && allRebind) {
        patched = impl_->appendCachedInstances(changedIds, true);
        structuralPatched = patched;
    }
    /*
     * A normal refinement publication can replace hundreds of structural
     * parts while also changing the cut of a few already-progressive peers.
     * Both mutations have bounded patch routes, but treating the combined
     * vector as neither an all-rebind nor an all-cut batch forced a complete
     * scene-plan rebuild after every wave.  Patch the structural subset first
     * and then the plan-neutral same-part subset as one owner-thread
     * transaction.  The source records have already been updated above, so a
     * rare patch rejection still falls back to the ordinary authoritative
     * rebuild without exposing partial state to a render traversal.
     */
    const bool mixedStructuralAndPlanNeutralSamePart =
        !patched && planPatchable && !structuralIds.empty() &&
        samePartCount != 0u && transformChangeCount == 0u &&
        styleChangeCount == 0u;
    if (mixedStructuralAndPlanNeutralSamePart) {
        if (newCount == 0u) {
            patched = impl_->patchCachedInstancePartRebinds(rebinds);
            if (!patched)
                patched = impl_->appendCachedInstances(
                    structuralIds, true);
        } else {
            patched = impl_->appendCachedInstances(
                structuralIds, rebindCount != 0u);
        }
        structuralPatched = patched;
        if (patched)
            patched = patchPlanNeutralSamePart();
    }
    if (!patched && samePartPlanNeutral && newCount == 0u &&
            rebindCount == 0u) {
        patched = patchPlanNeutralSamePart();
    }
    if (structuralPatched)
        impl_->finishSparseStructuralPatch();
    /*
     * Do not synchronously compact streamed presentation tombstones here.
     * A full 50k plan rebuild costs well over a frame and this method runs on
     * the GUI thread.  Hidden fallback records are semantically inert and
     * bounded to one per source occurrence, while the live geometry atlas and
     * command population are unchanged by compaction.  A future background
     * dense-plan publication may reclaim them after a revision-checked swap;
     * until then, retaining the bounded prefix is preferable to freezing user
     * input during otherwise incremental realization.
     */
    if (!patched) {
        if (cadPlanDebugEnabled() &&
                impl_->planDebugPatchMessageCount_++ <
                    cadPlanDebugMessageLimit())
            std::fprintf(stderr,
                "SoCADAssembly instance batch patch rejected "
                "batch=%zu new=%zu same_part=%zu rebind=%zu "
                "noop=%zu lod_only=%zu metadata_only=%zu "
                "lod_structural_role=%zu style_changed=%zu "
                "transform_changed=%zu "
                "all_new=%d all_rebind=%d plan_patchable=%d\n",
                updates.size(), newCount, samePartCount, rebindCount,
                noOpCount, lodOnlyCount, metadataOnlyCount,
                lodStructuralRoleChangeCount,
                styleChangeCount, transformChangeCount,
                allNew ? 1 : 0, allRebind ? 1 : 0,
                planPatchable ? 1 : 0);
        impl_->markDirty("instance-batch-unpatchable");
    }
    if (!impl_->inUpdate_)
        touch();
}

void
SoCADAssembly::updateInstanceCuts(
    const std::vector<Obol::InstanceLodUpdate>& updates)
{
    bool changed = false;
    bool sparsePlanPatch = !impl_->planDirty_ && !impl_->geometryDirty_ &&
        impl_->cachedDM_ >= WIREFRAME && impl_->cachedDM_ <= HIDDEN_LINE;
    std::unordered_set<size_t> changedPlanGroups;
    for (const auto& update : updates) {
        auto found = impl_->instances_.find(update.instance);
        if (found == impl_->instances_.end() ||
                found->second.lodCut == update.lodCut)
            continue;
        if (sparsePlanPatch &&
                !impl_->patchCachedInstanceCut(
                    update.instance, update.lodCut, changedPlanGroups))
            sparsePlanPatch = false;
        found->second.lodCut = update.lodCut;
        changed = true;
    }
    if (!changed) return;
    if (sparsePlanPatch)
        impl_->finishProgressiveShadedPlanPatch(changedPlanGroups);
    else
    {
        impl_->planDirty_ = true;
        impl_->planDirtyReason_ = "lod-unpatchable";
    }
    /* The instance BVH also carries the active cut used by exact picking.
     * Rebuilding remains lazy and therefore does not add work to rendering. */
    impl_->bvhDirty_ = true;
    if (!impl_->inUpdate_) touch();
}

void
SoCADAssembly::removeInstance(Obol::InstanceId iid)
{
    const auto instance = impl_->instances_.find(iid);
    if (instance != impl_->instances_.end()) {
        impl_->removeInstanceFromPartIndex(iid, instance->second.partId);
        impl_->instances_.erase(instance);
    }
    impl_->selected_.erase(iid);
    impl_->hidden_.erase(iid);
    impl_->unpickable_.erase(iid);
    impl_->pointProxyProtected_.erase(iid);
    impl_->bvhDirty_  = true;
    impl_->planDirty_ = true;
    impl_->geometryDirty_ = true;
    impl_->planDirtyReason_ = "instance-remove";
    if (!impl_->inUpdate_) touch();
}

void
SoCADAssembly::updateInstanceTransform(Obol::InstanceId iid, const SbMatrix& m)
{
    auto it = impl_->instances_.find(iid);
    if (it == impl_->instances_.end()) return;
    it->second.localToRoot = m;
    auto geomIt = impl_->parts_.find(it->second.partId);
    if (geomIt != impl_->parts_.end() && geomIt->second) {
        it->second.worldBounds = impl_->computeWorldBounds(*geomIt->second, m);
    }
    impl_->bvhDirty_  = true;
    impl_->planDirty_ = true;
    impl_->geometryDirty_ = true;
    impl_->planDirtyReason_ = "instance-transform";
    if (!impl_->inUpdate_) touch();
}

void
SoCADAssembly::updateInstanceStyle(Obol::InstanceId iid, const Obol::InstanceStyle& style)
{
    auto it = impl_->instances_.find(iid);
    if (it == impl_->instances_.end()) return;
    it->second.style = style;
    if (impl_->patchCachedInstanceStyle(iid))
        impl_->finishSparsePresentationPatch();
    else {
        impl_->planDirty_ = true;
        impl_->planDirtyReason_ = "instance-style";
        impl_->pendingInstanceAttributeIndices_.clear();
    }
    if (!impl_->inUpdate_) touch();
}

void
SoCADAssembly::updateInstanceStyles(
    const std::vector<Obol::InstanceStyleUpdate>& updates)
{
    if (updates.empty()) return;
    bool changed = false;
    bool sparsePlanPatch = !impl_->planDirty_ && !impl_->geometryDirty_ &&
        impl_->cachedDM_ >= SoCADAssembly::SHADED &&
        impl_->cachedDM_ <= SoCADAssembly::HIDDEN_LINE;
    for (const auto& update : updates) {
        auto it = impl_->instances_.find(update.instance);
        if (it == impl_->instances_.end()) continue;
        it->second.style = update.style;
        if (sparsePlanPatch &&
                !impl_->patchCachedInstanceStyle(update.instance))
            sparsePlanPatch = false;
        changed = true;
    }
    if (!changed) return;
    if (sparsePlanPatch)
        impl_->finishSparsePresentationPatch();
    else {
        impl_->planDirty_ = true;
        impl_->planDirtyReason_ = "instance-styles";
        impl_->pendingInstanceAttributeIndices_.clear();
    }
    if (!impl_->inUpdate_) touch();
}

void
SoCADAssembly::setSelectedInstances(const std::vector<Obol::InstanceId>& ids)
{
    std::unordered_set<Obol::InstanceId, std::hash<Obol::InstanceId>> next;
    next.reserve(ids.size());
    for (const Obol::InstanceId& id : ids)
        if (impl_->instances_.find(id) != impl_->instances_.end())
            next.insert(id);
    if (next == impl_->selected_)
        return;
    std::vector<Obol::InstanceId> changed;
    changed.reserve(next.size() + impl_->selected_.size());
    for (const Obol::InstanceId& id : impl_->selected_)
        if (!next.count(id))
            changed.push_back(id);
    for (const Obol::InstanceId& id : next)
        if (!impl_->selected_.count(id))
            changed.push_back(id);
    impl_->selected_.swap(next);
    bool sparsePlanPatch = !impl_->planDirty_ && !impl_->geometryDirty_;
    for (const Obol::InstanceId& id : changed)
        if (sparsePlanPatch && !impl_->patchCachedInstanceFlags(id))
            sparsePlanPatch = false;
    if (sparsePlanPatch) {
        std::unordered_set<Obol::PartId, std::hash<Obol::PartId>>
            selectionParts;
        selectionParts.reserve(changed.size());
        for (const Obol::InstanceId& id : changed) {
            const auto instance = impl_->instances_.find(id);
            if (instance != impl_->instances_.end())
                selectionParts.insert(instance->second.partId);
        }
        impl_->refreshWireProxyParts(selectionParts);
        impl_->finishSparsePresentationPatch();
    } else {
        impl_->planDirty_ = true;
        impl_->planDirtyReason_ = "selected-set";
        impl_->pendingInstanceAttributeIndices_.clear();
        impl_->pendingSubpixelProxyChange_ = false;
    }
    if (!impl_->inUpdate_) touch();
}

void
SoCADAssembly::setPointProxyProtectedInstances(
    const std::vector<Obol::InstanceId>& ids)
{
    std::unordered_set<Obol::InstanceId, std::hash<Obol::InstanceId>> next;
    next.reserve(ids.size());
    for (const Obol::InstanceId& id : ids)
        if (impl_->instances_.find(id) != impl_->instances_.end())
            next.insert(id);
    if (next == impl_->pointProxyProtected_)
        return;
    std::vector<Obol::InstanceId> changed;
    changed.reserve(next.size() + impl_->pointProxyProtected_.size());
    for (const Obol::InstanceId& id : impl_->pointProxyProtected_)
        if (!next.count(id))
            changed.push_back(id);
    for (const Obol::InstanceId& id : next)
        if (!impl_->pointProxyProtected_.count(id))
            changed.push_back(id);
    impl_->pointProxyProtected_.swap(next);
    bool sparsePlanPatch = !impl_->planDirty_ && !impl_->geometryDirty_;
    for (const Obol::InstanceId& id : changed)
        if (sparsePlanPatch && !impl_->patchCachedInstanceFlags(id))
            sparsePlanPatch = false;
    if (sparsePlanPatch) {
        std::unordered_set<Obol::PartId, std::hash<Obol::PartId>> parts;
        parts.reserve(changed.size());
        for (const Obol::InstanceId& id : changed) {
            const auto instance = impl_->instances_.find(id);
            if (instance != impl_->instances_.end())
                parts.insert(instance->second.partId);
        }
        impl_->refreshWireProxyParts(parts);
        impl_->finishSparsePresentationPatch();
    } else {
        impl_->planDirty_ = true;
        impl_->planDirtyReason_ = "point-proxy-protection-set";
        impl_->pendingInstanceAttributeIndices_.clear();
        impl_->pendingSubpixelProxyChange_ = false;
    }
    if (!impl_->inUpdate_) touch();
}

std::vector<Obol::InstanceId>
SoCADAssembly::pointProxyProtectedInstances() const
{
    std::vector<Obol::InstanceId> ids;
    ids.reserve(impl_->pointProxyProtected_.size());
    for (const Obol::InstanceId& id : impl_->pointProxyProtected_)
        ids.push_back(id);
    return ids;
}

void
SoCADAssembly::adoptPointProxyProtectedInstances(
    std::unordered_set<Obol::InstanceId,
        std::hash<Obol::InstanceId>>&& ids)
{
    impl_->pointProxyProtected_.swap(ids);
    /* Protection affects only view-local point classification.  Keep the
     * immutable geometry/instance plan and its GPU resources intact.  The
     * classifier builds the new mask and aggregate point list into scratch
     * storage over bounded frames, then swaps every product atomically; the
     * previous complete presentation remains drawable until that publication.
     * The caller has already proved that the adopted set differs, so this
     * commit performs no second O(instances) equality pass. */
    impl_->subpixelProxyBuildActive_ = false;
    impl_->subpixelProxyViewValid_ = false;
    if (!impl_->inUpdate_) touch();
}

// --- Query -----------------------------------------------------------------

size_t SoCADAssembly::instanceCount() const { return impl_->instances_.size(); }
size_t SoCADAssembly::partCount()     const { return impl_->parts_.size();     }
size_t SoCADAssembly::selectedInstanceCount() const
{
    return impl_->selected_.size();
}

std::vector<Obol::InstanceId>
SoCADAssembly::instanceIds() const
{
    std::vector<Obol::InstanceId> ids;
    ids.reserve(impl_->instances_.size());
    for (const auto &entry : impl_->instances_)
        ids.push_back(entry.first);
    std::sort(ids.begin(), ids.end(),
        [](const Obol::InstanceId &a, const Obol::InstanceId &b) {
            return a.w0 != b.w0 ? a.w0 < b.w0 : a.w1 < b.w1;
        });
    return ids;
}

bool
SoCADAssembly::isInstanceHidden(Obol::InstanceId iid) const
{
    return impl_->hidden_.find(iid) != impl_->hidden_.end();
}

bool SoCADAssembly::hasProgressivePartLod() const
{
    return !impl_->progressiveParts_.empty();
}

uint8_t SoCADAssembly::effectiveProgressiveCut(
    uint8_t requested) const
{
    const int ceiling = progressiveCutCeiling.getValue();
    if (ceiling < 0 || ceiling >= Obol::ProgressiveCutUnspecified)
        return requested;
    return std::min(requested, static_cast<uint8_t>(ceiling));
}

namespace {

static uint64_t
cadProgressiveFractionHash(Obol::PartId part)
{
    static constexpr uint64_t hashCombineConstant =
        0x9e3779b97f4a7c15ULL;
    static constexpr uint64_t splitMixFirstMultiplier =
        0xbf58476d1ce4e5b9ULL;
    static constexpr uint64_t splitMixSecondMultiplier =
        0x94d049bb133111ebULL;
    uint64_t value = part.w0 ^
        (part.w1 + hashCombineConstant +
         (part.w0 << 6) + (part.w0 >> 2));
    value ^= value >> 30;
    value *= splitMixFirstMultiplier;
    value ^= value >> 27;
    value *= splitMixSecondMultiplier;
    value ^= value >> 31;
    return value;
}

} // namespace

uint8_t SoCADAssembly::effectiveProgressiveCut(
    Obol::PartId part, uint8_t requested) const
{
    const uint8_t base = effectiveProgressiveCut(requested);
    const int ceiling = progressiveCutCeiling.getValue();
    const float fraction = progressiveCutNextFraction.getValue();
    if (base >= requested || ceiling < 0 ||
            !std::isfinite(fraction) || fraction <= 0.0f)
        return base;
    if (fraction >= 1.0f)
        return static_cast<uint8_t>(base + 1u);

    const long double normalized = static_cast<long double>(
        cadProgressiveFractionHash(part)) /
        static_cast<long double>(std::numeric_limits<uint64_t>::max());
    return normalized < static_cast<long double>(fraction) ?
        static_cast<uint8_t>(base + 1u) : base;
}

uint8_t SoCADAssembly::maximumEffectiveProgressiveCut(
    uint8_t requested) const
{
    const uint8_t base = effectiveProgressiveCut(requested);
    const float fraction = progressiveCutNextFraction.getValue();
    return base < requested && std::isfinite(fraction) && fraction > 0.0f ?
        static_cast<uint8_t>(base + 1u) : base;
}

const Obol::PartGeometry*
SoCADAssembly::partGeometry(Obol::PartId pid) const
{
    auto it = impl_->parts_.find(pid);
    if (it == impl_->parts_.end()) return nullptr;
    return it->second.get();
}

std::optional<Obol::InstanceRecord>
SoCADAssembly::getInstanceRecord(Obol::InstanceId iid) const
{
    auto it = impl_->instances_.find(iid);
    if (it == impl_->instances_.end()) return std::nullopt;
    const InstanceData& d = it->second;
    Obol::InstanceRecord rec;
    rec.part        = d.partId;
    rec.localToRoot = d.localToRoot;
    rec.style       = d.style;
    rec.parent      = d.parent;
    rec.childName  = d.childName;
    rec.occurrenceIndex = d.occurrenceIndex;
    rec.boolOp      = d.boolOp;
    rec.lodCut    = d.lodCut;
    rec.lodStructuralProxy = d.lodStructuralProxy;
    return rec;
}

void
SoCADAssembly::setHiddenInstances(const std::vector<Obol::InstanceId>& ids)
{
    std::unordered_set<Obol::InstanceId, std::hash<Obol::InstanceId>> next;
    next.reserve(ids.size());
    for (const Obol::InstanceId& id : ids)
        if (impl_->instances_.find(id) != impl_->instances_.end())
            next.insert(id);
    if (next == impl_->hidden_)
        return;
    std::vector<Obol::InstanceId> changed;
    changed.reserve(next.size() + impl_->hidden_.size());
    for (const Obol::InstanceId& id : impl_->hidden_)
        if (!next.count(id))
            changed.push_back(id);
    for (const Obol::InstanceId& id : next)
        if (!impl_->hidden_.count(id))
            changed.push_back(id);
    impl_->hidden_.swap(next);
    bool sparsePlanPatch = !impl_->planDirty_ && !impl_->geometryDirty_;
    for (const Obol::InstanceId& id : changed)
        if (sparsePlanPatch && !impl_->patchCachedInstanceFlags(id))
            sparsePlanPatch = false;
    if (sparsePlanPatch) {
        /*
         * Screen-size membership is unchanged, but the retained per-part
         * "has an uncollapsed wire occurrence" summary is visibility
         * sensitive.  Leaving it untouched keeps hidden structural proxies
         * in both the render fast-path set and its visible-box diagnostic
         * until an unrelated camera change forces a full classification.
         *
         * Refresh only parts referenced by changed instances.  This makes
         * final retirement of one cold-start overview O(1), rather than
         * rescanning every occurrence in a 50k scene.
         */
        std::unordered_set<Obol::PartId, std::hash<Obol::PartId>>
            visibilityParts;
        visibilityParts.reserve(changed.size());
        for (const Obol::InstanceId& id : changed) {
            const auto instance = impl_->instances_.find(id);
            if (instance != impl_->instances_.end())
                visibilityParts.insert(instance->second.partId);
        }
        impl_->refreshWireProxyParts(visibilityParts);
        impl_->finishSparsePresentationPatch(true);
    } else {
        impl_->planDirty_ = true;
        impl_->planDirtyReason_ = "hidden-set";
        impl_->pendingInstanceAttributeIndices_.clear();
    }
    impl_->bvhDirty_ = true;
    if (!impl_->inUpdate_) touch();
}

void
SoCADAssembly::setUnpickableInstances(const std::vector<Obol::InstanceId>& ids)
{
    impl_->unpickable_.clear();
    impl_->unpickable_.insert(ids.begin(), ids.end());
    impl_->bvhDirty_ = true;
    if (!impl_->inUpdate_) touch();
}

// ---------------------------------------------------------------------------
// GLRender
// ---------------------------------------------------------------------------

void
SoCADAssembly::GLRender(SoGLRenderAction* action)
{
    if (impl_->instances_.empty()) return;

    SoState* state = action->getState();

    // Obtain the GL dispatch context for the active rendering backend.
    // This routes calls correctly to either the system OpenGL or OSMesa.
    const SoGLContext* glue = sogl_glue_from_state(state);
    if (!glue) return;

    // SoCADAssembly issues GL calls directly instead of going through
    // SoShape.  Synchronize Coin's lazy shape state before doing so: the raw
    // GL cull bit may legitimately still describe a sibling whose separator
    // has already popped, while the element state already says this node is
    // two-sided.  Establish counter-clockwise/two-sided as the conservative
    // assembly baseline; the renderer locally enables culling only for parts
    // carrying a verified closed/oriented guarantee and restores this raw
    // state before returning.  Keeping this in a local state frame lets the
    // next node restore its own semantics through the normal lazy-element
    // path.
    state->push();
    SoShapeHintsElement::set(state, this,
        SoShapeHintsElement::COUNTERCLOCKWISE,
        SoShapeHintsElement::UNKNOWN_SHAPE_TYPE,
        SoShapeHintsElement::CONVEX);
    SoGLLazyElement::getInstance(state)->send(state,
        SoLazyElement::VERTEXORDERING_MASK |
        SoLazyElement::CULLING_MASK |
        SoLazyElement::TWOSIDE_MASK);

    // Lazy-create the renderer the first time we have a GL context.
    if (!impl_->renderer_) {
        impl_->renderer_ = std::make_unique<Obol::internal::CadRendererGL>();
    }

    // Build the combined view-projection matrix from the state stack.
    // Both matrices are OI row-major SbMatrix values.
    const SbMatrix viewMat = SoViewingMatrixElement::get(state);
    const SbMatrix projMat = SoProjectionMatrixElement::get(state);
    // OI post-multiply convention: VP = view * proj
    SbMatrix viewProj = viewMat;
    viewProj.multRight(projMat);

    const int dm = drawMode.getValue();

    // Rebuild the frame plan only when geometry, instances, styles, selection,
    // hidden set, or draw mode have changed.  Camera moves do NOT invalidate
    // the plan, so it is reused every frame during interactive orbit.
    if (impl_->planDirty_ || impl_->cachedDM_ != dm) {
        const bool geometryChanged = impl_->geometryDirty_ ||
                                     impl_->cachedDM_ != dm;
        ++impl_->framePlanBuildCount_;
        if (cadPlanDebugEnabled() &&
                impl_->planDebugBuildMessageCount_++ <
                    cadPlanDebugMessageLimit())
            std::fprintf(stderr,
                "SoCADAssembly frame plan build count=%llu "
                "plan_dirty=%d geometry_dirty=%d reason=%s "
                "old_mode=%d new_mode=%d "
                "instances=%zu parts=%zu\n",
                static_cast<unsigned long long>(
                    impl_->framePlanBuildCount_),
                impl_->planDirty_ ? 1 : 0,
                impl_->geometryDirty_ ? 1 : 0,
                impl_->planDirtyReason_ ? impl_->planDirtyReason_ :
                    "unknown",
                impl_->cachedDM_, dm, impl_->instances_.size(),
                impl_->parts_.size());
        /*
         * A full structural rebuild is an atomic retained-state transaction,
         * not a resumable render operation.  Aborting the local candidate or
         * reverse indexes discards every byte of progress and can livelock an
         * all-at-once warm cache forever.  Build them once without consulting
         * the presentation deadline; the common deadline check below still
         * prevents a late GL draw, so the next frame reuses the completed
         * plan.  Normal streaming, LoD, style, visibility, and selection
         * changes use the append/sparse paths and do not pay this cost.
         */
        Obol::internal::CadFramePlan candidatePlan =
            impl_->buildFramePlan(dm, impl_->selected_, impl_->hidden_);
        ++impl_->renderPreparationSerial_;
        if (impl_->renderPreparationSerial_ == 0)
            impl_->renderPreparationSerial_ = 1;
        impl_->cachedPlan_ = std::move(candidatePlan);
        impl_->cachedPlan_.revision = impl_->nextPlanRevision_++;
        if (impl_->nextPlanRevision_ == 0)
            impl_->nextPlanRevision_ = 1;
        impl_->cachedPlan_.shadedLayoutRevision =
            impl_->nextShadedLayoutRevision_++;
        if (impl_->nextShadedLayoutRevision_ == 0)
            impl_->nextShadedLayoutRevision_ = 1;
        impl_->cachedPlan_.subpixelProxyInputRevision =
            impl_->nextSubpixelProxyInputRevision_++;
        if (impl_->nextSubpixelProxyInputRevision_ == 0)
            impl_->nextSubpixelProxyInputRevision_ = 1;
        impl_->cachedPlan_.shadedLodRevision =
            impl_->nextShadedLodRevision_++;
        if (impl_->nextShadedLodRevision_ == 0)
            impl_->nextShadedLodRevision_ = 1;
        impl_->cachedPlan_.shadedLodDeltaFloorRevision =
            impl_->cachedPlan_.shadedLodRevision;
        impl_->cachedPlan_.appendRevision =
            impl_->nextAppendRevision_++;
        if (impl_->nextAppendRevision_ == 0)
            impl_->nextAppendRevision_ = 1;
        impl_->cachedPlan_.appendDeltaFloorRevision =
            impl_->cachedPlan_.appendRevision;
        impl_->cachedPlan_.partGeometryRevision =
            impl_->nextPartGeometryRevision_++;
        if (impl_->nextPartGeometryRevision_ == 0)
            impl_->nextPartGeometryRevision_ = 1;
        impl_->cachedPlan_.partGeometryDeltaFloorRevision =
            impl_->cachedPlan_.partGeometryRevision;
        impl_->cachedPlan_.instanceAttributeRevision =
            impl_->nextInstanceAttributeRevision_++;
        if (impl_->nextInstanceAttributeRevision_ == 0)
            impl_->nextInstanceAttributeRevision_ = 1;
        impl_->cachedPlan_.instanceAttributeDeltaFloorRevision =
            impl_->cachedPlan_.instanceAttributeRevision;
        impl_->pendingInstanceAttributeIndices_.clear();
        if (geometryChanged) {
            impl_->geometryRevision_ = impl_->nextGeometryRevision_++;
            if (impl_->nextGeometryRevision_ == 0)
                impl_->nextGeometryRevision_ = 1;
        }
        impl_->cachedPlan_.geometryRevision = impl_->geometryRevision_;
        impl_->cachedPlanTombstoneCount_ = 0;
        impl_->cachedDM_    = dm;
        if (!impl_->rebuildProgressiveShadedPlanIndex()) {
            /* No deadline callback is supplied, so this is defensive against
             * any future semantic failure mode rather than a retry path. */
            impl_->planDirty_ = true;
            impl_->planDirtyReason_ = "plan-index-build";
            SoGLLazyElement::getInstance(state)->reset(
                state, SoLazyElement::ALL_MASK);
            state->pop();
            return;
        }
        impl_->planDirty_ = false;
        impl_->geometryDirty_ = false;
    }

    const SbViewportRegion& viewport = SoViewportRegionElement::get(state);
    const SbViewVolume& viewVolume = SoViewVolumeElement::get(state);
    bool subpixelPreparationPerformed = false;
    const bool subpixelPreparationComplete =
        impl_->updateSubpixelProxyPlan(viewProj,
            viewport.getViewportSizePixels(),
            pointProxyPixelThreshold.getValue(),
            cameraMotionFrameReuse.getValue(), action,
            &subpixelPreparationPerformed);
    if (subpixelPreparationPerformed) {
        ++impl_->renderPreparationSerial_;
        if (impl_->renderPreparationSerial_ == 0)
            impl_->renderPreparationSerial_ = 1;
    }
    if (!subpixelPreparationComplete) {
        SoGLLazyElement::getInstance(state)->reset(
            state, SoLazyElement::ALL_MASK);
        state->pop();
        return;
    }

    /*
     * Do not test the host deadline before resumable presentation
     * preparation.  If traversal above this node has already exhausted the
     * frame, doing so prevents its retained cursor from ever advancing.  The
     * classifier supplies bounded safe points and publishes atomically; once
     * it completes, honor the deadline before issuing any GL work.  A final
     * over-budget preparation frame therefore retains its completed result,
     * and the next frame reuses it in O(1) before drawing.
     */
    if (action->abortNow()) {
        SoGLLazyElement::getInstance(state)->reset(
            state, SoLazyElement::ALL_MASK);
        state->pop();
        return;
    }

    const Obol::CadRenderState renderState =
        Obol::resolveCadRenderState(SoCADViewStateElement::get(state));
    const SoClipPlaneElement *clipPlanes =
        SoClipPlaneElement::getInstance(state);
    /* Coin owns the accumulated plane state.  The fixed-function retained
     * path consumes that exact state, while the shader paths deliberately do
     * not borrow legacy GL clip state.  Prefer correctness for the optional
     * sectioning feature until all shader tiers consume an explicit shared
     * plane contract. */
    const bool fixedFunctionClipPlanes = clipPlanes &&
        clipPlanes->getNum() > 0;

    const GLboolean lightingEnabled = glue->glIsEnabled(GL_LIGHTING);
    const GLboolean light0Enabled = glue->glIsEnabled(GL_LIGHT0);
    const bool hasTransparency = std::any_of(
        impl_->cachedPlan_.visibleInstances.begin(),
        impl_->cachedPlan_.visibleInstances.end(),
        [](const Obol::internal::CadVisibleInstance& instance) {
            return instance.rgba[3] < 255;
        });
    const GLboolean blendEnabled = glue->glIsEnabled(GL_BLEND);
    GLint blendSource = GL_ONE;
    GLint blendDestination = GL_ZERO;
    if (hasTransparency) {
        glue->glGetIntegerv(GL_BLEND_SRC, &blendSource);
        glue->glGetIntegerv(GL_BLEND_DST, &blendDestination);
        glue->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glue->glEnable(GL_BLEND);
    }

    ++impl_->renderExecutionSerial_;
    if (impl_->renderExecutionSerial_ == 0)
        impl_->renderExecutionSerial_ = 1;

    // Explicit FAST mode allows ordinary software wireframes to bypass Mesa's
    // fixed-function interpreter.  AUTO is deliberately quality-first because
    // direct CPU rasterization is workload-dependent and can be slower.
    CadSoftwareWireRenderResult softwareWireResult;
    if (dm == WIREFRAME &&
            renderState.softwareWireMode == Obol::CadSoftwareWireMode::FAST) {
        const std::vector<Obol::internal::CadSubpixelProxyPoint>&
            presentationPoints =
                impl_->renderer_->subpixelProxyPresentationPoints(
                    impl_->cachedPlan_, glue, viewProj);
        softwareWireResult = cadRenderSoftwareWire(
            impl_->cachedPlan_, *this, state, viewProj, presentationPoints);
    }
    const bool softwareWire = softwareWireResult.rendered;
    impl_->lastDirectSoftwareWire_ = softwareWire;
    if (softwareWire) {
        impl_->renderer_->completeDirectSoftwareWireFrame(
            softwareWireResult.work, glue->contextid,
            softwareWireResult.subpixelProxyDrawPointCount);
    } else {
        // Feed the shaded GLSL pass ALL enabled scene lights (the camera-tracked
        // headlight plus any in-scene database lights), so the hardware view
        // lights consistently with the fixed-function path instead of using a
        // single hardcoded world-fixed direction.  Directional, point, and spot
        // lights are all supported (up to CadRendererGL::kMaxLights), each with
        // its own RGB colour x intensity.
        //
        // The shaded GLSL pass lights in WORLD space (v_worldPos/v_norm use the
        // model matrix without the view).  Obol authors all its lights directly
        // in world coordinates -- the headlight direction is rewritten in world
        // space each frame, and DB light positions are world bbox centers -- and
        // places them without transform nodes above them, so the SoLight field
        // values ARE world space.  We therefore read the raw fields rather than
        // SoLightElement::getMatrix(), whose accumulated matrix for the
        // post-camera scene-lights group is contaminated with the view transform
        // in this custom render batch (which would put point/spot positions in
        // eye space and make them drift with the camera).
        const SoNodeList& lights = SoLightElement::getLights(state);
        std::vector<Obol::internal::CadRendererGL::GlLight> glLights;
        for (int li = 0; li < lights.getLength(); ++li) {
            SoLight* l = static_cast<SoLight*>(lights[li]);
            if (!l || !l->on.getValue())
                continue;
            const SbColor c = l->color.getValue();
            const float inten = l->intensity.getValue();
            Obol::internal::CadRendererGL::GlLight gl;
            gl.color[0] = c[0] * inten;
            gl.color[1] = c[1] * inten;
            gl.color[2] = c[2] * inten;
            if (l->isOfType(SoDirectionalLight::getClassTypeId())) {
                SoDirectionalLight* dl = static_cast<SoDirectionalLight*>(l);
                SbVec3f travel = dl->direction.getValue();
                if (travel.length() <= 0.0f)
                    continue;
                travel.normalize();
                gl.type = 0;  // directional; shader wants direction toward light
                gl.vec[0] = -travel[0]; gl.vec[1] = -travel[1];
                gl.vec[2] = -travel[2];
            } else if (l->isOfType(SoSpotLight::getClassTypeId())) {
                SoSpotLight* sl = static_cast<SoSpotLight*>(l);
                SbVec3f pos = sl->location.getValue();
                SbVec3f axis = sl->direction.getValue();
                if (axis.length() > 0.0f) axis.normalize();
                gl.type = 2;  // spot
                gl.vec[0] = pos[0];  gl.vec[1] = pos[1];  gl.vec[2] = pos[2];
                gl.axis[0] = axis[0]; gl.axis[1] = axis[1]; gl.axis[2] = axis[2];
                gl.cosCutoff =
                    static_cast<float>(std::cos(sl->cutOffAngle.getValue()));
            } else if (l->isOfType(SoPointLight::getClassTypeId())) {
                SoPointLight* pl = static_cast<SoPointLight*>(l);
                SbVec3f pos = pl->location.getValue();
                gl.type = 1;  // point
                gl.vec[0] = pos[0]; gl.vec[1] = pos[1]; gl.vec[2] = pos[2];
            } else {
                continue;
            }
            glLights.push_back(gl);
        }
        // Empty list => renderer falls back to its default fixed light.
        impl_->renderer_->setLights(glLights);
        const SbColor& ambientColor =
            SoEnvironmentElement::getAmbientColor(state);
        impl_->renderer_->setAmbientLight(
            ambientColor[0], ambientColor[1], ambientColor[2],
            SoEnvironmentElement::getAmbientIntensity(state));
        if (cadLightDebugEnabled()) {
            static unsigned int reportCount = 0;
            if (reportCount++ < 32) {
                std::fprintf(stderr,
                    "SoCADAssembly lights count=%zu stateCount=%d",
                    glLights.size(), lights.getLength());
                for (size_t i = 0;
                     i < std::min<size_t>(glLights.size(), 2); ++i) {
                    const auto& gl = glLights[i];
                    std::fprintf(stderr,
                        " l%zu={type=%d vec=(%.9g,%.9g,%.9g) "
                        "axis=(%.9g,%.9g,%.9g) "
                        "color=(%.9g,%.9g,%.9g) cos=%.9g}",
                        i, gl.type,
                        gl.vec[0], gl.vec[1], gl.vec[2],
                        gl.axis[0], gl.axis[1], gl.axis[2],
                        gl.color[0], gl.color[1], gl.color[2],
                        gl.cosCutoff);
                }
                std::fprintf(stderr, "\n");
            }
        }

        // Delegate to the VBO + shader renderer (GL 2.0 minimum; optional GL
        // 3.1+ instanced path selected automatically when available).
        impl_->renderer_->render(impl_->cachedPlan_, *this, action, glue, viewProj,
                                 viewMat, projMat, viewVolume,
				 fixedFunctionClipPlanes,
                                 impl_->partGeneration_);
        impl_->presentationPreparation_ =
            impl_->renderer_->presentationPreparationSnapshot();
    }
    if (hasTransparency) {
        glue->glBlendFunc(static_cast<GLenum>(blendSource),
                          static_cast<GLenum>(blendDestination));
        if (!blendEnabled) glue->glDisable(GL_BLEND);
    }
    if (cadDebugEnabled()) {
        std::fprintf(stderr,
                     "SoCADAssembly render tier=%d visible=%zu wireItems=%zu "
                     "shadedItems=%zu parts=%zu instances=%zu "
                     "softwareWireMode=%d direct=%d lighting=%d light0=%d\n",
                     impl_->renderer_->lastRenderTier(),
                     impl_->cachedPlan_.visibleInstances.size(),
                     impl_->cachedPlan_.wireItems.size(),
                     impl_->cachedPlan_.shadedItems.size(),
                     impl_->parts_.size(),
                     impl_->instances_.size(),
                     static_cast<int>(renderState.softwareWireMode),
                     softwareWire ? 1 : 0,
                     lightingEnabled ? 1 : 0, light0Enabled ? 1 : 0);
    }
    // The CAD renderer deliberately bypasses Coin's normal SoShape path and
    // issues raw GL calls.  Even though it restores the raw raster state it
    // borrows, material, lighting, blending, and shader transitions may no
    // longer match SoGLLazyElement's cached belief.  Invalidate that cache
    // before popping our local state frame so the next Coin node resends its
    // own state instead of inheriting a stale CAD frame.  This is the proper
    // renderer boundary; hosts must not compensate with extra clears or
    // presentation timing workarounds.
    SoGLLazyElement::getInstance(state)->reset(state,
        SoLazyElement::ALL_MASK);
    state->pop();
}

// ---------------------------------------------------------------------------
// rayPick
// ---------------------------------------------------------------------------

void
SoCADAssembly::rayPick(SoRayPickAction* action)
{
    if (impl_->instances_.empty()) return;

    impl_->rebuildBvhIfNeeded();

    // Match SoShape picking semantics: SoRayPickAction stores the active
    // ray in object space only after setObjectSpace() updates it from the
    // current traversal state.
    action->setObjectSpace();
    SbLine pickRay = action->getLine();

    // Determine effective pick mode
    int pm = pickMode.getValue();
    const bool automaticPick = pm == PICK_AUTO;
    if (pm == PICK_AUTO) {
        pm = (drawMode.getValue() == WIREFRAME) ? PICK_EDGE : PICK_TRIANGLE;
    }

    // Derive an assembly-space edge-pick tolerance from the screen-space field.
    // Approximate: use the view volume to find how large one pixel is in world
    // coordinates at the assembly centre, then scale by the user-specified tolerance.
    float toleranceWS = edgePickTolerancePx.getValue() * 0.01f;
    {
        SoState* state = action->getState();
        if (state) {
            const SbViewportRegion& vpr =
                SoViewportRegionElement::get(state);
            const SbViewVolume vv = SoViewVolumeElement::get(state);
            const float vpH = static_cast<float>(
                vpr.getViewportSizePixels()[1]);
            if (vpH > 0.0f && vv.getNearDist() > 0.0f) {
                // Pick ray distance to assembly centre (or fallback to near*10)
                SbBox3f bbox;
                for (const auto& [iid, idata] : impl_->instances_) {
                    if (impl_->hidden_.count(iid) ||
                            impl_->unpickable_.count(iid))
                        continue;
                    if (!idata.worldBounds.isEmpty())
                        bbox.extendBy(idata.worldBounds);
                }
                float dist = vv.getNearDist() * 10.0f;
                if (!bbox.isEmpty()) {
                    dist = (bbox.getCenter() - pickRay.getPosition())
                               .dot(pickRay.getDirection());
                    dist = std::max(vv.getNearDist(), dist);
                }
                // Height of the view volume at that distance (perspective or ortho)
                float nearH  = vv.getHeight();          // at nearDist for persp
                float nearD  = vv.getNearDist();
                float pixelH = (nearH / vpH) * (dist / nearD);
                toleranceWS = std::max(toleranceWS,
                    edgePickTolerancePx.getValue() * pixelH);
            }
        }
    }

    Obol::picking::CadPickResult result;
    const int configuredCutCeiling = progressiveCutCeiling.getValue();
    const uint8_t pickCutCeiling =
        configuredCutCeiling >= 0 &&
            configuredCutCeiling <
                static_cast<int>(Obol::ProgressiveCutLimit) ?
        static_cast<uint8_t>(configuredCutCeiling) :
        Obol::ProgressiveCutUnspecified;

    if (automaticPick || pm == PICK_EDGE || pm == PICK_HYBRID) {
        result = Obol::picking::CadPickQuery::pickPoint(
            pickRay, impl_->instanceBvh_, impl_->parts_, toleranceWS);
    }

    if (!result.valid && (pm == PICK_EDGE || pm == PICK_HYBRID)) {
        result = Obol::picking::CadPickQuery::pickEdge(
            pickRay,
            impl_->instanceBvh_,
            impl_->parts_,
            impl_->partEdgeBvhCache_,
            toleranceWS,
            pickCutCeiling);
    }

    if (!result.valid && (pm == PICK_TRIANGLE || pm == PICK_HYBRID)) {
        result = Obol::picking::CadPickQuery::pickTriangle(
            pickRay,
            impl_->instanceBvh_,
            impl_->parts_,
            impl_->partTriBvhCache_,
            toleranceWS,
            pickCutCeiling);
    }

    if (!result.valid && pm == PICK_BOUNDS) {
        result = Obol::picking::CadPickQuery::pickBounds(
            pickRay,
            impl_->instanceBvh_,
            toleranceWS);
    }

    // For PICK_HYBRID: also try bounds if triangle picking returned nothing.
    if (!result.valid && pm == PICK_HYBRID) {
        result = Obol::picking::CadPickQuery::pickBounds(
            pickRay,
            impl_->instanceBvh_,
            toleranceWS);
    }

    if (!result.valid) return;

    // Register the hit with the pick action
    SoPickedPoint* pp = action->addIntersection(result.hitPoint);
    if (!pp) return;

    Obol::CadPickDetailRecord hit;
    hit.instance = result.instanceId;
    hit.part = result.partId;
    hit.point = result.hitPoint;
    switch (result.primType) {
        case Obol::picking::CadPickResult::EDGE:
            hit.primitiveKind = Obol::CadPickDetailRecord::EDGE;
            hit.primIndex0 = result.primIndex0;
            hit.primIndex1 = result.primIndex1;
            hit.u = result.u;
            break;
        case Obol::picking::CadPickResult::TRIANGLE:
            hit.primitiveKind = Obol::CadPickDetailRecord::TRIANGLE;
            hit.primIndex0 = result.primIndex0;
            break;
        case Obol::picking::CadPickResult::POINT:
            hit.primitiveKind = Obol::CadPickDetailRecord::POINT;
            hit.primIndex0 = result.primIndex0;
            break;
        default:
            hit.primitiveKind = Obol::CadPickDetailRecord::BOUNDS;
            break;
    }

    SoDetail* detail = this->createPickDetail(hit);
    if (detail) {
        SoNode* detailNode = this;
        SoPath* path = pp->getPath();
        if (path && path->findNode(this) < 0 && path->getFullLength() > 0) {
            detailNode = path->getNode(path->getFullLength() - 1);
        }
        pp->setDetail(detail, detailNode);
    }
}

// ---------------------------------------------------------------------------
// getBoundingBox
// ---------------------------------------------------------------------------

void
SoCADAssembly::getBoundingBox(SoGetBoundingBoxAction* action)
{
    SbBox3f worldBox;
    for (const auto& [iid, idata] : impl_->instances_) {
        if (impl_->hidden_.count(iid))
            continue;
        if (!idata.worldBounds.isEmpty()) {
            worldBox.extendBy(idata.worldBounds);
        }
    }
    if (!worldBox.isEmpty()) {
        action->extendBy(worldBox);
        action->setCenter(worldBox.getCenter(), TRUE);
    }
}

// ---------------------------------------------------------------------------
// getPrimitiveCount
// ---------------------------------------------------------------------------

void
SoCADAssembly::getPrimitiveCount(SoGetPrimitiveCountAction* action)
{
    // Count total segments and triangles across all visible instances
    int totalLines = 0;
    int totalTris  = 0;
    for (const auto& [iid, idata] : impl_->instances_) {
        if (impl_->hidden_.count(iid))
            continue;
        auto geomIt = impl_->parts_.find(idata.partId);
        if (geomIt == impl_->parts_.end() || !geomIt->second) continue;
        const auto& geom = *geomIt->second;
        if (geom.points)
            action->addNumPoints(static_cast<int>(geom.points->positions.size()));
        if (geom.wire) {
            totalLines += static_cast<int>(geom.wire->segmentCount());
            for (const auto& poly : geom.wire->polylines) {
                if (poly.points.size() >= 2) {
                    totalLines += static_cast<int>(poly.points.size() - 1);
                }
            }
        }
        if (geom.shaded) {
            totalTris += static_cast<int>(geom.shaded->indices.size() / 3);
        }
    }
    action->addNumLines(totalLines);
    action->addNumTriangles(totalTris);
}

// ---------------------------------------------------------------------------
// lastRenderTier
// ---------------------------------------------------------------------------

int
SoCADAssembly::lastRenderTier() const
{
    if (!impl_->renderer_) return -1;
    return impl_->renderer_->lastRenderTier();
}

int
SoCADAssembly::lastIndirectStatus() const
{
    if (!impl_->renderer_) return -1;
    return impl_->renderer_->lastIndirectStatus();
}

uint64_t
SoCADAssembly::lastRenderedTriangleCount() const
{
    if (!impl_->renderer_) return 0;
    return impl_->renderer_->lastRenderedTriangleCount();
}

Obol::CadRenderedWork
SoCADAssembly::lastRenderedWork() const
{
    return impl_->renderer_ ? impl_->renderer_->lastRenderedWork() :
        Obol::CadRenderedWork();
}

uint64_t
SoCADAssembly::lastGpuRenderNanoseconds() const
{
    return impl_->renderer_ ?
        impl_->renderer_->lastGpuRenderNanoseconds() : 0;
}

uint64_t
SoCADAssembly::lastGpuRenderedTriangleCount() const
{
    return impl_->renderer_ ?
        impl_->renderer_->lastGpuRenderedTriangleCount() : 0;
}

float
SoCADAssembly::lastGpuPointProxyPixelThreshold() const
{
    return impl_->renderer_ ?
        impl_->renderer_->lastGpuPointProxyPixelThreshold() : 1.0f;
}

uint64_t
SoCADAssembly::gpuTimerSampleSerial() const
{
    return impl_->renderer_ ?
        impl_->renderer_->gpuTimerSampleSerial() : 0;
}

Obol::CadGpuResourceSnapshot
SoCADAssembly::gpuResourceSnapshot() const
{
    return impl_->renderer_ ? impl_->renderer_->gpuResourceSnapshot() :
        Obol::CadGpuResourceSnapshot();
}

bool
SoCADAssembly::lastRenderUsedPreparedReplay() const
{
    return impl_->renderer_ &&
        impl_->renderer_->lastRenderUsedPreparedReplay();
}

bool
SoCADAssembly::lastRenderUsedDirectSoftwareWire() const
{
    return impl_->lastDirectSoftwareWire_;
}

uint64_t
SoCADAssembly::renderExecutionSerial() const
{
    return impl_->renderExecutionSerial_;
}

uint64_t
SoCADAssembly::renderPreparationSerial() const
{
    const uint64_t rendererSerial = impl_->renderer_ ?
        impl_->renderer_->renderPreparationSerial() : 0;
    return rendererSerial > UINT64_MAX - impl_->renderPreparationSerial_ ?
        UINT64_MAX : rendererSerial + impl_->renderPreparationSerial_;
}

Obol::CadPresentationPreparationSnapshot
SoCADAssembly::presentationPreparationSnapshot() const
{
    return impl_->presentationPreparation_;
}

size_t
SoCADAssembly::lastSubpixelProxyCount() const
{
    size_t visible = 0;
    for (const Obol::internal::CadSubpixelProxyPoint& point :
            impl_->cachedPlan_.subpixelProxyPoints)
        if (!(point.flags & Obol::internal::CadInstanceHidden))
            ++visible;
    return visible +
        (impl_->renderer_ ? impl_->renderer_->lastPressureProxyCount() : 0);
}

size_t
SoCADAssembly::lastSubpixelProxyDrawPointCount() const
{
    return impl_->renderer_ ?
        impl_->renderer_->lastSubpixelProxyDrawPointCount() : 0u;
}

size_t
SoCADAssembly::lastUncollapsedStructuralProxyCount() const
{
    return impl_->uncollapsedStructuralProxyCount_;
}

std::vector<Obol::InstanceId>
SoCADAssembly::lastUncollapsedStructuralProxyInstances() const
{
    std::vector<Obol::InstanceId> instances;
    instances.reserve(impl_->uncollapsedStructuralProxyInstances_.size());
    for (const Obol::InstanceId instance :
            impl_->uncollapsedStructuralProxyInstances_)
        instances.push_back(instance);
    std::sort(instances.begin(), instances.end(),
        [](const Obol::InstanceId &left,
           const Obol::InstanceId &right) {
            return left.w0 != right.w0 ? left.w0 < right.w0 :
                left.w1 < right.w1;
        });
    return instances;
}

Obol::CadStructuralProxyProjectionHistogram
SoCADAssembly::lastStructuralProxyProjectionHistogram() const
{
    return impl_->structuralProjectionHistogram_;
}

uint64_t
SoCADAssembly::lastSubpixelProxyRevision() const
{
    return impl_->cachedPlan_.subpixelProxyRevision;
}

uint64_t
SoCADAssembly::framePlanBuildCount() const
{
    return impl_->framePlanBuildCount_;
}

size_t
SoCADAssembly::framePlanInstanceRecordCount() const
{
    return impl_->cachedPlan_.visibleInstances.size();
}
