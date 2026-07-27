#ifndef OBOL_CAD_CADGPURESOURCES_H
#define OBOL_CAD_CADGPURESOURCES_H

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
 * @file CadGpuResources.h
 * @brief Per-context, per-part GPU buffer management for the CAD renderer.
 *
 * CadGpuResources caches VBOs (and VAOs when available) for each
 * (contextId, partId) combination.  It is owned by CadRendererGL and
 * shared across frames.  Resources are invalidated when part geometry
 * changes (tracked via a generation counter).
 *
 * Layout of each CadPartGpuRep:
 *  - pointPosBuf: interleaved float[3] point primitive positions
 *  - wirePosBuf : interleaved float[3] positions for all polyline points
 *  - wireSegBuf : uint32 pairs (start, end) index pairs into wirePosBuf
 *  - triPosBuf  : interleaved float[3] positions for triangle vertices
 *  - triNormBuf : interleaved float[3] normals (may be 0 if no normals)
 *  - triIdxBuf  : uint32 triangle indices (3 per triangle)
 *  - wireVAO    : VAO binding wirePosBuf (only if caps.hasVAO)
 *  - triVAO     : VAO binding triPosBuf + triNormBuf (only if caps.hasVAO)
 */

#include <Obol/cad/CadIds.h>
#include "CadGLCaps.h"

#include <Inventor/system/gl.h>

#include <unordered_map>
#include <vector>
#include <array>
#include <cstdint>

struct SoGLContext;

namespace Obol {
namespace internal {

/** GPU buffers for one part's point representation. */
struct CadPointGpu {
    GLuint posBuf = 0;
    GLuint vao = 0;
    GLsizei count = 0;
    GLsizei posCapacity = 0;
};

/** GPU buffers for one part's wire representation. */
struct CadWireGpu {
    GLuint posBuf    = 0; ///< float[3] positions for all wire points
    GLuint segIdxBuf = 0; ///< uint32 pairs: segment indices, or 0 for sequential pairs
    GLuint vao       = 0; ///< VAO binding posBuf (0 if no VAO support)
    GLsizei segCount = 0; ///< number of line segments (indices / 2)
    GLsizei vertCount = 0; ///< total point count in posBuf
    GLsizei posCapacity = 0; ///< allocated float[3] entries
    GLsizei idxCount = 0; ///< allocated/logical segment index entries
    GLsizei idxCapacity = 0; ///< allocated uint32 entries
    bool sequentialSegments = false; ///< true when positions are already segment pairs
    GLuint instanceVbo = 0; ///< instance buffer recorded in this VAO
    uint32_t instanceBase = UINT32_MAX; ///< first instance recorded in this VAO
};

/** GPU buffers for one part's shaded triangle representation. */
struct CadTriGpu {
    GLuint posBuf   = 0; ///< float[3] positions
    GLuint normBuf  = 0; ///< float[3] normals (0 if no normals supplied)
    GLuint idxBuf   = 0; ///< uint32 triangle indices
    GLuint vao      = 0; ///< VAO binding pos+norm+idx (0 if no VAO support)
    GLsizei vertCount = 0; ///< total point/normal entries
    GLsizei idxCount = 0; ///< total index count (= 3 × triangle count)
    GLsizei posCapacity = 0; ///< allocated float[3] position entries
    GLsizei normCapacity = 0; ///< allocated float[3] normal entries
    GLsizei idxCapacity = 0; ///< allocated uint32 entries
    GLuint instanceVbo = 0; ///< instance buffer recorded in this VAO
    uint32_t instanceBase = UINT32_MAX; ///< first instance recorded in this VAO
};

/**
 * Fixed-function VBOs for one retained PoP coordinate cut.
 *
 * Indexed entries contain one snapped position per source vertex and reuse
 * the ordinary triangle index/normal buffers.  Expanded entries contain
 * triangle-corner positions and normals and are used when the source has no
 * normals, preserving flat lighting without per-frame glBegin/glVertex work.
 */
struct CadProgressiveGpu {
    GLuint posBuf = 0;
    GLuint normBuf = 0;
    GLsizei vertexCount = 0;
    bool indexed = false;
    size_t bytes = 0;
    uint64_t lastUsedFrame = 0;
};

/** All GPU representations for one part in one GL context. */
struct CadPartGpuRep {
    uint64_t generation = UINT64_MAX; ///< UINT64_MAX = not yet uploaded; otherwise matches part generation
    CadPointGpu point;
    CadWireGpu wire;
    CadTriGpu  tri;
};

struct CadFlatWireGroup {
    GLint first = 0;
    GLsizei count = 0;
    float lineWidth = 1.0f;
    uint16_t linePattern = 0xffffu;
    uint16_t linePatternFactor = 1u;
    uint8_t rgba[4] = {204, 204, 204, 255};
};

struct CadFlatWireGpu {
    GLuint posBuf = 0;
    GLuint vao = 0;
    uint64_t planRevision = 0;
    uint64_t geometryRevision = 0;
    GLsizei vertexCount = 0;
    std::vector<CadFlatWireGroup> groups;
};

struct CadFlatShadedGroup {
    GLint first = 0;
    GLsizei count = 0;
    uint8_t rgba[4] = {204, 204, 204, 255};
    bool cullBackfaces = false;
};

struct CadFlatShadedGpu {
    GLuint posBuf = 0;
    GLuint normBuf = 0;
    GLuint vao = 0;
    uint64_t planRevision = 0;
    uint64_t geometryRevision = 0;
    GLsizei vertexCount = 0;
    std::vector<CadFlatShadedGroup> groups;
};

/** Dynamic world-space vertex/color buffers for one frame's proxy points. */
struct CadSubpixelProxyGpu {
    GLuint posBuf = 0;
    GLuint colorBuf = 0;
    GLuint vao = 0;
    uint64_t revision = 0;
    GLsizei count = 0;
};

/**
 * @brief GPU resource cache shared across frames within one GL context.
 *
 * One CadGpuResources instance exists per SoCADAssembly per GL context.
 * Parts are uploaded lazily on first use and invalidated when geometry
 * changes.  The per-instance VBO used by the instanced-draw path is
 * rebuilt every frame.
 */
class CadGpuResources {
public:
    CadGpuResources() = default;
    ~CadGpuResources();

    /**
     * Ensure the GPU representation for @p pid is current.
     *
     * @param pid        Part ID.
     * @param pointData  Point geometry as packed float[3] positions.
     * @param pointCount Number of float[3] entries in pointData.
     * @param wireData   Wire geometry as packed float[3] positions.
     *                   May be nullptr if the part has no wire rep.
     * @param wireCount  Number of float[3] entries in wireData.
     * @param segIdx     Segment index pairs (start,end) into wireData.
     *                   May be nullptr when wireData is already consecutive
     *                   endpoint pairs for GL_LINES.
     * @param segIdxCount Number of uint32 elements in segIdx.
     * @param triPos     Triangle vertex positions, may be nullptr.
     * @param triPosCount Number of float[3] entries in triPos.
     * @param triNorm    Triangle normals (same count as triPos), may be nullptr.
     * @param triIdx     Triangle indices, may be nullptr.
     * @param triIdxCount Number of uint32 elements in triIdx.
     * @param generation Part generation counter (invalidates cached data).
     * @param glue       GL dispatch context.
     * @param caps       GL capability flags.
     */
    void upload(PartId pid,
                const float*    pointData,   GLsizei pointCount,
                const float*    wireData,    GLsizei wireCount,
                const uint32_t* segIdx,      GLsizei segIdxCount,
                const float*    triPos,      GLsizei triPosCount,
                const float*    triNorm,
                const uint32_t* triIdx,      GLsizei triIdxCount,
                uint64_t        generation,
                bool            progressive,
                const SoGLContext * glue,
                const CadGLCaps& caps);

    /**
     * Check whether the GPU data for @p pid is already current.
     *
     * Returns true when the cached generation for @p pid matches @p gen.
     * This allows callers to skip the expensive CPU-side array-building step
     * before calling upload() when the geometry has not changed.
     */
    bool isUpToDate(
        PartId pid, uint64_t gen, GLsizei requiredWirePoints = 0,
        GLsizei requiredWireIndices = 0,
        GLsizei requiredTriPoints = 0,
        GLsizei requiredTriIndices = 0) const;

    /** Return the point GPU rep for @p pid, or nullptr if not uploaded. */
    const CadPointGpu* pointFor(PartId pid) const;

    /** Return the wire GPU rep for @p pid, or nullptr if not uploaded. */
    const CadWireGpu* wireFor(PartId pid) const;

    /** Mutable wire rep for updating retained VAO instance bindings. */
    CadWireGpu* wireFor(PartId pid);

    /** Return the tri GPU rep for @p pid, or nullptr if not uploaded. */
    const CadTriGpu* triFor(PartId pid) const;

    /** Mutable triangle rep for updating retained VAO instance bindings. */
    CadTriGpu* triFor(PartId pid);

    /** Return a cached fixed-function PoP cut, or nullptr if not built. */
    const CadProgressiveGpu* progressiveFor(
        PartId pid, bool shaded, uint8_t level);

    /** Upload one fixed-function PoP cut for reuse across frames/instances. */
    void uploadProgressive(
        PartId pid, bool shaded, uint8_t level,
        const std::vector<float>& positions,
        const std::vector<float>& normals,
        bool indexed, const SoGLContext *glue);

    /** Delimit a render so active PoP cuts survive cache-budget pruning. */
    void beginProgressiveFrame();
    void endProgressiveFrame(const SoGLContext *glue);

    /**
     * Invalidate and delete GPU resources for @p pid.
     * Called when a part is removed or its geometry changes.
     */
    void invalidatePart(PartId pid, const SoGLContext * glue);

    /**
     * Upload the per-instance VBO used in the instanced-draw path.
     *
     * @param data         Pointer to tightly-packed CadVisibleInstance records.
     * @param byteSize     Total byte size of the data.
     * @param glue         GL dispatch context.
     */
    void uploadInstanceData(const void* data, GLsizeiptr byteSize,
                            const SoGLContext * glue);

    /** Return the per-instance VBO name, or 0 if not uploaded. */
    GLuint instanceVbo() const { return instanceVbo_; }

    void uploadFlatWire(uint64_t planRevision,
                        uint64_t geometryRevision,
                        const std::vector<float>& positions,
                        const std::vector<CadFlatWireGroup>& groups,
                        const SoGLContext *glue,
                        const CadGLCaps& caps);

    void updateFlatWireGroups(uint64_t planRevision,
                              const std::vector<CadFlatWireGroup>& groups);

    const CadFlatWireGpu& flatWire() const { return flatWire_; }

    void uploadFlatShaded(uint64_t planRevision,
                          uint64_t geometryRevision,
                          const std::vector<float>& positions,
                          const std::vector<float>& normals,
                          const std::vector<CadFlatShadedGroup>& groups,
                          const SoGLContext *glue,
                          const CadGLCaps& caps);

    void updateFlatShadedGroups(
        uint64_t planRevision,
        const std::vector<CadFlatShadedGroup>& groups);

    const CadFlatShadedGpu& flatShaded() const { return flatShaded_; }

    void uploadSubpixelProxyPoints(uint64_t revision,
                                   const std::vector<float>& positions,
                                   const std::vector<uint8_t>& colors,
                                   const SoGLContext *glue,
                                   const CadGLCaps& caps);

    const CadSubpixelProxyGpu& subpixelProxyPoints() const
    {
        return subpixelProxyPoints_;
    }

    /** Release all GL resources (call with the correct GL context active). */
    void releaseAll(const SoGLContext * glue);

private:
    struct Entry {
        uint64_t    generation = 0;
        CadPointGpu point;
        CadWireGpu  wire;
        CadTriGpu   tri;
        std::array<CadProgressiveGpu, 16> progressiveWire;
        std::array<CadProgressiveGpu, 16> progressiveTri;
    };

    std::unordered_map<PartId, Entry, std::hash<PartId>> cache_;
    GLuint instanceVbo_ = 0;
    CadFlatWireGpu flatWire_;
    CadFlatShadedGpu flatShaded_;
    CadSubpixelProxyGpu subpixelProxyPoints_;
    uint64_t progressiveFrame_ = 0;
    size_t progressiveBytes_ = 0;

    void deletePointGpu(CadPointGpu& p, const SoGLContext * glue);
    void deleteWireGpu(CadWireGpu& w, const SoGLContext * glue);
    void deleteTriGpu(CadTriGpu& t, const SoGLContext * glue);
    void deleteProgressiveGpu(
        CadProgressiveGpu& p, const SoGLContext *glue);
    void deleteProgressiveGpu(
        Entry& entry, const SoGLContext *glue);

    // Non-copyable
    CadGpuResources(const CadGpuResources&) = delete;
    CadGpuResources& operator=(const CadGpuResources&) = delete;
};

} // namespace internal
} // namespace Obol

#endif // OBOL_CAD_CADGPURESOURCES_H
