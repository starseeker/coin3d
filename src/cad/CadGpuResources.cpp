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

#include "CadGpuResources.h"

#include <Inventor/system/gl.h>
#include "glue/glp.h"

namespace obol {
namespace internal {

// ---------------------------------------------------------------------------
// Destructor
// ---------------------------------------------------------------------------

CadGpuResources::~CadGpuResources()
{
    // GL resources must be released explicitly via releaseAll() while the
    // correct context is current.  The destructor cannot safely delete
    // GPU objects because it may be called without a current GL context.
}

// ---------------------------------------------------------------------------
// Internal delete helpers
// ---------------------------------------------------------------------------

void CadGpuResources::deleteWireGpu(CadWireGpu& w, const SoGLContext * glue)
{
    if (w.vao && glue->glDeleteVertexArrays) {
        glue->glDeleteVertexArrays(1, &w.vao);
        w.vao = 0;
    }
    if (w.posBuf && glue->glDeleteBuffers) {
        glue->glDeleteBuffers(1, &w.posBuf);
        w.posBuf = 0;
    }
    if (w.segIdxBuf && glue->glDeleteBuffers) {
        glue->glDeleteBuffers(1, &w.segIdxBuf);
        w.segIdxBuf = 0;
    }
    w.segCount  = 0;
    w.vertCount = 0;
}

void CadGpuResources::deleteTriGpu(CadTriGpu& t, const SoGLContext * glue)
{
    if (t.vao && glue->glDeleteVertexArrays) {
        glue->glDeleteVertexArrays(1, &t.vao);
        t.vao = 0;
    }
    if (t.posBuf && glue->glDeleteBuffers) {
        glue->glDeleteBuffers(1, &t.posBuf);
        t.posBuf = 0;
    }
    if (t.normBuf && glue->glDeleteBuffers) {
        glue->glDeleteBuffers(1, &t.normBuf);
        t.normBuf = 0;
    }
    if (t.idxBuf && glue->glDeleteBuffers) {
        glue->glDeleteBuffers(1, &t.idxBuf);
        t.idxBuf = 0;
    }
    t.idxCount = 0;
}

// ---------------------------------------------------------------------------
// upload()
// ---------------------------------------------------------------------------

void CadGpuResources::upload(
        PartId pid,
        const float*    wireData,    GLsizei wireCount,
        const uint32_t* segIdx,      GLsizei segIdxCount,
        const float*    triPos,      GLsizei triPosCount,
        const float*    triNorm,
        const uint32_t* triIdx,      GLsizei triIdxCount,
        uint64_t        generation,
        const SoGLContext * glue,
        const CadGLCaps& caps)
{
    if (!glue || !caps.hasVBO) return;

    auto& entry = cache_[pid];

    // Already up-to-date?
    if (entry.generation == generation) return;

    // Release stale resources first
    deleteWireGpu(entry.wire, glue);
    deleteTriGpu(entry.tri, glue);
    entry.generation = generation;

    // --- Wire geometry ---
    if (wireData && wireCount > 0 && segIdx && segIdxCount > 0) {
        CadWireGpu& w = entry.wire;
        w.vertCount = wireCount;
        w.segCount  = segIdxCount / 2;

        glue->glGenBuffers(1, &w.posBuf);
        glue->glBindBuffer(GL_ARRAY_BUFFER, w.posBuf);
        glue->glBufferData(GL_ARRAY_BUFFER,
                           static_cast<GLsizeiptr>(wireCount) * 3 * sizeof(float),
                           wireData, GL_STATIC_DRAW);

        glue->glGenBuffers(1, &w.segIdxBuf);
        glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, w.segIdxBuf);
        glue->glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                           static_cast<GLsizeiptr>(segIdxCount) * sizeof(uint32_t),
                           segIdx, GL_STATIC_DRAW);

        // Build VAO for wire geometry
        if (caps.hasVAO && glue->glGenVertexArrays) {
            glue->glGenVertexArrays(1, &w.vao);
            glue->glBindVertexArray(w.vao);

            glue->glBindBuffer(GL_ARRAY_BUFFER, w.posBuf);
            glue->glVertexAttribPointerARB(0, 3, GL_FLOAT, GL_FALSE,
                                           3 * sizeof(float), nullptr);
            glue->glEnableVertexAttribArrayARB(0);

            glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, w.segIdxBuf);

            // Unbind VAO first, then the VBOs
            glue->glBindVertexArray(0);
        }

        glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
        glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    // --- Triangle geometry ---
    if (triPos && triPosCount > 0 && triIdx && triIdxCount > 0) {
        CadTriGpu& t = entry.tri;
        t.idxCount = triIdxCount;

        glue->glGenBuffers(1, &t.posBuf);
        glue->glBindBuffer(GL_ARRAY_BUFFER, t.posBuf);
        glue->glBufferData(GL_ARRAY_BUFFER,
                           static_cast<GLsizeiptr>(triPosCount) * 3 * sizeof(float),
                           triPos, GL_STATIC_DRAW);

        if (triNorm) {
            glue->glGenBuffers(1, &t.normBuf);
            glue->glBindBuffer(GL_ARRAY_BUFFER, t.normBuf);
            glue->glBufferData(GL_ARRAY_BUFFER,
                               static_cast<GLsizeiptr>(triPosCount) * 3 * sizeof(float),
                               triNorm, GL_STATIC_DRAW);
        }

        glue->glGenBuffers(1, &t.idxBuf);
        glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, t.idxBuf);
        glue->glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                           static_cast<GLsizeiptr>(triIdxCount) * sizeof(uint32_t),
                           triIdx, GL_STATIC_DRAW);

        // Build VAO for triangle geometry
        if (caps.hasVAO && glue->glGenVertexArrays) {
            glue->glGenVertexArrays(1, &t.vao);
            glue->glBindVertexArray(t.vao);

            // Attribute 0: position
            glue->glBindBuffer(GL_ARRAY_BUFFER, t.posBuf);
            glue->glVertexAttribPointerARB(0, 3, GL_FLOAT, GL_FALSE,
                                           3 * sizeof(float), nullptr);
            glue->glEnableVertexAttribArrayARB(0);

            // Attribute 1: normal (if present)
            if (t.normBuf) {
                glue->glBindBuffer(GL_ARRAY_BUFFER, t.normBuf);
                glue->glVertexAttribPointerARB(1, 3, GL_FLOAT, GL_FALSE,
                                               3 * sizeof(float), nullptr);
                glue->glEnableVertexAttribArrayARB(1);
            }

            glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, t.idxBuf);

            glue->glBindVertexArray(0);
        }

        glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
        glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
}

// ---------------------------------------------------------------------------
// isUpToDate()
// ---------------------------------------------------------------------------

bool CadGpuResources::isUpToDate(PartId pid, uint64_t gen) const
{
    auto it = cache_.find(pid);
    if (it == cache_.end()) return false;
    return it->second.generation == gen;
}

// ---------------------------------------------------------------------------
// Access
// ---------------------------------------------------------------------------

const CadWireGpu* CadGpuResources::wireFor(PartId pid) const
{
    auto it = cache_.find(pid);
    if (it == cache_.end() || it->second.wire.vertCount == 0) return nullptr;
    return &it->second.wire;
}

const CadTriGpu* CadGpuResources::triFor(PartId pid) const
{
    auto it = cache_.find(pid);
    if (it == cache_.end() || it->second.tri.idxCount == 0) return nullptr;
    return &it->second.tri;
}

// ---------------------------------------------------------------------------
// invalidatePart()
// ---------------------------------------------------------------------------

void CadGpuResources::invalidatePart(PartId pid, const SoGLContext * glue)
{
    auto it = cache_.find(pid);
    if (it == cache_.end()) return;
    if (glue) {
        deleteWireGpu(it->second.wire, glue);
        deleteTriGpu(it->second.tri,  glue);
    }
    cache_.erase(it);
}

// ---------------------------------------------------------------------------
// uploadInstanceData()
// ---------------------------------------------------------------------------

void CadGpuResources::uploadInstanceData(const void* data, GLsizeiptr byteSize,
                                         const SoGLContext * glue)
{
    if (!glue || !glue->glGenBuffers) return;

    if (!instanceVbo_) {
        glue->glGenBuffers(1, &instanceVbo_);
    }
    glue->glBindBuffer(GL_ARRAY_BUFFER, instanceVbo_);
    glue->glBufferData(GL_ARRAY_BUFFER, byteSize, data, GL_STREAM_DRAW);
    glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// ---------------------------------------------------------------------------
// releaseAll()
// ---------------------------------------------------------------------------

void CadGpuResources::releaseAll(const SoGLContext * glue)
{
    if (glue) {
        for (auto& kv : cache_) {
            deleteWireGpu(kv.second.wire, glue);
            deleteTriGpu(kv.second.tri,   glue);
        }
        if (instanceVbo_ && glue->glDeleteBuffers) {
            glue->glDeleteBuffers(1, &instanceVbo_);
        }
    }
    cache_.clear();
    instanceVbo_ = 0;
}

} // namespace internal
} // namespace obol
