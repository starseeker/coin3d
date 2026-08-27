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
#include <Obol/cad/CadGpuResourceSnapshot.h>
#include <Obol/cad/CadProgressive.h>
#include "CadGLCaps.h"

#include <Inventor/system/gl.h>

#include <unordered_map>
#include <vector>
#include <array>
#include <cstdint>
#include <memory>

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
    struct PackedRange {
        uint32_t sourceFirst = 0;
        uint32_t sourceCount = 0;
        uint32_t packedFirst = 0;
    };

    GLuint posBuf = 0;
    GLuint normBuf = 0;
    GLsizei vertexCount = 0;
    bool indexed = false;
    uint64_t rangeSignature = 0;
    std::vector<PackedRange> packedRanges;
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
    std::vector<GLint> firsts;
    std::vector<GLsizei> counts;
    float lineWidth = 1.0f;
    uint16_t linePattern = 0xffffu;
    uint16_t linePatternFactor = 1u;
    uint8_t rgba[4] = {204, 204, 204, 255};
};

struct CadFlatWireRangeKey {
    InstanceId instance;
    uint8_t cut = Obol::ProgressiveCutUnspecified;
    uint64_t geometryToken = 0;

    bool operator==(const CadFlatWireRangeKey& other) const noexcept {
        return instance == other.instance && cut == other.cut &&
               geometryToken == other.geometryToken;
    }
};

struct CadFlatWireRangeKeyHash {
    size_t operator()(const CadFlatWireRangeKey& key) const noexcept {
        size_t value = std::hash<InstanceId>()(key.instance);
        value ^= static_cast<size_t>(key.cut) +
                 static_cast<size_t>(0x9e3779b9u) +
                 (value << 6) + (value >> 2);
        value ^= static_cast<size_t>(key.geometryToken) +
                 static_cast<size_t>(0x9e3779b9u) +
                 (value << 6) + (value >> 2);
        return value;
    }
};

struct CadFlatWireRange {
    GLint first = 0;
    GLsizei count = 0;
};

struct CadFlatWireGpu {
    using RangeMap = std::unordered_map<
        CadFlatWireRangeKey, CadFlatWireRange, CadFlatWireRangeKeyHash>;

    GLuint posBuf = 0;
    GLuint vao = 0;
    uint64_t planRevision = 0;
    uint64_t geometryRevision = 0;
    GLsizei vertexCount = 0;
    GLsizei capacityVertexCount = 0;
    std::vector<CadFlatWireGroup> groups;
    RangeMap ranges;
    /*
     * Frame-plan occurrence slots are append-stable.  Point directly at the
     * corresponding unordered-map node so normal streaming frames avoid
     * hashing a 128-bit InstanceId for every retained fallback range.
     * unordered_map insert/rehash preserves node references; a full atlas
     * replacement clears these pointers before replacing the map.
     */
    std::vector<const RangeMap::value_type *> rangeSlots;
};

struct CadFlatShadedGroup {
    GLint first = 0;
    GLsizei count = 0;
    /* Progressive occurrences select independent prefix ranges in one
     * retained world-space atlas.  Adjacent ranges are coalesced, and the
     * renderer submits the remainder with glMultiDrawArrays. */
    std::vector<GLint> firsts;
    std::vector<GLsizei> counts;
    uint8_t rgba[4] = {204, 204, 204, 255};
    bool cullBackfaces = false;
    /* Closed, consistently oriented geometry does not need Mesa's expensive
     * two-sided fixed-function vertex-lighting path even while a non-exact
     * PoP cut temporarily disables back-face culling.  Open and unoriented
     * surfaces retain two-sided lighting. */
    bool twoSidedLighting = true;
};

struct CadFlatShadedRangeKey {
    InstanceId instance;
    uint8_t cut = Obol::ProgressiveCutUnspecified;
    /* Identifies the certified progressive lineage (or ordinary part
     * generation) and occurrence transform used to bake this world-space
     * range.  A later immutable generation in the same append-only lineage
     * preserves an existing cut byte-for-byte.  Presentation/style revisions
     * deliberately do not participate, so selection can reuse the vertices. */
    uint64_t geometryToken = 0;

    bool operator==(const CadFlatShadedRangeKey& other) const noexcept {
        return instance == other.instance && cut == other.cut &&
               geometryToken == other.geometryToken;
    }
};

struct CadFlatShadedRangeKeyHash {
    size_t operator()(const CadFlatShadedRangeKey& key) const noexcept {
        size_t value = std::hash<InstanceId>()(key.instance);
        value ^= static_cast<size_t>(key.cut) +
                 static_cast<size_t>(0x9e3779b9u) +
                 (value << 6) + (value >> 2);
        value ^= static_cast<size_t>(key.geometryToken) +
                 static_cast<size_t>(0x9e3779b9u) +
                 (value << 6) + (value >> 2);
        return value;
    }
};

struct CadFlatShadedRange {
    GLint first = 0;
    GLsizei count = 0;
};

struct CadFlatShadedGpu {
    GLuint posBuf = 0;
    GLuint normBuf = 0;
    GLuint vao = 0;
    uint64_t planRevision = 0;
    uint64_t geometryRevision = 0;
    GLsizei vertexCount = 0;
    GLsizei capacityVertexCount = 0;
    std::vector<CadFlatShadedGroup> groups;
    std::unordered_map<CadFlatShadedRangeKey, CadFlatShadedRange,
                       CadFlatShadedRangeKeyHash> ranges;
};

/** Dynamic world-space vertex/color buffers for one frame's proxy points. */
struct CadSubpixelProxyGpu {
    GLuint posBuf = 0;
    GLuint colorBuf = 0;
    GLuint vao = 0;
    uint64_t revision = 0;
    GLsizei count = 0;
    GLsizei capacityCount = 0;
};

/**
 * OpenGL's DrawElementsIndirectCommand binary layout.  Keep this definition
 * independent of newer system GL headers: Obol also builds against older
 * OSMesa headers, while a modern system context may still expose the entry
 * point at run time.
 */
struct CadDrawElementsIndirectCommand {
    uint32_t count = 0;
    uint32_t instanceCount = 0;
    uint32_t firstIndex = 0;
    int32_t baseVertex = 0;
    uint32_t baseInstance = 0;
};

/** One allocation within a paged triangle atlas, measured in elements. */
struct CadAtlasRange {
    uint32_t first = 0;
    uint32_t capacity = 0;

    bool empty() const noexcept { return capacity == 0; }
};

/**
 * Stable atlas location for a part.  indexCount/vertexCount are the richest
 * currently resident cumulative PoP prefix; commands may select any smaller
 * producer-authored prefix without changing this record or its buffers.
 */
struct CadTriangleAtlasPart {
    uint32_t page = UINT32_MAX;
    CadAtlasRange vertices;
    CadAtlasRange indices;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    uint32_t requestedVertexCount = 0;
    uint32_t requestedIndexCount = 0;
    uint64_t generation = UINT64_MAX;
    uint64_t lastUsedFrame = 0;
    uint64_t lowerDemandSinceFrame = 0;
    uint64_t progressiveLineage = 0;
    bool exactPreparationProtected = false;
    bool hasNormals = false;
    bool progressive = false;
};

/**
 * One independently reclaimable atlas page.  A page owns object-space
 * position/normal/index storage and an indirect command buffer.  Multiple
 * pages cost one MDI submission each, but avoid the latency and temporary
 * double memory of repeatedly reallocating a monolithic scene buffer.
 */
struct CadTriangleAtlasPage {
    GLuint posBuf = 0;
    GLuint normBuf = 0;
    GLuint idxBuf = 0;
    GLuint indirectBuf = 0;
    GLuint vao = 0;
    GLuint instanceVbo = 0;
    uint32_t vertexCapacity = 0;
    uint32_t indexCapacity = 0;
    uint32_t indirectCapacity = 0;
    std::vector<CadAtlasRange> freeVertices;
    std::vector<CadAtlasRange> freeIndices;
    /*
     * Exact maxima for the ordered free lists.  Page selection is on the
     * render/owner thread and may run once per visible PoP part; consulting
     * these values avoids rescanning every fragmented range on every
     * admission while allocation itself remains best-fit.
     */
    uint32_t largestFreeVertexCapacity = 0;
    uint32_t largestFreeIndexCapacity = 0;
    size_t partCount = 0;
    bool dedicated = false;
    /*
     * Pages containing only flat-shaded/source-normal-less meshes do not
     * need a second vertex-sized buffer.  Attribute 1 may alias posBuf
     * because the indirect shader ignores it for those instances.
     */
    bool storesNormals = false;

    size_t allocatedBytes() const noexcept {
        return static_cast<size_t>(vertexCapacity) *
                   (storesNormals ? 2u : 1u) * 3u * sizeof(float) +
               static_cast<size_t>(indexCapacity) * sizeof(uint32_t) +
               static_cast<size_t>(indirectCapacity) *
                   sizeof(CadDrawElementsIndirectCommand);
    }
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
    CadGpuResources();
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
     * @param wireProgressive Whether wireData is an append-only progressive
     *                        stream.
     * @param wireProgressiveLineage Producer certificate for the wire
     *                               stream, or zero for replacement.
     * @param triangleProgressive Whether triPos/triIdx are an append-only
     *                            progressive stream.
     * @param triangleProgressiveLineage Producer certificate for the
     *                                   triangle stream, or zero.
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
                bool            wireProgressive,
                uint64_t        wireProgressiveLineage,
                bool            triangleProgressive,
                uint64_t        triangleProgressiveLineage,
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

    /**
     * Return true when an uploaded progressive prefix may be retained for a
     * newly published immutable generation.
     *
     * A non-zero lineage is the producer's certificate that the new CPU
     * arrays begin with exactly the bytes already resident on the GPU.  A
     * zero lineage is deliberately conservative and must never be used to
     * extend CPU array counts to match an older GPU allocation.
     */
    bool hasCompatibleProgressiveWirePrefix(
        PartId pid, uint64_t progressiveLineage) const;
    bool hasCompatibleProgressiveTrianglePrefix(
        PartId pid, uint64_t progressiveLineage) const;

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
        PartId pid, bool shaded, uint8_t cut,
        uint64_t rangeSignature = 0);
    const CadProgressiveGpu* progressiveForAny(
        PartId pid, bool shaded, uint8_t cut);

    /** Upload one fixed-function PoP cut for reuse across frames/instances. */
    void uploadProgressive(
        PartId pid, bool shaded, uint8_t cut,
        const std::vector<float>& positions,
        const std::vector<float>& normals,
        bool indexed, uint64_t rangeSignature,
        const std::vector<CadProgressiveGpu::PackedRange>& packedRanges,
        const SoGLContext *glue);

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
    bool updateInstanceData(GLintptr byteOffset, const void* data,
                            GLsizeiptr byteSize,
                            const SoGLContext * glue);
    bool appendInstanceData(GLintptr expectedByteOffset, const void* data,
                            GLsizeiptr byteSize,
                            const SoGLContext *glue);

    /** Return the per-instance VBO name, or 0 if not uploaded. */
    GLuint instanceVbo() const { return instanceVbo_; }
    uint64_t instanceUploadSerial() const {
        return instanceUploadSerial_;
    }

    /**
     * Upload the transient per-instance stream used by the ordinary
     * instanced wire/box path.
     *
     * Retained indirect rendering owns instanceVbo().  Keeping this stream
     * separate is required: wire and fallback-box rendering happens after
     * the shaded MDI pass and must not overwrite the packed ordering retained
     * for a later replay.
     */
    void uploadTransientInstanceData(
        const void *data, GLsizeiptr byteSize,
        const SoGLContext *glue);
    GLuint transientInstanceVbo() const {
        return transientInstanceVbo_;
    }

    void uploadFlatWire(uint64_t planRevision,
                        uint64_t geometryRevision,
                        const std::vector<float>& positions,
                        const std::vector<CadFlatWireGroup>& groups,
                        const std::unordered_map<
                            CadFlatWireRangeKey,
                            CadFlatWireRange,
                            CadFlatWireRangeKeyHash>& ranges,
                        GLsizei capacityVertexCount,
                        const SoGLContext *glue,
                        const CadGLCaps& caps);

    bool appendFlatWire(
        const std::vector<float>& positions,
        const std::unordered_map<
            CadFlatWireRangeKey,
            CadFlatWireRange,
            CadFlatWireRangeKeyHash>& ranges,
        const SoGLContext *glue);

    void updateFlatWireGroups(uint64_t planRevision,
                              const std::vector<CadFlatWireGroup>& groups);

    bool lookupFlatWireRange(
        size_t visibleInstanceIndex,
        const CadFlatWireRangeKey& key,
        CadFlatWireRange *range);

    const CadFlatWireGpu& flatWire() const { return flatWire_; }

    bool uploadFlatShaded(uint64_t planRevision,
                          uint64_t geometryRevision,
                          const std::vector<float>& positions,
                          const std::vector<float>& normals,
                          const std::vector<CadFlatShadedGroup>& groups,
                          const std::unordered_map<
                              CadFlatShadedRangeKey,
                              CadFlatShadedRange,
                              CadFlatShadedRangeKeyHash>& ranges,
                          GLsizei capacityVertexCount,
                          const SoGLContext *glue,
                          const CadGLCaps& caps);

    bool appendFlatShaded(
        const std::vector<float>& positions,
        const std::vector<float>& normals,
        const std::unordered_map<
            CadFlatShadedRangeKey,
            CadFlatShadedRange,
            CadFlatShadedRangeKeyHash>& ranges,
        const SoGLContext *glue);

    void updateFlatShadedGroups(
        uint64_t planRevision,
        const std::vector<CadFlatShadedGroup>& groups);

    const CadFlatShadedGpu& flatShaded() const { return flatShaded_; }

    void uploadSubpixelProxyPoints(uint64_t revision,
                                   const std::vector<float>& positions,
                                   const std::vector<uint8_t>& colors,
                                   const SoGLContext *glue,
                                   const CadGLCaps& caps);

    void uploadPressureProxyPoints(
        uint64_t revision,
        const std::vector<float>& positions,
        const std::vector<uint8_t>& colors,
        const SoGLContext *glue,
        const CadGLCaps& caps);

    bool appendPressureProxyPoints(
        uint64_t expectedRevision,
        uint64_t revision,
        const std::vector<float>& positions,
        const std::vector<uint8_t>& colors,
        const SoGLContext *glue);

    const CadSubpixelProxyGpu& subpixelProxyPoints() const
    {
        return subpixelProxyPoints_;
    }
    const CadSubpixelProxyGpu& pressureProxyPoints() const
    {
        return pressureProxyPoints_;
    }

    /**
     * Start/end reference accounting for the retained triangle atlas.
     * Parts upserted between these calls are active in the current frame.
     * End-of-frame maintenance applies delayed tail shrinking and inexpensive
     * empty-page release; pressure reclamation happens synchronously during
     * allocation so the configured byte ceiling is never exceeded.
     */
    void beginTriangleAtlasFrame();
    void endTriangleAtlasFrame(const SoGLContext *glue);

    /**
     * Protect only the resident parts claimed by a resumable exact renderer
     * transaction.  Protection persists across presentation-frame slices,
     * while unrelated stale parts remain eligible for synchronous pressure
     * reclamation.  One CadGpuResources instance has one owner-thread exact
     * transaction at a time.
     */
    void beginTriangleAtlasExactPreparation();
    void protectTriangleAtlasExactPart(PartId pid);
    void endTriangleAtlasExactPreparation() noexcept;

    /**
     * Skip the O(resident parts) maintenance scan for an exactly replayed
     * prepared frame.  The renderer periodically leaves maintenance enabled,
     * and any atlas mutation changes triangleAtlasRevision() so cached
     * bindings are validated before they are used again.
     */
    void deferTriangleAtlasMaintenance() noexcept {
        triangleAtlasMaintenanceDeferred_ = true;
    }
    /**
     * Preserve the already prepared atlas working set while an append-only
     * publication admits new parts.  Allocation may use free capacity or add
     * pages, but fails rather than treating unscanned prepared parts as
     * inactive eviction candidates.  The caller can then use the exact
     * protection path under genuine memory pressure.
     */
    void deferTriangleAtlasReclamation() noexcept {
        triangleAtlasReclamationDeferred_ = true;
        triangleAtlasMaintenanceDeferred_ = true;
    }
    uint64_t triangleAtlasRevision() const noexcept {
        return triangleAtlasRevision_;
    }

    /**
     * Ensure one richest-needed object-space triangle prefix is resident.
     *
     * The source arrays contain at least vertexCount/indexCount entries.
     * Progressive growth appends tails in place when reserved capacity
     * permits.  Relocation uploads the current prefix once, returns the old
     * ranges to their page, and never duplicates instance transforms.
     */
    const CadTriangleAtlasPart *upsertTriangleAtlasPart(
        PartId pid, uint64_t generation,
        const float *positions, const float *normals, uint32_t vertexCount,
        const uint32_t *indices, uint32_t indexCount, bool progressive,
        uint64_t progressiveLineage,
        const SoGLContext *glue, const CadGLCaps& caps);

    const CadTriangleAtlasPart *triangleAtlasPart(PartId pid) const;
    const CadTriangleAtlasPage *triangleAtlasPage(uint32_t page) const;
    CadTriangleAtlasPage *triangleAtlasPage(uint32_t page);

    /**
     * Mark a retained part as required by this frame before any allocation.
     * Returns the binding when its generation and retained prefix already
     * satisfy the request.  This lets the renderer avoid a second hash-table
     * lookup/upsert for the common unchanged frame while preserving the
     * two-phase protection against pressure reclamation.
     */
    const CadTriangleAtlasPart *touchTriangleAtlasPart(
        PartId pid, uint64_t generation, bool hasNormals,
        uint32_t vertexCount, uint32_t indexCount);

    /**
     * Upload one bounded span of a page's compact frame command stream.
     *
     * Callers split larger streams at indirectCapacity.  Keeping command
     * storage page-local and fixed-size prevents a scene containing many
     * tiny parts from consuming the geometry budget merely to grow a
     * transient command buffer.
     */
    bool uploadTriangleAtlasCommands(
        uint32_t page,
        const CadDrawElementsIndirectCommand *commands,
        size_t commandCount,
        const SoGLContext *glue);

    /** Drop superseded shaded caches after the atlas becomes authoritative. */
    void releaseFlatShaded(const SoGLContext *glue);
    void releaseStandaloneTriangles(const SoGLContext *glue);

    size_t triangleAtlasAllocatedBytes() const noexcept {
        return triangleAtlasAllocatedBytes_;
    }
    size_t triangleAtlasBudgetBytes() const noexcept {
        return triangleAtlasBudgetBytes_;
    }
    size_t triangleAtlasLiveBytes() const noexcept;
    size_t triangleAtlasPartCount() const noexcept {
        return triangleAtlasParts_.size();
    }
    size_t triangleAtlasPageCount() const noexcept;

    /** Constant-time snapshot of all renderer-owned buffer allocations. */
    Obol::CadGpuResourceSnapshot resourceSnapshot() const noexcept;

    /**
     * Begin/end one asynchronous GPU frame-duration sample.
     *
     * beginFrameGpuTimer() only polls already-completed query objects and
     * never waits for the GPU.  It returns false when timer queries are not
     * supported or all bounded query slots are still in flight.
     */
    bool beginFrameGpuTimer(const SoGLContext *glue);
    void endFrameGpuTimer(uint64_t triangleCount,
                          float pointProxyPixelThreshold,
                          const SoGLContext *glue);
    uint64_t lastGpuTimeNanoseconds() const noexcept {
        return gpuTimerLastNanoseconds_;
    }
    uint64_t lastGpuTriangleCount() const noexcept {
        return gpuTimerLastTriangleCount_;
    }
    float lastGpuPointProxyPixelThreshold() const noexcept {
        return gpuTimerLastPointProxyPixelThreshold_;
    }
    uint64_t gpuTimerSampleSerial() const noexcept {
        return gpuTimerLastCompletedSubmission_;
    }

    /** Release all GL resources (call with the correct GL context active). */
    void releaseAll(const SoGLContext * glue);

private:
    struct Entry {
        uint64_t    generation = 0;
        uint64_t    wireProgressiveLineage = 0;
        uint64_t    triangleProgressiveLineage = 0;
        CadPointGpu point;
        CadWireGpu  wire;
        CadTriGpu   tri;
        std::vector<CadProgressiveGpu> progressiveWire;
        std::vector<CadProgressiveGpu> progressiveTri;
    };

    std::unordered_map<PartId, Entry, std::hash<PartId>> cache_;
    GLuint instanceVbo_ = 0;
    GLsizeiptr instanceVboBytes_ = 0;
    GLsizeiptr instanceVboCapacityBytes_ = 0;
    uint64_t instanceUploadSerial_ = 0;
    GLuint transientInstanceVbo_ = 0;
    GLsizeiptr transientInstanceVboCapacityBytes_ = 0;
    CadFlatWireGpu flatWire_;
    CadFlatShadedGpu flatShaded_;
    CadSubpixelProxyGpu subpixelProxyPoints_;
    CadSubpixelProxyGpu pressureProxyPoints_;
    uint64_t progressiveFrame_ = 0;
    size_t progressiveBytes_ = 0;
    size_t progressiveActiveBytes_ = 0;
    size_t progressiveReserveBudgetBytes_ = 0;
    uint64_t progressiveEvictionCount_ = 0;
    size_t ordinaryPartBufferBytes_ = 0;
    uint64_t ordinaryPartFullUploadBytes_ = 0;
    uint64_t ordinaryPartSuffixUploadBytes_ = 0;
    uint64_t ordinaryPartGpuCopyBytes_ = 0;
    uint64_t ordinaryPartLineageReuseCount_ = 0;
    uint64_t ordinaryPartLineageReplacementCount_ = 0;
    std::unordered_map<PartId, CadTriangleAtlasPart, std::hash<PartId>>
        triangleAtlasParts_;
    std::vector<std::unique_ptr<CadTriangleAtlasPage>>
        triangleAtlasPages_;
    uint64_t triangleAtlasFrame_ = 0;
    uint64_t triangleAtlasInactiveSweepFrame_ = 0;
    uint64_t triangleAtlasRevision_ = 1;
    bool triangleAtlasMaintenanceDeferred_ = false;
    bool triangleAtlasReclamationDeferred_ = false;
    bool triangleAtlasExactPreparationActive_ = false;
    uint64_t triangleAtlasCompactionFrame_ = 0;
    size_t triangleAtlasAllocatedBytes_ = 0;
    size_t triangleAtlasLiveBytes_ = 0;
    size_t triangleAtlasPageCount_ = 0;
    size_t triangleAtlasBudgetBytes_ = 0;
    uint64_t triangleAtlasReclamationCount_ = 0;
    uint64_t triangleAtlasFullUploadBytes_ = 0;
    uint64_t triangleAtlasSuffixUploadBytes_ = 0;
    uint64_t triangleAtlasLineageReuseCount_ = 0;

    struct GpuTimerSlot {
        GLuint query = 0;
        bool pending = false;
        uint64_t triangleCount = 0;
        float pointProxyPixelThreshold = 1.0f;
        uint64_t submission = 0;
    };
    std::array<GpuTimerSlot, 3> gpuTimerSlots_;
    bool gpuTimerSupportKnown_ = false;
    bool gpuTimerSupported_ = false;
    int gpuTimerActiveSlot_ = -1;
    size_t gpuTimerNextSlot_ = 0;
    uint64_t gpuTimerNextSubmission_ = 1;
    uint64_t gpuTimerLastCompletedSubmission_ = 0;
    uint64_t gpuTimerLastNanoseconds_ = 0;
    uint64_t gpuTimerLastTriangleCount_ = 0;
    float gpuTimerLastPointProxyPixelThreshold_ = 1.0f;

    void deletePointGpu(CadPointGpu& p, const SoGLContext * glue);
    void deleteWireGpu(CadWireGpu& w, const SoGLContext * glue);
    void deleteTriGpu(CadTriGpu& t, const SoGLContext * glue);
    void deleteProgressiveGpu(
        CadProgressiveGpu& p, const SoGLContext *glue);
    void deleteProgressiveWireGpu(
        Entry& entry, const SoGLContext *glue);
    void deleteProgressiveTriangleGpu(
        Entry& entry, const SoGLContext *glue);
    void deleteProgressiveGpu(
        Entry& entry, const SoGLContext *glue);
    void deleteTriangleAtlasPage(
        uint32_t page, const SoGLContext *glue);
    void releaseTriangleAtlasPart(
        PartId pid, const SoGLContext *glue);
    bool compactTriangleAtlasPages(const SoGLContext *glue);
    void bumpTriangleAtlasRevision() noexcept;
    static size_t pointAllocatedBytes(const CadPointGpu& point) noexcept;
    static size_t wireAllocatedBytes(const CadWireGpu& wire) noexcept;
    static size_t triAllocatedBytes(const CadTriGpu& tri) noexcept;
    static size_t ordinaryEntryAllocatedBytes(const Entry& entry) noexcept;
    static size_t triangleAtlasPartLiveBytes(
        const CadTriangleAtlasPart& part,
        const CadTriangleAtlasPage *page) noexcept;

    // Non-copyable
    CadGpuResources(const CadGpuResources&) = delete;
    CadGpuResources& operator=(const CadGpuResources&) = delete;
};

} // namespace internal
} // namespace Obol

#endif // OBOL_CAD_CADGPURESOURCES_H
