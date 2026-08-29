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

/** @file CadRendererGLIndirect.cpp @brief Indirect CAD draw preparation. */

#include "CadRendererGL.h"
#include "CadRendererConfiguration.h"
#include "CadRendererGLExecutorUtils.h"
#include "CadResolvedDraw.h"
#include "CadShaderSources.h"

#include <Obol/cad/CadProjectedProxy.h>
#include <Obol/cad/SoCADAssembly.h>

#include <Inventor/system/gl.h>
#include "glue/glp.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <new>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#ifndef GL_DRAW_INDIRECT_BUFFER
#define GL_DRAW_INDIRECT_BUFFER 0x8F3F
#endif

namespace Obol {
namespace internal {

static SbVec2s
cadPressureProxyViewportSize(const SoGLContext *glue)
{
    if (!glue)
        return SbVec2s(0, 0);

    GLint viewport[4] = {0, 0, 0, 0};
    glue->glGetIntegerv(GL_VIEWPORT, viewport);
    const auto dimension = [](GLint value) {
        return static_cast<short>(std::max<GLint>(0,
            std::min<GLint>(value,
                std::numeric_limits<short>::max())));
    };
    return SbVec2s(dimension(viewport[2]), dimension(viewport[3]));
}

static CadSubpixelProxyPoint
cadPressureProxyForInstance(const CadPartBinding& binding,
                            const CadVisibleInstance& instance,
                            const SbMatrix& viewProj,
                            const SbVec2s& viewportSize)
{
    CadSubpixelProxyPoint replacement;
    replacement.boundsMinimum = SbVec3f(
        instance.wbMin[0], instance.wbMin[1], instance.wbMin[2]);
    replacement.boundsMaximum = SbVec3f(
        instance.wbMax[0], instance.wbMax[1], instance.wbMax[2]);
    replacement.position =
        (replacement.boundsMinimum + replacement.boundsMaximum) * 0.5f;
    replacement.rgba = instance.rgba;
    replacement.instanceId = instance.instanceId;
    replacement.flags = instance.flags;

    SbMatrix model;
    model.setValue(instance.transform.data());
    const Obol::CadProjectedProxy projected =
        Obol::classifyCadProjectedProxy(
            binding.subpixelProxyCorners.data(), model, viewProj,
            viewportSize, Obol::CadMaximumPointProxyExtentPixels);
    if (projected.pointEligible) {
        replacement.position = projected.point;
        replacement.shape = CadAggregateProxyShape::Point;
    } else {
        /* A partially clipped or otherwise unclassifiable occurrence retains
         * its conservative extent.  Only a complete <=5-pixel projection is
         * safe to collapse under atlas pressure. */
        replacement.shape = CadAggregateProxyShape::Box;
        for (size_t corner = 0;
                corner < replacement.boxCorners.size(); ++corner)
            model.multVecMatrix(binding.subpixelProxyCorners[corner],
                replacement.boxCorners[corner]);
        replacement.boxCornersValid = true;
        replacement.boxOriented = binding.subpixelProxyOriented;
    }
    return replacement;
}

bool CadRendererGL::rejectIndirect(int status, const char *reason)
{
    lastIndirectStatus_ = status;
    if (configuration_->indirectDebug &&
            reportedIndirectStatus_ != status) {
        std::fprintf(stderr,
            "CadRendererGL indirect rejected status=%d reason=%s\n",
            status, reason ? reason : "unknown");
        reportedIndirectStatus_ = status;
    }
    return false;
}

bool CadRendererGL::submitIndirectPrepared(
        const SoGLContext *glue,
        const SbMatrix& viewProj,
        const SbViewVolume& viewVolume)
{
    if (!glue || !gpuRes_ || !indirectPrepared_.valid)
        return false;
    if (indirectPrepared_.pages.empty()) {
        if (indirectPrepared_.pressureProxyPoints.empty())
            return false;
        lastRenderedWork_ = indirectPrepared_.renderedWork;
        lastIndirectStatus_ = 0;
        reportedIndirectStatus_ = 0;
        return true;
    }

    if (indirectPrepared_.instances.empty())
        return rejectIndirect(9, "empty prepared instance stream");
    if (!gpuRes_->instanceVbo() ||
            gpuRes_->instanceUploadSerial() !=
                indirectPrepared_.instanceUploadSerial) {
        gpuRes_->uploadInstanceData(
            indirectPrepared_.instances.data(),
            static_cast<GLsizeiptr>(
                indirectPrepared_.instances.size() * sizeof(InstVertex)),
            glue);
        indirectPrepared_.instanceUploadSerial =
            gpuRes_->instanceUploadSerial();
    }
    const GLuint instanceVbo = gpuRes_->instanceVbo();
    if (!instanceVbo)
        return rejectIndirect(10, "prepared instance upload");

    for (const IndirectPageWork& work : indirectPrepared_.pages) {
        CadTriangleAtlasPage *page =
            gpuRes_->triangleAtlasPage(work.page);
        if (!page || !page->indirectBuf || !page->indirectCapacity ||
                (work.ordinary.empty() && work.culled.empty()))
            return rejectIndirect(11, "prepared page preflight");

        const bool newVao = !page->vao;
        if (newVao)
            glue->glGenVertexArrays(1, &page->vao);
        glue->glBindVertexArray(page->vao);
        if (newVao) {
            glue->glBindBuffer(GL_ARRAY_BUFFER, page->posBuf);
            glue->glVertexAttribPointerARB(
                0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
            glue->glEnableVertexAttribArrayARB(0);
            glue->glBindBuffer(
                GL_ARRAY_BUFFER,
                page->normBuf ? page->normBuf : page->posBuf);
            glue->glVertexAttribPointerARB(
                1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
            glue->glEnableVertexAttribArrayARB(1);
            glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, page->idxBuf);
        }
        if (newVao || page->instanceVbo != instanceVbo) {
            const GLsizei stride =
                static_cast<GLsizei>(sizeof(InstVertex));
            glue->glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
            for (GLuint column = 0; column < 4; ++column) {
                const GLuint location = kInstTransformLoc + column;
                const uintptr_t offset =
                    offsetof(InstVertex, transform) +
                    column * 4u * sizeof(float);
                glue->glVertexAttribPointerARB(
                    location, 4, GL_FLOAT, GL_FALSE, stride,
                    reinterpret_cast<const GLvoid *>(offset));
                glue->glEnableVertexAttribArrayARB(location);
                glue->glVertexAttribDivisor(location, 1);
            }
            glue->glVertexAttribPointerARB(
                kInstColorLoc, 4, GL_FLOAT, GL_FALSE, stride,
                reinterpret_cast<const GLvoid *>(
                    offsetof(InstVertex, color)));
            glue->glEnableVertexAttribArrayARB(kInstColorLoc);
            glue->glVertexAttribDivisor(kInstColorLoc, 1);
            glue->glVertexAttribPointerARB(
                kInstPopMinLevelLoc, 4, GL_FLOAT, GL_FALSE, stride,
                reinterpret_cast<const GLvoid *>(
                    offsetof(InstVertex, popMinLevel)));
            glue->glEnableVertexAttribArrayARB(kInstPopMinLevelLoc);
            glue->glVertexAttribDivisor(kInstPopMinLevelLoc, 1);
            glue->glVertexAttribPointerARB(
                kInstPopMaxFlagsLoc, 4, GL_FLOAT, GL_FALSE, stride,
                reinterpret_cast<const GLvoid *>(
                    offsetof(InstVertex, popMaxFlags)));
            glue->glEnableVertexAttribArrayARB(kInstPopMaxFlagsLoc);
            glue->glVertexAttribDivisor(kInstPopMaxFlagsLoc, 1);
            page->instanceVbo = instanceVbo;
        }
        glue->glBindVertexArray(0);
    }
    glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
    glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glue->glUseProgramObjectARB(shaders_.shadedIndirect);
    const GLint viewProjection = glue->glGetUniformLocationARB(
        shaders_.shadedIndirect, "u_viewProj");
    glue->glUniformMatrix4fvARB(
        viewProjection, 1, GL_FALSE, viewProj[0]);
    uploadLights(glue, shaders_.shadedIndirect);
    uploadViewFacing(glue, shaders_.shadedIndirect, viewVolume);

    for (const IndirectPageWork& work : indirectPrepared_.pages) {
        const CadTriangleAtlasPage *page =
            gpuRes_->triangleAtlasPage(work.page);
        if (!page) continue;
        glue->glBindVertexArray(page->vao);
        const auto drawCommands =
            [&](const std::vector<CadDrawElementsIndirectCommand>& commands,
                bool cullBackfaces) {
                setCadBackfaceCulling(glue, cullBackfaces);
                size_t offset = 0;
                while (offset < commands.size()) {
                    const size_t count = std::min<size_t>(
                        commands.size() - offset,
                        page->indirectCapacity);
                    if (!gpuRes_->uploadTriangleAtlasCommands(
                            work.page, commands.data() + offset,
                            count, glue))
                        return false;
                    glue->glBindBuffer(
                        GL_DRAW_INDIRECT_BUFFER, page->indirectBuf);
                    glue->glMultiDrawElementsIndirect(
                        GL_TRIANGLES, GL_UNSIGNED_INT, nullptr,
                        static_cast<GLsizei>(count),
                        sizeof(CadDrawElementsIndirectCommand));
                    offset += count;
                }
                return true;
            };
        if (!drawCommands(work.ordinary, false) ||
                !drawCommands(work.culled, true)) {
            glue->glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
            glue->glBindVertexArray(0);
            glue->glUseProgramObjectARB(0);
            gpuRes_->releaseFlatShaded(glue);
            gpuRes_->releaseStandaloneTriangles(glue);
            lastIndirectStatus_ = 12;
            return true;
        }
    }
    glue->glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    glue->glBindVertexArray(0);
    glue->glUseProgramObjectARB(0);
    gpuRes_->releaseFlatShaded(glue);
    gpuRes_->releaseStandaloneTriangles(glue);
    lastRenderedWork_ = indirectPrepared_.renderedWork;
    lastIndirectStatus_ = 0;
    reportedIndirectStatus_ = 0;
    return true;
}

bool CadRendererGL::patchIndirectPreparedAppend(
        const CadFramePlan& plan,
        const SoGLContext *glue,
        const SbMatrix& viewProj)
{
    size_t deadlineWork = 256u;
    if (renderInterruptedAfter(deadlineWork))
        return false;
    const auto fail = [&](const char *reason) {
        if (configuration_->patchDebug)
            std::fprintf(stderr,
                "CadRendererGL append patch detail reason=%s "
                "prepared_append=%llu plan_append=%llu "
                "source_extent=%zu plan_sources=%zu\n",
                reason ? reason : "unknown",
                static_cast<unsigned long long>(
                    indirectPrepared_.appendRevision),
                static_cast<unsigned long long>(
                    plan.appendRevision),
                indirectPrepared_.instanceIndexBySource.size(),
                plan.visibleInstances.size());
        return false;
    };
    if (indirectPrepared_.appendRevision ==
            plan.appendRevision)
        return true;
    if (!glue || !gpuRes_ ||
            indirectPrepared_.appendRevision <
                plan.appendDeltaFloorRevision)
        return fail("journal-floor");

    std::vector<const CadPlanAppendDelta *> deltas;
    for (const CadPlanAppendDelta& delta :
            plan.appendDeltas) {
        if (renderInterruptedAfter(deadlineWork))
            return false;
        if (delta.revision >
                indirectPrepared_.appendRevision)
            deltas.push_back(&delta);
    }
    if (deltas.empty())
        return fail("empty-journal");

    /*
     * Keep append publication cheap without making the retained submission
     * an unbounded journal replay.  Once the shaded tail has grown by either
     * one exact-frame population or a modest startup quantum, ask the caller
     * for one exact preparation.  The anchor then doubles, so the complete
     * cost over a stream is a geometric series (O(final population)), while
     * stale/tombstoned commands, reverse indices, atlas bindings, and packed
     * instances are periodically cross-checked and compacted together.
     *
     * This boundary is based on structural growth, not elapsed time or frame
     * count, so a fast producer does not cause more work than a slow one and
     * camera interaction never triggers it by itself.
     */
    constexpr size_t minimumAppendGrowth = 4096u;
    size_t appendedCandidateCount = 0;
    for (const CadPlanAppendDelta *delta : deltas) {
        if (renderInterruptedAfter(deadlineWork))
            return false;
        if (!delta ||
                delta->shadedItemCount >
                    std::numeric_limits<size_t>::max() -
                        appendedCandidateCount)
            return fail("candidate-overflow");
        appendedCandidateCount += delta->shadedItemCount;
    }
    const size_t appendAnchor =
        indirectPrepared_.appendPatchAnchorInstanceCount;
    const size_t growthAllowance =
        std::max(minimumAppendGrowth, appendAnchor);
    const size_t packedLimit =
        appendAnchor >
                std::numeric_limits<size_t>::max() - growthAllowance ?
            std::numeric_limits<size_t>::max() :
            appendAnchor + growthAllowance;
    if (indirectPrepared_.instances.size() > packedLimit ||
            appendedCandidateCount >
                packedLimit - indirectPrepared_.instances.size()) {
        if (configuration_->patchDebug)
            std::fprintf(stderr,
                "CadRendererGL retained append patch requests geometric "
                "revalidation anchor=%zu packed=%zu candidates=%zu "
                "limit=%zu\n",
                appendAnchor, indirectPrepared_.instances.size(),
                appendedCandidateCount, packedLimit);
        return false;
    }

    const uint32_t noSlot =
        std::numeric_limits<uint32_t>::max();
    size_t sourceExtent =
        indirectPrepared_.instanceIndexBySource.size();
    const size_t priorPackedInstanceCount =
        indirectPrepared_.instances.size();
    const size_t priorPressureProxyCount =
        indirectPrepared_.pressureProxyPoints.size();
    bool pressureProxyAdded = false;
    const ExecutorFrustumPlanes fp =
        extractExecutorFrustumPlanes(viewProj);
    const SbVec2s pressureProxyViewportSize =
        cadPressureProxyViewportSize(glue);
    /*
     * The exact path touches every old part before pressure reclamation.
     * This append path deliberately does not rescan them: preserve that
     * already validated working set and fall back to exact preparation only
     * if free/new atlas capacity cannot admit the tail.
     */
    gpuRes_->deferTriangleAtlasReclamation();

    for (const CadPlanAppendDelta *delta : deltas) {
        if (renderInterruptedAfter(deadlineWork))
            return false;
        if (!delta ||
                delta->visibleBegin != sourceExtent ||
                delta->visibleBegin >
                    plan.visibleInstances.size() ||
                delta->visibleCount >
                    plan.visibleInstances.size() -
                        delta->visibleBegin ||
                delta->partBegin >
                    plan.partBindings.size() ||
                delta->partCount >
                    plan.partBindings.size() -
                        delta->partBegin ||
                delta->shadedItemBegin >
                    plan.shadedItems.size() ||
                delta->shadedItemCount >
                    plan.shadedItems.size() -
                        delta->shadedItemBegin)
            return fail("delta-shape");
        for (const uint32_t retired :
                delta->retiredVisibleIndices) {
            if (renderInterruptedAfter(deadlineWork))
                return false;
            if (retired >= sourceExtent)
                return fail("retired-source-range");
            if (retired <
                    indirectPrepared_.
                        instanceIndexBySource.size() &&
                    indirectPrepared_.
                        instanceIndexBySource[retired] != noSlot)
                return fail("retired-packed-instance");
            if (retired <
                    indirectPrepared_.
                        pressureProxyIndexBySource.size() &&
                    indirectPrepared_.
                        pressureProxyIndexBySource[retired] != noSlot)
                return fail("retired-pressure-proxy");
        }

        const size_t newSourceExtent =
            static_cast<size_t>(delta->visibleBegin) +
            delta->visibleCount;
        indirectPrepared_.instanceIndexBySource.resize(
            newSourceExtent, noSlot);
        indirectPrepared_.pressureProxyIndexBySource.resize(
            newSourceExtent, noSlot);
        indirectPrepared_.partByPlanPartIndex.resize(
            static_cast<size_t>(delta->partBegin) +
                delta->partCount,
            noSlot);

        const size_t shadedEnd =
            static_cast<size_t>(delta->shadedItemBegin) +
            delta->shadedItemCount;
        for (size_t itemIndex =
                delta->shadedItemBegin;
                itemIndex < shadedEnd; ++itemIndex) {
            if (renderInterruptedAfter(deadlineWork))
                return false;
            const CadDrawItem& item =
                plan.shadedItems[itemIndex];
            if (!item.instanceCount)
                continue;
            /*
             * Append-only realization batches group by part.  Unique leaves
             * are the scalable fast path; shared occurrence runs retain the
             * exact builder until a run-level append journal is available.
             */
            if (item.instanceCount != 1u ||
                    item.baseInstance < delta->visibleBegin ||
                    item.baseInstance >= newSourceExtent ||
                    item.partIndex < delta->partBegin ||
                    item.partIndex >=
                        static_cast<size_t>(delta->partBegin) +
                            delta->partCount ||
                    item.partIndex >= plan.partBindings.size())
                return fail("item-shape");
            const uint32_t sourceIndex =
                item.baseInstance;
            const CadVisibleInstance& source =
                plan.visibleInstances[sourceIndex];
            if (source.partIndex != item.partIndex)
                return fail("source-part");
            if (!cadInstanceDrawable(
                    plan, item, sourceIndex, CadDrawChannel::Shaded) ||
                    isBoxOutsideExecutorFrustum(
                        source.wbMin, source.wbMax, fp))
                continue;

            const CadPartBinding& binding =
                plan.partBindings[item.partIndex];
            if (!binding.geometry ||
                    !binding.geometry->shaded)
                return fail("missing-geometry");
            const TriMesh& mesh =
                *binding.geometry->shaded;
            uint8_t level = mesh.isProgressive() ?
                cadResolvedProgressiveCut(
                    effectiveProgressiveCut(
                        item.rep.part, source.lodCut),
                    mesh.progressiveMinimumCut,
                    mesh.progressiveResidentCut) :
                15u;
            const size_t vertexCount = mesh.isProgressive() ?
                mesh.positionCountAtCut(level) :
                mesh.positions.size();
            const size_t indexCount = mesh.isProgressive() ?
                mesh.indexCountAtCut(level) :
                mesh.indices.size();
            if (!vertexCount || !indexCount ||
                    vertexCount >
                        std::numeric_limits<uint32_t>::max() ||
                    indexCount >
                        std::numeric_limits<uint32_t>::max())
                return fail("prefix-count");
            uint32_t coverageVertexCount =
                static_cast<uint32_t>(vertexCount);
            uint32_t coverageIndexCount =
                static_cast<uint32_t>(indexCount);
            if (mesh.isProgressive()) {
                coverageVertexCount = static_cast<uint32_t>(
                    mesh.positionCountAtCut(
                        mesh.progressiveMinimumCut));
                coverageIndexCount = static_cast<uint32_t>(
                    mesh.indexCountAtCut(
                        mesh.progressiveMinimumCut));
            }
            const CadTriangleAtlasPart *atlas =
                gpuRes_->upsertTriangleAtlasPart(
                    binding.part, binding.generation,
                    executorPackedVec3fData(mesh.positions),
                    executorPackedVec3fData(mesh.normals),
                    coverageVertexCount, mesh.indices.data(),
                    coverageIndexCount,
                    mesh.isProgressive(), mesh.progressiveLineage,
                    glue, caps_);
            if (atlas &&
                    (atlas->vertexCount < vertexCount ||
                     atlas->indexCount < indexCount)) {
                const CadTriangleAtlasPart *enriched =
                    gpuRes_->upsertTriangleAtlasPart(
                        binding.part, binding.generation,
                        executorPackedVec3fData(mesh.positions),
                        executorPackedVec3fData(mesh.normals),
                        static_cast<uint32_t>(vertexCount),
                        mesh.indices.data(),
                        static_cast<uint32_t>(indexCount),
                        mesh.isProgressive(),
                        mesh.progressiveLineage, glue, caps_);
                if (enriched)
                    atlas = enriched;
            }
            if (!atlas) {
                /*
                 * Atlas pressure is a normal bounded-memory outcome, not an
                 * append-journal failure.  Exact preparation already turns
                 * an eligible unadmitted occurrence into one aggregate
                 * point.  Do the same here so a stream which has reached its
                 * GPU working-set ceiling does not rebuild the entire scene
                 * for every subsequent publication batch.
                 */
                if (!binding.subpixelProxyEligible ||
                        indirectPrepared_.pressureProxyPoints.size() >=
                            std::numeric_limits<uint32_t>::max())
                    return fail("atlas-admission");
                CadSubpixelProxyPoint replacement =
                    cadPressureProxyForInstance(
                        binding, source, viewProj,
                        pressureProxyViewportSize);
                const uint32_t proxyIndex =
                    static_cast<uint32_t>(
                        indirectPrepared_.
                            pressureProxyPoints.size());
                indirectPrepared_.pressureProxyPoints.push_back(
                    replacement);
                indirectPrepared_.
                    pressureProxySourceInstanceIndices.push_back(
                        sourceIndex);
                indirectPrepared_.
                    pressureProxyIndexBySource[sourceIndex] =
                        proxyIndex;
                pressureProxyAdded = true;
                continue;
            }
            while (mesh.isProgressive() &&
                    level > mesh.progressiveMinimumCut &&
                    (mesh.positionCountAtCut(level) >
                         atlas->vertexCount ||
                     mesh.indexCountAtCut(level) >
                         atlas->indexCount))
                --level;
            const size_t residentIndexCount =
                mesh.isProgressive() ?
                    mesh.indexCountAtCut(level) :
                    mesh.indices.size();
            if (!residentIndexCount ||
                    residentIndexCount >
                        std::numeric_limits<uint32_t>::max() ||
                    atlas->vertices.first >
                        static_cast<uint32_t>(
                            std::numeric_limits<int32_t>::max()))
                return fail("resident-count");

            InstVertex target = {};
            std::memcpy(
                target.transform, source.transform.data(),
                16 * sizeof(float));
            target.color[0] = source.rgba[0] / 255.0f;
            target.color[1] = source.rgba[1] / 255.0f;
            target.color[2] = source.rgba[2] / 255.0f;
            target.color[3] = source.rgba[3] / 255.0f;
            const SbVec3f minimum = mesh.isProgressive() ?
                mesh.progressiveQuantizationMinimum :
                SbVec3f(0, 0, 0);
            const SbVec3f maximum = mesh.isProgressive() ?
                mesh.progressiveQuantizationMaximum :
                SbVec3f(0, 0, 0);
            for (int axis = 0; axis < 3; ++axis) {
                target.popMinLevel[axis] = minimum[axis];
                target.popMaxFlags[axis] = maximum[axis];
            }
            target.popMinLevel[3] = packedProgressiveQuantization(
                mesh.isProgressive() ? mesh.quantizationAtCut(level) :
                    ProgressiveQuantization());
            target.popMaxFlags[3] =
                (!mesh.normals.empty() ? 1.0f : 0.0f) +
                (mesh.isProgressive() ? 2.0f : 0.0f);
            const uint32_t packedInstance =
                static_cast<uint32_t>(
                    indirectPrepared_.instances.size());
            indirectPrepared_.instances.push_back(target);
            indirectPrepared_.
                sourceInstanceIndices.push_back(sourceIndex);
            indirectPrepared_.
                instanceIndexBySource[sourceIndex] =
                    packedInstance;

            IndirectPageWork *pageWork = nullptr;
            for (IndirectPageWork& candidate :
                    indirectPrepared_.pages) {
                if (candidate.page == atlas->page) {
                    pageWork = &candidate;
                    break;
                }
            }
            if (!pageWork) {
                IndirectPageWork work;
                work.page = atlas->page;
                indirectPrepared_.pages.push_back(
                    std::move(work));
                pageWork =
                    &indirectPrepared_.pages.back();
            }
            auto& commands = item.cullBackfaces ?
                pageWork->culled : pageWork->ordinary;
            CadDrawElementsIndirectCommand command;
            command.count =
                static_cast<uint32_t>(
                    residentIndexCount);
            command.instanceCount = 1u;
            command.firstIndex =
                atlas->indices.first;
            command.baseVertex =
                static_cast<int32_t>(
                    atlas->vertices.first);
            command.baseInstance =
                packedInstance;
            const uint32_t commandIndex =
                static_cast<uint32_t>(
                    commands.size());
            commands.push_back(command);

            IndirectPreparedPart demand;
            demand.part = binding.part;
            demand.partIndex = item.partIndex;
            demand.generation =
                binding.generation;
            demand.vertexCount = std::min(
                static_cast<uint32_t>(vertexCount),
                atlas->vertexCount);
            demand.indexCount = std::min(
                static_cast<uint32_t>(indexCount),
                atlas->indexCount);
            demand.admissionPressure =
                vertexCount > atlas->vertexCount ||
                indexCount > atlas->indexCount;
            if (demand.admissionPressure)
                ++indirectPrepared_.atlasPressurePartCount;
            demand.page = atlas->page;
            demand.vertexFirst =
                atlas->vertices.first;
            demand.indexFirst =
                atlas->indices.first;
            demand.hasNormals =
                !mesh.normals.empty();
            demand.packedInstance =
                packedInstance;
            demand.commandIndex =
                commandIndex;
            demand.commandCulled =
                item.cullBackfaces;
            indirectPrepared_.
                partByPlanPartIndex[item.partIndex] =
                    static_cast<uint32_t>(
                        indirectPrepared_.parts.size());
            indirectPrepared_.parts.push_back(
                demand);
            indirectPrepared_.renderedTriangleCount +=
                residentIndexCount / 3u;
            cadAccumulateRenderedShadedWork(
                indirectPrepared_.renderedWork, mesh, level,
                static_cast<uint64_t>(residentIndexCount / 3u));
        }
        sourceExtent = newSourceExtent;
    }

    const size_t appendedPackedCount =
        indirectPrepared_.instances.size() -
        priorPackedInstanceCount;
    if (appendedPackedCount) {
        const GLintptr byteOffset =
            static_cast<GLintptr>(
                priorPackedInstanceCount *
                sizeof(InstVertex));
        const GLsizeiptr byteCount =
            static_cast<GLsizeiptr>(
                appendedPackedCount *
                sizeof(InstVertex));
        bool uploaded =
            gpuRes_->instanceVbo() &&
            gpuRes_->instanceUploadSerial() ==
                indirectPrepared_.instanceUploadSerial &&
            gpuRes_->appendInstanceData(
                byteOffset,
                indirectPrepared_.instances.data() +
                    priorPackedInstanceCount,
                byteCount, glue);
        if (!uploaded) {
            gpuRes_->uploadInstanceData(
                indirectPrepared_.instances.data(),
                static_cast<GLsizeiptr>(
                    indirectPrepared_.instances.size() *
                    sizeof(InstVertex)),
                glue);
            uploaded =
                gpuRes_->instanceVbo() != 0;
        }
        if (!uploaded)
            return fail("instance-upload");
        indirectPrepared_.instanceUploadSerial =
            gpuRes_->instanceUploadSerial();
    }
    if (pressureProxyAdded) {
        pressureProxyAppendBaseRevision_ =
            pressureProxyRevision_;
        pressureProxyAppendBegin_ =
            priorPressureProxyCount;
        pressureProxyAppendOnly_ = true;
        ++pressureProxyRevision_;
        if (!pressureProxyRevision_)
            pressureProxyRevision_ = 1;
    }

    indirectPrepared_.appendRevision =
        plan.appendRevision;
    indirectPrepared_.planRevision =
        plan.revision;
    indirectPrepared_.geometryRevision =
        plan.geometryRevision;
    indirectPrepared_.shadedLayoutRevision =
        plan.shadedLayoutRevision;
    indirectPrepared_.subpixelProxyRevision =
        plan.subpixelProxyRevision;
    indirectPrepared_.atlasRevision =
        gpuRes_->triangleAtlasRevision();
    indirectPrepared_.atlasValidationCountdown =
        configuration_->atlasValidationIntervalFrames;
    indirectPrepared_.atlasValidationActive = false;
    indirectPrepared_.atlasValidationCursor = 0u;
    indirectPrepared_.atlasValidationRevision = 0u;
    indirectPrepared_.atlasAdmissionPressure =
        indirectPrepared_.atlasPressurePartCount > 0 ||
        !indirectPrepared_.pressureProxyPoints.empty();
    return true;
}

bool CadRendererGL::patchIndirectPreparedGeometry(
        const CadFramePlan& plan,
        const SoGLContext *glue)
{
    size_t deadlineWork = 256u;
    if (renderInterruptedAfter(deadlineWork))
        return false;
    const auto fail = [&](const char *reason) {
        if (configuration_->patchDebug)
            std::fprintf(stderr,
                "CadRendererGL geometry patch detail reason=%s\n",
                reason ? reason : "unknown");
        return false;
    };
    if (indirectPrepared_.partGeometryRevision ==
            plan.partGeometryRevision)
        return true;
    if (!glue || !gpuRes_ ||
            indirectPrepared_.partGeometryRevision <
                plan.partGeometryDeltaFloorRevision)
        return fail("journal-floor");

    std::vector<CadPartGeometryRange> changedRanges;
    for (const CadPartGeometryDelta& delta :
            plan.partGeometryDeltas) {
        if (renderInterruptedAfter(deadlineWork))
            return false;
        if (delta.revision <=
                indirectPrepared_.partGeometryRevision)
            continue;
        changedRanges.insert(
            changedRanges.end(),
            delta.ranges.begin(), delta.ranges.end());
    }
    if (changedRanges.empty())
        return fail("empty-journal");
    std::sort(changedRanges.begin(), changedRanges.end(),
        [](const auto& left, const auto& right) {
            if (left.partIndex != right.partIndex)
                return left.partIndex < right.partIndex;
            return left.baseInstance < right.baseInstance;
        });
    changedRanges.erase(
        std::unique(changedRanges.begin(), changedRanges.end(),
            [](const auto& left, const auto& right) {
                return left.partIndex == right.partIndex &&
                    left.baseInstance == right.baseInstance;
            }),
        changedRanges.end());

    const uint32_t noSlot =
        std::numeric_limits<uint32_t>::max();
    const ExecutorFrustumPlanes fp =
        extractExecutorFrustumPlanes(
            indirectPrepared_.viewProj);
    std::vector<uint32_t> changedPackedInstances;
    changedPackedInstances.reserve(changedRanges.size());
    gpuRes_->deferTriangleAtlasReclamation();

    for (const CadPartGeometryRange& range :
            changedRanges) {
        if (renderInterruptedAfter(deadlineWork))
            return false;
        /*
         * Wire/point-only part updates are consumed by their own retained
         * paths and do not alter this shaded indirect submission.
         */
        if (!range.shadedItemCount)
            continue;
        if (range.instanceCount != 1u ||
                range.shadedItemCount != 1u ||
                range.baseInstance >=
                    plan.visibleInstances.size() ||
                range.partIndex >=
                    plan.partBindings.size() ||
                range.shadedItemBegin >=
                    plan.shadedItems.size() ||
                range.partIndex >=
                    indirectPrepared_.
                        partByPlanPartIndex.size())
            return fail("range-shape");
        const uint32_t sourceIndex =
            range.baseInstance;
        const CadVisibleInstance& source =
            plan.visibleInstances[sourceIndex];
        const uint32_t preparedPartIndex =
            indirectPrepared_.
                partByPlanPartIndex[range.partIndex];
        const bool shouldDraw =
            !(source.flags & CadInstanceHidden) &&
            !cadInstanceSubpixelReplaced(
                plan, sourceIndex) &&
            !isBoxOutsideExecutorFrustum(
                source.wbMin, source.wbMax, fp);
        if (!shouldDraw) {
            if (preparedPartIndex != noSlot)
                return fail("demotion");
            continue;
        }
        if (preparedPartIndex == noSlot ||
                preparedPartIndex >=
                    indirectPrepared_.parts.size()) {
            /*
             * An exact admission pass may intentionally represent this
             * occurrence with the aggregate pressure proxy.  A richer PoP
             * generation does not promote it past the scene budget; retain
             * the proxy and consume the geometry journal entry.
             */
            if (sourceIndex <
                    indirectPrepared_.
                        pressureProxyIndexBySource.size() &&
                    indirectPrepared_.
                        pressureProxyIndexBySource[sourceIndex] != noSlot)
                continue;
            return fail("promotion");
        }
        IndirectPreparedPart& demand =
            indirectPrepared_.parts[
                preparedPartIndex];
        const CadPartBinding& binding =
            plan.partBindings[range.partIndex];
        if (!(demand.part == binding.part) ||
                demand.partIndex != range.partIndex ||
                demand.packedInstance == noSlot ||
                demand.packedInstance >=
                    indirectPrepared_.instances.size() ||
                demand.commandIndex == noSlot ||
                !binding.geometry ||
                !binding.geometry->shaded)
            return fail("prepared-binding");
        const TriMesh& mesh =
            *binding.geometry->shaded;
        uint8_t level = mesh.isProgressive() ?
            cadResolvedProgressiveCut(
                effectiveProgressiveCut(
                    binding.part, source.lodCut),
                mesh.progressiveMinimumCut,
                mesh.progressiveResidentCut) :
            15u;
        const size_t vertexCount = mesh.isProgressive() ?
            mesh.positionCountAtCut(level) :
            mesh.positions.size();
        const size_t indexCount = mesh.isProgressive() ?
            mesh.indexCountAtCut(level) :
            mesh.indices.size();
        if (!vertexCount || !indexCount ||
                vertexCount >
                    std::numeric_limits<uint32_t>::max() ||
                indexCount >
                    std::numeric_limits<uint32_t>::max())
            return fail("counts");
        const CadTriangleAtlasPart *atlas =
            gpuRes_->upsertTriangleAtlasPart(
                binding.part, binding.generation,
                executorPackedVec3fData(mesh.positions),
                executorPackedVec3fData(mesh.normals),
                static_cast<uint32_t>(vertexCount),
                mesh.indices.data(),
                static_cast<uint32_t>(indexCount),
                mesh.isProgressive(), mesh.progressiveLineage,
                glue, caps_);
        if (!atlas)
            return fail("atlas-admission");
        while (mesh.isProgressive() &&
                level > mesh.progressiveMinimumCut &&
                (mesh.positionCountAtCut(level) >
                     atlas->vertexCount ||
                 mesh.indexCountAtCut(level) >
                     atlas->indexCount))
            --level;
        const size_t residentIndexCount =
            mesh.isProgressive() ?
                mesh.indexCountAtCut(level) :
                mesh.indices.size();
        if (!residentIndexCount ||
                residentIndexCount >
                    std::numeric_limits<uint32_t>::max() ||
                atlas->vertices.first >
                    static_cast<uint32_t>(
                        std::numeric_limits<int32_t>::max()))
            return fail("resident-count");

        IndirectPageWork *oldPageWork = nullptr;
        for (IndirectPageWork& candidate :
                indirectPrepared_.pages) {
            if (candidate.page == demand.page) {
                oldPageWork = &candidate;
                break;
            }
        }
        if (!oldPageWork)
            return fail("old-page");
        auto& oldCommands = demand.commandCulled ?
            oldPageWork->culled :
            oldPageWork->ordinary;
        if (demand.commandIndex >=
                oldCommands.size())
            return fail("old-command-index");
        CadDrawElementsIndirectCommand& oldCommand =
            oldCommands[demand.commandIndex];
        if (oldCommand.instanceCount != 1u ||
                oldCommand.baseInstance !=
                    demand.packedInstance)
            return fail("old-command-shape");
        const uint64_t oldTriangles =
            oldCommand.count / 3u;
        const uint64_t oldPositions = demand.vertexCount;
        const bool oldHasNormals = demand.hasNormals;

        IndirectPageWork *newPageWork = nullptr;
        for (IndirectPageWork& candidate :
                indirectPrepared_.pages) {
            if (candidate.page == atlas->page) {
                newPageWork = &candidate;
                break;
            }
        }
        if (!newPageWork) {
            IndirectPageWork work;
            work.page = atlas->page;
            indirectPrepared_.pages.push_back(
                std::move(work));
            newPageWork =
                &indirectPrepared_.pages.back();
            /*
             * The outer-vector append may have invalidated oldPageWork.
             * Resolve it again before retiring the preceding command.
             */
            oldPageWork = nullptr;
            for (IndirectPageWork& candidate :
                    indirectPrepared_.pages) {
                if (candidate.page == demand.page) {
                    oldPageWork = &candidate;
                    break;
                }
            }
            if (!oldPageWork)
                return fail("old-page-relocation");
        }
        auto& refreshedOldCommands =
            demand.commandCulled ?
                oldPageWork->culled :
                oldPageWork->ordinary;
        if (demand.commandIndex >=
                refreshedOldCommands.size())
            return fail("refreshed-command-index");
        refreshedOldCommands[
            demand.commandIndex].instanceCount = 0u;

        const bool commandCulled =
            plan.shadedItems[
                range.shadedItemBegin].cullBackfaces;
        auto& newCommands = commandCulled ?
            newPageWork->culled :
            newPageWork->ordinary;
        CadDrawElementsIndirectCommand command;
        command.count =
            static_cast<uint32_t>(
                residentIndexCount);
        command.instanceCount = 1u;
        command.firstIndex =
            atlas->indices.first;
        command.baseVertex =
            static_cast<int32_t>(
                atlas->vertices.first);
        command.baseInstance =
            demand.packedInstance;
        const uint32_t commandIndex =
            static_cast<uint32_t>(
                newCommands.size());
        newCommands.push_back(command);

        InstVertex& target =
            indirectPrepared_.instances[
                demand.packedInstance];
        const SbVec3f minimum = mesh.isProgressive() ?
            mesh.progressiveQuantizationMinimum :
            SbVec3f(0, 0, 0);
        const SbVec3f maximum = mesh.isProgressive() ?
            mesh.progressiveQuantizationMaximum :
            SbVec3f(0, 0, 0);
        for (int axis = 0; axis < 3; ++axis) {
            target.popMinLevel[axis] = minimum[axis];
            target.popMaxFlags[axis] = maximum[axis];
        }
        target.popMinLevel[3] = packedProgressiveQuantization(
            mesh.isProgressive() ? mesh.quantizationAtCut(level) :
                ProgressiveQuantization());
        target.popMaxFlags[3] =
            (!mesh.normals.empty() ? 1.0f : 0.0f) +
            (mesh.isProgressive() ? 2.0f : 0.0f);
        changedPackedInstances.push_back(
            demand.packedInstance);

        demand.generation =
            binding.generation;
        const bool admissionPressure =
            vertexCount > atlas->vertexCount ||
            indexCount > atlas->indexCount;
        if (demand.admissionPressure != admissionPressure) {
            if (admissionPressure) {
                ++indirectPrepared_.atlasPressurePartCount;
            } else if (indirectPrepared_.atlasPressurePartCount) {
                --indirectPrepared_.atlasPressurePartCount;
            }
            demand.admissionPressure = admissionPressure;
        }
        demand.vertexCount = std::min(
            static_cast<uint32_t>(vertexCount),
            atlas->vertexCount);
        demand.indexCount = std::min(
            static_cast<uint32_t>(indexCount),
            atlas->indexCount);
        demand.page = atlas->page;
        demand.vertexFirst =
            atlas->vertices.first;
        demand.indexFirst =
            atlas->indices.first;
        demand.hasNormals =
            !mesh.normals.empty();
        demand.commandIndex =
            commandIndex;
        demand.commandCulled =
            commandCulled;
        const uint64_t newTriangles =
            residentIndexCount / 3u;
        indirectPrepared_.renderedTriangleCount =
            oldTriangles <=
                    indirectPrepared_.
                        renderedTriangleCount ?
                indirectPrepared_.
                    renderedTriangleCount -
                    oldTriangles + newTriangles :
                newTriangles;
        cadReplacePreparedShadedWork(
            indirectPrepared_.renderedWork,
            oldTriangles, oldPositions, oldHasNormals,
            newTriangles,
            static_cast<uint64_t>(
                mesh.isProgressive() ?
                    mesh.positionCountAtCut(level) :
                    mesh.positions.size()),
            !mesh.normals.empty());
    }

    indirectPrepared_.pages.erase(
        std::remove_if(
            indirectPrepared_.pages.begin(),
            indirectPrepared_.pages.end(),
            [&](const IndirectPageWork& work) {
                if (!gpuRes_->triangleAtlasPage(
                        work.page))
                    return true;
                const auto active =
                    [](const auto& commands) {
                        return std::any_of(
                            commands.begin(),
                            commands.end(),
                            [](const auto& command) {
                                return command.instanceCount != 0u;
                            });
                    };
                return !active(work.ordinary) &&
                    !active(work.culled);
            }),
        indirectPrepared_.pages.end());

    std::sort(changedPackedInstances.begin(),
        changedPackedInstances.end());
    changedPackedInstances.erase(
        std::unique(changedPackedInstances.begin(),
            changedPackedInstances.end()),
        changedPackedInstances.end());
    bool sparseUpload =
        changedPackedInstances.empty() ||
        (gpuRes_->instanceVbo() &&
         gpuRes_->instanceUploadSerial() ==
             indirectPrepared_.instanceUploadSerial);
    size_t begin = 0;
    while (sparseUpload &&
            begin < changedPackedInstances.size()) {
        if (renderInterruptedAfter(deadlineWork))
            return false;
        size_t end = begin + 1u;
        while (end < changedPackedInstances.size() &&
                changedPackedInstances[end] ==
                    changedPackedInstances[end - 1u] + 1u)
            ++end;
        const uint32_t first =
            changedPackedInstances[begin];
        sparseUpload = gpuRes_->updateInstanceData(
            static_cast<GLintptr>(first) *
                sizeof(InstVertex),
            indirectPrepared_.instances.data() + first,
            static_cast<GLsizeiptr>(end - begin) *
                sizeof(InstVertex),
            glue);
        begin = end;
    }
    if (sparseUpload)
        indirectPrepared_.instanceUploadSerial =
            gpuRes_->instanceUploadSerial();
    else
        indirectPrepared_.instanceUploadSerial = 0;

    indirectPrepared_.partGeometryRevision =
        plan.partGeometryRevision;
    indirectPrepared_.planRevision =
        plan.revision;
    indirectPrepared_.geometryRevision =
        plan.geometryRevision;
    indirectPrepared_.shadedLayoutRevision =
        plan.shadedLayoutRevision;
    indirectPrepared_.subpixelProxyRevision =
        plan.subpixelProxyRevision;
    indirectPrepared_.atlasRevision =
        gpuRes_->triangleAtlasRevision();
    indirectPrepared_.atlasValidationCountdown =
        configuration_->atlasValidationIntervalFrames;
    indirectPrepared_.atlasValidationActive = false;
    indirectPrepared_.atlasValidationCursor = 0u;
    indirectPrepared_.atlasValidationRevision = 0u;
    indirectPrepared_.atlasAdmissionPressure =
        indirectPrepared_.atlasPressurePartCount > 0 ||
        !indirectPrepared_.pressureProxyPoints.empty();
    return true;
}

bool CadRendererGL::patchIndirectPreparedCeiling(
        const CadFramePlan& plan,
        const SoGLContext *glue)
{
    size_t deadlineWork = 256u;
    if (renderInterruptedAfter(deadlineWork))
        return false;
    const int ceiling =
        activeViewState().progressiveCutCeiling;
    const float nextFraction =
        activeViewState().progressiveCutNextFraction;
    if (indirectPrepared_.progressiveCutCeiling ==
            ceiling &&
            indirectPrepared_.progressiveCutNextFraction ==
                nextFraction)
        return true;
    if (!glue || !gpuRes_)
        return false;

    const uint32_t noSlot =
        std::numeric_limits<uint32_t>::max();
    std::vector<uint32_t> changedPackedInstances;
    changedPackedInstances.reserve(
        indirectPrepared_.parts.size());
    for (IndirectPreparedPart& demand :
            indirectPrepared_.parts) {
        if (renderInterruptedAfter(deadlineWork))
            return false;
        if (demand.partIndex >=
                plan.partBindings.size())
            return false;
        const CadPartBinding& binding =
            plan.partBindings[demand.partIndex];
        if (!(binding.part == demand.part) ||
                !binding.geometry ||
                !binding.geometry->shaded)
            return false;
        const TriMesh& mesh =
            *binding.geometry->shaded;
        if (!mesh.isProgressive())
            continue;
        if (demand.packedInstance == noSlot ||
                demand.packedInstance >=
                    indirectPrepared_.instances.size() ||
                demand.packedInstance >=
                    indirectPrepared_.
                        sourceInstanceIndices.size() ||
                demand.commandIndex == noSlot)
            return false;
        const uint32_t sourceIndex =
            indirectPrepared_.
                sourceInstanceIndices[
                    demand.packedInstance];
        if (sourceIndex >=
                plan.visibleInstances.size())
            return false;
        uint8_t level = cadResolvedProgressiveCut(
            effectiveProgressiveCut(
                binding.part,
                plan.visibleInstances[sourceIndex].lodCut),
            mesh.progressiveMinimumCut,
            mesh.progressiveResidentCut);
        const CadTriangleAtlasPart *atlas =
            gpuRes_->triangleAtlasPart(
                binding.part);
        if (!atlas || atlas->page != demand.page ||
                atlas->vertices.first !=
                    demand.vertexFirst ||
                atlas->indices.first !=
                    demand.indexFirst)
            return false;
        const uint32_t requestedVertices = static_cast<uint32_t>(
            mesh.positionCountAtCut(level));
        const uint32_t requestedIndices = static_cast<uint32_t>(
            mesh.indexCountAtCut(level));
        const bool admissionPressure =
            requestedVertices > atlas->vertexCount ||
            requestedIndices > atlas->indexCount;
        if (demand.admissionPressure != admissionPressure) {
            if (admissionPressure) {
                ++indirectPrepared_.atlasPressurePartCount;
            } else if (indirectPrepared_.atlasPressurePartCount) {
                --indirectPrepared_.atlasPressurePartCount;
            }
            demand.admissionPressure = admissionPressure;
        }
        while (level > mesh.progressiveMinimumCut &&
                (mesh.positionCountAtCut(level) >
                     atlas->vertexCount ||
                 mesh.indexCountAtCut(level) >
                     atlas->indexCount))
            --level;
        const size_t commandCount =
            mesh.indexCountAtCut(level);
        if (!commandCount ||
                commandCount >
                    std::numeric_limits<uint32_t>::max())
            return false;
        IndirectPageWork *pageWork = nullptr;
        for (IndirectPageWork& candidate :
                indirectPrepared_.pages) {
            if (candidate.page == demand.page) {
                pageWork = &candidate;
                break;
            }
        }
        if (!pageWork)
            return false;
        auto& commands = demand.commandCulled ?
            pageWork->culled : pageWork->ordinary;
        if (demand.commandIndex >=
                commands.size())
            return false;
        CadDrawElementsIndirectCommand& command =
            commands[demand.commandIndex];
        if (command.instanceCount != 1u ||
                command.baseInstance !=
                    demand.packedInstance)
            return false;
        const uint64_t oldTriangles =
            command.count / 3u;
        const uint64_t newTriangles =
            commandCount / 3u;
        const uint64_t oldPositions = demand.vertexCount;
        const uint64_t newPositions =
            static_cast<uint64_t>(mesh.positionCountAtCut(level));
        indirectPrepared_.renderedTriangleCount =
            oldTriangles <=
                    indirectPrepared_.
                        renderedTriangleCount ?
                indirectPrepared_.
                    renderedTriangleCount -
                    oldTriangles + newTriangles :
                newTriangles;
        cadReplacePreparedShadedWork(
            indirectPrepared_.renderedWork,
            oldTriangles, oldPositions, demand.hasNormals,
            newTriangles, newPositions, demand.hasNormals);
        command.count =
            static_cast<uint32_t>(commandCount);
        indirectPrepared_.instances[
            demand.packedInstance].
                popMinLevel[3] =
                    packedProgressiveQuantization(
                        mesh.quantizationAtCut(level));
        demand.vertexCount =
            static_cast<uint32_t>(
                mesh.positionCountAtCut(level));
        demand.indexCount =
            static_cast<uint32_t>(commandCount);
        changedPackedInstances.push_back(
            demand.packedInstance);
    }

    std::sort(changedPackedInstances.begin(),
        changedPackedInstances.end());
    changedPackedInstances.erase(
        std::unique(changedPackedInstances.begin(),
            changedPackedInstances.end()),
        changedPackedInstances.end());
    bool sparseUpload =
        changedPackedInstances.empty() ||
        (gpuRes_->instanceVbo() &&
         gpuRes_->instanceUploadSerial() ==
             indirectPrepared_.instanceUploadSerial);
    size_t begin = 0;
    while (sparseUpload &&
            begin < changedPackedInstances.size()) {
        if (renderInterruptedAfter(deadlineWork))
            return false;
        size_t end = begin + 1u;
        while (end < changedPackedInstances.size() &&
                changedPackedInstances[end] ==
                    changedPackedInstances[end - 1u] + 1u)
            ++end;
        const uint32_t first =
            changedPackedInstances[begin];
        sparseUpload = gpuRes_->updateInstanceData(
            static_cast<GLintptr>(first) *
                sizeof(InstVertex),
            indirectPrepared_.instances.data() + first,
            static_cast<GLsizeiptr>(end - begin) *
                sizeof(InstVertex),
            glue);
        begin = end;
    }
    if (sparseUpload)
        indirectPrepared_.instanceUploadSerial =
            gpuRes_->instanceUploadSerial();
    else
        indirectPrepared_.instanceUploadSerial = 0;
    indirectPrepared_.progressiveCutCeiling =
        ceiling;
    indirectPrepared_.progressiveCutNextFraction =
        nextFraction;
    indirectPrepared_.atlasAdmissionPressure =
        indirectPrepared_.atlasPressurePartCount > 0 ||
        !indirectPrepared_.pressureProxyPoints.empty();
    return true;
}

bool CadRendererGL::patchIndirectPreparedCuts(
        const CadFramePlan& plan,
        const SoGLContext *glue)
{
    size_t deadlineWork = 256u;
    if (renderInterruptedAfter(deadlineWork))
        return false;
    if (indirectPrepared_.shadedLodRevision ==
            plan.shadedLodRevision)
        return true;
    if (!glue || !gpuRes_ ||
            indirectPrepared_.shadedLodRevision <
                plan.shadedLodDeltaFloorRevision)
        return false;

    std::vector<CadShadedLodRange> changedRanges;
    for (const CadShadedLodDelta& delta : plan.shadedLodDeltas) {
        if (renderInterruptedAfter(deadlineWork))
            return false;
        if (delta.revision <=
                indirectPrepared_.shadedLodRevision)
            continue;
        changedRanges.insert(
            changedRanges.end(),
            delta.ranges.begin(), delta.ranges.end());
    }
    if (changedRanges.empty())
        return false;
    std::sort(changedRanges.begin(), changedRanges.end(),
        [](const auto& left, const auto& right) {
            if (left.partIndex != right.partIndex)
                return left.partIndex < right.partIndex;
            return left.baseInstance < right.baseInstance;
        });
    changedRanges.erase(
        std::unique(changedRanges.begin(), changedRanges.end(),
            [](const auto& left, const auto& right) {
                return left.partIndex == right.partIndex &&
                    left.baseInstance == right.baseInstance;
            }),
        changedRanges.end());

    const uint32_t noSlot =
        std::numeric_limits<uint32_t>::max();
    std::vector<uint32_t> changedPackedInstances;
    changedPackedInstances.reserve(changedRanges.size());
    for (const CadShadedLodRange& range : changedRanges) {
        if (renderInterruptedAfter(deadlineWork))
            return false;
        /*
         * The high-asset-count vehicle case overwhelmingly consists of
         * unique leaves.  Their prepared instance and command slots are
         * invariant under a level change.  A shared part may reorder several
         * occurrences across level bins; retain the exact path for that
         * materially different operation until it has its own run journal.
         */
        if (range.instanceCount != 1u ||
                range.baseInstance >=
                    plan.visibleInstances.size() ||
                range.partIndex >= plan.partBindings.size() ||
                range.shadedItemBegin >= plan.shadedItems.size() ||
                range.shadedItemCount != 1u)
            return false;
        const uint32_t sourceIndex = range.baseInstance;
        const CadVisibleInstance& source =
            plan.visibleInstances[sourceIndex];
        if (source.partIndex != range.partIndex)
            return false;

        if (range.partIndex >=
                indirectPrepared_.partByPlanPartIndex.size())
            return false;
        const uint32_t preparedPartIndex =
            indirectPrepared_.
                partByPlanPartIndex[range.partIndex];
        if (preparedPartIndex == noSlot) {
            /*
             * The sole occurrence was outside the prepared view, collapsed
             * into a point, or represented by a pressure proxy.  Its PoP cut
             * cannot affect this retained submission.
             */
            if (sourceIndex <
                    indirectPrepared_.instanceIndexBySource.size() &&
                    indirectPrepared_.
                        instanceIndexBySource[sourceIndex] != noSlot)
                return false;
            continue;
        }
        if (preparedPartIndex >=
                indirectPrepared_.parts.size())
            return false;
        IndirectPreparedPart& demand =
            indirectPrepared_.parts[preparedPartIndex];
        const CadPartBinding& binding =
            plan.partBindings[range.partIndex];
        if (!(demand.part == binding.part) ||
                demand.partIndex != range.partIndex ||
                demand.generation != binding.generation ||
                demand.packedInstance == noSlot ||
                demand.packedInstance >=
                    indirectPrepared_.instances.size() ||
                demand.commandIndex == noSlot ||
                demand.packedInstance >=
                    indirectPrepared_.
                        sourceInstanceIndices.size() ||
                indirectPrepared_.
                    sourceInstanceIndices[demand.packedInstance] !=
                        sourceIndex)
            return false;
        if (!binding.geometry || !binding.geometry->shaded ||
                !binding.geometry->shaded->isProgressive())
            return false;
        const TriMesh& mesh = *binding.geometry->shaded;
        uint8_t level = cadResolvedProgressiveCut(
            effectiveProgressiveCut(
                binding.part, source.lodCut),
            mesh.progressiveMinimumCut,
            mesh.progressiveResidentCut);
        const uint32_t requestedVertices =
            static_cast<uint32_t>(
                mesh.positionCountAtCut(level));
        const uint32_t requestedIndices =
            static_cast<uint32_t>(
                mesh.indexCountAtCut(level));
        if (!requestedVertices || !requestedIndices)
            return false;

        const CadTriangleAtlasPart *atlas =
            gpuRes_->touchTriangleAtlasPart(
                binding.part, binding.generation,
                !mesh.normals.empty(),
                requestedVertices, requestedIndices);
        if (!atlas)
            atlas = gpuRes_->upsertTriangleAtlasPart(
                binding.part, binding.generation,
                executorPackedVec3fData(mesh.positions),
                executorPackedVec3fData(mesh.normals),
                requestedVertices, mesh.indices.data(),
                requestedIndices, true, mesh.progressiveLineage,
                glue, caps_);
        if (!atlas || atlas->page != demand.page ||
                atlas->vertices.first != demand.vertexFirst ||
                atlas->indices.first != demand.indexFirst)
            return false;

        const bool admissionPressure =
            requestedVertices > atlas->vertexCount ||
            requestedIndices > atlas->indexCount;
        if (demand.admissionPressure != admissionPressure) {
            if (admissionPressure) {
                ++indirectPrepared_.atlasPressurePartCount;
            } else if (indirectPrepared_.atlasPressurePartCount) {
                --indirectPrepared_.atlasPressurePartCount;
            }
            demand.admissionPressure = admissionPressure;
        }

        while (level > mesh.progressiveMinimumCut &&
                (mesh.positionCountAtCut(level) >
                     atlas->vertexCount ||
                 mesh.indexCountAtCut(level) >
                     atlas->indexCount))
            --level;
        const size_t commandCount =
            mesh.indexCountAtCut(level);
        if (!commandCount ||
                commandCount >
                    std::numeric_limits<uint32_t>::max())
            return false;

        IndirectPageWork *pageWork = nullptr;
        for (IndirectPageWork& candidate :
                indirectPrepared_.pages) {
            if (candidate.page == demand.page) {
                pageWork = &candidate;
                break;
            }
        }
        if (!pageWork)
            return false;
        auto& commands = demand.commandCulled ?
            pageWork->culled : pageWork->ordinary;
        if (demand.commandIndex >= commands.size())
            return false;
        CadDrawElementsIndirectCommand& command =
            commands[demand.commandIndex];
        if (command.instanceCount != 1u ||
                command.baseInstance !=
                    demand.packedInstance)
            return false;

        const uint64_t oldTriangles =
            static_cast<uint64_t>(command.count / 3u);
        const uint64_t newTriangles =
            static_cast<uint64_t>(commandCount / 3u);
        const uint64_t oldPositions = demand.vertexCount;
        const uint64_t newPositions =
            static_cast<uint64_t>(mesh.positionCountAtCut(level));
        indirectPrepared_.renderedTriangleCount =
            oldTriangles <=
                    indirectPrepared_.renderedTriangleCount ?
                indirectPrepared_.renderedTriangleCount -
                    oldTriangles + newTriangles :
                newTriangles;
        cadReplacePreparedShadedWork(
            indirectPrepared_.renderedWork,
            oldTriangles, oldPositions, demand.hasNormals,
            newTriangles, newPositions, demand.hasNormals);
        command.count =
            static_cast<uint32_t>(commandCount);
        InstVertex& target =
            indirectPrepared_.instances[
                demand.packedInstance];
        target.popMinLevel[3] =
            packedProgressiveQuantization(
                mesh.quantizationAtCut(level));
        demand.vertexCount = std::min(
            requestedVertices, atlas->vertexCount);
        demand.indexCount = std::min(
            requestedIndices, atlas->indexCount);
        changedPackedInstances.push_back(
            demand.packedInstance);
    }

    std::sort(changedPackedInstances.begin(),
        changedPackedInstances.end());
    changedPackedInstances.erase(
        std::unique(changedPackedInstances.begin(),
            changedPackedInstances.end()),
        changedPackedInstances.end());
    bool sparseUpload =
        changedPackedInstances.empty() ||
        (gpuRes_->instanceVbo() &&
         gpuRes_->instanceUploadSerial() ==
             indirectPrepared_.instanceUploadSerial);
    size_t begin = 0;
    while (sparseUpload &&
            begin < changedPackedInstances.size()) {
        if (renderInterruptedAfter(deadlineWork))
            return false;
        size_t end = begin + 1u;
        while (end < changedPackedInstances.size() &&
                changedPackedInstances[end] ==
                    changedPackedInstances[end - 1u] + 1u)
            ++end;
        const uint32_t first =
            changedPackedInstances[begin];
        sparseUpload = gpuRes_->updateInstanceData(
            static_cast<GLintptr>(first) *
                sizeof(InstVertex),
            indirectPrepared_.instances.data() + first,
            static_cast<GLsizeiptr>(end - begin) *
                sizeof(InstVertex),
            glue);
        begin = end;
    }
    if (sparseUpload)
        indirectPrepared_.instanceUploadSerial =
            gpuRes_->instanceUploadSerial();
    else
        indirectPrepared_.instanceUploadSerial = 0;

    indirectPrepared_.shadedLodRevision =
        plan.shadedLodRevision;
    indirectPrepared_.planRevision = plan.revision;
    indirectPrepared_.atlasRevision =
        gpuRes_->triangleAtlasRevision();
    indirectPrepared_.atlasValidationCountdown =
        configuration_->atlasValidationIntervalFrames;
    indirectPrepared_.atlasValidationActive = false;
    indirectPrepared_.atlasValidationCursor = 0u;
    indirectPrepared_.atlasValidationRevision = 0u;
    indirectPrepared_.atlasAdmissionPressure =
        indirectPrepared_.atlasPressurePartCount > 0 ||
        !indirectPrepared_.pressureProxyPoints.empty();
    return true;
}

bool CadRendererGL::replayIndirectShaded(
        const CadFramePlan& plan,
        const SoGLContext *glue,
        const SbMatrix& viewProj,
        const SbViewVolume& viewVolume)
{
    size_t deadlineWork = 256u;
    if (renderInterruptedAfter(deadlineWork))
        return false;
    const bool cameraChanged = !(indirectPrepared_.viewProj == viewProj);
    const bool subpixelProxyChanged =
        indirectPrepared_.subpixelProxyRevision !=
            plan.subpixelProxyRevision;
    bool mayReplayVisibilityProxyChange = false;
    if (!cameraChanged && subpixelProxyChanged &&
            indirectPrepared_.instanceAttributeRevision !=
                plan.instanceAttributeRevision &&
            indirectPrepared_.instanceAttributeRevision >=
                plan.instanceAttributeDeltaFloorRevision) {
        for (const CadInstanceAttributeDelta& delta :
                plan.instanceAttributeDeltas) {
            if (renderInterruptedAfter(deadlineWork))
                return false;
            if (delta.revision >
                    indirectPrepared_.instanceAttributeRevision &&
                    delta.visibilityChanged) {
                mayReplayVisibilityProxyChange = true;
                break;
            }
        }
    }
    /*
     * Camera motion never makes immutable geometry invalid.  When the view
     * controller has established mesh coverage it asks us to preserve this
     * coherent prepared cut for the whole input burst.  Event-count or timer
     * refreshes injected recurring O(scene) 30-50 ms stalls and made a faster
     * mouse produce worse FPS.  The controller clears the hint for the first
     * quiet frame, or sooner if its bounded coverage pass discovers newly
     * visible geometry which has no mesh presentation.
     */
    const bool mayReuseChangedCamera =
        cameraChanged && activeViewState().cameraMotionFrameReuse;
    const bool appendChanged =
        indirectPrepared_.appendRevision !=
            plan.appendRevision;
    const bool mayPatchAppend =
        appendChanged &&
        indirectPrepared_.appendRevision >=
            plan.appendDeltaFloorRevision;
    const bool partGeometryChanged =
        indirectPrepared_.partGeometryRevision !=
            plan.partGeometryRevision;
    const bool mayPatchPartGeometry =
        partGeometryChanged &&
        indirectPrepared_.partGeometryRevision >=
            plan.partGeometryDeltaFloorRevision;
    if (!indirectPrepared_.valid ||
            indirectPrepared_.contextId != glue->contextid ||
            ((indirectPrepared_.geometryRevision !=
                  plan.geometryRevision ||
              indirectPrepared_.shadedLayoutRevision !=
                  plan.shadedLayoutRevision) &&
                !mayPatchAppend &&
                !mayPatchPartGeometry) ||
            (subpixelProxyChanged &&
                !mayReplayVisibilityProxyChange &&
                !mayPatchAppend &&
                !mayPatchPartGeometry) ||
            (cameraChanged && !mayReuseChangedCamera)) {
        if (configuration_->patchDebug)
            std::fprintf(stderr,
                "CadRendererGL retained patch gate exact "
                "valid=%d context=%d geometry=%d layout=%d "
                "subpixel=%d append_changed=%d append_patch=%d "
                "append_rev=%llu/%llu/%llu "
                "geometry_changed=%d geometry_patch=%d "
                "geometry_rev=%llu/%llu/%llu "
                "camera=%d camera_reuse=%d\n",
                indirectPrepared_.valid ? 1 : 0,
                indirectPrepared_.contextId == glue->contextid ? 1 : 0,
                indirectPrepared_.geometryRevision ==
                    plan.geometryRevision ? 1 : 0,
                indirectPrepared_.shadedLayoutRevision ==
                    plan.shadedLayoutRevision ? 1 : 0,
                subpixelProxyChanged ? 1 : 0,
                appendChanged ? 1 : 0,
                mayPatchAppend ? 1 : 0,
                static_cast<unsigned long long>(
                    indirectPrepared_.appendRevision),
                static_cast<unsigned long long>(
                    plan.appendRevision),
                static_cast<unsigned long long>(
                    plan.appendDeltaFloorRevision),
                partGeometryChanged ? 1 : 0,
                mayPatchPartGeometry ? 1 : 0,
                static_cast<unsigned long long>(
                    indirectPrepared_.partGeometryRevision),
                static_cast<unsigned long long>(
                    plan.partGeometryRevision),
                static_cast<unsigned long long>(
                    plan.partGeometryDeltaFloorRevision),
                cameraChanged ? 1 : 0,
                mayReuseChangedCamera ? 1 : 0);
        indirectPrepared_.valid = false;
        return false;
    }
    if (cameraChanged) {
        ++indirectPrepared_.cameraMotionReplayCount;
        indirectPrepared_.viewProj = viewProj;
    }
    const bool appendPatchEnabled =
        configuration_->appendPatch;
    if (appendChanged)
        noteRenderPreparation("retained-append-patch");
    if (appendChanged &&
            (!appendPatchEnabled ||
             !patchIndirectPreparedAppend(
                plan, glue, viewProj))) {
        if (configuration_->patchDebug)
            std::fprintf(stderr,
                "CadRendererGL retained append patch rejected\n");
        indirectPrepared_.valid = false;
        return false;
    }
    if (renderInterrupted())
        return false;
    const bool geometryPatchEnabled =
        configuration_->geometryPatch;
    if (partGeometryChanged)
        noteRenderPreparation("retained-geometry-patch");
    if (partGeometryChanged &&
            (!geometryPatchEnabled ||
             !patchIndirectPreparedGeometry(
                plan, glue))) {
        if (configuration_->patchDebug)
            std::fprintf(stderr,
                "CadRendererGL retained geometry patch rejected\n");
        indirectPrepared_.valid = false;
        return false;
    }
    if (renderInterrupted())
        return false;
    const bool ceilingChanged =
        indirectPrepared_.progressiveCutCeiling !=
            activeViewState().progressiveCutCeiling ||
        indirectPrepared_.progressiveCutNextFraction !=
            activeViewState().progressiveCutNextFraction;
    if (ceilingChanged)
        noteRenderPreparation("retained-ceiling-patch");
    if (ceilingChanged &&
            !patchIndirectPreparedCeiling(
                plan, glue)) {
        if (configuration_->patchDebug)
            std::fprintf(stderr,
                "CadRendererGL retained ceiling patch rejected\n");
        indirectPrepared_.valid = false;
        return false;
    }
    if (renderInterrupted())
        return false;
    const bool lodPatchEnabled =
        configuration_->lodPatch;
    const bool lodChanged = indirectPrepared_.shadedLodRevision !=
        plan.shadedLodRevision;
    if (lodChanged)
        noteRenderPreparation("retained-lod-patch");
    if (lodChanged &&
            (!lodPatchEnabled ||
             !patchIndirectPreparedCuts(
                plan, glue))) {
        if (configuration_->patchDebug)
            std::fprintf(stderr,
                "CadRendererGL retained LoD patch rejected\n");
        indirectPrepared_.valid = false;
        return false;
    }
    if (renderInterrupted())
        return false;

    if (indirectPrepared_.instanceAttributeRevision !=
            plan.instanceAttributeRevision) {
        noteRenderPreparation("retained-attribute-patch");
        if (indirectPrepared_.sourceInstanceIndices.size() !=
                indirectPrepared_.instances.size() ||
                indirectPrepared_.pressureProxySourceInstanceIndices.size() !=
                indirectPrepared_.pressureProxyPoints.size()) {
            indirectPrepared_.valid = false;
            return false;
        }

        std::vector<uint32_t> changedSourceIndices;
        std::vector<uint32_t> visibilityChangedSourceIndices;
        bool sparseAttributes =
            indirectPrepared_.instanceAttributeRevision >=
                plan.instanceAttributeDeltaFloorRevision;
        if (sparseAttributes) {
            for (const CadInstanceAttributeDelta& delta :
                    plan.instanceAttributeDeltas) {
                if (renderInterruptedAfter(deadlineWork))
                    return false;
                if (delta.revision <=
                        indirectPrepared_.instanceAttributeRevision)
                    continue;
                changedSourceIndices.insert(
                    changedSourceIndices.end(),
                    delta.visibleIndices.begin(),
                    delta.visibleIndices.end());
                if (delta.visibilityChanged)
                    visibilityChangedSourceIndices.insert(
                        visibilityChangedSourceIndices.end(),
                        delta.visibleIndices.begin(),
                        delta.visibleIndices.end());
            }
            std::sort(changedSourceIndices.begin(),
                changedSourceIndices.end());
            changedSourceIndices.erase(
                std::unique(changedSourceIndices.begin(),
                    changedSourceIndices.end()),
                changedSourceIndices.end());
            std::sort(visibilityChangedSourceIndices.begin(),
                visibilityChangedSourceIndices.end());
            visibilityChangedSourceIndices.erase(
                std::unique(
                    visibilityChangedSourceIndices.begin(),
                    visibilityChangedSourceIndices.end()),
                visibilityChangedSourceIndices.end());
            if (changedSourceIndices.empty())
                sparseAttributes = false;
        }

        const auto updatePackedColor =
            [&](uint32_t packedIndex, uint32_t sourceIndex) {
            if (packedIndex >= indirectPrepared_.instances.size() ||
                    sourceIndex >= plan.visibleInstances.size())
                return false;
            const CadVisibleInstance& source =
                plan.visibleInstances[sourceIndex];
            InstVertex& target =
                indirectPrepared_.instances[packedIndex];
            target.color[0] = source.rgba[0] / 255.0f;
            target.color[1] = source.rgba[1] / 255.0f;
            target.color[2] = source.rgba[2] / 255.0f;
            target.color[3] = source.rgba[3] / 255.0f;
            const float geometryFlags =
                std::fmod(target.popMaxFlags[3], 4.0f);
            target.popMaxFlags[3] = geometryFlags +
                ((source.flags & CadInstanceHidden) ? 4.0f : 0.0f);
            return true;
        };
        std::vector<uint32_t> changedPackedIndices;
        bool pressureAttributesChanged = false;
        if (sparseAttributes) {
            changedPackedIndices.reserve(changedSourceIndices.size());
            for (const uint32_t sourceIndex : changedSourceIndices) {
                if (renderInterruptedAfter(deadlineWork))
                    return false;
                if (sourceIndex >= plan.visibleInstances.size() ||
                        sourceIndex >=
                            indirectPrepared_.instanceIndexBySource.size() ||
                        sourceIndex >=
                            indirectPrepared_.
                                pressureProxyIndexBySource.size()) {
                    indirectPrepared_.valid = false;
                    return false;
                }
                const uint32_t packedIndex =
                    indirectPrepared_.instanceIndexBySource[sourceIndex];
                const bool hasPacked = packedIndex !=
                    std::numeric_limits<uint32_t>::max();
                if (hasPacked) {
                    if (!updatePackedColor(packedIndex, sourceIndex)) {
                        indirectPrepared_.valid = false;
                        return false;
                    }
                    changedPackedIndices.push_back(packedIndex);
                }
                const uint32_t proxyIndex =
                    indirectPrepared_.
                        pressureProxyIndexBySource[sourceIndex];
                const bool hasProxy = proxyIndex !=
                    std::numeric_limits<uint32_t>::max();
                if (hasProxy) {
                    if (proxyIndex >=
                            indirectPrepared_.pressureProxyPoints.size()) {
                        indirectPrepared_.valid = false;
                        return false;
                    }
                    indirectPrepared_.pressureProxyPoints[proxyIndex].rgba =
                        plan.visibleInstances[sourceIndex].rgba;
                    indirectPrepared_.pressureProxyPoints[proxyIndex].flags =
                        plan.visibleInstances[sourceIndex].flags;
                    pressureAttributesChanged = true;
                }
                if (!hasPacked && !hasProxy &&
                        !(plan.visibleInstances[sourceIndex].flags &
                            CadInstanceHidden) &&
                        !cadInstanceSubpixelReplaced(plan, sourceIndex) &&
                        std::binary_search(
                            visibilityChangedSourceIndices.begin(),
                            visibilityChangedSourceIndices.end(),
                            sourceIndex)) {
                    /*
                     * This occurrence was absent when the prepared view was
                     * built (initially hidden or out of frame).  Showing it
                     * needs one exact visibility/admission pass.
                     */
                    indirectPrepared_.valid = false;
                    return false;
                }
            }
        } else {
            for (size_t i = 0;
                    i < indirectPrepared_.instances.size(); ++i) {
                if (renderInterruptedAfter(deadlineWork))
                    return false;
                const uint32_t sourceIndex =
                    indirectPrepared_.sourceInstanceIndices[i];
                if (!updatePackedColor(
                        static_cast<uint32_t>(i), sourceIndex)) {
                    indirectPrepared_.valid = false;
                    return false;
                }
            }
            for (size_t i = 0;
                    i < indirectPrepared_.pressureProxyPoints.size(); ++i) {
                if (renderInterruptedAfter(deadlineWork))
                    return false;
                const uint32_t sourceIndex =
                    indirectPrepared_.
                        pressureProxySourceInstanceIndices[i];
                if (sourceIndex >= plan.visibleInstances.size()) {
                    indirectPrepared_.valid = false;
                    return false;
                }
                indirectPrepared_.pressureProxyPoints[i].rgba =
                    plan.visibleInstances[sourceIndex].rgba;
                indirectPrepared_.pressureProxyPoints[i].flags =
                    plan.visibleInstances[sourceIndex].flags;
                pressureAttributesChanged = true;
            }
        }
        indirectPrepared_.instanceAttributeRevision =
            plan.instanceAttributeRevision;
        if (mayReplayVisibilityProxyChange)
            indirectPrepared_.subpixelProxyRevision =
                plan.subpixelProxyRevision;
        indirectPrepared_.planRevision = plan.revision;

        bool sparseUpload = sparseAttributes &&
            gpuRes_->instanceVbo() &&
            gpuRes_->instanceUploadSerial() ==
                indirectPrepared_.instanceUploadSerial;
        if (sparseUpload && !changedPackedIndices.empty()) {
            std::sort(changedPackedIndices.begin(),
                changedPackedIndices.end());
            changedPackedIndices.erase(
                std::unique(changedPackedIndices.begin(),
                    changedPackedIndices.end()),
                changedPackedIndices.end());
            size_t begin = 0;
            while (sparseUpload &&
                    begin < changedPackedIndices.size()) {
                if (renderInterruptedAfter(deadlineWork))
                    return false;
                size_t end = begin + 1;
                while (end < changedPackedIndices.size() &&
                        changedPackedIndices[end] ==
                            changedPackedIndices[end - 1] + 1u)
                    ++end;
                const uint32_t first = changedPackedIndices[begin];
                const size_t count = end - begin;
                sparseUpload = gpuRes_->updateInstanceData(
                    static_cast<GLintptr>(first) * sizeof(InstVertex),
                    indirectPrepared_.instances.data() + first,
                    static_cast<GLsizeiptr>(count) *
                        sizeof(InstVertex),
                    glue);
                begin = end;
            }
            if (sparseUpload)
                indirectPrepared_.instanceUploadSerial =
                    gpuRes_->instanceUploadSerial();
        }
        /*
         * A missing/overwritten VBO or an expired delta journal falls back to
         * one complete upload.  Command and atlas state remain reusable.
         */
        if (!sparseUpload)
            indirectPrepared_.instanceUploadSerial = 0;
        if (pressureAttributesChanged) {
            pressureProxyAppendOnly_ = false;
            ++pressureProxyRevision_;
            if (!pressureProxyRevision_)
                pressureProxyRevision_ = 1;
        }
    }

    /*
     * Expensive retained-record audit used by the graphical stress harness.
     * It reconstructs the command/instance contract from the authoritative
     * frame plan without touching GL.  Keeping this behind an environment
     * switch lets us distinguish a CPU journal defect from a GPU buffer/state
     * defect without making ordinary replay O(all visible parts).
     */
    if (configuration_->validateReplay) {
        const uint32_t noSlot =
            std::numeric_limits<uint32_t>::max();
        const auto rejectAudit =
            [&](size_t demandIndex, const char *reason) {
                std::fprintf(stderr,
                    "CadRendererGL retained audit failed demand=%zu "
                    "reason=%s parts=%zu packed=%zu pages=%zu\n",
                    demandIndex, reason ? reason : "unknown",
                    indirectPrepared_.parts.size(),
                    indirectPrepared_.instances.size(),
                    indirectPrepared_.pages.size());
                indirectPrepared_.valid = false;
                return false;
            };
        for (size_t demandIndex = 0;
                demandIndex < indirectPrepared_.parts.size();
                ++demandIndex) {
            if (renderInterruptedAfter(deadlineWork))
                return false;
            const IndirectPreparedPart& demand =
                indirectPrepared_.parts[demandIndex];
            if (demand.partIndex >= plan.partBindings.size())
                return rejectAudit(demandIndex, "part-index");
            const CadPartBinding& binding =
                plan.partBindings[demand.partIndex];
            if (!(binding.part == demand.part) ||
                    binding.generation != demand.generation ||
                    !binding.geometry || !binding.geometry->shaded)
                return rejectAudit(demandIndex, "binding");
            if (demand.packedInstance == noSlot ||
                    demand.packedInstance >=
                        indirectPrepared_.instances.size() ||
                    demand.packedInstance >=
                        indirectPrepared_.sourceInstanceIndices.size())
                return rejectAudit(demandIndex, "packed-instance");
            const uint32_t sourceIndex =
                indirectPrepared_.sourceInstanceIndices[
                    demand.packedInstance];
            if (sourceIndex >= plan.visibleInstances.size())
                return rejectAudit(demandIndex, "source-index");
            const CadVisibleInstance& source =
                plan.visibleInstances[sourceIndex];
            if (source.partIndex != demand.partIndex)
                return rejectAudit(demandIndex, "source-part");
            const InstVertex& instance =
                indirectPrepared_.instances[demand.packedInstance];
            if (std::memcmp(
                    instance.transform, source.transform.data(),
                    16u * sizeof(float)) != 0)
                return rejectAudit(demandIndex, "transform");

            const TriMesh& mesh = *binding.geometry->shaded;
            uint8_t level = mesh.isProgressive() ?
                cadResolvedProgressiveCut(
                    effectiveProgressiveCut(
                        binding.part, source.lodCut),
                    mesh.progressiveMinimumCut,
                    mesh.progressiveResidentCut) :
                15u;
            const CadTriangleAtlasPart *atlas =
                gpuRes_->triangleAtlasPart(binding.part);
            if (!atlas || atlas->page != demand.page ||
                    atlas->vertices.first != demand.vertexFirst ||
                    atlas->indices.first != demand.indexFirst)
                return rejectAudit(demandIndex, "atlas");
            while (mesh.isProgressive() &&
                    level > mesh.progressiveMinimumCut &&
                    (mesh.positionCountAtCut(level) >
                         atlas->vertexCount ||
                     mesh.indexCountAtCut(level) >
                         atlas->indexCount))
                --level;
            const uint32_t expectedCount =
                static_cast<uint32_t>(mesh.isProgressive() ?
                    mesh.indexCountAtCut(level) :
                    mesh.indices.size());
            if (!expectedCount ||
                    instance.popMinLevel[3] !=
                        packedProgressiveQuantization(
                            mesh.isProgressive() ?
                                mesh.quantizationAtCut(level) :
                                ProgressiveQuantization()))
                return rejectAudit(demandIndex, "lod-level");
            const SbVec3f expectedMinimum = mesh.isProgressive() ?
                mesh.progressiveQuantizationMinimum :
                SbVec3f(0, 0, 0);
            const SbVec3f expectedMaximum = mesh.isProgressive() ?
                mesh.progressiveQuantizationMaximum :
                SbVec3f(0, 0, 0);
            for (int axis = 0; axis < 3; ++axis) {
                if (instance.popMinLevel[axis] !=
                            expectedMinimum[axis] ||
                        instance.popMaxFlags[axis] !=
                            expectedMaximum[axis])
                    return rejectAudit(
                        demandIndex, "quantization");
            }

            const IndirectPageWork *pageWork = nullptr;
            for (const IndirectPageWork& candidate :
                    indirectPrepared_.pages) {
                if (candidate.page == demand.page) {
                    pageWork = &candidate;
                    break;
                }
            }
            if (!pageWork)
                return rejectAudit(demandIndex, "page-work");
            const auto& commands = demand.commandCulled ?
                pageWork->culled : pageWork->ordinary;
            if (demand.commandIndex >= commands.size())
                return rejectAudit(demandIndex, "command-index");
            const CadDrawElementsIndirectCommand& command =
                commands[demand.commandIndex];
            if (command.count != expectedCount ||
                    command.instanceCount != 1u ||
                    command.firstIndex != demand.indexFirst ||
                    command.baseVertex !=
                        static_cast<int32_t>(demand.vertexFirst) ||
                    command.baseInstance != demand.packedInstance)
                return rejectAudit(demandIndex, "command");
        }
    }

    const uint32_t validationIntervalFrames =
        configuration_->atlasValidationIntervalFrames;
    const bool validateAtlas =
        indirectPrepared_.atlasValidationActive ||
        indirectPrepared_.atlasRevision !=
            gpuRes_->triangleAtlasRevision() ||
        !indirectPrepared_.atlasValidationCountdown;
    if (validateAtlas) {
        if (!indirectPrepared_.atlasValidationActive) {
            indirectPrepared_.atlasValidationActive = true;
            indirectPrepared_.atlasValidationCursor = 0u;
            indirectPrepared_.atlasValidationRevision =
                gpuRes_->triangleAtlasRevision();
        }
        noteRenderPreparation("retained-atlas-validation-slice");
        while (indirectPrepared_.atlasValidationCursor <
                indirectPrepared_.parts.size()) {
            if (renderInterruptedAfter(deadlineWork))
                return false;
            const IndirectPreparedPart& demand =
                indirectPrepared_.parts[
                    indirectPrepared_.atlasValidationCursor++];
            if (demand.partIndex >= plan.partBindings.size()) {
                indirectPrepared_.atlasValidationActive = false;
                indirectPrepared_.valid = false;
                return false;
            }
            const CadPartBinding& binding =
                plan.partBindings[demand.partIndex];
            if (!(binding.part == demand.part) ||
                    binding.generation != demand.generation) {
                indirectPrepared_.atlasValidationActive = false;
                indirectPrepared_.valid = false;
                return false;
            }
            const CadTriangleAtlasPart *atlas =
                gpuRes_->touchTriangleAtlasPart(
                    demand.part, demand.generation, demand.hasNormals,
                    demand.vertexCount, demand.indexCount);
            if (!atlas || atlas->page != demand.page ||
                    atlas->vertices.first != demand.vertexFirst ||
                    atlas->indices.first != demand.indexFirst) {
                indirectPrepared_.atlasValidationActive = false;
                indirectPrepared_.valid = false;
                return false;
            }
        }
        /* No renderer or allocator may mutate this context's atlas while its
         * synchronous traversal is active.  Still verify the transaction's
         * revision before publishing its certificate so a future shared-
         * context implementation cannot accidentally validate a mixed
         * epoch. */
        if (indirectPrepared_.atlasValidationRevision !=
                gpuRes_->triangleAtlasRevision()) {
            indirectPrepared_.atlasValidationCursor = 0u;
            indirectPrepared_.atlasValidationRevision =
                gpuRes_->triangleAtlasRevision();
            return false;
        }
        indirectPrepared_.atlasRevision =
            gpuRes_->triangleAtlasRevision();
        indirectPrepared_.atlasValidationCountdown =
            validationIntervalFrames;
        indirectPrepared_.atlasValidationActive = false;
        indirectPrepared_.atlasValidationCursor = 0u;
        indirectPrepared_.atlasValidationRevision = 0u;
    } else {
        --indirectPrepared_.atlasValidationCountdown;
        if (!indirectPrepared_.parts.empty())
            gpuRes_->deferTriangleAtlasMaintenance();
    }

    pressureProxyPointsView_ =
        &indirectPrepared_.pressureProxyPoints;
    atlasAdmissionPressure_ =
        indirectPrepared_.atlasAdmissionPressure;
    lastRenderedTriangleCount_ =
        indirectPrepared_.renderedTriangleCount;
    const bool submitted =
        submitIndirectPrepared(glue, viewProj, viewVolume);
    if (submitted)
        lastRenderUsedPreparedReplay_ = true;
    return submitted;
}

bool CadRendererGL::renderIndirectShaded(
        const CadFramePlan& plan,
        const SoGLContext* glue,
        const SbMatrix& viewProj,
        const SbViewVolume& viewVolume)
{
    if (!glue || !gpuRes_ || !caps_.canUseIndirect() ||
            !shaders_.shadedIndirect || plan.shadedItems.empty() ||
            plan.visibleInstances.empty()) {
        if (gpuRes_ && indirectPreparation_.active) {
            gpuRes_->endTriangleAtlasExactPreparation();
            publishPreparation(
                presentationPreparation_.target,
                Obol::CadPresentationPreparationState::Failed,
                indirectPreparation_.totalUnits,
                indirectPreparation_.completedUnits,
                indirectPreparation_.requestedLiveBytes);
        }
        indirectPreparation_ = IndirectPreparationState();
        return rejectIndirect(1, "precondition");
    }
    IndirectPreparationState& build = indirectPreparation_;
    const int progressiveCutCeiling =
        activeViewState().progressiveCutCeiling;
    const float progressiveCutNextFraction =
        activeViewState().progressiveCutNextFraction;
    const bool matchingBuild = build.active &&
        build.contextId == glue->contextid &&
        build.planRevision == plan.revision &&
        build.progressiveCutCeiling == progressiveCutCeiling &&
        build.progressiveCutNextFraction == progressiveCutNextFraction &&
        build.viewProj == viewProj;
    if (build.active && !matchingBuild) {
        gpuRes_->endTriangleAtlasExactPreparation();
        build = IndirectPreparationState();
    }
    const bool replayEnabled = configuration_->replay;
    if (!build.active) {
        if (replayEnabled) {
            if (replayIndirectShaded(
                    plan, glue, viewProj, viewVolume))
                return true;
            if (renderInterrupted())
                return false;
        } else {
            /* Diagnostic/reference mode: retain the same atlas, shaders,
             * indirect commands, and GPU submission route while preparing
             * the CPU submission exactly for every frame. */
            indirectPrepared_.valid = false;
        }
    }
    using IndirectClock = std::chrono::steady_clock;
    const auto indirectStarted = IndirectClock::now();
    static constexpr size_t guaranteedWorkPerRetry = 4096u;
    size_t workSinceAbortCheck = 0u;
    const auto preparationInterrupted = [&](size_t work = 1u) {
        const size_t remaining = guaranteedWorkPerRetry -
            std::min(guaranteedWorkPerRetry, workSinceAbortCheck);
        if (work < remaining) {
            workSinceAbortCheck += work;
            return false;
        }
        workSinceAbortCheck = 0u;
        return renderInterrupted();
    };
    const auto abandon = [&](int status, const char *reason) {
        publishPreparation(
            presentationPreparation_.target,
            status == 4 ?
                Obol::CadPresentationPreparationState::Constrained :
                Obol::CadPresentationPreparationState::Failed,
            build.totalUnits, build.completedUnits,
            build.requestedLiveBytes);
        gpuRes_->endTriangleAtlasExactPreparation();
        build = IndirectPreparationState();
        indirectPrepared_.valid = false;
        return rejectIndirect(status, reason);
    };
    const auto completePreparationUnit = [&]() {
        if (build.completedUnits < build.totalUnits)
            ++build.completedUnits;
    };

    if (!build.active) {
        indirectPrepared_.valid = false;
        gpuRes_->beginTriangleAtlasExactPreparation();
        build.active = true;
        build.phase = IndirectPreparationPhase::Visibility;
        build.contextId = glue->contextid;
        build.planRevision = plan.revision;
        build.progressiveCutCeiling = progressiveCutCeiling;
        build.progressiveCutNextFraction = progressiveCutNextFraction;
        build.viewProj = viewProj;
        const uint64_t occurrenceUnits =
            static_cast<uint64_t>(plan.visibleInstances.size());
        const uint64_t partUnits =
            static_cast<uint64_t>(plan.partBindings.size());
        const auto saturatingMultiply = [](uint64_t value,
                                           uint64_t factor) {
            return value && factor > UINT64_MAX / value ?
                UINT64_MAX : value * factor;
        };
        const auto saturatingAdd = [](uint64_t left, uint64_t right) {
            return right > UINT64_MAX - left ?
                UINT64_MAX : left + right;
        };
        build.totalUnits = saturatingAdd(
            saturatingAdd(
                saturatingMultiply(occurrenceUnits, 6u),
                saturatingMultiply(partUnits, 5u)),
            3u);
        publishPreparation(
            preparationTarget(
                Obol::CadPresentationPreparationKind::RetainedIndirect,
                glue->contextid, plan.revision, plan.geometryRevision,
                progressiveCutCeiling, progressiveCutNextFraction,
                viewProj),
            Obol::CadPresentationPreparationState::Preparing,
            build.totalUnits, 0, 0);

        indirectVisibleMask_.assign(plan.visibleInstances.size(), 0u);
        indirectVisibleMaximumCut_.assign(plan.partBindings.size(), 0u);
        indirectVisiblePart_.assign(plan.partBindings.size(), 0u);
        indirectVisibleImportance_.assign(plan.partBindings.size(), 0.0);
        indirectVisiblePartIndices_.clear();
        if (indirectVisiblePartIndices_.capacity() <
                plan.partBindings.size())
            indirectVisiblePartIndices_.reserve(plan.partBindings.size());
        indirectFirstVisibleOccurrence_.assign(
            plan.partBindings.size(),
            std::numeric_limits<uint32_t>::max());
        indirectNextVisibleOccurrence_.assign(
            plan.visibleInstances.size(),
            std::numeric_limits<uint32_t>::max());
        indirectRequestedVertexCounts_.assign(
            plan.partBindings.size(), 0u);
        indirectRequestedIndexCounts_.assign(
            plan.partBindings.size(), 0u);
        indirectAtlasBindings_.assign(
            plan.partBindings.size(), nullptr);
    }
    ++build.sliceCount;
    /*
     * The preparation serial is a host-visible forward-progress witness.
     * Once all retained records have been published, Submit retries perform
     * no preparation: they only try to draw the already prepared frame.
     * Counting those retries as preparation caused a deadline livelock on
     * slow renderers.  The host kept granting an unchanged retry instead of
     * lowering the reversible progressive ceiling, even though every retry
     * was spending all of its time in the same draw.
     *
     * A slice which starts before Submit advances at least one bounded unit
     * of retained work before its first abort check (see
     * guaranteedWorkPerRetry above), so it is a valid progress witness.  The
     * first slice which reaches Submit is also counted; this grants one
     * unchanged replay of the newly published record.  If that replay still
     * misses its deadline, its unchanged serial correctly classifies the
     * failure as draw-capacity evidence.
     */
    if (build.phase != IndirectPreparationPhase::Submit)
        noteRenderPreparation("retained-exact-build-slice");

    /*
     * Resolve view visibility before GPU admission.  The assembly plan is a
     * structural/presentation cache and intentionally contains authored
     * visible instances outside this camera's frustum.  Admitting every part
     * here defeated view-aware LoD memory management and made a close zoom
     * retain an entire vehicle.  Subpixel proxy occurrences are likewise
     * owned by the aggregate point channel, never by the mesh atlas.
    */
    const ExecutorFrustumPlanes fp =
        extractExecutorFrustumPlanes(viewProj);
    const SbVec2s pressureProxyViewportSize =
        cadPressureProxyViewportSize(glue);
    auto& visibleMask = indirectVisibleMask_;
    auto& visibleMaximumCut = indirectVisibleMaximumCut_;
    auto& visiblePart = indirectVisiblePart_;
    auto& visibleImportance = indirectVisibleImportance_;
    auto& visiblePartIndices = indirectVisiblePartIndices_;
    const uint32_t noOccurrence = std::numeric_limits<uint32_t>::max();
    auto& firstVisibleOccurrence = indirectFirstVisibleOccurrence_;
    auto& nextVisibleOccurrence = indirectNextVisibleOccurrence_;
    if (build.phase == IndirectPreparationPhase::Visibility) {
      while (build.itemCursor < plan.shadedItems.size()) {
        const CadDrawItem& item = plan.shadedItems[build.itemCursor];
        if (!item.instanceCount ||
                item.partIndex >= plan.partBindings.size() ||
                item.baseInstance >= plan.visibleInstances.size() ||
                item.instanceCount > plan.visibleInstances.size() -
                    item.baseInstance) {
            ++build.itemCursor;
            build.occurrenceOffset = 0;
            continue;
        }
        const CadPartBinding& binding =
            plan.partBindings[item.partIndex];
        const PartGeometry *geometry = binding.geometry.get();
        if (!geometry || !geometry->shaded)
            return abandon(
                2, "visible part has no shaded geometry");
        const TriMesh& mesh = *geometry->shaded;
        while (build.occurrenceOffset < item.instanceCount) {
            if (preparationInterrupted())
                return false;
            const size_t visibleIndex = item.baseInstance +
                build.occurrenceOffset++;
            completePreparationUnit();
            const CadVisibleInstance& instance =
                plan.visibleInstances[visibleIndex];
            if (!cadInstanceDrawable(
                    plan, item, visibleIndex, CadDrawChannel::Shaded) ||
                    isBoxOutsideExecutorFrustum(
                        instance.wbMin, instance.wbMax, fp))
                continue;
            if (!visibleMask[visibleIndex]) {
                visibleMask[visibleIndex] = 1u;
                nextVisibleOccurrence[visibleIndex] =
                    firstVisibleOccurrence[item.partIndex];
                firstVisibleOccurrence[item.partIndex] =
                    static_cast<uint32_t>(visibleIndex);
                ++build.visibleOccurrenceCount;
            }
            const uint8_t requested = mesh.isProgressive() ?
                cadResolvedProgressiveCut(
                    effectiveProgressiveCut(
                        binding.part, instance.lodCut),
                    mesh.progressiveMinimumCut,
                    mesh.progressiveResidentCut) :
                Obol::ProgressiveCutUnspecified;
            if (!visiblePart[item.partIndex]) {
                visiblePart[item.partIndex] = 1u;
                visiblePartIndices.push_back(item.partIndex);
            }
            visibleMaximumCut[item.partIndex] =
                std::max(visibleMaximumCut[item.partIndex], requested);
            double importance = executorProjectedBoxImportance(
                instance.wbMin, instance.wbMax, viewProj);
            if (instance.flags & 3u)
                importance *= 16.0;
            visibleImportance[item.partIndex] = std::min(
                1.0e12,
                visibleImportance[item.partIndex] + importance);
        }
        ++build.itemCursor;
        build.occurrenceOffset = 0;
      }
        if (!build.visibleOccurrenceCount) {
        publishPreparation(
            presentationPreparation_.target,
            Obol::CadPresentationPreparationState::Complete,
            build.totalUnits, build.totalUnits,
            build.requestedLiveBytes);
        gpuRes_->endTriangleAtlasExactPreparation();
        build = IndirectPreparationState();
        lastIndirectStatus_ = 0;
        return true;
      }
      build.phase = IndirectPreparationPhase::Protection;
      build.partCursor = 0;
    }

    auto& requestedVertexCounts = indirectRequestedVertexCounts_;
    auto& requestedIndexCounts = indirectRequestedIndexCounts_;
    auto& atlasBindings = indirectAtlasBindings_;
    /*
     * Mark every retained consumer before admitting any new allocation.
     * Without this phase, pressure while processing part N could evict part
     * N+1 merely because it had not yet been encountered in this frame,
     * creating a perpetual evict/re-upload cycle at the memory ceiling.
     *
     * The touch also validates generation and retained prefix capacity.  An
     * unchanged frame can therefore reuse the returned bindings directly,
     * avoiding a second full-table upsert pass and a third lookup pass over
     * tens of thousands of unique parts.
     */
    if (build.phase == IndirectPreparationPhase::Protection) {
      while (build.partCursor < visiblePartIndices.size()) {
        if (preparationInterrupted())
            return false;
        const uint32_t partIndex =
            visiblePartIndices[build.partCursor++];
        completePreparationUnit();
        const CadPartBinding& binding =
            plan.partBindings[partIndex];
        const PartGeometry *geometry = binding.geometry.get();
        if (!geometry || !geometry->shaded)
            return abandon(
                2, "visible part has no shaded geometry");
        const TriMesh& mesh = *geometry->shaded;
        const uint8_t requested = visibleMaximumCut[partIndex];
        const size_t vertexCount = mesh.isProgressive() ?
            mesh.positionCountAtCut(requested) : mesh.positions.size();
        const size_t indexCount = mesh.isProgressive() ?
            mesh.indexCountAtCut(requested) : mesh.indices.size();
        if (!vertexCount || !indexCount ||
                vertexCount > std::numeric_limits<uint32_t>::max() ||
                indexCount > std::numeric_limits<uint32_t>::max())
            return abandon(3, "invalid retained prefix counts");
        requestedVertexCounts[partIndex] =
            static_cast<uint32_t>(vertexCount);
        requestedIndexCounts[partIndex] =
            static_cast<uint32_t>(indexCount);
        const uint64_t vertexStride = mesh.normals.empty() ? 12u : 24u;
        const uint64_t requestedBytes =
            static_cast<uint64_t>(vertexCount) * vertexStride +
            static_cast<uint64_t>(indexCount) * sizeof(uint32_t);
        build.requestedLiveBytes = requestedBytes >
                UINT64_MAX - build.requestedLiveBytes ?
            UINT64_MAX : build.requestedLiveBytes + requestedBytes;
        atlasBindings[partIndex] =
            gpuRes_->touchTriangleAtlasPart(
                binding.part, binding.generation,
                !mesh.normals.empty(),
                requestedVertexCounts[partIndex],
                requestedIndexCounts[partIndex]);
        gpuRes_->protectTriangleAtlasExactPart(binding.part);
      }

    auto& admissionPartIndices = indirectAdmissionPartIndices_;
    admissionPartIndices.assign(
        visiblePartIndices.begin(), visiblePartIndices.end());
    const size_t atlasBudget = gpuRes_->triangleAtlasBudgetBytes();
    const bool likelyMemoryPressure = atlasBudget > 0 &&
        (build.requestedLiveBytes >=
             static_cast<uint64_t>(atlasBudget / 4u) * 3u ||
         gpuRes_->triangleAtlasAllocatedBytes() >=
             atlasBudget / 4u * 3u);
    if (likelyMemoryPressure) {
        std::stable_sort(admissionPartIndices.begin(),
            admissionPartIndices.end(),
            [&](uint32_t left, uint32_t right) {
                return visibleImportance[left] >
                    visibleImportance[right];
            });
    }

    /*
     * Make each unique visible part's richest requested prefix resident.
     * Occurrences at smaller levels select smaller command counts and
     * independent quantization attributes from the same cumulative arrays.
     */
    auto& pressureProxyPoints = indirectPressureProxyPoints_;
    pressureProxyPoints.clear();
    indirectPrepared_.pressureProxySourceInstanceIndices.swap(
        indirectPressureProxySourceInstanceIndices_);
    auto& pressureProxySourceInstanceIndices =
        indirectPressureProxySourceInstanceIndices_;
    pressureProxySourceInstanceIndices.clear();
    if (pressureProxySourceInstanceIndices.capacity() <
            build.visibleOccurrenceCount)
        pressureProxySourceInstanceIndices.reserve(
            build.visibleOccurrenceCount);
    build.phase = IndirectPreparationPhase::Coverage;
    build.partCursor = 0;
    }

    auto& admissionPartIndices = indirectAdmissionPartIndices_;
    auto& pressureProxyPoints = indirectPressureProxyPoints_;
    auto& pressureProxySourceInstanceIndices =
        indirectPressureProxySourceInstanceIndices_;
    const auto beginPressureProxy = [&](uint32_t partIndex) {
        const CadPartBinding& binding = plan.partBindings[partIndex];
        if (!binding.subpixelProxyEligible)
            return false;
        build.proxyPartActive = true;
        build.proxyPartIndex = partIndex;
        build.proxyVisibleIndex = firstVisibleOccurrence[partIndex];
        return true;
    };
    const auto continuePressureProxy = [&]() {
        if (!build.proxyPartActive)
            return true;
        const CadPartBinding& binding =
            plan.partBindings[build.proxyPartIndex];
        while (build.proxyVisibleIndex != noOccurrence) {
            if (preparationInterrupted())
                return false;
            const uint32_t visibleIndex = build.proxyVisibleIndex;
            build.proxyVisibleIndex =
                nextVisibleOccurrence[visibleIndex];
            completePreparationUnit();
            if (!visibleMask[visibleIndex])
                continue;
            const CadVisibleInstance& instance =
                plan.visibleInstances[visibleIndex];
            pressureProxyPoints.push_back(
                cadPressureProxyForInstance(
                    binding, instance, viewProj,
                    pressureProxyViewportSize));
            pressureProxySourceInstanceIndices.push_back(visibleIndex);
            visibleMask[visibleIndex] = 0u;
            --build.visibleOccurrenceCount;
        }
        visiblePart[build.proxyPartIndex] = 0u;
        build.proxyPartActive = false;
        return true;
    };

    /* Coverage pass: give every progressive part its producer-authored
     * minimum coherent prefix before any part consumes memory on enrichment.
     * Under pressure, screen-prominent and selected occurrences are visited
     * first.  This removes plan-order starvation without compromising the
     * no-holes PoP contract. */
    if (build.phase == IndirectPreparationPhase::Coverage) {
      while (build.partCursor < admissionPartIndices.size()) {
        if (build.proxyPartActive) {
            if (!continuePressureProxy())
                return false;
            ++build.partCursor;
            completePreparationUnit();
            continue;
        }
        if (preparationInterrupted())
            return false;
        const uint32_t partIndex =
            admissionPartIndices[build.partCursor];
        if (!visiblePart[partIndex] || atlasBindings[partIndex]) {
            ++build.partCursor;
            completePreparationUnit();
            continue;
        }
        const CadPartBinding& binding = plan.partBindings[partIndex];
        const PartGeometry *geometry = binding.geometry.get();
        if (!geometry || !geometry->shaded)
            return abandon(2, "visible part has no shaded geometry");
        const TriMesh& mesh = *geometry->shaded;
        uint32_t coverageVertexCount = requestedVertexCounts[partIndex];
        uint32_t coverageIndexCount = requestedIndexCounts[partIndex];
        if (mesh.isProgressive()) {
            coverageVertexCount = static_cast<uint32_t>(
                mesh.positionCountAtCut(mesh.progressiveMinimumCut));
            coverageIndexCount = static_cast<uint32_t>(
                mesh.indexCountAtCut(mesh.progressiveMinimumCut));
        }
        const CadTriangleAtlasPart *admitted =
            gpuRes_->upsertTriangleAtlasPart(
                binding.part, binding.generation,
                executorPackedVec3fData(mesh.positions),
                executorPackedVec3fData(mesh.normals),
                coverageVertexCount, mesh.indices.data(),
                coverageIndexCount, mesh.isProgressive(),
                mesh.progressiveLineage, glue, caps_);
        if (!admitted) {
            build.atlasAdmissionPressure = true;
            if (configuration_->indirectDebug)
                std::fprintf(stderr,
                    "CadRendererGL indirect atlas coverage failed "
                    "part=%016llx:%016llx vertices=%zu indices=%zu "
                    "allocated=%zu live=%zu pages=%zu parts=%zu\n",
                    static_cast<unsigned long long>(binding.part.w0),
                    static_cast<unsigned long long>(binding.part.w1),
                    static_cast<size_t>(coverageVertexCount),
                    static_cast<size_t>(coverageIndexCount),
                    gpuRes_->triangleAtlasAllocatedBytes(),
                    gpuRes_->triangleAtlasLiveBytes(),
                    gpuRes_->triangleAtlasPageCount(),
                    gpuRes_->triangleAtlasPartCount());
            if (!beginPressureProxy(partIndex))
                return abandon(4, "triangle atlas coverage");
            continue;
        }
        atlasBindings[partIndex] = admitted;
        gpuRes_->protectTriangleAtlasExactPart(binding.part);
        ++build.partCursor;
        completePreparationUnit();
      }
      build.phase = IndirectPreparationPhase::Enrichment;
      build.partCursor = 0;
    }

    /* Enrichment pass: grow retained prefixes toward the view request in the
     * same value order.  A failed grow preserves and draws the coherent
     * coverage prefix; it never demotes that part back to a box or point. */
    if (build.phase == IndirectPreparationPhase::Enrichment) {
      while (build.partCursor < admissionPartIndices.size()) {
        if (build.proxyPartActive) {
            if (!continuePressureProxy())
                return false;
            ++build.partCursor;
            completePreparationUnit();
            continue;
        }
        if (preparationInterrupted())
            return false;
        const uint32_t partIndex =
            admissionPartIndices[build.partCursor];
        if (!visiblePart[partIndex] || !atlasBindings[partIndex]) {
            ++build.partCursor;
            completePreparationUnit();
            continue;
        }
        const uint32_t vertexCount = requestedVertexCounts[partIndex];
        const uint32_t indexCount = requestedIndexCounts[partIndex];
        const CadTriangleAtlasPart *current = atlasBindings[partIndex];
        if (current->vertexCount >= vertexCount &&
                current->indexCount >= indexCount) {
            ++build.partCursor;
            completePreparationUnit();
            continue;
        }
        const CadPartBinding& binding = plan.partBindings[partIndex];
        const TriMesh& mesh = *binding.geometry->shaded;
        const CadTriangleAtlasPart *enriched =
            gpuRes_->upsertTriangleAtlasPart(
                binding.part, binding.generation,
                executorPackedVec3fData(mesh.positions),
                executorPackedVec3fData(mesh.normals),
                vertexCount, mesh.indices.data(), indexCount,
                mesh.isProgressive(), mesh.progressiveLineage,
                glue, caps_);
        if (!enriched) {
            build.atlasAdmissionPressure = true;
            enriched = gpuRes_->triangleAtlasPart(binding.part);
        }
        if (!enriched) {
            if (!beginPressureProxy(partIndex))
                return abandon(4, "triangle atlas enrichment");
            atlasBindings[partIndex] = nullptr;
            continue;
        }
        atlasBindings[partIndex] = enriched;
        gpuRes_->protectTriangleAtlasExactPart(binding.part);
        if (enriched->vertexCount < vertexCount ||
                enriched->indexCount < indexCount)
            build.atlasAdmissionPressure = true;
        ++build.partCursor;
        completePreparationUnit();
      }
      build.phase = IndirectPreparationPhase::CommandSetup;
    }

    /*
     * The preceding prepared frame is no longer replayable once exact
     * preparation begins.  Reuse its per-page command capacities as the
     * scratch target for this build.  Page ids are dense atlas slots, so a
     * vector lookup avoids one red-black-tree allocation per page as well.
     */
    if (build.phase == IndirectPreparationPhase::CommandSetup) {
      if (!indirectPrepared_.pages.empty())
          indirectPageWorkScratch_.swap(indirectPrepared_.pages);
      indirectPrepared_.pages.clear();
      for (IndirectPageWork& work : indirectPageWorkScratch_) {
          work.ordinary.clear();
          work.culled.clear();
      }
      const size_t atlasPageCount = gpuRes_->triangleAtlasPageCount();
      indirectPageWorkSlotByPage_.assign(
          atlasPageCount, std::numeric_limits<uint32_t>::max());
      for (size_t i = 0; i < indirectPageWorkScratch_.size(); ++i) {
          const uint32_t page = indirectPageWorkScratch_[i].page;
          if (page < indirectPageWorkSlotByPage_.size())
              indirectPageWorkSlotByPage_[page] =
                  static_cast<uint32_t>(i);
      }
      const uint32_t noPreparedSlot =
          std::numeric_limits<uint32_t>::max();
      indirectCommandIndexByPart_.assign(
          plan.partBindings.size(), noPreparedSlot);
      indirectCommandCullByPart_.assign(
          plan.partBindings.size(), 0u);
      indirectPackedInstanceByPart_.assign(
          plan.partBindings.size(), noPreparedSlot);
      indirectPrepared_.instances.swap(indirectInstances_);
      indirectInstances_.clear();
      if (indirectInstances_.capacity() < build.visibleOccurrenceCount)
          indirectInstances_.reserve(build.visibleOccurrenceCount);
      indirectPrepared_.sourceInstanceIndices.swap(
          indirectSourceInstanceIndices_);
      indirectSourceInstanceIndices_.clear();
      if (indirectSourceInstanceIndices_.capacity() <
              build.visibleOccurrenceCount)
          indirectSourceInstanceIndices_.reserve(
              build.visibleOccurrenceCount);
      build.itemCursor = 0;
      build.occurrenceOffset = 0;
      build.commandItemActive = false;
      build.phase = IndirectPreparationPhase::Commands;
      completePreparationUnit();
    }
    const auto pageWorkFor = [&](uint32_t page) ->
            IndirectPageWork& {
        if (page >= indirectPageWorkSlotByPage_.size())
            indirectPageWorkSlotByPage_.resize(
                static_cast<size_t>(page) + 1u,
                std::numeric_limits<uint32_t>::max());
        uint32_t& slot = indirectPageWorkSlotByPage_[page];
        if (slot == std::numeric_limits<uint32_t>::max()) {
            slot = static_cast<uint32_t>(
                indirectPageWorkScratch_.size());
            IndirectPageWork work;
            work.page = page;
            indirectPageWorkScratch_.push_back(std::move(work));
        }
        return indirectPageWorkScratch_[slot];
    };
    const uint32_t noPreparedSlot =
        std::numeric_limits<uint32_t>::max();
    auto& instances = indirectInstances_;
    auto& sourceInstanceIndices = indirectSourceInstanceIndices_;
    /*
     * Protection and admission both return stable element pointers.  Use
     * those direct bindings below instead of performing a second hash-table
     * lookup for every visible part after an insertion.
     */
    if (build.phase == IndirectPreparationPhase::Commands) {
      while (build.itemCursor < plan.shadedItems.size()) {
        const CadDrawItem& item = plan.shadedItems[build.itemCursor];
        if (!build.commandItemActive) {
          if (!item.instanceCount ||
                  item.partIndex >= plan.partBindings.size() ||
                  item.baseInstance >= plan.visibleInstances.size() ||
                  item.instanceCount >
                      plan.visibleInstances.size() - item.baseInstance ||
                  !visiblePart[item.partIndex]) {
              ++build.itemCursor;
              build.occurrenceOffset = 0;
              continue;
          }
          if (instances.size() > std::numeric_limits<uint32_t>::max())
              return abandon(8, "instance stream overflow");
          build.commandBaseInstance =
              static_cast<uint32_t>(instances.size());
          build.commandCut = Obol::ProgressiveCutUnspecified;
          build.commandCount = 0;
          build.commandItemActive = true;
        }
        /*
         * Admission is intentionally view-aware.  A part whose occurrences
         * are all outside the frustum or represented by aggregate proxy
         * points has no atlas binding this frame and contributes no command.
         * Test that expected absence before resolving the binding; treating
         * it as an error made one culled/subpixel part throw the entire scene
         * into the CPU-flattened fallback.
         */
        const PartGeometry *geometry =
            plan.partBindings[item.partIndex].geometry.get();
        const CadTriangleAtlasPart *atlas =
            atlasBindings[item.partIndex];
        if (!geometry || !geometry->shaded || !atlas)
            return abandon(5, "missing admitted atlas binding");
        const TriMesh& mesh = *geometry->shaded;
        const bool progressive = mesh.isProgressive();
        while (build.occurrenceOffset < item.instanceCount) {
            if (preparationInterrupted())
                return false;
            const size_t instanceIndex = item.baseInstance +
                build.occurrenceOffset++;
            completePreparationUnit();
            if (!cadInstanceDrawable(
                    plan, item, instanceIndex, CadDrawChannel::Shaded) ||
                    !visibleMask[instanceIndex])
                continue;
            const CadVisibleInstance& source =
                plan.visibleInstances[instanceIndex];
            uint8_t level = progressive ?
                cadResolvedProgressiveCut(
                    effectiveProgressiveCut(
                        item.rep.part, source.lodCut),
                    mesh.progressiveMinimumCut,
                    mesh.progressiveResidentCut) :
                Obol::ProgressiveCutUnspecified;
            /*
             * Allocation pressure may deliberately retain a correct coarser
             * cumulative prefix.  Clamp to the richest level wholly resident
             * instead of failing the entire frame into a world-space rebuild.
             */
            while (progressive &&
                    level > mesh.progressiveMinimumCut &&
                    (mesh.positionCountAtCut(level) >
                         atlas->vertexCount ||
                     mesh.indexCountAtCut(level) > atlas->indexCount))
                --level;
            const size_t count = progressive ?
                mesh.indexCountAtCut(level) : mesh.indices.size();
            if (!count || count > atlas->indexCount ||
                    count > std::numeric_limits<uint32_t>::max())
                return abandon(6, "invalid indirect draw count");
            if (build.commandCount && level != build.commandCut)
                return abandon(
                    7, "mixed levels in one draw run");
            build.commandCut = level;
            build.commandCount = count;

            InstVertex target = {};
            std::memcpy(target.transform, source.transform.data(),
                        16 * sizeof(float));
            target.color[0] = source.rgba[0] / 255.0f;
            target.color[1] = source.rgba[1] / 255.0f;
            target.color[2] = source.rgba[2] / 255.0f;
            target.color[3] = source.rgba[3] / 255.0f;
            const SbVec3f minimum = progressive ?
                mesh.progressiveQuantizationMinimum : SbVec3f(0, 0, 0);
            const SbVec3f maximum = progressive ?
                mesh.progressiveQuantizationMaximum : SbVec3f(0, 0, 0);
            for (int axis = 0; axis < 3; ++axis) {
                target.popMinLevel[axis] = minimum[axis];
                target.popMaxFlags[axis] = maximum[axis];
            }
            target.popMinLevel[3] = packedProgressiveQuantization(
                progressive ? mesh.quantizationAtCut(level) :
                    ProgressiveQuantization());
            target.popMaxFlags[3] =
                (!mesh.normals.empty() ? 1.0f : 0.0f) +
                (progressive ? 2.0f : 0.0f) +
                ((source.flags & CadInstanceHidden) ? 4.0f : 0.0f);
            instances.push_back(target);
            sourceInstanceIndices.push_back(
                static_cast<uint32_t>(instanceIndex));
        }
        const uint32_t instanceCount =
            static_cast<uint32_t>(instances.size()) -
                build.commandBaseInstance;
        if (instanceCount && (!build.commandCount ||
                atlas->vertices.first >
                    static_cast<uint32_t>(
                        std::numeric_limits<int32_t>::max())))
            return abandon(8, "invalid atlas command base");

        if (instanceCount) {
          CadDrawElementsIndirectCommand command;
          command.count = static_cast<uint32_t>(build.commandCount);
          command.instanceCount = instanceCount;
          command.firstIndex = atlas->indices.first;
          command.baseVertex = static_cast<int32_t>(atlas->vertices.first);
          command.baseInstance = build.commandBaseInstance;
          build.renderedTriangleCount +=
              static_cast<uint64_t>(command.count / 3u) *
              static_cast<uint64_t>(command.instanceCount);
          cadAccumulateRenderedShadedWork(
              build.renderedWork, mesh, build.commandCut,
              static_cast<uint64_t>(command.count / 3u),
              static_cast<uint64_t>(command.instanceCount));
          IndirectPageWork& work = pageWorkFor(atlas->page);
          auto& commands =
              item.cullBackfaces ? work.culled : work.ordinary;
          if (item.partIndex < indirectCommandIndexByPart_.size()) {
            /*
             * A unique progressive part has exactly one active command and
             * one packed occurrence.  Shared parts intentionally retain the
             * sentinel and use the exact path when their LoD runs change.
             */
            if (item.instanceCount == 1u && instanceCount == 1u &&
                    indirectCommandIndexByPart_[item.partIndex] ==
                        noPreparedSlot) {
                indirectCommandIndexByPart_[item.partIndex] =
                    static_cast<uint32_t>(commands.size());
                indirectCommandCullByPart_[item.partIndex] =
                    item.cullBackfaces ? 1u : 0u;
                indirectPackedInstanceByPart_[item.partIndex] =
                    build.commandBaseInstance;
            } else {
                indirectCommandIndexByPart_[item.partIndex] =
                    noPreparedSlot;
                indirectPackedInstanceByPart_[item.partIndex] =
                    noPreparedSlot;
            }
          }
          commands.push_back(command);
        }
        ++build.itemCursor;
        build.occurrenceOffset = 0;
        build.commandItemActive = false;
      }
      build.phase = IndirectPreparationPhase::Preflight;
      build.pageCursor = 0;
    }
    /*
     * Preflight every page before issuing any draws.  Command streams larger
     * than a page's fixed scratch buffer are submitted in bounded chunks.
     * No recoverable rejection is permitted after the prepared frame becomes
     * visible to replay.
    */
    if (build.phase == IndirectPreparationPhase::Preflight) {
      const bool havePageWork = std::any_of(
          indirectPageWorkScratch_.begin(),
          indirectPageWorkScratch_.end(),
          [](const IndirectPageWork& work) {
              return !work.ordinary.empty() || !work.culled.empty();
          });
      if (!havePageWork && pressureProxyPoints.empty())
          return abandon(9, "empty visible page work");
      while (build.pageCursor < indirectPageWorkScratch_.size()) {
        if (preparationInterrupted())
            return false;
        const IndirectPageWork& work =
            indirectPageWorkScratch_[build.pageCursor++];
        completePreparationUnit();
        if (work.ordinary.empty() && work.culled.empty())
            continue;
        const CadTriangleAtlasPage *page =
            gpuRes_->triangleAtlasPage(work.page);
        if (!page || !page->indirectBuf || !page->indirectCapacity ||
                (work.ordinary.empty() && work.culled.empty()))
            return abandon(11, "indirect page preflight");
      }
      build.phase = IndirectPreparationPhase::PublishSetup;
    }

    /*
     * Publish an immutable CPU submission record.  A replay still touches
     * every demanded atlas part, which both protects it from reclamation and
     * verifies that generation, prefix capacity, page, and offsets remain
     * valid.  It can then skip visibility resolution, per-occurrence LoD,
     * instance packing, command construction, sorting, and proxy packing.
    */
    IndirectPreparedFrame& prepared = indirectPrepared_;
    if (build.phase == IndirectPreparationPhase::PublishSetup) {
      prepared.valid = false;
      prepared.contextId = glue->contextid;
      prepared.planRevision = plan.revision;
      prepared.geometryRevision = plan.geometryRevision;
      prepared.shadedLayoutRevision = plan.shadedLayoutRevision;
      prepared.shadedLodRevision = plan.shadedLodRevision;
      prepared.appendRevision = plan.appendRevision;
      prepared.partGeometryRevision = plan.partGeometryRevision;
      prepared.instanceAttributeRevision =
          plan.instanceAttributeRevision;
      prepared.subpixelProxyRevision = plan.subpixelProxyRevision;
      prepared.progressiveCutCeiling = progressiveCutCeiling;
      prepared.progressiveCutNextFraction =
          progressiveCutNextFraction;
      prepared.viewProj = viewProj;
      prepared.renderedTriangleCount = build.renderedTriangleCount;
      prepared.renderedWork = build.renderedWork;
      prepared.instanceUploadSerial = 0;
      prepared.atlasRevision = gpuRes_->triangleAtlasRevision();
      prepared.atlasValidationCountdown =
          configuration_->atlasValidationIntervalFrames;
      prepared.atlasValidationActive = false;
      prepared.atlasValidationCursor = 0u;
      prepared.atlasValidationRevision = 0u;
      prepared.cameraMotionReplayCount = 0;
      prepared.atlasAdmissionPressure = build.atlasAdmissionPressure;
      prepared.atlasPressurePartCount = 0;

      prepared.parts.clear();
      if (prepared.parts.capacity() < visiblePartIndices.size())
          prepared.parts.reserve(visiblePartIndices.size());
      prepared.partByPlanPartIndex.assign(
          plan.partBindings.size(),
          std::numeric_limits<uint32_t>::max());
      build.partCursor = 0;
      build.phase = IndirectPreparationPhase::PublishParts;
      completePreparationUnit();
    }
    if (build.phase == IndirectPreparationPhase::PublishParts) {
      while (build.partCursor < visiblePartIndices.size()) {
        if (preparationInterrupted())
            return false;
        const uint32_t partIndex =
            visiblePartIndices[build.partCursor++];
        completePreparationUnit();
        if (!visiblePart[partIndex])
            continue;
        const CadPartBinding& binding = plan.partBindings[partIndex];
        const PartGeometry *geometry = binding.geometry.get();
        const CadTriangleAtlasPart *atlas = atlasBindings[partIndex];
        if (!geometry || !geometry->shaded || !atlas)
            return abandon(11, "prepared part binding");
        IndirectPreparedPart demand;
        demand.part = binding.part;
        demand.partIndex = partIndex;
        demand.generation = binding.generation;
        /* Replay protects the coherent resident prefix, not an unaffordable
         * richer request.  The frame-level pressure bit asks scene policy to
         * revisit quality after memory/view conditions change without making
         * every stable replay fail validation and rebuild O(scene). */
        demand.vertexCount = std::min(
            requestedVertexCounts[partIndex], atlas->vertexCount);
        demand.indexCount = std::min(
            requestedIndexCounts[partIndex], atlas->indexCount);
        demand.admissionPressure =
            requestedVertexCounts[partIndex] > atlas->vertexCount ||
            requestedIndexCounts[partIndex] > atlas->indexCount;
        if (demand.admissionPressure)
            ++prepared.atlasPressurePartCount;
        demand.page = atlas->page;
        demand.vertexFirst = atlas->vertices.first;
        demand.indexFirst = atlas->indices.first;
        demand.hasNormals = !geometry->shaded->normals.empty();
        if (partIndex < indirectPackedInstanceByPart_.size())
            demand.packedInstance =
                indirectPackedInstanceByPart_[partIndex];
        if (partIndex < indirectCommandIndexByPart_.size())
            demand.commandIndex =
                indirectCommandIndexByPart_[partIndex];
        if (partIndex < indirectCommandCullByPart_.size())
            demand.commandCulled =
                indirectCommandCullByPart_[partIndex] != 0u;
        prepared.partByPlanPartIndex[partIndex] =
            static_cast<uint32_t>(prepared.parts.size());
        prepared.parts.push_back(demand);
      }

    /*
     * Remove atlas holes/stale pages without releasing the vector storage of
     * active pages.  Moving active work into the prepared store transfers
     * capacities wholesale; the next exact build swaps them back.
     */
      indirectPageWorkScratch_.erase(
        std::remove_if(
            indirectPageWorkScratch_.begin(),
            indirectPageWorkScratch_.end(),
            [](const IndirectPageWork& work) {
                return work.ordinary.empty() && work.culled.empty();
            }),
        indirectPageWorkScratch_.end());
      prepared.pages.swap(indirectPageWorkScratch_);
      prepared.instances.swap(instances);
      prepared.sourceInstanceIndices.swap(sourceInstanceIndices);
      prepared.pressureProxyPoints.swap(pressureProxyPoints);
      prepared.pressureProxySourceInstanceIndices.swap(
          pressureProxySourceInstanceIndices);
      prepared.appendPatchAnchorInstanceCount =
          prepared.instances.size();
      prepared.instanceIndexBySource.assign(
          plan.visibleInstances.size(), noPreparedSlot);
      build.reverseCursor = 0;
      build.phase = IndirectPreparationPhase::ReverseInstances;
    }
    if (build.phase == IndirectPreparationPhase::ReverseInstances) {
      while (build.reverseCursor <
              prepared.sourceInstanceIndices.size()) {
        if (preparationInterrupted())
            return false;
        const size_t i = build.reverseCursor++;
        completePreparationUnit();
        const uint32_t sourceIndex =
            prepared.sourceInstanceIndices[i];
        if (sourceIndex >= prepared.instanceIndexBySource.size())
            return abandon(11, "prepared instance reverse index");
        prepared.instanceIndexBySource[sourceIndex] =
            static_cast<uint32_t>(i);
      }
      prepared.pressureProxyIndexBySource.assign(
          plan.visibleInstances.size(), noPreparedSlot);
      build.reverseCursor = 0;
      build.phase = IndirectPreparationPhase::ReverseProxies;
    }
    if (build.phase == IndirectPreparationPhase::ReverseProxies) {
      while (build.reverseCursor <
              prepared.pressureProxySourceInstanceIndices.size()) {
        if (preparationInterrupted())
            return false;
        const size_t i = build.reverseCursor++;
        completePreparationUnit();
        const uint32_t sourceIndex =
            prepared.pressureProxySourceInstanceIndices[i];
        if (sourceIndex >=
                prepared.pressureProxyIndexBySource.size())
            return abandon(11, "prepared proxy reverse index");
        prepared.pressureProxyIndexBySource[sourceIndex] =
            static_cast<uint32_t>(i);
      }
      prepared.valid = true;
      pressureProxyAppendOnly_ = false;
      ++pressureProxyRevision_;
      if (!pressureProxyRevision_)
          pressureProxyRevision_ = 1;
      build.phase = IndirectPreparationPhase::Submit;
      publishPreparation(
          presentationPreparation_.target,
          Obol::CadPresentationPreparationState::Complete,
          build.totalUnits, build.totalUnits,
          build.requestedLiveBytes);
    }

    pressureProxyPointsView_ = &prepared.pressureProxyPoints;
    atlasAdmissionPressure_ = build.atlasAdmissionPressure ||
        prepared.atlasAdmissionPressure;
    lastRenderedTriangleCount_ = prepared.renderedTriangleCount;
    if (build.phase == IndirectPreparationPhase::Submit) {
      const bool submitted =
          submitIndirectPrepared(glue, viewProj, viewVolume);
      if (!submitted) {
          pressureProxyPointsView_ = nullptr;
          if (renderInterrupted())
              return false;
          return abandon(12, "indirect submission");
      }
      if (lastIndirectStatus_ != 0)
          return abandon(lastIndirectStatus_, "indirect submission");
    }

    const uint32_t completedSlices = build.sliceCount;
    const size_t completedVisibleOccurrences =
        build.visibleOccurrenceCount;
    const size_t completedVisibleParts = visiblePartIndices.size();
    const uint64_t completedTriangles = build.renderedTriangleCount;
    gpuRes_->endTriangleAtlasExactPreparation();
    publishPreparation(
        presentationPreparation_.target,
        Obol::CadPresentationPreparationState::Complete,
        build.totalUnits, build.totalUnits,
        build.requestedLiveBytes);
    build = IndirectPreparationState();

    /*
     * The atlas is now the only shaded geometry owner for this context.
     * Retaining the former expanded world-space and per-part triangle VBOs
     * would defeat the unified memory ceiling.
     */
    gpuRes_->releaseFlatShaded(glue);
    gpuRes_->releaseStandaloneTriangles(glue);
    if (lastIndirectStatus_ == 0)
        reportedIndirectStatus_ = 0;
    if (configuration_->renderTiming) {
        const auto completed = IndirectClock::now();
        const auto milliseconds = [](auto begin, auto end) {
            return std::chrono::duration<double, std::milli>(
                end - begin).count();
        };
        const double total =
            milliseconds(indirectStarted, completed);
        if (total >= 10.0)
            std::fprintf(stderr,
                "CadRendererGL exact indirect final-slice=%.3fms "
                "slices=%u "
                "source_instances=%zu visible_instances=%zu "
                "visible_parts=%zu triangles=%llu\n",
                total,
                completedSlices,
                plan.visibleInstances.size(),
                completedVisibleOccurrences,
                completedVisibleParts,
                static_cast<unsigned long long>(
                    completedTriangles));
    }
    if (configuration_->indirectDebug) {
        static uint32_t lastDebugContext = UINT32_MAX;
        static size_t lastDebugParts = 0;
        const size_t parts = gpuRes_->triangleAtlasPartCount();
        if (lastDebugContext != glue->contextid ||
                parts >= lastDebugParts + 4096u) {
            std::fprintf(stderr,
                "CadRendererGL indirect rendered context=%u commands=%zu "
                "pages=%zu parts=%zu allocated=%zu live=%zu\n",
                glue->contextid, plan.shadedItems.size(),
                gpuRes_->triangleAtlasPageCount(), parts,
                gpuRes_->triangleAtlasAllocatedBytes(),
                gpuRes_->triangleAtlasLiveBytes());
            lastDebugContext = glue->contextid;
            lastDebugParts = parts;
        }
    }
    return true;
}


} // namespace internal
} // namespace Obol
