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

#include <obol/cad/SoCADAssembly.h>

#include <Inventor/SbMatrix.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/system/gl.h>

#include <memory>
#include <cstdint>
#include <unordered_map>
#include <vector>

struct SoGLContext;
class SoGLRenderAction;

namespace obol {
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
     * @param cameraPos  Camera (eye) position in world space (for LoD distance).
     * @param lodEnabled Apply POP LoD level selection when drawing triangles.
     * @param partGenMap Map from PartId → generation counter (to detect stale VBOs).
     */
    void render(const CadFramePlan& plan,
                const SoCADAssembly& assembly,
                const SoGLContext*   glue,
                const SbMatrix&      viewProj,
                const SbVec3f&       cameraPos,
                bool                 lodEnabled,
                const std::unordered_map<PartId, uint64_t,
                                         std::hash<PartId>>& partGenMap);

    /**
     * Release all GPU resources held by this renderer for @p glue.
     * Call while the correct GL context is current (e.g. from SoCADAssembly
     * destructor or context-destruction callback).
     */
    void releaseGpuResources(const SoGLContext * glue);

    /**
     * Returns the rendering tier used during the last render() call:
     *   0 = immediate-mode fallback (GL 1.1, no working GLSL+VBO)
     *   1 = VBO-loop (GL 2.0, GLSL 1.10)
     *   2 = instanced (GL 3.1+)
     *  -1 = render() not yet called
     */
    int lastRenderTier() const { return lastRenderTier_; }

private:
    // Capability flags (populated on first render call)
    bool      capsDetected_ = false;
    CadGLCaps caps_;
    int       lastRenderTier_ = -1; ///< last tier used: -1=none, 0=imm, 1=vbo, 2=inst

    // Per-context GPU resource cache
    CadGpuResources gpuRes_;

    // CPU-side LoD index cache.  Keyed by (PartId, level); invalidated when
    // the part's generation counter changes.
    struct LodCacheEntry {
        uint64_t generation = UINT64_MAX;
        std::unordered_map<uint8_t, std::vector<uint32_t>> byLevel;
    };
    std::unordered_map<PartId, LodCacheEntry, std::hash<PartId>> lodCache_;

    // Compiled shader programs (keyed by context id, lazily compiled)
    struct ShaderPrograms {
        GLuint wire    = 0; ///< Wire-pass shader (no lighting)
        GLuint shaded  = 0; ///< Shaded-pass shader (Phong, no instancing)
        GLuint wireInst   = 0; ///< Wire-pass shader (instanced)
        GLuint shadedInst = 0; ///< Shaded-pass shader (instanced Phong)
    };
    ShaderPrograms shaders_;
    uint32_t shadersContextId_ = 0;

    // Ensure capabilities have been detected and shaders compiled
    bool ensureReady(const SoGLContext * glue);

    // Ensure part geometry has been uploaded to GPU
    void ensurePartUploaded(PartId pid, const SoCADAssembly& assembly,
                            uint64_t gen, const SoGLContext * glue);

    // Return cached LoD-filtered triangle indices for (pid, level, gen).
    // Builds the filtered list on first access and reuses it thereafter.
    // Returns nullptr if the assembly has no LoD structure for this part.
    const std::vector<uint32_t>* getLodCachedIndices(
        PartId pid, uint8_t level, uint64_t gen,
        const SoCADAssembly& assembly);

    // -----------------------------------------------------------------------
    // Tier-1: VBO-loop path (GL 2.0+)
    // -----------------------------------------------------------------------

    void renderVboLoop(const CadFramePlan& plan,
                       const SoCADAssembly& assembly,
                       const SoGLContext*   glue,
                       const SbMatrix&      viewProj,
                       const SbVec3f&       cameraPos,
                       bool                 lodEnabled,
                       const std::unordered_map<PartId, uint64_t,
                                                std::hash<PartId>>& partGenMap);

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
                             const SbVec3f&       cameraPos,
                             bool                 lodEnabled,
                             const std::unordered_map<PartId, uint64_t,
                                                       std::hash<PartId>>& partGenMap);

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
    };

    void renderInstanced(const CadFramePlan& plan,
                         const SoCADAssembly& assembly,
                         const SoGLContext*   glue,
                         const SbMatrix&      viewProj,
                         const std::unordered_map<PartId, uint64_t,
                                                   std::hash<PartId>>& partGenMap);

    // -----------------------------------------------------------------------
    // Shader compilation helpers
    // -----------------------------------------------------------------------

    GLuint compileShader(const SoGLContext* glue, GLenum type, const char* src);
    GLuint linkProgram(const SoGLContext* glue, GLuint vs, GLuint fs);
    bool   compileAllShaders(const SoGLContext* glue);
    void   deleteShaders(const SoGLContext* glue);

    // Non-copyable
    CadRendererGL(const CadRendererGL&) = delete;
    CadRendererGL& operator=(const CadRendererGL&) = delete;
};

} // namespace internal
} // namespace obol

#endif // OBOL_CAD_CADRENDERERGL_H
