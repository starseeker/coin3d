#ifndef OBOL_CAD_CADRENDERERGL_H
#define OBOL_CAD_CADRENDERERGL_H

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
 * @file CadRendererGL.h
 * @brief VBO + shader-based GL renderer for SoCADAssembly.
 *
 * Provides two rendering tiers selectable at runtime based on detected
 * GL capabilities:
 *
 *  Tier 1 – GL 2.0 VBO-loop (minimum requirement: VBO + GLSL shaders)
 *    Works on: GL 2.0, GL 3.x core profile, OSMesa, any GL2-compatible backend.
 *    Strategy: per-part VBOs, per-instance glUniformMatrix4fv + glDrawElements.
 *    Draw-call count: O(instances × passes).
 *
 *  Tier 2 – GL 3.1+ instanced (requires VAO + glDrawElementsInstanced +
 *                                         glVertexAttribDivisor)
 *    Strategy: per-part VAO, one instance-attribute VBO per frame,
 *              glDrawElementsInstanced – O(unique_parts × passes) draw calls.
 *
 * CadRendererGL is owned by SoCADAssemblyImpl and reused across frames.
 * It detects capabilities on first use and caches GPU resources.
 *
 * Shader details
 * --------------
 * The shaders use GLSL 1.10 syntax (no #version directive) so they compile
 * on both GL 2.0 (compatibility) and GL 3.x (GLSL 1.10 is still accepted by
 * Mesa / NVidia with ARB_shader_objects).  For the Tier-2 instanced path the
 * shaders switch to GLSL 1.40 (#version 140) which requires GL 3.1+.
 *
 * Matrix convention
 * -----------------
 * SbMatrix stores data row-major (Open Inventor post-multiply row-vector
 * convention).  Passing the raw float[16] to glUniformMatrix4fv with
 * transpose=GL_FALSE gives the GL column-major representation of the
 * transpose, which is exactly the column-major matrix needed by the
 * standard GL pre-multiply column-vector convention.  Concretely:
 *   gl_Position = u_viewProj * u_model * vec4(a_pos, 1.0)
 * where u_viewProj and u_model are loaded from OI float[16] with GL_FALSE.
 */

#include "CadGLCaps.h"
#include "CadGpuResources.h"
#include "CadFramePlan.h"

#include <Obol/cad/SoCADAssembly.h>
#include <Obol/cad/CadViewState.h>

#include <Inventor/SbMatrix.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbViewVolume.h>
#include <Inventor/system/gl.h>

#include <memory>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <vector>

struct SoGLContext;
struct CadRendererConfiguration;
class SoGLRenderAction;

namespace Obol {
namespace internal {

/**
 * @brief VBO + shader-based renderer for one SoCADAssembly node.
 */
class CadRendererGL {
public:
    CadRendererGL();
    ~CadRendererGL();

    /**
     * Render the assembly described by @p plan.
     *
     * @param plan       Pre-built frame plan from SoCADAssembly::buildFramePlan().
     * @param assembly   The owning node (for geometry access).
     * @param glue       Active GL dispatch context (from sogl_current_render_glue()).
     * @param viewProj   Combined view-projection matrix (OI row-major, GL_FALSE upload).
     * @param viewMatrix Active model-view matrix before local instance transforms.
     * @param projectionMatrix Active projection matrix.
     * @param viewVolume Camera projection point/direction used to orient
     *                   derivative-generated two-sided normals.
     * @param partGenMap Map from PartId → generation counter (to detect stale VBOs).
     */
    void render(const CadFramePlan& plan,
                const SoCADAssembly& assembly,
                SoGLRenderAction*    action,
                const SoGLContext*   glue,
                const SbMatrix&      viewProj,
                const SbMatrix&      viewMatrix,
                const SbMatrix&      projectionMatrix,
                const SbViewVolume&  viewVolume,
                const std::unordered_map<PartId, uint64_t,
                                         std::hash<PartId>>& partGenMap);

    /**
     * Publish a complete frame written directly to a software framebuffer.
     *
     * The direct rasterizer deliberately bypasses GL execution, but it must
     * not bypass the renderer's completed-frame contract.  Hosts use this
     * record to distinguish a completed presentation from an interrupted or
     * not-yet-visited assembly and to calibrate view-LoD work.
     */
    void completeDirectSoftwareWireFrame(
        const Obol::CadRenderedWork& work, uint32_t contextId);

    /**
     * Release all GPU resources held by this renderer for @p glue.
     * Call while the correct GL context is current (e.g. from SoCADAssembly
     * destructor or context-destruction callback).
     */
    void releaseGpuResources(const SoGLContext * glue);

    /**
     * Returns the rendering tier used during the last render() call:
     *   0 = immediate-mode fallback (GL 1.1, no working GLSL+VBO)
     *   1 = retained VBO loop (fixed-function compatibility or GLSL)
     *   2 = instanced (GL 3.1+)
     *   3 = flattened wire/hidden-line batch
     *   4 = flattened shaded batch
     *   6 = retained paged triangle atlas + indirect command stream
     *  -1 = render() not yet called
     */
    int lastRenderTier() const { return lastRenderTier_; }

    /**
     * Status of the most recent retained indirect-shaded attempt.
     * Zero is success, -1 means no attempt, and a positive value identifies
     * the preflight invariant that rejected the fast path.
     */
    int lastIndirectStatus() const { return lastIndirectStatus_; }

    /** Triangles actually submitted by the last shaded rendering pass. */
    uint64_t lastRenderedTriangleCount() const {
        return lastRenderedTriangleCount_;
    }

    Obol::CadRenderedWork lastRenderedWork() const {
        return lastRenderedWork_;
    }

    /** Most recently completed, asynchronously measured CAD GPU work. */
    uint64_t lastGpuRenderNanoseconds() const {
        return gpuRes_ ? gpuRes_->lastGpuTimeNanoseconds() : 0;
    }
    uint64_t lastGpuRenderedTriangleCount() const {
        return gpuRes_ ? gpuRes_->lastGpuTriangleCount() : 0;
    }
    float lastGpuPointProxyPixelThreshold() const {
        return gpuRes_ ? gpuRes_->lastGpuPointProxyPixelThreshold() : 1.0f;
    }
    uint64_t gpuTimerSampleSerial() const {
        return gpuRes_ ? gpuRes_->gpuTimerSampleSerial() : 0;
    }
    Obol::CadGpuResourceSnapshot gpuResourceSnapshot() const {
        return lastGpuResourceSnapshot_;
    }

    /** True when the last indirect frame reused its prepared visibility,
     * instance, atlas, and command record instead of rebuilding it. */
    bool lastRenderUsedPreparedReplay() const {
        return lastRenderUsedPreparedReplay_;
    }

    /** Monotonic token advanced when a render performs retained-record,
     * resource-upload, or renderer-initialization work which is not part of
     * a steady prepared draw. */
    uint64_t renderPreparationSerial() const {
        return renderPreparationSerial_;
    }

    /** Number of visible occurrences proxied because atlas admission failed. */
    size_t lastPressureProxyCount() const {
        size_t visible = 0;
        for (const CadSubpixelProxyPoint& point : pressureProxyPoints())
            if (!(point.flags & CadInstanceHidden))
                ++visible;
        return visible;
    }

    /// Maximum simultaneous lights the shaded GLSL passes evaluate.
    static const int kMaxLights = 8;

    /// One light for the shaded GLSL passes, in world space.
    struct GlLight {
        int   type = 0;                       ///< 0=directional, 1=point, 2=spot
        float vec[3] = { 0.577f, 0.577f, 0.577f }; ///< dir: toward-light; point/spot: world position
        float axis[3] = { 0.0f, 0.0f, -1.0f };     ///< spot: world travel axis (else unused)
        float color[3] = { 1.0f, 1.0f, 1.0f };     ///< rgb premultiplied by intensity
        float cosCutoff = -2.0f;              ///< spot cutoff cosine (<= -1 disables the cone test)
    };

    /**
     * Set the complete scene-light snapshot used by every shaded renderer.
     * Supplied each frame by SoCADAssembly from the enabled SoLight nodes
     * (directional headlight and any in-scene point/spot/directional sources).
     * Once supplied, an empty list deliberately means ambient-only lighting;
     * the historical default light is used only by standalone clients which
     * have never supplied a scene-light snapshot.
     */
    void setLights(const std::vector<GlLight>& lights) {
        this->lights_ = lights;
        this->lightsSupplied_ = true;
    }
    /** Set environment ambient RGB and intensity for shaded shader passes.
     * Material ambient remains the BRL-CAD default 0.2 in the shader. */
    void setAmbientLight(float red, float green, float blue, float intensity);

private:
    /*
     * Coin's abort callback is normally sampled only between scene nodes.
     * One SoCADAssembly can represent tens of thousands of occurrences, so
     * its retained executors must also expose bounded safe points.  The
     * active action is thread-local: renderers may be used by independent GL
     * contexts on different threads, while nested render actions restore the
     * preceding slot on exit.
     */
    static thread_local SoGLRenderAction *activeRenderAction_;
    /* SoGLRenderAction::abortNow() is an edge-triggered query in some Coin
     * traversal paths: once a retained executor observes ABORT, a later
     * query made while unwinding the same CAD render is not guaranteed to
     * report it again.  Keep the observation sticky for the whole assembly
     * render so fallback executors cannot turn a partially drawn frame into
     * an apparently exact one. */
    mutable bool activeRenderInterrupted_ = false;
    bool renderInterrupted() const;
    bool renderInterruptedAfter(size_t& workCounter,
                                size_t work = 1u) const;
    void noteRenderPreparation(const char *reason);

    bool softwareGlslRequested() const;
    bool cadLightDebugRequested() const;
    const char *cadShaderDebugMode() const;

    /// Upload the current light set to @p program's u_light* uniforms.
    void uploadLights(const SoGLContext* glue, GLuint program);
    /// Upload environment ambient RGB x intensity to a shaded program.
    void uploadAmbientLight(const SoGLContext* glue, GLuint program);
    /** Program compatibility-profile lighting from the same explicit snapshot
     * used by GLSL.  The caller must have loaded the world-to-eye model-view
     * matrix before calling this method. */
    void uploadFixedLights(const SoGLContext* glue);
    /// Upload camera-facing data used only when a mesh has no normal stream.
    void uploadViewFacing(const SoGLContext* glue, GLuint program,
                          const SbViewVolume& viewVolume);

    // Capability flags (populated on first render call)
    bool      capsDetected_ = false;
    std::unique_ptr<::CadRendererConfiguration> configuration_;
    CadGLCaps caps_;
    int       lastRenderTier_ = -1; ///< -1=none, 0=imm, 1=vbo, 2=inst, 3/4=flat, 5=mixed flat-wire
    int       lastIndirectStatus_ = -1;
    uint64_t  lastRenderedTriangleCount_ = 0;
    Obol::CadRenderedWork lastRenderedWork_;
    bool      lastRenderUsedPreparedReplay_ = false;
    uint64_t  renderPreparationSerial_ = 0;
    bool      atlasAdmissionPressure_ = false;
    Obol::CadGpuResourceSnapshot lastGpuResourceSnapshot_;
    uint64_t completedResourceFrameSerial_ = 0;
    int       reportedIndirectStatus_ = -1;
    bool      indirectStatusReported_ = false;
    /*
     * Non-indirect paths may construct an owned pressure-proxy stream.
     * Retained indirect rendering already owns the same immutable stream in
     * indirectPrepared_; pressureProxyPointsView_ lets the rest of this
     * synchronous render consume that storage directly instead of copying
     * O(visible pressure proxies) on every replay.
     */
    std::vector<CadSubpixelProxyPoint> pressureProxyPoints_;
    const std::vector<CadSubpixelProxyPoint> *pressureProxyPointsView_ =
        nullptr;
    const std::vector<CadSubpixelProxyPoint>& pressureProxyPoints() const {
        return pressureProxyPointsView_ ?
            *pressureProxyPointsView_ : pressureProxyPoints_;
    }
    /// Changes only when a newly prepared indirect frame publishes proxies.
    uint64_t pressureProxyRevision_ = 1;
    /*
     * A successful retained append preserves the preceding pressure stream
     * byte-for-byte and adds a tail.  Record that relationship until the
     * proxy pass consumes it, allowing a bounded glBufferSubData instead of
     * repacking and reallocating the complete pressure stream.
     */
    uint64_t pressureProxyAppendBaseRevision_ = 0;
    size_t pressureProxyAppendBegin_ = 0;
    bool pressureProxyAppendOnly_ = false;
    /// Scene lights for the shaded GLSL passes (set per-frame by SoCADAssembly).
    /// Empty means "use the historical fixed directional light" only until a
    /// client has supplied its first explicit (possibly empty) snapshot.
    bool lightsSupplied_ = false;
    std::vector<GlLight> lights_;
    float ambientLight_[3] = {0.3f, 0.3f, 0.3f};

    // GPU objects are namespaced by GL context.  A renderer may be traversed
    // by multiple system-GL or offscreen contexts during its lifetime.
    std::unordered_map<uint32_t, std::unique_ptr<CadGpuResources>> gpuResources_;
    CadGpuResources *gpuRes_ = nullptr;
    uint32_t gpuContextId_ = 0;

    // Compiled shader programs (keyed by context id, lazily compiled)
    struct ShaderPrograms {
        GLuint wire    = 0; ///< Wire-pass shader (no lighting)
        GLuint wirePop = 0; ///< Wire-pass shader with branchless PoP snapping
        GLuint proxyPoint = 0; ///< Batched subpixel-proxy point shader
        GLuint shaded  = 0; ///< Shaded-pass shader (Phong, no instancing)
        GLuint shadedPop = 0; ///< Shaded-pass shader with branchless PoP snapping
        GLuint shadedDirectionalNorm = 0; ///< One-directional-light shader with vertex normals
        GLuint shadedDirectionalFace = 0; ///< One-directional-light shader with derivative normals
        GLuint shadedPopDirectionalNorm = 0; ///< Directional PoP shader with vertex normals
        GLuint shadedPopDirectionalFace = 0; ///< Directional PoP shader with derivative normals
        GLuint wireInst   = 0; ///< Wire-pass shader (instanced)
        GLuint wirePopInst = 0; ///< Wire-pass PoP shader (instanced by level)
        GLuint shadedInst = 0; ///< Shaded-pass shader (instanced Phong)
        GLuint shadedPopInst = 0; ///< Shaded PoP shader (instanced by level)
        GLuint shadedIndirect = 0; ///< Cross-part retained atlas shader
    };
    ShaderPrograms shaders_;
    uint32_t shadersContextId_ = 0;

    static void contextDestroyed(uint32_t contextId, void *closure);
    void releaseContext(uint32_t contextId, const SoGLContext *glue);

    // Ensure capabilities have been detected and shaders compiled
    bool ensureReady(const SoGLContext * glue);

    // Ensure part geometry has been uploaded to GPU
    void ensurePartUploaded(PartId pid, const SoCADAssembly& assembly,
                            uint64_t gen, uint8_t requestedCut,
                            const SoGLContext * glue);

    void renderPoints(const CadFramePlan& plan,
                      const SoCADAssembly& assembly,
                      const SoGLContext* glue,
                      const SbMatrix& viewProj,
                      const std::unordered_map<PartId, uint64_t,
                                               std::hash<PartId>>& partGenMap);

    void renderSubpixelProxyPoints(const CadFramePlan& plan,
                                   const SoGLContext* glue,
                                   const SbMatrix& viewProj);

    static bool wireRepHasUncollapsedInstances(const CadFramePlan& plan,
                                               PartId part);

    // -----------------------------------------------------------------------
    // Tier-1: VBO-loop path (GL 2.0+)
    // -----------------------------------------------------------------------

    void renderVboLoop(const CadFramePlan& plan,
                       const SoCADAssembly& assembly,
                       const SoGLContext*   glue,
                       const SbMatrix&      viewProj,
                       const SbViewVolume&  viewVolume,
                       bool drawWire,
                       bool customWireOnly,
                       bool drawShaded);

    void renderFixedVboLoop(const CadFramePlan& plan,
                            const SoCADAssembly& assembly,
                            const SoGLContext* glue,
                            const SbMatrix& viewProj,
                            const SbMatrix& viewMatrix,
                            const SbMatrix& projectionMatrix,
                            bool drawWire,
                            bool drawShaded);

    /**
     * Tier-0 fallback: fixed-function immediate-mode rendering via
     * glBegin/glEnd/glVertex3f.  Used when the GLSL+VBO path isn't
     * functional (e.g. Mesa 7.x swrast where GLSL doesn't execute during
     * glDrawElements).  Supports wire (GL_LINES) and shaded (GL_TRIANGLES)
     * geometry.
     */
    void renderImmediateMode(const CadFramePlan& plan,
                             const SoCADAssembly& assembly,
                             const SoGLContext*   glue,
                             const SbMatrix&      viewProj,
                             const SbMatrix&      viewMatrix,
                             const SbMatrix&      projectionMatrix,
                             bool drawWire,
                             bool drawShaded);

    void drawWireVboLoop(const CadFramePlan& plan,
                         const SoGLContext* glue,
                         GLint locMVP, GLint locColor);

    void drawShadedVboLoop(const CadFramePlan& plan,
                           const SoGLContext* glue,
                           GLint locMVP, GLint locColor,
                           GLint locLightDir, GLint locHasNorm);

    // -----------------------------------------------------------------------
    // Tier-2: instanced path (GL 3.1+)
    // -----------------------------------------------------------------------

    struct InstVertex {
        float transform[16];  ///< column-major 4×4 (raw OI float[16])
        float color[4];        ///< RGBA [0,1]
        float popMinLevel[4];  ///< quantization minimum xyz + active level
        float popMaxFlags[4];  ///< quantization maximum xyz + packed flags
    };

    struct IndirectPreparedPart {
        PartId part;
        uint32_t partIndex = 0;
        uint64_t generation = 0;
        uint32_t vertexCount = 0;
        uint32_t indexCount = 0;
        uint32_t page = 0;
        uint32_t vertexFirst = 0;
        uint32_t indexFirst = 0;
        bool hasNormals = false;
        bool admissionPressure = false;
        /*
         * A one-occurrence progressive part has one stable command and one
         * packed instance slot.  Recording their locations lets the common
         * many-unique-parts LoD wave update O(changed parts), while shared
         * multi-occurrence groups conservatively fall back to exact rebuild.
         */
        uint32_t packedInstance = std::numeric_limits<uint32_t>::max();
        uint32_t commandIndex = std::numeric_limits<uint32_t>::max();
        bool commandCulled = false;
    };

    struct IndirectPageWork {
        uint32_t page = 0;
        std::vector<CadDrawElementsIndirectCommand> ordinary;
        std::vector<CadDrawElementsIndirectCommand> culled;
    };

    struct IndirectPreparedFrame {
        bool valid = false;
        uint32_t contextId = 0;
        uint64_t planRevision = 0;
        uint64_t geometryRevision = 0;
        uint64_t shadedLayoutRevision = 0;
        uint64_t shadedLodRevision = 0;
        uint64_t appendRevision = 0;
        uint64_t partGeometryRevision = 0;
        uint64_t instanceAttributeRevision = 0;
        uint64_t subpixelProxyRevision = 0;
        int progressiveCutCeiling = -1;
        SbMatrix viewProj;
        std::vector<IndirectPreparedPart> parts;
        std::vector<uint32_t> partByPlanPartIndex;
        std::vector<IndirectPageWork> pages;
        std::vector<InstVertex> instances;
        // Source CadFramePlan index for each packed instance.  Presentation
        // attributes can then be refreshed without rebuilding visibility,
        // atlas demands, or indirect commands.
        std::vector<uint32_t> sourceInstanceIndices;
        // Reverse lookup from a CadFramePlan visible-instance index to its
        // packed indirect instance, or UINT32_MAX when it is not admitted.
        std::vector<uint32_t> instanceIndexBySource;
        std::vector<CadSubpixelProxyPoint> pressureProxyPoints;
        std::vector<uint32_t> pressureProxySourceInstanceIndices;
        std::vector<uint32_t> pressureProxyIndexBySource;
        uint64_t renderedTriangleCount = 0;
        Obol::CadRenderedWork renderedWork;
        uint64_t instanceUploadSerial = 0;
        uint64_t atlasRevision = 0;
        uint32_t atlasValidationCountdown = 0;
        /*
         * Periodic atlas validation is O(retained parts).  It must obey the
         * same resumable-preparation contract as an exact frame build: a
         * large scene may not be able to validate every binding inside one
         * host presentation deadline.  Retain the cursor rather than
         * restarting at part zero after every abort.
         */
        bool atlasValidationActive = false;
        size_t atlasValidationCursor = 0;
        uint64_t atlasValidationRevision = 0;
        uint32_t cameraMotionReplayCount = 0;
        bool atlasAdmissionPressure = false;
        size_t atlasPressurePartCount = 0;
        /*
         * Packed shaded-occurrence count at the last exact preparation.
         * Append replay is deliberately bounded to geometric growth from
         * this anchor.  Crossing the bound requests one exact preparation,
         * giving streaming an amortized O(N) compaction/revalidation instead
         * of allowing an indefinitely patched command stream.
         */
        size_t appendPatchAnchorInstanceCount = 0;
    };

    enum class IndirectPreparationPhase : uint8_t {
        Idle = 0,
        Visibility,
        Protection,
        Coverage,
        Enrichment,
        CommandSetup,
        Commands,
        Preflight,
        PublishSetup,
        PublishParts,
        ReverseInstances,
        ReverseProxies,
        Submit
    };

    /**
     * Atomic exact-indirect preparation transaction.
     *
     * A large CAD assembly can require more than one host presentation
     * deadline to classify its occurrences, protect/admit atlas prefixes,
     * and pack its indirect commands.  The completed replay record cannot be
     * mutated in place while that work is incomplete, and restarting an O(N)
     * build on every deadline guarantees livelock once N crosses the work
     * possible in one frame.  Keep only scalar cursors here; the scene-sized
     * scratch arrays already belong to CadRendererGL and are published by
     * constant-time swaps after every phase has completed.
     */
    struct IndirectPreparationState {
        bool active = false;
        IndirectPreparationPhase phase =
            IndirectPreparationPhase::Idle;
        uint32_t contextId = 0;
        uint64_t planRevision = 0;
        int progressiveCutCeiling = -1;
        SbMatrix viewProj;

        size_t itemCursor = 0;
        uint32_t occurrenceOffset = 0;
        size_t partCursor = 0;
        size_t pageCursor = 0;
        size_t reverseCursor = 0;
        size_t visibleOccurrenceCount = 0;
        uint64_t requestedLiveBytes = 0;

        bool proxyPartActive = false;
        uint32_t proxyPartIndex = 0;
        uint32_t proxyVisibleIndex =
            std::numeric_limits<uint32_t>::max();

        bool commandItemActive = false;
        uint32_t commandBaseInstance = 0;
        uint8_t commandCut = Obol::ProgressiveCutUnspecified;
        size_t commandCount = 0;
        uint64_t renderedTriangleCount = 0;
        Obol::CadRenderedWork renderedWork;
        bool atlasAdmissionPressure = false;
        uint32_t sliceCount = 0;
    };

    /*
     * Indirect rendering is a per-frame operation, but its largest CPU
     * arrays are only scratch.  Retain their capacity with the renderer
     * instead of allocating and freeing O(instances + parts) storage on the
     * GUI thread for every paint.
     */
    std::vector<uint8_t> indirectVisibleMask_;
    std::vector<uint8_t> indirectVisibleMaximumCut_;
    std::vector<uint8_t> indirectVisiblePart_;
    std::vector<uint32_t> indirectVisiblePartIndices_;
    std::vector<double> indirectVisibleImportance_;
    std::vector<uint32_t> indirectAdmissionPartIndices_;
    std::vector<uint32_t> indirectFirstVisibleOccurrence_;
    std::vector<uint32_t> indirectNextVisibleOccurrence_;
    std::vector<uint32_t> indirectRequestedVertexCounts_;
    std::vector<uint32_t> indirectRequestedIndexCounts_;
    std::vector<const CadTriangleAtlasPart *> indirectAtlasBindings_;
    std::vector<CadSubpixelProxyPoint> indirectPressureProxyPoints_;
    std::vector<uint32_t> indirectPressureProxySourceInstanceIndices_;
    std::vector<InstVertex> indirectInstances_;
    std::vector<uint32_t> indirectSourceInstanceIndices_;
    /*
     * Exact preparation and replay use alternating page-command stores.
     * Recycle the preceding prepared vectors instead of allocating and
     * freeing tens of thousands of commands on every progressive cut.
     */
    std::vector<IndirectPageWork> indirectPageWorkScratch_;
    std::vector<uint32_t> indirectPageWorkSlotByPage_;
    std::vector<uint32_t> indirectCommandIndexByPart_;
    std::vector<uint8_t> indirectCommandCullByPart_;
    std::vector<uint32_t> indirectPackedInstanceByPart_;
    IndirectPreparedFrame indirectPrepared_;
    IndirectPreparationState indirectPreparation_;

    bool renderIndirectShaded(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const SoGLContext *glue,
        const SbMatrix& viewProj,
        const SbViewVolume& viewVolume);
    bool replayIndirectShaded(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const SoGLContext *glue,
        const SbMatrix& viewProj,
        const SbViewVolume& viewVolume);
    bool patchIndirectPreparedCuts(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const SoGLContext *glue);
    bool patchIndirectPreparedAppend(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const SoGLContext *glue,
        const SbMatrix& viewProj);
    bool patchIndirectPreparedGeometry(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const SoGLContext *glue);
    bool patchIndirectPreparedCeiling(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const SoGLContext *glue);
    bool submitIndirectPrepared(
        const SoGLContext *glue,
        const SbMatrix& viewProj,
        const SbViewVolume& viewVolume);
    bool rejectIndirect(int status, const char *reason);

    void renderInstanced(const CadFramePlan& plan,
                         const SoCADAssembly& assembly,
                         const SoGLContext*   glue,
                         const SbMatrix&      viewProj,
                         const SbViewVolume&  viewVolume,
                         const std::unordered_map<PartId, uint64_t,
                                                   std::hash<PartId>>& partGenMap,
                         bool drawWire,
                         bool solidWireOnly,
                         bool drawShaded);

    bool renderFlatWire(const CadFramePlan& plan,
                        const SoCADAssembly& assembly,
                        const SoGLContext* glue,
                        const SbMatrix& viewProj);

    bool renderFlatShaded(const CadFramePlan& plan,
                          const SoCADAssembly& assembly,
                          const SoGLContext* glue,
                          const SbMatrix& viewProj,
                          const SbMatrix& viewMatrix,
                          const SbMatrix& projectionMatrix,
                          bool depthOnly);

    bool renderFlatTriangleEdges(const CadFramePlan& plan,
                                 const SoGLContext* glue,
                                 const SbMatrix& viewProj,
                                 const SbMatrix& viewMatrix,
                                 const SbMatrix& projectionMatrix);

    /** Draw zero-copy WireRep triangle-edge aliases from the retained indexed
     * triangle buffers.  Returns false only when such work exists but could
     * not be rendered by the active backend. */
    bool renderIndexedTriangleWire(const CadFramePlan& plan,
                                   const SoCADAssembly& assembly,
                                   const SoGLContext* glue,
                                   const SbMatrix& viewProj,
                                   const SbMatrix& viewMatrix,
                                   const SbMatrix& projectionMatrix);

    // -----------------------------------------------------------------------
    // Shader compilation helpers
    // -----------------------------------------------------------------------

    GLuint compileShader(const SoGLContext* glue, GLenum type, const char* src);
    GLuint linkProgram(const SoGLContext* glue, GLuint vs, GLuint fs);
    bool   compileAllShaders(const SoGLContext* glue);

    // Non-copyable
    CadRendererGL(const CadRendererGL&) = delete;
    CadRendererGL& operator=(const CadRendererGL&) = delete;
};

} // namespace internal
} // namespace Obol

#endif // OBOL_CAD_CADRENDERERGL_H
