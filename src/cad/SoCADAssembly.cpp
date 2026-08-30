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
#include "CadAssemblyImpl.h"
#include "CadFramePlan.h"
#include "CadRendererGL.h"
#include "CadSceneMutationTestHooks.h"
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
#include <new>
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

namespace Obol {
namespace internal {

namespace {
thread_local unsigned int cadSceneMutationFailurePoint = 0;

void
cadCheckSceneMutationFailurePointForTesting(unsigned int point)
{
    if (cadSceneMutationFailurePoint != point)
        return;
    cadSceneMutationFailurePoint = 0;
    throw std::bad_alloc();
}
}

void
cadSetSceneMutationFailurePointForTesting(unsigned int point) noexcept
{
    cadSceneMutationFailurePoint = point;
}

} // namespace internal
} // namespace Obol

namespace {

using PartSet = std::unordered_set<Obol::PartId,
    std::hash<Obol::PartId>>;
using InstanceSet = std::unordered_set<Obol::InstanceId,
    std::hash<Obol::InstanceId>>;

/*
 * Sparse mutations retain the strong exception guarantee without cloning the
 * complete scene.  Capture only authoritative records which the requested
 * operations can change.  Renderer plans and picking caches are derived; a
 * rollback invalidates those instead of copying their potentially large
 * allocations.
 */
class CadSceneMutationSnapshot {
public:
    CadSceneMutationSnapshot(
        SoCADAssemblyImpl& impl,
        const Obol::CadSceneMutation& mutation) :
        impl_(impl), nextGeneration_(impl.nextGeneration_)
    {
        changedParts_.reserve(mutation.parts.size());
        changedInstances_.reserve(mutation.instances.size());
        changedStyles_.reserve(mutation.styles.size());
        changedCuts_.reserve(mutation.cuts.size());
        changedIndexParts_.reserve(mutation.instances.size() * 2u);

        for (const Obol::PartUpdate& update : mutation.parts)
            changedParts_.insert(update.part);
        for (const Obol::InstanceUpdate& update : mutation.instances) {
            changedInstances_.insert(update.instance);
            const auto preceding = impl_.instances_.find(update.instance);
            if (preceding == impl_.instances_.end()) {
                changedIndexParts_.insert(update.record.part);
            } else if (!(preceding->second.partId == update.record.part)) {
                changedIndexParts_.insert(preceding->second.partId);
                changedIndexParts_.insert(update.record.part);
            }
        }
        for (const Obol::InstanceStyleUpdate& update : mutation.styles)
            changedStyles_.insert(update.instance);
        for (const Obol::InstanceLodUpdate& update : mutation.cuts)
            changedCuts_.insert(update.instance);

        capturePartState();
        captureInstanceState();
        capturePartIndexState();
    }

    CadSceneMutationSnapshot(const CadSceneMutationSnapshot&) = delete;
    CadSceneMutationSnapshot& operator=(
        const CadSceneMutationSnapshot&) = delete;

    void rollback() noexcept
    {
        restorePartIndexes();
        restoreInstances();
        restoreParts();
        impl_.nextGeneration_ = nextGeneration_;

        /* A failed plan patch may have changed any derived presentation
         * table.  Discard it and rebuild from the restored database. */
        impl_.markDirty("scene-mutation-rollback");
        impl_.subpixelProxyBuildActive_ = false;
        impl_.subpixelProxyViewValid_ = false;
    }

private:
    using PartMap = decltype(SoCADAssemblyImpl::parts_);
    using InstanceMap = decltype(SoCADAssemblyImpl::instances_);
    using PartBucketMap = decltype(SoCADAssemblyImpl::instanceIdsByPart_);
    using SlotMap = decltype(SoCADAssemblyImpl::instancePartSlot_);
    using ProxyCornerMap = decltype(
        SoCADAssemblyImpl::subpixelProxyCorners_);
    using GenerationMap = decltype(SoCADAssemblyImpl::partGeneration_);
    using StyleMap = std::unordered_map<Obol::InstanceId,
        Obol::InstanceStyle, std::hash<Obol::InstanceId>>;
    struct CutState {
        uint8_t cut = Obol::ProgressiveCutUnspecified;
        bool structuralProxy = false;
    };
    using CutMap = std::unordered_map<Obol::InstanceId, CutState,
        std::hash<Obol::InstanceId>>;
    using BoundsMap = std::unordered_map<Obol::InstanceId, SbBox3f,
        std::hash<Obol::InstanceId>>;

    static void restoreInstanceData(
        InstanceData& destination, InstanceData& source) noexcept
    {
        destination.partId = source.partId;
        destination.localToRoot = source.localToRoot;
        destination.style = source.style;
        destination.parent = source.parent;
        destination.childName.swap(source.childName);
        destination.occurrenceIndex = source.occurrenceIndex;
        destination.boolOp = source.boolOp;
        destination.lodCut = source.lodCut;
        destination.lodStructuralProxy = source.lodStructuralProxy;
        destination.worldBounds = source.worldBounds;
    }

    void capturePartState()
    {
        precedingParts_.reserve(changedParts_.size());
        precedingProxyCorners_.reserve(changedParts_.size());
        precedingGenerations_.reserve(changedParts_.size());
        precedingProgressiveParts_.reserve(changedParts_.size());
        precedingWorldBounds_.reserve(changedParts_.size());

        for (const Obol::PartId part : changedParts_) {
            const auto geometry = impl_.parts_.find(part);
            if (geometry != impl_.parts_.end())
                precedingParts_.emplace(part, geometry->second);
            const auto corners = impl_.subpixelProxyCorners_.find(part);
            if (corners != impl_.subpixelProxyCorners_.end())
                precedingProxyCorners_.emplace(part, corners->second);
            const auto generation = impl_.partGeneration_.find(part);
            if (generation != impl_.partGeneration_.end())
                precedingGenerations_.emplace(part, generation->second);
            if (impl_.progressiveParts_.count(part))
                precedingProgressiveParts_.insert(part);

            const auto bucket = impl_.instanceIdsByPart_.find(part);
            if (bucket == impl_.instanceIdsByPart_.end())
                continue;
            for (size_t slot = 0; slot < bucket->second.size(); ++slot) {
                const Obol::InstanceId instance = bucket->second.at(slot);
                const auto live = impl_.instances_.find(instance);
                if (live != impl_.instances_.end())
                    precedingWorldBounds_.emplace(
                        instance, live->second.worldBounds);
            }
        }
    }

    void captureInstanceState()
    {
        precedingInstances_.reserve(changedInstances_.size());
        precedingStyles_.reserve(changedStyles_.size());
        precedingCuts_.reserve(changedCuts_.size());
        for (const Obol::InstanceId instance : changedInstances_) {
            const auto live = impl_.instances_.find(instance);
            if (live != impl_.instances_.end())
                precedingInstances_.emplace(instance, live->second);
        }
        for (const Obol::InstanceId instance : changedStyles_) {
            const auto live = impl_.instances_.find(instance);
            if (live != impl_.instances_.end())
                precedingStyles_.emplace(instance, live->second.style);
        }
        for (const Obol::InstanceId instance : changedCuts_) {
            const auto live = impl_.instances_.find(instance);
            if (live != impl_.instances_.end())
                precedingCuts_.emplace(instance, CutState{
                    live->second.lodCut,
                    live->second.lodStructuralProxy});
        }
    }

    void capturePartIndexState()
    {
        changedSlotInstances_.reserve(changedInstances_.size());
        precedingSlots_.reserve(changedInstances_.size());
        for (const Obol::InstanceId instance : changedInstances_)
            changedSlotInstances_.insert(instance);

        for (const Obol::PartId part : changedIndexParts_) {
            const auto bucket = impl_.instanceIdsByPart_.find(part);
            if (bucket == impl_.instanceIdsByPart_.end())
                continue;
            precedingBuckets_.emplace(part, bucket->second);
            for (size_t slot = 0; slot < bucket->second.size(); ++slot)
                changedSlotInstances_.insert(bucket->second.at(slot));
        }
        precedingSlots_.reserve(changedSlotInstances_.size());
        for (const Obol::InstanceId instance : changedSlotInstances_) {
            const auto slot = impl_.instancePartSlot_.find(instance);
            if (slot != impl_.instancePartSlot_.end())
                precedingSlots_.emplace(instance, slot->second);
        }
    }

    void restorePartIndexes() noexcept
    {
        for (const Obol::PartId part : changedIndexParts_)
            impl_.instanceIdsByPart_.erase(part);
        while (!precedingBuckets_.empty()) {
            auto node = precedingBuckets_.extract(
                precedingBuckets_.begin());
            impl_.instanceIdsByPart_.insert(std::move(node));
        }

        for (const Obol::InstanceId instance : changedSlotInstances_)
            impl_.instancePartSlot_.erase(instance);
        while (!precedingSlots_.empty()) {
            auto node = precedingSlots_.extract(precedingSlots_.begin());
            impl_.instancePartSlot_.insert(std::move(node));
        }
    }

    void restoreInstances() noexcept
    {
        for (const Obol::InstanceId instance : changedInstances_) {
            auto preceding = precedingInstances_.find(instance);
            if (preceding == precedingInstances_.end()) {
                impl_.instances_.erase(instance);
                continue;
            }
            const auto live = impl_.instances_.find(instance);
            if (live != impl_.instances_.end())
                restoreInstanceData(live->second, preceding->second);
        }
        for (const auto& entry : precedingWorldBounds_) {
            const auto live = impl_.instances_.find(entry.first);
            if (live != impl_.instances_.end())
                live->second.worldBounds = entry.second;
        }
        for (const auto& entry : precedingStyles_) {
            const auto live = impl_.instances_.find(entry.first);
            if (live != impl_.instances_.end())
                live->second.style = entry.second;
        }
        for (const auto& entry : precedingCuts_) {
            const auto live = impl_.instances_.find(entry.first);
            if (live != impl_.instances_.end()) {
                live->second.lodCut = entry.second.cut;
                live->second.lodStructuralProxy =
                    entry.second.structuralProxy;
            }
        }
    }

    void restoreParts() noexcept
    {
        for (const Obol::PartId part : changedParts_) {
            const auto preceding = precedingParts_.find(part);
            if (preceding == precedingParts_.end()) {
                impl_.parts_.erase(part);
            } else {
                const auto live = impl_.parts_.find(part);
                if (live != impl_.parts_.end())
                    live->second = preceding->second;
            }
        }

        for (const Obol::PartId part : changedParts_) {
            impl_.subpixelProxyCorners_.erase(part);
            impl_.partGeneration_.erase(part);
            impl_.progressiveParts_.erase(part);
        }
        while (!precedingProxyCorners_.empty()) {
            auto node = precedingProxyCorners_.extract(
                precedingProxyCorners_.begin());
            impl_.subpixelProxyCorners_.insert(std::move(node));
        }
        while (!precedingGenerations_.empty()) {
            auto node = precedingGenerations_.extract(
                precedingGenerations_.begin());
            impl_.partGeneration_.insert(std::move(node));
        }
        while (!precedingProgressiveParts_.empty()) {
            auto node = precedingProgressiveParts_.extract(
                precedingProgressiveParts_.begin());
            impl_.progressiveParts_.insert(std::move(node));
        }
    }

    SoCADAssemblyImpl& impl_;
    uint64_t nextGeneration_;
    PartSet changedParts_;
    InstanceSet changedInstances_;
    InstanceSet changedStyles_;
    InstanceSet changedCuts_;
    PartSet changedIndexParts_;
    InstanceSet changedSlotInstances_;
    PartMap precedingParts_;
    InstanceMap precedingInstances_;
    PartBucketMap precedingBuckets_;
    SlotMap precedingSlots_;
    ProxyCornerMap precedingProxyCorners_;
    GenerationMap precedingGenerations_;
    PartSet precedingProgressiveParts_;
    BoundsMap precedingWorldBounds_;
    StyleMap precedingStyles_;
    CutMap precedingCuts_;
};

Obol::CadSceneMutationResult
cadSceneMutationResourceUnavailable() noexcept
{
    Obol::CadSceneMutationResult result;
    result.domain = Obol::CadSceneMutationDomain::ResourceUnavailable;
    return result;
}

} // namespace


// ---------------------------------------------------------------------------
// SoCADAssembly
// ---------------------------------------------------------------------------

SO_NODE_SOURCE(SoCADAssembly);

void
SoCADAssembly::initClass()
{
    if (SoCADAssembly::getClassTypeId() != SoType::badType())
        return;
    SO_NODE_INIT_CLASS(SoCADAssembly, SoNode, "Node");
    SoCADDetail::initClass();
    SoCADViewState::initClass();
}

SoCADAssembly::SoCADAssembly()
    : impl_(new SoCADAssemblyImpl)
{
    SO_NODE_CONSTRUCTOR(SoCADAssembly);
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

SoCADAssembly::UpdateScope::UpdateScope(SoCADAssembly *assembly) noexcept :
    assembly_(assembly)
{
}

SoCADAssembly::UpdateScope::UpdateScope(UpdateScope&& other) noexcept :
    assembly_(other.assembly_)
{
    other.assembly_ = nullptr;
}

SoCADAssembly::UpdateScope&
SoCADAssembly::UpdateScope::operator=(UpdateScope&& other) noexcept
{
    if (this == &other)
        return *this;
    finish();
    assembly_ = other.assembly_;
    other.assembly_ = nullptr;
    return *this;
}

SoCADAssembly::UpdateScope::~UpdateScope()
{
    finish();
}

void
SoCADAssembly::UpdateScope::finish() noexcept
{
    if (!assembly_)
        return;
    assembly_->endUpdate();
    assembly_ = nullptr;
}

SoCADAssembly::UpdateScope
SoCADAssembly::batchUpdate()
{
    beginUpdate();
    return UpdateScope(this);
}

void
SoCADAssembly::beginUpdate()
{
    ++impl_->updateDepth_;
}

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
    if (!impl_->updateDepth_)
        return;
    --impl_->updateDepth_;
    if (impl_->updateDepth_)
        return;
    /* Public mutations record the exact caches they invalidate even while
     * notifications are batched.  Do not turn a selection/style-only batch
     * into a geometry and BVH rebuild merely because it was framed by
     * batchUpdate(). */
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
    if (!impl_->updateDepth_)
        touch();
}

Obol::CadSceneReplacementResult
SoCADAssembly::replaceScene(
    const std::vector<Obol::PartUpdate>& parts,
    const std::vector<Obol::InstanceUpdate>& instances)
{
    Obol::CadSceneReplacementResult result;
    result.geometry = Obol::cadValidatePartUpdates(parts);
    if (!result.geometry) {
        result.error = Obol::CadSceneReplacementError::Geometry;
        return result;
    }
    result.instances = Obol::cadValidateInstanceUpdates(instances);
    if (!result.instances) {
        result.error = Obol::CadSceneReplacementError::Instances;
        return result;
    }

    /*
     * Construct the complete retained database before changing the live
     * scene.  Geometry remains shared, so this duplicates only lightweight
     * part/occurrence records.  Besides preserving the scene on validation
     * rejection, this gives replacement the strong exception guarantee: an
     * allocation failure while staging cannot expose a cleared or partially
     * populated assembly.
     */
    std::unique_ptr<SoCADAssemblyImpl> replacement;
    try {
        replacement = std::make_unique<SoCADAssemblyImpl>();
        replacement->nextGeneration_ = impl_->nextGeneration_;
        const size_t occurrenceCapacity = std::max(
            instances.size(), impl_->streamingOccurrenceCapacityHint_);
        replacement->parts_.reserve(parts.size());
        replacement->subpixelProxyCorners_.reserve(parts.size());
        replacement->partGeneration_.reserve(parts.size());
        replacement->progressiveParts_.reserve(parts.size());
        replacement->instances_.reserve(occurrenceCapacity);
        replacement->instancePartSlot_.reserve(occurrenceCapacity);

        for (const Obol::PartUpdate& part : parts)
            replacement->updatePartGeometry(
                part.part, part.geometry.shared());
        for (const Obol::InstanceUpdate& instance : instances)
            replacement->updateInstance(instance.instance, instance.record);
    } catch (const std::bad_alloc&) {
        result.error =
            Obol::CadSceneReplacementError::ResourceUnavailable;
        return result;
    }

    using SceneDatabase = Obol::internal::CadSceneDatabase;
    using ProxyCornerMap = decltype(impl_->subpixelProxyCorners_);
    static_assert(std::is_nothrow_swappable_v<SceneDatabase>,
        "retained scene publication must not allocate");
    static_assert(std::is_nothrow_swappable_v<ProxyCornerMap>,
        "proxy-bound publication must not allocate");

    auto update = batchUpdate();
    clear();
    using std::swap;
    swap(static_cast<SceneDatabase&>(*impl_),
         static_cast<SceneDatabase&>(*replacement));
    swap(impl_->subpixelProxyCorners_,
         replacement->subpixelProxyCorners_);
    impl_->planDirtyReason_ = "scene-replace";
    return result;
}

Obol::CadSceneMutationResult
SoCADAssembly::validateSceneMutation(
    const Obol::CadSceneMutation& mutation) const
{
    Obol::CadSceneMutationResult result;
    result.geometry = Obol::cadValidatePartUpdates(mutation.parts);
    if (!result.geometry) {
        result.domain = Obol::CadSceneMutationDomain::Parts;
        return result;
    }
    result.scene = Obol::cadValidateInstanceUpdates(mutation.instances);
    if (!result.scene) {
        result.domain = Obol::CadSceneMutationDomain::Instances;
        return result;
    }
    result.scene = Obol::cadValidateInstanceStyleUpdates(mutation.styles);
    if (!result.scene) {
        result.domain = Obol::CadSceneMutationDomain::Styles;
        return result;
    }
    result.scene = Obol::cadValidateInstanceLodUpdates(mutation.cuts);
    if (!result.scene) {
        result.domain = Obol::CadSceneMutationDomain::Cuts;
        return result;
    }

    const auto sceneFailure = [&result](
            Obol::CadSceneMutationDomain domain,
            Obol::CadSceneError sceneError, size_t index) {
        result.domain = domain;
        result.scene.error = sceneError;
        result.scene.updateIndex = index;
        return result;
    };

    std::unordered_set<Obol::PartId, std::hash<Obol::PartId>> updatedParts;
    std::unordered_set<Obol::PartId, std::hash<Obol::PartId>> removedParts;
    updatedParts.reserve(mutation.parts.size());
    removedParts.reserve(mutation.removedParts.size());
    for (size_t i = 0; i < mutation.parts.size(); ++i) {
        if (!updatedParts.insert(mutation.parts[i].part).second)
            return sceneFailure(Obol::CadSceneMutationDomain::Parts,
                Obol::CadSceneError::ConflictingUpdate, i);
    }
    for (size_t i = 0; i < mutation.removedParts.size(); ++i) {
        const Obol::PartId part = mutation.removedParts[i];
        if (!part.isValid())
            return sceneFailure(Obol::CadSceneMutationDomain::RemovedParts,
                Obol::CadSceneError::InvalidPartId, i);
        if (!removedParts.insert(part).second || updatedParts.count(part))
            return sceneFailure(Obol::CadSceneMutationDomain::RemovedParts,
                Obol::CadSceneError::ConflictingUpdate, i);
    }

    std::unordered_set<Obol::InstanceId, std::hash<Obol::InstanceId>>
        updatedInstances;
    std::unordered_set<Obol::InstanceId, std::hash<Obol::InstanceId>>
        removedInstances;
    updatedInstances.reserve(mutation.instances.size());
    removedInstances.reserve(mutation.removedInstances.size());
    for (size_t i = 0; i < mutation.instances.size(); ++i) {
        if (!updatedInstances.insert(mutation.instances[i].instance).second)
            return sceneFailure(Obol::CadSceneMutationDomain::Instances,
                Obol::CadSceneError::ConflictingUpdate, i);
    }
    for (size_t i = 0; i < mutation.removedInstances.size(); ++i) {
        const Obol::InstanceId instance = mutation.removedInstances[i];
        if (!instance.isValid())
            return sceneFailure(
                Obol::CadSceneMutationDomain::RemovedInstances,
                Obol::CadSceneError::InvalidInstanceId, i);
        if (!removedInstances.insert(instance).second ||
                updatedInstances.count(instance))
            return sceneFailure(Obol::CadSceneMutationDomain::RemovedInstances,
                Obol::CadSceneError::ConflictingUpdate, i);
    }

    const auto instanceExists = [this, &updatedInstances,
            &removedInstances](Obol::InstanceId instance) {
        if (removedInstances.count(instance))
            return false;
        return updatedInstances.count(instance) ||
            impl_->instances_.find(instance) != impl_->instances_.end();
    };
    std::unordered_set<Obol::InstanceId, std::hash<Obol::InstanceId>>
        styledInstances;
    styledInstances.reserve(mutation.styles.size());
    for (size_t i = 0; i < mutation.styles.size(); ++i) {
        const Obol::InstanceId instance = mutation.styles[i].instance;
        if (!styledInstances.insert(instance).second)
            return sceneFailure(Obol::CadSceneMutationDomain::Styles,
                Obol::CadSceneError::ConflictingUpdate, i);
        if (!instanceExists(instance))
            return sceneFailure(Obol::CadSceneMutationDomain::Styles,
                Obol::CadSceneError::MissingInstance, i);
    }
    std::unordered_set<Obol::InstanceId, std::hash<Obol::InstanceId>>
        cutInstances;
    cutInstances.reserve(mutation.cuts.size());
    for (size_t i = 0; i < mutation.cuts.size(); ++i) {
        const Obol::InstanceId instance = mutation.cuts[i].instance;
        if (!cutInstances.insert(instance).second)
            return sceneFailure(Obol::CadSceneMutationDomain::Cuts,
                Obol::CadSceneError::ConflictingUpdate, i);
        if (!instanceExists(instance))
            return sceneFailure(Obol::CadSceneMutationDomain::Cuts,
                Obol::CadSceneError::MissingInstance, i);
    }
    return result;
}

Obol::CadSceneMutationResult
SoCADAssembly::applySceneMutation(const Obol::CadSceneMutation& mutation)
{
    Obol::CadSceneMutationResult result;
    try {
        result = validateSceneMutation(mutation);
    } catch (const std::bad_alloc&) {
        return cadSceneMutationResourceUnavailable();
    }
    if (!result || mutation.empty())
        return result;

    std::unique_ptr<CadSceneMutationSnapshot> snapshot;
    try {
        snapshot = std::make_unique<CadSceneMutationSnapshot>(
            *impl_, mutation);
    } catch (const std::bad_alloc&) {
        return cadSceneMutationResourceUnavailable();
    }

    beginUpdate();
    try {
        /* Validation above covers every semantic rejection path.  These
         * calls can still allocate while updating sparse indexes or patching
         * retained plans, so retain rollback state until all have finished. */
        (void)upsertParts(mutation.parts);
        Obol::internal::cadCheckSceneMutationFailurePointForTesting(1);
        (void)upsertInstances(mutation.instances);
        Obol::internal::cadCheckSceneMutationFailurePointForTesting(2);
        (void)updateInstanceStyles(mutation.styles);
        Obol::internal::cadCheckSceneMutationFailurePointForTesting(3);
        (void)updateInstanceCuts(mutation.cuts);
        Obol::internal::cadCheckSceneMutationFailurePointForTesting(4);
    } catch (const std::bad_alloc&) {
        snapshot->rollback();
        --impl_->updateDepth_;
        return cadSceneMutationResourceUnavailable();
    } catch (...) {
        snapshot->rollback();
        --impl_->updateDepth_;
        throw;
    }

    /* Erasure and retained-record destruction are non-allocating.  Delay
     * them until the allocation-capable portion has committed so rollback
     * never needs to reconstruct an erased node. */
    for (const Obol::InstanceId instance : mutation.removedInstances)
        removeInstance(instance);
    for (const Obol::PartId part : mutation.removedParts)
        removePart(part);
    endUpdate();
    return result;
}

// --- Part library ----------------------------------------------------------

Obol::CadGeometryValidation
SoCADAssembly::upsertParts(
    const std::vector<Obol::PartUpdate>& updates)
{
    if (updates.empty())
        return Obol::CadGeometryValidation();
    const Obol::CadGeometryValidation validation =
        Obol::cadValidatePartUpdates(updates);
    if (!validation)
        return validation;
    std::unordered_set<Obol::PartId, std::hash<Obol::PartId>> changedParts;
    std::unordered_set<Obol::PartId, std::hash<Obol::PartId>>
        boundsPreservingParts;
    bool topologyDirty = false;
    changedParts.reserve(updates.size());
    boundsPreservingParts.reserve(updates.size());
    for (const auto& update : updates) {
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
                preceding->second == update.geometry.shared())
            continue;
        const bool preservesBounds =
            update.preservesBounds &&
            preceding != impl_->parts_.end() &&
            preceding->second &&
            SoCADAssemblyImpl::partGeometryBoundsEqual(
                *preceding->second, *update.geometry.get());
        const bool firstUpdate =
            changedParts.insert(update.part).second;
        if (firstUpdate) {
            if (preservesBounds)
                boundsPreservingParts.insert(update.part);
        } else if (!preservesBounds) {
            boundsPreservingParts.erase(update.part);
        }
        impl_->updatePartGeometry(update.part, update.geometry.shared());
    }
    if (changedParts.empty())
        return Obol::CadGeometryValidation();
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
    if (!impl_->updateDepth_) touch();
    return Obol::CadGeometryValidation();
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
    if (!impl_->updateDepth_) touch();
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
    if (!impl_->updateDepth_) touch();
}

// --- Instance management ---------------------------------------------------

Obol::CadInstanceUpdateResult
SoCADAssembly::upsertInstanceAuto(const Obol::InstanceRecord& rec)
{
    Obol::CadInstanceUpdateResult result;
    result.instance = Obol::CadIdBuilder::childInstance(
        rec.parent, rec.childName, rec.occurrenceIndex, rec.boolOp);
    result.validation = upsertInstance(result.instance, rec);
    if (!result.validation)
        result.instance = Obol::InstanceId();
    return result;
}

Obol::CadSceneValidation
SoCADAssembly::upsertInstance(Obol::InstanceId iid, const Obol::InstanceRecord& rec)
{
    const Obol::CadSceneValidation validation =
        Obol::cadValidateInstanceRecord(iid, rec);
    if (!validation)
        return validation;
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
    if (!impl_->updateDepth_) touch();
    return validation;
}

Obol::CadInstanceBatchResult
SoCADAssembly::upsertInstancesAuto(
    const std::vector<Obol::InstanceRecord>& records)
{
    Obol::CadInstanceBatchResult result;
    result.instances.reserve(records.size());
    if (records.empty())
        return result;

    for (size_t i = 0; i < records.size(); ++i) {
        const Obol::InstanceRecord& rec = records[i];
        const Obol::InstanceId iid = Obol::CadIdBuilder::childInstance(
            rec.parent, rec.childName, rec.occurrenceIndex, rec.boolOp);
        result.validation = Obol::cadValidateInstanceRecord(iid, rec);
        if (!result.validation) {
            result.validation.updateIndex = i;
            result.instances.clear();
            return result;
        }
        result.instances.push_back(iid);
    }
    for (size_t i = 0; i < records.size(); ++i)
        impl_->updateInstance(result.instances[i], records[i]);
    if (!impl_->appendCachedInstances(result.instances))
        impl_->markDirty("instance-auto-batch-unpatchable");
    else
        impl_->finishSparseStructuralPatch();
    if (!impl_->updateDepth_)
        touch();
    return result;
}

Obol::CadSceneValidation
SoCADAssembly::upsertInstances(
    const std::vector<Obol::InstanceUpdate>& updates)
{
    if (updates.empty())
        return Obol::CadSceneValidation();

    const Obol::CadSceneValidation validation =
        Obol::cadValidateInstanceUpdates(updates);
    if (!validation)
        return validation;

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
        return Obol::CadSceneValidation();
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
    if (!impl_->updateDepth_)
        touch();
    return Obol::CadSceneValidation();
}

Obol::CadSceneValidation
SoCADAssembly::updateInstanceCuts(
    const std::vector<Obol::InstanceLodUpdate>& updates)
{
    Obol::CadSceneValidation validation =
        Obol::cadValidateInstanceLodUpdates(updates);
    if (!validation)
        return validation;
    for (size_t i = 0; i < updates.size(); ++i) {
        if (impl_->instances_.find(updates[i].instance) ==
                impl_->instances_.end()) {
            validation.error = Obol::CadSceneError::MissingInstance;
            validation.updateIndex = i;
            return validation;
        }
    }
    bool changed = false;
    bool sparsePlanPatch = !impl_->planDirty_ && !impl_->geometryDirty_ &&
        impl_->cachedDrawModeHasShaded();
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
    if (!changed)
        return Obol::CadSceneValidation();
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
    if (!impl_->updateDepth_) touch();
    return Obol::CadSceneValidation();
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
    if (!impl_->updateDepth_) touch();
}

Obol::CadSceneValidation
SoCADAssembly::updateInstanceTransform(Obol::InstanceId iid, const SbMatrix& m)
{
    Obol::CadSceneValidation validation =
        Obol::cadValidateInstanceTransform(iid, m);
    if (!validation)
        return validation;
    auto it = impl_->instances_.find(iid);
    if (it == impl_->instances_.end()) {
        validation.error = Obol::CadSceneError::MissingInstance;
        return validation;
    }
    it->second.localToRoot = m;
    auto geomIt = impl_->parts_.find(it->second.partId);
    if (geomIt != impl_->parts_.end() && geomIt->second) {
        it->second.worldBounds = impl_->computeWorldBounds(*geomIt->second, m);
    }
    impl_->bvhDirty_  = true;
    impl_->planDirty_ = true;
    impl_->geometryDirty_ = true;
    impl_->planDirtyReason_ = "instance-transform";
    if (!impl_->updateDepth_) touch();
    return validation;
}

Obol::CadSceneValidation
SoCADAssembly::updateInstanceStyle(Obol::InstanceId iid, const Obol::InstanceStyle& style)
{
    Obol::CadSceneValidation validation =
        Obol::cadValidateInstanceStyle(iid, style);
    if (!validation)
        return validation;
    auto it = impl_->instances_.find(iid);
    if (it == impl_->instances_.end()) {
        validation.error = Obol::CadSceneError::MissingInstance;
        return validation;
    }
    it->second.style = style;
    if (impl_->patchCachedInstanceStyle(iid))
        impl_->finishSparsePresentationPatch();
    else {
        impl_->planDirty_ = true;
        impl_->planDirtyReason_ = "instance-style";
        impl_->pendingInstanceAttributeIndices_.clear();
    }
    if (!impl_->updateDepth_) touch();
    return validation;
}

Obol::CadSceneValidation
SoCADAssembly::updateInstanceStyles(
    const std::vector<Obol::InstanceStyleUpdate>& updates)
{
    if (updates.empty())
        return Obol::CadSceneValidation();
    Obol::CadSceneValidation validation =
        Obol::cadValidateInstanceStyleUpdates(updates);
    if (!validation)
        return validation;
    for (size_t i = 0; i < updates.size(); ++i) {
        validation = Obol::CadSceneValidation();
        if (validation && impl_->instances_.find(updates[i].instance) ==
                impl_->instances_.end())
            validation.error = Obol::CadSceneError::MissingInstance;
        if (!validation) {
            validation.updateIndex = i;
            return validation;
        }
    }
    bool changed = false;
    bool sparsePlanPatch = !impl_->planDirty_ && !impl_->geometryDirty_ &&
        impl_->cachedDrawModeHasShaded();
    for (const auto& update : updates) {
        auto it = impl_->instances_.find(update.instance);
        if (it == impl_->instances_.end()) continue;
        it->second.style = update.style;
        if (sparsePlanPatch &&
                !impl_->patchCachedInstanceStyle(update.instance))
            sparsePlanPatch = false;
        changed = true;
    }
    if (!changed)
        return Obol::CadSceneValidation();
    if (sparsePlanPatch)
        impl_->finishSparsePresentationPatch();
    else {
        impl_->planDirty_ = true;
        impl_->planDirtyReason_ = "instance-styles";
        impl_->pendingInstanceAttributeIndices_.clear();
    }
    if (!impl_->updateDepth_) touch();
    return Obol::CadSceneValidation();
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
    if (!impl_->updateDepth_) touch();
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
    if (!impl_->updateDepth_) touch();
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
    if (!impl_->updateDepth_) touch();
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
    if (!impl_->updateDepth_) touch();
}

void
SoCADAssembly::setUnpickableInstances(const std::vector<Obol::InstanceId>& ids)
{
    impl_->unpickable_.clear();
    impl_->unpickable_.insert(ids.begin(), ids.end());
    impl_->bvhDirty_ = true;
    if (!impl_->updateDepth_) touch();
}

// ---------------------------------------------------------------------------
// GLRender
// ---------------------------------------------------------------------------

void
SoCADAssembly::GLRender(SoGLRenderAction* action)
{
    std::lock_guard<std::mutex> renderLock(impl_->renderMutex_);
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

    const Obol::CadViewState viewState =
        SoCADViewStateElement::get(state);
    const Obol::CadDrawMode drawMode = viewState.drawMode;

    // Rebuild the frame plan only when geometry, instances, styles, selection,
    // hidden set, or draw mode have changed.  Camera moves do NOT invalidate
    // the plan, so it is reused every frame during interactive orbit.
    if (impl_->planDirty_ || impl_->cachedDrawMode_ != drawMode) {
        const bool geometryChanged = impl_->geometryDirty_ ||
                                     impl_->cachedDrawMode_ != drawMode;
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
                impl_->cachedDrawMode_ ?
                    static_cast<int>(*impl_->cachedDrawMode_) : -1,
                static_cast<int>(drawMode), impl_->instances_.size(),
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
            impl_->buildFramePlan(
                drawMode, impl_->selected_, impl_->hidden_);
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
        impl_->cachedDrawMode_ = drawMode;
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
        impl_->updateSubpixelProxyPlan(viewState.viewId, viewProj,
            viewport.getViewportSizePixels(),
            viewState.pointProxyPixelThreshold,
            viewState.cameraMotionFrameReuse, action,
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
    if (drawMode == Obol::CadDrawMode::Wireframe &&
            viewState.softwareWireMode == Obol::CadSoftwareWireMode::FAST) {
        const std::vector<Obol::internal::CadSubpixelProxyPoint>&
            presentationPoints =
                impl_->renderer_->subpixelProxyPresentationPoints(
                    impl_->cachedPlan_, glue, viewProj);
        softwareWireResult = cadRenderSoftwareWire(
            impl_->cachedPlan_, *this, viewState, state, viewProj,
            presentationPoints);
    }
    const bool softwareWire = softwareWireResult.rendered;
    impl_->lastDirectSoftwareWire_ = softwareWire;
    if (softwareWire) {
        softwareWireResult.work.viewState = viewState;
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
        impl_->renderer_->render(impl_->cachedPlan_, *this, viewState,
                                 action, glue, viewProj,
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
                     static_cast<int>(viewState.softwareWireMode),
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

    const Obol::CadViewState viewState =
        SoCADViewStateElement::get(action->getState());
    Obol::CadPickMode pickPolicy = viewState.pickMode;
    const bool automaticPick = pickPolicy == Obol::CadPickMode::Automatic;
    if (automaticPick) {
        pickPolicy = viewState.drawMode == Obol::CadDrawMode::Wireframe ?
            Obol::CadPickMode::Edge : Obol::CadPickMode::Triangle;
    }

    // Derive an assembly-space edge-pick tolerance from the screen-space field.
    // Approximate: use the view volume to find how large one pixel is in world
    // coordinates at the assembly centre, then scale by the user-specified tolerance.
    float toleranceWS = viewState.edgePickTolerancePixels * 0.01f;
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
                    viewState.edgePickTolerancePixels * pixelH);
            }
        }
    }

    Obol::picking::CadPickResult result;
    const int configuredCutCeiling = viewState.progressiveCutCeiling;
    const uint8_t pickCutCeiling =
        configuredCutCeiling >= 0 &&
            configuredCutCeiling <
                static_cast<int>(Obol::ProgressiveCutLimit) ?
        static_cast<uint8_t>(configuredCutCeiling) :
        Obol::ProgressiveCutUnspecified;

    if (automaticPick || pickPolicy == Obol::CadPickMode::Edge ||
            pickPolicy == Obol::CadPickMode::Hybrid) {
        result = Obol::picking::CadPickQuery::pickPoint(
            pickRay, impl_->instanceBvh_, impl_->parts_, toleranceWS);
    }

    if (!result.valid && (pickPolicy == Obol::CadPickMode::Edge ||
            pickPolicy == Obol::CadPickMode::Hybrid)) {
        result = Obol::picking::CadPickQuery::pickEdge(
            pickRay,
            impl_->instanceBvh_,
            impl_->parts_,
            impl_->partEdgeBvhCache_,
            toleranceWS,
            pickCutCeiling);
    }

    if (!result.valid && (pickPolicy == Obol::CadPickMode::Triangle ||
            pickPolicy == Obol::CadPickMode::Hybrid)) {
        result = Obol::picking::CadPickQuery::pickTriangle(
            pickRay,
            impl_->instanceBvh_,
            impl_->parts_,
            impl_->partTriBvhCache_,
            toleranceWS,
            pickCutCeiling);
    }

    if (!result.valid && pickPolicy == Obol::CadPickMode::Bounds) {
        result = Obol::picking::CadPickQuery::pickBounds(
            pickRay,
            impl_->instanceBvh_,
            toleranceWS);
    }

    // For PICK_HYBRID: also try bounds if triangle picking returned nothing.
    if (!result.valid && pickPolicy == Obol::CadPickMode::Hybrid) {
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

Obol::CadAggregateProxyPresentationWork
SoCADAssembly::lastAggregateProxyPresentationWork() const
{
    Obol::CadAggregateProxyPresentationWork work;
    work.exact = impl_->renderer_ &&
        impl_->renderer_->lastRenderedWork().exact;

    for (const Obol::internal::CadSubpixelProxyPoint& proxy :
            impl_->cachedPlan_.subpixelProxyPoints)
        Obol::internal::cadAccumulateVisibleAggregateProxy(proxy, work);

    if (!impl_->renderer_)
        return work;
    const Obol::CadAggregateProxyPresentationWork pressure =
        impl_->renderer_->lastPressureProxyPresentationWork();
    const auto add = [](uint64_t left, uint64_t right) {
        return right > UINT64_MAX - left ? UINT64_MAX : left + right;
    };
    work.pointCount = add(work.pointCount, pressure.pointCount);
    work.axisAlignedBoxCount = add(work.axisAlignedBoxCount,
        pressure.axisAlignedBoxCount);
    work.orientedBoxCount = add(work.orientedBoxCount,
        pressure.orientedBoxCount);
    work.exact = work.exact && pressure.exact;
    return work;
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

std::vector<Obol::InstanceId>
SoCADAssembly::lastStructuralProxyInstancesAbovePixels(float pixels) const
{
    std::vector<Obol::InstanceId> instances;
    const Obol::CadStructuralProxyProjectionHistogram &histogram =
        impl_->structuralProjectionHistogram_;
    if (!histogram.exact ||
            impl_->structuralProjectionBucketByVisible_.size() !=
                impl_->cachedPlan_.visibleInstances.size())
        return instances;

    pixels = std::isfinite(pixels) ?
        std::max(1.0f, std::min(64.0f, pixels)) : 1.0f;
    size_t safeBucket = 0;
    float upperBound = 1.0f;
    while (safeBucket + 1 <
            Obol::CadStructuralProxyProjectionHistogram::BucketCount &&
            upperBound * 2.0f <= pixels) {
        upperBound *= 2.0f;
        ++safeBucket;
    }

    instances.reserve(histogram.visibleCount);
    for (size_t visibleIndex = 0;
            visibleIndex < impl_->cachedPlan_.visibleInstances.size();
            ++visibleIndex) {
        const int8_t bucket =
            impl_->structuralProjectionBucketByVisible_[visibleIndex];
        if (bucket < 0 || static_cast<size_t>(bucket) <= safeBucket)
            continue;
        const Obol::internal::CadVisibleInstance &instance =
            impl_->cachedPlan_.visibleInstances[visibleIndex];
        if (!(instance.flags &
                Obol::internal::CadInstanceLodStructuralProxy))
            continue;
        instances.push_back(instance.instanceId);
    }
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

Obol::CadStructuralProxyPresentationWork
SoCADAssembly::lastStructuralProxyPresentationWork() const
{
    Obol::CadStructuralProxyPresentationWork work;
    work.exact = impl_->structuralProjectionHistogram_.exact;
    work.retainedWireBoxCount = impl_->uncollapsedStructuralProxyCount_;

    for (const Obol::internal::CadSubpixelProxyPoint& proxy :
            impl_->cachedPlan_.subpixelProxyPoints) {
        if ((proxy.flags & Obol::internal::CadInstanceHidden) ||
                !(proxy.flags &
                    Obol::internal::CadInstanceLodStructuralProxy))
            continue;
        if (proxy.shape ==
                Obol::internal::CadAggregateProxyShape::Box) {
            if (work.aggregateBoxCount != UINT64_MAX)
                ++work.aggregateBoxCount;
        } else if (work.aggregatePointCount != UINT64_MAX) {
            ++work.aggregatePointCount;
        }
    }
    return work;
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
