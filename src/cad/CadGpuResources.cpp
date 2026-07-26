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

#include <algorithm>
#include <limits>

namespace Obol {
namespace internal {

static GLsizei
progressiveBufferCapacity(GLsizei required, GLsizei current,
                          bool progressive)
{
    if (!progressive || current <= 0)
        return required;
    const GLsizei maximum = std::numeric_limits<GLsizei>::max();
    const GLsizei doubled =
        current > maximum / 2 ? maximum : current * 2;
    return std::max(required, doubled);
}

static void
allocateAndPopulateBuffer(const SoGLContext *glue, GLenum target,
                          GLsizeiptr capacityBytes,
                          GLsizeiptr logicalBytes, const void *data,
                          GLenum usage)
{
    /* glBufferData's size describes both the allocation and, when data is
     * non-null, how many bytes the driver reads from data.  Progressive
     * growth may deliberately reserve more capacity than the current vector
     * contains, so allocate the store first and upload only the logical
     * bytes. */
    if (capacityBytes > logicalBytes) {
        glue->glBufferData(target, capacityBytes, nullptr, usage);
        glue->glBufferSubData(target, 0, logicalBytes, data);
    } else {
        glue->glBufferData(target, logicalBytes, data, usage);
    }
}

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

void CadGpuResources::deletePointGpu(CadPointGpu& p, const SoGLContext * glue)
{
    if (p.vao && glue->glDeleteVertexArrays) {
        glue->glDeleteVertexArrays(1, &p.vao);
        p.vao = 0;
    }
    if (p.posBuf && glue->glDeleteBuffers) {
        glue->glDeleteBuffers(1, &p.posBuf);
        p.posBuf = 0;
    }
    p.count = 0;
    p.posCapacity = 0;
}

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
    w.posCapacity = 0;
    w.idxCount = 0;
    w.idxCapacity = 0;
    w.sequentialSegments = false;
    w.instanceVbo = 0;
    w.instanceBase = UINT32_MAX;
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
    t.vertCount = 0;
    t.idxCount = 0;
    t.posCapacity = 0;
    t.normCapacity = 0;
    t.idxCapacity = 0;
    t.instanceVbo = 0;
    t.instanceBase = UINT32_MAX;
}

// ---------------------------------------------------------------------------
// upload()
// ---------------------------------------------------------------------------

void CadGpuResources::upload(
        PartId pid,
        const float*    pointData,   GLsizei pointCount,
        const float*    wireData,    GLsizei wireCount,
        const uint32_t* segIdx,      GLsizei segIdxCount,
        const float*    triPos,      GLsizei triPosCount,
        const float*    triNorm,
        const uint32_t* triIdx,      GLsizei triIdxCount,
        uint64_t        generation,
        bool            progressive,
        const SoGLContext * glue,
        const CadGLCaps& caps)
{
    if (!glue || !caps.hasVBO) return;

    auto& entry = cache_[pid];

    // Already up-to-date?
    if (entry.generation == generation) return;

    /* Ordinary geometry replacement is allowed to change every byte and
     * representation.  Producer-declared progressive geometry is a stricter
     * cumulative-prefix contract, so preserve its buffer objects below and
     * append only newly resident tails. */
    if (!progressive) {
        deletePointGpu(entry.point, glue);
        deleteWireGpu(entry.wire, glue);
        deleteTriGpu(entry.tri, glue);
    } else {
        /* Point primitives have no retained prefix contract. */
        deletePointGpu(entry.point, glue);
    }
    entry.generation = generation;

    if (pointData && pointCount > 0) {
        CadPointGpu& p = entry.point;
        p.count = pointCount;
        p.posCapacity = pointCount;
        glue->glGenBuffers(1, &p.posBuf);
        glue->glBindBuffer(GL_ARRAY_BUFFER, p.posBuf);
        glue->glBufferData(GL_ARRAY_BUFFER,
                           static_cast<GLsizeiptr>(pointCount) * 3 * sizeof(float),
                           pointData, GL_STATIC_DRAW);
        if (caps.hasVAO && glue->glGenVertexArrays) {
            glue->glGenVertexArrays(1, &p.vao);
            glue->glBindVertexArray(p.vao);
            glue->glBindBuffer(GL_ARRAY_BUFFER, p.posBuf);
            glue->glVertexAttribPointerARB(0, 3, GL_FLOAT, GL_FALSE,
                                           3 * sizeof(float), nullptr);
            glue->glEnableVertexAttribArrayARB(0);
            glue->glBindVertexArray(0);
        }
        glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    // --- Wire geometry ---
    if (wireData && wireCount > 0 &&
            ((segIdx && segIdxCount > 0) || (!segIdx && wireCount >= 2))) {
        CadWireGpu& w = entry.wire;
        const bool sequential = (segIdx == nullptr);
        const bool appendable = progressive && w.posBuf &&
            w.sequentialSegments == sequential &&
            wireCount >= w.vertCount &&
            (sequential || (w.segIdxBuf && segIdxCount >= w.idxCount));

        if (!appendable && w.posBuf)
            deleteWireGpu(w, glue);

        const GLsizei oldWireCount = w.vertCount;
        const GLsizei oldSegIdxCount = w.idxCount;
        const bool newWireBuffers = !w.posBuf;
        if (newWireBuffers)
            glue->glGenBuffers(1, &w.posBuf);
        glue->glBindBuffer(GL_ARRAY_BUFFER, w.posBuf);
        if (newWireBuffers || wireCount > w.posCapacity) {
            const GLsizei capacity = progressiveBufferCapacity(
                wireCount, w.posCapacity, progressive);
            allocateAndPopulateBuffer(
                glue, GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(capacity) * 3 * sizeof(float),
                static_cast<GLsizeiptr>(wireCount) * 3 * sizeof(float),
                wireData, progressive ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
            w.posCapacity = capacity;
        } else if (wireCount > oldWireCount) {
            glue->glBufferSubData(
                GL_ARRAY_BUFFER,
                static_cast<GLintptr>(oldWireCount) * 3 * sizeof(float),
                static_cast<GLsizeiptr>(wireCount - oldWireCount) *
                    3 * sizeof(float),
                wireData + static_cast<size_t>(oldWireCount) * 3);
        }

        if (!sequential) {
            const bool newIndexBuffer = !w.segIdxBuf;
            if (newIndexBuffer)
                glue->glGenBuffers(1, &w.segIdxBuf);
            glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, w.segIdxBuf);
            if (newIndexBuffer || segIdxCount > w.idxCapacity) {
                const GLsizei capacity = progressiveBufferCapacity(
                    segIdxCount, w.idxCapacity, progressive);
                allocateAndPopulateBuffer(
                    glue, GL_ELEMENT_ARRAY_BUFFER,
                    static_cast<GLsizeiptr>(capacity) * sizeof(uint32_t),
                    static_cast<GLsizeiptr>(segIdxCount) * sizeof(uint32_t),
                    segIdx,
                    progressive ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
                w.idxCapacity = capacity;
            } else if (segIdxCount > oldSegIdxCount) {
                glue->glBufferSubData(
                    GL_ELEMENT_ARRAY_BUFFER,
                    static_cast<GLintptr>(oldSegIdxCount) * sizeof(uint32_t),
                    static_cast<GLsizeiptr>(segIdxCount - oldSegIdxCount) *
                        sizeof(uint32_t),
                    segIdx + oldSegIdxCount);
            }
        }
        w.vertCount = wireCount;
        w.idxCount = sequential ? 0 : segIdxCount;
        w.sequentialSegments = sequential;
        w.segCount = sequential ? wireCount / 2 : segIdxCount / 2;

        // Build VAO for wire geometry
        if (caps.hasVAO && glue->glGenVertexArrays && !w.vao) {
            glue->glGenVertexArrays(1, &w.vao);
            glue->glBindVertexArray(w.vao);

            glue->glBindBuffer(GL_ARRAY_BUFFER, w.posBuf);
            glue->glVertexAttribPointerARB(0, 3, GL_FLOAT, GL_FALSE,
                                           3 * sizeof(float), nullptr);
            glue->glEnableVertexAttribArrayARB(0);

            if (!w.sequentialSegments)
                glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, w.segIdxBuf);

            // Unbind VAO first, then the VBOs
            glue->glBindVertexArray(0);
        }

        glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
        glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    } else if (progressive && entry.wire.posBuf) {
        deleteWireGpu(entry.wire, glue);
    }

    // --- Triangle geometry ---
    if (triPos && triPosCount > 0 && triIdx && triIdxCount > 0) {
        CadTriGpu& t = entry.tri;
        const bool normalsMatch = static_cast<bool>(triNorm) ==
            static_cast<bool>(t.normBuf);
        const bool appendable = progressive && t.posBuf && t.idxBuf &&
            normalsMatch && triPosCount >= t.vertCount &&
            triIdxCount >= t.idxCount;
        if (!appendable && t.posBuf)
            deleteTriGpu(t, glue);

        const GLsizei oldVertCount = t.vertCount;
        const GLsizei oldIdxCount = t.idxCount;
        const bool newPosBuffer = !t.posBuf;
        if (newPosBuffer)
            glue->glGenBuffers(1, &t.posBuf);
        glue->glBindBuffer(GL_ARRAY_BUFFER, t.posBuf);
        if (newPosBuffer || triPosCount > t.posCapacity) {
            const GLsizei capacity = progressiveBufferCapacity(
                triPosCount, t.posCapacity, progressive);
            allocateAndPopulateBuffer(
                glue, GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(capacity) * 3 * sizeof(float),
                static_cast<GLsizeiptr>(triPosCount) * 3 * sizeof(float),
                triPos, progressive ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
            t.posCapacity = capacity;
        } else if (triPosCount > oldVertCount) {
            glue->glBufferSubData(
                GL_ARRAY_BUFFER,
                static_cast<GLintptr>(oldVertCount) * 3 * sizeof(float),
                static_cast<GLsizeiptr>(triPosCount - oldVertCount) *
                    3 * sizeof(float),
                triPos + static_cast<size_t>(oldVertCount) * 3);
        }

        if (triNorm) {
            const bool newNormBuffer = !t.normBuf;
            if (newNormBuffer)
                glue->glGenBuffers(1, &t.normBuf);
            glue->glBindBuffer(GL_ARRAY_BUFFER, t.normBuf);
            if (newNormBuffer || triPosCount > t.normCapacity) {
                const GLsizei capacity = progressiveBufferCapacity(
                    triPosCount, t.normCapacity, progressive);
                allocateAndPopulateBuffer(
                    glue, GL_ARRAY_BUFFER,
                    static_cast<GLsizeiptr>(capacity) * 3 * sizeof(float),
                    static_cast<GLsizeiptr>(triPosCount) * 3 * sizeof(float),
                    triNorm,
                    progressive ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
                t.normCapacity = capacity;
            } else if (triPosCount > oldVertCount) {
                glue->glBufferSubData(
                    GL_ARRAY_BUFFER,
                    static_cast<GLintptr>(oldVertCount) * 3 * sizeof(float),
                    static_cast<GLsizeiptr>(triPosCount - oldVertCount) *
                        3 * sizeof(float),
                    triNorm + static_cast<size_t>(oldVertCount) * 3);
            }
        }

        const bool newIndexBuffer = !t.idxBuf;
        if (newIndexBuffer)
            glue->glGenBuffers(1, &t.idxBuf);
        glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, t.idxBuf);
        if (newIndexBuffer || triIdxCount > t.idxCapacity) {
            const GLsizei capacity = progressiveBufferCapacity(
                triIdxCount, t.idxCapacity, progressive);
            allocateAndPopulateBuffer(
                glue, GL_ELEMENT_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(capacity) * sizeof(uint32_t),
                static_cast<GLsizeiptr>(triIdxCount) * sizeof(uint32_t),
                triIdx, progressive ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
            t.idxCapacity = capacity;
        } else if (triIdxCount > oldIdxCount) {
            glue->glBufferSubData(
                GL_ELEMENT_ARRAY_BUFFER,
                static_cast<GLintptr>(oldIdxCount) * sizeof(uint32_t),
                static_cast<GLsizeiptr>(triIdxCount - oldIdxCount) *
                    sizeof(uint32_t),
                triIdx + oldIdxCount);
        }
        t.vertCount = triPosCount;
        t.idxCount = triIdxCount;

        // Build VAO for triangle geometry
        if (caps.hasVAO && glue->glGenVertexArrays && !t.vao) {
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
    } else if (progressive && entry.tri.posBuf) {
        deleteTriGpu(entry.tri, glue);
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

const CadPointGpu* CadGpuResources::pointFor(PartId pid) const
{
    auto it = cache_.find(pid);
    if (it == cache_.end() || it->second.point.count == 0) return nullptr;
    return &it->second.point;
}

const CadWireGpu* CadGpuResources::wireFor(PartId pid) const
{
    auto it = cache_.find(pid);
    if (it == cache_.end() || it->second.wire.vertCount == 0) return nullptr;
    return &it->second.wire;
}

CadWireGpu* CadGpuResources::wireFor(PartId pid)
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

CadTriGpu* CadGpuResources::triFor(PartId pid)
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
        deletePointGpu(it->second.point, glue);
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

void CadGpuResources::uploadFlatWire(
        uint64_t planRevision,
        uint64_t geometryRevision,
        const std::vector<float>& positions,
        const std::vector<CadFlatWireGroup>& groups,
        const SoGLContext *glue,
        const CadGLCaps& caps)
{
    if (!glue || !caps.hasVBO || positions.empty()) return;

    if (!flatWire_.posBuf)
        glue->glGenBuffers(1, &flatWire_.posBuf);
    glue->glBindBuffer(GL_ARRAY_BUFFER, flatWire_.posBuf);
    glue->glBufferData(GL_ARRAY_BUFFER,
                       static_cast<GLsizeiptr>(positions.size() * sizeof(float)),
                       positions.data(), GL_STATIC_DRAW);

    if (!flatWire_.vao && caps.hasVAO && glue->glGenVertexArrays) {
        glue->glGenVertexArrays(1, &flatWire_.vao);
        glue->glBindVertexArray(flatWire_.vao);
        glue->glBindBuffer(GL_ARRAY_BUFFER, flatWire_.posBuf);
        glue->glVertexAttribPointerARB(0, 3, GL_FLOAT, GL_FALSE,
                                       3 * sizeof(float), nullptr);
        glue->glEnableVertexAttribArrayARB(0);
        glue->glBindVertexArray(0);
    }
    glue->glBindBuffer(GL_ARRAY_BUFFER, 0);

    flatWire_.planRevision = planRevision;
    flatWire_.geometryRevision = geometryRevision;
    flatWire_.vertexCount = static_cast<GLsizei>(positions.size() / 3);
    flatWire_.groups = groups;
}

void CadGpuResources::updateFlatWireGroups(
        uint64_t planRevision,
        const std::vector<CadFlatWireGroup>& groups)
{
    flatWire_.planRevision = planRevision;
    flatWire_.groups = groups;
}

void CadGpuResources::uploadFlatShaded(
        uint64_t planRevision,
        uint64_t geometryRevision,
        const std::vector<float>& positions,
        const std::vector<float>& normals,
        const std::vector<CadFlatShadedGroup>& groups,
        const SoGLContext *glue,
        const CadGLCaps& caps)
{
    if (!glue || positions.empty() || positions.size() != normals.size())
        return;
    if (!flatShaded_.posBuf)
        glue->glGenBuffers(1, &flatShaded_.posBuf);
    if (!flatShaded_.normBuf)
        glue->glGenBuffers(1, &flatShaded_.normBuf);
    glue->glBindBuffer(GL_ARRAY_BUFFER, flatShaded_.posBuf);
    glue->glBufferData(GL_ARRAY_BUFFER,
                       static_cast<GLsizeiptr>(positions.size() * sizeof(float)),
                       positions.data(), GL_STATIC_DRAW);
    glue->glBindBuffer(GL_ARRAY_BUFFER, flatShaded_.normBuf);
    glue->glBufferData(GL_ARRAY_BUFFER,
                       static_cast<GLsizeiptr>(normals.size() * sizeof(float)),
                       normals.data(), GL_STATIC_DRAW);

    /* Flat batches are rendered by direct GLSL calls.  Never record their
     * attribute pointers in VAO 0: Qt's QOpenGLWidget compositor also uses
     * that VAO in its shared presentation context. */
    if (!flatShaded_.vao && caps.hasVAO && glue->glGenVertexArrays) {
        glue->glGenVertexArrays(1, &flatShaded_.vao);
        glue->glBindVertexArray(flatShaded_.vao);
        glue->glBindBuffer(GL_ARRAY_BUFFER, flatShaded_.posBuf);
        glue->glVertexAttribPointerARB(0, 3, GL_FLOAT, GL_FALSE,
                                       3 * sizeof(float), nullptr);
        glue->glEnableVertexAttribArrayARB(0);
        glue->glBindBuffer(GL_ARRAY_BUFFER, flatShaded_.normBuf);
        glue->glVertexAttribPointerARB(1, 3, GL_FLOAT, GL_FALSE,
                                       3 * sizeof(float), nullptr);
        glue->glEnableVertexAttribArrayARB(1);
        glue->glBindVertexArray(0);
    }
    glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
    flatShaded_.planRevision = planRevision;
    flatShaded_.geometryRevision = geometryRevision;
    flatShaded_.vertexCount = static_cast<GLsizei>(positions.size() / 3);
    flatShaded_.groups = groups;
}

void CadGpuResources::updateFlatShadedGroups(
        uint64_t planRevision,
        const std::vector<CadFlatShadedGroup>& groups)
{
    flatShaded_.planRevision = planRevision;
    flatShaded_.groups = groups;
}

void CadGpuResources::uploadSubpixelProxyPoints(
        uint64_t revision,
        const std::vector<float>& positions,
        const std::vector<uint8_t>& colors,
        const SoGLContext *glue,
        const CadGLCaps& caps)
{
    if (!glue || positions.empty() || positions.size() % 3 != 0 ||
            colors.size() != (positions.size() / 3) * 4)
        return;

    if (!subpixelProxyPoints_.posBuf)
        glue->glGenBuffers(1, &subpixelProxyPoints_.posBuf);
    if (!subpixelProxyPoints_.colorBuf)
        glue->glGenBuffers(1, &subpixelProxyPoints_.colorBuf);

    glue->glBindBuffer(GL_ARRAY_BUFFER, subpixelProxyPoints_.posBuf);
    glue->glBufferData(GL_ARRAY_BUFFER,
                       static_cast<GLsizeiptr>(positions.size() * sizeof(float)),
                       positions.data(), GL_STREAM_DRAW);
    glue->glBindBuffer(GL_ARRAY_BUFFER, subpixelProxyPoints_.colorBuf);
    glue->glBufferData(GL_ARRAY_BUFFER,
                       static_cast<GLsizeiptr>(colors.size() * sizeof(uint8_t)),
                       colors.data(), GL_STREAM_DRAW);

    /* Proxy membership changes during view motion, which makes this path a
     * frequent compositor boundary.  Keep both streams in an Obol-owned VAO
     * instead of mutating the caller/default VAO every frame. */
    if (!subpixelProxyPoints_.vao && caps.hasVAO &&
            glue->glGenVertexArrays) {
        glue->glGenVertexArrays(1, &subpixelProxyPoints_.vao);
        glue->glBindVertexArray(subpixelProxyPoints_.vao);
        glue->glBindBuffer(GL_ARRAY_BUFFER, subpixelProxyPoints_.posBuf);
        glue->glVertexAttribPointerARB(0, 3, GL_FLOAT, GL_FALSE,
                                       3 * sizeof(float), nullptr);
        glue->glEnableVertexAttribArrayARB(0);
        glue->glBindBuffer(GL_ARRAY_BUFFER, subpixelProxyPoints_.colorBuf);
        glue->glVertexAttribPointerARB(1, 4, GL_UNSIGNED_BYTE, GL_TRUE,
                                       4 * sizeof(uint8_t), nullptr);
        glue->glEnableVertexAttribArrayARB(1);
        glue->glBindVertexArray(0);
    }
    glue->glBindBuffer(GL_ARRAY_BUFFER, 0);

    subpixelProxyPoints_.revision = revision;
    subpixelProxyPoints_.count = static_cast<GLsizei>(positions.size() / 3);
}

// ---------------------------------------------------------------------------
// releaseAll()
// ---------------------------------------------------------------------------

void CadGpuResources::releaseAll(const SoGLContext * glue)
{
    if (glue) {
        for (auto& kv : cache_) {
            deletePointGpu(kv.second.point, glue);
            deleteWireGpu(kv.second.wire, glue);
            deleteTriGpu(kv.second.tri,   glue);
        }
        if (instanceVbo_ && glue->glDeleteBuffers) {
            glue->glDeleteBuffers(1, &instanceVbo_);
        }
        if (flatWire_.vao && glue->glDeleteVertexArrays)
            glue->glDeleteVertexArrays(1, &flatWire_.vao);
        if (flatWire_.posBuf && glue->glDeleteBuffers)
            glue->glDeleteBuffers(1, &flatWire_.posBuf);
        if (flatShaded_.posBuf && glue->glDeleteBuffers)
            glue->glDeleteBuffers(1, &flatShaded_.posBuf);
        if (flatShaded_.normBuf && glue->glDeleteBuffers)
            glue->glDeleteBuffers(1, &flatShaded_.normBuf);
        if (flatShaded_.vao && glue->glDeleteVertexArrays)
            glue->glDeleteVertexArrays(1, &flatShaded_.vao);
        if (subpixelProxyPoints_.posBuf && glue->glDeleteBuffers)
            glue->glDeleteBuffers(1, &subpixelProxyPoints_.posBuf);
        if (subpixelProxyPoints_.colorBuf && glue->glDeleteBuffers)
            glue->glDeleteBuffers(1, &subpixelProxyPoints_.colorBuf);
        if (subpixelProxyPoints_.vao && glue->glDeleteVertexArrays)
            glue->glDeleteVertexArrays(1, &subpixelProxyPoints_.vao);
    }
    cache_.clear();
    instanceVbo_ = 0;
    flatWire_ = CadFlatWireGpu();
    flatShaded_ = CadFlatShadedGpu();
    subpixelProxyPoints_ = CadSubpixelProxyGpu();
}

} // namespace internal
} // namespace Obol
