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
#include <cstdlib>
#include <limits>
#include <cstring>

#ifndef GL_DRAW_INDIRECT_BUFFER
#define GL_DRAW_INDIRECT_BUFFER 0x8F3F
#endif

#ifndef GL_TIME_ELAPSED
#define GL_TIME_ELAPSED 0x88BF
#endif

namespace {

using Obol::internal::CadAtlasRange;

static size_t
cadTriangleAtlasBudget()
{
    size_t budget = 512u * 1024u * 1024u;
    if (const char *value = std::getenv("OBOL_CAD_ATLAS_MB")) {
        char *end = nullptr;
        const unsigned long long megabytes = std::strtoull(value, &end, 10);
        if (end != value && *end == '\0' && megabytes > 0 &&
                megabytes <= std::numeric_limits<size_t>::max() /
                    (1024u * 1024u))
            budget = static_cast<size_t>(megabytes) * 1024u * 1024u;
    }
    return budget;
}

static uint32_t
cadAtlasRoundUp(uint32_t value, uint32_t granularity)
{
    if (!value || granularity <= 1) return value;
    const uint64_t rounded =
        ((static_cast<uint64_t>(value) + granularity - 1u) /
         granularity) * granularity;
    return rounded <= std::numeric_limits<uint32_t>::max() ?
        static_cast<uint32_t>(rounded) : value;
}

static uint32_t
cadAtlasReservedCount(uint32_t required, bool progressive,
                      uint32_t granularity)
{
    if (!required) return 0;
    uint64_t reserve = required;
    if (progressive)
        reserve += std::max<uint64_t>(granularity,
                                      static_cast<uint64_t>(required) / 8u);
    reserve = std::min<uint64_t>(
        reserve, std::numeric_limits<uint32_t>::max());
    return cadAtlasRoundUp(static_cast<uint32_t>(reserve), granularity);
}

static uint32_t
cadAtlasFreeRange(std::vector<CadAtlasRange>& freeRanges,
                  CadAtlasRange released)
{
    if (released.empty()) return 0;
    /*
     * Allocation and adjacent growth preserve address ordering, and every
     * release comes through this function.  Preserve that invariant with one
     * ordered insertion and local coalescing.  Sorting the complete list for
     * every PoP relocation made progressive realization O(releases * n log n)
     * within each atlas page; at 50k unique parts it consumed roughly a fifth
     * of the render/owner thread even though the list was already almost
     * sorted.
     */
    auto next = std::lower_bound(
        freeRanges.begin(), freeRanges.end(), released.first,
        [](const CadAtlasRange& range, uint32_t first) {
            return range.first < first;
        });
    size_t current = static_cast<size_t>(
        std::distance(freeRanges.begin(), next));
    if (current > 0) {
        CadAtlasRange& previous = freeRanges[current - 1];
        const uint64_t previousEnd =
            static_cast<uint64_t>(previous.first) + previous.capacity;
        if (previousEnd >= released.first) {
            const uint64_t releasedEnd =
                static_cast<uint64_t>(released.first) +
                released.capacity;
            const uint64_t mergedEnd =
                std::max(previousEnd, releasedEnd);
            previous.capacity = static_cast<uint32_t>(
                std::min<uint64_t>(
                    mergedEnd - previous.first,
                    std::numeric_limits<uint32_t>::max()));
            --current;
        } else {
            freeRanges.insert(next, released);
        }
    } else {
        freeRanges.insert(next, released);
    }

    while (current + 1 < freeRanges.size()) {
        CadAtlasRange& range = freeRanges[current];
        const CadAtlasRange& following = freeRanges[current + 1];
        const uint64_t rangeEnd =
            static_cast<uint64_t>(range.first) + range.capacity;
        if (rangeEnd < following.first)
            break;
        const uint64_t followingEnd =
            static_cast<uint64_t>(following.first) +
            following.capacity;
        const uint64_t mergedEnd =
            std::max(rangeEnd, followingEnd);
        range.capacity = static_cast<uint32_t>(
            std::min<uint64_t>(
                mergedEnd - range.first,
                std::numeric_limits<uint32_t>::max()));
        freeRanges.erase(
            freeRanges.begin() +
            static_cast<std::ptrdiff_t>(current + 1));
    }
    return freeRanges[current].capacity;
}

static uint32_t
cadAtlasLargestFreeCapacity(
        const std::vector<CadAtlasRange>& freeRanges)
{
    uint32_t largest = 0;
    for (const CadAtlasRange& range : freeRanges)
        largest = std::max(largest, range.capacity);
    return largest;
}

/*
 * Test whether an allocation would fit after returning one live range
 * without copying and coalescing the complete free-range vector.  The
 * ranges are kept ordered and non-overlapping by cadAtlasFreeRange(), so a
 * single scan can account for both an already-suitable range and the one
 * contiguous interval which releasing `released` would form.
 *
 * This is intentionally a predicate rather than an allocation operation.
 * Progressive relocation uses it to prove that releasing the old prefix is
 * safe before doing so.  Copying both free lists for that proof made every
 * growing part proportional to the page's fragmentation; a large stream of
 * unique PoP meshes consequently spent most of its owner-thread time copying
 * vectors which were immediately discarded.
 */
static bool
cadAtlasCanAllocateAfterRelease(
        const std::vector<CadAtlasRange>& freeRanges,
        uint32_t largestFreeCapacity, CadAtlasRange released,
        uint32_t capacity)
{
    if (!capacity)
        return true;
    if (released.empty())
        return largestFreeCapacity >= capacity;
    if (largestFreeCapacity >= capacity)
        return true;

    uint64_t mergedFirst = released.first;
    uint64_t mergedEnd =
        static_cast<uint64_t>(released.first) + released.capacity;
    auto next = std::lower_bound(
        freeRanges.begin(), freeRanges.end(), released.first,
        [](const CadAtlasRange& range, uint32_t first) {
            return range.first < first;
        });
    if (next != freeRanges.begin()) {
        const CadAtlasRange& previous = *(next - 1);
        const uint64_t previousEnd =
            static_cast<uint64_t>(previous.first) + previous.capacity;
        if (previousEnd >= mergedFirst) {
            mergedFirst = previous.first;
            mergedEnd = std::max(mergedEnd, previousEnd);
        }
    }
    if (next != freeRanges.end() &&
            mergedEnd >= static_cast<uint64_t>(next->first)) {
        mergedEnd = std::max(
            mergedEnd,
            static_cast<uint64_t>(next->first) + next->capacity);
    }
    return mergedEnd - mergedFirst >= capacity;
}

static bool
cadAtlasAllocateRange(std::vector<CadAtlasRange>& freeRanges,
                      uint32_t capacity, CadAtlasRange& result,
                      uint32_t& largestFreeCapacity)
{
    result = CadAtlasRange();
    if (!capacity) return true;
    size_t best = freeRanges.size();
    uint32_t bestWaste = std::numeric_limits<uint32_t>::max();
    for (size_t i = 0; i < freeRanges.size(); ++i) {
        if (freeRanges[i].capacity < capacity) continue;
        const uint32_t waste = freeRanges[i].capacity - capacity;
        if (best == freeRanges.size() || waste < bestWaste) {
            best = i;
            bestWaste = waste;
        }
    }
    if (best == freeRanges.size()) return false;
    const uint32_t previousCapacity = freeRanges[best].capacity;
    result.first = freeRanges[best].first;
    result.capacity = capacity;
    freeRanges[best].first += capacity;
    freeRanges[best].capacity -= capacity;
    if (!freeRanges[best].capacity)
        freeRanges.erase(freeRanges.begin() +
                         static_cast<std::ptrdiff_t>(best));
    if (previousCapacity == largestFreeCapacity)
        largestFreeCapacity =
            cadAtlasLargestFreeCapacity(freeRanges);
    return true;
}

static bool
cadAtlasGrowAdjacent(std::vector<CadAtlasRange>& freeRanges,
                     CadAtlasRange& allocated, uint32_t newCapacity,
                     uint32_t& largestFreeCapacity)
{
    if (newCapacity <= allocated.capacity) return true;
    const uint32_t extra = newCapacity - allocated.capacity;
    const uint64_t oldEnd =
        static_cast<uint64_t>(allocated.first) + allocated.capacity;
    if (oldEnd > std::numeric_limits<uint32_t>::max()) return false;
    for (size_t i = 0; i < freeRanges.size(); ++i) {
        if (freeRanges[i].first != static_cast<uint32_t>(oldEnd) ||
                freeRanges[i].capacity < extra)
            continue;
        const uint32_t previousFreeCapacity =
            freeRanges[i].capacity;
        freeRanges[i].first += extra;
        freeRanges[i].capacity -= extra;
        allocated.capacity = newCapacity;
        if (!freeRanges[i].capacity)
            freeRanges.erase(freeRanges.begin() +
                             static_cast<std::ptrdiff_t>(i));
        if (previousFreeCapacity == largestFreeCapacity)
            largestFreeCapacity =
                cadAtlasLargestFreeCapacity(freeRanges);
        return true;
    }
    return false;
}

} // namespace

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

void CadGpuResources::deleteProgressiveGpu(
        CadProgressiveGpu& p, const SoGLContext *glue)
{
    if (p.posBuf && glue->glDeleteBuffers)
        glue->glDeleteBuffers(1, &p.posBuf);
    if (p.normBuf && glue->glDeleteBuffers)
        glue->glDeleteBuffers(1, &p.normBuf);
    progressiveBytes_ = p.bytes <= progressiveBytes_ ?
        progressiveBytes_ - p.bytes : 0;
    p = CadProgressiveGpu();
}

void CadGpuResources::deleteProgressiveGpu(
        Entry& entry, const SoGLContext *glue)
{
    for (CadProgressiveGpu& p : entry.progressiveWire)
        deleteProgressiveGpu(p, glue);
    for (CadProgressiveGpu& p : entry.progressiveTri)
        deleteProgressiveGpu(p, glue);
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

    // The same progressive source generation may be realized at a larger
    // view-dependent prefix later.
    if (isUpToDate(
            pid, generation, wireCount, segIdxCount,
            triPosCount, triIdxCount))
        return;

    /* Ordinary geometry replacement is allowed to change every byte and
     * representation.  Producer-declared progressive geometry is a stricter
     * cumulative-prefix contract, so preserve its buffer objects below and
     * append only newly resident tails. */
    const bool progressiveReset = progressive &&
        ((entry.wire.vertCount > 0 && wireCount < entry.wire.vertCount) ||
         (entry.tri.vertCount > 0 && triPosCount < entry.tri.vertCount) ||
         (entry.tri.idxCount > 0 && triIdxCount < entry.tri.idxCount));
    if (!progressive || progressiveReset) {
        deletePointGpu(entry.point, glue);
        deleteWireGpu(entry.wire, glue);
        deleteTriGpu(entry.tri, glue);
        deleteProgressiveGpu(entry, glue);
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

bool CadGpuResources::isUpToDate(
        PartId pid, uint64_t gen, GLsizei requiredWirePoints,
        GLsizei requiredWireIndices, GLsizei requiredTriPoints,
        GLsizei requiredTriIndices) const
{
    auto it = cache_.find(pid);
    if (it == cache_.end()) return false;
    const Entry& entry = it->second;
    if (entry.generation != gen)
        return false;
    if (requiredWirePoints > entry.wire.vertCount ||
            requiredWireIndices > entry.wire.idxCount ||
            requiredTriPoints > entry.tri.vertCount ||
            requiredTriIndices > entry.tri.idxCount)
        return false;
    return true;
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

const CadProgressiveGpu* CadGpuResources::progressiveFor(
        PartId pid, bool shaded, uint8_t level)
{
    auto it = cache_.find(pid);
    if (it == cache_.end() || level >= 16) return nullptr;
    CadProgressiveGpu& p = shaded ?
        it->second.progressiveTri[level] :
        it->second.progressiveWire[level];
    if (p.posBuf && p.vertexCount > 0)
        p.lastUsedFrame = progressiveFrame_;
    return p.posBuf && p.vertexCount > 0 ? &p : nullptr;
}

void CadGpuResources::uploadProgressive(
        PartId pid, bool shaded, uint8_t level,
        const std::vector<float>& positions,
        const std::vector<float>& normals,
        bool indexed, const SoGLContext *glue)
{
    if (!glue || !glue->glGenBuffers || level >= 16 ||
            positions.empty() || positions.size() % 3 != 0 ||
            (!normals.empty() && normals.size() != positions.size()))
        return;
    auto found = cache_.find(pid);
    if (found == cache_.end()) return;
    CadProgressiveGpu& p = shaded ?
        found->second.progressiveTri[level] :
        found->second.progressiveWire[level];
    deleteProgressiveGpu(p, glue);

    glue->glGenBuffers(1, &p.posBuf);
    glue->glBindBuffer(GL_ARRAY_BUFFER, p.posBuf);
    glue->glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(positions.size() * sizeof(float)),
        positions.data(), GL_STATIC_DRAW);
    if (!normals.empty()) {
        glue->glGenBuffers(1, &p.normBuf);
        glue->glBindBuffer(GL_ARRAY_BUFFER, p.normBuf);
        glue->glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(normals.size() * sizeof(float)),
            normals.data(), GL_STATIC_DRAW);
    }
    glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
    p.vertexCount = static_cast<GLsizei>(positions.size() / 3);
    p.indexed = indexed;
    p.bytes = (positions.size() + normals.size()) * sizeof(float);
    p.lastUsedFrame = progressiveFrame_;
    progressiveBytes_ += p.bytes;
}

void CadGpuResources::beginProgressiveFrame()
{
    ++progressiveFrame_;
    if (progressiveFrame_ == 0)
        progressiveFrame_ = 1;
}

void CadGpuResources::endProgressiveFrame(const SoGLContext *glue)
{
    if (!glue) return;

    size_t reserveBudget = 64u * 1024u * 1024u;
    if (const char *value = std::getenv("OBOL_CAD_PROGRESSIVE_CACHE_MB")) {
        char *end = nullptr;
        const unsigned long long megabytes = std::strtoull(value, &end, 10);
        if (end != value && *end == '\0' && megabytes > 0 &&
                megabytes <= std::numeric_limits<size_t>::max() /
                    (1024u * 1024u))
            reserveBudget =
                static_cast<size_t>(megabytes) * 1024u * 1024u;
    }

    /* The current stable cut can be hundreds of megabytes by itself.  Treat
     * the configured amount as a bounded auxiliary LRU reserve above that
     * unavoidable active working set, rather than allowing the active cut to
     * evict every cheap interaction cut.  This keeps a recently used coarse
     * PoP VBO ready for immediate motion without unbounded GPU growth. */
    size_t activeBytes = 0;
    for (const auto& item : cache_) {
        const auto countActive = [&](const CadProgressiveGpu& p) {
            if (p.posBuf && p.lastUsedFrame == progressiveFrame_ &&
                    p.bytes <= std::numeric_limits<size_t>::max() -
                        activeBytes)
                activeBytes += p.bytes;
        };
        for (const CadProgressiveGpu& p : item.second.progressiveWire)
            countActive(p);
        for (const CadProgressiveGpu& p : item.second.progressiveTri)
            countActive(p);
    }
    const size_t budget =
        activeBytes <= std::numeric_limits<size_t>::max() - reserveBudget ?
        activeBytes + reserveBudget : std::numeric_limits<size_t>::max();
    while (progressiveBytes_ > budget) {
        CadProgressiveGpu *victim = nullptr;
        bool victimIsAnchor = true;
        uint8_t victimLevel = 0;
        for (auto& item : cache_) {
            auto consider = [&](auto& cuts, uint8_t level) {
                CadProgressiveGpu& p = cuts[level];
                if (!p.posBuf || p.lastUsedFrame == progressiveFrame_)
                    return;
                bool isAnchor = true;
                for (uint8_t lower = 0; lower < level; ++lower) {
                    if (cuts[lower].posBuf) {
                        isAnchor = false;
                        break;
                    }
                }
                if (!victim ||
                        (victimIsAnchor && !isAnchor) ||
                        (victimIsAnchor == isAnchor &&
                         (level > victimLevel ||
                          (level == victimLevel &&
                           (p.lastUsedFrame < victim->lastUsedFrame ||
                            (p.lastUsedFrame == victim->lastUsedFrame &&
                             p.bytes > victim->bytes)))))) {
                    victim = &p;
                    victimIsAnchor = isAnchor;
                    victimLevel = level;
                }
            };
            for (uint8_t level = 0; level < 16; ++level)
                consider(item.second.progressiveWire, level);
            for (uint8_t level = 0; level < 16; ++level)
                consider(item.second.progressiveTri, level);
        }
        if (!victim) break;
        deleteProgressiveGpu(*victim, glue);
    }
}

// ---------------------------------------------------------------------------
// Retained paged triangle atlas
// ---------------------------------------------------------------------------

void CadGpuResources::bumpTriangleAtlasRevision() noexcept
{
    ++triangleAtlasRevision_;
    if (!triangleAtlasRevision_)
        triangleAtlasRevision_ = 1;
}

void CadGpuResources::beginTriangleAtlasFrame()
{
    triangleAtlasMaintenanceDeferred_ = false;
    triangleAtlasReclamationDeferred_ = false;
    ++triangleAtlasFrame_;
    if (!triangleAtlasFrame_) {
        triangleAtlasFrame_ = 1;
        triangleAtlasInactiveSweepFrame_ = 0;
    }
}

const CadTriangleAtlasPart *
CadGpuResources::triangleAtlasPart(PartId pid) const
{
    const auto found = triangleAtlasParts_.find(pid);
    return found == triangleAtlasParts_.end() ? nullptr : &found->second;
}

const CadTriangleAtlasPage *
CadGpuResources::triangleAtlasPage(uint32_t page) const
{
    return page < triangleAtlasPages_.size() &&
            triangleAtlasPages_[page] ?
        triangleAtlasPages_[page].get() : nullptr;
}

CadTriangleAtlasPage *
CadGpuResources::triangleAtlasPage(uint32_t page)
{
    return page < triangleAtlasPages_.size() &&
            triangleAtlasPages_[page] ?
        triangleAtlasPages_[page].get() : nullptr;
}

const CadTriangleAtlasPart *
CadGpuResources::touchTriangleAtlasPart(
        PartId pid, uint64_t generation, bool hasNormals,
        uint32_t vertexCount, uint32_t indexCount)
{
    const auto found = triangleAtlasParts_.find(pid);
    if (found == triangleAtlasParts_.end())
        return nullptr;
    CadTriangleAtlasPart& part = found->second;
    part.lastUsedFrame = triangleAtlasFrame_;
    part.requestedVertexCount = vertexCount;
    part.requestedIndexCount = indexCount;
    if (vertexCount < part.vertexCount ||
            indexCount < part.indexCount) {
        if (!part.lowerDemandSinceFrame)
            part.lowerDemandSinceFrame = triangleAtlasFrame_;
    } else {
        part.lowerDemandSinceFrame = 0;
    }
    if (part.generation != generation ||
            part.hasNormals != hasNormals ||
            vertexCount > part.vertexCount ||
            indexCount > part.indexCount)
        return nullptr;
    return &part;
}

size_t CadGpuResources::triangleAtlasPageCount() const noexcept
{
    size_t count = 0;
    for (const auto& page : triangleAtlasPages_)
        if (page) ++count;
    return count;
}

size_t CadGpuResources::triangleAtlasLiveBytes() const noexcept
{
    size_t bytes = 0;
    for (const auto& item : triangleAtlasParts_) {
        const CadTriangleAtlasPart& part = item.second;
        const CadTriangleAtlasPage *page =
            triangleAtlasPage(part.page);
        const size_t vertexBytes =
            static_cast<size_t>(part.vertexCount) *
            (page && page->storesNormals ? 2u : 1u) *
            3u * sizeof(float);
        const size_t indexBytes =
            static_cast<size_t>(part.indexCount) * sizeof(uint32_t);
        if (vertexBytes <= std::numeric_limits<size_t>::max() - bytes)
            bytes += vertexBytes;
        else
            return std::numeric_limits<size_t>::max();
        if (indexBytes <= std::numeric_limits<size_t>::max() - bytes)
            bytes += indexBytes;
        else
            return std::numeric_limits<size_t>::max();
    }
    return bytes;
}

void CadGpuResources::deleteTriangleAtlasPage(
        uint32_t pageIndex, const SoGLContext *glue)
{
    if (pageIndex >= triangleAtlasPages_.size() ||
            !triangleAtlasPages_[pageIndex])
        return;
    CadTriangleAtlasPage& page = *triangleAtlasPages_[pageIndex];
    if (page.partCount)
        return;
    const size_t bytes = page.allocatedBytes();
    if (glue) {
        if (page.vao && glue->glDeleteVertexArrays)
            glue->glDeleteVertexArrays(1, &page.vao);
        const GLuint buffers[] = {
            page.posBuf, page.normBuf, page.idxBuf, page.indirectBuf
        };
        for (GLuint buffer : buffers)
            if (buffer && glue->glDeleteBuffers)
                glue->glDeleteBuffers(1, &buffer);
    }
    triangleAtlasAllocatedBytes_ =
        bytes <= triangleAtlasAllocatedBytes_ ?
        triangleAtlasAllocatedBytes_ - bytes : 0;
    triangleAtlasPages_[pageIndex].reset();
    bumpTriangleAtlasRevision();
}

void CadGpuResources::releaseTriangleAtlasPart(
        PartId pid, const SoGLContext *glue)
{
    const auto found = triangleAtlasParts_.find(pid);
    if (found == triangleAtlasParts_.end())
        return;
    const uint32_t pageIndex = found->second.page;
    CadTriangleAtlasPage *page = triangleAtlasPage(pageIndex);
    if (page) {
        page->largestFreeVertexCapacity = std::max(
            page->largestFreeVertexCapacity,
            cadAtlasFreeRange(
                page->freeVertices, found->second.vertices));
        page->largestFreeIndexCapacity = std::max(
            page->largestFreeIndexCapacity,
            cadAtlasFreeRange(
                page->freeIndices, found->second.indices));
        if (page->partCount)
            --page->partCount;
    }
    triangleAtlasParts_.erase(found);
    bumpTriangleAtlasRevision();
    if (page && !page->partCount)
        deleteTriangleAtlasPage(pageIndex, glue);
}

static void
cadAtlasUploadZeroNormals(const SoGLContext *glue, GLintptr byteOffset,
                          uint32_t vertexCount)
{
    if (!glue || !vertexCount) return;
    static const std::array<float, 3u * 1024u> zeros = {};
    uint32_t written = 0;
    while (written < vertexCount) {
        const uint32_t amount =
            std::min<uint32_t>(vertexCount - written, 1024u);
        glue->glBufferSubData(
            GL_ARRAY_BUFFER,
            byteOffset + static_cast<GLintptr>(written) *
                3 * sizeof(float),
            static_cast<GLsizeiptr>(amount) * 3 * sizeof(float),
            zeros.data());
        written += amount;
    }
}

const CadTriangleAtlasPart *CadGpuResources::upsertTriangleAtlasPart(
        PartId pid, uint64_t generation,
        const float *positions, const float *normals, uint32_t vertexCount,
        const uint32_t *indices, uint32_t indexCount, bool progressive,
        const SoGLContext *glue, const CadGLCaps& caps)
{
    if (!glue || !caps.canUseIndirect() || !positions || !indices ||
            !vertexCount || !indexCount)
        return nullptr;

    const bool hasNormals = normals != nullptr;
    const uint32_t vertexReserve =
        cadAtlasReservedCount(vertexCount, progressive, 64u);
    const uint32_t indexReserve =
        cadAtlasReservedCount(indexCount, progressive, 192u);
    if (vertexReserve < vertexCount || indexReserve < indexCount)
        return nullptr;
    auto found = triangleAtlasParts_.find(pid);
    if (found != triangleAtlasParts_.end()) {
        CadTriangleAtlasPart& part = found->second;
        part.lastUsedFrame = triangleAtlasFrame_;
        part.requestedVertexCount = vertexCount;
        part.requestedIndexCount = indexCount;
        if (part.hasNormals == hasNormals) {
            CadTriangleAtlasPage *page = triangleAtlasPage(part.page);
            if (page) {
                const bool generationChanged =
                    part.generation != generation;
                const uint32_t wantedVertexCapacity =
                    cadAtlasReservedCount(vertexCount, progressive, 64u);
                const uint32_t wantedIndexCapacity =
                    cadAtlasReservedCount(indexCount, progressive, 192u);
                const bool verticesFit =
                    wantedVertexCapacity <= part.vertices.capacity ||
                    cadAtlasGrowAdjacent(
                        page->freeVertices, part.vertices,
                        wantedVertexCapacity,
                        page->largestFreeVertexCapacity);
                const bool indicesFit =
                    wantedIndexCapacity <= part.indices.capacity ||
                    cadAtlasGrowAdjacent(
                        page->freeIndices, part.indices,
                        wantedIndexCapacity,
                        page->largestFreeIndexCapacity);
                if (verticesFit && indicesFit) {
                    const uint32_t previousVertexCount = part.vertexCount;
                    const uint32_t previousIndexCount = part.indexCount;
                    const uint32_t previousVertexCapacity =
                        part.vertices.capacity;
                    const uint32_t previousIndexCapacity =
                        part.indices.capacity;
                    if (generationChanged) {
                        /*
                         * A new producer generation behind the same PartId
                         * may alter any authored value, but it does not need
                         * a second atlas reservation when the replacement
                         * fits.  Overwrite the fixed ranges in place.  This
                         * is also the common PoP resident-prefix path and
                         * avoids release/reallocate churn at the memory
                         * ceiling.
                         */
                        const GLintptr offset =
                            static_cast<GLintptr>(
                                part.vertices.first) * 3 * sizeof(float);
                        glue->glBindBuffer(
                            GL_ARRAY_BUFFER, page->posBuf);
                        glue->glBufferSubData(
                            GL_ARRAY_BUFFER, offset,
                            static_cast<GLsizeiptr>(vertexCount) *
                                3 * sizeof(float),
                            positions);
                        if (page->normBuf) {
                            glue->glBindBuffer(
                                GL_ARRAY_BUFFER, page->normBuf);
                            if (hasNormals) {
                                glue->glBufferSubData(
                                    GL_ARRAY_BUFFER, offset,
                                    static_cast<GLsizeiptr>(vertexCount) *
                                        3 * sizeof(float),
                                    normals);
                            } else {
                                cadAtlasUploadZeroNormals(
                                    glue, offset, vertexCount);
                            }
                        }
                        glue->glBindBuffer(
                            GL_ELEMENT_ARRAY_BUFFER, page->idxBuf);
                        glue->glBufferSubData(
                            GL_ELEMENT_ARRAY_BUFFER,
                            static_cast<GLintptr>(
                                part.indices.first) * sizeof(uint32_t),
                            static_cast<GLsizeiptr>(indexCount) *
                                sizeof(uint32_t),
                            indices);
                        part.vertexCount = vertexCount;
                        part.indexCount = indexCount;
                        part.generation = generation;
                        part.progressive = progressive;
                    } else if (vertexCount > part.vertexCount) {
                        const uint32_t appended =
                            vertexCount - part.vertexCount;
                        const GLintptr offset =
                            static_cast<GLintptr>(
                                part.vertices.first + part.vertexCount) *
                            3 * sizeof(float);
                        glue->glBindBuffer(
                            GL_ARRAY_BUFFER, page->posBuf);
                        glue->glBufferSubData(
                            GL_ARRAY_BUFFER, offset,
                            static_cast<GLsizeiptr>(appended) *
                                3 * sizeof(float),
                            positions +
                                static_cast<size_t>(part.vertexCount) * 3);
                        if (page->normBuf) {
                            glue->glBindBuffer(
                                GL_ARRAY_BUFFER, page->normBuf);
                            if (hasNormals) {
                                glue->glBufferSubData(
                                    GL_ARRAY_BUFFER, offset,
                                    static_cast<GLsizeiptr>(appended) *
                                        3 * sizeof(float),
                                    normals +
                                        static_cast<size_t>(
                                            part.vertexCount) * 3);
                            } else {
                                cadAtlasUploadZeroNormals(
                                    glue, offset, appended);
                            }
                        }
                        part.vertexCount = vertexCount;
                    }
                    if (!generationChanged &&
                            indexCount > part.indexCount) {
                        const uint32_t appended =
                            indexCount - part.indexCount;
                        glue->glBindBuffer(
                            GL_ELEMENT_ARRAY_BUFFER, page->idxBuf);
                        glue->glBufferSubData(
                            GL_ELEMENT_ARRAY_BUFFER,
                            static_cast<GLintptr>(
                                part.indices.first + part.indexCount) *
                                sizeof(uint32_t),
                            static_cast<GLsizeiptr>(appended) *
                                sizeof(uint32_t),
                            indices + part.indexCount);
                        part.indexCount = indexCount;
                    }
                    glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
                    glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
                    if (vertexCount < previousVertexCount ||
                            indexCount < previousIndexCount) {
                        if (!part.lowerDemandSinceFrame)
                            part.lowerDemandSinceFrame =
                                triangleAtlasFrame_;
                    } else {
                        part.lowerDemandSinceFrame = 0;
                    }
                    if (generationChanged ||
                            part.vertexCount != previousVertexCount ||
                            part.indexCount != previousIndexCount ||
                            part.vertices.capacity !=
                                previousVertexCapacity ||
                            part.indices.capacity !=
                                previousIndexCapacity)
                        bumpTriangleAtlasRevision();
                    return &part;
                }

                /*
                 * One side may have extended successfully while the other
                 * could not.  Keep the harmless reservation; relocation below
                 * returns both complete ranges to the page.
                 */
            }
        }

        /*
         * Geometry replacement or a prefix that outgrew its contiguous
         * ranges needs a relocation.  The former policy returned the old
         * progressive binding here unconditionally.  A normal zoom could
         * exceed its modest 12.5% reserve on the first level change, leaving
         * that object permanently stuck at the initial coarse cut even after
         * the view became quiet and ample atlas memory was available.
         *
         * Relocate without a temporary copy of the mesh.  First prove that
         * releasing this part will expose a suitable existing range or that
         * the resulting allocation budget can admit one replacement page.
         * Source arrays remain authoritative, so an unexpected GL allocation
         * failure can reconstruct the old prefix before returning.
         */
        if (found != triangleAtlasParts_.end() &&
                found->second.hasNormals == hasNormals &&
                (found->second.generation == generation ||
                 (found->second.progressive && progressive))) {
            const CadTriangleAtlasPart previous = found->second;
            bool existingRangeFits = false;
            for (uint32_t pageIndex = 0;
                    pageIndex < triangleAtlasPages_.size(); ++pageIndex) {
                const CadTriangleAtlasPage *candidate =
                    triangleAtlasPage(pageIndex);
                if (!candidate || candidate->dedicated ||
                        (hasNormals && !candidate->storesNormals))
                    continue;
                /*
                 * Releasing the sole part deletes its page, so it cannot be
                 * counted as reusable capacity.  Otherwise simulate the
                 * merged free ranges without mutating live GPU ownership.
                 */
                if (pageIndex == previous.page &&
                        candidate->partCount == 1)
                    continue;
                const bool releasesPrevious =
                    pageIndex == previous.page;
                const bool vertexFits = releasesPrevious ?
                    cadAtlasCanAllocateAfterRelease(
                        candidate->freeVertices,
                        candidate->largestFreeVertexCapacity,
                        previous.vertices,
                        vertexReserve) :
                    candidate->largestFreeVertexCapacity >=
                        vertexReserve;
                const bool indexFits = releasesPrevious ?
                    cadAtlasCanAllocateAfterRelease(
                        candidate->freeIndices,
                        candidate->largestFreeIndexCapacity,
                        previous.indices,
                        indexReserve) :
                    candidate->largestFreeIndexCapacity >=
                        indexReserve;
                if (vertexFits && indexFits) {
                    existingRangeFits = true;
                    break;
                }
            }

            constexpr size_t targetPageBytes =
                16u * 1024u * 1024u;
            constexpr uint32_t defaultCommands = 4096u;
            const size_t vertexStrideBytes =
                hasNormals ? 24u : 12u;
            const size_t requestBytes =
                static_cast<size_t>(vertexReserve) *
                    vertexStrideBytes +
                static_cast<size_t>(indexReserve) *
                    sizeof(uint32_t);
            const bool dedicated =
                requestBytes > targetPageBytes / 2u;
            uint32_t pageVertices = vertexReserve;
            uint32_t pageIndices = indexReserve;
            if (!dedicated) {
                const size_t commandBytes =
                    static_cast<size_t>(defaultCommands) *
                        sizeof(CadDrawElementsIndirectCommand);
                const size_t payloadBytes =
                    targetPageBytes > commandBytes ?
                        targetPageBytes - commandBytes :
                        targetPageBytes;
                const long double indexRatio =
                    static_cast<long double>(indexReserve) /
                    static_cast<long double>(
                        std::max<uint32_t>(1u, vertexReserve));
                const long double bytesPerVertex =
                    static_cast<long double>(vertexStrideBytes) +
                    indexRatio * sizeof(uint32_t);
                const uint64_t shapedVertices =
                    static_cast<uint64_t>(
                        static_cast<long double>(payloadBytes) /
                        bytesPerVertex);
                pageVertices = cadAtlasRoundUp(
                    static_cast<uint32_t>(std::min<uint64_t>(
                        std::max<uint64_t>(
                            shapedVertices, vertexReserve),
                        std::numeric_limits<uint32_t>::max())),
                    64u);
                const uint64_t shapedIndices =
                    static_cast<uint64_t>(
                        static_cast<long double>(pageVertices) *
                        indexRatio);
                pageIndices = cadAtlasRoundUp(
                    static_cast<uint32_t>(std::min<uint64_t>(
                        std::max<uint64_t>(
                            shapedIndices, indexReserve),
                        std::numeric_limits<uint32_t>::max())),
                    192u);
            }
            CadTriangleAtlasPage plannedPage;
            plannedPage.vertexCapacity = pageVertices;
            plannedPage.indexCapacity = pageIndices;
            plannedPage.indirectCapacity = defaultCommands;
            plannedPage.storesNormals = hasNormals;
            const size_t replacementPageBytes =
                plannedPage.allocatedBytes();
            size_t allocatedAfterRelease =
                triangleAtlasAllocatedBytes_;
            const CadTriangleAtlasPage *previousPage =
                triangleAtlasPage(previous.page);
            if (previousPage && previousPage->partCount == 1) {
                const size_t releasedBytes =
                    previousPage->allocatedBytes();
                allocatedAfterRelease =
                    releasedBytes <= allocatedAfterRelease ?
                        allocatedAfterRelease - releasedBytes : 0;
            }
            const size_t budget = cadTriangleAtlasBudget();
            const bool replacementPageFits =
                replacementPageBytes <= budget &&
                allocatedAfterRelease <=
                    budget - replacementPageBytes;

            if (existingRangeFits || replacementPageFits) {
                releaseTriangleAtlasPart(pid, glue);
                const CadTriangleAtlasPart *replacement =
                    upsertTriangleAtlasPart(
                        pid, generation, positions, normals,
                        vertexCount, indices, indexCount,
                        progressive, glue, caps);
                if (replacement)
                    return replacement;
                /*
                 * The capacity proof excludes ordinary pressure failures, but
                 * a driver allocation may still fail.  Restore the exact old
                 * prefix from the authoritative leading arrays so a failed
                 * refinement never turns a visible mesh into a hole/box.
                 */
                return upsertTriangleAtlasPart(
                    pid, generation, positions, normals,
                    previous.vertexCount, indices,
                    previous.indexCount, progressive, glue, caps);
            }

            /*
             * Genuine atlas pressure: preserve the coherent old prefix and
             * let command construction clamp to it.  A later memory/view
             * epoch will retry this same bounded relocation.
             */
            if (found->second.generation != generation) {
                found->second.generation = generation;
                bumpTriangleAtlasRevision();
            }
            return &found->second;
        }
        releaseTriangleAtlasPart(pid, glue);
    }

    const auto findPage = [&]() -> uint32_t {
        uint32_t bestPage = UINT32_MAX;
        uint64_t bestWaste = std::numeric_limits<uint64_t>::max();
        for (uint32_t i = 0; i < triangleAtlasPages_.size(); ++i) {
            const CadTriangleAtlasPage *page = triangleAtlasPage(i);
            if (!page || page->dedicated ||
                    (hasNormals && !page->storesNormals))
                continue;
            if (page->largestFreeVertexCapacity < vertexReserve ||
                    page->largestFreeIndexCapacity < indexReserve)
                continue;
            /*
             * The exact best-fit choice is made within the selected page.
             * Across pages, use their exact maximum capacities as a constant
             * time fragmentation proxy.  This preserves bounded allocation
             * while avoiding two complete free-list scans per page.
             */
            const uint32_t vertexWaste =
                page->largestFreeVertexCapacity - vertexReserve;
            const uint32_t indexWaste =
                page->largestFreeIndexCapacity - indexReserve;
            const uint64_t waste =
                static_cast<uint64_t>(vertexWaste) *
                    (page->storesNormals ? 24u : 12u) +
                static_cast<uint64_t>(indexWaste) * 4u;
            if (bestPage == UINT32_MAX || waste < bestWaste) {
                bestPage = i;
                bestWaste = waste;
            }
        }
        return bestPage;
    };

    uint32_t pageIndex = findPage();
    if (pageIndex == UINT32_MAX &&
            !triangleAtlasReclamationDeferred_ &&
            triangleAtlasInactiveSweepFrame_ != triangleAtlasFrame_) {
        /*
         * Allocation pressure first retires parts absent from this frame.
         * This is also what makes erasing a sub-path reclaimable without
         * disturbing a shared part still referenced by another occurrence.
         *
         * The renderer touches every retained visible consumer before it
         * starts admitting new parts.  Consequently one complete sweep is
         * sufficient for the entire frame: after it has removed every
         * inactive candidate, repeating the same hash-table walk for each
         * subsequent failed admission cannot discover another victim.  The
         * former per-request sweep was O(unadmitted * resident), consuming
         * seconds on the GUI thread at 50k unique parts once the atlas filled.
         */
        triangleAtlasInactiveSweepFrame_ = triangleAtlasFrame_;
        std::vector<std::pair<uint64_t, PartId>> inactive;
        inactive.reserve(triangleAtlasParts_.size());
        for (const auto& item : triangleAtlasParts_)
            if (item.second.lastUsedFrame != triangleAtlasFrame_)
                inactive.emplace_back(item.second.lastUsedFrame, item.first);
        std::sort(inactive.begin(), inactive.end(),
            [](const auto& left, const auto& right) {
                return left.first < right.first;
            });
        for (const auto& victim : inactive) {
            releaseTriangleAtlasPart(victim.second, glue);
            pageIndex = findPage();
            if (pageIndex != UINT32_MAX)
                break;
        }
    }

    if (pageIndex == UINT32_MAX) {
        /*
         * Shape ordinary pages around the index/vertex ratio that requested
         * them.  A fixed 3:1 page left almost half of every vertex buffer
         * unused for typical manifold meshes (roughly 5-6 indices/vertex),
         * exhausting a 512 MiB allocation ceiling with only ~195 MiB live.
         * Smaller ratio-adaptive pages also bound internal fragmentation
         * while preserving hundreds of commands per MDI submission.
         */
        constexpr size_t targetPageBytes = 16u * 1024u * 1024u;
        constexpr uint32_t defaultCommands = 4096u;
        const size_t vertexStrideBytes = hasNormals ? 24u : 12u;
        const size_t requestBytes =
            static_cast<size_t>(vertexReserve) * vertexStrideBytes +
            static_cast<size_t>(indexReserve) * sizeof(uint32_t);
        const bool dedicated = requestBytes > targetPageBytes / 2u;
        uint32_t pageVertices = vertexReserve;
        uint32_t pageIndices = indexReserve;
        if (!dedicated) {
            const size_t commandBytes =
                static_cast<size_t>(defaultCommands) *
                sizeof(CadDrawElementsIndirectCommand);
            const size_t payloadBytes = targetPageBytes > commandBytes ?
                targetPageBytes - commandBytes : targetPageBytes;
            const long double indexRatio =
                static_cast<long double>(indexReserve) /
                static_cast<long double>(std::max<uint32_t>(
                    1u, vertexReserve));
            const long double bytesPerVertex =
                static_cast<long double>(vertexStrideBytes) +
                indexRatio * sizeof(uint32_t);
            const uint64_t shapedVertices = static_cast<uint64_t>(
                static_cast<long double>(payloadBytes) / bytesPerVertex);
            pageVertices = cadAtlasRoundUp(
                static_cast<uint32_t>(std::min<uint64_t>(
                    std::max<uint64_t>(shapedVertices, vertexReserve),
                    std::numeric_limits<uint32_t>::max())), 64u);
            const uint64_t shapedIndices = static_cast<uint64_t>(
                static_cast<long double>(pageVertices) * indexRatio);
            pageIndices = cadAtlasRoundUp(
                static_cast<uint32_t>(std::min<uint64_t>(
                    std::max<uint64_t>(shapedIndices, indexReserve),
                    std::numeric_limits<uint32_t>::max())), 192u);
        }

        std::unique_ptr<CadTriangleAtlasPage> page(
            new CadTriangleAtlasPage);
        page->vertexCapacity = pageVertices;
        page->indexCapacity = pageIndices;
        page->indirectCapacity = defaultCommands;
        page->dedicated = dedicated;
        page->storesNormals = hasNormals;
        page->freeVertices.push_back({0u, pageVertices});
        page->freeIndices.push_back({0u, pageIndices});
        page->largestFreeVertexCapacity = pageVertices;
        page->largestFreeIndexCapacity = pageIndices;
        const size_t pageBytes = page->allocatedBytes();

        /*
         * Delete empty retained pages before admitting new storage.  Empty
         * pages can exist after delayed tail/part reclamation; deleting them
         * here makes the configured ceiling an allocation ceiling, not merely
         * a live-data estimate.
         */
        for (uint32_t i = 0; i < triangleAtlasPages_.size(); ++i) {
            CadTriangleAtlasPage *old = triangleAtlasPage(i);
            if (old && !old->partCount)
                deleteTriangleAtlasPage(i, glue);
        }
        const size_t budget = cadTriangleAtlasBudget();
        if (pageBytes > budget ||
                triangleAtlasAllocatedBytes_ > budget - pageBytes)
            return nullptr;

        glue->glGenBuffers(1, &page->posBuf);
        glue->glBindBuffer(GL_ARRAY_BUFFER, page->posBuf);
        glue->glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(pageVertices) * 3 * sizeof(float),
            nullptr, GL_DYNAMIC_DRAW);
        if (page->storesNormals) {
            glue->glGenBuffers(1, &page->normBuf);
            glue->glBindBuffer(GL_ARRAY_BUFFER, page->normBuf);
            glue->glBufferData(
                GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(pageVertices) * 3 * sizeof(float),
                nullptr, GL_DYNAMIC_DRAW);
        }
        glue->glGenBuffers(1, &page->idxBuf);
        glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, page->idxBuf);
        glue->glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(pageIndices) * sizeof(uint32_t),
            nullptr, GL_DYNAMIC_DRAW);
        glue->glGenBuffers(1, &page->indirectBuf);
        glue->glBindBuffer(GL_DRAW_INDIRECT_BUFFER, page->indirectBuf);
        glue->glBufferData(
            GL_DRAW_INDIRECT_BUFFER,
            static_cast<GLsizeiptr>(defaultCommands) *
                sizeof(CadDrawElementsIndirectCommand),
            nullptr, GL_STREAM_DRAW);
        glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
        glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glue->glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

        pageIndex = triangleAtlasPages_.size();
        for (uint32_t i = 0; i < triangleAtlasPages_.size(); ++i) {
            if (!triangleAtlasPages_[i]) {
                pageIndex = i;
                triangleAtlasPages_[i] = std::move(page);
                break;
            }
        }
        if (pageIndex == triangleAtlasPages_.size())
            triangleAtlasPages_.push_back(std::move(page));
        triangleAtlasAllocatedBytes_ += pageBytes;
        bumpTriangleAtlasRevision();
    }

    CadTriangleAtlasPage *page = triangleAtlasPage(pageIndex);
    if (!page) return nullptr;
    CadTriangleAtlasPart part;
    part.page = pageIndex;
    if (!cadAtlasAllocateRange(
            page->freeVertices, vertexReserve, part.vertices,
            page->largestFreeVertexCapacity) ||
            !cadAtlasAllocateRange(
                page->freeIndices, indexReserve, part.indices,
                page->largestFreeIndexCapacity)) {
        page->largestFreeVertexCapacity = std::max(
            page->largestFreeVertexCapacity,
            cadAtlasFreeRange(
                page->freeVertices, part.vertices));
        page->largestFreeIndexCapacity = std::max(
            page->largestFreeIndexCapacity,
            cadAtlasFreeRange(
                page->freeIndices, part.indices));
        return nullptr;
    }
    part.vertexCount = vertexCount;
    part.indexCount = indexCount;
    part.requestedVertexCount = vertexCount;
    part.requestedIndexCount = indexCount;
    part.generation = generation;
    part.lastUsedFrame = triangleAtlasFrame_;
    part.hasNormals = hasNormals;
    part.progressive = progressive;
    ++page->partCount;

    const GLintptr vertexOffset =
        static_cast<GLintptr>(part.vertices.first) * 3 * sizeof(float);
    const GLsizeiptr vertexBytes =
        static_cast<GLsizeiptr>(vertexCount) * 3 * sizeof(float);
    const GLintptr indexOffset =
        static_cast<GLintptr>(part.indices.first) * sizeof(uint32_t);
    const GLsizeiptr indexBytes =
        static_cast<GLsizeiptr>(indexCount) * sizeof(uint32_t);
    glue->glBindBuffer(GL_ARRAY_BUFFER, page->posBuf);
    glue->glBufferSubData(
        GL_ARRAY_BUFFER, vertexOffset,
        vertexBytes, positions);
    if (page->normBuf) {
        glue->glBindBuffer(GL_ARRAY_BUFFER, page->normBuf);
        if (hasNormals) {
            glue->glBufferSubData(
                GL_ARRAY_BUFFER, vertexOffset,
                vertexBytes, normals);
        } else {
            cadAtlasUploadZeroNormals(
                glue, vertexOffset, vertexCount);
        }
    }
    glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, page->idxBuf);
    glue->glBufferSubData(
        GL_ELEMENT_ARRAY_BUFFER,
        indexOffset, indexBytes, indices);
    glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
    glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    const auto inserted = triangleAtlasParts_.emplace(pid, part);
    if (!inserted.second) {
        page->largestFreeVertexCapacity = std::max(
            page->largestFreeVertexCapacity,
            cadAtlasFreeRange(
                page->freeVertices, part.vertices));
        page->largestFreeIndexCapacity = std::max(
            page->largestFreeIndexCapacity,
            cadAtlasFreeRange(
                page->freeIndices, part.indices));
        if (page->partCount) --page->partCount;
        return nullptr;
    }
    bumpTriangleAtlasRevision();
    return &inserted.first->second;
}

bool CadGpuResources::uploadTriangleAtlasCommands(
        uint32_t pageIndex,
        const CadDrawElementsIndirectCommand *commands,
        size_t commandCount,
        const SoGLContext *glue)
{
    CadTriangleAtlasPage *page = triangleAtlasPage(pageIndex);
    if (!page || !glue || !page->indirectBuf || !commands ||
            !commandCount ||
            commandCount > page->indirectCapacity ||
            commandCount >
                static_cast<size_t>(std::numeric_limits<GLsizei>::max()))
        return false;
    glue->glBindBuffer(GL_DRAW_INDIRECT_BUFFER, page->indirectBuf);
    glue->glBufferSubData(
        GL_DRAW_INDIRECT_BUFFER, 0,
        static_cast<GLsizeiptr>(commandCount) *
            sizeof(CadDrawElementsIndirectCommand),
        commands);
    glue->glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    return true;
}

void CadGpuResources::endTriangleAtlasFrame(const SoGLContext *glue)
{
    if (!glue) return;
    if (triangleAtlasMaintenanceDeferred_) {
        triangleAtlasMaintenanceDeferred_ = false;
        return;
    }
    constexpr uint64_t shrinkDelayFrames = 120u;
    constexpr uint64_t unusedRetentionFrames = 600u;

    std::vector<PartId> stale;
    for (auto& item : triangleAtlasParts_) {
        CadTriangleAtlasPart& part = item.second;
        if (part.lastUsedFrame != triangleAtlasFrame_) {
            if (triangleAtlasFrame_ > part.lastUsedFrame &&
                    triangleAtlasFrame_ - part.lastUsedFrame >=
                        unusedRetentionFrames)
                stale.push_back(item.first);
            continue;
        }
        if (!part.lowerDemandSinceFrame ||
                triangleAtlasFrame_ < part.lowerDemandSinceFrame ||
                triangleAtlasFrame_ - part.lowerDemandSinceFrame <
                    shrinkDelayFrames)
            continue;
        CadTriangleAtlasPage *page = triangleAtlasPage(part.page);
        if (!page) continue;

        const uint32_t vertexTarget = cadAtlasReservedCount(
            part.requestedVertexCount, part.progressive, 64u);
        const uint32_t indexTarget = cadAtlasReservedCount(
            part.requestedIndexCount, part.progressive, 192u);
        /*
         * Hysteresis prevents a one-level oscillation from fragmenting pages.
         * We reclaim only when at least a quarter of a reservation becomes
         * free, and leave modest progressive headroom for the next zoom.
         */
        bool shrunk = false;
        if (vertexTarget < part.vertices.capacity &&
                vertexTarget <= part.vertices.capacity * 3u / 4u) {
            CadAtlasRange tail = {
                part.vertices.first + vertexTarget,
                part.vertices.capacity - vertexTarget
            };
            part.vertices.capacity = vertexTarget;
            page->largestFreeVertexCapacity = std::max(
                page->largestFreeVertexCapacity,
                cadAtlasFreeRange(page->freeVertices, tail));
            shrunk = true;
        }
        if (indexTarget < part.indices.capacity &&
                indexTarget <= part.indices.capacity * 3u / 4u) {
            CadAtlasRange tail = {
                part.indices.first + indexTarget,
                part.indices.capacity - indexTarget
            };
            part.indices.capacity = indexTarget;
            page->largestFreeIndexCapacity = std::max(
                page->largestFreeIndexCapacity,
                cadAtlasFreeRange(page->freeIndices, tail));
            shrunk = true;
        }
        const uint32_t previousVertexCount = part.vertexCount;
        const uint32_t previousIndexCount = part.indexCount;
        part.vertexCount =
            std::min(previousVertexCount, part.requestedVertexCount);
        part.indexCount =
            std::min(previousIndexCount, part.requestedIndexCount);
        part.lowerDemandSinceFrame = 0;
        if (shrunk || part.vertexCount != previousVertexCount ||
                part.indexCount != previousIndexCount)
            bumpTriangleAtlasRevision();
    }
    for (PartId pid : stale)
        releaseTriangleAtlasPart(pid, glue);
}

// ---------------------------------------------------------------------------
// invalidatePart()
// ---------------------------------------------------------------------------

void CadGpuResources::invalidatePart(PartId pid, const SoGLContext * glue)
{
    releaseTriangleAtlasPart(pid, glue);
    auto it = cache_.find(pid);
    if (it == cache_.end()) return;
    if (glue) {
        deletePointGpu(it->second.point, glue);
        deleteWireGpu(it->second.wire, glue);
        deleteTriGpu(it->second.tri,  glue);
        deleteProgressiveGpu(it->second, glue);
    }
    cache_.erase(it);
}

// ---------------------------------------------------------------------------
// uploadInstanceData()
// ---------------------------------------------------------------------------

void CadGpuResources::uploadInstanceData(const void* data, GLsizeiptr byteSize,
                                         const SoGLContext * glue)
{
    if (!glue || !glue->glGenBuffers || !data || byteSize <= 0)
        return;

    if (!instanceVbo_) {
        glue->glGenBuffers(1, &instanceVbo_);
    }
    glue->glBindBuffer(GL_ARRAY_BUFFER, instanceVbo_);
    if (instanceVboCapacityBytes_ < byteSize) {
        constexpr GLsizeiptr minimumCapacity = 64 * 1024;
        GLsizeiptr capacity = std::max(
            byteSize, minimumCapacity);
        if (instanceVboCapacityBytes_ > 0 &&
                instanceVboCapacityBytes_ <=
                    std::numeric_limits<GLsizeiptr>::max() -
                        instanceVboCapacityBytes_ / 2)
            capacity = std::max(
                capacity,
                instanceVboCapacityBytes_ +
                    instanceVboCapacityBytes_ / 2);
        glue->glBufferData(
            GL_ARRAY_BUFFER, capacity, nullptr, GL_DYNAMIC_DRAW);
        instanceVboCapacityBytes_ = capacity;
    }
    glue->glBufferSubData(
        GL_ARRAY_BUFFER, 0, byteSize, data);
    glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
    instanceVboBytes_ = byteSize;
    ++instanceUploadSerial_;
    if (!instanceUploadSerial_)
        instanceUploadSerial_ = 1;
}

void CadGpuResources::uploadTransientInstanceData(
        const void *data, GLsizeiptr byteSize,
        const SoGLContext *glue)
{
    if (!glue || !glue->glGenBuffers || !data || byteSize <= 0)
        return;

    if (!transientInstanceVbo_)
        glue->glGenBuffers(1, &transientInstanceVbo_);
    glue->glBindBuffer(GL_ARRAY_BUFFER, transientInstanceVbo_);
    if (transientInstanceVboCapacityBytes_ < byteSize) {
        constexpr GLsizeiptr minimumCapacity = 64 * 1024;
        GLsizeiptr capacity = std::max(byteSize, minimumCapacity);
        if (transientInstanceVboCapacityBytes_ > 0 &&
                transientInstanceVboCapacityBytes_ <=
                    std::numeric_limits<GLsizeiptr>::max() -
                        transientInstanceVboCapacityBytes_ / 2) {
            capacity = std::max(
                capacity,
                transientInstanceVboCapacityBytes_ +
                    transientInstanceVboCapacityBytes_ / 2);
        }
        glue->glBufferData(
            GL_ARRAY_BUFFER, capacity, nullptr, GL_STREAM_DRAW);
        transientInstanceVboCapacityBytes_ = capacity;
    }
    glue->glBufferSubData(
        GL_ARRAY_BUFFER, 0, byteSize, data);
    glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
}

bool CadGpuResources::updateInstanceData(
        GLintptr byteOffset, const void* data, GLsizeiptr byteSize,
        const SoGLContext *glue)
{
    if (!glue || !glue->glBufferSubData || !instanceVbo_ || !data ||
            byteOffset < 0 || byteSize <= 0 ||
            byteOffset > instanceVboBytes_ ||
            byteSize > instanceVboBytes_ - byteOffset)
        return false;
    glue->glBindBuffer(GL_ARRAY_BUFFER, instanceVbo_);
    glue->glBufferSubData(GL_ARRAY_BUFFER, byteOffset, byteSize, data);
    glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
    ++instanceUploadSerial_;
    if (!instanceUploadSerial_)
        instanceUploadSerial_ = 1;
    return true;
}

bool CadGpuResources::appendInstanceData(
        GLintptr expectedByteOffset, const void* data,
        GLsizeiptr byteSize, const SoGLContext *glue)
{
    if (!glue || !glue->glBufferSubData || !instanceVbo_ || !data ||
            expectedByteOffset < 0 || byteSize <= 0 ||
            expectedByteOffset != instanceVboBytes_ ||
            byteSize > instanceVboCapacityBytes_ - instanceVboBytes_)
        return false;
    glue->glBindBuffer(GL_ARRAY_BUFFER, instanceVbo_);
    glue->glBufferSubData(
        GL_ARRAY_BUFFER, expectedByteOffset, byteSize, data);
    glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
    instanceVboBytes_ += byteSize;
    ++instanceUploadSerial_;
    if (!instanceUploadSerial_)
        instanceUploadSerial_ = 1;
    return true;
}

void CadGpuResources::uploadFlatWire(
        uint64_t planRevision,
        uint64_t geometryRevision,
        const std::vector<float>& positions,
        const std::vector<CadFlatWireGroup>& groups,
        const std::unordered_map<CadFlatWireRangeKey,
                                 CadFlatWireRange,
                                 CadFlatWireRangeKeyHash>& ranges,
        GLsizei capacityVertexCount,
        const SoGLContext *glue,
        const CadGLCaps& caps)
{
    if (!glue || !caps.hasVBO || positions.empty()) return;

    const GLsizei vertexCount =
        static_cast<GLsizei>(positions.size() / 3);
    capacityVertexCount = std::max(vertexCount, capacityVertexCount);
    if (!flatWire_.posBuf)
        glue->glGenBuffers(1, &flatWire_.posBuf);
    glue->glBindBuffer(GL_ARRAY_BUFFER, flatWire_.posBuf);
    glue->glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(capacityVertexCount) * 3 * sizeof(float),
        nullptr, GL_DYNAMIC_DRAW);
    glue->glBufferSubData(
        GL_ARRAY_BUFFER, 0,
        static_cast<GLsizeiptr>(positions.size() * sizeof(float)),
        positions.data());

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
    flatWire_.vertexCount = vertexCount;
    flatWire_.capacityVertexCount = capacityVertexCount;
    flatWire_.groups = groups;
    flatWire_.rangeSlots.clear();
    flatWire_.ranges = ranges;
}

bool CadGpuResources::appendFlatWire(
        const std::vector<float>& positions,
        const std::unordered_map<CadFlatWireRangeKey,
                                 CadFlatWireRange,
                                 CadFlatWireRangeKeyHash>& ranges,
        const SoGLContext *glue)
{
    if (!glue || positions.empty() || !flatWire_.posBuf)
        return false;
    const GLsizei appended =
        static_cast<GLsizei>(positions.size() / 3);
    if (appended <= 0 ||
            flatWire_.vertexCount >
                flatWire_.capacityVertexCount - appended)
        return false;

    const GLsizeiptr byteOffset =
        static_cast<GLsizeiptr>(flatWire_.vertexCount) *
        3 * sizeof(float);
    const GLsizeiptr byteCount =
        static_cast<GLsizeiptr>(positions.size() * sizeof(float));
    glue->glBindBuffer(GL_ARRAY_BUFFER, flatWire_.posBuf);
    glue->glBufferSubData(
        GL_ARRAY_BUFFER, byteOffset, byteCount, positions.data());
    glue->glBindBuffer(GL_ARRAY_BUFFER, 0);

    flatWire_.vertexCount += appended;
    flatWire_.ranges.insert(ranges.begin(), ranges.end());
    return true;
}

void CadGpuResources::updateFlatWireGroups(
        uint64_t planRevision,
        const std::vector<CadFlatWireGroup>& groups)
{
    flatWire_.planRevision = planRevision;
    flatWire_.groups = groups;
}

bool CadGpuResources::lookupFlatWireRange(
        size_t visibleInstanceIndex,
        const CadFlatWireRangeKey& key,
        CadFlatWireRange *range)
{
    if (!range)
        return false;
    if (visibleInstanceIndex < flatWire_.rangeSlots.size()) {
        const CadFlatWireGpu::RangeMap::value_type *slot =
            flatWire_.rangeSlots[visibleInstanceIndex];
        if (slot && slot->first == key) {
            *range = slot->second;
            return true;
        }
    }

    const auto found = flatWire_.ranges.find(key);
    if (found == flatWire_.ranges.end())
        return false;
    if (visibleInstanceIndex >= flatWire_.rangeSlots.size())
        flatWire_.rangeSlots.resize(visibleInstanceIndex + 1, nullptr);
    flatWire_.rangeSlots[visibleInstanceIndex] = &*found;
    *range = found->second;
    return true;
}

void CadGpuResources::uploadFlatShaded(
        uint64_t planRevision,
        uint64_t geometryRevision,
        const std::vector<float>& positions,
        const std::vector<float>& normals,
        const std::vector<CadFlatShadedGroup>& groups,
        const std::unordered_map<CadFlatShadedRangeKey,
                                 CadFlatShadedRange,
                                 CadFlatShadedRangeKeyHash>& ranges,
        GLsizei capacityVertexCount,
        const SoGLContext *glue,
        const CadGLCaps& caps)
{
    if (!glue || positions.empty() || positions.size() != normals.size())
        return;
    const GLsizei vertexCount =
        static_cast<GLsizei>(positions.size() / 3);
    capacityVertexCount = std::max(vertexCount, capacityVertexCount);
    if (!flatShaded_.posBuf)
        glue->glGenBuffers(1, &flatShaded_.posBuf);
    if (!flatShaded_.normBuf)
        glue->glGenBuffers(1, &flatShaded_.normBuf);
    glue->glBindBuffer(GL_ARRAY_BUFFER, flatShaded_.posBuf);
    glue->glBufferData(GL_ARRAY_BUFFER,
                       static_cast<GLsizeiptr>(capacityVertexCount) *
                           3 * sizeof(float),
                       nullptr, GL_DYNAMIC_DRAW);
    glue->glBufferSubData(GL_ARRAY_BUFFER, 0,
                         static_cast<GLsizeiptr>(
                             positions.size() * sizeof(float)),
                         positions.data());
    glue->glBindBuffer(GL_ARRAY_BUFFER, flatShaded_.normBuf);
    glue->glBufferData(GL_ARRAY_BUFFER,
                       static_cast<GLsizeiptr>(capacityVertexCount) *
                           3 * sizeof(float),
                       nullptr, GL_DYNAMIC_DRAW);
    glue->glBufferSubData(GL_ARRAY_BUFFER, 0,
                         static_cast<GLsizeiptr>(
                             normals.size() * sizeof(float)),
                         normals.data());

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
    flatShaded_.vertexCount = vertexCount;
    flatShaded_.capacityVertexCount = capacityVertexCount;
    flatShaded_.groups = groups;
    flatShaded_.ranges = ranges;
}

bool CadGpuResources::appendFlatShaded(
        const std::vector<float>& positions,
        const std::vector<float>& normals,
        const std::unordered_map<CadFlatShadedRangeKey,
                                 CadFlatShadedRange,
                                 CadFlatShadedRangeKeyHash>& ranges,
        const SoGLContext *glue)
{
    if (!glue || positions.empty() || positions.size() != normals.size() ||
            !flatShaded_.posBuf || !flatShaded_.normBuf)
        return false;
    const GLsizei appended =
        static_cast<GLsizei>(positions.size() / 3);
    if (appended <= 0 ||
            flatShaded_.vertexCount >
                flatShaded_.capacityVertexCount - appended)
        return false;

    const GLsizeiptr byteOffset =
        static_cast<GLsizeiptr>(flatShaded_.vertexCount) *
        3 * sizeof(float);
    const GLsizeiptr byteCount =
        static_cast<GLsizeiptr>(positions.size() * sizeof(float));
    glue->glBindBuffer(GL_ARRAY_BUFFER, flatShaded_.posBuf);
    glue->glBufferSubData(GL_ARRAY_BUFFER, byteOffset, byteCount,
                          positions.data());
    glue->glBindBuffer(GL_ARRAY_BUFFER, flatShaded_.normBuf);
    glue->glBufferSubData(GL_ARRAY_BUFFER, byteOffset, byteCount,
                          normals.data());
    glue->glBindBuffer(GL_ARRAY_BUFFER, 0);

    flatShaded_.vertexCount += appended;
    flatShaded_.ranges.insert(ranges.begin(), ranges.end());
    return true;
}

void CadGpuResources::updateFlatShadedGroups(
        uint64_t planRevision,
        const std::vector<CadFlatShadedGroup>& groups)
{
    flatShaded_.planRevision = planRevision;
    flatShaded_.groups = groups;
}

void CadGpuResources::releaseFlatShaded(const SoGLContext *glue)
{
    if (glue) {
        if (flatShaded_.posBuf && glue->glDeleteBuffers)
            glue->glDeleteBuffers(1, &flatShaded_.posBuf);
        if (flatShaded_.normBuf && glue->glDeleteBuffers)
            glue->glDeleteBuffers(1, &flatShaded_.normBuf);
        if (flatShaded_.vao && glue->glDeleteVertexArrays)
            glue->glDeleteVertexArrays(1, &flatShaded_.vao);
    }
    flatShaded_ = CadFlatShadedGpu();
}

void CadGpuResources::releaseStandaloneTriangles(const SoGLContext *glue)
{
    if (!glue) return;
    for (auto& item : cache_) {
        deleteTriGpu(item.second.tri, glue);
        for (CadProgressiveGpu& cut : item.second.progressiveTri)
            deleteProgressiveGpu(cut, glue);
    }
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

    const GLsizei count =
        static_cast<GLsizei>(positions.size() / 3);
    if (subpixelProxyPoints_.capacityCount < count) {
        const GLsizei minimumReserve = 4096;
        GLsizei capacity = std::max(count, minimumReserve);
        if (count <= std::numeric_limits<GLsizei>::max() / 2)
            capacity = std::max(capacity, count * 2);
        glue->glBindBuffer(
            GL_ARRAY_BUFFER, subpixelProxyPoints_.posBuf);
        glue->glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(capacity) *
                3 * sizeof(float),
            nullptr, GL_DYNAMIC_DRAW);
        glue->glBindBuffer(
            GL_ARRAY_BUFFER, subpixelProxyPoints_.colorBuf);
        glue->glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(capacity) *
                4 * sizeof(uint8_t),
            nullptr, GL_DYNAMIC_DRAW);
        subpixelProxyPoints_.capacityCount = capacity;
    }
    glue->glBindBuffer(GL_ARRAY_BUFFER, subpixelProxyPoints_.posBuf);
    glue->glBufferSubData(
        GL_ARRAY_BUFFER, 0,
        static_cast<GLsizeiptr>(
            positions.size() * sizeof(float)),
        positions.data());
    glue->glBindBuffer(GL_ARRAY_BUFFER, subpixelProxyPoints_.colorBuf);
    glue->glBufferSubData(
        GL_ARRAY_BUFFER, 0,
        static_cast<GLsizeiptr>(
            colors.size() * sizeof(uint8_t)),
        colors.data());

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
    subpixelProxyPoints_.count = count;
}

void CadGpuResources::uploadPressureProxyPoints(
        uint64_t revision,
        const std::vector<float>& positions,
        const std::vector<uint8_t>& colors,
        const SoGLContext *glue,
        const CadGLCaps& caps)
{
    if (!glue || positions.empty() || positions.size() % 3 != 0 ||
            colors.size() != (positions.size() / 3) * 4 ||
            positions.size() / 3 >
                static_cast<size_t>(
                    std::numeric_limits<GLsizei>::max()))
        return;

    const GLsizei count =
        static_cast<GLsizei>(positions.size() / 3);
    if (!pressureProxyPoints_.posBuf)
        glue->glGenBuffers(1, &pressureProxyPoints_.posBuf);
    if (!pressureProxyPoints_.colorBuf)
        glue->glGenBuffers(1, &pressureProxyPoints_.colorBuf);

    if (pressureProxyPoints_.capacityCount < count) {
        const GLsizei minimumReserve = 4096;
        GLsizei capacity = std::max(count, minimumReserve);
        if (count <= std::numeric_limits<GLsizei>::max() / 2)
            capacity = std::max(capacity, count * 2);
        glue->glBindBuffer(
            GL_ARRAY_BUFFER, pressureProxyPoints_.posBuf);
        glue->glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(capacity) *
                3 * sizeof(float),
            nullptr, GL_DYNAMIC_DRAW);
        glue->glBindBuffer(
            GL_ARRAY_BUFFER, pressureProxyPoints_.colorBuf);
        glue->glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(capacity) *
                4 * sizeof(uint8_t),
            nullptr, GL_DYNAMIC_DRAW);
        pressureProxyPoints_.capacityCount = capacity;
    }

    glue->glBindBuffer(
        GL_ARRAY_BUFFER, pressureProxyPoints_.posBuf);
    glue->glBufferSubData(
        GL_ARRAY_BUFFER, 0,
        static_cast<GLsizeiptr>(
            positions.size() * sizeof(float)),
        positions.data());
    glue->glBindBuffer(
        GL_ARRAY_BUFFER, pressureProxyPoints_.colorBuf);
    glue->glBufferSubData(
        GL_ARRAY_BUFFER, 0,
        static_cast<GLsizeiptr>(
            colors.size() * sizeof(uint8_t)),
        colors.data());

    if (!pressureProxyPoints_.vao && caps.hasVAO &&
            glue->glGenVertexArrays) {
        glue->glGenVertexArrays(1, &pressureProxyPoints_.vao);
        glue->glBindVertexArray(pressureProxyPoints_.vao);
        glue->glBindBuffer(
            GL_ARRAY_BUFFER, pressureProxyPoints_.posBuf);
        glue->glVertexAttribPointerARB(
            0, 3, GL_FLOAT, GL_FALSE,
            3 * sizeof(float), nullptr);
        glue->glEnableVertexAttribArrayARB(0);
        glue->glBindBuffer(
            GL_ARRAY_BUFFER, pressureProxyPoints_.colorBuf);
        glue->glVertexAttribPointerARB(
            1, 4, GL_UNSIGNED_BYTE, GL_TRUE,
            4 * sizeof(uint8_t), nullptr);
        glue->glEnableVertexAttribArrayARB(1);
        glue->glBindVertexArray(0);
    }
    glue->glBindBuffer(GL_ARRAY_BUFFER, 0);

    pressureProxyPoints_.revision = revision;
    pressureProxyPoints_.count = count;
}

bool CadGpuResources::appendPressureProxyPoints(
        uint64_t expectedRevision,
        uint64_t revision,
        const std::vector<float>& positions,
        const std::vector<uint8_t>& colors,
        const SoGLContext *glue)
{
    if (!glue || positions.empty() || positions.size() % 3 != 0 ||
            colors.size() != (positions.size() / 3) * 4 ||
            positions.size() / 3 >
                static_cast<size_t>(
                    std::numeric_limits<GLsizei>::max()) ||
            !pressureProxyPoints_.posBuf ||
            !pressureProxyPoints_.colorBuf ||
            pressureProxyPoints_.revision != expectedRevision)
        return false;
    const GLsizei appended =
        static_cast<GLsizei>(positions.size() / 3);
    if (appended <= 0 ||
            pressureProxyPoints_.count >
                pressureProxyPoints_.capacityCount - appended)
        return false;

    const GLsizeiptr positionOffset =
        static_cast<GLsizeiptr>(pressureProxyPoints_.count) *
        3 * sizeof(float);
    const GLsizeiptr colorOffset =
        static_cast<GLsizeiptr>(pressureProxyPoints_.count) *
        4 * sizeof(uint8_t);
    glue->glBindBuffer(
        GL_ARRAY_BUFFER, pressureProxyPoints_.posBuf);
    glue->glBufferSubData(
        GL_ARRAY_BUFFER, positionOffset,
        static_cast<GLsizeiptr>(
            positions.size() * sizeof(float)),
        positions.data());
    glue->glBindBuffer(
        GL_ARRAY_BUFFER, pressureProxyPoints_.colorBuf);
    glue->glBufferSubData(
        GL_ARRAY_BUFFER, colorOffset,
        static_cast<GLsizeiptr>(
            colors.size() * sizeof(uint8_t)),
        colors.data());
    glue->glBindBuffer(GL_ARRAY_BUFFER, 0);

    pressureProxyPoints_.revision = revision;
    pressureProxyPoints_.count += appended;
    return true;
}

// ---------------------------------------------------------------------------
// Non-blocking GPU frame timing
// ---------------------------------------------------------------------------

bool
CadGpuResources::beginFrameGpuTimer(const SoGLContext *glue)
{
    if (!glue)
        return false;

    if (!gpuTimerSupportKnown_) {
        gpuTimerSupportKnown_ = true;
        gpuTimerSupported_ =
            glue->glGenQueries && glue->glDeleteQueries &&
            glue->glBeginQuery && glue->glEndQuery &&
            glue->glGetQueryObjectuiv &&
            (SoGLContext_glversion_matches_at_least(glue, 3, 3, 0) ||
             SoGLContext_glext_supported(glue, "GL_ARB_timer_query"));

        if (gpuTimerSupported_) {
            std::array<GLuint, 3> queries = {{0, 0, 0}};
            glue->glGenQueries(
                static_cast<GLsizei>(queries.size()), queries.data());
            for (size_t i = 0; i < queries.size(); ++i)
                gpuTimerSlots_[i].query = queries[i];
            for (const GpuTimerSlot& slot : gpuTimerSlots_) {
                if (!slot.query) {
                    gpuTimerSupported_ = false;
                    break;
                }
            }
            if (!gpuTimerSupported_) {
                glue->glDeleteQueries(
                    static_cast<GLsizei>(queries.size()), queries.data());
                for (GpuTimerSlot& slot : gpuTimerSlots_)
                    slot = GpuTimerSlot();
            }
        }
    }

    if (!gpuTimerSupported_ || gpuTimerActiveSlot_ >= 0)
        return false;

    /*
     * Harvest every result which is already complete.  GL_QUERY_RESULT is
     * issued only after AVAILABLE succeeds, so this path cannot serialize the
     * GUI thread with previously queued raster work.  Results can complete
     * out of slot order; publish only the newest submission.
     */
    for (GpuTimerSlot& slot : gpuTimerSlots_) {
        if (!slot.pending || !slot.query)
            continue;
        GLuint available = GL_FALSE;
        glue->glGetQueryObjectuiv(
            slot.query, GL_QUERY_RESULT_AVAILABLE, &available);
        if (!available)
            continue;

        uint64_t nanoseconds = 0;
        if (glue->glGetQueryObjectui64v) {
            glue->glGetQueryObjectui64v(
                slot.query, GL_QUERY_RESULT, &nanoseconds);
        } else {
            GLuint limitedNanoseconds = 0;
            glue->glGetQueryObjectuiv(
                slot.query, GL_QUERY_RESULT, &limitedNanoseconds);
            nanoseconds = limitedNanoseconds;
        }
        slot.pending = false;
        if (slot.submission > gpuTimerLastCompletedSubmission_) {
            gpuTimerLastCompletedSubmission_ = slot.submission;
            gpuTimerLastNanoseconds_ = nanoseconds;
            gpuTimerLastTriangleCount_ = slot.triangleCount;
        }
    }

    for (size_t probe = 0; probe < gpuTimerSlots_.size(); ++probe) {
        const size_t index =
            (gpuTimerNextSlot_ + probe) % gpuTimerSlots_.size();
        GpuTimerSlot& slot = gpuTimerSlots_[index];
        if (slot.pending || !slot.query)
            continue;
        GLint currentQuery = 0;
        glue->glGetQueryiv(
            GL_TIME_ELAPSED, GL_CURRENT_QUERY, &currentQuery);
        if (currentQuery)
            return false;
        slot.submission = gpuTimerNextSubmission_++;
        if (!gpuTimerNextSubmission_)
            gpuTimerNextSubmission_ = 1;
        slot.triangleCount = 0;
        glue->glBeginQuery(GL_TIME_ELAPSED, slot.query);
        gpuTimerActiveSlot_ = static_cast<int>(index);
        gpuTimerNextSlot_ = (index + 1) % gpuTimerSlots_.size();
        return true;
    }

    return false;
}

void
CadGpuResources::endFrameGpuTimer(
        uint64_t triangleCount, const SoGLContext *glue)
{
    if (!glue || gpuTimerActiveSlot_ < 0)
        return;
    const size_t index = static_cast<size_t>(gpuTimerActiveSlot_);
    glue->glEndQuery(GL_TIME_ELAPSED);
    gpuTimerSlots_[index].triangleCount = triangleCount;
    gpuTimerSlots_[index].pending = true;
    gpuTimerActiveSlot_ = -1;
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
            deleteProgressiveGpu(kv.second, glue);
        }
        if (instanceVbo_ && glue->glDeleteBuffers) {
            glue->glDeleteBuffers(1, &instanceVbo_);
        }
        if (transientInstanceVbo_ && glue->glDeleteBuffers)
            glue->glDeleteBuffers(1, &transientInstanceVbo_);
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
        if (pressureProxyPoints_.posBuf && glue->glDeleteBuffers)
            glue->glDeleteBuffers(1, &pressureProxyPoints_.posBuf);
        if (pressureProxyPoints_.colorBuf && glue->glDeleteBuffers)
            glue->glDeleteBuffers(1, &pressureProxyPoints_.colorBuf);
        if (pressureProxyPoints_.vao && glue->glDeleteVertexArrays)
            glue->glDeleteVertexArrays(1, &pressureProxyPoints_.vao);
        /*
         * Page deletion requires an empty reference count.  releaseAll owns
         * the complete cache, so clear part ownership first and then delete
         * every independently allocated page.
         */
        triangleAtlasParts_.clear();
        for (uint32_t i = 0; i < triangleAtlasPages_.size(); ++i) {
            if (triangleAtlasPages_[i])
                triangleAtlasPages_[i]->partCount = 0;
            deleteTriangleAtlasPage(i, glue);
        }
        if (glue->glDeleteQueries) {
            std::array<GLuint, 3> queries = {{0, 0, 0}};
            size_t queryCount = 0;
            for (const GpuTimerSlot& slot : gpuTimerSlots_) {
                if (slot.query)
                    queries[queryCount++] = slot.query;
            }
            if (queryCount)
                glue->glDeleteQueries(
                    static_cast<GLsizei>(queryCount), queries.data());
        }
    }
    cache_.clear();
    instanceVbo_ = 0;
    instanceVboBytes_ = 0;
    instanceVboCapacityBytes_ = 0;
    instanceUploadSerial_ = 0;
    transientInstanceVbo_ = 0;
    transientInstanceVboCapacityBytes_ = 0;
    flatWire_ = CadFlatWireGpu();
    flatShaded_ = CadFlatShadedGpu();
    subpixelProxyPoints_ = CadSubpixelProxyGpu();
    pressureProxyPoints_ = CadSubpixelProxyGpu();
    progressiveBytes_ = 0;
    progressiveFrame_ = 0;
    triangleAtlasParts_.clear();
    triangleAtlasPages_.clear();
    triangleAtlasFrame_ = 0;
    triangleAtlasInactiveSweepFrame_ = 0;
    triangleAtlasMaintenanceDeferred_ = false;
    triangleAtlasReclamationDeferred_ = false;
    bumpTriangleAtlasRevision();
    triangleAtlasAllocatedBytes_ = 0;
    for (GpuTimerSlot& slot : gpuTimerSlots_)
        slot = GpuTimerSlot();
    gpuTimerSupportKnown_ = false;
    gpuTimerSupported_ = false;
    gpuTimerActiveSlot_ = -1;
    gpuTimerNextSlot_ = 0;
    gpuTimerNextSubmission_ = 1;
    gpuTimerLastCompletedSubmission_ = 0;
    gpuTimerLastNanoseconds_ = 0;
    gpuTimerLastTriangleCount_ = 0;
}

} // namespace internal
} // namespace Obol
