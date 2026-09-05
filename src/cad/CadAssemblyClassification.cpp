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
 * @file CadAssemblyClassification.cpp
 * @brief View-local subpixel classification for retained CAD assemblies.
 */

#include "CadAssemblyImpl.h"
#include "CadIdentityCounter.h"
#include <Obol/cad/CadProjectedProxy.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <limits>
#include <unordered_set>
#include <vector>

namespace {

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
cadSameSubpixelProxyPoint(
        const Obol::internal::CadSubpixelProxyPoint& left,
        const Obol::internal::CadSubpixelProxyPoint& right)
{
    return left.instanceId == right.instanceId &&
        left.rgba == right.rgba &&
        left.flags == right.flags &&
        left.shape == right.shape &&
        left.position[0] == right.position[0] &&
        left.position[1] == right.position[1] &&
        left.position[2] == right.position[2] &&
        left.boundsMinimum == right.boundsMinimum &&
        left.boundsMaximum == right.boundsMaximum &&
        left.boxCornersValid == right.boxCornersValid &&
        left.boxOriented == right.boxOriented &&
        (!left.boxCornersValid || left.boxCorners == right.boxCorners);
}

static bool
cadSameSubpixelProxyPoints(
        const std::vector<Obol::internal::CadSubpixelProxyPoint>& left,
        const std::vector<Obol::internal::CadSubpixelProxyPoint>& right)
{
    if (left.size() != right.size())
        return false;
    for (size_t i = 0; i < left.size(); ++i) {
        if (!cadSameSubpixelProxyPoint(left[i], right[i]))
            return false;
    }
    return true;
}

static void
cadSetWorldProxyCorners(
    const Obol::internal::CadPartBinding& binding,
    const Obol::internal::CadVisibleInstance& instance,
    Obol::internal::CadSubpixelProxyPoint& replacement)
{
    SbMatrix model;
    model.setValue(instance.transform.data());
    for (size_t corner = 0; corner < replacement.boxCorners.size(); ++corner)
        model.multVecMatrix(binding.subpixelProxyCorners[corner],
            replacement.boxCorners[corner]);
    replacement.boxCornersValid = true;
    replacement.boxOriented = binding.subpixelProxyOriented;
}


} // namespace

CadProxyPresentation SoCADAssemblyImpl::subpixelProxyPresentationForOccurrence(
            const Obol::internal::CadFramePlan& plan,
            size_t visibleIndex,
            const SbMatrix& viewProj,
            const SbVec2s& viewportSize,
            float pixelThreshold,
            CadProxyPresentation previousPresentation,
            Obol::internal::CadSubpixelProxyPoint& replacement,
            CadStructuralProjectionSample *structuralSample) const {
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
        const bool wireActive = cachedDrawModeHasWire();
        const bool shadedActive = cachedDrawModeHasShaded();
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
        const bool wasCollapsed =
            previousPresentation == CadProxyPresentation::Point ||
            previousPresentation == CadProxyPresentation::Box;
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
        replacement.boundsMinimum = SbVec3f(
            instance.wbMin[0], instance.wbMin[1], instance.wbMin[2]);
        replacement.boundsMaximum = SbVec3f(
            instance.wbMax[0], instance.wbMax[1], instance.wbMax[2]);
        replacement.rgba = instance.rgba;
        replacement.instanceId = instance.instanceId;
        replacement.flags = instance.flags;
        const float maximumPixels = std::max(
            projected.pixelWidth, projected.pixelHeight);
        /* Five pixels is a hard legibility ceiling, so point-to-box has no
         * upper hysteresis.  Delay only the reverse transition: a retained
         * box must become clearly point-sized before changing shape. */
        const float pointExtentLimit =
            previousPresentation == CadProxyPresentation::Box ?
                Obol::CadMaximumPointProxyExtentPixels * 0.75f :
                Obol::CadMaximumPointProxyExtentPixels;
        replacement.shape = maximumPixels > pointExtentLimit ?
            CadAggregateProxyShape::Box : CadAggregateProxyShape::Point;
        if (replacement.shape == CadAggregateProxyShape::Box)
            cadSetWorldProxyCorners(binding, instance, replacement);
        return replacement.shape == CadAggregateProxyShape::Box ?
            CadProxyPresentation::Box : CadProxyPresentation::Point;
    }

void SoCADAssemblyImpl::updateStructuralProjectionForVisible(size_t visibleIndex) {
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
            static_cast<CadProxyPresentation>(
                subpixelProxyState_[visibleIndex]), ignored, &sample);
        const int8_t newBucket = cadStructuralProjectionBucket(sample);
        if (oldBucket == newBucket)
            return;
        cadUpdateStructuralProjectionHistogram(
            structuralProjectionHistogram_, oldBucket, false);
        cadUpdateStructuralProjectionHistogram(
            structuralProjectionHistogram_, newBucket, true);
        structuralProjectionBucketByVisible_[visibleIndex] = newBucket;
        structuralProjectionHistogram_.revision =
            Obol::internal::cadTakeNonzeroIdentity(
                nextStructuralProjectionRevision_);
    }

bool SoCADAssemblyImpl::patchSubpixelProxyGeometryForVisible(
            size_t visibleIndex, uint64_t priorInputRevision) {
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

        const CadProxyPresentation previousPresentation =
            static_cast<CadProxyPresentation>(
                subpixelProxyState_[visibleIndex]);
        CadSubpixelProxyPoint replacement;
        CadStructuralProjectionSample structuralSample;
        const CadProxyPresentation presentation =
            subpixelProxyPresentationForOccurrence(
                plan, visibleIndex, subpixelProxyViewProj_,
                subpixelProxyViewportSize_, subpixelProxyPixelThreshold_,
                previousPresentation, replacement, &structuralSample);
        const bool previousSuppressed =
            plan.subpixelProxyMask[visibleIndex] != 0u;
        const bool suppressed =
            presentation != CadProxyPresentation::Geometry;
        const bool previousCollapsed = currentPoint != noPoint;
        const bool collapsed =
            presentation == CadProxyPresentation::Point ||
            presentation == CadProxyPresentation::Box;
        const bool proxyOutputChanged =
            previousPresentation != presentation ||
            previousSuppressed != suppressed ||
            previousCollapsed != collapsed ||
            (collapsed && !cadSameSubpixelProxyPoint(
                plan.subpixelProxyPoints[currentPoint], replacement));

        const int8_t previousStructuralBucket =
            visibleIndex < structuralProjectionBucketByVisible_.size() ?
                structuralProjectionBucketByVisible_[visibleIndex] : -1;
        const int8_t structuralBucket =
            cadStructuralProjectionBucket(structuralSample);
        if (visibleIndex <
                structuralProjectionBucketByVisible_.size() &&
                previousStructuralBucket != structuralBucket) {
            cadUpdateStructuralProjectionHistogram(
                structuralProjectionHistogram_,
                previousStructuralBucket, false);
            structuralProjectionBucketByVisible_[visibleIndex] =
                structuralBucket;
            cadUpdateStructuralProjectionHistogram(
                structuralProjectionHistogram_, structuralBucket, true);
            if (structuralProjectionHistogram_.exact) {
                structuralProjectionHistogram_.revision =
                    Obol::internal::cadTakeNonzeroIdentity(
                        nextStructuralProjectionRevision_);
            }
        } else if (visibleIndex >=
                structuralProjectionBucketByVisible_.size() &&
                structuralProjectionHistogram_.exact) {
            structuralProjectionHistogram_.exact = false;
        }

        /* The classifier input revision is an internal freshness witness.
         * Renderer consumers need a new proxy revision only when the
         * published mask or aggregate record actually changes.  In
         * particular, replacing a startup box part with an ordinary mesh
         * while both classify as Geometry must remain a sparse geometry
         * patch rather than invalidating the complete retained frame. */
        if (!proxyOutputChanged)
            return true;

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

        subpixelProxyState_[visibleIndex] =
            static_cast<uint8_t>(presentation);
        if (presentation != CadProxyPresentation::Geometry)
            plan.subpixelProxyMask[visibleIndex] = 1u;
        if (presentation == CadProxyPresentation::Point ||
                presentation == CadProxyPresentation::Box) {
            subpixelProxyPointByVisible_[visibleIndex] =
                static_cast<uint32_t>(plan.subpixelProxyPoints.size());
            plan.subpixelProxyPoints.push_back(std::move(replacement));
            subpixelProxyVisibleByPoint_.push_back(
                static_cast<uint32_t>(visibleIndex));
        }

        plan.subpixelProxyRevision =
            Obol::internal::cadTakeNonzeroIdentity(
                nextSubpixelProxyRevision_);
        return true;
    }

void SoCADAssemblyImpl::refreshWireProxyParts(
            const std::unordered_set<Obol::PartId,
                                     std::hash<Obol::PartId>>& parts) {
        using namespace Obol::internal;
        CadFramePlan& plan = cachedPlan_;

        /* A sparse part rebind leaves the old span in the append-only plan as
         * a tombstone and adds a span for the new part.  The same visible
         * instance may consequently occur in both affected-part span lists.
         * Retire every old frontier contribution before rebuilding any new
         * one: erasing and reinserting one part at a time makes the result
         * depend on unordered_set iteration order when an old tombstone is
         * visited after the live replacement. */
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

            const auto spans = cachedPlanPartSpansByPart_.find(part);
            if (spans == cachedPlanPartSpansByPart_.end())
                continue;
            for (const CachedPlanPartSpan& span : spans->second) {
                for (uint32_t offset = 0;
                        offset < span.instanceCount; ++offset) {
                    const size_t visibleIndex =
                        span.baseInstance + offset;
                    if (visibleIndex >= plan.visibleInstances.size())
                        continue;
                    uncollapsedStructuralProxyInstances_.erase(
                        plan.visibleInstances[visibleIndex].instanceId);
                }
            }
        }

        for (const Obol::PartId part : parts) {
            auto presentation = plan.partPresentation.find(part);
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

bool SoCADAssemblyImpl::patchSubpixelProxyAppendPlan(
            const SbMatrix& viewProj,
            const SbVec2s& viewportSize,
            float pixelThreshold) {
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
                        pixelThreshold, CadProxyPresentation::Geometry,
                        replacement,
                        &structuralSample);
                const int8_t structuralBucket =
                    cadStructuralProjectionBucket(structuralSample);
                structuralProjectionBucketByVisible_[visibleIndex] =
                    structuralBucket;
                cadUpdateStructuralProjectionHistogram(
                    structuralProjectionHistogram_, structuralBucket, true);
                const bool collapsed =
                    presentation == CadProxyPresentation::Point ||
                    presentation == CadProxyPresentation::Box;
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
        plan.subpixelProxyRevision =
            Obol::internal::cadTakeNonzeroIdentity(
                nextSubpixelProxyRevision_);
        structuralProjectionHistogram_.revision =
            Obol::internal::cadTakeNonzeroIdentity(
                nextStructuralProjectionRevision_);
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
    bool SoCADAssemblyImpl::extendSubpixelProxyAppendBuild(
            uint64_t viewId, const SbMatrix& viewProj,
            const SbVec2s& viewportSize,
            float pixelThreshold) {
        using namespace Obol::internal;
        CadFramePlan& plan = cachedPlan_;
        if (!subpixelProxyBuildActive_ ||
                subpixelProxyBuildViewId_ != viewId ||
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

Obol::CadPresentationPreparationTarget SoCADAssemblyImpl::subpixelPreparationTarget(
            uint64_t viewId, const SbMatrix& viewProj,
            const SbVec2s& viewportSize,
            float pixelThreshold) {
        Obol::CadPresentationPreparationTarget target;
        target.kind =
            Obol::CadPresentationPreparationKind::SubpixelClassification;
        target.viewId = viewId;
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
            Obol::internal::cadTakeNonzeroIdentity(
                nextPresentationPreparationRevision_);
        return target;
    }

uint64_t SoCADAssemblyImpl::subpixelPreparationReservedBytes() const {
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

uint64_t SoCADAssemblyImpl::subpixelPreparationCompletedUnits() const {
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

void SoCADAssemblyImpl::publishSubpixelPreparation(
            const Obol::CadPresentationPreparationTarget& target,
            Obol::CadPresentationPreparationState state,
            uint64_t completedUnits) {
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

bool SoCADAssemblyImpl::updateSubpixelProxyPlan(uint64_t viewId,
                                 const SbMatrix& viewProj,
                                 const SbVec2s& viewportSize,
                                 float pixelThreshold,
                                 bool cameraMotionReuse,
                                 SoGLRenderAction *renderAction,
                                 bool *preparationPerformed) {
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
        if (subpixelProxyViewId_ == viewId &&
                plan.subpixelProxyInputRevision !=
                subpixelProxyViewInputRevision_) {
            if (preparationPerformed)
                *preparationPerformed = true;
            if (patchSubpixelProxyAppendPlan(
                    viewProj, viewportSize, pixelThreshold)) {
                subpixelProxyBuildActive_ = false;
                subpixelProxyBuildTotalUnits_ = 1;
                const Obol::CadPresentationPreparationTarget target =
                    subpixelPreparationTarget(
                        viewId, viewProj, viewportSize, pixelThreshold);
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
                subpixelProxyViewId_ == viewId &&
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
                subpixelProxyViewId_ == viewId &&
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
            subpixelProxyBuildViewId_ == viewId &&
            subpixelProxyBuildInputRevision_ ==
                plan.subpixelProxyInputRevision &&
            subpixelProxyBuildViewportSize_[0] == viewportSize[0] &&
            subpixelProxyBuildViewportSize_[1] == viewportSize[1] &&
            subpixelProxyBuildPixelThreshold_ == pixelThreshold &&
            subpixelProxyBuildViewProj_ == viewProj;
        const bool extendedBuild = !matchingBuild &&
            subpixelProxyBuildActive_ &&
            extendSubpixelProxyAppendBuild(
                viewId, viewProj, viewportSize, pixelThreshold);
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
                static std::atomic<unsigned int> resetMessageCount{0};
                if (resetMessageCount.fetch_add(
                        1, std::memory_order_relaxed) < 256)
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
            subpixelProxyBuildViewId_ = viewId;
            subpixelProxyBuildViewProj_ = viewProj;
            subpixelProxyBuildViewportSize_ = viewportSize;
            subpixelProxyBuildPixelThreshold_ = pixelThreshold;
            subpixelProxyBuildActive_ = true;
        }
        const Obol::CadPresentationPreparationTarget preparationTarget =
            subpixelPreparationTarget(
                viewId, viewProj, viewportSize, pixelThreshold);
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
                    static std::atomic<unsigned int> abortMessageCount{0};
                    if (abortMessageCount.fetch_add(
                            1, std::memory_order_relaxed) < 256)
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
                    static_cast<CadProxyPresentation>(
                        subpixelProxyState_[visibleIndex]),
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
            Obol::internal::cadTakeNonzeroIdentity(
                nextStructuralProjectionRevision_);
        structuralProjectionHistogram_.exact = true;

        const bool changed = mask != plan.subpixelProxyMask ||
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
            plan.subpixelProxyRevision =
                Obol::internal::cadTakeNonzeroIdentity(
                    nextSubpixelProxyRevision_);
        }
        subpixelProxyViewProj_ = viewProj;
        subpixelProxyViewportSize_ = viewportSize;
        subpixelProxyPixelThreshold_ = pixelThreshold;
        subpixelProxyViewId_ = viewId;
        subpixelProxyViewInputRevision_ =
            plan.subpixelProxyInputRevision;
        subpixelProxyClassifiedAppendRevision_ =
            plan.appendRevision;
        classifiedPointProxyProtectionRevision_ =
            pointProxyProtectionRevision_;
        subpixelProxyViewValid_ = true;
        subpixelProxyBuildActive_ = false;
        publishSubpixelPreparation(
            preparationTarget,
            Obol::CadPresentationPreparationState::Complete,
            subpixelProxyBuildTotalUnits_);
        if (cadPlanDebugEnabled()) {
            static std::atomic<unsigned int> completeMessageCount{0};
            if (completeMessageCount.fetch_add(
                    1, std::memory_order_relaxed) < 256)
                std::fprintf(stderr,
                    "SoCADAssembly subpixel classifier complete "
                    "visible=%zu wire=%zu points=%zu threshold=%.9g\n",
                    plan.visibleInstances.size(), plan.wireItems.size(),
                    plan.subpixelProxyPoints.size(), pixelThreshold);
        }
        return true;
    }
