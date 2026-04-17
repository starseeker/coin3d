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
 *  - GLRender: iterates visible instances, draws wire polylines using
 *    GL_LINE_STRIP (instancing-aware loop) and optionally triangle meshes.
 *  - rayPick: delegates to CadPickQuery using the current pickMode.
 *  - getBoundingBox: returns the union of all instance world bounds.
 *  - The Pimpl (SoCADAssemblyImpl) holds the mutable instance/part databases
 *    and lazily-built acceleration structures.
 */

#include <obol/cad/SoCADAssembly.h>
#include <obol/cad/SoCADDetail.h>
#include "CadFramePlan.h"
#include "CadRendererGL.h"
#include "picking/CadPicking.h"
#include "lod/TrianglePopLod.h"

#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/actions/SoRayPickAction.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/actions/SoGetPrimitiveCountAction.h>
#include <Inventor/SoPickedPoint.h>
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

#include <Inventor/system/gl.h>
#include "rendering/SoGL.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <algorithm>
#include <cstring>

// ---------------------------------------------------------------------------
// SoCADAssemblyImpl – private implementation (Pimpl pattern)
// ---------------------------------------------------------------------------

struct InstanceData {
    obol::PartId          partId;
    SbMatrix              localToRoot;
    obol::InstanceStyle   style;
    SbBox3f               worldBounds;   // cached; recomputed on transform change
};

struct SoCADAssemblyImpl {
    // Part library
    std::unordered_map<obol::PartId, obol::PartGeometry,
                       std::hash<obol::PartId>> parts_;

    // Instance database
    std::unordered_map<obol::InstanceId, InstanceData,
                       std::hash<obol::InstanceId>> instances_;

    // Selection set
    std::unordered_set<obol::InstanceId,
                       std::hash<obol::InstanceId>> selected_;

    // Hidden-instance set (excluded from rendering and frame plan)
    std::unordered_set<obol::InstanceId,
                       std::hash<obol::InstanceId>> hidden_;

    // Picking acceleration structures
    obol::picking::CadInstanceBVH instanceBvh_;
    bool bvhDirty_   = true;
    bool inUpdate_   = false;

    // Per-part edge BVH cache (lazily built during picking)
    std::unordered_map<obol::PartId, obol::picking::CadPartEdgeBVH,
                       std::hash<obol::PartId>> partEdgeBvhCache_;

    // Per-part triangle BVH cache (lazily built during picking)
    std::unordered_map<obol::PartId, obol::picking::CadPartTriBVH,
                       std::hash<obol::PartId>> partTriBvhCache_;

    // Per-part LoD structures (built when shaded geometry is supplied via upsertPart)
    std::unordered_map<obol::PartId, obol::TrianglePopLod,
                       std::hash<obol::PartId>> partLods_;

    // Per-part generation counter – incremented when geometry changes so the
    // renderer knows to re-upload VBOs.
    std::unordered_map<obol::PartId, uint64_t,
                       std::hash<obol::PartId>> partGeneration_;
    uint64_t nextGeneration_ = 1;

    // LoD index cache: per-(PartId, level) filtered triangle index lists.
    // Cleared when a part's generation counter changes.
    struct LodCacheEntry {
        uint64_t generation = UINT64_MAX;
        std::unordered_map<uint8_t, std::vector<uint32_t>> byLevel;
    };
    mutable std::unordered_map<obol::PartId, LodCacheEntry,
                               std::hash<obol::PartId>> lodIndexCache_;

    // Frame plan cache.  Rebuilt lazily when planDirty_ is true.
    obol::internal::CadFramePlan cachedPlan_;
    bool planDirty_    = true;   ///< Plan must be rebuilt before next render
    int  cachedDM_     = -1;     ///< Draw mode used for the cached plan

    // VBO + shader renderer (lazy-created on first GLRender call)
    std::unique_ptr<obol::internal::CadRendererGL> renderer_;

    // Rebuild instance BVH if dirty
    void rebuildBvhIfNeeded() {
        if (!bvhDirty_) return;
        std::vector<obol::picking::CadInstanceBVH::Entry> entries;
        entries.reserve(instances_.size());
        for (const auto& [iid, idata] : instances_) {
            obol::picking::CadInstanceBVH::Entry e;
            e.worldBounds  = idata.worldBounds;
            e.instanceId   = iid;
            e.partId       = idata.partId;
            e.localToWorld = idata.localToRoot;
            entries.push_back(e);
        }
        instanceBvh_.build(std::move(entries));
        bvhDirty_ = false;
    }

    // Compute world bounds for an instance from part geometry
    SbBox3f computeWorldBounds(const obol::PartGeometry& geom,
                               const SbMatrix& m) const {
        SbBox3f local;
        if (geom.wire)   { local.extendBy(geom.wire->bounds);   }
        if (geom.shaded) { local.extendBy(geom.shaded->bounds); }
        if (local.isEmpty()) {
            // Fall back to a unit cube at the origin
            local.extendBy(SbVec3f(-0.5f,-0.5f,-0.5f));
            local.extendBy(SbVec3f( 0.5f, 0.5f, 0.5f));
        }
        // Transform all 8 corners
        SbBox3f world;
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

    /**
     * Build a CadFramePlan from the current instance and part databases.
     *
     * Groups instances by part to maximise batching in the renderer.
     * The instance list is sorted by part so each CadDrawItem covers a
     * contiguous run of visibleInstances.
     *
     * @param dm       Draw mode (WIREFRAME / SHADED / SHADED_WITH_EDGES).
     * @param selected Set of selected instance IDs (for colour override).
     * @param hidden   Set of hidden instance IDs (excluded from the plan).
     */
    obol::internal::CadFramePlan buildFramePlan(
            int dm,
            const std::unordered_set<obol::InstanceId,
                                     std::hash<obol::InstanceId>>& selected,
            const std::unordered_set<obol::InstanceId,
                                     std::hash<obol::InstanceId>>& hidden) const
    {
        using namespace obol::internal;

        CadFramePlan plan;
        if (instances_.empty()) return plan;

        // Collect (partId → list of CadVisibleInstance) grouped by part.
        // We use a stable insertion-order map so every run is contiguous.
        std::unordered_map<obol::PartId,
                           std::vector<CadVisibleInstance>,
                           std::hash<obol::PartId>> byPart;

        for (const auto& [iid, idata] : instances_) {
            // Skip hidden instances
            if (hidden.count(iid)) continue;

            CadVisibleInstance vi;
            vi.instanceId = iid;
            vi.partIndex  = 0; // filled in below

            // Copy transform: raw OI float[16] (row-major).
            // The renderer uploads with GL_FALSE so GL sees it as column-major
            // (= transpose of OI matrix), which is the correct GL convention.
            std::memcpy(vi.transform.data(), idata.localToRoot[0],
                        16 * sizeof(float));

            // Colour with selection override – evaluate selected once only.
            const bool isSel = selected.count(iid) != 0;
            float r = 0.8f, g = 0.8f, b = 0.8f, a = 1.0f;
            if (idata.style.hasColorOverride) {
                r = idata.style.color[0];
                g = idata.style.color[1];
                b = idata.style.color[2];
                a = idata.style.color[3];
            }
            if (isSel) { r = 1.0f; g = 1.0f; b = 0.0f; }

            vi.rgba[0] = static_cast<uint8_t>(std::min(255.0f, r * 255.0f));
            vi.rgba[1] = static_cast<uint8_t>(std::min(255.0f, g * 255.0f));
            vi.rgba[2] = static_cast<uint8_t>(std::min(255.0f, b * 255.0f));
            vi.rgba[3] = static_cast<uint8_t>(std::min(255.0f, a * 255.0f));
            vi.flags   = isSel ? 1u : 0u;

            // Store world bounding box for per-instance frustum culling.
            SbVec3f wbMn, wbMx;
            idata.worldBounds.getBounds(wbMn, wbMx);
            vi.wbMin[0] = wbMn[0]; vi.wbMin[1] = wbMn[1]; vi.wbMin[2] = wbMn[2];
            vi.wbMax[0] = wbMx[0]; vi.wbMax[1] = wbMx[1]; vi.wbMax[2] = wbMx[2];

            byPart[idata.partId].push_back(vi);
        }

        const bool needWire   = (dm == SoCADAssembly::WIREFRAME ||
                                 dm == SoCADAssembly::SHADED_WITH_EDGES);
        const bool needShaded = (dm == SoCADAssembly::SHADED ||
                                 dm == SoCADAssembly::SHADED_WITH_EDGES);

        // Track which (part, type) pairs have already been added to requiredReps
        // to avoid duplicates (a part with both wire and shaded gets one entry each).
        std::unordered_set<obol::PartId, std::hash<obol::PartId>> requiredWireParts, requiredShadedParts;

        uint32_t baseInst = 0;
        for (auto& [pid, vis] : byPart) {
            auto partIt = parts_.find(pid);
            if (partIt == parts_.end()) continue;
            const auto& geom = partIt->second;

            const uint32_t count = static_cast<uint32_t>(vis.size());

            // Fill partIndex (index into the upcoming visibleInstances block)
            for (auto& vi : vis) vi.partIndex = baseInst;

            // Append instances to flat list
            for (const auto& vi : vis) plan.visibleInstances.push_back(vi);

            // Wire draw item
            if (needWire && geom.wire.has_value()) {
                CadDrawItem item;
                item.rep.part  = pid;
                item.rep.type  = CadRepType::WireSegments;
                item.rep.level = 255;
                item.baseInstance  = baseInst;
                item.instanceCount = count;
                plan.wireItems.push_back(item);
                if (!requiredWireParts.count(pid)) {
                    plan.requiredReps.push_back(item.rep);
                    requiredWireParts.insert(pid);
                }
            }

            // Shaded draw item
            if (needShaded && geom.shaded.has_value()) {
                CadDrawItem item;
                item.rep.part  = pid;
                item.rep.type  = CadRepType::Triangles;
                item.rep.level = 255;
                item.baseInstance  = baseInst;
                item.instanceCount = count;
                plan.shadedItems.push_back(item);
                if (!requiredShadedParts.count(pid)) {
                    plan.requiredReps.push_back(item.rep);
                    requiredShadedParts.insert(pid);
                }
            }

            baseInst += count;
        }

        return plan;
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
}

SoCADAssembly::SoCADAssembly()
    : impl_(new SoCADAssemblyImpl)
{
    SO_NODE_CONSTRUCTOR(SoCADAssembly);

    SO_NODE_DEFINE_ENUM_VALUE(DrawMode, SHADED);
    SO_NODE_DEFINE_ENUM_VALUE(DrawMode, WIREFRAME);
    SO_NODE_DEFINE_ENUM_VALUE(DrawMode, SHADED_WITH_EDGES);
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
    SO_NODE_ADD_FIELD(lodEnabled,          (FALSE));
}

SoCADAssembly::~SoCADAssembly() = default;

// --- Update framing --------------------------------------------------------

void SoCADAssembly::beginUpdate() { impl_->inUpdate_ = true; }

void SoCADAssembly::endUpdate()
{
    impl_->inUpdate_ = false;
    impl_->bvhDirty_ = true;
    impl_->planDirty_ = true;
    touch();
}

// --- Part library ----------------------------------------------------------

void
SoCADAssembly::upsertPart(obol::PartId pid, const obol::PartGeometry& geom)
{
    impl_->parts_[pid] = geom;
    // Bump generation counter so the renderer re-uploads VBOs
    impl_->partGeneration_[pid] = impl_->nextGeneration_++;
    // Invalidate edge and triangle BVH caches for this part
    impl_->partEdgeBvhCache_.erase(pid);
    impl_->partTriBvhCache_.erase(pid);
    // Invalidate LoD index cache for this part
    impl_->lodIndexCache_.erase(pid);
    // Build or replace LoD structure for shaded geometry
    if (geom.shaded.has_value()) {
        const auto& mesh = *geom.shaded;
        obol::TrianglePopLod lod;
        lod.build(mesh.positions, mesh.indices, mesh.bounds);
        impl_->partLods_[pid] = std::move(lod);
    } else {
        impl_->partLods_.erase(pid);
    }
    // Recompute world bounds for any instances using this part
    for (auto& [iid, idata] : impl_->instances_) {
        if (idata.partId == pid) {
            idata.worldBounds = impl_->computeWorldBounds(geom, idata.localToRoot);
        }
    }
    impl_->bvhDirty_  = true;
    impl_->planDirty_ = true;
    if (!impl_->inUpdate_) touch();
}

void
SoCADAssembly::removePart(obol::PartId pid)
{
    impl_->parts_.erase(pid);
    impl_->partEdgeBvhCache_.erase(pid);
    impl_->partTriBvhCache_.erase(pid);
    impl_->partLods_.erase(pid);
    impl_->lodIndexCache_.erase(pid);
    impl_->bvhDirty_  = true;
    impl_->planDirty_ = true;
    if (!impl_->inUpdate_) touch();
}

// --- Instance management ---------------------------------------------------

obol::InstanceId
SoCADAssembly::upsertInstanceAuto(const obol::InstanceRecord& rec)
{
    obol::InstanceId iid = obol::CadIdBuilder::extendNameOccBool(
        rec.parent, rec.childName, rec.occurrenceIndex, rec.boolOp);
    upsertInstance(iid, rec);
    return iid;
}

void
SoCADAssembly::upsertInstance(obol::InstanceId iid, const obol::InstanceRecord& rec)
{
    InstanceData& idata  = impl_->instances_[iid];
    idata.partId         = rec.part;
    idata.localToRoot    = rec.localToRoot;
    idata.style          = rec.style;

    auto geomIt = impl_->parts_.find(rec.part);
    if (geomIt != impl_->parts_.end()) {
        idata.worldBounds = impl_->computeWorldBounds(geomIt->second, rec.localToRoot);
    } else {
        idata.worldBounds = SbBox3f();
    }

    impl_->bvhDirty_  = true;
    impl_->planDirty_ = true;
    if (!impl_->inUpdate_) touch();
}

void
SoCADAssembly::removeInstance(obol::InstanceId iid)
{
    impl_->instances_.erase(iid);
    impl_->selected_.erase(iid);
    impl_->hidden_.erase(iid);
    impl_->bvhDirty_  = true;
    impl_->planDirty_ = true;
    if (!impl_->inUpdate_) touch();
}

void
SoCADAssembly::updateInstanceTransform(obol::InstanceId iid, const SbMatrix& m)
{
    auto it = impl_->instances_.find(iid);
    if (it == impl_->instances_.end()) return;
    it->second.localToRoot = m;
    auto geomIt = impl_->parts_.find(it->second.partId);
    if (geomIt != impl_->parts_.end()) {
        it->second.worldBounds = impl_->computeWorldBounds(geomIt->second, m);
    }
    impl_->bvhDirty_  = true;
    impl_->planDirty_ = true;
    if (!impl_->inUpdate_) touch();
}

void
SoCADAssembly::updateInstanceStyle(obol::InstanceId iid, const obol::InstanceStyle& style)
{
    auto it = impl_->instances_.find(iid);
    if (it == impl_->instances_.end()) return;
    it->second.style = style;
    impl_->planDirty_ = true;
    if (!impl_->inUpdate_) touch();
}

void
SoCADAssembly::setSelectedInstances(const std::vector<obol::InstanceId>& ids)
{
    impl_->selected_.clear();
    impl_->selected_.insert(ids.begin(), ids.end());
    impl_->planDirty_ = true;
    if (!impl_->inUpdate_) touch();
}

// --- Query -----------------------------------------------------------------

size_t SoCADAssembly::instanceCount() const { return impl_->instances_.size(); }
size_t SoCADAssembly::partCount()     const { return impl_->parts_.size();     }

const obol::PartGeometry*
SoCADAssembly::partGeometry(obol::PartId pid) const
{
    auto it = impl_->parts_.find(pid);
    if (it == impl_->parts_.end()) return nullptr;
    return &it->second;
}

std::optional<obol::InstanceRecord>
SoCADAssembly::getInstanceRecord(obol::InstanceId iid) const
{
    auto it = impl_->instances_.find(iid);
    if (it == impl_->instances_.end()) return std::nullopt;
    const InstanceData& d = it->second;
    obol::InstanceRecord rec;
    rec.part        = d.partId;
    rec.localToRoot = d.localToRoot;
    rec.style       = d.style;
    // parent/childName/occurrenceIndex/boolOp are derivation inputs and are
    // not stored after insert; the caller must track them externally if needed.
    return rec;
}

const std::vector<uint32_t>*
SoCADAssembly::getLodFilteredIndices(obol::PartId pid, uint8_t level) const
{
    auto lodIt = impl_->partLods_.find(pid);
    if (lodIt == impl_->partLods_.end() || !lodIt->second.isBuilt()) return nullptr;

    auto genIt = impl_->partGeneration_.find(pid);
    uint64_t gen = (genIt != impl_->partGeneration_.end()) ? genIt->second : 0;

    auto& cacheEntry = impl_->lodIndexCache_[pid];
    if (cacheEntry.generation != gen) {
        cacheEntry.generation = gen;
        cacheEntry.byLevel.clear();
    }
    auto it = cacheEntry.byLevel.find(level);
    if (it == cacheEntry.byLevel.end()) {
        cacheEntry.byLevel[level] = lodIt->second.trianglesAtLevel(level);
        return &cacheEntry.byLevel[level];
    }
    return &it->second;
}

void
SoCADAssembly::setHiddenInstances(const std::vector<obol::InstanceId>& ids)
{
    impl_->hidden_.clear();
    impl_->hidden_.insert(ids.begin(), ids.end());
    impl_->planDirty_ = true;
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

    // Lazy-create the renderer the first time we have a GL context.
    if (!impl_->renderer_) {
        impl_->renderer_ = std::make_unique<obol::internal::CadRendererGL>();
    }

    // Build the combined view-projection matrix from the state stack.
    // Both matrices are OI row-major SbMatrix values.
    const SbMatrix viewMat = SoViewingMatrixElement::get(state);
    const SbMatrix projMat = SoProjectionMatrixElement::get(state);
    // OI post-multiply convention: VP = view * proj
    SbMatrix viewProj = viewMat;
    viewProj.multRight(projMat);

    // Derive camera (eye) position in world space from the inverse view matrix.
    // In OI convention: p_view = p_world * view, so p_world = p_view * view^-1.
    // The view-space origin (0,0,0) maps to the translation row of the inverse.
    SbMatrix invView = viewMat.inverse();
    SbVec3f cameraPos(invView[3][0], invView[3][1], invView[3][2]);

    const int dm = drawMode.getValue();

    // Rebuild the frame plan only when geometry, instances, styles, selection,
    // hidden set, or draw mode have changed.  Camera moves do NOT invalidate
    // the plan, so it is reused every frame during interactive orbit.
    if (impl_->planDirty_ || impl_->cachedDM_ != dm) {
        impl_->cachedPlan_  = impl_->buildFramePlan(dm, impl_->selected_,
                                                     impl_->hidden_);
        impl_->planDirty_   = false;
        impl_->cachedDM_    = dm;
    }

    const bool lod = (lodEnabled.getValue() == TRUE);

    // Delegate to the VBO + shader renderer (GL 2.0 minimum; optional GL 3.1+
    // instanced path selected automatically when available)
    impl_->renderer_->render(impl_->cachedPlan_, *this, glue, viewProj,
                             cameraPos, lod, impl_->partGeneration_);
}

// ---------------------------------------------------------------------------
// rayPick
// ---------------------------------------------------------------------------

void
SoCADAssembly::rayPick(SoRayPickAction* action)
{
    if (impl_->instances_.empty()) return;

    impl_->rebuildBvhIfNeeded();

    // Get the pick ray in world space
    SbLine worldRay = action->getLine();

    // Determine effective pick mode
    int pm = pickMode.getValue();
    if (pm == PICK_AUTO) {
        pm = (drawMode.getValue() == WIREFRAME) ? PICK_EDGE : PICK_TRIANGLE;
    }

    // Derive a world-space edge-pick tolerance from the screen-space field.
    // Approximate: use the view volume to find how large one pixel is in world
    // space at the assembly centre, then scale by the user-specified tolerance.
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
                    if (!idata.worldBounds.isEmpty())
                        bbox.extendBy(idata.worldBounds);
                }
                float dist = vv.getNearDist() * 10.0f;
                if (!bbox.isEmpty()) {
                    dist = (bbox.getCenter() - worldRay.getPosition())
                               .dot(worldRay.getDirection());
                    dist = std::max(vv.getNearDist(), dist);
                }
                // Height of the view volume at that distance (perspective or ortho)
                float nearH  = vv.getHeight();          // at nearDist for persp
                float nearD  = vv.getNearDist();
                float pixelH = (nearH / vpH) * (dist / nearD);
                toleranceWS  = edgePickTolerancePx.getValue() * pixelH;
            }
        }
    }

    obol::picking::CadPickResult result;

    if (pm == PICK_EDGE || pm == PICK_HYBRID) {
        result = obol::picking::CadPickQuery::pickEdge(
            worldRay,
            impl_->instanceBvh_,
            impl_->parts_,
            impl_->partEdgeBvhCache_,
            toleranceWS);
    }

    if (!result.valid && (pm == PICK_TRIANGLE || pm == PICK_HYBRID)) {
        result = obol::picking::CadPickQuery::pickTriangle(
            worldRay,
            impl_->instanceBvh_,
            impl_->parts_,
            impl_->partTriBvhCache_);
    }

    if (!result.valid && pm == PICK_BOUNDS) {
        result = obol::picking::CadPickQuery::pickBounds(
            worldRay,
            impl_->instanceBvh_);
    }

    // For PICK_HYBRID: also try bounds if triangle picking returned nothing.
    if (!result.valid && pm == PICK_HYBRID) {
        result = obol::picking::CadPickQuery::pickBounds(
            worldRay,
            impl_->instanceBvh_);
    }

    if (!result.valid) return;

    // Register the hit with the pick action
    SoPickedPoint* pp = action->addIntersection(result.hitPoint);
    if (!pp) return;

    SoCADDetail* detail = new SoCADDetail;
    detail->setInstanceId(result.instanceId);
    detail->setPartId(result.partId);
    switch (result.primType) {
        case obol::picking::CadPickResult::EDGE:
            detail->setPrimType(SoCADDetail::EDGE);
            detail->setPrimIndex0(result.primIndex0);
            detail->setPrimIndex1(result.primIndex1);
            detail->setU(result.u);
            break;
        case obol::picking::CadPickResult::TRIANGLE:
            detail->setPrimType(SoCADDetail::TRIANGLE);
            detail->setPrimIndex0(result.primIndex0);
            break;
        default:
            detail->setPrimType(SoCADDetail::BOUNDS);
            break;
    }
    pp->setDetail(detail, this);
}

// ---------------------------------------------------------------------------
// getBoundingBox
// ---------------------------------------------------------------------------

void
SoCADAssembly::getBoundingBox(SoGetBoundingBoxAction* action)
{
    SbBox3f worldBox;
    for (const auto& [iid, idata] : impl_->instances_) {
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
        auto geomIt = impl_->parts_.find(idata.partId);
        if (geomIt == impl_->parts_.end()) continue;
        const auto& geom = geomIt->second;
        if (geom.wire) {
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
