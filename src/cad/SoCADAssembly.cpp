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

#include <Inventor/system/gl.h>

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

    // Picking acceleration structures
    obol::picking::CadInstanceBVH instanceBvh_;
    bool bvhDirty_   = true;
    bool inUpdate_   = false;

    // Per-part edge BVH cache (lazily built during picking)
    std::unordered_map<obol::PartId, obol::picking::CadPartEdgeBVH,
                       std::hash<obol::PartId>> partEdgeBvhCache_;

    // Per-part LoD structures (built when shaded geometry is supplied via upsertPart)
    std::unordered_map<obol::PartId, obol::TrianglePopLod,
                       std::hash<obol::PartId>> partLods_;

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
    touch();
}

// --- Part library ----------------------------------------------------------

void
SoCADAssembly::upsertPart(obol::PartId pid, const obol::PartGeometry& geom)
{
    impl_->parts_[pid] = geom;
    // Invalidate edge BVH for this part
    impl_->partEdgeBvhCache_.erase(pid);
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
    impl_->bvhDirty_ = true;
    if (!impl_->inUpdate_) touch();
}

void
SoCADAssembly::removePart(obol::PartId pid)
{
    impl_->parts_.erase(pid);
    impl_->partEdgeBvhCache_.erase(pid);
    impl_->partLods_.erase(pid);
    impl_->bvhDirty_ = true;
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

    impl_->bvhDirty_ = true;
    if (!impl_->inUpdate_) touch();
}

void
SoCADAssembly::removeInstance(obol::InstanceId iid)
{
    impl_->instances_.erase(iid);
    impl_->selected_.erase(iid);
    impl_->bvhDirty_ = true;
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
    impl_->bvhDirty_ = true;
    if (!impl_->inUpdate_) touch();
}

void
SoCADAssembly::updateInstanceStyle(obol::InstanceId iid, const obol::InstanceStyle& style)
{
    auto it = impl_->instances_.find(iid);
    if (it == impl_->instances_.end()) return;
    it->second.style = style;
    if (!impl_->inUpdate_) touch();
}

void
SoCADAssembly::setSelectedInstances(const std::vector<obol::InstanceId>& ids)
{
    impl_->selected_.clear();
    impl_->selected_.insert(ids.begin(), ids.end());
    if (!impl_->inUpdate_) touch();
}

// --- Query -----------------------------------------------------------------

size_t SoCADAssembly::instanceCount() const { return impl_->instances_.size(); }
size_t SoCADAssembly::partCount()     const { return impl_->parts_.size();     }

// ---------------------------------------------------------------------------
// GLRender
// ---------------------------------------------------------------------------

void
SoCADAssembly::GLRender(SoGLRenderAction* action)
{
    if (impl_->instances_.empty()) return;

    SoState* state = action->getState();
    const int dm = drawMode.getValue();
    const bool useLod = (lodEnabled.getValue() == TRUE);

    // Gather camera/viewport info once if LoD is active
    SbVec3f camPos(0.0f, 0.0f, 0.0f);
    float   vpHeight = 1.0f;
    if (useLod) {
        SbViewVolume   vv = SoViewVolumeElement::get(state);
        SbViewportRegion vp = SoViewportRegionElement::get(state);
        vpHeight = static_cast<float>(vp.getViewportSizePixels()[1]);
        camPos   = vv.getProjectionPoint();
    }

    // For each instance: draw its part geometry in the current draw mode.
    for (const auto& [iid, idata] : impl_->instances_) {
        auto geomIt = impl_->parts_.find(idata.partId);
        if (geomIt == impl_->parts_.end()) continue;
        const auto& geom = geomIt->second;

        // Set per-instance color
        float r = 0.8f, g = 0.8f, b = 0.8f, a = 1.0f;
        if (idata.style.hasColorOverride) {
            r = idata.style.color[0];
            g = idata.style.color[1];
            b = idata.style.color[2];
            a = idata.style.color[3];
        }
        bool isSelected = impl_->selected_.count(iid) > 0;
        if (isSelected) { r = 1.0f; g = 1.0f; b = 0.0f; }

        // Push instance transform
        // SbMatrix is row-major (OI convention); glMultMatrixf expects the raw
        // OI float[16] data directly – GL interprets it as column-major, which
        // gives the transpose of the OI matrix and is the correct conversion
        // (OI uses post-multiply row vectors; GL uses pre-multiply column vectors).
        glPushMatrix();
        glMultMatrixf(idata.localToRoot[0]);

        glColor4f(r, g, b, a);

        // --- Wireframe pass ---
        if (dm == WIREFRAME || dm == SHADED_WITH_EDGES) {
            if (geom.wire.has_value()) {
                glLineWidth(idata.style.lineWidth);
                for (const auto& poly : geom.wire->polylines) {
                    if (poly.points.size() < 2) continue;
                    glBegin(GL_LINE_STRIP);
                    for (const auto& pt : poly.points) {
                        glVertex3f(pt[0], pt[1], pt[2]);
                    }
                    glEnd();
                }
                glLineWidth(1.0f);
            }
        }

        // --- Shaded pass ---
        if (dm == SHADED || dm == SHADED_WITH_EDGES) {
            if (geom.shaded.has_value()) {
                const auto& mesh = *geom.shaded;

                // Determine which triangles to render (LoD selection)
                const obol::TrianglePopLod* lodPtr = nullptr;
                uint8_t lodLevel = obol::TrianglePopLod::kMaxLevel;
                if (useLod) {
                    auto lodIt = impl_->partLods_.find(idata.partId);
                    if (lodIt != impl_->partLods_.end() && lodIt->second.isBuilt()) {
                        lodPtr = &lodIt->second;
                        // Compute screen-space projected size of bounding sphere
                        if (!idata.worldBounds.isEmpty()) {
                            SbVec3f center = idata.worldBounds.getCenter();
                            SbVec3f mn, mx;
                            idata.worldBounds.getBounds(mn, mx);
                            float radius = ((mx - mn) * 0.5f).length();
                            float dist = std::max(1e-4f, (camPos - center).length());
                            // screen pixels covered by the bounding sphere radius
                            float screenPixels = (radius / dist) * vpHeight;
                            lodLevel = static_cast<uint8_t>(
                                std::min(255.0f, std::max(0.0f, screenPixels)));
                        }
                    }
                }

                // Render triangles, filtered by LoD when active
                auto renderTri = [&](uint32_t i0, uint32_t i1, uint32_t i2) {
                    if (i0 >= mesh.positions.size() ||
                        i1 >= mesh.positions.size() ||
                        i2 >= mesh.positions.size()) return;
                    if (!mesh.normals.empty() && i0 < mesh.normals.size()) {
                        const auto& n = mesh.normals[i0];
                        glNormal3f(n[0], n[1], n[2]);
                    }
                    const auto& p0 = mesh.positions[i0];
                    const auto& p1 = mesh.positions[i1];
                    const auto& p2 = mesh.positions[i2];
                    glVertex3f(p0[0], p0[1], p0[2]);
                    glVertex3f(p1[0], p1[1], p1[2]);
                    glVertex3f(p2[0], p2[1], p2[2]);
                };

                glBegin(GL_TRIANGLES);
                if (lodPtr && lodLevel < obol::TrianglePopLod::kMaxLevel) {
                    // LoD path: render only triangles non-degenerate at this level
                    std::vector<uint32_t> tris = lodPtr->trianglesAtLevel(lodLevel);
                    for (uint32_t triIdx : tris) {
                        renderTri(mesh.indices[3*triIdx],
                                  mesh.indices[3*triIdx+1],
                                  mesh.indices[3*triIdx+2]);
                    }
                } else {
                    // Full-detail path
                    const size_t numTris = mesh.indices.size() / 3;
                    for (size_t t = 0; t < numTris; ++t) {
                        renderTri(mesh.indices[3*t],
                                  mesh.indices[3*t+1],
                                  mesh.indices[3*t+2]);
                    }
                }
                glEnd();
            }
        }

        glPopMatrix();
    }
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

    // World-space tolerance (approximate from screen-space tolerance)
    // We use a fixed world-space fallback here; screen-space computation
    // requires the viewport/projection which varies per viewer.
    const float toleranceWS = edgePickTolerancePx.getValue() * 0.01f;

    obol::picking::CadPickResult result;

    if (pm == PICK_EDGE || pm == PICK_HYBRID) {
        result = obol::picking::CadPickQuery::pickEdge(
            worldRay,
            impl_->instanceBvh_,
            impl_->parts_,
            impl_->partEdgeBvhCache_,
            toleranceWS);
    }

    if (!result.valid && (pm == PICK_TRIANGLE || pm == PICK_HYBRID || pm == PICK_BOUNDS)) {
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
