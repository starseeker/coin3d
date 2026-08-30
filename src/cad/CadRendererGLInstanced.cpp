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

/** @file CadRendererGLInstanced.cpp @brief Instanced CAD draw submission. */

#include "CadRendererGL.h"
#include "CadRendererConfiguration.h"
#include "CadRendererGLExecutorUtils.h"
#include "CadResolvedDraw.h"
#include "CadShaderSources.h"

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
void CadRendererGL::renderInstanced(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const SoGLContext*   glue,
        const SbMatrix&      viewProj,
        const SbViewVolume&  viewVolume,
        const std::unordered_map<PartId, uint64_t,
                                 std::hash<PartId>>& /*partGenMap*/,
        bool drawWire,
        bool solidWireOnly,
        bool drawShaded)
{
    size_t deadlineWork = 256u;
    bool interrupted = renderInterruptedAfter(deadlineWork);
    if (interrupted)
        return;

    // Build per-instance vertex data (transform + colour)
    const size_t nInst = plan.visibleInstances.size();
    if (nInst == 0) return;
    uint64_t renderedTriangleCount = 0;

    std::vector<InstVertex> instData(nInst);
    for (size_t i = 0; i < nInst; ++i) {
        if (renderInterruptedAfter(deadlineWork))
            return;
        const auto& vi = plan.visibleInstances[i];
        std::memcpy(instData[i].transform, vi.transform.data(), 16 * sizeof(float));
        cadPackInstanceNormalTransform(
            vi.transform.data(), instData[i].normalTransform);
        instData[i].color[0] = vi.rgba[0] / 255.0f;
        instData[i].color[1] = vi.rgba[1] / 255.0f;
        instData[i].color[2] = vi.rgba[2] / 255.0f;
        instData[i].color[3] = vi.rgba[3] / 255.0f;
    }

    gpuRes_->uploadTransientInstanceData(
        instData.data(),
        static_cast<GLsizeiptr>(nInst * sizeof(InstVertex)),
        glue);
    if (renderInterruptedAfter(deadlineWork))
        return;

    const GLuint instVbo = gpuRes_->transientInstanceVbo();
    const float* vp = viewProj[0];
    const GLsizei instStride = static_cast<GLsizei>(sizeof(InstVertex));

    // --- Helper to bind per-instance attributes ---
    //
    // Must be called with the correct VAO already bound (if any).  The
    // baseInstance parameter is the index of the first instance for this draw
    // item in the per-frame instance VBO; it is used as a byte offset so each
    // part reads its own slice of the buffer without needing GL 4.2
    // glDrawElementsInstancedBaseInstance.
    auto bindInstAttribs = [&](uint32_t baseInstance) {
        glue->glBindBuffer(GL_ARRAY_BUFFER, instVbo);

        const GLsizeiptr baseOff =
            static_cast<GLsizeiptr>(baseInstance) * instStride;

        // a_instTransform occupies 4 consecutive attribute locations.
        // We use the fixed layout (kInstTransformLoc..kInstTransformLoc+3).
        for (GLuint col = 0; col < 4; ++col) {
            GLuint aloc = kInstTransformLoc + col;
            const GLvoid* off = reinterpret_cast<const GLvoid*>(
                baseOff +
                static_cast<GLsizeiptr>(offsetof(InstVertex, transform)) +
                static_cast<GLsizeiptr>(col) * 4 * static_cast<GLsizeiptr>(sizeof(float)));
            glue->glVertexAttribPointerARB(aloc, 4, GL_FLOAT, GL_FALSE,
                                           instStride, off);
            glue->glEnableVertexAttribArrayARB(aloc);
            glue->glVertexAttribDivisor(aloc, 1);
        }

        // a_instNormalTransform occupies three consecutive mat3 locations.
        for (GLuint col = 0; col < 3; ++col) {
            const GLuint aloc = kInstNormalTransformLoc + col;
            const GLvoid* off = reinterpret_cast<const GLvoid*>(
                baseOff + static_cast<GLsizeiptr>(
                    offsetof(InstVertex, normalTransform)) +
                static_cast<GLsizeiptr>(col) * 3 *
                    static_cast<GLsizeiptr>(sizeof(float)));
            glue->glVertexAttribPointerARB(aloc, 3, GL_FLOAT, GL_FALSE,
                                           instStride, off);
            glue->glEnableVertexAttribArrayARB(aloc);
            glue->glVertexAttribDivisor(aloc, 1);
        }

        // a_instColor
        {
            GLuint aloc = kInstColorLoc;
            const GLvoid* off = reinterpret_cast<const GLvoid*>(
                baseOff +
                static_cast<GLsizeiptr>(offsetof(InstVertex, color)));
            glue->glVertexAttribPointerARB(aloc, 4, GL_FLOAT, GL_FALSE,
                                           instStride, off);
            glue->glEnableVertexAttribArrayARB(aloc);
            glue->glVertexAttribDivisor(aloc, 1);
        }
        glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
    };

    // Must also be called with the same VAO still bound so the cleanup state
    // is recorded there (resets divisors to 0, disables the attribs).
    auto unbindInstAttribs = [&]() {
        for (GLuint col = 0; col < 4; ++col) {
            glue->glVertexAttribDivisor(kInstTransformLoc + col, 0);
            glue->glDisableVertexAttribArrayARB(kInstTransformLoc + col);
        }
        for (GLuint col = 0; col < 3; ++col) {
            const GLuint aloc = kInstNormalTransformLoc + col;
            glue->glVertexAttribDivisor(aloc, 0);
            glue->glDisableVertexAttribArrayARB(aloc);
        }
        glue->glVertexAttribDivisor(kInstColorLoc, 0);
        glue->glDisableVertexAttribArrayARB(kInstColorLoc);
    };

    // --- Wire pass ---
    if (drawWire && !plan.wireItems.empty()) {
        const CadWireRasterState rasterState = captureWireRasterState(
            glue, caps_.hasLineStipple);
        struct WireLocations {
            GLint viewProjection = -1;
            GLint position = 0;
            GLint encodeScale = -1;
            GLint decodeScale = -1;
            GLint minimum = -1;
        };
        const GLuint programs[2] = {
            shaders_.wireInst, shaders_.wirePopInst
        };
        WireLocations locations[2];
        for (int variant = 0; variant < 2; ++variant) {
            if (!programs[variant])
                continue;
            locations[variant].viewProjection =
                glue->glGetUniformLocationARB(
                    programs[variant], "u_viewProj");
            locations[variant].position =
                glue->glGetAttribLocationARB(programs[variant], "a_pos");
            if (locations[variant].position < 0)
                locations[variant].position = 0;
        }
        if (programs[1]) {
            locations[1].encodeScale = glue->glGetUniformLocationARB(
                programs[1], "u_popEncodeScale");
            locations[1].decodeScale = glue->glGetUniformLocationARB(
                programs[1], "u_popDecodeScale");
            locations[1].minimum = glue->glGetUniformLocationARB(
                programs[1], "u_popMin");
        }
        GLuint activeProgram = 0;

        for (const auto& item : plan.wireItems) {
            if (renderInterruptedAfter(deadlineWork)) {
                interrupted = true;
                break;
            }
            if (solidWireOnly && item.customWireStyle) continue;
            CadWireGpu* w = gpuRes_->wireFor(item.rep.part);
            if (!w || w->segCount == 0) continue;
            const PartGeometry *geometry =
                assembly.partGeometry(item.rep.part);
            const WireRep *progressive =
                geometry && geometry->wire &&
                geometry->wire->isProgressive() ?
                &*geometry->wire : nullptr;
            uint32_t runStart = 0;
            while (runStart < item.instanceCount) {
                if (renderInterruptedAfter(deadlineWork)) {
                    interrupted = true;
                    break;
                }
                while (runStart < item.instanceCount &&
                    !cadInstanceDrawable(
                        plan, item, item.baseInstance + runStart,
                        CadDrawChannel::Wire)) {
                    if (renderInterruptedAfter(deadlineWork)) {
                        interrupted = true;
                        break;
                    }
                    ++runStart;
                }
                if (interrupted)
                    break;
                if (runStart == item.instanceCount)
                    break;
                const uint32_t baseInstance =
                    item.baseInstance + runStart;
                const CadVisibleInstance& levelInstance =
                    plan.visibleInstances[baseInstance];
                const uint8_t level = progressive ?
                    cadResolvedProgressiveCut(
                        effectiveProgressiveCut(
                            item.rep.part,
                            levelInstance.lodCut),
                        progressive->progressiveMinimumCut,
                        progressive->progressiveResidentCut) :
                    Obol::ProgressiveCutUnspecified;
                uint32_t runEnd = runStart + 1;
                while (runEnd < item.instanceCount &&
                    cadInstanceDrawable(
                        plan, item, item.baseInstance + runEnd,
                        CadDrawChannel::Wire) &&
                    (!progressive ||
                     cadResolvedProgressiveCut(
                        effectiveProgressiveCut(
                            item.rep.part,
                            plan.visibleInstances[
                                item.baseInstance + runEnd].lodCut),
                        progressive->progressiveMinimumCut,
                        progressive->progressiveResidentCut) == level))
                {
                    if (renderInterruptedAfter(deadlineWork)) {
                        interrupted = true;
                        break;
                    }
                    ++runEnd;
                }
                if (interrupted)
                    break;

                const int variant = progressive &&
                    !progressive->quantizationAtCut(level).isExact() ? 1 : 0;
                const WireLocations& loc = locations[variant];
                if (!programs[variant]) {
                    runStart = runEnd;
                    continue;
                }
                if (activeProgram != programs[variant]) {
                    activeProgram = programs[variant];
                    glue->glUseProgramObjectARB(activeProgram);
                    glue->glUniformMatrix4fvARB(
                        loc.viewProjection, 1, GL_FALSE, vp);
                }
                if (variant) {
                    uploadProgressivePositionUniforms(
                        glue, loc.encodeScale, loc.decodeScale, loc.minimum,
                        progressive->quantizationAtCut(level),
                        progressive->progressiveQuantizationMinimum,
                        progressive->progressiveQuantizationMaximum);
                }
                const GLsizei segmentFirst = progressive ?
                    static_cast<GLsizei>(
                        progressive->segmentFirstAtCut(level)) : 0;
                const GLsizei segmentCount = progressive ?
                    static_cast<GLsizei>(
                        progressive->segmentCountAtCut(level)) :
                    w->segCount;
                if (segmentCount <= 0) {
                    runStart = runEnd;
                    continue;
                }
                const auto& styleInst = plan.visibleInstances[baseInstance];
                applyWireRasterStyle(glue, styleInst, caps_.hasLineStipple);
                if (w->vao && glue->glBindVertexArray) {
                    glue->glBindVertexArray(w->vao);
                    if (w->instanceVbo != instVbo ||
                            w->instanceBase != baseInstance) {
                        bindInstAttribs(baseInstance);
                        w->instanceVbo = instVbo;
                        w->instanceBase = baseInstance;
                    }
                } else {
                    glue->glBindBuffer(GL_ARRAY_BUFFER, w->posBuf);
                    glue->glVertexAttribPointerARB(
                                                   static_cast<GLuint>(loc.position), 3,
                                                   GL_FLOAT, GL_FALSE,
                                                   3 * sizeof(float), nullptr);
                    glue->glEnableVertexAttribArrayARB(
                        static_cast<GLuint>(loc.position));
                    if (!w->sequentialSegments)
                        glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, w->segIdxBuf);
                    bindInstAttribs(baseInstance);
                }

                const GLsizei runCount = static_cast<GLsizei>(runEnd - runStart);
                if (w->sequentialSegments) {
                    glue->glDrawArraysInstanced(
                                                GL_LINES, segmentFirst * 2,
                                                segmentCount * 2,
                                                runCount);
                } else {
                    glue->glDrawElementsInstanced(
                                                  GL_LINES, segmentCount * 2,
                                                  GL_UNSIGNED_INT,
                                                  reinterpret_cast<const GLvoid *>(
                                                      static_cast<uintptr_t>(segmentFirst) * 2u *
                                                      sizeof(uint32_t)),
                                                  runCount);
                }
                cadAccumulateRenderedWireWork(
                    lastRenderedWork_,
                    static_cast<uint64_t>(segmentCount),
                    static_cast<uint64_t>(runCount));

                if (w->vao && glue->glBindVertexArray) {
                    glue->glBindVertexArray(0);
                } else {
                    unbindInstAttribs();
                    glue->glDisableVertexAttribArrayARB(
                        static_cast<GLuint>(loc.position));
                    glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
                    if (!w->sequentialSegments)
                        glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
                }
                runStart = runEnd;
            }
            if (interrupted)
                break;
        }

        glue->glUseProgramObjectARB(0);
        restoreWireRasterState(glue, rasterState, caps_.hasLineStipple);
    }
    if (interrupted)
        return;

    // --- Shaded pass ---
    if (drawShaded && !plan.shadedItems.empty()) {
        struct ShadedLocations {
            GLint viewProjection = -1;
            GLint hasNormal = -1;
            GLint position = 0;
            GLint normal = 1;
            GLint encodeScale = -1;
            GLint decodeScale = -1;
            GLint minimum = -1;
        };
        const GLuint programs[2] = {
            shaders_.shadedInst, shaders_.shadedPopInst
        };
        ShadedLocations locations[2];
        for (int variant = 0; variant < 2; ++variant) {
            if (!programs[variant])
                continue;
            locations[variant].viewProjection =
                glue->glGetUniformLocationARB(
                    programs[variant], "u_viewProj");
            locations[variant].hasNormal =
                glue->glGetUniformLocationARB(
                    programs[variant], "u_hasNorm");
            locations[variant].position =
                glue->glGetAttribLocationARB(programs[variant], "a_pos");
            locations[variant].normal =
                glue->glGetAttribLocationARB(programs[variant], "a_norm");
            if (locations[variant].position < 0)
                locations[variant].position = 0;
            if (locations[variant].normal < 0)
                locations[variant].normal = 1;
        }
        if (programs[1]) {
            locations[1].encodeScale = glue->glGetUniformLocationARB(
                programs[1], "u_popEncodeScale");
            locations[1].decodeScale = glue->glGetUniformLocationARB(
                programs[1], "u_popDecodeScale");
            locations[1].minimum = glue->glGetUniformLocationARB(
                programs[1], "u_popMin");
        }
        GLuint activeProgram = 0;
        bool lightsUploaded[2] = {false, false};

        for (const auto& item : plan.shadedItems) {
            if (renderInterruptedAfter(deadlineWork)) {
                interrupted = true;
                break;
            }
            CadTriGpu* t = gpuRes_->triFor(item.rep.part);
            if (!t || t->idxCount == 0) continue;
            size_t levelInstanceIndex = plan.visibleInstances.size();
            for (uint32_t instanceOffset = 0;
                    instanceOffset < item.instanceCount; ++instanceOffset) {
                if (renderInterruptedAfter(deadlineWork)) {
                    interrupted = true;
                    break;
                }
                const size_t candidate =
                    item.baseInstance + instanceOffset;
                if (cadInstanceDrawable(
                        plan, item, candidate, CadDrawChannel::Shaded)) {
                    levelInstanceIndex = candidate;
                    break;
                }
            }
            if (interrupted)
                break;
            if (levelInstanceIndex >= plan.visibleInstances.size())
                continue;
            const PartGeometry *geometry =
                assembly.partGeometry(item.rep.part);
            const TriMesh *progressive =
                geometry && geometry->shaded &&
                geometry->shaded->isProgressive() ?
                &*geometry->shaded : nullptr;
            const CadVisibleInstance& levelInstance =
                plan.visibleInstances[levelInstanceIndex];
            const uint8_t level = progressive ?
                cadResolvedProgressiveCut(
                    effectiveProgressiveCut(
                        item.rep.part,
                        levelInstance.lodCut),
                    progressive->progressiveMinimumCut,
                    progressive->progressiveResidentCut) :
                Obol::ProgressiveCutUnspecified;
            const int variant = progressive &&
                !progressive->quantizationAtCut(level).isExact() ? 1 : 0;
            const ShadedLocations& loc = locations[variant];
            if (!programs[variant])
                continue;
            if (activeProgram != programs[variant]) {
                activeProgram = programs[variant];
                glue->glUseProgramObjectARB(activeProgram);
                glue->glUniformMatrix4fvARB(
                    loc.viewProjection, 1, GL_FALSE, vp);
                if (!lightsUploaded[variant]) {
                    this->uploadLights(glue, activeProgram);
                    this->uploadViewFacing(
                        glue, activeProgram, viewVolume);
                    lightsUploaded[variant] = true;
                }
            }
            if (variant) {
                uploadProgressivePositionUniforms(
                    glue, loc.encodeScale, loc.decodeScale, loc.minimum,
                    progressive->quantizationAtCut(level),
                    progressive->progressiveQuantizationMinimum,
                    progressive->progressiveQuantizationMaximum);
            }
            const GLsizei indexCount = progressiveTriangleIndexCount(
                assembly, activeViewState(), item.rep.part, levelInstance, t->idxCount);
            if (indexCount <= 0)
                continue;

            setCadBackfaceCulling(glue,
                cadProgressiveCutCullSafe(
                    item.cullBackfaces, progressive, level));
            glue->glUniform1iARB(
                loc.hasNormal, (t->normBuf != 0) ? 1 : 0);

            const bool retainedVao =
                t->vao && glue->glBindVertexArray;
            if (retainedVao) {
                glue->glBindVertexArray(t->vao);
            } else {
                glue->glBindBuffer(GL_ARRAY_BUFFER, t->posBuf);
                glue->glVertexAttribPointerARB(
                                               static_cast<GLuint>(loc.position), 3,
                                               GL_FLOAT, GL_FALSE,
                                               3 * sizeof(float), nullptr);
                glue->glEnableVertexAttribArrayARB(
                    static_cast<GLuint>(loc.position));
                if (t->normBuf && loc.normal >= 0) {
                    glue->glBindBuffer(GL_ARRAY_BUFFER, t->normBuf);
                    glue->glVertexAttribPointerARB(
                                                   static_cast<GLuint>(loc.normal), 3,
                                                   GL_FLOAT, GL_FALSE,
                                                   3 * sizeof(float), nullptr);
                    glue->glEnableVertexAttribArrayARB(
                        static_cast<GLuint>(loc.normal));
                }
                glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, t->idxBuf);
            }
            uint32_t runStart = 0;
            while (runStart < item.instanceCount) {
                if (renderInterruptedAfter(deadlineWork)) {
                    interrupted = true;
                    break;
                }
                while (runStart < item.instanceCount &&
                        !cadInstanceDrawable(
                            plan, item, item.baseInstance + runStart,
                            CadDrawChannel::Shaded)) {
                    if (renderInterruptedAfter(deadlineWork)) {
                        interrupted = true;
                        break;
                    }
                    ++runStart;
                }
                if (interrupted)
                    break;
                if (runStart == item.instanceCount)
                    break;
                uint32_t runEnd = runStart + 1;
                while (runEnd < item.instanceCount &&
                        cadInstanceDrawable(
                            plan, item, item.baseInstance + runEnd,
                            CadDrawChannel::Shaded)) {
                    if (renderInterruptedAfter(deadlineWork)) {
                        interrupted = true;
                        break;
                    }
                    ++runEnd;
                }
                if (interrupted)
                    break;
                const uint32_t baseInstance =
                    item.baseInstance + runStart;
                if (!retainedVao ||
                        t->instanceVbo != instVbo ||
                        t->instanceBase != baseInstance) {
                    bindInstAttribs(baseInstance);
                    t->instanceVbo = instVbo;
                    t->instanceBase = baseInstance;
                }
                glue->glDrawElementsInstanced(
                    GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr,
                    static_cast<GLsizei>(runEnd - runStart));
                if (geometry && geometry->shaded)
                    cadAccumulateRenderedShadedWork(
                        lastRenderedWork_, *geometry->shaded, level,
                        static_cast<uint64_t>(indexCount / 3),
                        static_cast<uint64_t>(runEnd - runStart));
                renderedTriangleCount +=
                    static_cast<uint64_t>(indexCount / 3) *
                    static_cast<uint64_t>(runEnd - runStart);
                runStart = runEnd;
            }

            if (retainedVao) {
                glue->glBindVertexArray(0);
            } else {
                unbindInstAttribs();
                if (t->normBuf && loc.normal >= 0) {
                    glue->glDisableVertexAttribArrayARB(
                        static_cast<GLuint>(loc.normal));
                }
                glue->glDisableVertexAttribArrayARB(
                    static_cast<GLuint>(loc.position));
                glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
                glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            }
            if (interrupted)
                break;
        }

        glue->glUseProgramObjectARB(0);
        lastRenderedTriangleCount_ =
            renderedTriangleCount >
                    UINT64_MAX - lastRenderedTriangleCount_ ?
                UINT64_MAX :
                lastRenderedTriangleCount_ + renderedTriangleCount;
    }
}

} // namespace internal
} // namespace Obol
