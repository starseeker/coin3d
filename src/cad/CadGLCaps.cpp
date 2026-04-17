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

#include "CadGLCaps.h"
#include "glue/glp.h"

#include <Inventor/system/gl.h>

namespace obol {
namespace internal {

CadGLCaps CadGLCaps::detect(const SoGLContext * glue)
{
    CadGLCaps caps;
    if (!glue) return caps;

    caps.hasVBO = (glue->glGenBuffers       != nullptr &&
                   glue->glBindBuffer       != nullptr &&
                   glue->glBufferData       != nullptr &&
                   glue->glDeleteBuffers    != nullptr &&
                   glue->glDrawElements     != nullptr &&
                   glue->glVertexAttribPointerARB        != nullptr &&
                   glue->glEnableVertexAttribArrayARB    != nullptr &&
                   glue->glDisableVertexAttribArrayARB   != nullptr);

    caps.hasShaderObjects = (glue->glCreateShaderObjectARB  != nullptr &&
                             glue->glShaderSourceARB         != nullptr &&
                             glue->glCompileShaderARB        != nullptr &&
                             glue->glCreateProgramObjectARB  != nullptr &&
                             glue->glAttachObjectARB         != nullptr &&
                             glue->glLinkProgramARB          != nullptr &&
                             glue->glUseProgramObjectARB     != nullptr &&
                             glue->glGetUniformLocationARB   != nullptr &&
                             glue->glUniformMatrix4fvARB     != nullptr &&
                             glue->glUniform4fvARB           != nullptr &&
                             glue->glUniform3fvARB           != nullptr &&
                             glue->glUniform1iARB            != nullptr &&
                             glue->glGetAttribLocationARB    != nullptr);

    caps.hasVAO = (glue->glGenVertexArrays    != nullptr &&
                   glue->glBindVertexArray    != nullptr &&
                   glue->glDeleteVertexArrays != nullptr);

    caps.hasInstancing = (glue->glDrawElementsInstanced != nullptr);

    caps.hasAttribDivisor = (glue->glVertexAttribDivisor != nullptr);

    // Probe whether GLSL vertex shaders actually execute during glDrawElements.
    // Some software renderers (Mesa 7.x swrast) report ARB_shader_objects but
    // silently produce no output when drawing with a GLSL program + VBO.
    // We compile a minimal GLSL 1.10 program, draw a full-viewport triangle
    // clipped to a single pixel, read back the pixel, and immediately restore
    // the framebuffer so the caller's render is not contaminated.
    //
    // Key design choices:
    //   - Oversized NDC triangle (-2,-2)→(4,-2)→(-2,4) covers the ENTIRE
    //     viewport regardless of size, ensuring pixel (0,0) is always hit.
    //   - Scissor to (0,0,1,1) restricts the draw to one pixel so the probe
    //     is fast and easy to restore.
    //   - After the test we clear pixel (0,0) back to its original colour via
    //     glClear + scissor so the caller's framebuffer is not contaminated.
    if (caps.hasVBO && caps.hasShaderObjects) {
        static const char * kProbeVS =
            "attribute vec3 a_pos;\n"
            "void main() { gl_Position = vec4(a_pos, 1.0); }\n";
        static const char * kProbeFS =
            "void main() { gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0); }\n";

        // Compile probe shaders
        GLhandleARB vs = glue->glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
        GLhandleARB fs = glue->glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
        GLhandleARB prog = 0;
        bool probeOk = false;

        auto compOk = [&](GLhandleARB sh, const char* src) -> bool {
            glue->glShaderSourceARB(sh, 1, (const OBOL_GLchar**)&src, nullptr);
            glue->glCompileShaderARB(sh);
            GLint ok = 0;
            glue->glGetObjectParameterivARB(sh, GL_OBJECT_COMPILE_STATUS_ARB, &ok);
            return ok != 0;
        };

        if (vs && fs && compOk(vs, kProbeVS) && compOk(fs, kProbeFS)) {
            prog = glue->glCreateProgramObjectARB();
            if (prog) {
                glue->glAttachObjectARB(prog, vs);
                glue->glAttachObjectARB(prog, fs);
                if (glue->glBindAttribLocationARB) {
                    glue->glBindAttribLocationARB(prog, 0,
                        reinterpret_cast<OBOL_GLchar*>(const_cast<char*>("a_pos")));
                }
                glue->glLinkProgramARB(prog);
                GLint linked = 0;
                glue->glGetObjectParameterivARB(prog, GL_OBJECT_LINK_STATUS_ARB, &linked);

                if (linked) {
                    // Oversized NDC triangle (-2,-2)→(4,-2)→(-2,4): covers the
                    // entire viewport so pixel (0,0) is always inside the raster,
                    // regardless of viewport dimensions.
                    static const float kTri[9] = {
                        -2.0f, -2.0f, 0.0f,
                         4.0f, -2.0f, 0.0f,
                        -2.0f,  4.0f, 0.0f
                    };
                    static const GLuint kIdx[3] = { 0, 1, 2 };

                    GLuint vbo = 0, ibo = 0;
                    glue->glGenBuffers(1, &vbo);
                    glue->glGenBuffers(1, &ibo);
                    glue->glBindBuffer(GL_ARRAY_BUFFER, vbo);
                    glue->glBufferData(GL_ARRAY_BUFFER, sizeof(kTri), kTri, GL_STATIC_DRAW);
                    glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
                    glue->glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kIdx), kIdx, GL_STATIC_DRAW);

                    // Save pixel (0,0) before probe so we can restore it
                    unsigned char before[4] = {}, after[4] = {};
                    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, before);

                    // Restrict the probe draw to a single pixel via scissor
                    GLboolean scissorWasEnabled = glIsEnabled(GL_SCISSOR_TEST);
                    GLint scissorBox[4] = {};
                    glGetIntegerv(GL_SCISSOR_BOX, scissorBox);
                    glEnable(GL_SCISSOR_TEST);
                    glScissor(0, 0, 1, 1);

                    // Disable depth test for the probe so it always writes
                    GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
                    glDisable(GL_DEPTH_TEST);

                    glue->glUseProgramObjectARB(prog);
                    glue->glVertexAttribPointerARB(0, 3, GL_FLOAT, GL_FALSE,
                                                   3 * sizeof(float), nullptr);
                    glue->glEnableVertexAttribArrayARB(0);
                    glue->glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, nullptr);
                    glue->glDisableVertexAttribArrayARB(0);
                    glue->glUseProgramObjectARB(0);

                    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, after);

                    // Restore the single pixel we may have overwritten
                    // (write it back via a 1x1 glDrawPixels if available,
                    // otherwise fall back to glClear of just that pixel)
                    glScissor(0, 0, 1, 1);
                    glClearColor(before[0] / 255.0f, before[1] / 255.0f,
                                 before[2] / 255.0f, before[3] / 255.0f);
                    glClear(GL_COLOR_BUFFER_BIT);
                    // Restore scissor state
                    if (!scissorWasEnabled)
                        glDisable(GL_SCISSOR_TEST);
                    glScissor(scissorBox[0], scissorBox[1],
                              scissorBox[2], scissorBox[3]);
                    // Restore depth test state
                    if (depthWasEnabled)
                        glEnable(GL_DEPTH_TEST);

                    glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
                    glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
                    glue->glDeleteBuffers(1, &vbo);
                    glue->glDeleteBuffers(1, &ibo);

                    // The red channel of the probe triangle is 1.0.  If the pixel
                    // changed to non-zero red, GLSL+VBO rendering works.
                    probeOk = (after[0] > 0);
                }
            }
        }

        if (vs)   glue->glDeleteObjectARB(vs);
        if (fs)   glue->glDeleteObjectARB(fs);
        if (prog) glue->glDeleteObjectARB(prog);

        caps.hasGLSLDraw = probeOk;
    }

    return caps;
}

} // namespace internal
} // namespace obol
