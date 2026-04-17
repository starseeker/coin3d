#ifndef OBOL_CAD_CADGLCAPS_H
#define OBOL_CAD_CADGLCAPS_H

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
 * @file CadGLCaps.h
 * @brief Per-context GL capability flags for the CAD renderer.
 *
 * CadGLCaps is detected once per context (on first render) and cached
 * inside CadRendererGL.  It drives selection of the rendering tier:
 *
 *  Tier 1 (GL 2.0 baseline)
 *    Requirements: hasVBO + hasShaderObjects
 *    Strategy: per-part VBOs, per-instance glUniformMatrix4fv + glDrawElements
 *
 *  Tier 2 (GL 3.1+ instanced)
 *    Requirements: Tier 1 + hasVAO + hasInstancing + hasAttribDivisor
 *    Strategy: per-part VAO, per-frame instance-attribute VBO,
 *              glDrawElementsInstanced – O(unique_parts) draw calls
 */

struct SoGLContext;

namespace obol {
namespace internal {

struct CadGLCaps {
    /** VBO functions available (GL 1.5 / ARB_vertex_buffer_object). */
    bool hasVBO           = false;
    /** GLSL shader objects available (GL 2.0 / ARB_shader_objects). */
    bool hasShaderObjects = false;
    /** Vertex Array Objects available (GL 3.0 / ARB_vertex_array_object). */
    bool hasVAO           = false;
    /** glDrawElementsInstanced available (GL 3.1 / ARB_draw_instanced). */
    bool hasInstancing    = false;
    /** glVertexAttribDivisor available (GL 3.3 / ARB_instanced_arrays). */
    bool hasAttribDivisor = false;
    /**
     * True when GLSL vertex shaders actually execute during glDrawElements.
     * Some older software renderers (e.g. Mesa 7.x swrast) report
     * ARB_shader_objects as available but silently produce no output when
     * glDrawElements is called with a GLSL program active.  detect() draws
     * a test triangle into a tiny scratch framebuffer and reads back a pixel
     * to confirm the path works end-to-end.
     */
    bool hasGLSLDraw      = false;

    /** True if the minimum requirements for Tier-2 instanced rendering are met. */
    bool canUseInstanced() const {
        return hasVBO && hasShaderObjects && hasGLSLDraw
            && hasVAO && hasInstancing && hasAttribDivisor;
    }

    /** True if the minimum requirements for Tier-1 VBO-loop rendering are met. */
    bool canUseVbo() const {
        return hasVBO && hasShaderObjects && hasGLSLDraw;
    }

    /** Detect capabilities from the given GL context. */
    static CadGLCaps detect(const SoGLContext * glue);
};

} // namespace internal
} // namespace obol

#endif // OBOL_CAD_CADGLCAPS_H
