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

#include "CadRendererGL.h"

#include <Obol/cad/SoCADAssembly.h>

#include <Inventor/misc/SoContextHandler.h>
#include <Inventor/system/gl.h>
#include <Inventor/SbVec3f.h>
#include "glue/glp.h"

#include <cstring>
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>
#include <cassert>
#include <mutex>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <chrono>

#ifndef GL_DRAW_INDIRECT_BUFFER
#define GL_DRAW_INDIRECT_BUFFER 0x8F3F
#endif
#ifndef GL_DRAW_INDIRECT_BUFFER_BINDING
#define GL_DRAW_INDIRECT_BUFFER_BINDING 0x8F43
#endif

static_assert(sizeof(SbVec3f) == 3 * sizeof(float),
              "SbVec3f must remain tightly packed for CAD GPU uploads");

static const float*
packedVec3fData(const std::vector<SbVec3f>& values)
{
    return values.empty() ? nullptr : values[0].getValue();
}

static void
appendPackedPoint(std::vector<float>& packed, const SbVec3f& point)
{
    packed.push_back(point[0]);
    packed.push_back(point[1]);
    packed.push_back(point[2]);
}

static void
setImmediateMaterialFromRgba(const SoGLContext *glue, const uint8_t rgba[4])
{
    const float r = rgba[0] / 255.0f;
    const float g = rgba[1] / 255.0f;
    const float b = rgba[2] / 255.0f;
    const float a = rgba[3] / 255.0f;

    const GLfloat ambient[4] = {r * 0.2f, g * 0.2f, b * 0.2f, a};
    const GLfloat diffuse[4] = {r * 0.6f, g * 0.6f, b * 0.6f, a};
    const GLfloat specular[4] = {r * 0.2f, g * 0.2f, b * 0.2f, a};
    const GLfloat emission[4] = {0.0f, 0.0f, 0.0f, a};

    glue->glColor4ub(rgba[0], rgba[1], rgba[2], rgba[3]);
    glue->glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, emission);
    glue->glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambient);
    glue->glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
    glue->glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
}

class CadCullRasterGuard {
public:
    explicit CadCullRasterGuard(const SoGLContext *context) : glue_(context)
    {
        enabled_ = glue_->glIsEnabled(GL_CULL_FACE);
        glue_->glGetIntegerv(GL_FRONT_FACE, &frontFace_);
        glue_->glGetIntegerv(GL_CULL_FACE_MODE, &cullFace_);
        SoGLContext_glFrontFace(glue_, GL_CCW);
        glue_->glCullFace(GL_BACK);
        SoGLContext_glDisable(glue_, GL_CULL_FACE);
    }

    ~CadCullRasterGuard()
    {
        SoGLContext_glFrontFace(glue_, static_cast<GLenum>(frontFace_));
        glue_->glCullFace(static_cast<GLenum>(cullFace_));
        if (enabled_)
            SoGLContext_glEnable(glue_, GL_CULL_FACE);
        else
            SoGLContext_glDisable(glue_, GL_CULL_FACE);
    }

private:
    const SoGLContext *glue_;
    GLboolean enabled_ = GL_FALSE;
    GLint frontFace_ = GL_CCW;
    GLint cullFace_ = GL_BACK;
};

/* Direct CAD rendering is embedded in a Coin traversal.  Preserve bindings
 * and state which are outside SoGLLazyElement's model; leaving any of these at
 * CAD-local values can corrupt the next node or the next QOpenGLWidget frame. */
class CadDirectGLStateGuard {
public:
    CadDirectGLStateGuard(const SoGLContext *context, bool vbo, bool vao,
                          bool indirect)
        : glue_(context), hasVbo_(vbo), hasVao_(vao),
          hasIndirect_(indirect)
    {
#ifdef GL_CURRENT_PROGRAM
        if (glue_->glUseProgramObjectARB) {
            glue_->glGetIntegerv(GL_CURRENT_PROGRAM, &program_);
            hasProgram_ = true;
        }
#endif
#ifdef GL_VERTEX_ARRAY_BINDING
        if (hasVao_)
            glue_->glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao_);
#endif
#if defined(GL_ARRAY_BUFFER_BINDING) && defined(GL_ELEMENT_ARRAY_BUFFER_BINDING)
        if (hasVbo_) {
            glue_->glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer_);
            glue_->glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING,
                                 &elementBuffer_);
        }
#endif
        if (hasIndirect_)
            glue_->glGetIntegerv(
                GL_DRAW_INDIRECT_BUFFER_BINDING, &indirectBuffer_);
        glue_->glGetIntegerv(GL_MATRIX_MODE, &matrixMode_);
        glue_->glGetBooleanv(GL_COLOR_WRITEMASK, colorMask_);
        polygonOffsetEnabled_ =
            glue_->glIsEnabled(GL_POLYGON_OFFSET_FILL);
        glue_->glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &polygonOffsetFactor_);
        glue_->glGetFloatv(GL_POLYGON_OFFSET_UNITS, &polygonOffsetUnits_);
    }

    ~CadDirectGLStateGuard()
    {
        if (hasProgram_)
            glue_->glUseProgramObjectARB(
                static_cast<GLhandleARB>(program_));
        if (hasVao_)
            glue_->glBindVertexArray(static_cast<GLuint>(vao_));
        if (hasVbo_) {
            glue_->glBindBuffer(GL_ARRAY_BUFFER,
                                static_cast<GLuint>(arrayBuffer_));
            glue_->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
                                static_cast<GLuint>(elementBuffer_));
        }
        if (hasIndirect_)
            glue_->glBindBuffer(GL_DRAW_INDIRECT_BUFFER,
                                static_cast<GLuint>(indirectBuffer_));
        glue_->glMatrixMode(static_cast<GLenum>(matrixMode_));
        SoGLContext_glColorMask(glue_, colorMask_[0], colorMask_[1],
                                colorMask_[2], colorMask_[3]);
        SoGLContext_glPolygonOffset(glue_, polygonOffsetFactor_,
                                    polygonOffsetUnits_);
        if (polygonOffsetEnabled_)
            SoGLContext_glEnable(glue_, GL_POLYGON_OFFSET_FILL);
        else
            SoGLContext_glDisable(glue_, GL_POLYGON_OFFSET_FILL);
    }

private:
    const SoGLContext *glue_;
    bool hasProgram_ = false;
    bool hasVbo_ = false;
    bool hasVao_ = false;
    bool hasIndirect_ = false;
    GLint program_ = 0;
    GLint vao_ = 0;
    GLint arrayBuffer_ = 0;
    GLint elementBuffer_ = 0;
    GLint indirectBuffer_ = 0;
    GLint matrixMode_ = GL_MODELVIEW;
    GLboolean colorMask_[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
    GLboolean polygonOffsetEnabled_ = GL_FALSE;
    GLfloat polygonOffsetFactor_ = 0.0f;
    GLfloat polygonOffsetUnits_ = 0.0f;
};

/* GL_TIME_ELAPSED queries are deliberately harvested on a later render.
 * Ending a query is non-blocking; reading it here would move the driver's
 * deferred raster wait directly back onto the GUI thread. */
class CadGpuTimerGuard {
public:
    CadGpuTimerGuard(Obol::internal::CadGpuResources *resources,
                     const SoGLContext *context,
                     const uint64_t *triangles)
        : resources_(resources), glue_(context), triangles_(triangles)
    {
        active_ = resources_ &&
            resources_->beginFrameGpuTimer(glue_);
    }

    ~CadGpuTimerGuard()
    {
        if (active_)
            resources_->endFrameGpuTimer(
                triangles_ ? *triangles_ : 0, glue_);
    }

private:
    Obol::internal::CadGpuResources *resources_ = nullptr;
    const SoGLContext *glue_ = nullptr;
    const uint64_t *triangles_ = nullptr;
    bool active_ = false;
};

static void
setCadBackfaceCulling(const SoGLContext *glue, bool enabled)
{
    if (enabled)
        SoGLContext_glEnable(glue, GL_CULL_FACE);
    else
        SoGLContext_glDisable(glue, GL_CULL_FACE);
}

static bool
softwareGlslRequested()
{
    const char *value = std::getenv("OBOL_CAD_SOFTWARE_GLSL");
    return value && value[0] != '\0' && value[0] != '0';
}

static bool
cadLightDebugRequested()
{
    const char *value = std::getenv("OBOL_CAD_LIGHT_DEBUG");
    return value && value[0] != '\0' && value[0] != '0';
}

static const char *
cadShaderDebugMode()
{
    const char *value = std::getenv("OBOL_CAD_SHADER_DEBUG");
    return value && value[0] != '\0' ? value : nullptr;
}

// ---------------------------------------------------------------------------
// GLSL shader sources – Tier 1 (GL 2.0 / GLSL 1.10, no #version directive)
// ---------------------------------------------------------------------------

// Wire pass: no lighting, colour from uniform
static const char * kWireVS1 =
    "attribute vec3 a_pos;\n"
    "uniform mat4 u_model;\n"
    "uniform mat4 u_viewProj;\n"
    "uniform vec4 u_color;\n"
    "varying vec4 v_color;\n"
    "void main() {\n"
    "    vec4 wp = u_model * vec4(a_pos, 1.0);\n"
    "    gl_Position = u_viewProj * wp;\n"
    "    v_color = u_color;\n"
    "}\n";

static const char * kWirePopVS1 =
    "attribute vec3 a_pos;\n"
    "uniform mat4 u_model;\n"
    "uniform mat4 u_viewProj;\n"
    "uniform vec4 u_color;\n"
    "uniform vec3 u_popEncodeScale;\n"
    "uniform vec3 u_popDecodeScale;\n"
    "uniform vec3 u_popMin;\n"
    "varying vec4 v_color;\n"
    "void main() {\n"
    "    vec3 scaled = (a_pos - u_popMin) * u_popEncodeScale;\n"
    "    vec3 low = floor(scaled);\n"
    "    vec3 high = ceil(scaled);\n"
    "    vec3 p = (low + high) * u_popDecodeScale + u_popMin;\n"
    "    vec4 wp = u_model * vec4(p, 1.0);\n"
    "    gl_Position = u_viewProj * wp;\n"
    "    v_color = u_color;\n"
    "}\n";

static const char * kWireFS1 =
    "varying vec4 v_color;\n"
    "void main() {\n"
    "    gl_FragColor = v_color;\n"
    "}\n";

// View-local proxy points carry their own per-occurrence colour so thousands
// of differently coloured AABB/OBB replacements remain one draw call.
static const char * kProxyPointVS1 =
    "attribute vec3 a_pos;\n"
    "attribute vec4 a_color;\n"
    "uniform mat4 u_viewProj;\n"
    "varying vec4 v_color;\n"
    "void main() {\n"
    "    gl_Position = u_viewProj * vec4(a_pos, 1.0);\n"
    "    v_color = a_color;\n"
    "}\n";

// Shaded pass: multi-light (directional/point/spot) in world space
static const char * kShadedVS1 =
    "attribute vec3 a_pos;\n"
    "attribute vec3 a_norm;\n"
    "uniform mat4  u_model;\n"
    "uniform mat4  u_viewProj;\n"
    "uniform vec4  u_color;\n"
    "uniform int   u_hasNorm;\n"
    "varying vec3  v_norm;\n"
    "varying vec3  v_worldPos;\n"
    "varying vec4  v_color;\n"
    "void main() {\n"
    "    vec4 wp = u_model * vec4(a_pos, 1.0);\n"
    "    gl_Position = u_viewProj * wp;\n"
    "    v_worldPos = wp.xyz;\n"
    "    v_norm = mat3(u_model[0].xyz, u_model[1].xyz, u_model[2].xyz) * a_norm;\n"
    "    v_color = u_color;\n"
    "}\n";

static const char * kShadedPopVS1 =
    "attribute vec3 a_pos;\n"
    "attribute vec3 a_norm;\n"
    "uniform mat4  u_model;\n"
    "uniform mat4  u_viewProj;\n"
    "uniform vec4  u_color;\n"
    "uniform int   u_hasNorm;\n"
    "uniform vec3  u_popEncodeScale;\n"
    "uniform vec3  u_popDecodeScale;\n"
    "uniform vec3  u_popMin;\n"
    "varying vec3  v_norm;\n"
    "varying vec3  v_worldPos;\n"
    "varying vec4  v_color;\n"
    "void main() {\n"
    "    vec3 scaled = (a_pos - u_popMin) * u_popEncodeScale;\n"
    "    vec3 low = floor(scaled);\n"
    "    vec3 high = ceil(scaled);\n"
    "    vec3 p = (low + high) * u_popDecodeScale + u_popMin;\n"
    "    vec4 wp = u_model * vec4(p, 1.0);\n"
    "    gl_Position = u_viewProj * wp;\n"
    "    v_worldPos = wp.xyz;\n"
    "    v_norm = mat3(u_model[0].xyz, u_model[1].xyz, u_model[2].xyz) * a_norm;\n"
    "    v_color = u_color;\n"
    "}\n";

/* Face-normal shading derives its normal from world-position derivatives in
 * the fragment stage.  Do not make every vertex transform an a_norm value
 * that this program can never consume. */
static const char * kShadedFaceVS1 =
    "attribute vec3 a_pos;\n"
    "uniform mat4  u_model;\n"
    "uniform mat4  u_viewProj;\n"
    "uniform vec4  u_color;\n"
    "varying vec3  v_worldPos;\n"
    "varying vec4  v_color;\n"
    "void main() {\n"
    "    vec4 wp = u_model * vec4(a_pos, 1.0);\n"
    "    gl_Position = u_viewProj * wp;\n"
    "    v_worldPos = wp.xyz;\n"
    "    v_color = u_color;\n"
    "}\n";

static const char * kShadedPopFaceVS1 =
    "attribute vec3 a_pos;\n"
    "uniform mat4  u_model;\n"
    "uniform mat4  u_viewProj;\n"
    "uniform vec4  u_color;\n"
    "uniform vec3  u_popEncodeScale;\n"
    "uniform vec3  u_popDecodeScale;\n"
    "uniform vec3  u_popMin;\n"
    "varying vec3  v_worldPos;\n"
    "varying vec4  v_color;\n"
    "void main() {\n"
    "    vec3 scaled = (a_pos - u_popMin) * u_popEncodeScale;\n"
    "    vec3 low = floor(scaled);\n"
    "    vec3 high = ceil(scaled);\n"
    "    vec3 p = (low + high) * u_popDecodeScale + u_popMin;\n"
    "    vec4 wp = u_model * vec4(p, 1.0);\n"
    "    gl_Position = u_viewProj * wp;\n"
    "    v_worldPos = wp.xyz;\n"
    "    v_color = u_color;\n"
    "}\n";

/* Keep the direct GLSL tiers visually consistent with the compatibility
 * renderer and BRL-CAD's established default material: material ambient 0.2
 * under environment ambient 0.3 gives 0.06, diffuse is 0.6, and the default
 * zero-shininess specular term contributes 0.2 whenever the face is lit.
 * Besides avoiding backend-dependent appearance, retaining these exact
 * weights prevents broad headlight-facing surfaces from saturating. */
static const char * kShadedFS1 =
    "uniform int   u_numLights;\n"
    "uniform int   u_hasNorm;\n"
    "uniform int   u_ltype[8];\n"
    "uniform vec3  u_lvec[8];\n"
    "uniform vec3  u_laxis[8];\n"
    "uniform vec3  u_lcolor[8];\n"
    "uniform float u_lcos[8];\n"
    "uniform vec3  u_eyeWorld;\n"
    "uniform vec3  u_viewTowardEye;\n"
    "uniform int   u_perspective;\n"
    "varying vec3  v_norm;\n"
    "varying vec3  v_worldPos;\n"
    "varying vec4  v_color;\n"
    "void main() {\n"
    "    vec3 n;\n"
    "    if (u_hasNorm != 0) {\n"
    "        n = normalize(v_norm);\n"
    "        if (!gl_FrontFacing) n = -n;\n"
    "    } else {\n"
    "        vec3 fn = cross(dFdx(v_worldPos), dFdy(v_worldPos));\n"
    "        float fl = length(fn);\n"
    "        n = (fl > 0.0) ? fn / fl : vec3(0.0, 0.0, 1.0);\n"
    "        vec3 toEye = (u_perspective != 0) ?\n"
    "            normalize(u_eyeWorld - v_worldPos) : u_viewTowardEye;\n"
    "        if (dot(n, toEye) < 0.0) n = -n;\n"
    "    }\n"
    "    vec3 col = v_color.rgb * 0.06;\n"
    "    for (int i = 0; i < 8; i++) {\n"
    "        if (i >= u_numLights) break;\n"
    "        vec3 L;\n"
    "        float atten = 1.0;\n"
    "        if (u_ltype[i] == 0) {\n"
    "            L = normalize(u_lvec[i]);\n"
    "        } else {\n"
    "            vec3 d = u_lvec[i] - v_worldPos;\n"
    "            float dl = length(d);\n"
    "            L = (dl > 0.0) ? d / dl : vec3(0.0, 0.0, 1.0);\n"
    "            if (u_ltype[i] == 2) {\n"
    "                float c = dot(normalize(u_laxis[i]), -L);\n"
    "                if (c < u_lcos[i]) atten = 0.0;\n"
    "            }\n"
    "        }\n"
    "        float ndl = max(0.0, dot(n, L));\n"
    "        float spec = (ndl > 0.0) ? 0.2 : 0.0;\n"
    "        col += v_color.rgb * u_lcolor[i] *\n"
    "               ((ndl * 0.6 + spec) * atten);\n"
    "    }\n"
    "    gl_FragColor = vec4(col, v_color.a);\n"
    "}\n";

/* The ordinary qged/mged scene has exactly one directional headlight.  Keep a
 * pair of compact shaders for that overwhelmingly common case instead of
 * making every software-rendered fragment execute the generic eight-light
 * loop, dynamically indexed uniform arrays, and both normal-generation paths.
 * The generic shader remains authoritative for mixed or multiple lights. */
static const char * kShadedDirectionalNormFS1 =
    "uniform vec3  u_lightVec;\n"
    "uniform vec3  u_lightColor;\n"
    "varying vec3  v_norm;\n"
    "varying vec4  v_color;\n"
    "void main() {\n"
    "    vec3 n = normalize(v_norm);\n"
    "    n *= gl_FrontFacing ? 1.0 : -1.0;\n"
    "    float ndl = max(0.0, dot(n, u_lightVec));\n"
    "    float spec = (ndl > 0.0) ? 0.2 : 0.0;\n"
    "    vec3 col = v_color.rgb *\n"
    "        (vec3(0.06) + u_lightColor * (ndl * 0.6 + spec));\n"
    "    gl_FragColor = vec4(col, v_color.a);\n"
    "}\n";

static const char * kShadedDirectionalFaceFS1 =
    "uniform vec3  u_lightVec;\n"
    "uniform vec3  u_lightColor;\n"
    "uniform vec3  u_eyeWorld;\n"
    "uniform vec3  u_viewTowardEye;\n"
    "uniform int   u_perspective;\n"
    "varying vec3  v_worldPos;\n"
    "varying vec4  v_color;\n"
    "void main() {\n"
    "    vec3 fn = cross(dFdx(v_worldPos), dFdy(v_worldPos));\n"
    "    vec3 n = fn * inversesqrt(max(dot(fn, fn), 1.0e-30));\n"
    "    vec3 toEye = (u_perspective != 0) ?\n"
    "        normalize(u_eyeWorld - v_worldPos) : u_viewTowardEye;\n"
    "    n *= 2.0 * step(0.0, dot(n, toEye)) - 1.0;\n"
    "    float ndl = max(0.0, dot(n, u_lightVec));\n"
    "    float spec = (ndl > 0.0) ? 0.2 : 0.0;\n"
    "    vec3 col = v_color.rgb *\n"
    "        (vec3(0.06) + u_lightColor * (ndl * 0.6 + spec));\n"
    "    gl_FragColor = vec4(col, v_color.a);\n"
    "}\n";

static const char * kShadedFaceDebugFS1 =
    "void main() {\n"
    "    gl_FragColor = gl_FrontFacing ?\n"
    "        vec4(1.0, 0.0, 0.0, 1.0) : vec4(0.0, 0.0, 1.0, 1.0);\n"
    "}\n";

static const char * kShadedNormalDebugFS1 =
    "uniform int u_hasNorm;\n"
    "varying vec3 v_norm;\n"
    "void main() {\n"
    "    vec3 n = normalize(v_norm);\n"
    "    if (!gl_FrontFacing) n = -n;\n"
    "    gl_FragColor = (u_hasNorm == 0) ?\n"
    "        vec4(1.0, 1.0, 0.0, 1.0) :\n"
    "        vec4(n * 0.5 + vec3(0.5), 1.0);\n"
    "}\n";

// ---------------------------------------------------------------------------
// GLSL shader sources – Tier 2 (GL 3.1+ / GLSL 1.40, instanced)
// Per-instance transform and color are passed as vertex attributes with
// divisor=1.  The mat4 occupies 4 consecutive attribute locations.
// ---------------------------------------------------------------------------

static const char * kWireVS2 =
    "#version 140\n"
    "in vec3  a_pos;\n"
    "in mat4  a_instTransform;\n"  // locations: BASE_INST_LOC .. BASE_INST_LOC+3
    "in vec4  a_instColor;\n"      // location:  BASE_INST_LOC+4
    "uniform mat4 u_viewProj;\n"
    "out vec4 v_color;\n"
    "void main() {\n"
    "    gl_Position = u_viewProj * a_instTransform * vec4(a_pos, 1.0);\n"
    "    v_color = a_instColor;\n"
    "}\n";

static const char * kWirePopVS2 =
    "#version 140\n"
    "in vec3  a_pos;\n"
    "in mat4  a_instTransform;\n"
    "in vec4  a_instColor;\n"
    "uniform mat4 u_viewProj;\n"
    "uniform vec3 u_popEncodeScale;\n"
    "uniform vec3 u_popDecodeScale;\n"
    "uniform vec3 u_popMin;\n"
    "out vec4 v_color;\n"
    "void main() {\n"
    "    vec3 scaled = (a_pos - u_popMin) * u_popEncodeScale;\n"
    "    vec3 low = floor(scaled);\n"
    "    vec3 high = ceil(scaled);\n"
    "    vec3 p = (low + high) * u_popDecodeScale + u_popMin;\n"
    "    gl_Position = u_viewProj * a_instTransform * vec4(p, 1.0);\n"
    "    v_color = a_instColor;\n"
    "}\n";

static const char * kWireFS2 =
    "#version 140\n"
    "in  vec4 v_color;\n"
    "out vec4 fragColor;\n"
    "void main() { fragColor = v_color; }\n";

static const char * kShadedVS2 =
    "#version 140\n"
    "in vec3  a_pos;\n"
    "in vec3  a_norm;\n"
    "in mat4  a_instTransform;\n"
    "in vec4  a_instColor;\n"
    "uniform mat4 u_viewProj;\n"
    "uniform int  u_hasNorm;\n"
    "out vec3 v_norm;\n"
    "out vec3 v_worldPos;\n"
    "out vec4 v_color;\n"
    "void main() {\n"
    "    vec4 wp = a_instTransform * vec4(a_pos, 1.0);\n"
    "    gl_Position = u_viewProj * wp;\n"
    "    v_worldPos = wp.xyz;\n"
    "    if (u_hasNorm != 0) {\n"
    "        mat3 nm = mat3(a_instTransform[0].xyz,\n"
    "                       a_instTransform[1].xyz,\n"
    "                       a_instTransform[2].xyz);\n"
    "        v_norm = nm * a_norm;\n"
    "    } else {\n"
    "        v_norm = vec3(0.0, 0.0, 1.0);\n"
    "    }\n"
    "    v_color = a_instColor;\n"
    "}\n";

static const char * kShadedPopVS2 =
    "#version 140\n"
    "in vec3  a_pos;\n"
    "in vec3  a_norm;\n"
    "in mat4  a_instTransform;\n"
    "in vec4  a_instColor;\n"
    "uniform mat4 u_viewProj;\n"
    "uniform int  u_hasNorm;\n"
    "uniform vec3 u_popEncodeScale;\n"
    "uniform vec3 u_popDecodeScale;\n"
    "uniform vec3 u_popMin;\n"
    "out vec3 v_norm;\n"
    "out vec3 v_worldPos;\n"
    "out vec4 v_color;\n"
    "void main() {\n"
    "    vec3 scaled = (a_pos - u_popMin) * u_popEncodeScale;\n"
    "    vec3 low = floor(scaled);\n"
    "    vec3 high = ceil(scaled);\n"
    "    vec3 p = (low + high) * u_popDecodeScale + u_popMin;\n"
    "    vec4 wp = a_instTransform * vec4(p, 1.0);\n"
    "    gl_Position = u_viewProj * wp;\n"
    "    v_worldPos = wp.xyz;\n"
    "    if (u_hasNorm != 0) {\n"
    "        mat3 nm = mat3(a_instTransform[0].xyz,\n"
    "                       a_instTransform[1].xyz,\n"
    "                       a_instTransform[2].xyz);\n"
    "        v_norm = nm * a_norm;\n"
    "    } else {\n"
    "        v_norm = vec3(0.0, 0.0, 1.0);\n"
    "    }\n"
    "    v_color = a_instColor;\n"
    "}\n";

static const char * kShadedFS2 =
    "#version 140\n"
    "uniform int   u_numLights;\n"
    "uniform int   u_hasNorm;\n"
    "uniform int   u_ltype[8];\n"
    "uniform vec3  u_lvec[8];\n"
    "uniform vec3  u_laxis[8];\n"
    "uniform vec3  u_lcolor[8];\n"
    "uniform float u_lcos[8];\n"
    "uniform vec3  u_eyeWorld;\n"
    "uniform vec3  u_viewTowardEye;\n"
    "uniform int   u_perspective;\n"
    "in  vec3 v_norm;\n"
    "in  vec3 v_worldPos;\n"
    "in  vec4 v_color;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "    vec3 n;\n"
    "    if (u_hasNorm != 0) {\n"
    "        n = normalize(v_norm);\n"
    "        if (!gl_FrontFacing) n = -n;\n"
    "    } else {\n"
    "        vec3 fn = cross(dFdx(v_worldPos), dFdy(v_worldPos));\n"
    "        float fl = length(fn);\n"
    "        n = (fl > 0.0) ? fn / fl : vec3(0.0, 0.0, 1.0);\n"
    "        vec3 toEye = (u_perspective != 0) ?\n"
    "            normalize(u_eyeWorld - v_worldPos) : u_viewTowardEye;\n"
    "        if (dot(n, toEye) < 0.0) n = -n;\n"
    "    }\n"
    "    vec3 col = v_color.rgb * 0.06;\n"
    "    for (int i = 0; i < 8; i++) {\n"
    "        if (i >= u_numLights) break;\n"
    "        vec3 L;\n"
    "        float atten = 1.0;\n"
    "        if (u_ltype[i] == 0) {\n"
    "            L = normalize(u_lvec[i]);\n"
    "        } else {\n"
    "            vec3 d = u_lvec[i] - v_worldPos;\n"
    "            float dl = length(d);\n"
    "            L = (dl > 0.0) ? d / dl : vec3(0.0, 0.0, 1.0);\n"
    "            if (u_ltype[i] == 2) {\n"
    "                float c = dot(normalize(u_laxis[i]), -L);\n"
    "                if (c < u_lcos[i]) atten = 0.0;\n"
    "            }\n"
    "        }\n"
    "        float ndl = max(0.0, dot(n, L));\n"
    "        float spec = (ndl > 0.0) ? 0.2 : 0.0;\n"
    "        col += v_color.rgb * u_lcolor[i] *\n"
    "               ((ndl * 0.6 + spec) * atten);\n"
    "    }\n"
    "    fragColor = vec4(col, v_color.a);\n"
    "}\n";

/*
 * Cross-part indirect shading.  Unlike the older per-part instanced shader,
 * every value that differs by part or PoP cut is an instance attribute.
 * baseInstance in each indirect command selects the appropriate records, so
 * thousands of unrelated mesh ranges share one program and one submission.
 */
static const char * kShadedIndirectVS2 =
    "#version 140\n"
    "in vec3  a_pos;\n"
    "in vec3  a_norm;\n"
    "in mat4  a_instTransform;\n"
    "in vec4  a_instColor;\n"
    "in vec4  a_instPopMinLevel;\n"
    "in vec4  a_instPopMaxFlags;\n"
    "uniform mat4 u_viewProj;\n"
    "out vec3 v_norm;\n"
    "out vec3 v_worldPos;\n"
    "out vec4 v_color;\n"
    "flat out float v_hasNorm;\n"
    "void main() {\n"
    "    float packedFlags = floor(a_instPopMaxFlags.w + 0.5);\n"
    "    float hasNorm = mod(packedFlags, 2.0);\n"
    "    bool progressive = mod(floor(packedFlags / 2.0), 2.0) > 0.5;\n"
    "    bool hidden = packedFlags >= 4.0;\n"
    "    vec3 p = a_pos;\n"
    "    if (progressive && a_instPopMinLevel.w < 15.0) {\n"
    "        float level = clamp(a_instPopMinLevel.w, 0.0, 15.0);\n"
    "        float mask = exp2(15.0 - level);\n"
    "        vec3 minimum = a_instPopMinLevel.xyz;\n"
    "        vec3 extent = a_instPopMaxFlags.xyz - minimum;\n"
    "        vec3 safeExtent = max(extent, vec3(1.0e-30));\n"
    "        vec3 scaled = (a_pos - minimum) *\n"
    "            (vec3(65535.0) / (safeExtent * mask));\n"
    "        p = (floor(scaled) + ceil(scaled)) *\n"
    "            (0.5 * mask * extent / 65535.0) + minimum;\n"
    "    }\n"
    "    vec4 wp = a_instTransform * vec4(p, 1.0);\n"
    "    gl_Position = hidden ? vec4(2.0, 2.0, 2.0, 1.0) :\n"
    "        u_viewProj * wp;\n"
    "    v_worldPos = wp.xyz;\n"
    "    if (hasNorm > 0.5) {\n"
    "        mat3 nm = mat3(a_instTransform[0].xyz,\n"
    "                       a_instTransform[1].xyz,\n"
    "                       a_instTransform[2].xyz);\n"
    "        v_norm = nm * a_norm;\n"
    "    } else {\n"
    "        v_norm = vec3(0.0, 0.0, 1.0);\n"
    "    }\n"
    "    v_hasNorm = hasNorm;\n"
    "    v_color = a_instColor;\n"
    "}\n";

static const char * kShadedIndirectFS2 =
    "#version 140\n"
    "uniform int   u_numLights;\n"
    "uniform int   u_ltype[8];\n"
    "uniform vec3  u_lvec[8];\n"
    "uniform vec3  u_laxis[8];\n"
    "uniform vec3  u_lcolor[8];\n"
    "uniform float u_lcos[8];\n"
    "uniform vec3  u_eyeWorld;\n"
    "uniform vec3  u_viewTowardEye;\n"
    "uniform int   u_perspective;\n"
    "in  vec3 v_norm;\n"
    "in  vec3 v_worldPos;\n"
    "in  vec4 v_color;\n"
    "flat in float v_hasNorm;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "    vec3 n;\n"
    "    if (v_hasNorm > 0.5) {\n"
    "        n = normalize(v_norm);\n"
    "        if (!gl_FrontFacing) n = -n;\n"
    "    } else {\n"
    "        vec3 fn = cross(dFdx(v_worldPos), dFdy(v_worldPos));\n"
    "        float fl = length(fn);\n"
    "        n = (fl > 0.0) ? fn / fl : vec3(0.0, 0.0, 1.0);\n"
    "        vec3 toEye = (u_perspective != 0) ?\n"
    "            normalize(u_eyeWorld - v_worldPos) : u_viewTowardEye;\n"
    "        if (dot(n, toEye) < 0.0) n = -n;\n"
    "    }\n"
    "    vec3 col = v_color.rgb * 0.06;\n"
    "    for (int i = 0; i < 8; i++) {\n"
    "        if (i >= u_numLights) break;\n"
    "        vec3 L;\n"
    "        float atten = 1.0;\n"
    "        if (u_ltype[i] == 0) {\n"
    "            L = normalize(u_lvec[i]);\n"
    "        } else {\n"
    "            vec3 d = u_lvec[i] - v_worldPos;\n"
    "            float dl = length(d);\n"
    "            L = (dl > 0.0) ? d / dl : vec3(0.0, 0.0, 1.0);\n"
    "            if (u_ltype[i] == 2) {\n"
    "                float c = dot(normalize(u_laxis[i]), -L);\n"
    "                if (c < u_lcos[i]) atten = 0.0;\n"
    "            }\n"
    "        }\n"
    "        float ndl = max(0.0, dot(n, L));\n"
    "        float spec = (ndl > 0.0) ? 0.2 : 0.0;\n"
    "        col += v_color.rgb * u_lcolor[i] *\n"
    "               ((ndl * 0.6 + spec) * atten);\n"
    "    }\n"
    "    fragColor = vec4(col, v_color.a);\n"
    "}\n";

// Attribute locations for instanced vertex attributes
// Attribute 0: a_pos (vec3)
// Attribute 1: a_norm (vec3)  -- shaded only
// Attributes 2..5: a_instTransform (mat4 = 4 × vec4 columns)
// Attribute 6: a_instColor (vec4)
static const GLuint kInstTransformLoc = 2;
static const GLuint kInstColorLoc     = 6;
static const GLuint kInstPopMinLevelLoc = 7;
static const GLuint kInstPopMaxFlagsLoc = 8;

// Fixed light direction (world space, normalised)
static const float kLightDir[3] = { 0.577f, 0.577f, 0.577f };

namespace Obol {
namespace internal {

namespace {

struct SharedCadShaders {
    CadGLCaps caps;
    GLuint wire = 0;
    GLuint wirePop = 0;
    GLuint proxyPoint = 0;
    GLuint shaded = 0;
    GLuint shadedPop = 0;
    GLuint shadedDirectionalNorm = 0;
    GLuint shadedDirectionalFace = 0;
    GLuint shadedPopDirectionalNorm = 0;
    GLuint shadedPopDirectionalFace = 0;
    GLuint wireInst = 0;
    GLuint wirePopInst = 0;
    GLuint shadedInst = 0;
    GLuint shadedPopInst = 0;
    GLuint shadedIndirect = 0;
};

std::mutex sharedCadShadersMutex;
std::unordered_map<uint32_t, SharedCadShaders> sharedCadShaders;
std::once_flag sharedCadShadersCallbackOnce;
std::mutex cadRendererRegistryMutex;
std::unordered_set<CadRendererGL *> cadRendererRegistry;
std::once_flag cadRendererRegistryCallbackOnce;

/*
 * Sparse scene publication deliberately leaves holes in compiled draw-item
 * ranges.  A record may be rebound to a different part while the old range
 * remains allocated, allowing a box-to-mesh promotion to avoid rebuilding
 * the complete scene plan.  Consequently range containment is not proof of
 * membership: every consumer must validate the record's current partIndex.
 */
bool
drawItemOwnsInstance(const CadFramePlan& plan, const CadDrawItem& item,
                     size_t visibleIndex)
{
    return visibleIndex < plan.visibleInstances.size() &&
        plan.visibleInstances[visibleIndex].partIndex == item.partIndex;
}

size_t
firstDrawItemInstance(const CadFramePlan& plan, const CadDrawItem& item)
{
    for (uint32_t offset = 0; offset < item.instanceCount; ++offset) {
        const size_t visibleIndex = item.baseInstance + offset;
        if (drawItemOwnsInstance(plan, item, visibleIndex))
            return visibleIndex;
    }
    return plan.visibleInstances.size();
}

size_t
drawItemLiveInstanceCount(const CadFramePlan& plan,
                          const CadDrawItem& item)
{
    size_t count = 0;
    for (uint32_t offset = 0; offset < item.instanceCount; ++offset)
        if (drawItemOwnsInstance(
                plan, item, item.baseInstance + offset))
            ++count;
    return count;
}

void
releaseSharedCadShaders(uint32_t contextId, void *)
{
    const SoGLContext *glue = SoGLContext_instance(static_cast<int>(contextId));
    std::lock_guard<std::mutex> guard(sharedCadShadersMutex);
    const auto found = sharedCadShaders.find(contextId);
    if (found == sharedCadShaders.end()) return;

    SharedCadShaders &programs = found->second;
    if (glue && glue->glDeleteObjectARB) {
        if (programs.wire) glue->glDeleteObjectARB(programs.wire);
        if (programs.wirePop) glue->glDeleteObjectARB(programs.wirePop);
        if (programs.proxyPoint) glue->glDeleteObjectARB(programs.proxyPoint);
        if (programs.shaded) glue->glDeleteObjectARB(programs.shaded);
        if (programs.shadedPop) glue->glDeleteObjectARB(programs.shadedPop);
        if (programs.shadedDirectionalNorm)
            glue->glDeleteObjectARB(programs.shadedDirectionalNorm);
        if (programs.shadedDirectionalFace)
            glue->glDeleteObjectARB(programs.shadedDirectionalFace);
        if (programs.shadedPopDirectionalNorm)
            glue->glDeleteObjectARB(programs.shadedPopDirectionalNorm);
        if (programs.shadedPopDirectionalFace)
            glue->glDeleteObjectARB(programs.shadedPopDirectionalFace);
        if (programs.wireInst) glue->glDeleteObjectARB(programs.wireInst);
        if (programs.wirePopInst)
            glue->glDeleteObjectARB(programs.wirePopInst);
        if (programs.shadedInst) glue->glDeleteObjectARB(programs.shadedInst);
        if (programs.shadedPopInst)
            glue->glDeleteObjectARB(programs.shadedPopInst);
        if (programs.shadedIndirect)
            glue->glDeleteObjectARB(programs.shadedIndirect);
    }
    sharedCadShaders.erase(found);
}

} // namespace

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

CadRendererGL::CadRendererGL()
{
    {
        std::lock_guard<std::mutex> guard(cadRendererRegistryMutex);
        cadRendererRegistry.insert(this);
    }
    std::call_once(cadRendererRegistryCallbackOnce, []() {
        SoContextHandler::addContextDestructionCallback(
            CadRendererGL::contextDestroyed, nullptr);
    });
}

void
CadRendererGL::uploadLights(const SoGLContext* glue, GLuint program)
{
    // Build parallel arrays for the shaded shader's u_light* uniforms.  Fall
    // back to a single fixed directional light when no scene lights are set.
    int   type[kMaxLights];
    float vec[kMaxLights * 3];
    float axis[kMaxLights * 3];
    float color[kMaxLights * 3];
    float cosCut[kMaxLights];
    int n = 0;
    if (this->lights_.empty()) {
        type[0] = 0;
        vec[0] = kLightDir[0]; vec[1] = kLightDir[1]; vec[2] = kLightDir[2];
        axis[0] = 0.0f; axis[1] = 0.0f; axis[2] = -1.0f;
        color[0] = 1.0f; color[1] = 1.0f; color[2] = 1.0f;
        cosCut[0] = -2.0f;
        n = 1;
    } else {
        for (const GlLight& l : this->lights_) {
            if (n >= kMaxLights) break;
            type[n] = l.type;
            vec[n * 3 + 0] = l.vec[0];
            vec[n * 3 + 1] = l.vec[1];
            vec[n * 3 + 2] = l.vec[2];
            axis[n * 3 + 0] = l.axis[0];
            axis[n * 3 + 1] = l.axis[1];
            axis[n * 3 + 2] = l.axis[2];
            color[n * 3 + 0] = l.color[0];
            color[n * 3 + 1] = l.color[1];
            color[n * 3 + 2] = l.color[2];
            cosCut[n] = l.cosCutoff;
            ++n;
        }
        if (n == 0) { // all filtered out; keep the shader well-defined
            type[0] = 0;
            vec[0] = kLightDir[0]; vec[1] = kLightDir[1]; vec[2] = kLightDir[2];
            axis[0] = 0.0f; axis[1] = 0.0f; axis[2] = -1.0f;
            color[0] = 1.0f; color[1] = 1.0f; color[2] = 1.0f;
            cosCut[0] = -2.0f;
            n = 1;
        }
    }
    const GLint countLocation =
        glue->glGetUniformLocationARB(program, "u_numLights");
    const GLint typeLocation =
        glue->glGetUniformLocationARB(program, "u_ltype");
    const GLint vectorLocation =
        glue->glGetUniformLocationARB(program, "u_lvec");
    const GLint axisLocation =
        glue->glGetUniformLocationARB(program, "u_laxis");
    const GLint colorLocation =
        glue->glGetUniformLocationARB(program, "u_lcolor");
    const GLint cutoffLocation =
        glue->glGetUniformLocationARB(program, "u_lcos");
    glue->glUniform1iARB(countLocation, n);
    glue->glUniform1ivARB(typeLocation, n, type);
    glue->glUniform3fvARB(vectorLocation, n, vec);
    glue->glUniform3fvARB(axisLocation, n, axis);
    glue->glUniform3fvARB(colorLocation, n, color);
    glue->glUniform1fvARB(cutoffLocation, n, cosCut);
    if (cadLightDebugRequested()) {
        static unsigned int reportCount = 0;
        if (reportCount++ < 32) {
            std::fprintf(stderr,
                "CadRendererGL uploadLights program=%u n=%d "
                "locations={count=%d type=%d vec=%d axis=%d color=%d cos=%d} "
                "l0={type=%d vec=(%.9g,%.9g,%.9g) "
                "color=(%.9g,%.9g,%.9g)}\n",
                program, n, countLocation, typeLocation, vectorLocation,
                axisLocation, colorLocation, cutoffLocation, type[0],
                vec[0], vec[1], vec[2], color[0], color[1], color[2]);
        }
    }
}

void
CadRendererGL::uploadViewFacing(const SoGLContext* glue, GLuint program,
                                const SbViewVolume& viewVolume)
{
    const SbVec3f eye = viewVolume.getProjectionPoint();
    SbVec3f towardEye = -viewVolume.getProjectionDirection();
    if (towardEye.normalize() == 0.0f)
        towardEye.setValue(0.0f, 0.0f, 1.0f);
    const GLint eyeLocation =
        glue->glGetUniformLocationARB(program, "u_eyeWorld");
    const GLint directionLocation =
        glue->glGetUniformLocationARB(program, "u_viewTowardEye");
    const GLint perspectiveLocation =
        glue->glGetUniformLocationARB(program, "u_perspective");
    glue->glUniform3fvARB(eyeLocation, 1, eye.getValue());
    glue->glUniform3fvARB(directionLocation, 1, towardEye.getValue());
    glue->glUniform1iARB(
        perspectiveLocation,
        viewVolume.getProjectionType() == SbViewVolume::PERSPECTIVE ? 1 : 0);
}

CadRendererGL::~CadRendererGL()
{
    std::lock_guard<std::mutex> guard(cadRendererRegistryMutex);
    cadRendererRegistry.erase(this);
}

void
CadRendererGL::contextDestroyed(uint32_t contextId, void *)
{
    const SoGLContext *glue = SoGLContext_instance(static_cast<int>(contextId));
    std::lock_guard<std::mutex> guard(cadRendererRegistryMutex);
    for (CadRendererGL *renderer : cadRendererRegistry)
        renderer->releaseContext(contextId, glue);
}

void
CadRendererGL::releaseContext(uint32_t contextId, const SoGLContext *glue)
{
    if (indirectPrepared_.contextId == contextId)
        indirectPrepared_.valid = false;
    const auto found = gpuResources_.find(contextId);
    if (found == gpuResources_.end()) return;
    if (gpuRes_ == found->second.get()) {
        gpuRes_ = nullptr;
        gpuContextId_ = 0;
    }
    found->second->releaseAll(glue);
    gpuResources_.erase(found);
}

// ---------------------------------------------------------------------------
// Shader compilation
// ---------------------------------------------------------------------------

GLuint CadRendererGL::compileShader(const SoGLContext* glue, GLenum type,
                                    const char* src)
{
    GLhandleARB h = glue->glCreateShaderObjectARB(type);
    if (!h) return 0;

    glue->glShaderSourceARB(h, 1, (const OBOL_GLchar**)&src, nullptr);
    glue->glCompileShaderARB(h);

    GLint ok = 0;
    glue->glGetObjectParameterivARB(h, GL_OBJECT_COMPILE_STATUS_ARB, &ok);
    if (!ok) {
        GLint len = 0;
        glue->glGetObjectParameterivARB(h, GL_OBJECT_INFO_LOG_LENGTH_ARB, &len);
        if (len > 1) {
            std::vector<char> log(static_cast<size_t>(len));
            glue->glGetInfoLogARB(h, len, nullptr,
                                  reinterpret_cast<OBOL_GLchar*>(log.data()));
            std::fprintf(stderr, "CadRendererGL: shader compile error:\n%s\n",
                         log.data());
        }
        glue->glDeleteObjectARB(h);
        return 0;
    }
    return static_cast<GLuint>(h);
}

GLuint CadRendererGL::linkProgram(const SoGLContext* glue, GLuint vs, GLuint fs)
{
    GLhandleARB prog = glue->glCreateProgramObjectARB();
    if (!prog) return 0;

    glue->glAttachObjectARB(prog, vs);
    glue->glAttachObjectARB(prog, fs);

    // Bind attribute locations before linking so they are always predictable,
    // regardless of what the GLSL compiler assigns implicitly.
    // Tier-1 uses a_pos (0) and a_norm (1).
    // Tier-2 additionally uses a_instTransform (kInstTransformLoc) and
    // a_instColor (kInstColorLoc).  For a mat4 attribute, binding the base
    // location pins all 4 consecutive slots automatically.  Bindings for
    // attributes absent from the shader are silently ignored.
    if (glue->glBindAttribLocationARB) {
        glue->glBindAttribLocationARB(prog, 0,
                                      reinterpret_cast<OBOL_GLchar*>(
                                          const_cast<char*>("a_pos")));
        glue->glBindAttribLocationARB(prog, 1,
                                      reinterpret_cast<OBOL_GLchar*>(
                                          const_cast<char*>("a_norm")));
        glue->glBindAttribLocationARB(prog, 1,
                                      reinterpret_cast<OBOL_GLchar*>(
                                          const_cast<char*>("a_color")));
        glue->glBindAttribLocationARB(prog, kInstTransformLoc,
                                      reinterpret_cast<OBOL_GLchar*>(
                                          const_cast<char*>("a_instTransform")));
        glue->glBindAttribLocationARB(prog, kInstColorLoc,
                                      reinterpret_cast<OBOL_GLchar*>(
                                          const_cast<char*>("a_instColor")));
        glue->glBindAttribLocationARB(
            prog, kInstPopMinLevelLoc,
            reinterpret_cast<OBOL_GLchar*>(
                const_cast<char*>("a_instPopMinLevel")));
        glue->glBindAttribLocationARB(
            prog, kInstPopMaxFlagsLoc,
            reinterpret_cast<OBOL_GLchar*>(
                const_cast<char*>("a_instPopMaxFlags")));
    }

    glue->glLinkProgramARB(prog);

    GLint ok = 0;
    glue->glGetObjectParameterivARB(prog, GL_OBJECT_LINK_STATUS_ARB, &ok);
    if (!ok) {
        GLint len = 0;
        glue->glGetObjectParameterivARB(prog, GL_OBJECT_INFO_LOG_LENGTH_ARB, &len);
        if (len > 1) {
            std::vector<char> log(static_cast<size_t>(len));
            glue->glGetInfoLogARB(prog, len, nullptr,
                                  reinterpret_cast<OBOL_GLchar*>(log.data()));
            std::fprintf(stderr, "CadRendererGL: shader link error:\n%s\n",
                         log.data());
        }
        glue->glDeleteObjectARB(prog);
        return 0;
    }
    if (cadLightDebugRequested()) {
        const GLint positionLocation =
            glue->glGetAttribLocationARB(prog, "a_pos");
        const GLint normalLocation =
            glue->glGetAttribLocationARB(prog, "a_norm");
        const GLint colorLocation =
            glue->glGetAttribLocationARB(prog, "a_color");
        std::fprintf(stderr,
            "CadRendererGL linked program=%u attributes={pos=%d norm=%d "
            "color=%d}\n",
            static_cast<unsigned int>(prog), positionLocation,
            normalLocation, colorLocation);
        GLint activeUniforms = 0;
        glue->glGetObjectParameterivARB(
            prog, GL_OBJECT_ACTIVE_UNIFORMS_ARB, &activeUniforms);
        for (GLint i = 0; i < activeUniforms; ++i) {
            OBOL_GLchar name[128] = {};
            GLsizei length = 0;
            GLint size = 0;
            GLenum type = 0;
            glue->glGetActiveUniformARB(
                prog, static_cast<GLuint>(i), sizeof(name), &length,
                &size, &type, name);
            const GLint location = glue->glGetUniformLocationARB(prog, name);
            std::fprintf(stderr,
                "CadRendererGL program=%u uniform[%d]={name=%s loc=%d "
                "size=%d type=0x%x}\n",
                static_cast<unsigned int>(prog), i,
                reinterpret_cast<const char *>(name), location, size,
                static_cast<unsigned int>(type));
        }
    }

    // Detach shaders – they are no longer needed after linking
    glue->glDetachObjectARB(prog, vs);
    glue->glDetachObjectARB(prog, fs);

    return static_cast<GLuint>(prog);
}

bool CadRendererGL::compileAllShaders(const SoGLContext* glue)
{
    // --- Tier-1 shaders ---
    {
        GLuint vs = compileShader(glue, GL_VERTEX_SHADER_ARB, kWireVS1);
        GLuint fs = compileShader(glue, GL_FRAGMENT_SHADER_ARB, kWireFS1);
        if (!vs || !fs) {
            if (vs) glue->glDeleteObjectARB(vs);
            if (fs) glue->glDeleteObjectARB(fs);
            return false;
        }
        shaders_.wire = linkProgram(glue, vs, fs);
        glue->glDeleteObjectARB(vs);
        glue->glDeleteObjectARB(fs);
        if (!shaders_.wire) return false;
    }
    {
        GLuint vs = compileShader(glue, GL_VERTEX_SHADER_ARB, kWirePopVS1);
        GLuint fs = compileShader(glue, GL_FRAGMENT_SHADER_ARB, kWireFS1);
        if (!vs || !fs) {
            if (vs) glue->glDeleteObjectARB(vs);
            if (fs) glue->glDeleteObjectARB(fs);
            return false;
        }
        shaders_.wirePop = linkProgram(glue, vs, fs);
        glue->glDeleteObjectARB(vs);
        glue->glDeleteObjectARB(fs);
        if (!shaders_.wirePop) return false;
    }
    {
        GLuint vs = compileShader(glue, GL_VERTEX_SHADER_ARB, kProxyPointVS1);
        GLuint fs = compileShader(glue, GL_FRAGMENT_SHADER_ARB, kWireFS1);
        if (!vs || !fs) {
            if (vs) glue->glDeleteObjectARB(vs);
            if (fs) glue->glDeleteObjectARB(fs);
            return false;
        }
        shaders_.proxyPoint = linkProgram(glue, vs, fs);
        glue->glDeleteObjectARB(vs);
        glue->glDeleteObjectARB(fs);
        if (!shaders_.proxyPoint) return false;
    }
    {
        GLuint vs = compileShader(glue, GL_VERTEX_SHADER_ARB, kShadedVS1);
        const char *fragmentSource = kShadedFS1;
        if (const char *mode = cadShaderDebugMode()) {
            if (std::strcmp(mode, "face") == 0)
                fragmentSource = kShadedFaceDebugFS1;
            else if (std::strcmp(mode, "normal") == 0)
                fragmentSource = kShadedNormalDebugFS1;
        }
        GLuint fs = compileShader(
            glue, GL_FRAGMENT_SHADER_ARB, fragmentSource);
        if (!vs || !fs) {
            if (vs) glue->glDeleteObjectARB(vs);
            if (fs) glue->glDeleteObjectARB(fs);
            return false;
        }
        shaders_.shaded = linkProgram(glue, vs, fs);
        glue->glDeleteObjectARB(vs);
        glue->glDeleteObjectARB(fs);
        if (!shaders_.shaded) return false;
    }
    {
        GLuint vs = compileShader(glue, GL_VERTEX_SHADER_ARB, kShadedPopVS1);
        const char *fragmentSource = kShadedFS1;
        if (const char *mode = cadShaderDebugMode()) {
            if (std::strcmp(mode, "face") == 0)
                fragmentSource = kShadedFaceDebugFS1;
            else if (std::strcmp(mode, "normal") == 0)
                fragmentSource = kShadedNormalDebugFS1;
        }
        GLuint fs = compileShader(
            glue, GL_FRAGMENT_SHADER_ARB, fragmentSource);
        if (!vs || !fs) {
            if (vs) glue->glDeleteObjectARB(vs);
            if (fs) glue->glDeleteObjectARB(fs);
            return false;
        }
        shaders_.shadedPop = linkProgram(glue, vs, fs);
        glue->glDeleteObjectARB(vs);
        glue->glDeleteObjectARB(fs);
        if (!shaders_.shadedPop) return false;
    }
    if (!cadShaderDebugMode()) {
        const auto compileDirectionalPair =
            [this, glue](const char *normalVertexSource,
                         const char *faceVertexSource,
                         GLuint& normalProgram,
                         GLuint& faceProgram) {
                GLuint normalVs = compileShader(
                    glue, GL_VERTEX_SHADER_ARB, normalVertexSource);
                GLuint faceVs = compileShader(
                    glue, GL_VERTEX_SHADER_ARB, faceVertexSource);
                GLuint normalFs = compileShader(
                    glue, GL_FRAGMENT_SHADER_ARB,
                    kShadedDirectionalNormFS1);
                GLuint faceFs = compileShader(
                    glue, GL_FRAGMENT_SHADER_ARB,
                    kShadedDirectionalFaceFS1);
                if (normalVs && normalFs)
                    normalProgram = linkProgram(
                        glue, normalVs, normalFs);
                if (faceVs && faceFs)
                    faceProgram = linkProgram(glue, faceVs, faceFs);
                if (normalVs) glue->glDeleteObjectARB(normalVs);
                if (faceVs) glue->glDeleteObjectARB(faceVs);
                if (normalFs) glue->glDeleteObjectARB(normalFs);
                if (faceFs) glue->glDeleteObjectARB(faceFs);
                if (!normalProgram || !faceProgram) {
                    if (normalProgram)
                        glue->glDeleteObjectARB(normalProgram);
                    if (faceProgram)
                        glue->glDeleteObjectARB(faceProgram);
                    normalProgram = 0;
                    faceProgram = 0;
                }
            };
        compileDirectionalPair(
            kShadedVS1, kShadedFaceVS1,
            shaders_.shadedDirectionalNorm,
            shaders_.shadedDirectionalFace);
        compileDirectionalPair(
            kShadedPopVS1, kShadedPopFaceVS1,
            shaders_.shadedPopDirectionalNorm,
            shaders_.shadedPopDirectionalFace);
    }

    // --- Tier-2 shaders (only when instancing is available) ---
    if (caps_.canUseInstanced()) {
        {
            GLuint vs = compileShader(glue, GL_VERTEX_SHADER_ARB, kWireVS2);
            GLuint fs = compileShader(glue, GL_FRAGMENT_SHADER_ARB, kWireFS2);
            if (vs && fs) {
                shaders_.wireInst = linkProgram(glue, vs, fs);
            }
            if (vs) glue->glDeleteObjectARB(vs);
            if (fs) glue->glDeleteObjectARB(fs);
        }
        {
            GLuint vs = compileShader(
                glue, GL_VERTEX_SHADER_ARB, kWirePopVS2);
            GLuint fs = compileShader(glue, GL_FRAGMENT_SHADER_ARB, kWireFS2);
            if (vs && fs) {
                shaders_.wirePopInst = linkProgram(glue, vs, fs);
            }
            if (vs) glue->glDeleteObjectARB(vs);
            if (fs) glue->glDeleteObjectARB(fs);
        }
        {
            GLuint vs = compileShader(glue, GL_VERTEX_SHADER_ARB, kShadedVS2);
            GLuint fs = compileShader(glue, GL_FRAGMENT_SHADER_ARB, kShadedFS2);
            if (vs && fs) {
                shaders_.shadedInst = linkProgram(glue, vs, fs);
            }
            if (vs) glue->glDeleteObjectARB(vs);
            if (fs) glue->glDeleteObjectARB(fs);
        }
        {
            GLuint vs = compileShader(
                glue, GL_VERTEX_SHADER_ARB, kShadedPopVS2);
            GLuint fs = compileShader(glue, GL_FRAGMENT_SHADER_ARB, kShadedFS2);
            if (vs && fs) {
                shaders_.shadedPopInst = linkProgram(glue, vs, fs);
            }
            if (vs) glue->glDeleteObjectARB(vs);
            if (fs) glue->glDeleteObjectARB(fs);
        }
        // If instanced shaders failed, fall back to Tier-1
        if (!shaders_.wireInst || !shaders_.wirePopInst ||
                !shaders_.shadedInst || !shaders_.shadedPopInst) {
            if (shaders_.wireInst)   glue->glDeleteObjectARB(shaders_.wireInst);
            if (shaders_.wirePopInst)
                glue->glDeleteObjectARB(shaders_.wirePopInst);
            if (shaders_.shadedInst) glue->glDeleteObjectARB(shaders_.shadedInst);
            if (shaders_.shadedPopInst)
                glue->glDeleteObjectARB(shaders_.shadedPopInst);
            shaders_.wireInst   = 0;
            shaders_.wirePopInst = 0;
            shaders_.shadedInst = 0;
            shaders_.shadedPopInst = 0;
        }
    }
    if (caps_.canUseIndirect()) {
        GLuint vs = compileShader(
            glue, GL_VERTEX_SHADER_ARB, kShadedIndirectVS2);
        GLuint fs = compileShader(
            glue, GL_FRAGMENT_SHADER_ARB, kShadedIndirectFS2);
        if (vs && fs)
            shaders_.shadedIndirect = linkProgram(glue, vs, fs);
        if (vs) glue->glDeleteObjectARB(vs);
        if (fs) glue->glDeleteObjectARB(fs);
    }

    return true;
}

// ---------------------------------------------------------------------------
// ensureReady()
// ---------------------------------------------------------------------------

bool CadRendererGL::ensureReady(const SoGLContext* glue)
{
    if (!glue) return false;

    if (!gpuRes_ || gpuContextId_ != glue->contextid) {
        auto &gpu = gpuResources_[glue->contextid];
        if (!gpu) gpu = std::make_unique<CadGpuResources>();
        gpuRes_ = gpu.get();
        gpuContextId_ = glue->contextid;
    }

    if (!capsDetected_ || shadersContextId_ != glue->contextid) {
        std::call_once(sharedCadShadersCallbackOnce, []() {
            SoContextHandler::addContextDestructionCallback(
                releaseSharedCadShaders, nullptr);
        });

        std::lock_guard<std::mutex> guard(sharedCadShadersMutex);
        const auto found = sharedCadShaders.find(glue->contextid);
        if (found != sharedCadShaders.end()) {
            caps_ = found->second.caps;
            shaders_.wire = found->second.wire;
            shaders_.wirePop = found->second.wirePop;
            shaders_.proxyPoint = found->second.proxyPoint;
            shaders_.shaded = found->second.shaded;
            shaders_.shadedPop = found->second.shadedPop;
            shaders_.shadedDirectionalNorm =
                found->second.shadedDirectionalNorm;
            shaders_.shadedDirectionalFace =
                found->second.shadedDirectionalFace;
            shaders_.shadedPopDirectionalNorm =
                found->second.shadedPopDirectionalNorm;
            shaders_.shadedPopDirectionalFace =
                found->second.shadedPopDirectionalFace;
            shaders_.wireInst = found->second.wireInst;
            shaders_.wirePopInst = found->second.wirePopInst;
            shaders_.shadedInst = found->second.shadedInst;
            shaders_.shadedPopInst = found->second.shadedPopInst;
            shaders_.shadedIndirect = found->second.shadedIndirect;
        } else {
            caps_ = CadGLCaps::detect(glue);
            shaders_ = ShaderPrograms();
            // Shader programs are used by both retained rendering tiers.  If
            // the capability probe rejects GLSL drawing, render() falls back
            // to immediate mode and these programs remain harmless.
            if (caps_.hasShaderObjects &&
                    (!caps_.isSoftwareRenderer || softwareGlslRequested()))
                compileAllShaders(glue);
            SharedCadShaders programs;
            programs.caps = caps_;
            programs.wire = shaders_.wire;
            programs.wirePop = shaders_.wirePop;
            programs.proxyPoint = shaders_.proxyPoint;
            programs.shaded = shaders_.shaded;
            programs.shadedPop = shaders_.shadedPop;
            programs.shadedDirectionalNorm =
                shaders_.shadedDirectionalNorm;
            programs.shadedDirectionalFace =
                shaders_.shadedDirectionalFace;
            programs.shadedPopDirectionalNorm =
                shaders_.shadedPopDirectionalNorm;
            programs.shadedPopDirectionalFace =
                shaders_.shadedPopDirectionalFace;
            programs.wireInst = shaders_.wireInst;
            programs.wirePopInst = shaders_.wirePopInst;
            programs.shadedInst = shaders_.shadedInst;
            programs.shadedPopInst = shaders_.shadedPopInst;
            programs.shadedIndirect = shaders_.shadedIndirect;
            sharedCadShaders.emplace(glue->contextid, programs);
        }
        capsDetected_ = true;
        shadersContextId_ = glue->contextid;
    }
    return true;
}

// ---------------------------------------------------------------------------
// ensurePartUploaded()
// ---------------------------------------------------------------------------

void CadRendererGL::ensurePartUploaded(
        PartId pid, const SoCADAssembly& assembly, uint64_t gen,
        uint8_t requestedLod, const SoGLContext* glue)
{
    // Retrieve part geometry from the assembly
    const Obol::PartGeometry* geom = assembly.partGeometry(pid);
    if (!geom) return;
    const bool progressive =
        (geom->wire.has_value() && geom->wire->isProgressive()) ||
        (geom->shaded.has_value() && geom->shaded->isProgressive());

    // Ordinary geometry has no view-dependent upload size, so retain its
    // original constant-time generation fast path.
    if (!progressive && gpuRes_->isUpToDate(pid, gen))
        return;

    const float* pPointPos = nullptr;
    GLsizei pointCount = 0;
    if (geom->points.has_value()) {
        pPointPos = packedVec3fData(geom->points->positions);
        pointCount = static_cast<GLsizei>(geom->points->positions.size());
    }

    // Build CPU-side flat arrays for indexed wire geometry.  Pure flat
    // segment-pair input can be uploaded directly from WireRep::segmentPoints.
    std::vector<float>    wirePos;
    std::vector<uint32_t> wireSegIdx;
    const float*          pWirePos = nullptr;
    const uint32_t*       pWireSeg = nullptr;
    GLsizei               wirePointCount = 0;
    GLsizei               wireSegIdxCount = 0;
    if (geom->wire.has_value()) {
        const auto& wr = *geom->wire;
        const size_t flatPointCount =
            (wr.isProgressive() ?
                wr.segmentCountAtLevel(requestedLod) :
                wr.segmentCount()) * 2;
        if (flatPointCount > 0 && wr.polylines.empty()) {
            pWirePos = packedVec3fData(wr.segmentPoints);
            wirePointCount = static_cast<GLsizei>(flatPointCount);
        } else {
            size_t polyPointCount = 0;
            size_t polySegmentCount = 0;
            for (const auto& poly : wr.polylines) {
                polyPointCount += poly.points.size();
                if (poly.points.size() >= 2)
                    polySegmentCount += poly.points.size() - 1;
            }
            wirePos.reserve((flatPointCount + polyPointCount) * 3);
            wireSegIdx.reserve(flatPointCount + polySegmentCount * 2);

            for (size_t i = 0; i + 1 < flatPointCount; i += 2) {
                const uint32_t base =
                    static_cast<uint32_t>(wirePos.size() / 3);
                appendPackedPoint(wirePos, wr.segmentPoints[i]);
                appendPackedPoint(wirePos, wr.segmentPoints[i + 1]);
                wireSegIdx.push_back(base);
                wireSegIdx.push_back(base + 1);
            }

            for (const auto& poly : wr.polylines) {
                if (poly.points.size() < 2) continue;
                const uint32_t base =
                    static_cast<uint32_t>(wirePos.size() / 3);
                for (const auto& pt : poly.points)
                    appendPackedPoint(wirePos, pt);
                for (uint32_t i = 0; i + 1 < poly.points.size(); ++i) {
                    wireSegIdx.push_back(base + i);
                    wireSegIdx.push_back(base + i + 1);
                }
            }

            pWirePos = wirePos.empty() ? nullptr : wirePos.data();
            pWireSeg = wireSegIdx.empty() ? nullptr : wireSegIdx.data();
            wirePointCount = static_cast<GLsizei>(wirePos.size() / 3);
            wireSegIdxCount = static_cast<GLsizei>(wireSegIdx.size());
        }
    }

    // Triangle mesh vectors already use packed SbVec3f storage.
    const float*    pTriPos = nullptr;
    const float*    pTriNorm = nullptr;
    const uint32_t* pTriIdx = nullptr;
    GLsizei         triPosCount = 0;
    GLsizei         triIdxCount = 0;
    if (geom->shaded.has_value()) {
        const auto& mesh = *geom->shaded;
        size_t requiredIndexCount = mesh.isProgressive() ?
            mesh.indexCountAtLevel(requestedLod) : mesh.indices.size();
        size_t requiredPositionCount = mesh.isProgressive() ?
            mesh.positionCountAtLevel(requestedLod) : mesh.positions.size();
        if (requiredIndexCount > 0 && requiredPositionCount == 0)
            return;
        pTriPos = packedVec3fData(mesh.positions);
        pTriNorm = packedVec3fData(mesh.normals);
        pTriIdx = mesh.indices.empty() ? nullptr : mesh.indices.data();
        triPosCount = static_cast<GLsizei>(requiredPositionCount);
        triIdxCount = static_cast<GLsizei>(requiredIndexCount);
    }

    /*
     * A richer cumulative prefix already on the GPU remains valid when the
     * view asks for less.  Never shrink the retained ordinary buffers merely
     * to enter interaction; only defer appending newly resident tails.
     */
    if (progressive) {
        if (const CadWireGpu *wire = gpuRes_->wireFor(pid)) {
            wirePointCount = std::max(wirePointCount, wire->vertCount);
            wireSegIdxCount = std::max(wireSegIdxCount, wire->idxCount);
        }
        if (const CadTriGpu *tri = gpuRes_->triFor(pid)) {
            triPosCount = std::max(triPosCount, tri->vertCount);
            triIdxCount = std::max(triIdxCount, tri->idxCount);
        }
    }
    if (gpuRes_->isUpToDate(
            pid, gen, wirePointCount, wireSegIdxCount,
            triPosCount, triIdxCount))
        return;

    gpuRes_->upload(pid,
                   pPointPos, pointCount,
                   pWirePos,  wirePointCount,
                   pWireSeg,  wireSegIdxCount,
                   pTriPos,   triPosCount,
                   pTriNorm,
                   pTriIdx,   triIdxCount,
                   gen,
                   progressive,
                   glue, caps_);
}

// ---------------------------------------------------------------------------
// render() – top-level entry point
// ---------------------------------------------------------------------------

// Extract six frustum half-space planes from the OI viewProj matrix.
//
// OI convention: p_clip = p_world * VP  (row vector, row-major matrix)
// where VP[r][c] = row r, col c.
//
// The six clip-space half-spaces (inside when ≥ 0):
//   Left:   x + w ≥ 0  →  col0 + col3
//   Right: -x + w ≥ 0  → -col0 + col3
//   Bottom: y + w ≥ 0  →  col1 + col3
//   Top:   -y + w ≥ 0  → -col1 + col3
//   Near:   z + w ≥ 0  →  col2 + col3
//   Far:   -z + w ≥ 0  → -col2 + col3
//
// Returns planes as float[6][4] where p[i] = {a,b,c,d}:
//   inside if a*x + b*y + c*z + d >= 0
struct FrustumPlanes { float planes[6][4]; };

static FrustumPlanes extractFrustumPlanes(const SbMatrix& vp) noexcept
{
    FrustumPlanes fp;
    for (int col = 0; col < 3; ++col) {
        for (int sg = 0; sg < 2; ++sg) {
            int   planeIndex = col * 2 + sg;
            float sign       = (sg == 0) ? 1.0f : -1.0f;
            fp.planes[planeIndex][0] = sign * vp[0][col] + vp[0][3];
            fp.planes[planeIndex][1] = sign * vp[1][col] + vp[1][3];
            fp.planes[planeIndex][2] = sign * vp[2][col] + vp[2][3];
            fp.planes[planeIndex][3] = sign * vp[3][col] + vp[3][3];
        }
    }
    return fp;
}

// Returns true when the AABB [wbMin,wbMax] is completely outside at least one
// frustum half-space and can therefore be safely skipped.
static bool isBoxOutsideFrustum(const float wbMin[3], const float wbMax[3],
                                 const FrustumPlanes& fp) noexcept
{
    for (int i = 0; i < 6; ++i) {
        // Positive vertex: corner with the maximum distance to the plane.
        // The box is wholly outside only when even this corner is outside.
        float px = (fp.planes[i][0] < 0.0f) ? wbMin[0] : wbMax[0];
        float py = (fp.planes[i][1] < 0.0f) ? wbMin[1] : wbMax[1];
        float pz = (fp.planes[i][2] < 0.0f) ? wbMin[2] : wbMax[2];
        if (fp.planes[i][0]*px + fp.planes[i][1]*py + fp.planes[i][2]*pz + fp.planes[i][3] < 0.0f)
            return true; // Completely outside this plane.
    }
    return false;
}

struct CadWireRasterState {
    GLfloat lineWidth = 1.0f;
    GLboolean stippleEnabled = GL_FALSE;
    GLint stipplePattern = 0xffff;
    GLint stippleFactor = 1;
};

static CadWireRasterState captureWireRasterState(
        const SoGLContext *glue, bool hasLineStipple)
{
    CadWireRasterState state;
    glue->glGetFloatv(GL_LINE_WIDTH, &state.lineWidth);
    if (hasLineStipple) {
        state.stippleEnabled = glue->glIsEnabled(GL_LINE_STIPPLE);
        glue->glGetIntegerv(GL_LINE_STIPPLE_PATTERN, &state.stipplePattern);
        glue->glGetIntegerv(GL_LINE_STIPPLE_REPEAT, &state.stippleFactor);
    }
    return state;
}

static void applyWireRasterStyle(
        const SoGLContext *glue,
        const Obol::internal::CadVisibleInstance& inst,
        bool hasLineStipple)
{
    glue->glLineWidth(std::max(1.0f, inst.lineWidth));
    if (!hasLineStipple)
        return;
    if (inst.linePattern != 0xffffu) {
        glue->glLineStipple(std::max<GLint>(1, inst.linePatternFactor),
                            inst.linePattern);
        glue->glEnable(GL_LINE_STIPPLE);
    } else {
        glue->glDisable(GL_LINE_STIPPLE);
    }
}

static void restoreWireRasterState(
        const SoGLContext *glue,
        const CadWireRasterState& state,
        bool hasLineStipple)
{
    glue->glLineWidth(state.lineWidth);
    if (!hasLineStipple)
        return;
    glue->glLineStipple(state.stippleFactor,
                        static_cast<GLushort>(state.stipplePattern));
    if (state.stippleEnabled)
        glue->glEnable(GL_LINE_STIPPLE);
    else
        glue->glDisable(GL_LINE_STIPPLE);
}

static uint8_t maximumRequestedLod(
        const CadFramePlan& plan, const SoCADAssembly& assembly, PartId part)
{
    const auto found = plan.maximumRequestedLodByPart.find(part);
    return found != plan.maximumRequestedLodByPart.end() ?
        assembly.effectiveProgressiveLodLevel(found->second) : 15;
}

void CadRendererGL::renderPoints(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const SoGLContext* glue,
        const SbMatrix& viewProj,
        const std::unordered_map<PartId, uint64_t,
                                 std::hash<PartId>>& partGenMap)
{
    if (plan.pointItems.empty()) return;

    for (const CadDrawItem& item : plan.pointItems) {
        auto generation = partGenMap.find(item.rep.part);
        ensurePartUploaded(item.rep.part, assembly,
            generation != partGenMap.end() ? generation->second : 0,
            15, glue);
    }

    GLfloat savedPointSize = 1.0f;
    glue->glGetFloatv(GL_POINT_SIZE, &savedPointSize);
    const FrustumPlanes fp = extractFrustumPlanes(viewProj);
    const bool fixedFunction =
        (caps_.isSoftwareRenderer && !softwareGlslRequested()) ||
        !shaders_.wire;

    if (fixedFunction) {
        glue->glMatrixMode(GL_PROJECTION);
        glue->glPushMatrix();
        glue->glLoadIdentity();
        glue->glMatrixMode(GL_MODELVIEW);
        glue->glPushMatrix();
        const GLboolean wasLighting = glue->glIsEnabled(GL_LIGHTING);
        glue->glDisable(GL_LIGHTING);
        glue->glEnableClientState(GL_VERTEX_ARRAY);

        for (const CadDrawItem& item : plan.pointItems) {
            const PartGeometry* geometry = assembly.partGeometry(item.rep.part);
            const CadPointGpu* gpu = gpuRes_->pointFor(item.rep.part);
            if (!geometry || !geometry->points) continue;
            const PointRep& points = *geometry->points;
            const GLsizei pointCount = static_cast<GLsizei>(
                points.positions.size());
            if (pointCount <= 0) continue;
            const bool pointColors = points.colors.size() == points.positions.size() &&
                points.colorValid.size() == points.positions.size();
            if (gpu) {
                glue->glBindBuffer(GL_ARRAY_BUFFER, gpu->posBuf);
                glue->glVertexPointer(3, GL_FLOAT, 3 * sizeof(float), nullptr);
            }

            for (uint32_t i = 0; i < item.instanceCount; ++i) {
                const size_t instanceIndex = item.baseInstance + i;
                if (!drawItemOwnsInstance(plan, item, instanceIndex) ||
                        isHiddenInstance(plan, instanceIndex))
                    continue;
                const CadVisibleInstance& inst =
                    plan.visibleInstances[instanceIndex];
                if (isBoxOutsideFrustum(inst.wbMin, inst.wbMax, fp)) continue;
                SbMatrix model;
                model.setValue(inst.transform.data());
                SbMatrix mvp = model;
                mvp.multRight(viewProj);
                glue->glLoadMatrixf(mvp[0]);
                const bool usePointColors = pointColors && !(inst.flags & 5u);
                if (gpu && !usePointColors) {
                    glue->glColor4ub(inst.rgba[0], inst.rgba[1],
                                     inst.rgba[2], inst.rgba[3]);
                    glue->glPointSize(std::max(1.0f, inst.lineWidth));
                    glue->glDrawArrays(GL_POINTS, 0, pointCount);
                    continue;
                }
                glue->glPointSize(std::max(1.0f, inst.lineWidth));
                if (!gpu) glue->glBegin(GL_POINTS);
                for (GLsizei p = 0; p < pointCount; ++p) {
                    if (usePointColors && points.colorValid[p]) {
                        const SbColor& color = points.colors[p];
                        glue->glColor4f(color[0], color[1], color[2],
                                        inst.rgba[3] / 255.0f);
                    } else {
                        glue->glColor4ub(inst.rgba[0], inst.rgba[1],
                                         inst.rgba[2], inst.rgba[3]);
                    }
                    if (gpu)
                        glue->glDrawArrays(GL_POINTS, p, 1);
                    else {
                        const SbVec3f& point = points.positions[p];
                        glue->glVertex3f(point[0], point[1], point[2]);
                    }
                }
                if (!gpu) glue->glEnd();
            }
        }

        glue->glDisableClientState(GL_VERTEX_ARRAY);
        glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
        if (wasLighting) glue->glEnable(GL_LIGHTING);
        glue->glMatrixMode(GL_MODELVIEW);
        glue->glPopMatrix();
        glue->glMatrixMode(GL_PROJECTION);
        glue->glPopMatrix();
        glue->glMatrixMode(GL_MODELVIEW);
    } else {
        glue->glUseProgramObjectARB(shaders_.wire);
        const GLint locVP = glue->glGetUniformLocationARB(shaders_.wire,
                                                           "u_viewProj");
        const GLint locModel = glue->glGetUniformLocationARB(shaders_.wire,
                                                              "u_model");
        const GLint locColor = glue->glGetUniformLocationARB(shaders_.wire,
                                                              "u_color");
        GLint locPos = glue->glGetAttribLocationARB(shaders_.wire, "a_pos");
        if (locPos < 0) locPos = 0;
        glue->glUniformMatrix4fvARB(locVP, 1, GL_FALSE, viewProj[0]);

        for (const CadDrawItem& item : plan.pointItems) {
            const PartGeometry* geometry = assembly.partGeometry(item.rep.part);
            const CadPointGpu* gpu = gpuRes_->pointFor(item.rep.part);
            if (!geometry || !geometry->points || !gpu) continue;
            const PointRep& points = *geometry->points;
            const bool pointColors = points.colors.size() == points.positions.size() &&
                points.colorValid.size() == points.positions.size();
            if (gpu->vao && glue->glBindVertexArray) {
                glue->glBindVertexArray(gpu->vao);
            } else {
                glue->glBindBuffer(GL_ARRAY_BUFFER, gpu->posBuf);
                glue->glVertexAttribPointerARB(static_cast<GLuint>(locPos), 3,
                    GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
                glue->glEnableVertexAttribArrayARB(static_cast<GLuint>(locPos));
            }

            for (uint32_t i = 0; i < item.instanceCount; ++i) {
                const size_t instanceIndex = item.baseInstance + i;
                if (!drawItemOwnsInstance(plan, item, instanceIndex) ||
                        isHiddenInstance(plan, instanceIndex))
                    continue;
                const CadVisibleInstance& inst =
                    plan.visibleInstances[instanceIndex];
                if (isBoxOutsideFrustum(inst.wbMin, inst.wbMax, fp)) continue;
                glue->glUniformMatrix4fvARB(locModel, 1, GL_FALSE,
                                            inst.transform.data());
                const bool usePointColors = pointColors && !(inst.flags & 5u);
                if (!usePointColors) {
                    const float color[4] = {
                        inst.rgba[0] / 255.0f, inst.rgba[1] / 255.0f,
                        inst.rgba[2] / 255.0f, inst.rgba[3] / 255.0f};
                    glue->glUniform4fvARB(locColor, 1, color);
                    glue->glPointSize(std::max(1.0f, inst.lineWidth));
                    glue->glDrawArrays(GL_POINTS, 0, gpu->count);
                    continue;
                }
                for (GLsizei p = 0; p < gpu->count; ++p) {
                    float color[4] = {
                        inst.rgba[0] / 255.0f, inst.rgba[1] / 255.0f,
                        inst.rgba[2] / 255.0f, inst.rgba[3] / 255.0f};
                    if (usePointColors && points.colorValid[p]) {
                        color[0] = points.colors[p][0];
                        color[1] = points.colors[p][1];
                        color[2] = points.colors[p][2];
                    }
                    glue->glUniform4fvARB(locColor, 1, color);
                    glue->glPointSize(std::max(1.0f, inst.lineWidth));
                    glue->glDrawArrays(GL_POINTS, p, 1);
                }
            }
            if (gpu->vao && glue->glBindVertexArray)
                glue->glBindVertexArray(0);
            else {
                glue->glDisableVertexAttribArrayARB(static_cast<GLuint>(locPos));
                glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
            }
        }
        glue->glUseProgramObjectARB(0);
    }
    glue->glPointSize(savedPointSize);
}

bool CadRendererGL::isSubpixelProxyInstance(
        const CadFramePlan& plan, size_t visibleInstanceIndex)
{
    return visibleInstanceIndex < plan.subpixelProxyMask.size() &&
        plan.subpixelProxyMask[visibleInstanceIndex] != 0;
}

bool CadRendererGL::isHiddenInstance(
        const CadFramePlan& plan, size_t instanceIndex)
{
    return instanceIndex >= plan.visibleInstances.size() ||
        (plan.visibleInstances[instanceIndex].flags &
            CadInstanceHidden) != 0;
}

bool CadRendererGL::wireRepHasUncollapsedInstances(
        const CadFramePlan& plan, PartId part)
{
    return plan.wirePartsWithUncollapsedInstances.find(part) !=
        plan.wirePartsWithUncollapsedInstances.end();
}

void CadRendererGL::renderSubpixelProxyPoints(
        const CadFramePlan& plan, const SoGLContext* glue,
        const SbMatrix& viewProj)
{
    const std::vector<CadSubpixelProxyPoint>& pressurePoints =
        pressureProxyPoints();
    const auto pointVisible = [](const CadSubpixelProxyPoint& point) {
        return !(point.flags & CadInstanceHidden);
    };
    const bool includePlan = std::any_of(
        plan.subpixelProxyPoints.begin(),
        plan.subpixelProxyPoints.end(), pointVisible);
    const bool includePressure = std::any_of(
        pressurePoints.begin(), pressurePoints.end(), pointVisible);
    if (!includePlan && !includePressure)
        return;

    const bool useVbo = caps_.canUseVbo();
    const bool fixedFunction =
        (caps_.isSoftwareRenderer && !softwareGlslRequested()) ||
        !shaders_.proxyPoint || !useVbo;
    GLfloat savedPointSize = 1.0f;
    glue->glGetFloatv(GL_POINT_SIZE, &savedPointSize);
    glue->glPointSize(1.0f);

    if (!useVbo) {
        glue->glMatrixMode(GL_PROJECTION);
        glue->glPushMatrix();
        glue->glLoadIdentity();
        glue->glMatrixMode(GL_MODELVIEW);
        glue->glPushMatrix();
        glue->glLoadMatrixf(viewProj[0]);
        const GLboolean wasLighting = glue->glIsEnabled(GL_LIGHTING);
        glue->glDisable(GL_LIGHTING);
        glue->glBegin(GL_POINTS);
        for (const CadSubpixelProxyPoint& point : plan.subpixelProxyPoints) {
            if (!pointVisible(point))
                continue;
            glue->glColor4ub(point.rgba[0], point.rgba[1], point.rgba[2],
                             point.rgba[3]);
            glue->glVertex3f(point.position[0], point.position[1],
                             point.position[2]);
        }
        for (const CadSubpixelProxyPoint& point : pressurePoints) {
            if (!pointVisible(point))
                continue;
            glue->glColor4ub(point.rgba[0], point.rgba[1], point.rgba[2],
                             point.rgba[3]);
            glue->glVertex3f(point.position[0], point.position[1],
                             point.position[2]);
        }
        glue->glEnd();
        if (wasLighting) glue->glEnable(GL_LIGHTING);
        glue->glMatrixMode(GL_MODELVIEW);
        glue->glPopMatrix();
        glue->glMatrixMode(GL_PROJECTION);
        glue->glPopMatrix();
        glue->glMatrixMode(GL_MODELVIEW);
        glue->glPointSize(savedPointSize);
        return;
    }

    const auto packPointRange =
        [&](const std::vector<CadSubpixelProxyPoint>& points,
            size_t begin, size_t end,
            std::vector<float>& positions,
            std::vector<uint8_t>& colors) {
        begin = std::min(begin, points.size());
        end = std::min(end, points.size());
        positions.clear();
        colors.clear();
        positions.reserve((end - begin) * 3u);
        colors.reserve((end - begin) * 4u);
        for (size_t i = begin; i < end; ++i) {
            const CadSubpixelProxyPoint& point = points[i];
            if (!pointVisible(point))
                continue;
            appendPackedPoint(positions, point.position);
            colors.insert(
                colors.end(), point.rgba.begin(), point.rgba.end());
        }
    };

    uint64_t planUploadRevision = plan.subpixelProxyRevision;
    /*
     * Aggregate points retain stable slots across selection, style, and
     * visibility changes.  Mix the attribute revision so changed colors and
     * hidden flags refresh this compact batch without reclassifying every
     * occurrence.
     */
    planUploadRevision ^= plan.instanceAttributeRevision +
        UINT64_C(0x9e3779b97f4a7c15) +
        (planUploadRevision << 6) + (planUploadRevision >> 2);
    if (!planUploadRevision)
        planUploadRevision = 1;

    const CadSubpixelProxyGpu& cachedPlan =
        gpuRes_->subpixelProxyPoints();
    if (includePlan &&
            (cachedPlan.revision != planUploadRevision ||
             !cachedPlan.posBuf || !cachedPlan.colorBuf)) {
        std::vector<float> positions;
        std::vector<uint8_t> colors;
        packPointRange(
            plan.subpixelProxyPoints, 0,
            plan.subpixelProxyPoints.size(), positions, colors);
        if (!positions.empty())
            gpuRes_->uploadSubpixelProxyPoints(
                planUploadRevision, positions, colors, glue, caps_);
    }

    const CadSubpixelProxyGpu& cachedPressure =
        gpuRes_->pressureProxyPoints();
    if (includePressure &&
            (cachedPressure.revision != pressureProxyRevision_ ||
             !cachedPressure.posBuf || !cachedPressure.colorBuf)) {
        std::vector<float> positions;
        std::vector<uint8_t> colors;
        bool appended = false;
        if (pressureProxyAppendOnly_ &&
                pressureProxyAppendBegin_ <= pressurePoints.size() &&
                pressureProxyAppendBegin_ <=
                    static_cast<size_t>(
                        std::numeric_limits<GLsizei>::max()) &&
                cachedPressure.revision ==
                    pressureProxyAppendBaseRevision_ &&
                cachedPressure.count ==
                    static_cast<GLsizei>(
                        pressureProxyAppendBegin_)) {
            packPointRange(
                pressurePoints, pressureProxyAppendBegin_,
                pressurePoints.size(), positions, colors);
            /*
             * A hidden tail would make packed GPU slots diverge from source
             * slots, so conservatively refresh the complete stream in that
             * uncommon presentation-update case.
             */
            if (positions.size() / 3u ==
                    pressurePoints.size() -
                        pressureProxyAppendBegin_) {
                appended = gpuRes_->appendPressureProxyPoints(
                    pressureProxyAppendBaseRevision_,
                    pressureProxyRevision_,
                    positions, colors, glue);
            }
        }
        if (!appended) {
            packPointRange(
                pressurePoints, 0, pressurePoints.size(),
                positions, colors);
            if (!positions.empty())
                gpuRes_->uploadPressureProxyPoints(
                    pressureProxyRevision_,
                    positions, colors, glue, caps_);
        }
        if (gpuRes_->pressureProxyPoints().revision ==
                pressureProxyRevision_)
            pressureProxyAppendOnly_ = false;
    }

    const CadSubpixelProxyGpu *streams[2] = {nullptr, nullptr};
    size_t streamCount = 0;
    const CadSubpixelProxyGpu& planGpu =
        gpuRes_->subpixelProxyPoints();
    if (includePlan && planGpu.posBuf && planGpu.colorBuf &&
            planGpu.count > 0)
        streams[streamCount++] = &planGpu;
    const CadSubpixelProxyGpu& pressureGpu =
        gpuRes_->pressureProxyPoints();
    if (includePressure && pressureGpu.posBuf &&
            pressureGpu.colorBuf && pressureGpu.count > 0)
        streams[streamCount++] = &pressureGpu;
    if (!streamCount) {
        glue->glPointSize(savedPointSize);
        return;
    }

    if (fixedFunction) {
        glue->glMatrixMode(GL_PROJECTION);
        glue->glPushMatrix();
        glue->glLoadIdentity();
        glue->glMatrixMode(GL_MODELVIEW);
        glue->glPushMatrix();
        glue->glLoadMatrixf(viewProj[0]);
        const GLboolean wasLighting = glue->glIsEnabled(GL_LIGHTING);
        glue->glDisable(GL_LIGHTING);
        glue->glEnableClientState(GL_VERTEX_ARRAY);
        glue->glEnableClientState(GL_COLOR_ARRAY);
        for (size_t i = 0; i < streamCount; ++i) {
            const CadSubpixelProxyGpu& gpu = *streams[i];
            glue->glBindBuffer(GL_ARRAY_BUFFER, gpu.posBuf);
            glue->glVertexPointer(
                3, GL_FLOAT, 3 * sizeof(float), nullptr);
            glue->glBindBuffer(GL_ARRAY_BUFFER, gpu.colorBuf);
            glue->glColorPointer(
                4, GL_UNSIGNED_BYTE, 4 * sizeof(uint8_t), nullptr);
            glue->glDrawArrays(GL_POINTS, 0, gpu.count);
        }
        glue->glDisableClientState(GL_COLOR_ARRAY);
        glue->glDisableClientState(GL_VERTEX_ARRAY);
        glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
        if (wasLighting) glue->glEnable(GL_LIGHTING);
        glue->glMatrixMode(GL_MODELVIEW);
        glue->glPopMatrix();
        glue->glMatrixMode(GL_PROJECTION);
        glue->glPopMatrix();
        glue->glMatrixMode(GL_MODELVIEW);
    } else {
        glue->glUseProgramObjectARB(shaders_.proxyPoint);
        const GLint locVP = glue->glGetUniformLocationARB(shaders_.proxyPoint,
                                                           "u_viewProj");
        glue->glUniformMatrix4fvARB(locVP, 1, GL_FALSE, viewProj[0]);
        for (size_t i = 0; i < streamCount; ++i) {
            const CadSubpixelProxyGpu& gpu = *streams[i];
            if (gpu.vao && glue->glBindVertexArray) {
                glue->glBindVertexArray(gpu.vao);
            } else {
                glue->glBindBuffer(GL_ARRAY_BUFFER, gpu.posBuf);
                glue->glVertexAttribPointerARB(
                    0, 3, GL_FLOAT, GL_FALSE,
                    3 * sizeof(float), nullptr);
                glue->glEnableVertexAttribArrayARB(0);
                glue->glBindBuffer(GL_ARRAY_BUFFER, gpu.colorBuf);
                glue->glVertexAttribPointerARB(
                    1, 4, GL_UNSIGNED_BYTE, GL_TRUE,
                    4 * sizeof(uint8_t), nullptr);
                glue->glEnableVertexAttribArrayARB(1);
            }
            glue->glDrawArrays(GL_POINTS, 0, gpu.count);
            if (gpu.vao && glue->glBindVertexArray) {
                glue->glBindVertexArray(0);
            } else {
                glue->glDisableVertexAttribArrayARB(1);
                glue->glDisableVertexAttribArrayARB(0);
                glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
            }
        }
        glue->glUseProgramObjectARB(0);
    }
    glue->glPointSize(savedPointSize);
}

void CadRendererGL::render(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const SoGLContext*   glue,
        const SbMatrix&      viewProj,
        const SbMatrix&      viewMatrix,
        const SbMatrix&      projectionMatrix,
        const SbViewVolume&  viewVolume,
        const std::unordered_map<PartId, uint64_t,
                                 std::hash<PartId>>& partGenMap)
{
    using RenderClock = std::chrono::steady_clock;
    const bool renderTimingEnabled =
        std::getenv("OBOL_CAD_RENDER_TIMING") != nullptr;
    const auto renderStarted = renderTimingEnabled ?
        RenderClock::now() : RenderClock::time_point();
    // Pressure proxies describe only the current camera and atlas admission
    // result.  Never let an early return expose a prior frame's replacements.
    pressureProxyPointsView_ = nullptr;
    pressureProxyPoints_.clear();
    lastRenderedTriangleCount_ = 0;
    lastRenderUsedPreparedReplay_ = false;
    if (!ensureReady(glue)) return;
    if (plan.visibleInstances.empty()) return;
    CadGpuTimerGuard gpuTimer(
        gpuRes_, glue, &lastRenderedTriangleCount_);
    gpuRes_->beginProgressiveFrame();
    gpuRes_->beginTriangleAtlasFrame();

    CadDirectGLStateGuard directState(
        glue, caps_.hasVBO,
        caps_.hasVAO && glue->glBindVertexArray != nullptr,
        caps_.hasMultiDrawIndirect);

    // SoCADAssembly has synchronized Coin's lazy shape state before entering
    // this direct-GL renderer.  Keep all internal cull changes local and
    // restore the raw state on every return path.
    CadCullRasterGuard cullRaster(glue);

    const char *flatShadedEnv = std::getenv("OBOL_CAD_FLAT_SHADED");
    const bool flatShadedEnabled = flatShadedEnv ?
        flatShadedEnv[0] != '0' : true;
    const bool canUseFlatShaded = caps_.isSoftwareRenderer ?
        caps_.canUseFixedVbo() : (caps_.canUseVbo() && shaders_.shaded);
    const bool hiddenLine =
        assembly.drawMode.getValue() == SoCADAssembly::HIDDEN_LINE;
    const bool retainedProgressive = assembly.hasProgressivePartLod();
    const bool progressiveWireShaderReady =
        !retainedProgressive || shaders_.wirePop;
    const bool progressiveShadedShaderReady =
        !retainedProgressive || shaders_.shadedPop;
    const bool progressiveWireInstShaderReady =
        !retainedProgressive || shaders_.wirePopInst;
    const bool progressiveShadedInstShaderReady =
        !retainedProgressive || shaders_.shadedPopInst;
    const bool useFlatShaded = flatShadedEnabled && canUseFlatShaded &&
        (hiddenLine || plan.shadedItems.size() >= 128);

    renderPoints(plan, assembly, glue, viewProj, partGenMap);
    const auto pointsCompleted = renderTimingEnabled ?
        RenderClock::now() : RenderClock::time_point();

    if (hiddenLine) {
        if (useFlatShaded) {
            SoGLContext_glColorMask(glue, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            SoGLContext_glEnable(glue, GL_POLYGON_OFFSET_FILL);
            SoGLContext_glPolygonOffset(glue, 1.0f, 1.0f);
            const bool depthRendered = renderFlatShaded(
                plan, assembly, glue, viewProj, viewMatrix, projectionMatrix,
                true);
            SoGLContext_glDisable(glue, GL_POLYGON_OFFSET_FILL);
            SoGLContext_glColorMask(glue, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            const bool triangleEdgesRendered = depthRendered &&
                renderFlatTriangleEdges(plan, glue, viewProj, viewMatrix,
                                        projectionMatrix);
            const bool explicitWireRendered = plan.wireItems.empty() ||
                renderFlatWire(plan, assembly, glue, viewProj);
            if (triangleEdgesRendered && explicitWireRendered) {
                lastRenderTier_ = 3;
                renderSubpixelProxyPoints(plan, glue, viewProj);
                gpuRes_->endTriangleAtlasFrame(glue);
                gpuRes_->endProgressiveFrame(glue);
                return;
            }
        }

        // The per-part fallback needs retained representations.
        for (const auto& repKey : plan.requiredReps) {
            if (repKey.type == CadRepType::WireSegments &&
                    !wireRepHasUncollapsedInstances(plan, repKey.part))
                continue;
            auto genIt = partGenMap.find(repKey.part);
            uint64_t gen = (genIt != partGenMap.end()) ? genIt->second : 0;
            ensurePartUploaded(
                repKey.part, assembly, gen,
                maximumRequestedLod(plan, assembly, repKey.part), glue);
        }
        SoGLContext_glColorMask(glue, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        SoGLContext_glEnable(glue, GL_POLYGON_OFFSET_FILL);
        SoGLContext_glPolygonOffset(glue, 1.0f, 1.0f);
        if (caps_.canUseVbo() && shaders_.shaded &&
                progressiveShadedShaderReady) {
            renderVboLoop(
                plan, assembly, glue, viewProj, viewVolume,
                false, false, true);
        } else if (caps_.canUseFixedVbo()) {
            renderFixedVboLoop(plan, assembly, glue, viewProj, viewMatrix,
                               projectionMatrix, false, true);
        } else {
            renderImmediateMode(plan, assembly, glue, viewProj, viewMatrix,
                                projectionMatrix, false, true);
        }
        SoGLContext_glDisable(glue, GL_POLYGON_OFFSET_FILL);
        SoGLContext_glColorMask(glue, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

        if (caps_.canUseInstanced() && shaders_.wireInst &&
                progressiveWireInstShaderReady) {
            lastRenderTier_ = 2;
            renderInstanced(plan, assembly, glue, viewProj, viewVolume,
                            partGenMap,
                            true, false, false);
        } else if (caps_.canUseVbo() && shaders_.wire &&
                progressiveWireShaderReady) {
            lastRenderTier_ = 1;
            renderVboLoop(
                plan, assembly, glue, viewProj, viewVolume,
                true, false, false);
        } else if (caps_.canUseFixedVbo()) {
            lastRenderTier_ = 1;
            renderFixedVboLoop(plan, assembly, glue, viewProj, viewMatrix,
                               projectionMatrix, true, false);
        } else {
            lastRenderTier_ = 0;
            renderImmediateMode(plan, assembly, glue, viewProj, viewMatrix,
                                projectionMatrix, true, false);
        }
        renderSubpixelProxyPoints(plan, glue, viewProj);
        gpuRes_->endTriangleAtlasFrame(glue);
        gpuRes_->endProgressiveFrame(glue);
        return;
    }

    const char *indirectEnv = std::getenv("OBOL_CAD_INDIRECT");
    const bool indirectEnabled =
        !indirectEnv || indirectEnv[0] != '0';
    if (!indirectStatusReported_ &&
            std::getenv("OBOL_CAD_INDIRECT_DEBUG") &&
            plan.shadedItems.size() >= 128) {
        std::fprintf(stderr,
            "CadRendererGL indirect status context=%u items=%zu "
            "enabled=%d capability=%d function=%d shader=%u\n",
            glue->contextid, plan.shadedItems.size(),
            indirectEnabled ? 1 : 0,
            caps_.canUseIndirect() ? 1 : 0,
            glue->glMultiDrawElementsIndirect ? 1 : 0,
            static_cast<unsigned int>(shaders_.shadedIndirect));
        indirectStatusReported_ = true;
    }
    /*
     * Shaded CAD may be followed by a coplanar wire presentation from this
     * assembly or a separate source later in the Coin traversal.  All other
     * shaded tiers bias their depth behind that overlay; the indirect tier
     * used to omit the bias, producing view-dependent dark pixels and
     * apparently chopped surfaces wherever the two representations fought
     * for the same depth values.
     */
    const GLboolean indirectPolygonOffsetWasEnabled =
        glue->glIsEnabled(GL_POLYGON_OFFSET_FILL);
    if (!indirectPolygonOffsetWasEnabled) {
        SoGLContext_glEnable(glue, GL_POLYGON_OFFSET_FILL);
        SoGLContext_glPolygonOffset(glue, 1.0f, 1.0f);
    }
    const bool indirectShadedRendered =
        indirectEnabled && plan.shadedItems.size() >= 128 &&
        renderIndirectShaded(
            plan, assembly, glue, viewProj, viewVolume);
    if (!indirectPolygonOffsetWasEnabled)
        SoGLContext_glDisable(glue, GL_POLYGON_OFFSET_FILL);
    const auto shadedCompleted = renderTimingEnabled ?
        RenderClock::now() : RenderClock::time_point();
    if (indirectShadedRendered) {
        bool wireRendered = plan.wireItems.empty();
        if (!wireRendered) {
            const char *flatWireEnv = std::getenv("OBOL_CAD_FLAT_WIRE");
            const bool flatWireEnabled =
                !flatWireEnv || flatWireEnv[0] != '0';
            const bool canUseFlatWire = caps_.isSoftwareRenderer ?
                caps_.canUseFixedVbo() :
                (caps_.canUseVbo() && shaders_.wire);
            size_t wireInstanceCount = 0;
            for (const CadDrawItem& item : plan.wireItems)
                wireInstanceCount +=
                    drawItemLiveInstanceCount(plan, item);
            const bool preferFlatWire = caps_.isSoftwareRenderer ?
                wireInstanceCount >= 128 : plan.wireItems.size() >= 128;
            wireRendered = flatWireEnabled && preferFlatWire &&
                canUseFlatWire &&
                renderFlatWire(plan, assembly, glue, viewProj);
        }
        if (!wireRendered) {
            for (const CadRepKey& repKey : plan.requiredReps) {
                if (repKey.type != CadRepType::WireSegments ||
                        !wireRepHasUncollapsedInstances(
                            plan, repKey.part))
                    continue;
                const auto generation = partGenMap.find(repKey.part);
                ensurePartUploaded(
                    repKey.part, assembly,
                    generation == partGenMap.end() ?
                        0 : generation->second,
                    maximumRequestedLod(
                        plan, assembly, repKey.part),
                    glue);
            }
            if (caps_.canUseInstanced() && shaders_.wireInst &&
                    progressiveWireInstShaderReady) {
                renderInstanced(
                    plan, assembly, glue, viewProj, viewVolume,
                    partGenMap, true, false, false);
            } else if (caps_.canUseVbo() && shaders_.wire &&
                    progressiveWireShaderReady) {
                renderVboLoop(
                    plan, assembly, glue, viewProj, viewVolume,
                    true, false, false);
            } else if (caps_.canUseFixedVbo()) {
                renderFixedVboLoop(
                    plan, assembly, glue, viewProj, viewMatrix,
                    projectionMatrix, true, false);
            } else {
                renderImmediateMode(
                    plan, assembly, glue, viewProj, viewMatrix,
                    projectionMatrix, true, false);
            }
        }
        const auto wireCompleted = renderTimingEnabled ?
            RenderClock::now() : RenderClock::time_point();
        lastRenderTier_ = 6;
        renderSubpixelProxyPoints(plan, glue, viewProj);
        gpuRes_->endTriangleAtlasFrame(glue);
        gpuRes_->endProgressiveFrame(glue);
        if (renderTimingEnabled) {
            const auto completed = RenderClock::now();
            const auto milliseconds = [](auto begin, auto end) {
                return std::chrono::duration<double, std::milli>(
                    end - begin).count();
            };
            const double total =
                milliseconds(renderStarted, completed);
            if (total >= 10.0)
                std::fprintf(stderr,
                    "CadRendererGL retained frame total=%.3fms "
                    "points=%.3fms shaded=%.3fms wire=%.3fms "
                    "proxy-maintenance=%.3fms "
                    "source_instances=%zu wire_items=%zu "
                    "wire_uncollapsed_parts=%zu proxies=%zu\n",
                    total,
                    milliseconds(renderStarted, pointsCompleted),
                    milliseconds(pointsCompleted, shadedCompleted),
                    milliseconds(shadedCompleted, wireCompleted),
                    milliseconds(wireCompleted, completed),
                    plan.visibleInstances.size(),
                    plan.wireItems.size(),
                    plan.wirePartsWithUncollapsedInstances.size(),
                    plan.subpixelProxyPoints.size() +
                        pressureProxyPoints().size());
        }
        return;
    }

    bool flatShadedRendered = false;
    if (useFlatShaded) {
        const GLboolean polygonOffsetWasEnabled =
            glue->glIsEnabled(GL_POLYGON_OFFSET_FILL);
        if (!polygonOffsetWasEnabled) {
            SoGLContext_glEnable(glue, GL_POLYGON_OFFSET_FILL);
            SoGLContext_glPolygonOffset(glue, 1.0f, 1.0f);
        }
        flatShadedRendered = renderFlatShaded(
            plan, assembly, glue, viewProj, viewMatrix, projectionMatrix,
            false);
        if (!polygonOffsetWasEnabled)
            SoGLContext_glDisable(glue, GL_POLYGON_OFFSET_FILL);
    }
    if (flatShadedRendered) {
        if (plan.wireItems.empty() ||
                renderFlatWire(plan, assembly, glue, viewProj)) {
            lastRenderTier_ = 4;
            renderSubpixelProxyPoints(plan, glue, viewProj);
            gpuRes_->endTriangleAtlasFrame(glue);
            gpuRes_->endProgressiveFrame(glue);
            return;
        }
    }

    /*
     * Wire and shaded work are independent presentation channels.  In
     * particular, thousands of conservative fallback boxes must remain one
     * flat world-space batch even after the first few retained PoP meshes
     * appear in the shaded channel.  The old all-or-nothing gate disabled
     * this path as soon as any progressive part existed, turning a 50k-leaf
     * cold scene back into tens of thousands of VBO uploads and draw calls.
     */
    const char *flatWireEnv = std::getenv("OBOL_CAD_FLAT_WIRE");
    const bool flatWireEnabled = flatWireEnv ? flatWireEnv[0] != '0' : true;
    const bool canUseFlatWire = caps_.isSoftwareRenderer ?
        caps_.canUseFixedVbo() : (caps_.canUseVbo() && shaders_.wire);
    size_t wireInstanceCount = 0;
    for (const CadDrawItem& item : plan.wireItems)
        wireInstanceCount += drawItemLiveInstanceCount(plan, item);
    /*
     * Hardware should prefer true instancing when many AABBs share the unit
     * cube part.  OSMesa's fixed-function software path is faster with one
     * flattened world-space buffer, so its threshold is occurrence count
     * rather than unique-part count.
     */
    const bool preferFlatWire = caps_.isSoftwareRenderer ?
        wireInstanceCount >= 128 : plan.wireItems.size() >= 128;
    const bool flatWireRendered = flatWireEnabled &&
        preferFlatWire && canUseFlatWire &&
        renderFlatWire(plan, assembly, glue, viewProj);

    // Upload only the representations still needed by the per-part paths.
    for (const auto& repKey : plan.requiredReps) {
        if (repKey.type == CadRepType::WireSegments &&
                (flatWireRendered ||
                 !wireRepHasUncollapsedInstances(plan, repKey.part)))
            continue;
        auto genIt = partGenMap.find(repKey.part);
        uint64_t gen = (genIt != partGenMap.end()) ? genIt->second : 0;
        ensurePartUploaded(
            repKey.part, assembly, gen,
            maximumRequestedLod(plan, assembly, repKey.part), glue);
    }

    /* Keep shaded polygons fractionally behind wire geometry.  A wire source
     * may be a separate assembly from its shaded peer, so relying on a local
     * shaded-with-edges mode leaves coplanar overlays at the mercy of depth
     * rounding. */
    const GLboolean polygonOffsetWasEnabled =
        glue->glIsEnabled(GL_POLYGON_OFFSET_FILL);
    if (!plan.shadedItems.empty() && !polygonOffsetWasEnabled) {
        SoGLContext_glEnable(glue, GL_POLYGON_OFFSET_FILL);
        SoGLContext_glPolygonOffset(glue, 1.0f, 1.0f);
    }

    if (flatWireRendered && plan.shadedItems.empty()) {
        lastRenderTier_ = 3;
    } else if (caps_.isSoftwareRenderer && !softwareGlslRequested() &&
            caps_.canUseFixedVbo()) {
        lastRenderTier_ = 1;
        renderFixedVboLoop(plan, assembly, glue, viewProj, viewMatrix,
                           projectionMatrix, !flatWireRendered, true);
    } else if (caps_.canUseInstanced() &&
            shaders_.wireInst && shaders_.shadedInst &&
            progressiveWireInstShaderReady &&
            progressiveShadedInstShaderReady) {
        lastRenderTier_ = 2;
        renderInstanced(plan, assembly, glue, viewProj, viewVolume, partGenMap,
                        !flatWireRendered, false, true);
    } else if (caps_.canUseVbo() && shaders_.wire &&
            progressiveWireShaderReady && progressiveShadedShaderReady) {
        lastRenderTier_ = 1;
        renderVboLoop(plan, assembly, glue, viewProj, viewVolume,
                      !flatWireRendered, false, true);
    } else if (caps_.canUseFixedVbo()) {
        lastRenderTier_ = 1;
        renderFixedVboLoop(plan, assembly, glue, viewProj, viewMatrix,
                           projectionMatrix, !flatWireRendered, true);
    } else {
        lastRenderTier_ = 0;
        renderImmediateMode(plan, assembly, glue, viewProj, viewMatrix,
                            projectionMatrix, !flatWireRendered, true);
    }
    if (flatWireRendered && !plan.shadedItems.empty())
        lastRenderTier_ = 5;

    if (!plan.shadedItems.empty() && !polygonOffsetWasEnabled)
        SoGLContext_glDisable(glue, GL_POLYGON_OFFSET_FILL);
    renderSubpixelProxyPoints(plan, glue, viewProj);
    gpuRes_->endTriangleAtlasFrame(glue);
    gpuRes_->endProgressiveFrame(glue);
}

namespace {

struct FlatWireStyleKey {
    uint8_t priority = 0;
    uint32_t rgba = 0;
    uint32_t widthBits = 0;
    uint16_t pattern = 0xffffu;
    uint16_t factor = 1u;

    bool operator<(const FlatWireStyleKey& other) const noexcept {
        if (priority != other.priority) return priority < other.priority;
        if (rgba != other.rgba) return rgba < other.rgba;
        if (widthBits != other.widthBits) return widthBits < other.widthBits;
        if (pattern != other.pattern) return pattern < other.pattern;
        return factor < other.factor;
    }

    bool operator==(const FlatWireStyleKey& other) const noexcept {
        return priority == other.priority &&
               rgba == other.rgba &&
               widthBits == other.widthBits &&
               pattern == other.pattern &&
               factor == other.factor;
    }
};

struct FlatWireStyleKeyHash {
    size_t operator()(const FlatWireStyleKey& key) const noexcept {
        size_t value = static_cast<size_t>(key.rgba);
        const auto mix = [&value](size_t component) {
            value ^= component + static_cast<size_t>(0x9e3779b9u) +
                     (value << 6) + (value >> 2);
        };
        mix(static_cast<size_t>(key.widthBits));
        mix(static_cast<size_t>(key.pattern) |
            (static_cast<size_t>(key.factor) << 16));
        mix(static_cast<size_t>(key.priority));
        return value;
    }
};

static FlatWireStyleKey flatWireStyleKey(const CadVisibleInstance& inst)
{
    FlatWireStyleKey key;
    // Draw hover/selection emphasis after ordinary geometry at equal depth.
    key.priority = static_cast<uint8_t>(inst.flags & 3u);
    key.rgba = static_cast<uint32_t>(inst.rgba[0]) |
               (static_cast<uint32_t>(inst.rgba[1]) << 8) |
               (static_cast<uint32_t>(inst.rgba[2]) << 16) |
               (static_cast<uint32_t>(inst.rgba[3]) << 24);
    std::memcpy(&key.widthBits, &inst.lineWidth, sizeof(key.widthBits));
    key.pattern = inst.linePattern;
    key.factor = inst.linePatternFactor;
    return key;
}

static void writeTransformedFlatPoint(
        std::vector<float>& positions,
        size_t& offset,
        const SbVec3f& point,
        const std::array<float, 16>& matrix)
{
    const float x = point[0];
    const float y = point[1];
    const float z = point[2];
    positions[offset++] = x * matrix[0] + y * matrix[4] +
                          z * matrix[8] + matrix[12];
    positions[offset++] = x * matrix[1] + y * matrix[5] +
                          z * matrix[9] + matrix[13];
    positions[offset++] = x * matrix[2] + y * matrix[6] +
                          z * matrix[10] + matrix[14];
}

static SbVec3f transformedFlatPoint(
        const SbVec3f& point,
        const std::array<float, 16>& matrix)
{
    const float x = point[0];
    const float y = point[1];
    const float z = point[2];
    return SbVec3f(x * matrix[0] + y * matrix[4] + z * matrix[8] + matrix[12],
                   x * matrix[1] + y * matrix[5] + z * matrix[9] + matrix[13],
                   x * matrix[2] + y * matrix[6] + z * matrix[10] + matrix[14]);
}

static uint32_t flatRgbaKey(const CadVisibleInstance& inst)
{
    return static_cast<uint32_t>(inst.rgba[0]) |
           (static_cast<uint32_t>(inst.rgba[1]) << 8) |
           (static_cast<uint32_t>(inst.rgba[2]) << 16) |
           (static_cast<uint32_t>(inst.rgba[3]) << 24);
}

} // namespace

static uint8_t progressiveLevel(
    uint8_t requested, uint8_t minimum, uint8_t resident);
static SbVec3f progressiveSnapPoint(
    const SbVec3f& point, const SbVec3f& minimum,
    const SbVec3f& maximum, uint8_t level);

bool CadRendererGL::renderFlatWire(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const SoGLContext* glue,
        const SbMatrix& viewProj)
{
    constexpr size_t maxPositionBytes = 256u * 1024u * 1024u;
    constexpr size_t progressiveGrowthReserve = 16u;
    const size_t maxVertexCount =
        maxPositionBytes / (3 * sizeof(float));
    const uint64_t presentationRevision = plan.subpixelProxyRevision ?
        plan.subpixelProxyRevision : plan.revision;
    const FrustumPlanes fp = extractFrustumPlanes(viewProj);
    struct Occurrence {
        const Obol::WireRep *wire = nullptr;
        const CadVisibleInstance *instance = nullptr;
        CadFlatWireRangeKey rangeKey;
        size_t flatSegments = 0;
        size_t polylineSegments = 0;
        uint8_t level = 15;
        FlatWireStyleKey style;
        uint32_t styleBucket = 0;
        size_t visibleIndex = 0;
        CadFlatWireRange range;
        bool rangeValid = false;
        bool rangeCachePending = false;
    };

    /*
     * Retain world-space ranges across structural publication batches.  A
     * 50k-leaf warm start otherwise re-expanded and re-uploaded every box
     * after each 64..512 occurrence merge, making total work quadratic.
     */
    std::vector<Occurrence> occurrences;
    occurrences.reserve(plan.visibleInstances.size());
    size_t visibleVertexCount = 0;
    for (const CadDrawItem& item : plan.wireItems) {
        if (item.partIndex >= plan.partBindings.size())
            continue;
        const CadPartBinding& binding =
            plan.partBindings[item.partIndex];
        const Obol::PartGeometry *geom = binding.geometry.get();
        if (!geom || !geom->wire.has_value()) continue;
        const Obol::WireRep& wire = *geom->wire;
        size_t polylineSegments = 0;
        for (const Obol::WirePolyline& poly : wire.polylines)
            if (poly.points.size() >= 2)
                polylineSegments += poly.points.size() - 1;
        for (uint32_t ii = 0; ii < item.instanceCount; ++ii) {
            const size_t visibleIndex = item.baseInstance + ii;
            if (!drawItemOwnsInstance(plan, item, visibleIndex) ||
                    isHiddenInstance(plan, visibleIndex) ||
                    isSubpixelProxyInstance(plan, visibleIndex))
                continue;
            const CadVisibleInstance& instance =
                plan.visibleInstances[visibleIndex];
            if (isBoxOutsideFrustum(instance.wbMin, instance.wbMax, fp))
                continue;
            const uint8_t effectiveLevel =
                assembly.effectiveProgressiveLodLevel(instance.lodLevel);
            const uint8_t level = progressiveLevel(
                effectiveLevel, wire.progressiveMinimumLevel,
                wire.progressiveResidentLevel);
            const size_t flatSegments =
                wire.segmentCountAtLevel(effectiveLevel);
            const size_t segments = flatSegments + polylineSegments;
            if (segments == 0)
                continue;
            const size_t vertices = segments * 2;
            if (vertices > maxVertexCount ||
                    visibleVertexCount > maxVertexCount - vertices)
                return false;
            visibleVertexCount += vertices;

            uint64_t geometryToken = 1469598103934665603ULL;
            geometryToken ^= binding.generation;
            geometryToken *= 1099511628211ULL;
            geometryToken ^= item.rep.part.w0;
            geometryToken *= 1099511628211ULL;
            geometryToken ^= item.rep.part.w1;
            geometryToken *= 1099511628211ULL;
            for (float value : instance.transform) {
                uint32_t bits = 0;
                std::memcpy(&bits, &value, sizeof(bits));
                geometryToken ^= bits;
                geometryToken *= 1099511628211ULL;
            }

            Occurrence occurrence;
            occurrence.wire = &wire;
            occurrence.instance = &instance;
            occurrence.rangeKey = CadFlatWireRangeKey{
                instance.instanceId, level, geometryToken};
            occurrence.flatSegments = flatSegments;
            occurrence.polylineSegments = polylineSegments;
            occurrence.level = level;
            occurrence.style = flatWireStyleKey(instance);
            occurrence.visibleIndex = visibleIndex;
            occurrence.rangeValid = gpuRes_->lookupFlatWireRange(
                visibleIndex, occurrence.rangeKey, &occurrence.range);
            occurrences.push_back(occurrence);
        }
    }

    /*
     * Structural fallback leaves often share one wire style.  Keep their
     * retained-plan order in that case.  For mixed colors or selection
     * emphasis, group in linear time and sort only the distinct style keys.
     * Sorting the full, relatively large Occurrence record made every
     * incremental publication frame O(N log N) in the number of remaining
     * boxes.
     */
    const bool uniformStyle = occurrences.empty() ||
        std::all_of(
            occurrences.begin() + 1, occurrences.end(),
            [&occurrences](const Occurrence& occurrence) {
                const FlatWireStyleKey& first = occurrences.front().style;
                return occurrence.style == first;
            });
    std::vector<uint32_t> occurrenceOrder;
    if (!uniformStyle) {
        std::unordered_map<FlatWireStyleKey, uint32_t,
                           FlatWireStyleKeyHash> bucketByStyle;
        std::vector<FlatWireStyleKey> bucketStyles;
        std::vector<size_t> bucketCounts;
        bucketByStyle.reserve(std::min<size_t>(
            occurrences.size(), 4096u));
        bucketStyles.reserve(std::min<size_t>(
            occurrences.size(), 4096u));
        bucketCounts.reserve(std::min<size_t>(
            occurrences.size(), 4096u));
        for (Occurrence& occurrence : occurrences) {
            const uint32_t candidate =
                static_cast<uint32_t>(bucketStyles.size());
            const auto inserted =
                bucketByStyle.emplace(occurrence.style, candidate);
            const uint32_t bucket = inserted.first->second;
            if (inserted.second) {
                bucketStyles.push_back(occurrence.style);
                bucketCounts.push_back(0);
            }
            occurrence.styleBucket = bucket;
            ++bucketCounts[bucket];
        }

        std::vector<uint32_t> bucketOrder(bucketStyles.size());
        for (size_t i = 0; i < bucketOrder.size(); ++i)
            bucketOrder[i] = static_cast<uint32_t>(i);
        std::sort(
            bucketOrder.begin(), bucketOrder.end(),
            [&bucketStyles](uint32_t left, uint32_t right) {
                return bucketStyles[left] < bucketStyles[right];
            });

        std::vector<size_t> bucketWrite(bucketStyles.size());
        size_t offset = 0;
        for (const uint32_t bucket : bucketOrder) {
            bucketWrite[bucket] = offset;
            offset += bucketCounts[bucket];
        }
        occurrenceOrder.resize(occurrences.size());
        for (size_t i = 0; i < occurrences.size(); ++i) {
            const uint32_t bucket = occurrences[i].styleBucket;
            occurrenceOrder[bucketWrite[bucket]++] =
                static_cast<uint32_t>(i);
        }
    }
    const auto& orderedOccurrence =
        [&occurrences, &occurrenceOrder](size_t index) -> Occurrence& {
            return occurrences[
                occurrenceOrder.empty() ?
                    index : occurrenceOrder[index]];
        };

    auto buildAtlasRanges = [&](
            bool onlyMissing, GLint baseVertex,
            std::vector<float>& positions,
            std::unordered_map<CadFlatWireRangeKey, CadFlatWireRange,
                               CadFlatWireRangeKeyHash>& ranges) {
        size_t appendVertexCount = 0;
        for (size_t i = 0; i < occurrences.size(); ++i) {
            const Occurrence& occurrence = orderedOccurrence(i);
            if (onlyMissing && occurrence.rangeValid)
                continue;
            const size_t vertices =
                (occurrence.flatSegments +
                 occurrence.polylineSegments) * 2;
            if (vertices > maxVertexCount ||
                    appendVertexCount > maxVertexCount - vertices)
                return false;
            appendVertexCount += vertices;
        }
        positions.clear();
        positions.resize(appendVertexCount * 3);
        ranges.clear();
        size_t positionOffset = 0;
        for (size_t i = 0; i < occurrences.size(); ++i) {
            Occurrence& occurrence = orderedOccurrence(i);
            if (onlyMissing && occurrence.rangeValid)
                continue;
            const Obol::WireRep& wire = *occurrence.wire;
            const CadVisibleInstance& inst = *occurrence.instance;
            const GLint first = baseVertex +
                static_cast<GLint>(positionOffset / 3);
            const size_t flatPointCount = occurrence.flatSegments * 2;
            for (size_t p = 0; p + 1 < flatPointCount; p += 2) {
                const SbVec3f a = wire.isProgressive() &&
                        occurrence.level < 15 ?
                    progressiveSnapPoint(wire.segmentPoints[p],
                        wire.progressiveQuantizationMinimum,
                        wire.progressiveQuantizationMaximum,
                        occurrence.level) : wire.segmentPoints[p];
                const SbVec3f b = wire.isProgressive() &&
                        occurrence.level < 15 ?
                    progressiveSnapPoint(wire.segmentPoints[p + 1],
                        wire.progressiveQuantizationMinimum,
                        wire.progressiveQuantizationMaximum,
                        occurrence.level) : wire.segmentPoints[p + 1];
                writeTransformedFlatPoint(
                    positions, positionOffset, a, inst.transform);
                writeTransformedFlatPoint(
                    positions, positionOffset, b, inst.transform);
            }
            for (const Obol::WirePolyline& poly : wire.polylines) {
                for (size_t p = 0; p + 1 < poly.points.size(); ++p) {
                    writeTransformedFlatPoint(
                        positions, positionOffset,
                        poly.points[p], inst.transform);
                    writeTransformedFlatPoint(
                        positions, positionOffset,
                        poly.points[p + 1], inst.transform);
                }
            }
            occurrence.range = CadFlatWireRange{
                first,
                static_cast<GLsizei>(
                    (occurrence.flatSegments +
                     occurrence.polylineSegments) * 2)};
            occurrence.rangeValid = true;
            occurrence.rangeCachePending = true;
            ranges.emplace(occurrence.rangeKey, occurrence.range);
        }
        return true;
    };

    bool rebuild = !gpuRes_->flatWire().posBuf;
    std::vector<float> positions;
    std::unordered_map<CadFlatWireRangeKey, CadFlatWireRange,
                       CadFlatWireRangeKeyHash> newRanges;
    if (!rebuild) {
        size_t missingVertexCount = 0;
        const CadFlatWireGpu& flat = gpuRes_->flatWire();
        for (const Occurrence& occurrence : occurrences) {
            if (occurrence.rangeValid)
                continue;
            const size_t vertices =
                (occurrence.flatSegments +
                 occurrence.polylineSegments) * 2;
            if (vertices > maxVertexCount ||
                    missingVertexCount > maxVertexCount - vertices)
                return false;
            missingVertexCount += vertices;
        }
        if (missingVertexCount) {
            if (flat.vertexCount >
                    flat.capacityVertexCount -
                        static_cast<GLsizei>(missingVertexCount)) {
                rebuild = true;
            } else if (!buildAtlasRanges(
                    true, flat.vertexCount, positions, newRanges) ||
                    !gpuRes_->appendFlatWire(
                        positions, newRanges, glue)) {
                return false;
            }
        }
    }
    if (rebuild) {
        if (occurrences.empty())
            return true;
        if (!buildAtlasRanges(false, 0, positions, newRanges))
            return false;
        const size_t vertexCount = positions.size() / 3;
        const size_t reserve = std::min(
            maxVertexCount,
            vertexCount <= maxVertexCount / progressiveGrowthReserve ?
                vertexCount * progressiveGrowthReserve : maxVertexCount);
        gpuRes_->uploadFlatWire(
            presentationRevision, plan.geometryRevision, positions,
            std::vector<CadFlatWireGroup>(), newRanges,
            static_cast<GLsizei>(reserve), glue, caps_);
    }
    for (Occurrence& occurrence : occurrences) {
        if (!occurrence.rangeCachePending)
            continue;
        if (!gpuRes_->lookupFlatWireRange(
                occurrence.visibleIndex, occurrence.rangeKey,
                &occurrence.range))
            return false;
        occurrence.rangeCachePending = false;
    }

    std::vector<CadFlatWireGroup> groups;
    FlatWireStyleKey activeKey;
    bool haveGroup = false;
    for (size_t i = 0; i < occurrences.size(); ++i) {
        const Occurrence& occurrence = orderedOccurrence(i);
        if (!occurrence.rangeValid)
            return false;
        const FlatWireStyleKey& key = occurrence.style;
        if (!haveGroup || !(key == activeKey)) {
            CadFlatWireGroup group;
            group.lineWidth = occurrence.instance->lineWidth;
            group.linePattern = occurrence.instance->linePattern;
            group.linePatternFactor =
                occurrence.instance->linePatternFactor;
            std::copy(occurrence.instance->rgba.begin(),
                      occurrence.instance->rgba.end(), group.rgba);
            groups.push_back(group);
            activeKey = key;
            haveGroup = true;
        }
        CadFlatWireGroup& group = groups.back();
        const GLint first = occurrence.range.first;
        const GLsizei count = occurrence.range.count;
        if (!group.firsts.empty() &&
                group.firsts.back() + group.counts.back() == first) {
            group.counts.back() += count;
        } else {
            group.firsts.push_back(first);
            group.counts.push_back(count);
        }
    }
    for (CadFlatWireGroup& group : groups) {
        if (group.firsts.empty()) continue;
        group.first = group.firsts.front();
        group.count = group.counts.front();
    }
    gpuRes_->updateFlatWireGroups(presentationRevision, groups);

    const CadFlatWireGpu& flat = gpuRes_->flatWire();
    if (occurrences.empty()) return true;
    if (!flat.posBuf || flat.groups.empty()) return false;

    const CadWireRasterState rasterState =
        captureWireRasterState(glue, caps_.hasLineStipple);
    const bool fixedFunction = caps_.isSoftwareRenderer;
    GLboolean wasLighting = GL_FALSE;
    GLint locColor = -1;
    GLint locPos = 0;
    if (fixedFunction) {
        glue->glMatrixMode(GL_PROJECTION);
        glue->glPushMatrix();
        glue->glLoadIdentity();
        glue->glMatrixMode(GL_MODELVIEW);
        glue->glPushMatrix();
        glue->glLoadMatrixf(viewProj[0]);
        wasLighting = glue->glIsEnabled(GL_LIGHTING);
        glue->glDisable(GL_LIGHTING);
        glue->glEnableClientState(GL_VERTEX_ARRAY);
        glue->glBindBuffer(GL_ARRAY_BUFFER, flat.posBuf);
        glue->glVertexPointer(3, GL_FLOAT, 3 * sizeof(float), nullptr);
    } else {
        glue->glUseProgramObjectARB(shaders_.wire);
    const GLint locVP = glue->glGetUniformLocationARB(shaders_.wire, "u_viewProj");
    const GLint locModel = glue->glGetUniformLocationARB(shaders_.wire, "u_model");
        locColor = glue->glGetUniformLocationARB(shaders_.wire, "u_color");
        locPos = glue->glGetAttribLocationARB(shaders_.wire, "a_pos");
    if (locPos < 0) locPos = 0;
    const SbMatrix identity = SbMatrix::identity();
    glue->glUniformMatrix4fvARB(locVP, 1, GL_FALSE, viewProj[0]);
    glue->glUniformMatrix4fvARB(locModel, 1, GL_FALSE, identity[0]);

    if (flat.vao && glue->glBindVertexArray) {
        glue->glBindVertexArray(flat.vao);
    } else {
        glue->glBindBuffer(GL_ARRAY_BUFFER, flat.posBuf);
        glue->glVertexAttribPointerARB(static_cast<GLuint>(locPos), 3,
                                       GL_FLOAT, GL_FALSE,
                                       3 * sizeof(float), nullptr);
        glue->glEnableVertexAttribArrayARB(static_cast<GLuint>(locPos));
    }
    }
    for (const CadFlatWireGroup& group : flat.groups) {
        if (fixedFunction) {
            glue->glColor4ub(group.rgba[0], group.rgba[1],
                             group.rgba[2], group.rgba[3]);
        } else {
            const float color[4] = {group.rgba[0] / 255.0f,
                                    group.rgba[1] / 255.0f,
                                    group.rgba[2] / 255.0f,
                                    group.rgba[3] / 255.0f};
            glue->glUniform4fvARB(locColor, 1, color);
        }
        glue->glLineWidth(std::max(1.0f, group.lineWidth));
        if (caps_.hasLineStipple) {
            if (group.linePattern != 0xffffu) {
                glue->glLineStipple(std::max<GLint>(1, group.linePatternFactor),
                                    group.linePattern);
                glue->glEnable(GL_LINE_STIPPLE);
            } else {
                glue->glDisable(GL_LINE_STIPPLE);
            }
        }
        if (group.firsts.empty()) {
            glue->glDrawArrays(GL_LINES, group.first, group.count);
        } else if (group.firsts.size() == 1) {
            glue->glDrawArrays(
                GL_LINES, group.firsts.front(), group.counts.front());
        } else if (glue->glMultiDrawArrays) {
            SoGLContext_glMultiDrawArrays(
                glue, GL_LINES, group.firsts.data(), group.counts.data(),
                static_cast<GLsizei>(group.firsts.size()));
        } else {
            for (size_t range = 0; range < group.firsts.size(); ++range) {
                glue->glDrawArrays(
                    GL_LINES, group.firsts[range], group.counts[range]);
            }
        }
    }
    if (fixedFunction) {
        glue->glDisableClientState(GL_VERTEX_ARRAY);
        glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
        if (wasLighting) glue->glEnable(GL_LIGHTING);
        glue->glMatrixMode(GL_MODELVIEW);
        glue->glPopMatrix();
        glue->glMatrixMode(GL_PROJECTION);
        glue->glPopMatrix();
        glue->glMatrixMode(GL_MODELVIEW);
    } else if (flat.vao && glue->glBindVertexArray)
        glue->glBindVertexArray(0);
    else {
        glue->glDisableVertexAttribArrayARB(static_cast<GLuint>(locPos));
        glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    if (!fixedFunction)
        glue->glUseProgramObjectARB(0);
    restoreWireRasterState(glue, rasterState, caps_.hasLineStipple);
    return true;
}

static void
drawFlatShadedGroup(const SoGLContext *glue, GLenum mode,
                    const CadFlatShadedGroup& group)
{
    if (group.firsts.empty()) {
        glue->glDrawArrays(mode, group.first, group.count);
        return;
    }
    if (group.firsts.size() == 1) {
        glue->glDrawArrays(
            mode, group.firsts.front(), group.counts.front());
        return;
    }
    if (glue->glMultiDrawArrays) {
        SoGLContext_glMultiDrawArrays(
            glue, mode, group.firsts.data(), group.counts.data(),
            static_cast<GLsizei>(group.firsts.size()));
        return;
    }
    for (size_t range = 0; range < group.firsts.size(); ++range)
        glue->glDrawArrays(mode, group.firsts[range], group.counts[range]);
}

bool CadRendererGL::renderFlatShaded(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const SoGLContext* glue,
        const SbMatrix& viewProj,
        const SbMatrix& viewMatrix,
        const SbMatrix& projectionMatrix,
        bool depthOnly)
{
    constexpr size_t maxVertexBytes = 512u * 1024u * 1024u;
    constexpr size_t floatsPerVertex = 6;
    const size_t maxVertexCount =
        maxVertexBytes / (floatsPerVertex * sizeof(float));
    const FrustumPlanes fp = extractFrustumPlanes(viewProj);

    struct Occurrence {
        const Obol::TriMesh *mesh = nullptr;
        const CadVisibleInstance *instance = nullptr;
        CadFlatShadedRangeKey rangeKey;
        size_t indexCount = 0;
        uint8_t level = 15;
        uint64_t styleKey = 0;
        bool cullBackfaces = false;
    };
    std::vector<Occurrence> occurrences;
    occurrences.reserve(plan.visibleInstances.size());
    size_t currentVertexCount = 0;
    for (const CadDrawItem& item : plan.shadedItems) {
        if (item.partIndex >= plan.partBindings.size())
            continue;
        const CadPartBinding& binding =
            plan.partBindings[item.partIndex];
        const Obol::PartGeometry *geom = binding.geometry.get();
        if (!geom || !geom->shaded.has_value()) continue;
        const Obol::TriMesh& mesh = *geom->shaded;
        for (uint32_t ii = 0; ii < item.instanceCount; ++ii) {
            const size_t visibleIndex = item.baseInstance + ii;
            if (!drawItemOwnsInstance(plan, item, visibleIndex) ||
                    isHiddenInstance(plan, visibleIndex) ||
                    isSubpixelProxyInstance(plan, visibleIndex))
                continue;
            const CadVisibleInstance& instance =
                plan.visibleInstances[visibleIndex];
            if (isBoxOutsideFrustum(instance.wbMin, instance.wbMax, fp))
                continue;
            const uint8_t requested =
                assembly.effectiveProgressiveLodLevel(instance.lodLevel);
            const uint8_t level = mesh.isProgressive() ?
                progressiveLevel(requested, mesh.progressiveMinimumLevel,
                                 mesh.progressiveResidentLevel) : 15;
            const size_t indexCount = mesh.isProgressive() ?
                mesh.indexCountAtLevel(level) : mesh.indices.size();
            if (indexCount < 3)
                continue;
            if (indexCount > maxVertexCount ||
                    currentVertexCount > maxVertexCount - indexCount)
                return false;
            currentVertexCount += indexCount;
            Occurrence occurrence;
            occurrence.mesh = &mesh;
            occurrence.instance = &instance;
            uint64_t geometryToken = 1469598103934665603ULL;
            geometryToken ^= binding.generation;
            geometryToken *= 1099511628211ULL;
            geometryToken ^= item.rep.part.w0;
            geometryToken *= 1099511628211ULL;
            geometryToken ^= item.rep.part.w1;
            geometryToken *= 1099511628211ULL;
            for (float value : instance.transform) {
                uint32_t bits = 0;
                std::memcpy(&bits, &value, sizeof(bits));
                geometryToken ^= bits;
                geometryToken *= 1099511628211ULL;
            }
            occurrence.rangeKey = CadFlatShadedRangeKey{
                instance.instanceId, level, geometryToken};
            occurrence.indexCount = indexCount;
            occurrence.level = level;
            occurrence.styleKey =
                static_cast<uint64_t>(flatRgbaKey(instance)) |
                (static_cast<uint64_t>(item.cullBackfaces) << 32);
            occurrence.cullBackfaces = item.cullBackfaces;
            occurrences.push_back(occurrence);
        }
    }

    /* Keep equal styles adjacent in both the atlas and the draw-range list.
     * A uniformly styled 50,000-leaf scene therefore collapses to one GL
     * call on its first frame rather than 50,000 calls. */
    std::sort(occurrences.begin(), occurrences.end(),
        [](const Occurrence& a, const Occurrence& b) {
            if (a.styleKey != b.styleKey)
                return a.styleKey < b.styleKey;
            return a.rangeKey.instance < b.rangeKey.instance;
        });

    auto buildAtlasRanges = [&](
            bool onlyMissing, GLint baseVertex,
            std::vector<float>& positions,
            std::vector<float>& normals,
            std::unordered_map<CadFlatShadedRangeKey,
                               CadFlatShadedRange,
                               CadFlatShadedRangeKeyHash>& ranges) {
        const CadFlatShadedGpu& current = gpuRes_->flatShaded();
        size_t appendVertexCount = 0;
        for (const Occurrence& occurrence : occurrences) {
            if (onlyMissing &&
                    current.ranges.find(occurrence.rangeKey) !=
                        current.ranges.end())
                continue;
            if (appendVertexCount >
                    maxVertexCount - occurrence.indexCount)
                return false;
            appendVertexCount += occurrence.indexCount;
        }
        positions.clear();
        normals.clear();
        positions.reserve(appendVertexCount * 3);
        normals.reserve(appendVertexCount * 3);
        ranges.clear();
        for (const Occurrence& occurrence : occurrences) {
            if (onlyMissing &&
                    current.ranges.find(occurrence.rangeKey) !=
                        current.ranges.end())
                continue;
            const Obol::TriMesh& mesh = *occurrence.mesh;
            const CadVisibleInstance& instance = *occurrence.instance;
            const bool hasVertexNormals =
                mesh.normals.size() == mesh.positions.size();
            SbMatrix transform;
            transform.setValue(instance.transform.data());
            const SbMatrix normalMatrix = transform.inverse().transpose();
            const GLint first = baseVertex +
                static_cast<GLint>(positions.size() / 3);
            for (size_t t = 0; t + 2 < occurrence.indexCount; t += 3) {
                const uint32_t ia = mesh.indices[t];
                const uint32_t ib = mesh.indices[t + 1];
                const uint32_t ic = mesh.indices[t + 2];
                if (ia >= mesh.positions.size() ||
                        ib >= mesh.positions.size() ||
                        ic >= mesh.positions.size())
                    return false;
                const uint32_t indices[3] = {ia, ib, ic};
                SbVec3f triangle[3];
                for (size_t vertex = 0; vertex < 3; ++vertex) {
                    SbVec3f point = mesh.positions[indices[vertex]];
                    if (mesh.isProgressive() && occurrence.level < 15) {
                        point = progressiveSnapPoint(
                            point, mesh.progressiveQuantizationMinimum,
                            mesh.progressiveQuantizationMaximum,
                            occurrence.level);
                    }
                    triangle[vertex] =
                        transformedFlatPoint(point, instance.transform);
                }
                SbVec3f faceNormal =
                    (triangle[1] - triangle[0]).cross(
                        triangle[2] - triangle[0]);
                if (faceNormal.sqrLength() > 0.0f)
                    faceNormal.normalize();
                else
                    faceNormal.setValue(0.0f, 0.0f, 1.0f);
                for (size_t vertex = 0; vertex < 3; ++vertex) {
                    SbVec3f normal = faceNormal;
                    if (hasVertexNormals) {
                        normalMatrix.multDirMatrix(
                            mesh.normals[indices[vertex]], normal);
                        if (normal.sqrLength() > 0.0f)
                            normal.normalize();
                        else
                            normal = faceNormal;
                    }
                    appendPackedPoint(positions, triangle[vertex]);
                    appendPackedPoint(normals, normal);
                }
            }
            ranges.emplace(occurrence.rangeKey,
                CadFlatShadedRange{
                    first,
                    static_cast<GLsizei>(occurrence.indexCount)});
        }
        return true;
    };

    bool rebuild = !gpuRes_->flatShaded().posBuf ||
                   !gpuRes_->flatShaded().normBuf;
    std::vector<float> positions;
    std::vector<float> normals;
    std::unordered_map<CadFlatShadedRangeKey, CadFlatShadedRange,
                       CadFlatShadedRangeKeyHash> newRanges;

    if (!rebuild) {
        size_t missingVertexCount = 0;
        const CadFlatShadedGpu& flat = gpuRes_->flatShaded();
        for (const Occurrence& occurrence : occurrences) {
            if (flat.ranges.find(occurrence.rangeKey) != flat.ranges.end())
                continue;
            if (missingVertexCount >
                    maxVertexCount - occurrence.indexCount)
                return false;
            missingVertexCount += occurrence.indexCount;
        }
        if (missingVertexCount) {
            if (flat.vertexCount >
                    flat.capacityVertexCount -
                        static_cast<GLsizei>(missingVertexCount)) {
                /* The retained level history has filled its reserve.  Compact
                 * to just the cuts needed by this view, then leave fresh
                 * headroom for later view changes. */
                rebuild = true;
            } else {
                if (!buildAtlasRanges(
                        true, flat.vertexCount, positions, normals,
                        newRanges) ||
                        !gpuRes_->appendFlatShaded(
                            positions, normals, newRanges, glue))
                    return false;
            }
        }
    }

    if (rebuild) {
        if (occurrences.empty())
            return true;
        if (!buildAtlasRanges(
                false, 0, positions, normals, newRanges))
            return false;
        const size_t vertexCount = positions.size() / 3;
        /*
         * Scene admission deliberately grows a cheap measured cut by as much
         * as 4x while substantial frame headroom remains.  A 2x VBO reserve
         * therefore guaranteed a full CPU re-expansion and GPU reallocation
         * at nearly every calibrated population wave.  Keep room for two
         * such waves (including the retained old ranges) so progressive
         * publication normally appends; the 512 MiB aggregate vertex-data
         * ceiling above still bounds the allocation.
         */
        constexpr size_t progressiveGrowthReserve = 16u;
        const size_t reserve = std::min(
            maxVertexCount,
            vertexCount <= maxVertexCount / progressiveGrowthReserve ?
                vertexCount * progressiveGrowthReserve : maxVertexCount);
        gpuRes_->uploadFlatShaded(
            plan.revision, plan.geometryRevision,
            positions, normals, std::vector<CadFlatShadedGroup>(),
            newRanges, static_cast<GLsizei>(reserve), glue, caps_);
    }

    std::vector<CadFlatShadedGroup> groups;
    uint64_t activeStyle = 0;
    bool haveGroup = false;
    const CadFlatShadedGpu& atlas = gpuRes_->flatShaded();
    for (const Occurrence& occurrence : occurrences) {
        const auto rangeIt = atlas.ranges.find(occurrence.rangeKey);
        if (rangeIt == atlas.ranges.end())
            return false;
        if (!haveGroup || occurrence.styleKey != activeStyle) {
            CadFlatShadedGroup group;
            std::copy(occurrence.instance->rgba.begin(),
                      occurrence.instance->rgba.end(), group.rgba);
            group.cullBackfaces = occurrence.cullBackfaces;
            groups.push_back(group);
            activeStyle = occurrence.styleKey;
            haveGroup = true;
        }
        CadFlatShadedGroup& group = groups.back();
        const GLint first = rangeIt->second.first;
        const GLsizei count = rangeIt->second.count;
        if (!group.firsts.empty() &&
                group.firsts.back() + group.counts.back() == first) {
            group.counts.back() += count;
        } else {
            group.firsts.push_back(first);
            group.counts.push_back(count);
        }
    }
    for (CadFlatShadedGroup& group : groups) {
        if (group.firsts.empty()) continue;
        group.first = group.firsts.front();
        group.count = group.counts.front();
    }
    gpuRes_->updateFlatShadedGroups(plan.revision, groups);

    const CadFlatShadedGpu& flat = gpuRes_->flatShaded();
    if (occurrences.empty()) return true;
    if (!flat.posBuf || !flat.normBuf || flat.groups.empty()) return false;

    /*
     * The flattened path submits the same view-selected progressive prefixes
     * as the retained indexed paths, but historically left the shared
     * diagnostic at zero.  Besides hiding actual software-renderer work from
     * the HUD and graphical tests, that made a render-only interaction
     * ceiling indistinguishable from rewriting every producer-authored
     * occurrence cut.  currentVertexCount is the exact sum of the flattened
     * triangle vertices selected above.
     */
    lastRenderedTriangleCount_ =
        static_cast<uint64_t>(currentVertexCount / 3u);

    if (caps_.isSoftwareRenderer) {
        glue->glMatrixMode(GL_PROJECTION);
        glue->glPushMatrix();
        glue->glLoadMatrixf(projectionMatrix[0]);
        glue->glMatrixMode(GL_MODELVIEW);
        glue->glPushMatrix();
        glue->glLoadMatrixf(viewMatrix[0]);
        const GLboolean wasLighting = glue->glIsEnabled(GL_LIGHTING);
        if (depthOnly) glue->glDisable(GL_LIGHTING);
        else glue->glEnable(GL_LIGHTING);
        glue->glEnableClientState(GL_VERTEX_ARRAY);
        glue->glBindBuffer(GL_ARRAY_BUFFER, flat.posBuf);
        glue->glVertexPointer(3, GL_FLOAT, 3 * sizeof(float), nullptr);
        if (!depthOnly) {
            glue->glEnableClientState(GL_NORMAL_ARRAY);
            glue->glBindBuffer(GL_ARRAY_BUFFER, flat.normBuf);
            glue->glNormalPointer(GL_FLOAT, 3 * sizeof(float), nullptr);
        }
        for (const CadFlatShadedGroup& group : flat.groups) {
            setCadBackfaceCulling(glue, group.cullBackfaces);
            if (!depthOnly) setImmediateMaterialFromRgba(glue, group.rgba);
            drawFlatShadedGroup(glue, GL_TRIANGLES, group);
        }
        if (!depthOnly) glue->glDisableClientState(GL_NORMAL_ARRAY);
        glue->glDisableClientState(GL_VERTEX_ARRAY);
        glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
        if (wasLighting) glue->glEnable(GL_LIGHTING);
        else glue->glDisable(GL_LIGHTING);
        glue->glMatrixMode(GL_MODELVIEW);
        glue->glPopMatrix();
        glue->glMatrixMode(GL_PROJECTION);
        glue->glPopMatrix();
        glue->glMatrixMode(GL_MODELVIEW);
        return true;
    }

    glue->glUseProgramObjectARB(shaders_.shaded);
    const GLint locVP = glue->glGetUniformLocationARB(shaders_.shaded,
                                                       "u_viewProj");
    const GLint locModel = glue->glGetUniformLocationARB(shaders_.shaded,
                                                          "u_model");
    const GLint locColor = glue->glGetUniformLocationARB(shaders_.shaded,
                                                          "u_color");
    const GLint locHasNorm = glue->glGetUniformLocationARB(shaders_.shaded,
                                                            "u_hasNorm");
    const SbMatrix identity = SbMatrix::identity();
    glue->glUniformMatrix4fvARB(locVP, 1, GL_FALSE, viewProj[0]);
    glue->glUniformMatrix4fvARB(locModel, 1, GL_FALSE, identity[0]);
    this->uploadLights(glue, shaders_.shaded);
    glue->glUniform1iARB(locHasNorm, depthOnly ? 0 : 1);
    if (flat.vao && glue->glBindVertexArray) {
        glue->glBindVertexArray(flat.vao);
    } else {
        glue->glBindBuffer(GL_ARRAY_BUFFER, flat.posBuf);
        glue->glVertexAttribPointerARB(0, 3, GL_FLOAT, GL_FALSE,
                                       3 * sizeof(float), nullptr);
        glue->glEnableVertexAttribArrayARB(0);
        glue->glBindBuffer(GL_ARRAY_BUFFER, flat.normBuf);
        glue->glVertexAttribPointerARB(1, 3, GL_FLOAT, GL_FALSE,
                                       3 * sizeof(float), nullptr);
        glue->glEnableVertexAttribArrayARB(1);
    }
    for (const CadFlatShadedGroup& group : flat.groups) {
        setCadBackfaceCulling(glue, group.cullBackfaces);
        const float rgba[4] = {group.rgba[0] / 255.0f,
                               group.rgba[1] / 255.0f,
                               group.rgba[2] / 255.0f,
                               group.rgba[3] / 255.0f};
        glue->glUniform4fvARB(locColor, 1, rgba);
        drawFlatShadedGroup(glue, GL_TRIANGLES, group);
    }
    if (flat.vao && glue->glBindVertexArray) {
        glue->glBindVertexArray(0);
    } else {
        glue->glDisableVertexAttribArrayARB(1);
        glue->glDisableVertexAttribArrayARB(0);
        glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    glue->glUseProgramObjectARB(0);
    return true;
}

bool CadRendererGL::renderFlatTriangleEdges(
        const CadFramePlan& plan,
        const SoGLContext* glue,
        const SbMatrix& viewProj,
        const SbMatrix& viewMatrix,
        const SbMatrix& projectionMatrix)
{
    const CadFlatShadedGpu& flat = gpuRes_->flatShaded();
    if (flat.planRevision != plan.revision || !flat.posBuf ||
            flat.groups.empty())
        return false;

    GLint polygonMode[2] = {GL_FILL, GL_FILL};
    glue->glGetIntegerv(GL_POLYGON_MODE, polygonMode);
    glue->glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    if (caps_.isSoftwareRenderer) {
        glue->glMatrixMode(GL_PROJECTION);
        glue->glPushMatrix();
        glue->glLoadMatrixf(projectionMatrix[0]);
        glue->glMatrixMode(GL_MODELVIEW);
        glue->glPushMatrix();
        glue->glLoadMatrixf(viewMatrix[0]);
        const GLboolean wasLighting = glue->glIsEnabled(GL_LIGHTING);
        glue->glDisable(GL_LIGHTING);
        glue->glEnableClientState(GL_VERTEX_ARRAY);
        glue->glBindBuffer(GL_ARRAY_BUFFER, flat.posBuf);
        glue->glVertexPointer(3, GL_FLOAT, 3 * sizeof(float), nullptr);
        for (const CadFlatShadedGroup& group : flat.groups) {
            setCadBackfaceCulling(glue, group.cullBackfaces);
            glue->glColor4ub(group.rgba[0], group.rgba[1],
                             group.rgba[2], group.rgba[3]);
            drawFlatShadedGroup(glue, GL_TRIANGLES, group);
        }
        glue->glDisableClientState(GL_VERTEX_ARRAY);
        glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
        if (wasLighting) glue->glEnable(GL_LIGHTING);
        glue->glMatrixMode(GL_MODELVIEW);
        glue->glPopMatrix();
        glue->glMatrixMode(GL_PROJECTION);
        glue->glPopMatrix();
        glue->glMatrixMode(GL_MODELVIEW);
    } else {
        glue->glUseProgramObjectARB(shaders_.wire);
        const GLint locVP = glue->glGetUniformLocationARB(shaders_.wire,
                                                          "u_viewProj");
        const GLint locModel = glue->glGetUniformLocationARB(shaders_.wire,
                                                             "u_model");
        const GLint locColor = glue->glGetUniformLocationARB(shaders_.wire,
                                                             "u_color");
        const SbMatrix identity = SbMatrix::identity();
        glue->glUniformMatrix4fvARB(locVP, 1, GL_FALSE, viewProj[0]);
        glue->glUniformMatrix4fvARB(locModel, 1, GL_FALSE, identity[0]);
        if (flat.vao && glue->glBindVertexArray) {
            glue->glBindVertexArray(flat.vao);
        } else {
            glue->glBindBuffer(GL_ARRAY_BUFFER, flat.posBuf);
            glue->glVertexAttribPointerARB(0, 3, GL_FLOAT, GL_FALSE,
                                           3 * sizeof(float), nullptr);
            glue->glEnableVertexAttribArrayARB(0);
        }
        for (const CadFlatShadedGroup& group : flat.groups) {
            setCadBackfaceCulling(glue, group.cullBackfaces);
            const float rgba[4] = {group.rgba[0] / 255.0f,
                                   group.rgba[1] / 255.0f,
                                   group.rgba[2] / 255.0f,
                                   group.rgba[3] / 255.0f};
            glue->glUniform4fvARB(locColor, 1, rgba);
            drawFlatShadedGroup(glue, GL_TRIANGLES, group);
        }
        if (flat.vao && glue->glBindVertexArray) {
            glue->glBindVertexArray(0);
        } else {
            glue->glDisableVertexAttribArrayARB(0);
            glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
        glue->glUseProgramObjectARB(0);
    }

    glue->glPolygonMode(GL_FRONT, static_cast<GLenum>(polygonMode[0]));
    glue->glPolygonMode(GL_BACK, static_cast<GLenum>(polygonMode[1]));
    return true;
}

// ---------------------------------------------------------------------------
// Tier-1: VBO-loop rendering (GL 2.0+)
// ---------------------------------------------------------------------------

static GLsizei progressiveWireSegmentCount(
        const SoCADAssembly& assembly, PartId part,
        const CadVisibleInstance& instance, GLsizei residentCount)
{
    const PartGeometry *geometry = assembly.partGeometry(part);
    if (!geometry || !geometry->wire || !geometry->wire->isProgressive())
        return residentCount;
    return static_cast<GLsizei>(geometry->wire->segmentCountAtLevel(
        assembly.effectiveProgressiveLodLevel(instance.lodLevel)));
}

static GLsizei progressiveTriangleIndexCount(
        const SoCADAssembly& assembly, PartId part,
        const CadVisibleInstance& instance, GLsizei residentCount)
{
    const PartGeometry *geometry = assembly.partGeometry(part);
    if (!geometry || !geometry->shaded ||
            !geometry->shaded->isProgressive())
        return residentCount;
    return static_cast<GLsizei>(geometry->shaded->indexCountAtLevel(
        assembly.effectiveProgressiveLodLevel(instance.lodLevel)));
}

static uint8_t
progressiveLevel(uint8_t requested, uint8_t minimum, uint8_t resident)
{
    if (resident >= 16) return 15;
    return std::max(minimum, std::min(resident, requested));
}

static void
uploadProgressivePositionUniforms(
        const SoGLContext *glue, GLint encodeScaleLocation,
        GLint decodeScaleLocation, GLint minLocation, uint8_t level,
        const SbVec3f& minimum, const SbVec3f& maximum)
{
    const GLfloat mask = std::ldexp(1.0f, 15 - static_cast<int>(level));
    SbVec3f encodeScale;
    SbVec3f decodeScale;
    for (int axis = 0; axis < 3; ++axis) {
        const GLfloat extent = maximum[axis] - minimum[axis];
        const GLfloat safeExtent = std::max(extent, 1.0e-30f);
        encodeScale[axis] = 65535.0f / (safeExtent * mask);
        decodeScale[axis] = 0.5f * mask * extent / 65535.0f;
    }
    glue->glUniform3fvARB(
        encodeScaleLocation, 1, encodeScale.getValue());
    glue->glUniform3fvARB(
        decodeScaleLocation, 1, decodeScale.getValue());
    glue->glUniform3fvARB(minLocation, 1, minimum.getValue());
}

static float
progressiveSnapCoordinate(float value, float minimum, float maximum,
                          double mask)
{
    if (!(maximum > minimum)) return value;
    const double scaled =
        (static_cast<double>(value) - minimum) /
        (static_cast<double>(maximum) - minimum) * 65535.0;
    const double low = std::floor(scaled / mask);
    const double high = std::ceil(scaled / mask);
    const double snapped = (low + high) * 0.5 * mask;
    return static_cast<float>(
        (snapped / 65535.0) *
        (static_cast<double>(maximum) - minimum) + minimum);
}

static SbVec3f
progressiveSnapPoint(const SbVec3f& point, const SbVec3f& minimum,
                     const SbVec3f& maximum, uint8_t level)
{
    if (level >= 15) return point;
    const double mask = std::ldexp(1.0, 15 - static_cast<int>(level));
    return SbVec3f(
        progressiveSnapCoordinate(point[0], minimum[0], maximum[0], mask),
        progressiveSnapCoordinate(point[1], minimum[1], maximum[1], mask),
        progressiveSnapCoordinate(point[2], minimum[2], maximum[2], mask));
}

static const CadProgressiveGpu*
ensureProgressiveWireGpu(
        CadGpuResources *resources, PartId part, const WireRep& wire,
        uint8_t level, const SoGLContext *glue)
{
    if (!resources || !glue) return nullptr;

    const size_t pointCount = wire.segmentCountAtLevel(level) * 2;
    if (pointCount == 0 || pointCount > wire.segmentPoints.size())
        return nullptr;
    if (const CadProgressiveGpu *cached =
            resources->progressiveFor(part, false, level))
        return cached;
    std::vector<float> positions;
    positions.reserve(pointCount * 3);
    for (size_t i = 0; i < pointCount; ++i) {
        const SbVec3f point = progressiveSnapPoint(
            wire.segmentPoints[i],
            wire.progressiveQuantizationMinimum,
            wire.progressiveQuantizationMaximum, level);
        appendPackedPoint(positions, point);
    }
    resources->uploadProgressive(
        part, false, level, positions, std::vector<float>(), false, glue);
    return resources->progressiveFor(part, false, level);
}

static const CadProgressiveGpu*
ensureProgressiveTriGpu(
        CadGpuResources *resources, PartId part, const TriMesh& mesh,
        uint8_t level, const SoGLContext *glue)
{
    if (!resources || !glue) return nullptr;

    const size_t indexCount = mesh.indexCountAtLevel(level);
    if (indexCount < 3 || indexCount > mesh.indices.size())
        return nullptr;
    const bool indexed = mesh.normals.size() == mesh.positions.size();
    uint32_t maximumIndex = 0;
    if (indexed) {
        for (size_t i = 0; i < indexCount; ++i) {
            if (mesh.indices[i] >= mesh.positions.size())
                return nullptr;
            maximumIndex = std::max(maximumIndex, mesh.indices[i]);
        }
    }
    if (const CadProgressiveGpu *cached =
            resources->progressiveFor(part, true, level))
        return cached;

    std::vector<float> positions;
    std::vector<float> normals;
    if (indexed) {
        positions.reserve((static_cast<size_t>(maximumIndex) + 1) * 3);
        for (size_t i = 0; i <= maximumIndex; ++i) {
            const SbVec3f point = progressiveSnapPoint(
                mesh.positions[i],
                mesh.progressiveQuantizationMinimum,
                mesh.progressiveQuantizationMaximum, level);
            appendPackedPoint(positions, point);
        }
    } else {
        positions.reserve(indexCount * 3);
        normals.reserve(indexCount * 3);
        for (size_t t = 0; t + 2 < indexCount; t += 3) {
            SbVec3f triangle[3];
            for (int k = 0; k < 3; ++k) {
                const uint32_t index = mesh.indices[t + k];
                if (index >= mesh.positions.size())
                    return nullptr;
                triangle[k] = progressiveSnapPoint(
                    mesh.positions[index],
                    mesh.progressiveQuantizationMinimum,
                    mesh.progressiveQuantizationMaximum, level);
            }
            SbVec3f normal =
                (triangle[1] - triangle[0]).cross(triangle[2] - triangle[0]);
            if (normal.sqrLength() > 0.0f)
                normal.normalize();
            else
                normal.setValue(0.0f, 0.0f, 1.0f);
            for (int k = 0; k < 3; ++k) {
                appendPackedPoint(positions, triangle[k]);
                appendPackedPoint(normals, normal);
            }
        }
    }
    resources->uploadProgressive(
        part, true, level, positions, normals, indexed, glue);
    return resources->progressiveFor(part, true, level);
}

// Bind a wire VBO, set up attribute 0 (position), draw segments.
// Works with or without VAO.
static void bindAndDrawWire(const CadWireGpu* w, const SoGLContext* glue,
                             GLint locPos, GLsizei segmentCount)
{
    if (!w || w->segCount == 0 || segmentCount <= 0) return;
    segmentCount = std::min(segmentCount, w->segCount);

    if (w->vao && glue->glBindVertexArray) {
        glue->glBindVertexArray(w->vao);
    } else {
        glue->glBindBuffer(GL_ARRAY_BUFFER, w->posBuf);
        glue->glVertexAttribPointerARB(static_cast<GLuint>(locPos), 3,
                                       GL_FLOAT, GL_FALSE,
                                       3 * sizeof(float), nullptr);
        glue->glEnableVertexAttribArrayARB(static_cast<GLuint>(locPos));
        if (!w->sequentialSegments)
            glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, w->segIdxBuf);
    }

    if (w->sequentialSegments) {
        glue->glDrawArrays(GL_LINES, 0, segmentCount * 2);
    } else {
        glue->glDrawElements(GL_LINES,
                             segmentCount * 2,
                             GL_UNSIGNED_INT,
                             nullptr);
    }
    {
        GLenum err = glue->glGetError();
        if (err != GL_NO_ERROR)
            std::fprintf(stderr, "CadRendererGL: glDrawElements error: 0x%x\n", err);
    }

    if (w->vao && glue->glBindVertexArray) {
        glue->glBindVertexArray(0);
    } else {
        glue->glDisableVertexAttribArrayARB(static_cast<GLuint>(locPos));
        glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
        if (!w->sequentialSegments)
            glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
}

// Bind a tri VBO, set up attributes 0 (position) and 1 (normal), draw.
static void bindAndDrawTri(const CadTriGpu* t, const SoGLContext* glue,
                            GLint locPos, GLint locNorm, bool& hasNorm,
                            GLsizei indexCount)
{
    if (!t || t->idxCount == 0 || indexCount <= 0) return;
    indexCount = std::min(indexCount, t->idxCount);

    hasNorm = (t->normBuf != 0);

    if (t->vao && glue->glBindVertexArray) {
        glue->glBindVertexArray(t->vao);
    } else {
        glue->glBindBuffer(GL_ARRAY_BUFFER, t->posBuf);
        glue->glVertexAttribPointerARB(static_cast<GLuint>(locPos), 3,
                                       GL_FLOAT, GL_FALSE,
                                       3 * sizeof(float), nullptr);
        glue->glEnableVertexAttribArrayARB(static_cast<GLuint>(locPos));

        if (hasNorm && locNorm >= 0) {
            glue->glBindBuffer(GL_ARRAY_BUFFER, t->normBuf);
            glue->glVertexAttribPointerARB(static_cast<GLuint>(locNorm), 3,
                                           GL_FLOAT, GL_FALSE,
                                           3 * sizeof(float), nullptr);
            glue->glEnableVertexAttribArrayARB(static_cast<GLuint>(locNorm));
        }
        glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, t->idxBuf);
    }

    glue->glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);

    if (t->vao && glue->glBindVertexArray) {
        glue->glBindVertexArray(0);
    } else {
        if (hasNorm && locNorm >= 0) {
            glue->glDisableVertexAttribArrayARB(static_cast<GLuint>(locNorm));
        }
        glue->glDisableVertexAttribArrayARB(static_cast<GLuint>(locPos));
        glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
        glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
}

void CadRendererGL::renderVboLoop(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const SoGLContext*   glue,
        const SbMatrix&      viewProj,
        const SbViewVolume&  viewVolume,
        bool drawWire,
        bool customWireOnly,
        bool drawShaded)
{
    // OI stores matrices row-major.  GL reads them column-major.  Passing
    // the raw float[16] with GL_FALSE means GL transposes our row-major
    // matrix into the column-major form the shader expects, which is
    // exactly the GL column-vector convention.  (Same as SoGLSLShaderParameter.)
    const float* vpData = viewProj[0];

    // a_pos=0, a_norm=1 are pinned via glBindAttribLocationARB before linking
    const GLint locPos  = 0;
    const GLint locNorm = 1;

    // Extract frustum planes for per-instance culling.
    const FrustumPlanes fp = extractFrustumPlanes(viewProj);

    // --- Wire pass ---
    if (drawWire && !plan.wireItems.empty()) {
        const CadWireRasterState rasterState = captureWireRasterState(
            glue, caps_.hasLineStipple);
        struct WireLocations {
            GLint viewProjection = -1;
            GLint model = -1;
            GLint color = -1;
            GLint encodeScale = -1;
            GLint decodeScale = -1;
            GLint minimum = -1;
        };
        const GLuint programs[2] = {shaders_.wire, shaders_.wirePop};
        WireLocations locations[2];
        for (int variant = 0; variant < 2; ++variant) {
            if (!programs[variant])
                continue;
            locations[variant].viewProjection =
                glue->glGetUniformLocationARB(
                    programs[variant], "u_viewProj");
            locations[variant].model = glue->glGetUniformLocationARB(
                programs[variant], "u_model");
            locations[variant].color = glue->glGetUniformLocationARB(
                programs[variant], "u_color");
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
            if (customWireOnly && !item.customWireStyle) continue;
            CadWireGpu* w = gpuRes_->wireFor(item.rep.part);
            if (!w) continue;
            const PartGeometry *geometry =
                assembly.partGeometry(item.rep.part);
            const WireRep *progressive =
                geometry && geometry->wire &&
                geometry->wire->isProgressive() ?
                &*geometry->wire : nullptr;

            for (uint32_t i = 0; i < item.instanceCount; ++i) {
                const size_t visibleIndex = item.baseInstance + i;
                if (!drawItemOwnsInstance(plan, item, visibleIndex) ||
                        isHiddenInstance(plan, visibleIndex) ||
                        isSubpixelProxyInstance(plan, visibleIndex))
                    continue;
                const auto& inst = plan.visibleInstances[visibleIndex];
                if (isBoxOutsideFrustum(inst.wbMin, inst.wbMax, fp)) continue;

                const uint8_t level = progressive ?
                    progressiveLevel(
                        assembly.effectiveProgressiveLodLevel(inst.lodLevel),
                        progressive->progressiveMinimumLevel,
                        progressive->progressiveResidentLevel) : 15;
                const int variant = progressive && level < 15 ? 1 : 0;
                const WireLocations& loc = locations[variant];
                if (activeProgram != programs[variant]) {
                    activeProgram = programs[variant];
                    glue->glUseProgramObjectARB(activeProgram);
                    glue->glUniformMatrix4fvARB(
                        loc.viewProjection, 1, GL_FALSE, vpData);
                }
                glue->glUniformMatrix4fvARB(loc.model, 1, GL_FALSE,
                                            inst.transform.data());
                float rgba[4] = {
                    inst.rgba[0] / 255.0f, inst.rgba[1] / 255.0f,
                    inst.rgba[2] / 255.0f, inst.rgba[3] / 255.0f
                };
                glue->glUniform4fvARB(loc.color, 1, rgba);
                if (variant) {
                    uploadProgressivePositionUniforms(
                        glue, loc.encodeScale, loc.decodeScale, loc.minimum,
                        level,
                        progressive->progressiveQuantizationMinimum,
                        progressive->progressiveQuantizationMaximum);
                }
                applyWireRasterStyle(glue, inst, caps_.hasLineStipple);
                bindAndDrawWire(w, glue, locPos,
                    progressiveWireSegmentCount(assembly, item.rep.part,
                                                inst, w->segCount));
            }
        }

        glue->glUseProgramObjectARB(0);
        restoreWireRasterState(glue, rasterState, caps_.hasLineStipple);
    }

    // --- Shaded pass ---
    if (drawShaded && !plan.shadedItems.empty()) {
        struct ShadedLocations {
            GLint viewProjection = -1;
            GLint model = -1;
            GLint color = -1;
            GLint hasNormal = -1;
            GLint lightVector = -1;
            GLint lightColor = -1;
            GLint encodeScale = -1;
            GLint decodeScale = -1;
            GLint minimum = -1;
        };
        enum ShadedProgramIndex {
            GenericExact = 0,
            GenericPop,
            DirectionalNormExact,
            DirectionalNormPop,
            DirectionalFaceExact,
            DirectionalFacePop,
            ShadedProgramCount
        };
        const GLuint programs[ShadedProgramCount] = {
            shaders_.shaded,
            shaders_.shadedPop,
            shaders_.shadedDirectionalNorm,
            shaders_.shadedPopDirectionalNorm,
            shaders_.shadedDirectionalFace,
            shaders_.shadedPopDirectionalFace
        };
        ShadedLocations locations[ShadedProgramCount];
        for (int programIndex = 0;
                programIndex < ShadedProgramCount; ++programIndex) {
            if (!programs[programIndex])
                continue;
            locations[programIndex].viewProjection =
                glue->glGetUniformLocationARB(
                    programs[programIndex], "u_viewProj");
            locations[programIndex].model =
                glue->glGetUniformLocationARB(
                    programs[programIndex], "u_model");
            locations[programIndex].color =
                glue->glGetUniformLocationARB(
                    programs[programIndex], "u_color");
            locations[programIndex].hasNormal =
                glue->glGetUniformLocationARB(
                    programs[programIndex], "u_hasNorm");
            locations[programIndex].lightVector =
                glue->glGetUniformLocationARB(
                    programs[programIndex], "u_lightVec");
            locations[programIndex].lightColor =
                glue->glGetUniformLocationARB(
                    programs[programIndex], "u_lightColor");
            const bool progressiveProgram =
                programIndex == GenericPop ||
                programIndex == DirectionalNormPop ||
                programIndex == DirectionalFacePop;
            if (progressiveProgram) {
                locations[programIndex].encodeScale =
                    glue->glGetUniformLocationARB(
                        programs[programIndex], "u_popEncodeScale");
                locations[programIndex].decodeScale =
                    glue->glGetUniformLocationARB(
                        programs[programIndex], "u_popDecodeScale");
                locations[programIndex].minimum =
                    glue->glGetUniformLocationARB(
                        programs[programIndex], "u_popMin");
            }
        }

        bool directionalLight = false;
        float directionalVector[3] = {
            kLightDir[0], kLightDir[1], kLightDir[2]
        };
        float directionalColor[3] = {1.0f, 1.0f, 1.0f};
        if (this->lights_.empty()) {
            directionalLight = true;
        } else if (this->lights_.size() == 1 &&
                   this->lights_[0].type == 0) {
            directionalLight = true;
            for (int component = 0; component < 3; ++component) {
                directionalVector[component] =
                    this->lights_[0].vec[component];
                directionalColor[component] =
                    this->lights_[0].color[component];
            }
        }
        if (directionalLight) {
            SbVec3f normalized(
                directionalVector[0], directionalVector[1],
                directionalVector[2]);
            if (normalized.normalize() == 0.0f)
                normalized.setValue(kLightDir);
            directionalVector[0] = normalized[0];
            directionalVector[1] = normalized[1];
            directionalVector[2] = normalized[2];
        }

        GLuint activeProgram = 0;
        bool programUploaded[ShadedProgramCount] = {};
        if (cadLightDebugRequested()) {
            static unsigned int uniformLocationReportCount = 0;
            if (uniformLocationReportCount++ < 4) {
                std::fprintf(stderr,
                    "CadRendererGL shaded locations="
                    "{base={vp=%d model=%d color=%d hasNorm=%d} "
                    "pop={vp=%d model=%d color=%d hasNorm=%d "
                    "encode=%d decode=%d min=%d}}\n",
                    locations[0].viewProjection, locations[0].model,
                    locations[0].color, locations[0].hasNormal,
                    locations[1].viewProjection, locations[1].model,
                    locations[1].color, locations[1].hasNormal,
                    locations[1].encodeScale, locations[1].decodeScale,
                    locations[1].minimum);
            }
        }

        for (const auto& item : plan.shadedItems) {
            const CadTriGpu* t = gpuRes_->triFor(item.rep.part);
            if (!t) continue;
            const PartGeometry *geometry =
                assembly.partGeometry(item.rep.part);
            const TriMesh *progressive =
                geometry && geometry->shaded &&
                geometry->shaded->isProgressive() ?
                &*geometry->shaded : nullptr;

            setCadBackfaceCulling(glue, item.cullBackfaces);
            bool hasNorm = (t->normBuf != 0);
            if (cadLightDebugRequested()) {
                static unsigned int geometryReportCount = 0;
                if (geometryReportCount++ < 32) {
                    const size_t positionCount =
                        geometry && geometry->shaded ?
                        geometry->shaded->positions.size() : 0;
                    const size_t normalCount =
                        geometry && geometry->shaded ?
                        geometry->shaded->normals.size() : 0;
                    const size_t indexCount =
                        geometry && geometry->shaded ?
                        geometry->shaded->indices.size() : 0;
                    std::fprintf(stderr,
                        "CadRendererGL shaded geometry positions=%zu "
                        "normals=%zu indices=%zu gpuVerts=%d gpuIndices=%d "
                        "hasNorm=%d progressive=%d cull=%d\n",
                        positionCount, normalCount, indexCount,
                        static_cast<int>(t->vertCount),
                        static_cast<int>(t->idxCount), hasNorm ? 1 : 0,
                        progressive ? 1 : 0,
                        item.cullBackfaces ? 1 : 0);
                }
            }

            for (uint32_t i = 0; i < item.instanceCount; ++i) {
                const size_t instanceIndex = item.baseInstance + i;
                if (!drawItemOwnsInstance(plan, item, instanceIndex) ||
                        isHiddenInstance(plan, instanceIndex))
                    continue;
                const auto& inst = plan.visibleInstances[instanceIndex];
                if (isBoxOutsideFrustum(inst.wbMin, inst.wbMax, fp)) continue;

                const uint8_t level = progressive ?
                    progressiveLevel(
                        assembly.effectiveProgressiveLodLevel(inst.lodLevel),
                        progressive->progressiveMinimumLevel,
                        progressive->progressiveResidentLevel) : 15;
                const int variant = progressive && level < 15 ? 1 : 0;
                int programIndex = variant ? GenericPop : GenericExact;
                if (directionalLight) {
                    const int directionalIndex = hasNorm ?
                        (variant ? DirectionalNormPop :
                                   DirectionalNormExact) :
                        (variant ? DirectionalFacePop :
                                   DirectionalFaceExact);
                    if (programs[directionalIndex])
                        programIndex = directionalIndex;
                }
                const ShadedLocations& loc = locations[programIndex];
                if (activeProgram != programs[programIndex]) {
                    activeProgram = programs[programIndex];
                    glue->glUseProgramObjectARB(activeProgram);
                    glue->glUniformMatrix4fvARB(
                        loc.viewProjection, 1, GL_FALSE, vpData);
                    if (!programUploaded[programIndex]) {
                        if (programIndex >= DirectionalNormExact) {
                            glue->glUniform3fvARB(
                                loc.lightVector, 1, directionalVector);
                            glue->glUniform3fvARB(
                                loc.lightColor, 1, directionalColor);
                            if (programIndex == DirectionalFaceExact ||
                                    programIndex == DirectionalFacePop) {
                                this->uploadViewFacing(
                                    glue, activeProgram, viewVolume);
                            }
                        } else {
                            this->uploadLights(glue, activeProgram);
                            this->uploadViewFacing(
                                glue, activeProgram, viewVolume);
                        }
                        programUploaded[programIndex] = true;
                    }
                }
                glue->glUniform1iARB(
                    loc.hasNormal, hasNorm ? 1 : 0);
                if (!hasNorm) {
                    SoGLContext_glVertexAttrib3f(
                        glue, static_cast<GLuint>(locNorm),
                        0.0f, 0.0f, 1.0f);
                }

                if (cadLightDebugRequested() && !this->lights_.empty() &&
                        std::fabs(this->lights_[0].vec[1]) > 0.9f &&
                        geometry && geometry->shaded &&
                        geometry->shaded->indices.size() >= 3) {
                    static unsigned int normalReportCount = 0;
                    if (normalReportCount++ < 32) {
                        const TriMesh& mesh = *geometry->shaded;
                        const uint32_t ia = mesh.indices[0];
                        const uint32_t ib = mesh.indices[1];
                        const uint32_t ic = mesh.indices[2];
                        if (ia < mesh.positions.size() &&
                                ib < mesh.positions.size() &&
                                ic < mesh.positions.size()) {
                            SbVec3f face =
                                (mesh.positions[ib] - mesh.positions[ia]).cross(
                                    mesh.positions[ic] - mesh.positions[ia]);
                            if (face.sqrLength() > 0.0f)
                                face.normalize();
                            const SbVec3f normal =
                                ia < mesh.normals.size() ?
                                mesh.normals[ia] : face;
                            const float *m = inst.transform.data();
                            const auto transformDirection =
                                [m](const SbVec3f& direction) {
                                    SbVec3f transformed(
                                        m[0] * direction[0] +
                                            m[4] * direction[1] +
                                            m[8] * direction[2],
                                        m[1] * direction[0] +
                                            m[5] * direction[1] +
                                            m[9] * direction[2],
                                        m[2] * direction[0] +
                                            m[6] * direction[1] +
                                            m[10] * direction[2]);
                                    if (transformed.sqrLength() > 0.0f)
                                        transformed.normalize();
                                    return transformed;
                                };
                            const SbVec3f worldFace =
                                transformDirection(face);
                            const SbVec3f worldNormal =
                                transformDirection(normal);
                            SbVec3f light(
                                this->lights_[0].vec[0],
                                this->lights_[0].vec[1],
                                this->lights_[0].vec[2]);
                            if (light.sqrLength() > 0.0f)
                                light.normalize();
                            const double determinant =
                                static_cast<double>(m[0]) *
                                    (static_cast<double>(m[5]) * m[10] -
                                     static_cast<double>(m[6]) * m[9]) -
                                static_cast<double>(m[4]) *
                                    (static_cast<double>(m[1]) * m[10] -
                                     static_cast<double>(m[2]) * m[9]) +
                                static_cast<double>(m[8]) *
                                    (static_cast<double>(m[1]) * m[6] -
                                     static_cast<double>(m[2]) * m[5]);
                            std::fprintf(stderr,
                                "CadRendererGL normal diagnostic "
                                "indices=(%u,%u,%u) det=%.9g "
                                "localFace=(%.6g,%.6g,%.6g) "
                                "localNormal=(%.6g,%.6g,%.6g) "
                                "worldFace=(%.6g,%.6g,%.6g) "
                                "worldNormal=(%.6g,%.6g,%.6g) "
                                "faceDot=%.6g normalDot=%.6g align=%.6g\n",
                                ia, ib, ic, determinant,
                                face[0], face[1], face[2],
                                normal[0], normal[1], normal[2],
                                worldFace[0], worldFace[1], worldFace[2],
                                worldNormal[0], worldNormal[1],
                                worldNormal[2], worldFace.dot(light),
                                worldNormal.dot(light),
                                worldFace.dot(worldNormal));
                        }
                    }
                }

                glue->glUniformMatrix4fvARB(loc.model, 1, GL_FALSE,
                                            inst.transform.data());
                float rgba[4] = {
                    inst.rgba[0] / 255.0f, inst.rgba[1] / 255.0f,
                    inst.rgba[2] / 255.0f, inst.rgba[3] / 255.0f
                };
                glue->glUniform4fvARB(loc.color, 1, rgba);
                if (variant) {
                    uploadProgressivePositionUniforms(
                        glue, loc.encodeScale, loc.decodeScale, loc.minimum,
                        level,
                        progressive->progressiveQuantizationMinimum,
                        progressive->progressiveQuantizationMaximum);
                }
                if (cadLightDebugRequested()) {
                    static unsigned int uniformValueReportCount = 0;
                    if (uniformValueReportCount++ < 8) {
                        typedef void (APIENTRY * GetUniformivProc)(
                            GLuint, GLint, GLint *);
                        GetUniformivProc getUniform =
                            reinterpret_cast<GetUniformivProc>(
                                SoGLContext_getprocaddress(
                                    glue, "glGetUniformivARB"));
                        GLint storedHasNorm = -999;
                        if (getUniform) {
                            getUniform(activeProgram, loc.hasNormal,
                                       &storedHasNorm);
                        }
                        std::fprintf(stderr,
                            "CadRendererGL shaded uniform values "
                            "hasNorm=%d popActive=%d level=%u getProc=%d\n",
                            storedHasNorm, variant,
                            static_cast<unsigned int>(level),
                            getUniform ? 1 : 0);
                    }
                }

                bindAndDrawTri(t, glue, locPos, locNorm, hasNorm,
                    progressiveTriangleIndexCount(
                        assembly, item.rep.part, inst, t->idxCount));
            }
        }

        glue->glUseProgramObjectARB(0);
    }
}

// ---------------------------------------------------------------------------
// Tier-1 compatibility path: retained VBOs with fixed-function arrays
// ---------------------------------------------------------------------------

void CadRendererGL::renderFixedVboLoop(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const SoGLContext* glue,
        const SbMatrix& viewProj,
        const SbMatrix& viewMatrix,
        const SbMatrix& projectionMatrix,
        bool drawWire,
        bool drawShaded)
{
    glue->glMatrixMode(GL_PROJECTION);
    glue->glPushMatrix();
    glue->glLoadMatrixf(projectionMatrix[0]);
    glue->glMatrixMode(GL_MODELVIEW);
    glue->glPushMatrix();

    const GLboolean wasLighting = glue->glIsEnabled(GL_LIGHTING);
    glue->glDisable(GL_LIGHTING);
    glue->glEnableClientState(GL_VERTEX_ARRAY);
    const FrustumPlanes fp = extractFrustumPlanes(viewProj);
    const CadWireRasterState rasterState = captureWireRasterState(
        glue, caps_.hasLineStipple);

    if (drawWire) for (const auto& item : plan.wireItems) {
        const CadWireGpu *wire = gpuRes_->wireFor(item.rep.part);
        if (!wire) continue;
        const PartGeometry *geometry = assembly.partGeometry(item.rep.part);
        const WireRep *progressive =
            geometry && geometry->wire && geometry->wire->isProgressive() ?
            &*geometry->wire : nullptr;
        if (!wire->sequentialSegments)
            glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, wire->segIdxBuf);

        for (uint32_t i = 0; i < item.instanceCount; ++i) {
            const size_t visibleIndex = item.baseInstance + i;
            if (!drawItemOwnsInstance(plan, item, visibleIndex) ||
                    isHiddenInstance(plan, visibleIndex) ||
                    isSubpixelProxyInstance(plan, visibleIndex))
                continue;
            const auto& inst = plan.visibleInstances[visibleIndex];
            if (isBoxOutsideFrustum(inst.wbMin, inst.wbMax, fp)) continue;

            SbMatrix model;
            model.setValue(inst.transform.data());
            SbMatrix modelView = model;
            modelView.multRight(viewMatrix);
            glue->glLoadMatrixf(modelView[0]);
            glue->glColor4ub(inst.rgba[0], inst.rgba[1],
                             inst.rgba[2], inst.rgba[3]);
            applyWireRasterStyle(glue, inst, caps_.hasLineStipple);

            GLuint positionBuffer = wire->posBuf;
            if (progressive) {
                const uint8_t level = progressiveLevel(
                    assembly.effectiveProgressiveLodLevel(inst.lodLevel),
                    progressive->progressiveMinimumLevel,
                    progressive->progressiveResidentLevel);
                if (level < 15) {
                    const CadProgressiveGpu *cut =
                        ensureProgressiveWireGpu(
                            gpuRes_, item.rep.part, *progressive, level, glue);
                    if (!cut) continue;
                    positionBuffer = cut->posBuf;
                }
            }
            glue->glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);
            glue->glVertexPointer(3, GL_FLOAT, 3 * sizeof(float), nullptr);
            const GLsizei segmentCount = progressiveWireSegmentCount(
                assembly, item.rep.part, inst, wire->segCount);
            if (wire->sequentialSegments)
                glue->glDrawArrays(GL_LINES, 0, segmentCount * 2);
            else
                glue->glDrawElements(GL_LINES, segmentCount * 2,
                                     GL_UNSIGNED_INT, nullptr);
        }
    }

    glue->glDisableClientState(GL_VERTEX_ARRAY);
    restoreWireRasterState(glue, rasterState, caps_.hasLineStipple);

    const GLboolean wasColorMaterial = glue->glIsEnabled(GL_COLOR_MATERIAL);
    GLint wasTwoSidedLighting = GL_FALSE;
    glue->glGetIntegerv(GL_LIGHT_MODEL_TWO_SIDE, &wasTwoSidedLighting);
    if (drawShaded && !plan.shadedItems.empty()) {
        // CAD shading must not depend on the caller enabling GL_LIGHTING.
        glue->glEnable(GL_LIGHTING);
        glue->glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
    }
    glue->glDisable(GL_COLOR_MATERIAL);
    glue->glEnableClientState(GL_VERTEX_ARRAY);
    if (drawShaded) for (const auto& item : plan.shadedItems) {
        const CadTriGpu *tri = gpuRes_->triFor(item.rep.part);
        if (!tri) continue;
        const PartGeometry *geometry = assembly.partGeometry(item.rep.part);
        const TriMesh *progressive =
            geometry && geometry->shaded &&
            geometry->shaded->isProgressive() ?
            &*geometry->shaded : nullptr;

        setCadBackfaceCulling(glue, item.cullBackfaces);
        bool normalArrayEnabled = false;

        for (uint32_t i = 0; i < item.instanceCount; ++i) {
            const size_t instanceIndex = item.baseInstance + i;
            if (!drawItemOwnsInstance(plan, item, instanceIndex) ||
                    isHiddenInstance(plan, instanceIndex))
                continue;
            const auto& inst = plan.visibleInstances[instanceIndex];
            if (isBoxOutsideFrustum(inst.wbMin, inst.wbMax, fp)) continue;

            SbMatrix model;
            model.setValue(inst.transform.data());
            SbMatrix modelView = model;
            modelView.multRight(viewMatrix);
            glue->glLoadMatrixf(modelView[0]);
            setImmediateMaterialFromRgba(glue, inst.rgba.data());

            const GLsizei indexCount = progressiveTriangleIndexCount(
                assembly, item.rep.part, inst, tri->idxCount);
            const CadProgressiveGpu *cut = nullptr;
            if (progressive) {
                const uint8_t level = progressiveLevel(
                    assembly.effectiveProgressiveLodLevel(inst.lodLevel),
                    progressive->progressiveMinimumLevel,
                    progressive->progressiveResidentLevel);
                if (level < 15 || tri->normBuf == 0) {
                    cut = ensureProgressiveTriGpu(
                        gpuRes_, item.rep.part, *progressive, level, glue);
                    if (!cut) continue;
                }
            }

            const GLuint positionBuffer = cut ? cut->posBuf : tri->posBuf;
            glue->glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);
            glue->glVertexPointer(3, GL_FLOAT, 3 * sizeof(float), nullptr);
            const GLuint normalBuffer =
                cut && cut->normBuf ? cut->normBuf : tri->normBuf;
            if (normalBuffer) {
                if (!normalArrayEnabled) {
                    glue->glEnableClientState(GL_NORMAL_ARRAY);
                    normalArrayEnabled = true;
                }
                glue->glBindBuffer(GL_ARRAY_BUFFER, normalBuffer);
                glue->glNormalPointer(GL_FLOAT, 3 * sizeof(float), nullptr);
            } else if (normalArrayEnabled) {
                glue->glDisableClientState(GL_NORMAL_ARRAY);
                normalArrayEnabled = false;
            }

            if (cut && !cut->indexed) {
                glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
                glue->glDrawArrays(
                    GL_TRIANGLES, 0,
                    std::min(indexCount, cut->vertexCount));
            } else {
                glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tri->idxBuf);
                glue->glDrawElements(
                    GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
            }
        }
        if (normalArrayEnabled)
            glue->glDisableClientState(GL_NORMAL_ARRAY);
    }
    glue->glDisableClientState(GL_VERTEX_ARRAY);
    if (wasColorMaterial) glue->glEnable(GL_COLOR_MATERIAL);
    else glue->glDisable(GL_COLOR_MATERIAL);
    glue->glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, wasTwoSidedLighting);
    if (wasLighting) glue->glEnable(GL_LIGHTING);
    else glue->glDisable(GL_LIGHTING);

    glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
    glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glue->glMatrixMode(GL_MODELVIEW);
    glue->glPopMatrix();
    glue->glMatrixMode(GL_PROJECTION);
    glue->glPopMatrix();
    glue->glMatrixMode(GL_MODELVIEW);
}

// ---------------------------------------------------------------------------
// Tier-0: immediate-mode fallback (GL 1.1, Mesa 7.x swrast)
// ---------------------------------------------------------------------------

void CadRendererGL::renderImmediateMode(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const SoGLContext*   glue,
        const SbMatrix&      viewProj,
        const SbMatrix&      viewMatrix,
        const SbMatrix&      projectionMatrix,
        bool drawWire,
        bool drawShaded)
{
    // Keep projection out of GL_MODELVIEW so normal transformation uses only
    // the affine local-to-eye transform.
    glue->glMatrixMode(GL_PROJECTION);
    glue->glPushMatrix();
    glue->glLoadMatrixf(projectionMatrix[0]);

    glue->glMatrixMode(GL_MODELVIEW);
    glue->glPushMatrix();

    // Disable lighting so glColor4f controls the final colour
    GLboolean wasLighting = glue->glIsEnabled(GL_LIGHTING);
    glue->glDisable(GL_LIGHTING);
    const CadWireRasterState rasterState = captureWireRasterState(
        glue, caps_.hasLineStipple);

    // Extract frustum planes for per-instance culling.
    const FrustumPlanes fp = extractFrustumPlanes(viewProj);

    // --- Wire pass ---
    if (drawWire) for (const auto& item : plan.wireItems) {
        const Obol::PartGeometry* geom = assembly.partGeometry(item.rep.part);
        if (!geom || !geom->wire.has_value()) continue;
        const Obol::WireRep& wire = *geom->wire;

        for (uint32_t ii = 0; ii < item.instanceCount; ++ii) {
            const size_t visibleIndex = item.baseInstance + ii;
            if (!drawItemOwnsInstance(plan, item, visibleIndex) ||
                    isHiddenInstance(plan, visibleIndex) ||
                    isSubpixelProxyInstance(plan, visibleIndex))
                continue;
            const auto& inst = plan.visibleInstances[visibleIndex];
            if (isBoxOutsideFrustum(inst.wbMin, inst.wbMax, fp)) continue;

            SbMatrix model;
            model.setValue(inst.transform.data());
            SbMatrix modelView = model;
            modelView.multRight(viewMatrix);
            glue->glLoadMatrixf(modelView[0]);

            glue->glColor4ub(inst.rgba[0], inst.rgba[1], inst.rgba[2], inst.rgba[3]);
            applyWireRasterStyle(glue, inst, caps_.hasLineStipple);

            const size_t flatPointCount =
                wire.segmentCountAtLevel(
                    assembly.effectiveProgressiveLodLevel(
                        inst.lodLevel)) * 2;
            const uint8_t drawLevel = progressiveLevel(
                assembly.effectiveProgressiveLodLevel(inst.lodLevel),
                wire.progressiveMinimumLevel,
                wire.progressiveResidentLevel);
            if (flatPointCount > 0) {
                glue->glBegin(GL_LINES);
                for (size_t i = 0; i + 1 < flatPointCount; i += 2) {
                    const SbVec3f a = wire.isProgressive() ?
                        progressiveSnapPoint(wire.segmentPoints[i],
                            wire.progressiveQuantizationMinimum,
                            wire.progressiveQuantizationMaximum,
                            drawLevel) : wire.segmentPoints[i];
                    const SbVec3f b = wire.isProgressive() ?
                        progressiveSnapPoint(wire.segmentPoints[i + 1],
                            wire.progressiveQuantizationMinimum,
                            wire.progressiveQuantizationMaximum,
                            drawLevel) : wire.segmentPoints[i + 1];
                    glue->glVertex3f(a[0], a[1], a[2]);
                    glue->glVertex3f(b[0], b[1], b[2]);
                }
                glue->glEnd();
            }

            for (const auto& poly : wire.polylines) {
                if (poly.points.size() < 2) continue;
                glue->glBegin(GL_LINE_STRIP);
                for (const auto& pt : poly.points) {
                    glue->glVertex3f(pt[0], pt[1], pt[2]);
                }
                glue->glEnd();
            }
        }
    }

    restoreWireRasterState(glue, rasterState, caps_.hasLineStipple);

    // --- Shaded pass ---
    GLboolean wasColorMaterial = glue->glIsEnabled(GL_COLOR_MATERIAL);
    GLint wasTwoSidedLighting = GL_FALSE;
    glue->glGetIntegerv(GL_LIGHT_MODEL_TWO_SIDE, &wasTwoSidedLighting);
    if (drawShaded && !plan.shadedItems.empty()) {
        // Shaded CAD geometry always uses its normals, regardless of the
        // lighting state inherited from the surrounding scene graph.
        glue->glEnable(GL_LIGHTING);
        glue->glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
    }
    glue->glDisable(GL_COLOR_MATERIAL);
    if (drawShaded) for (const auto& item : plan.shadedItems) {
        const Obol::PartGeometry* geom = assembly.partGeometry(item.rep.part);
        if (!geom || !geom->shaded.has_value()) continue;
        const Obol::TriMesh& mesh = *geom->shaded;

        setCadBackfaceCulling(glue, item.cullBackfaces);
        const bool hasNorm = !mesh.normals.empty();

        for (uint32_t ii = 0; ii < item.instanceCount; ++ii) {
            const size_t instanceIndex = item.baseInstance + ii;
            if (!drawItemOwnsInstance(plan, item, instanceIndex) ||
                    isHiddenInstance(plan, instanceIndex))
                continue;
            const auto& inst = plan.visibleInstances[instanceIndex];
            if (isBoxOutsideFrustum(inst.wbMin, inst.wbMax, fp)) continue;

            SbMatrix model;
            model.setValue(inst.transform.data());
            SbMatrix modelView = model;
            modelView.multRight(viewMatrix);
            glue->glLoadMatrixf(modelView[0]);

            setImmediateMaterialFromRgba(glue, inst.rgba.data());

            const std::vector<uint32_t>& drawIdx = mesh.indices;
            const size_t drawIndexCount =
                mesh.indexCountAtLevel(
                    assembly.effectiveProgressiveLodLevel(
                        inst.lodLevel));
            const uint8_t drawLevel = progressiveLevel(
                assembly.effectiveProgressiveLodLevel(inst.lodLevel),
                mesh.progressiveMinimumLevel,
                mesh.progressiveResidentLevel);

            glue->glBegin(GL_TRIANGLES);
            for (size_t t = 0; t + 2 < drawIndexCount; t += 3) {
                SbVec3f triangle[3];
                for (int k = 0; k < 3; ++k) {
                    uint32_t idx = drawIdx[t + k];
                    triangle[k] = mesh.isProgressive() ?
                        progressiveSnapPoint(mesh.positions[idx],
                            mesh.progressiveQuantizationMinimum,
                            mesh.progressiveQuantizationMaximum,
                            drawLevel) : mesh.positions[idx];
                }
                if (!hasNorm) {
                    SbVec3f faceNormal =
                        (triangle[1] - triangle[0]).cross(
                            triangle[2] - triangle[0]);
                    if (faceNormal.sqrLength() > 0.0f)
                        faceNormal.normalize();
                    else
                        faceNormal.setValue(0.0f, 0.0f, 1.0f);
                    glue->glNormal3f(faceNormal[0], faceNormal[1],
                                     faceNormal[2]);
                }
                for (int k = 0; k < 3; ++k) {
                    uint32_t idx = drawIdx[t + k];
                    if (hasNorm && idx < mesh.normals.size()) {
                        const auto& n = mesh.normals[idx];
                        glue->glNormal3f(n[0], n[1], n[2]);
                    }
                    const SbVec3f& p = triangle[k];
                    glue->glVertex3f(p[0], p[1], p[2]);
                }
            }
            glue->glEnd();
        }
    }
    if (wasColorMaterial) glue->glEnable(GL_COLOR_MATERIAL);
    else glue->glDisable(GL_COLOR_MATERIAL);
    glue->glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, wasTwoSidedLighting);
    if (wasLighting) glue->glEnable(GL_LIGHTING);
    else glue->glDisable(GL_LIGHTING);

    // Restore matrix state
    glue->glMatrixMode(GL_MODELVIEW);
    glue->glPopMatrix();
    glue->glMatrixMode(GL_PROJECTION);
    glue->glPopMatrix();
    glue->glMatrixMode(GL_MODELVIEW);
}

// ---------------------------------------------------------------------------
// Tier-2: instanced rendering (GL 3.1+)
// ---------------------------------------------------------------------------

bool CadRendererGL::rejectIndirect(int status, const char *reason)
{
    lastIndirectStatus_ = status;
    if (std::getenv("OBOL_CAD_INDIRECT_DEBUG") &&
            reportedIndirectStatus_ != status) {
        std::fprintf(stderr,
            "CadRendererGL indirect rejected status=%d reason=%s\n",
            status, reason ? reason : "unknown");
        reportedIndirectStatus_ = status;
    }
    return false;
}

bool CadRendererGL::submitIndirectPrepared(
        const SoGLContext *glue,
        const SbMatrix& viewProj,
        const SbViewVolume& viewVolume)
{
    if (!glue || !gpuRes_ || !indirectPrepared_.valid)
        return false;
    if (indirectPrepared_.pages.empty()) {
        if (indirectPrepared_.pressureProxyPoints.empty())
            return false;
        lastIndirectStatus_ = 0;
        reportedIndirectStatus_ = 0;
        return true;
    }

    if (indirectPrepared_.instances.empty())
        return rejectIndirect(9, "empty prepared instance stream");
    if (!gpuRes_->instanceVbo() ||
            gpuRes_->instanceUploadSerial() !=
                indirectPrepared_.instanceUploadSerial) {
        gpuRes_->uploadInstanceData(
            indirectPrepared_.instances.data(),
            static_cast<GLsizeiptr>(
                indirectPrepared_.instances.size() * sizeof(InstVertex)),
            glue);
        indirectPrepared_.instanceUploadSerial =
            gpuRes_->instanceUploadSerial();
    }
    const GLuint instanceVbo = gpuRes_->instanceVbo();
    if (!instanceVbo)
        return rejectIndirect(10, "prepared instance upload");

    for (const IndirectPageWork& work : indirectPrepared_.pages) {
        CadTriangleAtlasPage *page =
            gpuRes_->triangleAtlasPage(work.page);
        if (!page || !page->indirectBuf || !page->indirectCapacity ||
                (work.ordinary.empty() && work.culled.empty()))
            return rejectIndirect(11, "prepared page preflight");

        const bool newVao = !page->vao;
        if (newVao)
            glue->glGenVertexArrays(1, &page->vao);
        glue->glBindVertexArray(page->vao);
        if (newVao) {
            glue->glBindBuffer(GL_ARRAY_BUFFER, page->posBuf);
            glue->glVertexAttribPointerARB(
                0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
            glue->glEnableVertexAttribArrayARB(0);
            glue->glBindBuffer(
                GL_ARRAY_BUFFER,
                page->normBuf ? page->normBuf : page->posBuf);
            glue->glVertexAttribPointerARB(
                1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
            glue->glEnableVertexAttribArrayARB(1);
            glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, page->idxBuf);
        }
        if (newVao || page->instanceVbo != instanceVbo) {
            const GLsizei stride =
                static_cast<GLsizei>(sizeof(InstVertex));
            glue->glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
            for (GLuint column = 0; column < 4; ++column) {
                const GLuint location = kInstTransformLoc + column;
                const uintptr_t offset =
                    offsetof(InstVertex, transform) +
                    column * 4u * sizeof(float);
                glue->glVertexAttribPointerARB(
                    location, 4, GL_FLOAT, GL_FALSE, stride,
                    reinterpret_cast<const GLvoid *>(offset));
                glue->glEnableVertexAttribArrayARB(location);
                glue->glVertexAttribDivisor(location, 1);
            }
            glue->glVertexAttribPointerARB(
                kInstColorLoc, 4, GL_FLOAT, GL_FALSE, stride,
                reinterpret_cast<const GLvoid *>(
                    offsetof(InstVertex, color)));
            glue->glEnableVertexAttribArrayARB(kInstColorLoc);
            glue->glVertexAttribDivisor(kInstColorLoc, 1);
            glue->glVertexAttribPointerARB(
                kInstPopMinLevelLoc, 4, GL_FLOAT, GL_FALSE, stride,
                reinterpret_cast<const GLvoid *>(
                    offsetof(InstVertex, popMinLevel)));
            glue->glEnableVertexAttribArrayARB(kInstPopMinLevelLoc);
            glue->glVertexAttribDivisor(kInstPopMinLevelLoc, 1);
            glue->glVertexAttribPointerARB(
                kInstPopMaxFlagsLoc, 4, GL_FLOAT, GL_FALSE, stride,
                reinterpret_cast<const GLvoid *>(
                    offsetof(InstVertex, popMaxFlags)));
            glue->glEnableVertexAttribArrayARB(kInstPopMaxFlagsLoc);
            glue->glVertexAttribDivisor(kInstPopMaxFlagsLoc, 1);
            page->instanceVbo = instanceVbo;
        }
        glue->glBindVertexArray(0);
    }
    glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
    glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glue->glUseProgramObjectARB(shaders_.shadedIndirect);
    const GLint viewProjection = glue->glGetUniformLocationARB(
        shaders_.shadedIndirect, "u_viewProj");
    glue->glUniformMatrix4fvARB(
        viewProjection, 1, GL_FALSE, viewProj[0]);
    uploadLights(glue, shaders_.shadedIndirect);
    uploadViewFacing(glue, shaders_.shadedIndirect, viewVolume);

    for (const IndirectPageWork& work : indirectPrepared_.pages) {
        const CadTriangleAtlasPage *page =
            gpuRes_->triangleAtlasPage(work.page);
        if (!page) continue;
        glue->glBindVertexArray(page->vao);
        const auto drawCommands =
            [&](const std::vector<CadDrawElementsIndirectCommand>& commands,
                bool cullBackfaces) {
                setCadBackfaceCulling(glue, cullBackfaces);
                size_t offset = 0;
                while (offset < commands.size()) {
                    const size_t count = std::min<size_t>(
                        commands.size() - offset,
                        page->indirectCapacity);
                    if (!gpuRes_->uploadTriangleAtlasCommands(
                            work.page, commands.data() + offset,
                            count, glue))
                        return false;
                    glue->glBindBuffer(
                        GL_DRAW_INDIRECT_BUFFER, page->indirectBuf);
                    glue->glMultiDrawElementsIndirect(
                        GL_TRIANGLES, GL_UNSIGNED_INT, nullptr,
                        static_cast<GLsizei>(count),
                        sizeof(CadDrawElementsIndirectCommand));
                    offset += count;
                }
                return true;
            };
        if (!drawCommands(work.ordinary, false) ||
                !drawCommands(work.culled, true)) {
            glue->glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
            glue->glBindVertexArray(0);
            glue->glUseProgramObjectARB(0);
            gpuRes_->releaseFlatShaded(glue);
            gpuRes_->releaseStandaloneTriangles(glue);
            lastIndirectStatus_ = 12;
            return true;
        }
    }
    glue->glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    glue->glBindVertexArray(0);
    glue->glUseProgramObjectARB(0);
    gpuRes_->releaseFlatShaded(glue);
    gpuRes_->releaseStandaloneTriangles(glue);
    lastIndirectStatus_ = 0;
    reportedIndirectStatus_ = 0;
    return true;
}

bool CadRendererGL::patchIndirectPreparedAppend(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const SoGLContext *glue,
        const SbMatrix& viewProj)
{
    const auto fail = [&](const char *reason) {
        if (std::getenv("OBOL_CAD_PATCH_DEBUG"))
            std::fprintf(stderr,
                "CadRendererGL append patch detail reason=%s "
                "prepared_append=%llu plan_append=%llu "
                "source_extent=%zu plan_sources=%zu\n",
                reason ? reason : "unknown",
                static_cast<unsigned long long>(
                    indirectPrepared_.appendRevision),
                static_cast<unsigned long long>(
                    plan.appendRevision),
                indirectPrepared_.instanceIndexBySource.size(),
                plan.visibleInstances.size());
        return false;
    };
    if (indirectPrepared_.appendRevision ==
            plan.appendRevision)
        return true;
    if (!glue || !gpuRes_ ||
            indirectPrepared_.appendRevision <
                plan.appendDeltaFloorRevision)
        return fail("journal-floor");

    std::vector<const CadPlanAppendDelta *> deltas;
    for (const CadPlanAppendDelta& delta :
            plan.appendDeltas) {
        if (delta.revision >
                indirectPrepared_.appendRevision)
            deltas.push_back(&delta);
    }
    if (deltas.empty())
        return fail("empty-journal");

    /*
     * Keep append publication cheap without making the retained submission
     * an unbounded journal replay.  Once the shaded tail has grown by either
     * one exact-frame population or a modest startup quantum, ask the caller
     * for one exact preparation.  The anchor then doubles, so the complete
     * cost over a stream is a geometric series (O(final population)), while
     * stale/tombstoned commands, reverse indices, atlas bindings, and packed
     * instances are periodically cross-checked and compacted together.
     *
     * This boundary is based on structural growth, not elapsed time or frame
     * count, so a fast producer does not cause more work than a slow one and
     * camera interaction never triggers it by itself.
     */
    constexpr size_t minimumAppendGrowth = 4096u;
    size_t appendedCandidateCount = 0;
    for (const CadPlanAppendDelta *delta : deltas) {
        if (!delta ||
                delta->shadedItemCount >
                    std::numeric_limits<size_t>::max() -
                        appendedCandidateCount)
            return fail("candidate-overflow");
        appendedCandidateCount += delta->shadedItemCount;
    }
    const size_t appendAnchor =
        indirectPrepared_.appendPatchAnchorInstanceCount;
    const size_t growthAllowance =
        std::max(minimumAppendGrowth, appendAnchor);
    const size_t packedLimit =
        appendAnchor >
                std::numeric_limits<size_t>::max() - growthAllowance ?
            std::numeric_limits<size_t>::max() :
            appendAnchor + growthAllowance;
    if (indirectPrepared_.instances.size() > packedLimit ||
            appendedCandidateCount >
                packedLimit - indirectPrepared_.instances.size()) {
        if (std::getenv("OBOL_CAD_PATCH_DEBUG"))
            std::fprintf(stderr,
                "CadRendererGL retained append patch requests geometric "
                "revalidation anchor=%zu packed=%zu candidates=%zu "
                "limit=%zu\n",
                appendAnchor, indirectPrepared_.instances.size(),
                appendedCandidateCount, packedLimit);
        return false;
    }

    const uint32_t noSlot =
        std::numeric_limits<uint32_t>::max();
    size_t sourceExtent =
        indirectPrepared_.instanceIndexBySource.size();
    const size_t priorPackedInstanceCount =
        indirectPrepared_.instances.size();
    const size_t priorPressureProxyCount =
        indirectPrepared_.pressureProxyPoints.size();
    bool pressureProxyAdded = false;
    const FrustumPlanes fp =
        extractFrustumPlanes(viewProj);
    /*
     * The exact path touches every old part before pressure reclamation.
     * This append path deliberately does not rescan them: preserve that
     * already validated working set and fall back to exact preparation only
     * if free/new atlas capacity cannot admit the tail.
     */
    gpuRes_->deferTriangleAtlasReclamation();

    for (const CadPlanAppendDelta *delta : deltas) {
        if (!delta ||
                delta->visibleBegin != sourceExtent ||
                delta->visibleBegin >
                    plan.visibleInstances.size() ||
                delta->visibleCount >
                    plan.visibleInstances.size() -
                        delta->visibleBegin ||
                delta->partBegin >
                    plan.partBindings.size() ||
                delta->partCount >
                    plan.partBindings.size() -
                        delta->partBegin ||
                delta->shadedItemBegin >
                    plan.shadedItems.size() ||
                delta->shadedItemCount >
                    plan.shadedItems.size() -
                        delta->shadedItemBegin)
            return fail("delta-shape");
        for (const uint32_t retired :
                delta->retiredVisibleIndices) {
            if (retired >= sourceExtent)
                return fail("retired-source-range");
            if (retired <
                    indirectPrepared_.
                        instanceIndexBySource.size() &&
                    indirectPrepared_.
                        instanceIndexBySource[retired] != noSlot)
                return fail("retired-packed-instance");
            if (retired <
                    indirectPrepared_.
                        pressureProxyIndexBySource.size() &&
                    indirectPrepared_.
                        pressureProxyIndexBySource[retired] != noSlot)
                return fail("retired-pressure-proxy");
        }

        const size_t newSourceExtent =
            static_cast<size_t>(delta->visibleBegin) +
            delta->visibleCount;
        indirectPrepared_.instanceIndexBySource.resize(
            newSourceExtent, noSlot);
        indirectPrepared_.pressureProxyIndexBySource.resize(
            newSourceExtent, noSlot);
        indirectPrepared_.partByPlanPartIndex.resize(
            static_cast<size_t>(delta->partBegin) +
                delta->partCount,
            noSlot);

        const size_t shadedEnd =
            static_cast<size_t>(delta->shadedItemBegin) +
            delta->shadedItemCount;
        for (size_t itemIndex =
                delta->shadedItemBegin;
                itemIndex < shadedEnd; ++itemIndex) {
            const CadDrawItem& item =
                plan.shadedItems[itemIndex];
            if (!item.instanceCount)
                continue;
            /*
             * Append-only realization batches group by part.  Unique leaves
             * are the scalable fast path; shared occurrence runs retain the
             * exact builder until a run-level append journal is available.
             */
            if (item.instanceCount != 1u ||
                    item.baseInstance < delta->visibleBegin ||
                    item.baseInstance >= newSourceExtent ||
                    item.partIndex < delta->partBegin ||
                    item.partIndex >=
                        static_cast<size_t>(delta->partBegin) +
                            delta->partCount ||
                    item.partIndex >= plan.partBindings.size())
                return fail("item-shape");
            const uint32_t sourceIndex =
                item.baseInstance;
            const CadVisibleInstance& source =
                plan.visibleInstances[sourceIndex];
            if (source.partIndex != item.partIndex)
                return fail("source-part");
            if ((source.flags & CadInstanceHidden) ||
                    isSubpixelProxyInstance(
                        plan, sourceIndex) ||
                    isBoxOutsideFrustum(
                        source.wbMin, source.wbMax, fp))
                continue;

            const CadPartBinding& binding =
                plan.partBindings[item.partIndex];
            if (!binding.geometry ||
                    !binding.geometry->shaded)
                return fail("missing-geometry");
            const TriMesh& mesh =
                *binding.geometry->shaded;
            uint8_t level = mesh.isProgressive() ?
                progressiveLevel(
                    assembly.effectiveProgressiveLodLevel(
                        source.lodLevel),
                    mesh.progressiveMinimumLevel,
                    mesh.progressiveResidentLevel) :
                15u;
            const size_t vertexCount = mesh.isProgressive() ?
                mesh.positionCountAtLevel(level) :
                mesh.positions.size();
            const size_t indexCount = mesh.isProgressive() ?
                mesh.indexCountAtLevel(level) :
                mesh.indices.size();
            if (!vertexCount || !indexCount ||
                    vertexCount >
                        std::numeric_limits<uint32_t>::max() ||
                    indexCount >
                        std::numeric_limits<uint32_t>::max())
                return fail("prefix-count");
            const CadTriangleAtlasPart *atlas =
                gpuRes_->upsertTriangleAtlasPart(
                    binding.part, binding.generation,
                    packedVec3fData(mesh.positions),
                    packedVec3fData(mesh.normals),
                    static_cast<uint32_t>(vertexCount),
                    mesh.indices.data(),
                    static_cast<uint32_t>(indexCount),
                    mesh.isProgressive(), glue, caps_);
            if (!atlas) {
                /*
                 * Atlas pressure is a normal bounded-memory outcome, not an
                 * append-journal failure.  Exact preparation already turns
                 * an eligible unadmitted occurrence into one aggregate
                 * point.  Do the same here so a stream which has reached its
                 * GPU working-set ceiling does not rebuild the entire scene
                 * for every subsequent publication batch.
                 */
                if (!binding.subpixelProxyEligible ||
                        indirectPrepared_.pressureProxyPoints.size() >=
                            std::numeric_limits<uint32_t>::max())
                    return fail("atlas-admission");
                SbVec3f localCenter(0.0f, 0.0f, 0.0f);
                for (const SbVec3f& corner :
                        binding.subpixelProxyCorners)
                    localCenter += corner;
                localCenter /= 8.0f;
                CadSubpixelProxyPoint replacement;
                replacement.position = transformedFlatPoint(
                    localCenter, source.transform);
                replacement.rgba = source.rgba;
                replacement.instanceId = source.instanceId;
                replacement.flags = source.flags;
                const uint32_t proxyIndex =
                    static_cast<uint32_t>(
                        indirectPrepared_.
                            pressureProxyPoints.size());
                indirectPrepared_.pressureProxyPoints.push_back(
                    replacement);
                indirectPrepared_.
                    pressureProxySourceInstanceIndices.push_back(
                        sourceIndex);
                indirectPrepared_.
                    pressureProxyIndexBySource[sourceIndex] =
                        proxyIndex;
                pressureProxyAdded = true;
                continue;
            }
            while (mesh.isProgressive() &&
                    level > mesh.progressiveMinimumLevel &&
                    (mesh.positionCountAtLevel(level) >
                         atlas->vertexCount ||
                     mesh.indexCountAtLevel(level) >
                         atlas->indexCount))
                --level;
            const size_t residentIndexCount =
                mesh.isProgressive() ?
                    mesh.indexCountAtLevel(level) :
                    mesh.indices.size();
            if (!residentIndexCount ||
                    residentIndexCount >
                        std::numeric_limits<uint32_t>::max() ||
                    atlas->vertices.first >
                        static_cast<uint32_t>(
                            std::numeric_limits<int32_t>::max()))
                return fail("resident-count");

            InstVertex target = {};
            std::memcpy(
                target.transform, source.transform.data(),
                16 * sizeof(float));
            target.color[0] = source.rgba[0] / 255.0f;
            target.color[1] = source.rgba[1] / 255.0f;
            target.color[2] = source.rgba[2] / 255.0f;
            target.color[3] = source.rgba[3] / 255.0f;
            const SbVec3f minimum = mesh.isProgressive() ?
                mesh.progressiveQuantizationMinimum :
                SbVec3f(0, 0, 0);
            const SbVec3f maximum = mesh.isProgressive() ?
                mesh.progressiveQuantizationMaximum :
                SbVec3f(0, 0, 0);
            for (int axis = 0; axis < 3; ++axis) {
                target.popMinLevel[axis] = minimum[axis];
                target.popMaxFlags[axis] = maximum[axis];
            }
            target.popMinLevel[3] =
                static_cast<float>(level);
            target.popMaxFlags[3] =
                (!mesh.normals.empty() ? 1.0f : 0.0f) +
                (mesh.isProgressive() ? 2.0f : 0.0f);
            const uint32_t packedInstance =
                static_cast<uint32_t>(
                    indirectPrepared_.instances.size());
            indirectPrepared_.instances.push_back(target);
            indirectPrepared_.
                sourceInstanceIndices.push_back(sourceIndex);
            indirectPrepared_.
                instanceIndexBySource[sourceIndex] =
                    packedInstance;

            IndirectPageWork *pageWork = nullptr;
            for (IndirectPageWork& candidate :
                    indirectPrepared_.pages) {
                if (candidate.page == atlas->page) {
                    pageWork = &candidate;
                    break;
                }
            }
            if (!pageWork) {
                IndirectPageWork work;
                work.page = atlas->page;
                indirectPrepared_.pages.push_back(
                    std::move(work));
                pageWork =
                    &indirectPrepared_.pages.back();
            }
            auto& commands = item.cullBackfaces ?
                pageWork->culled : pageWork->ordinary;
            CadDrawElementsIndirectCommand command;
            command.count =
                static_cast<uint32_t>(
                    residentIndexCount);
            command.instanceCount = 1u;
            command.firstIndex =
                atlas->indices.first;
            command.baseVertex =
                static_cast<int32_t>(
                    atlas->vertices.first);
            command.baseInstance =
                packedInstance;
            const uint32_t commandIndex =
                static_cast<uint32_t>(
                    commands.size());
            commands.push_back(command);

            IndirectPreparedPart demand;
            demand.part = binding.part;
            demand.partIndex = item.partIndex;
            demand.generation =
                binding.generation;
            demand.vertexCount =
                static_cast<uint32_t>(vertexCount);
            demand.indexCount =
                static_cast<uint32_t>(indexCount);
            demand.page = atlas->page;
            demand.vertexFirst =
                atlas->vertices.first;
            demand.indexFirst =
                atlas->indices.first;
            demand.hasNormals =
                !mesh.normals.empty();
            demand.packedInstance =
                packedInstance;
            demand.commandIndex =
                commandIndex;
            demand.commandCulled =
                item.cullBackfaces;
            indirectPrepared_.
                partByPlanPartIndex[item.partIndex] =
                    static_cast<uint32_t>(
                        indirectPrepared_.parts.size());
            indirectPrepared_.parts.push_back(
                demand);
            indirectPrepared_.renderedTriangleCount +=
                residentIndexCount / 3u;
        }
        sourceExtent = newSourceExtent;
    }

    const size_t appendedPackedCount =
        indirectPrepared_.instances.size() -
        priorPackedInstanceCount;
    if (appendedPackedCount) {
        const GLintptr byteOffset =
            static_cast<GLintptr>(
                priorPackedInstanceCount *
                sizeof(InstVertex));
        const GLsizeiptr byteCount =
            static_cast<GLsizeiptr>(
                appendedPackedCount *
                sizeof(InstVertex));
        bool uploaded =
            gpuRes_->instanceVbo() &&
            gpuRes_->instanceUploadSerial() ==
                indirectPrepared_.instanceUploadSerial &&
            gpuRes_->appendInstanceData(
                byteOffset,
                indirectPrepared_.instances.data() +
                    priorPackedInstanceCount,
                byteCount, glue);
        if (!uploaded) {
            gpuRes_->uploadInstanceData(
                indirectPrepared_.instances.data(),
                static_cast<GLsizeiptr>(
                    indirectPrepared_.instances.size() *
                    sizeof(InstVertex)),
                glue);
            uploaded =
                gpuRes_->instanceVbo() != 0;
        }
        if (!uploaded)
            return fail("instance-upload");
        indirectPrepared_.instanceUploadSerial =
            gpuRes_->instanceUploadSerial();
    }
    if (pressureProxyAdded) {
        pressureProxyAppendBaseRevision_ =
            pressureProxyRevision_;
        pressureProxyAppendBegin_ =
            priorPressureProxyCount;
        pressureProxyAppendOnly_ = true;
        ++pressureProxyRevision_;
        if (!pressureProxyRevision_)
            pressureProxyRevision_ = 1;
    }

    indirectPrepared_.appendRevision =
        plan.appendRevision;
    indirectPrepared_.planRevision =
        plan.revision;
    indirectPrepared_.geometryRevision =
        plan.geometryRevision;
    indirectPrepared_.shadedLayoutRevision =
        plan.shadedLayoutRevision;
    indirectPrepared_.subpixelProxyRevision =
        plan.subpixelProxyRevision;
    indirectPrepared_.atlasRevision =
        gpuRes_->triangleAtlasRevision();
    indirectPrepared_.atlasValidationCountdown = 30u;
    return true;
}

bool CadRendererGL::patchIndirectPreparedGeometry(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const SoGLContext *glue)
{
    const auto fail = [&](const char *reason) {
        if (std::getenv("OBOL_CAD_PATCH_DEBUG"))
            std::fprintf(stderr,
                "CadRendererGL geometry patch detail reason=%s\n",
                reason ? reason : "unknown");
        return false;
    };
    if (indirectPrepared_.partGeometryRevision ==
            plan.partGeometryRevision)
        return true;
    if (!glue || !gpuRes_ ||
            indirectPrepared_.partGeometryRevision <
                plan.partGeometryDeltaFloorRevision)
        return fail("journal-floor");

    std::vector<CadPartGeometryRange> changedRanges;
    for (const CadPartGeometryDelta& delta :
            plan.partGeometryDeltas) {
        if (delta.revision <=
                indirectPrepared_.partGeometryRevision)
            continue;
        changedRanges.insert(
            changedRanges.end(),
            delta.ranges.begin(), delta.ranges.end());
    }
    if (changedRanges.empty())
        return fail("empty-journal");
    std::sort(changedRanges.begin(), changedRanges.end(),
        [](const auto& left, const auto& right) {
            if (left.partIndex != right.partIndex)
                return left.partIndex < right.partIndex;
            return left.baseInstance < right.baseInstance;
        });
    changedRanges.erase(
        std::unique(changedRanges.begin(), changedRanges.end(),
            [](const auto& left, const auto& right) {
                return left.partIndex == right.partIndex &&
                    left.baseInstance == right.baseInstance;
            }),
        changedRanges.end());

    const uint32_t noSlot =
        std::numeric_limits<uint32_t>::max();
    const FrustumPlanes fp =
        extractFrustumPlanes(
            indirectPrepared_.viewProj);
    std::vector<uint32_t> changedPackedInstances;
    changedPackedInstances.reserve(changedRanges.size());
    gpuRes_->deferTriangleAtlasReclamation();

    for (const CadPartGeometryRange& range :
            changedRanges) {
        /*
         * Wire/point-only part updates are consumed by their own retained
         * paths and do not alter this shaded indirect submission.
         */
        if (!range.shadedItemCount)
            continue;
        if (range.instanceCount != 1u ||
                range.shadedItemCount != 1u ||
                range.baseInstance >=
                    plan.visibleInstances.size() ||
                range.partIndex >=
                    plan.partBindings.size() ||
                range.shadedItemBegin >=
                    plan.shadedItems.size() ||
                range.partIndex >=
                    indirectPrepared_.
                        partByPlanPartIndex.size())
            return fail("range-shape");
        const uint32_t sourceIndex =
            range.baseInstance;
        const CadVisibleInstance& source =
            plan.visibleInstances[sourceIndex];
        const uint32_t preparedPartIndex =
            indirectPrepared_.
                partByPlanPartIndex[range.partIndex];
        const bool shouldDraw =
            !(source.flags & CadInstanceHidden) &&
            !isSubpixelProxyInstance(
                plan, sourceIndex) &&
            !isBoxOutsideFrustum(
                source.wbMin, source.wbMax, fp);
        if (!shouldDraw) {
            if (preparedPartIndex != noSlot)
                return fail("demotion");
            continue;
        }
        if (preparedPartIndex == noSlot ||
                preparedPartIndex >=
                    indirectPrepared_.parts.size()) {
            /*
             * An exact admission pass may intentionally represent this
             * occurrence with the aggregate pressure proxy.  A richer PoP
             * generation does not promote it past the scene budget; retain
             * the proxy and consume the geometry journal entry.
             */
            if (sourceIndex <
                    indirectPrepared_.
                        pressureProxyIndexBySource.size() &&
                    indirectPrepared_.
                        pressureProxyIndexBySource[sourceIndex] != noSlot)
                continue;
            return fail("promotion");
        }
        IndirectPreparedPart& demand =
            indirectPrepared_.parts[
                preparedPartIndex];
        const CadPartBinding& binding =
            plan.partBindings[range.partIndex];
        if (!(demand.part == binding.part) ||
                demand.partIndex != range.partIndex ||
                demand.packedInstance == noSlot ||
                demand.packedInstance >=
                    indirectPrepared_.instances.size() ||
                demand.commandIndex == noSlot ||
                !binding.geometry ||
                !binding.geometry->shaded)
            return fail("prepared-binding");
        const TriMesh& mesh =
            *binding.geometry->shaded;
        uint8_t level = mesh.isProgressive() ?
            progressiveLevel(
                assembly.effectiveProgressiveLodLevel(
                    source.lodLevel),
                mesh.progressiveMinimumLevel,
                mesh.progressiveResidentLevel) :
            15u;
        const size_t vertexCount = mesh.isProgressive() ?
            mesh.positionCountAtLevel(level) :
            mesh.positions.size();
        const size_t indexCount = mesh.isProgressive() ?
            mesh.indexCountAtLevel(level) :
            mesh.indices.size();
        if (!vertexCount || !indexCount ||
                vertexCount >
                    std::numeric_limits<uint32_t>::max() ||
                indexCount >
                    std::numeric_limits<uint32_t>::max())
            return fail("counts");
        const CadTriangleAtlasPart *atlas =
            gpuRes_->upsertTriangleAtlasPart(
                binding.part, binding.generation,
                packedVec3fData(mesh.positions),
                packedVec3fData(mesh.normals),
                static_cast<uint32_t>(vertexCount),
                mesh.indices.data(),
                static_cast<uint32_t>(indexCount),
                mesh.isProgressive(), glue, caps_);
        if (!atlas)
            return fail("atlas-admission");
        while (mesh.isProgressive() &&
                level > mesh.progressiveMinimumLevel &&
                (mesh.positionCountAtLevel(level) >
                     atlas->vertexCount ||
                 mesh.indexCountAtLevel(level) >
                     atlas->indexCount))
            --level;
        const size_t residentIndexCount =
            mesh.isProgressive() ?
                mesh.indexCountAtLevel(level) :
                mesh.indices.size();
        if (!residentIndexCount ||
                residentIndexCount >
                    std::numeric_limits<uint32_t>::max() ||
                atlas->vertices.first >
                    static_cast<uint32_t>(
                        std::numeric_limits<int32_t>::max()))
            return fail("resident-count");

        IndirectPageWork *oldPageWork = nullptr;
        for (IndirectPageWork& candidate :
                indirectPrepared_.pages) {
            if (candidate.page == demand.page) {
                oldPageWork = &candidate;
                break;
            }
        }
        if (!oldPageWork)
            return fail("old-page");
        auto& oldCommands = demand.commandCulled ?
            oldPageWork->culled :
            oldPageWork->ordinary;
        if (demand.commandIndex >=
                oldCommands.size())
            return fail("old-command-index");
        CadDrawElementsIndirectCommand& oldCommand =
            oldCommands[demand.commandIndex];
        if (oldCommand.instanceCount != 1u ||
                oldCommand.baseInstance !=
                    demand.packedInstance)
            return fail("old-command-shape");
        const uint64_t oldTriangles =
            oldCommand.count / 3u;

        IndirectPageWork *newPageWork = nullptr;
        for (IndirectPageWork& candidate :
                indirectPrepared_.pages) {
            if (candidate.page == atlas->page) {
                newPageWork = &candidate;
                break;
            }
        }
        if (!newPageWork) {
            IndirectPageWork work;
            work.page = atlas->page;
            indirectPrepared_.pages.push_back(
                std::move(work));
            newPageWork =
                &indirectPrepared_.pages.back();
            /*
             * The outer-vector append may have invalidated oldPageWork.
             * Resolve it again before retiring the preceding command.
             */
            oldPageWork = nullptr;
            for (IndirectPageWork& candidate :
                    indirectPrepared_.pages) {
                if (candidate.page == demand.page) {
                    oldPageWork = &candidate;
                    break;
                }
            }
            if (!oldPageWork)
                return fail("old-page-relocation");
        }
        auto& refreshedOldCommands =
            demand.commandCulled ?
                oldPageWork->culled :
                oldPageWork->ordinary;
        if (demand.commandIndex >=
                refreshedOldCommands.size())
            return fail("refreshed-command-index");
        refreshedOldCommands[
            demand.commandIndex].instanceCount = 0u;

        const bool commandCulled =
            plan.shadedItems[
                range.shadedItemBegin].cullBackfaces;
        auto& newCommands = commandCulled ?
            newPageWork->culled :
            newPageWork->ordinary;
        CadDrawElementsIndirectCommand command;
        command.count =
            static_cast<uint32_t>(
                residentIndexCount);
        command.instanceCount = 1u;
        command.firstIndex =
            atlas->indices.first;
        command.baseVertex =
            static_cast<int32_t>(
                atlas->vertices.first);
        command.baseInstance =
            demand.packedInstance;
        const uint32_t commandIndex =
            static_cast<uint32_t>(
                newCommands.size());
        newCommands.push_back(command);

        InstVertex& target =
            indirectPrepared_.instances[
                demand.packedInstance];
        const SbVec3f minimum = mesh.isProgressive() ?
            mesh.progressiveQuantizationMinimum :
            SbVec3f(0, 0, 0);
        const SbVec3f maximum = mesh.isProgressive() ?
            mesh.progressiveQuantizationMaximum :
            SbVec3f(0, 0, 0);
        for (int axis = 0; axis < 3; ++axis) {
            target.popMinLevel[axis] = minimum[axis];
            target.popMaxFlags[axis] = maximum[axis];
        }
        target.popMinLevel[3] =
            static_cast<float>(level);
        target.popMaxFlags[3] =
            (!mesh.normals.empty() ? 1.0f : 0.0f) +
            (mesh.isProgressive() ? 2.0f : 0.0f);
        changedPackedInstances.push_back(
            demand.packedInstance);

        demand.generation =
            binding.generation;
        demand.vertexCount =
            static_cast<uint32_t>(vertexCount);
        demand.indexCount =
            static_cast<uint32_t>(indexCount);
        demand.page = atlas->page;
        demand.vertexFirst =
            atlas->vertices.first;
        demand.indexFirst =
            atlas->indices.first;
        demand.hasNormals =
            !mesh.normals.empty();
        demand.commandIndex =
            commandIndex;
        demand.commandCulled =
            commandCulled;
        const uint64_t newTriangles =
            residentIndexCount / 3u;
        indirectPrepared_.renderedTriangleCount =
            oldTriangles <=
                    indirectPrepared_.
                        renderedTriangleCount ?
                indirectPrepared_.
                    renderedTriangleCount -
                    oldTriangles + newTriangles :
                newTriangles;
    }

    indirectPrepared_.pages.erase(
        std::remove_if(
            indirectPrepared_.pages.begin(),
            indirectPrepared_.pages.end(),
            [&](const IndirectPageWork& work) {
                if (!gpuRes_->triangleAtlasPage(
                        work.page))
                    return true;
                const auto active =
                    [](const auto& commands) {
                        return std::any_of(
                            commands.begin(),
                            commands.end(),
                            [](const auto& command) {
                                return command.instanceCount != 0u;
                            });
                    };
                return !active(work.ordinary) &&
                    !active(work.culled);
            }),
        indirectPrepared_.pages.end());

    std::sort(changedPackedInstances.begin(),
        changedPackedInstances.end());
    changedPackedInstances.erase(
        std::unique(changedPackedInstances.begin(),
            changedPackedInstances.end()),
        changedPackedInstances.end());
    bool sparseUpload =
        changedPackedInstances.empty() ||
        (gpuRes_->instanceVbo() &&
         gpuRes_->instanceUploadSerial() ==
             indirectPrepared_.instanceUploadSerial);
    size_t begin = 0;
    while (sparseUpload &&
            begin < changedPackedInstances.size()) {
        size_t end = begin + 1u;
        while (end < changedPackedInstances.size() &&
                changedPackedInstances[end] ==
                    changedPackedInstances[end - 1u] + 1u)
            ++end;
        const uint32_t first =
            changedPackedInstances[begin];
        sparseUpload = gpuRes_->updateInstanceData(
            static_cast<GLintptr>(first) *
                sizeof(InstVertex),
            indirectPrepared_.instances.data() + first,
            static_cast<GLsizeiptr>(end - begin) *
                sizeof(InstVertex),
            glue);
        begin = end;
    }
    if (sparseUpload)
        indirectPrepared_.instanceUploadSerial =
            gpuRes_->instanceUploadSerial();
    else
        indirectPrepared_.instanceUploadSerial = 0;

    indirectPrepared_.partGeometryRevision =
        plan.partGeometryRevision;
    indirectPrepared_.planRevision =
        plan.revision;
    indirectPrepared_.geometryRevision =
        plan.geometryRevision;
    indirectPrepared_.shadedLayoutRevision =
        plan.shadedLayoutRevision;
    indirectPrepared_.subpixelProxyRevision =
        plan.subpixelProxyRevision;
    indirectPrepared_.atlasRevision =
        gpuRes_->triangleAtlasRevision();
    indirectPrepared_.atlasValidationCountdown = 30u;
    return true;
}

bool CadRendererGL::patchIndirectPreparedCeiling(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const SoGLContext *glue)
{
    const int ceiling =
        assembly.progressiveLodCeiling.getValue();
    if (indirectPrepared_.progressiveLodCeiling ==
            ceiling)
        return true;
    if (!glue || !gpuRes_)
        return false;

    const uint32_t noSlot =
        std::numeric_limits<uint32_t>::max();
    std::vector<uint32_t> changedPackedInstances;
    changedPackedInstances.reserve(
        indirectPrepared_.parts.size());
    for (IndirectPreparedPart& demand :
            indirectPrepared_.parts) {
        if (demand.partIndex >=
                plan.partBindings.size())
            return false;
        const CadPartBinding& binding =
            plan.partBindings[demand.partIndex];
        if (!(binding.part == demand.part) ||
                !binding.geometry ||
                !binding.geometry->shaded)
            return false;
        const TriMesh& mesh =
            *binding.geometry->shaded;
        if (!mesh.isProgressive())
            continue;
        if (demand.packedInstance == noSlot ||
                demand.packedInstance >=
                    indirectPrepared_.instances.size() ||
                demand.packedInstance >=
                    indirectPrepared_.
                        sourceInstanceIndices.size() ||
                demand.commandIndex == noSlot)
            return false;
        const uint32_t sourceIndex =
            indirectPrepared_.
                sourceInstanceIndices[
                    demand.packedInstance];
        if (sourceIndex >=
                plan.visibleInstances.size())
            return false;
        uint8_t level = progressiveLevel(
            assembly.effectiveProgressiveLodLevel(
                plan.visibleInstances[sourceIndex].lodLevel),
            mesh.progressiveMinimumLevel,
            mesh.progressiveResidentLevel);
        const CadTriangleAtlasPart *atlas =
            gpuRes_->triangleAtlasPart(
                binding.part);
        if (!atlas || atlas->page != demand.page ||
                atlas->vertices.first !=
                    demand.vertexFirst ||
                atlas->indices.first !=
                    demand.indexFirst)
            return false;
        while (level > mesh.progressiveMinimumLevel &&
                (mesh.positionCountAtLevel(level) >
                     atlas->vertexCount ||
                 mesh.indexCountAtLevel(level) >
                     atlas->indexCount))
            --level;
        const size_t commandCount =
            mesh.indexCountAtLevel(level);
        if (!commandCount ||
                commandCount >
                    std::numeric_limits<uint32_t>::max())
            return false;
        IndirectPageWork *pageWork = nullptr;
        for (IndirectPageWork& candidate :
                indirectPrepared_.pages) {
            if (candidate.page == demand.page) {
                pageWork = &candidate;
                break;
            }
        }
        if (!pageWork)
            return false;
        auto& commands = demand.commandCulled ?
            pageWork->culled : pageWork->ordinary;
        if (demand.commandIndex >=
                commands.size())
            return false;
        CadDrawElementsIndirectCommand& command =
            commands[demand.commandIndex];
        if (command.instanceCount != 1u ||
                command.baseInstance !=
                    demand.packedInstance)
            return false;
        const uint64_t oldTriangles =
            command.count / 3u;
        const uint64_t newTriangles =
            commandCount / 3u;
        indirectPrepared_.renderedTriangleCount =
            oldTriangles <=
                    indirectPrepared_.
                        renderedTriangleCount ?
                indirectPrepared_.
                    renderedTriangleCount -
                    oldTriangles + newTriangles :
                newTriangles;
        command.count =
            static_cast<uint32_t>(commandCount);
        indirectPrepared_.instances[
            demand.packedInstance].
                popMinLevel[3] =
                    static_cast<float>(level);
        demand.vertexCount =
            static_cast<uint32_t>(
                mesh.positionCountAtLevel(level));
        demand.indexCount =
            static_cast<uint32_t>(commandCount);
        changedPackedInstances.push_back(
            demand.packedInstance);
    }

    std::sort(changedPackedInstances.begin(),
        changedPackedInstances.end());
    changedPackedInstances.erase(
        std::unique(changedPackedInstances.begin(),
            changedPackedInstances.end()),
        changedPackedInstances.end());
    bool sparseUpload =
        changedPackedInstances.empty() ||
        (gpuRes_->instanceVbo() &&
         gpuRes_->instanceUploadSerial() ==
             indirectPrepared_.instanceUploadSerial);
    size_t begin = 0;
    while (sparseUpload &&
            begin < changedPackedInstances.size()) {
        size_t end = begin + 1u;
        while (end < changedPackedInstances.size() &&
                changedPackedInstances[end] ==
                    changedPackedInstances[end - 1u] + 1u)
            ++end;
        const uint32_t first =
            changedPackedInstances[begin];
        sparseUpload = gpuRes_->updateInstanceData(
            static_cast<GLintptr>(first) *
                sizeof(InstVertex),
            indirectPrepared_.instances.data() + first,
            static_cast<GLsizeiptr>(end - begin) *
                sizeof(InstVertex),
            glue);
        begin = end;
    }
    if (sparseUpload)
        indirectPrepared_.instanceUploadSerial =
            gpuRes_->instanceUploadSerial();
    else
        indirectPrepared_.instanceUploadSerial = 0;
    indirectPrepared_.progressiveLodCeiling =
        ceiling;
    return true;
}

bool CadRendererGL::patchIndirectPreparedLod(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const SoGLContext *glue)
{
    if (indirectPrepared_.shadedLodRevision ==
            plan.shadedLodRevision)
        return true;
    if (!glue || !gpuRes_ ||
            indirectPrepared_.shadedLodRevision <
                plan.shadedLodDeltaFloorRevision)
        return false;

    std::vector<CadShadedLodRange> changedRanges;
    for (const CadShadedLodDelta& delta : plan.shadedLodDeltas) {
        if (delta.revision <=
                indirectPrepared_.shadedLodRevision)
            continue;
        changedRanges.insert(
            changedRanges.end(),
            delta.ranges.begin(), delta.ranges.end());
    }
    if (changedRanges.empty())
        return false;
    std::sort(changedRanges.begin(), changedRanges.end(),
        [](const auto& left, const auto& right) {
            if (left.partIndex != right.partIndex)
                return left.partIndex < right.partIndex;
            return left.baseInstance < right.baseInstance;
        });
    changedRanges.erase(
        std::unique(changedRanges.begin(), changedRanges.end(),
            [](const auto& left, const auto& right) {
                return left.partIndex == right.partIndex &&
                    left.baseInstance == right.baseInstance;
            }),
        changedRanges.end());

    const uint32_t noSlot =
        std::numeric_limits<uint32_t>::max();
    std::vector<uint32_t> changedPackedInstances;
    changedPackedInstances.reserve(changedRanges.size());
    for (const CadShadedLodRange& range : changedRanges) {
        /*
         * The high-asset-count vehicle case overwhelmingly consists of
         * unique leaves.  Their prepared instance and command slots are
         * invariant under a level change.  A shared part may reorder several
         * occurrences across level bins; retain the exact path for that
         * materially different operation until it has its own run journal.
         */
        if (range.instanceCount != 1u ||
                range.baseInstance >=
                    plan.visibleInstances.size() ||
                range.partIndex >= plan.partBindings.size() ||
                range.shadedItemBegin >= plan.shadedItems.size() ||
                range.shadedItemCount != 1u)
            return false;
        const uint32_t sourceIndex = range.baseInstance;
        const CadVisibleInstance& source =
            plan.visibleInstances[sourceIndex];
        if (source.partIndex != range.partIndex)
            return false;

        if (range.partIndex >=
                indirectPrepared_.partByPlanPartIndex.size())
            return false;
        const uint32_t preparedPartIndex =
            indirectPrepared_.
                partByPlanPartIndex[range.partIndex];
        if (preparedPartIndex == noSlot) {
            /*
             * The sole occurrence was outside the prepared view, collapsed
             * into a point, or represented by a pressure proxy.  Its PoP cut
             * cannot affect this retained submission.
             */
            if (sourceIndex <
                    indirectPrepared_.instanceIndexBySource.size() &&
                    indirectPrepared_.
                        instanceIndexBySource[sourceIndex] != noSlot)
                return false;
            continue;
        }
        if (preparedPartIndex >=
                indirectPrepared_.parts.size())
            return false;
        IndirectPreparedPart& demand =
            indirectPrepared_.parts[preparedPartIndex];
        const CadPartBinding& binding =
            plan.partBindings[range.partIndex];
        if (!(demand.part == binding.part) ||
                demand.partIndex != range.partIndex ||
                demand.generation != binding.generation ||
                demand.packedInstance == noSlot ||
                demand.packedInstance >=
                    indirectPrepared_.instances.size() ||
                demand.commandIndex == noSlot ||
                demand.packedInstance >=
                    indirectPrepared_.
                        sourceInstanceIndices.size() ||
                indirectPrepared_.
                    sourceInstanceIndices[demand.packedInstance] !=
                        sourceIndex)
            return false;
        if (!binding.geometry || !binding.geometry->shaded ||
                !binding.geometry->shaded->isProgressive())
            return false;
        const TriMesh& mesh = *binding.geometry->shaded;
        uint8_t level = progressiveLevel(
            assembly.effectiveProgressiveLodLevel(source.lodLevel),
            mesh.progressiveMinimumLevel,
            mesh.progressiveResidentLevel);
        const uint32_t requestedVertices =
            static_cast<uint32_t>(
                mesh.positionCountAtLevel(level));
        const uint32_t requestedIndices =
            static_cast<uint32_t>(
                mesh.indexCountAtLevel(level));
        if (!requestedVertices || !requestedIndices)
            return false;

        const CadTriangleAtlasPart *atlas =
            gpuRes_->touchTriangleAtlasPart(
                binding.part, binding.generation,
                !mesh.normals.empty(),
                requestedVertices, requestedIndices);
        if (!atlas)
            atlas = gpuRes_->upsertTriangleAtlasPart(
                binding.part, binding.generation,
                packedVec3fData(mesh.positions),
                packedVec3fData(mesh.normals),
                requestedVertices, mesh.indices.data(),
                requestedIndices, true, glue, caps_);
        if (!atlas || atlas->page != demand.page ||
                atlas->vertices.first != demand.vertexFirst ||
                atlas->indices.first != demand.indexFirst)
            return false;

        while (level > mesh.progressiveMinimumLevel &&
                (mesh.positionCountAtLevel(level) >
                     atlas->vertexCount ||
                 mesh.indexCountAtLevel(level) >
                     atlas->indexCount))
            --level;
        const size_t commandCount =
            mesh.indexCountAtLevel(level);
        if (!commandCount ||
                commandCount >
                    std::numeric_limits<uint32_t>::max())
            return false;

        IndirectPageWork *pageWork = nullptr;
        for (IndirectPageWork& candidate :
                indirectPrepared_.pages) {
            if (candidate.page == demand.page) {
                pageWork = &candidate;
                break;
            }
        }
        if (!pageWork)
            return false;
        auto& commands = demand.commandCulled ?
            pageWork->culled : pageWork->ordinary;
        if (demand.commandIndex >= commands.size())
            return false;
        CadDrawElementsIndirectCommand& command =
            commands[demand.commandIndex];
        if (command.instanceCount != 1u ||
                command.baseInstance !=
                    demand.packedInstance)
            return false;

        const uint64_t oldTriangles =
            static_cast<uint64_t>(command.count / 3u);
        const uint64_t newTriangles =
            static_cast<uint64_t>(commandCount / 3u);
        indirectPrepared_.renderedTriangleCount =
            oldTriangles <=
                    indirectPrepared_.renderedTriangleCount ?
                indirectPrepared_.renderedTriangleCount -
                    oldTriangles + newTriangles :
                newTriangles;
        command.count =
            static_cast<uint32_t>(commandCount);
        InstVertex& target =
            indirectPrepared_.instances[
                demand.packedInstance];
        target.popMinLevel[3] =
            static_cast<float>(level);
        demand.vertexCount = requestedVertices;
        demand.indexCount = requestedIndices;
        changedPackedInstances.push_back(
            demand.packedInstance);
    }

    std::sort(changedPackedInstances.begin(),
        changedPackedInstances.end());
    changedPackedInstances.erase(
        std::unique(changedPackedInstances.begin(),
            changedPackedInstances.end()),
        changedPackedInstances.end());
    bool sparseUpload =
        changedPackedInstances.empty() ||
        (gpuRes_->instanceVbo() &&
         gpuRes_->instanceUploadSerial() ==
             indirectPrepared_.instanceUploadSerial);
    size_t begin = 0;
    while (sparseUpload &&
            begin < changedPackedInstances.size()) {
        size_t end = begin + 1u;
        while (end < changedPackedInstances.size() &&
                changedPackedInstances[end] ==
                    changedPackedInstances[end - 1u] + 1u)
            ++end;
        const uint32_t first =
            changedPackedInstances[begin];
        sparseUpload = gpuRes_->updateInstanceData(
            static_cast<GLintptr>(first) *
                sizeof(InstVertex),
            indirectPrepared_.instances.data() + first,
            static_cast<GLsizeiptr>(end - begin) *
                sizeof(InstVertex),
            glue);
        begin = end;
    }
    if (sparseUpload)
        indirectPrepared_.instanceUploadSerial =
            gpuRes_->instanceUploadSerial();
    else
        indirectPrepared_.instanceUploadSerial = 0;

    indirectPrepared_.shadedLodRevision =
        plan.shadedLodRevision;
    indirectPrepared_.planRevision = plan.revision;
    indirectPrepared_.atlasRevision =
        gpuRes_->triangleAtlasRevision();
    indirectPrepared_.atlasValidationCountdown = 30u;
    return true;
}

bool CadRendererGL::replayIndirectShaded(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const SoGLContext *glue,
        const SbMatrix& viewProj,
        const SbViewVolume& viewVolume)
{
    const bool cameraChanged = !(indirectPrepared_.viewProj == viewProj);
    const bool subpixelProxyChanged =
        indirectPrepared_.subpixelProxyRevision !=
            plan.subpixelProxyRevision;
    bool mayReplayVisibilityProxyChange = false;
    if (!cameraChanged && subpixelProxyChanged &&
            indirectPrepared_.instanceAttributeRevision !=
                plan.instanceAttributeRevision &&
            indirectPrepared_.instanceAttributeRevision >=
                plan.instanceAttributeDeltaFloorRevision) {
        for (const CadInstanceAttributeDelta& delta :
                plan.instanceAttributeDeltas) {
            if (delta.revision >
                    indirectPrepared_.instanceAttributeRevision &&
                    delta.visibilityChanged) {
                mayReplayVisibilityProxyChange = true;
                break;
            }
        }
    }
    /*
     * Camera motion never makes immutable geometry invalid.  When the view
     * controller has established mesh coverage it asks us to preserve this
     * coherent prepared cut for the whole input burst.  Event-count or timer
     * refreshes injected recurring O(scene) 30-50 ms stalls and made a faster
     * mouse produce worse FPS.  The controller clears the hint for the first
     * quiet frame, or sooner if its bounded coverage pass discovers newly
     * visible geometry which has no mesh presentation.
     */
    const bool mayReuseChangedCamera =
        cameraChanged && assembly.cameraMotionFrameReuse.getValue();
    const bool appendChanged =
        indirectPrepared_.appendRevision !=
            plan.appendRevision;
    const bool mayPatchAppend =
        appendChanged &&
        indirectPrepared_.appendRevision >=
            plan.appendDeltaFloorRevision;
    const bool partGeometryChanged =
        indirectPrepared_.partGeometryRevision !=
            plan.partGeometryRevision;
    const bool mayPatchPartGeometry =
        partGeometryChanged &&
        indirectPrepared_.partGeometryRevision >=
            plan.partGeometryDeltaFloorRevision;
    if (!indirectPrepared_.valid ||
            indirectPrepared_.contextId != glue->contextid ||
            ((indirectPrepared_.geometryRevision !=
                  plan.geometryRevision ||
              indirectPrepared_.shadedLayoutRevision !=
                  plan.shadedLayoutRevision) &&
                !mayPatchAppend &&
                !mayPatchPartGeometry) ||
            (subpixelProxyChanged &&
                !mayReplayVisibilityProxyChange &&
                !mayPatchAppend &&
                !mayPatchPartGeometry) ||
            (cameraChanged && !mayReuseChangedCamera)) {
        if (std::getenv("OBOL_CAD_PATCH_DEBUG"))
            std::fprintf(stderr,
                "CadRendererGL retained patch gate exact "
                "valid=%d context=%d geometry=%d layout=%d "
                "subpixel=%d append_changed=%d append_patch=%d "
                "append_rev=%llu/%llu/%llu "
                "geometry_changed=%d geometry_patch=%d "
                "geometry_rev=%llu/%llu/%llu "
                "camera=%d camera_reuse=%d\n",
                indirectPrepared_.valid ? 1 : 0,
                indirectPrepared_.contextId == glue->contextid ? 1 : 0,
                indirectPrepared_.geometryRevision ==
                    plan.geometryRevision ? 1 : 0,
                indirectPrepared_.shadedLayoutRevision ==
                    plan.shadedLayoutRevision ? 1 : 0,
                subpixelProxyChanged ? 1 : 0,
                appendChanged ? 1 : 0,
                mayPatchAppend ? 1 : 0,
                static_cast<unsigned long long>(
                    indirectPrepared_.appendRevision),
                static_cast<unsigned long long>(
                    plan.appendRevision),
                static_cast<unsigned long long>(
                    plan.appendDeltaFloorRevision),
                partGeometryChanged ? 1 : 0,
                mayPatchPartGeometry ? 1 : 0,
                static_cast<unsigned long long>(
                    indirectPrepared_.partGeometryRevision),
                static_cast<unsigned long long>(
                    plan.partGeometryRevision),
                static_cast<unsigned long long>(
                    plan.partGeometryDeltaFloorRevision),
                cameraChanged ? 1 : 0,
                mayReuseChangedCamera ? 1 : 0);
        indirectPrepared_.valid = false;
        return false;
    }
    if (cameraChanged) {
        ++indirectPrepared_.cameraMotionReplayCount;
        indirectPrepared_.viewProj = viewProj;
    }
    const char *appendPatchEnv =
        std::getenv("OBOL_CAD_APPEND_PATCH");
    const bool appendPatchEnabled =
        !appendPatchEnv || appendPatchEnv[0] != '0';
    if (appendChanged &&
            (!appendPatchEnabled ||
             !patchIndirectPreparedAppend(
                plan, assembly, glue, viewProj))) {
        if (std::getenv("OBOL_CAD_PATCH_DEBUG"))
            std::fprintf(stderr,
                "CadRendererGL retained append patch rejected\n");
        indirectPrepared_.valid = false;
        return false;
    }
    const char *geometryPatchEnv =
        std::getenv("OBOL_CAD_GEOMETRY_PATCH");
    const bool geometryPatchEnabled =
        !geometryPatchEnv || geometryPatchEnv[0] != '0';
    if (partGeometryChanged &&
            (!geometryPatchEnabled ||
             !patchIndirectPreparedGeometry(
                plan, assembly, glue))) {
        if (std::getenv("OBOL_CAD_PATCH_DEBUG"))
            std::fprintf(stderr,
                "CadRendererGL retained geometry patch rejected\n");
        indirectPrepared_.valid = false;
        return false;
    }
    if (indirectPrepared_.progressiveLodCeiling !=
            assembly.progressiveLodCeiling.getValue() &&
            !patchIndirectPreparedCeiling(
                plan, assembly, glue)) {
        if (std::getenv("OBOL_CAD_PATCH_DEBUG"))
            std::fprintf(stderr,
                "CadRendererGL retained ceiling patch rejected\n");
        indirectPrepared_.valid = false;
        return false;
    }
    const char *lodPatchEnv =
        std::getenv("OBOL_CAD_LOD_PATCH");
    const bool lodPatchEnabled =
        !lodPatchEnv || lodPatchEnv[0] != '0';
    if (indirectPrepared_.shadedLodRevision !=
            plan.shadedLodRevision &&
            (!lodPatchEnabled ||
             !patchIndirectPreparedLod(
                plan, assembly, glue))) {
        if (std::getenv("OBOL_CAD_PATCH_DEBUG"))
            std::fprintf(stderr,
                "CadRendererGL retained LoD patch rejected\n");
        indirectPrepared_.valid = false;
        return false;
    }

    if (indirectPrepared_.instanceAttributeRevision !=
            plan.instanceAttributeRevision) {
        if (indirectPrepared_.sourceInstanceIndices.size() !=
                indirectPrepared_.instances.size() ||
                indirectPrepared_.pressureProxySourceInstanceIndices.size() !=
                indirectPrepared_.pressureProxyPoints.size()) {
            indirectPrepared_.valid = false;
            return false;
        }

        std::vector<uint32_t> changedSourceIndices;
        std::vector<uint32_t> visibilityChangedSourceIndices;
        bool sparseAttributes =
            indirectPrepared_.instanceAttributeRevision >=
                plan.instanceAttributeDeltaFloorRevision;
        if (sparseAttributes) {
            for (const CadInstanceAttributeDelta& delta :
                    plan.instanceAttributeDeltas) {
                if (delta.revision <=
                        indirectPrepared_.instanceAttributeRevision)
                    continue;
                changedSourceIndices.insert(
                    changedSourceIndices.end(),
                    delta.visibleIndices.begin(),
                    delta.visibleIndices.end());
                if (delta.visibilityChanged)
                    visibilityChangedSourceIndices.insert(
                        visibilityChangedSourceIndices.end(),
                        delta.visibleIndices.begin(),
                        delta.visibleIndices.end());
            }
            std::sort(changedSourceIndices.begin(),
                changedSourceIndices.end());
            changedSourceIndices.erase(
                std::unique(changedSourceIndices.begin(),
                    changedSourceIndices.end()),
                changedSourceIndices.end());
            std::sort(visibilityChangedSourceIndices.begin(),
                visibilityChangedSourceIndices.end());
            visibilityChangedSourceIndices.erase(
                std::unique(
                    visibilityChangedSourceIndices.begin(),
                    visibilityChangedSourceIndices.end()),
                visibilityChangedSourceIndices.end());
            if (changedSourceIndices.empty())
                sparseAttributes = false;
        }

        const auto updatePackedColor =
            [&](uint32_t packedIndex, uint32_t sourceIndex) {
            if (packedIndex >= indirectPrepared_.instances.size() ||
                    sourceIndex >= plan.visibleInstances.size())
                return false;
            const CadVisibleInstance& source =
                plan.visibleInstances[sourceIndex];
            InstVertex& target =
                indirectPrepared_.instances[packedIndex];
            target.color[0] = source.rgba[0] / 255.0f;
            target.color[1] = source.rgba[1] / 255.0f;
            target.color[2] = source.rgba[2] / 255.0f;
            target.color[3] = source.rgba[3] / 255.0f;
            const float geometryFlags =
                std::fmod(target.popMaxFlags[3], 4.0f);
            target.popMaxFlags[3] = geometryFlags +
                ((source.flags & CadInstanceHidden) ? 4.0f : 0.0f);
            return true;
        };
        std::vector<uint32_t> changedPackedIndices;
        bool pressureAttributesChanged = false;
        if (sparseAttributes) {
            changedPackedIndices.reserve(changedSourceIndices.size());
            for (const uint32_t sourceIndex : changedSourceIndices) {
                if (sourceIndex >= plan.visibleInstances.size() ||
                        sourceIndex >=
                            indirectPrepared_.instanceIndexBySource.size() ||
                        sourceIndex >=
                            indirectPrepared_.
                                pressureProxyIndexBySource.size()) {
                    indirectPrepared_.valid = false;
                    return false;
                }
                const uint32_t packedIndex =
                    indirectPrepared_.instanceIndexBySource[sourceIndex];
                const bool hasPacked = packedIndex !=
                    std::numeric_limits<uint32_t>::max();
                if (hasPacked) {
                    if (!updatePackedColor(packedIndex, sourceIndex)) {
                        indirectPrepared_.valid = false;
                        return false;
                    }
                    changedPackedIndices.push_back(packedIndex);
                }
                const uint32_t proxyIndex =
                    indirectPrepared_.
                        pressureProxyIndexBySource[sourceIndex];
                const bool hasProxy = proxyIndex !=
                    std::numeric_limits<uint32_t>::max();
                if (hasProxy) {
                    if (proxyIndex >=
                            indirectPrepared_.pressureProxyPoints.size()) {
                        indirectPrepared_.valid = false;
                        return false;
                    }
                    indirectPrepared_.pressureProxyPoints[proxyIndex].rgba =
                        plan.visibleInstances[sourceIndex].rgba;
                    indirectPrepared_.pressureProxyPoints[proxyIndex].flags =
                        plan.visibleInstances[sourceIndex].flags;
                    pressureAttributesChanged = true;
                }
                if (!hasPacked && !hasProxy &&
                        !(plan.visibleInstances[sourceIndex].flags &
                            CadInstanceHidden) &&
                        !isSubpixelProxyInstance(plan, sourceIndex) &&
                        std::binary_search(
                            visibilityChangedSourceIndices.begin(),
                            visibilityChangedSourceIndices.end(),
                            sourceIndex)) {
                    /*
                     * This occurrence was absent when the prepared view was
                     * built (initially hidden or out of frame).  Showing it
                     * needs one exact visibility/admission pass.
                     */
                    indirectPrepared_.valid = false;
                    return false;
                }
            }
        } else {
            for (size_t i = 0;
                    i < indirectPrepared_.instances.size(); ++i) {
                const uint32_t sourceIndex =
                    indirectPrepared_.sourceInstanceIndices[i];
                if (!updatePackedColor(
                        static_cast<uint32_t>(i), sourceIndex)) {
                    indirectPrepared_.valid = false;
                    return false;
                }
            }
            for (size_t i = 0;
                    i < indirectPrepared_.pressureProxyPoints.size(); ++i) {
                const uint32_t sourceIndex =
                    indirectPrepared_.
                        pressureProxySourceInstanceIndices[i];
                if (sourceIndex >= plan.visibleInstances.size()) {
                    indirectPrepared_.valid = false;
                    return false;
                }
                indirectPrepared_.pressureProxyPoints[i].rgba =
                    plan.visibleInstances[sourceIndex].rgba;
                indirectPrepared_.pressureProxyPoints[i].flags =
                    plan.visibleInstances[sourceIndex].flags;
                pressureAttributesChanged = true;
            }
        }
        indirectPrepared_.instanceAttributeRevision =
            plan.instanceAttributeRevision;
        if (mayReplayVisibilityProxyChange)
            indirectPrepared_.subpixelProxyRevision =
                plan.subpixelProxyRevision;
        indirectPrepared_.planRevision = plan.revision;

        bool sparseUpload = sparseAttributes &&
            gpuRes_->instanceVbo() &&
            gpuRes_->instanceUploadSerial() ==
                indirectPrepared_.instanceUploadSerial;
        if (sparseUpload && !changedPackedIndices.empty()) {
            std::sort(changedPackedIndices.begin(),
                changedPackedIndices.end());
            changedPackedIndices.erase(
                std::unique(changedPackedIndices.begin(),
                    changedPackedIndices.end()),
                changedPackedIndices.end());
            size_t begin = 0;
            while (sparseUpload &&
                    begin < changedPackedIndices.size()) {
                size_t end = begin + 1;
                while (end < changedPackedIndices.size() &&
                        changedPackedIndices[end] ==
                            changedPackedIndices[end - 1] + 1u)
                    ++end;
                const uint32_t first = changedPackedIndices[begin];
                const size_t count = end - begin;
                sparseUpload = gpuRes_->updateInstanceData(
                    static_cast<GLintptr>(first) * sizeof(InstVertex),
                    indirectPrepared_.instances.data() + first,
                    static_cast<GLsizeiptr>(count) *
                        sizeof(InstVertex),
                    glue);
                begin = end;
            }
            if (sparseUpload)
                indirectPrepared_.instanceUploadSerial =
                    gpuRes_->instanceUploadSerial();
        }
        /*
         * A missing/overwritten VBO or an expired delta journal falls back to
         * one complete upload.  Command and atlas state remain reusable.
         */
        if (!sparseUpload)
            indirectPrepared_.instanceUploadSerial = 0;
        if (pressureAttributesChanged) {
            pressureProxyAppendOnly_ = false;
            ++pressureProxyRevision_;
            if (!pressureProxyRevision_)
                pressureProxyRevision_ = 1;
        }
    }

    /*
     * Expensive retained-record audit used by the graphical stress harness.
     * It reconstructs the command/instance contract from the authoritative
     * frame plan without touching GL.  Keeping this behind an environment
     * switch lets us distinguish a CPU journal defect from a GPU buffer/state
     * defect without making ordinary replay O(all visible parts).
     */
    if (std::getenv("OBOL_CAD_VALIDATE_REPLAY")) {
        const uint32_t noSlot =
            std::numeric_limits<uint32_t>::max();
        const auto rejectAudit =
            [&](size_t demandIndex, const char *reason) {
                std::fprintf(stderr,
                    "CadRendererGL retained audit failed demand=%zu "
                    "reason=%s parts=%zu packed=%zu pages=%zu\n",
                    demandIndex, reason ? reason : "unknown",
                    indirectPrepared_.parts.size(),
                    indirectPrepared_.instances.size(),
                    indirectPrepared_.pages.size());
                indirectPrepared_.valid = false;
                return false;
            };
        for (size_t demandIndex = 0;
                demandIndex < indirectPrepared_.parts.size();
                ++demandIndex) {
            const IndirectPreparedPart& demand =
                indirectPrepared_.parts[demandIndex];
            if (demand.partIndex >= plan.partBindings.size())
                return rejectAudit(demandIndex, "part-index");
            const CadPartBinding& binding =
                plan.partBindings[demand.partIndex];
            if (!(binding.part == demand.part) ||
                    binding.generation != demand.generation ||
                    !binding.geometry || !binding.geometry->shaded)
                return rejectAudit(demandIndex, "binding");
            if (demand.packedInstance == noSlot ||
                    demand.packedInstance >=
                        indirectPrepared_.instances.size() ||
                    demand.packedInstance >=
                        indirectPrepared_.sourceInstanceIndices.size())
                return rejectAudit(demandIndex, "packed-instance");
            const uint32_t sourceIndex =
                indirectPrepared_.sourceInstanceIndices[
                    demand.packedInstance];
            if (sourceIndex >= plan.visibleInstances.size())
                return rejectAudit(demandIndex, "source-index");
            const CadVisibleInstance& source =
                plan.visibleInstances[sourceIndex];
            if (source.partIndex != demand.partIndex)
                return rejectAudit(demandIndex, "source-part");
            const InstVertex& instance =
                indirectPrepared_.instances[demand.packedInstance];
            if (std::memcmp(
                    instance.transform, source.transform.data(),
                    16u * sizeof(float)) != 0)
                return rejectAudit(demandIndex, "transform");

            const TriMesh& mesh = *binding.geometry->shaded;
            uint8_t level = mesh.isProgressive() ?
                progressiveLevel(
                    assembly.effectiveProgressiveLodLevel(
                        source.lodLevel),
                    mesh.progressiveMinimumLevel,
                    mesh.progressiveResidentLevel) :
                15u;
            const CadTriangleAtlasPart *atlas =
                gpuRes_->triangleAtlasPart(binding.part);
            if (!atlas || atlas->page != demand.page ||
                    atlas->vertices.first != demand.vertexFirst ||
                    atlas->indices.first != demand.indexFirst)
                return rejectAudit(demandIndex, "atlas");
            while (mesh.isProgressive() &&
                    level > mesh.progressiveMinimumLevel &&
                    (mesh.positionCountAtLevel(level) >
                         atlas->vertexCount ||
                     mesh.indexCountAtLevel(level) >
                         atlas->indexCount))
                --level;
            const uint32_t expectedCount =
                static_cast<uint32_t>(mesh.isProgressive() ?
                    mesh.indexCountAtLevel(level) :
                    mesh.indices.size());
            if (!expectedCount ||
                    instance.popMinLevel[3] !=
                        static_cast<float>(level))
                return rejectAudit(demandIndex, "lod-level");
            const SbVec3f expectedMinimum = mesh.isProgressive() ?
                mesh.progressiveQuantizationMinimum :
                SbVec3f(0, 0, 0);
            const SbVec3f expectedMaximum = mesh.isProgressive() ?
                mesh.progressiveQuantizationMaximum :
                SbVec3f(0, 0, 0);
            for (int axis = 0; axis < 3; ++axis) {
                if (instance.popMinLevel[axis] !=
                            expectedMinimum[axis] ||
                        instance.popMaxFlags[axis] !=
                            expectedMaximum[axis])
                    return rejectAudit(
                        demandIndex, "quantization");
            }

            const IndirectPageWork *pageWork = nullptr;
            for (const IndirectPageWork& candidate :
                    indirectPrepared_.pages) {
                if (candidate.page == demand.page) {
                    pageWork = &candidate;
                    break;
                }
            }
            if (!pageWork)
                return rejectAudit(demandIndex, "page-work");
            const auto& commands = demand.commandCulled ?
                pageWork->culled : pageWork->ordinary;
            if (demand.commandIndex >= commands.size())
                return rejectAudit(demandIndex, "command-index");
            const CadDrawElementsIndirectCommand& command =
                commands[demand.commandIndex];
            if (command.count != expectedCount ||
                    command.instanceCount != 1u ||
                    command.firstIndex != demand.indexFirst ||
                    command.baseVertex !=
                        static_cast<int32_t>(demand.vertexFirst) ||
                    command.baseInstance != demand.packedInstance)
                return rejectAudit(demandIndex, "command");
        }
    }

    constexpr uint32_t validationIntervalFrames = 30u;
    const bool validateAtlas =
        indirectPrepared_.atlasRevision !=
            gpuRes_->triangleAtlasRevision() ||
        !indirectPrepared_.atlasValidationCountdown;
    if (validateAtlas) {
        for (const IndirectPreparedPart& demand : indirectPrepared_.parts) {
            if (demand.partIndex >= plan.partBindings.size()) {
                indirectPrepared_.valid = false;
                return false;
            }
            const CadPartBinding& binding =
                plan.partBindings[demand.partIndex];
            if (!(binding.part == demand.part) ||
                    binding.generation != demand.generation) {
                indirectPrepared_.valid = false;
                return false;
            }
            const CadTriangleAtlasPart *atlas =
                gpuRes_->touchTriangleAtlasPart(
                    demand.part, demand.generation, demand.hasNormals,
                    demand.vertexCount, demand.indexCount);
            if (!atlas || atlas->page != demand.page ||
                    atlas->vertices.first != demand.vertexFirst ||
                    atlas->indices.first != demand.indexFirst) {
                indirectPrepared_.valid = false;
                return false;
            }
        }
        indirectPrepared_.atlasRevision =
            gpuRes_->triangleAtlasRevision();
        indirectPrepared_.atlasValidationCountdown =
            validationIntervalFrames;
    } else {
        --indirectPrepared_.atlasValidationCountdown;
        if (!indirectPrepared_.parts.empty())
            gpuRes_->deferTriangleAtlasMaintenance();
    }

    pressureProxyPointsView_ =
        &indirectPrepared_.pressureProxyPoints;
    lastRenderedTriangleCount_ =
        indirectPrepared_.renderedTriangleCount;
    const bool submitted =
        submitIndirectPrepared(glue, viewProj, viewVolume);
    if (submitted)
        lastRenderUsedPreparedReplay_ = true;
    return submitted;
}

bool CadRendererGL::renderIndirectShaded(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const SoGLContext* glue,
        const SbMatrix& viewProj,
        const SbViewVolume& viewVolume)
{
    if (!glue || !gpuRes_ || !caps_.canUseIndirect() ||
            !shaders_.shadedIndirect || plan.shadedItems.empty() ||
            plan.visibleInstances.empty())
        return rejectIndirect(1, "precondition");
    const char *replayEnv = std::getenv("OBOL_CAD_REPLAY");
    const bool replayEnabled =
        !replayEnv || replayEnv[0] != '0';
    if (replayEnabled) {
        if (replayIndirectShaded(
                plan, assembly, glue, viewProj, viewVolume))
            return true;
    } else {
        /*
         * Diagnostic/reference mode: keep the same atlas, shaders, indirect
         * commands, and GPU submission route, but prepare the CPU submission
         * exactly for every frame.  This isolates retained-record defects
         * from atlas/MDI/shader defects without falling back to a materially
         * different renderer.
         */
        indirectPrepared_.valid = false;
    }
    using IndirectClock = std::chrono::steady_clock;
    const auto indirectStarted = IndirectClock::now();
    auto visibilityCompleted = indirectStarted;
    auto protectionCompleted = indirectStarted;
    auto admissionCompleted = indirectStarted;
    auto commandsCompleted = indirectStarted;

    /*
     * Resolve view visibility before GPU admission.  The assembly plan is a
     * structural/presentation cache and intentionally contains authored
     * visible instances outside this camera's frustum.  Admitting every part
     * here defeated view-aware LoD memory management and made a close zoom
     * retain an entire vehicle.  Subpixel proxy occurrences are likewise
     * owned by the aggregate point channel, never by the mesh atlas.
    */
    const FrustumPlanes fp = extractFrustumPlanes(viewProj);
    auto& visibleMask = indirectVisibleMask_;
    visibleMask.assign(plan.visibleInstances.size(), 0u);
    auto& visibleMaximumLod = indirectVisibleMaximumLod_;
    visibleMaximumLod.assign(plan.partBindings.size(), 0u);
    auto& visiblePart = indirectVisiblePart_;
    visiblePart.assign(plan.partBindings.size(), 0u);
    auto& visiblePartIndices = indirectVisiblePartIndices_;
    visiblePartIndices.clear();
    if (visiblePartIndices.capacity() < plan.partBindings.size())
        visiblePartIndices.reserve(plan.partBindings.size());
    const uint32_t noOccurrence = std::numeric_limits<uint32_t>::max();
    auto& firstVisibleOccurrence = indirectFirstVisibleOccurrence_;
    firstVisibleOccurrence.assign(plan.partBindings.size(), noOccurrence);
    auto& nextVisibleOccurrence = indirectNextVisibleOccurrence_;
    nextVisibleOccurrence.assign(plan.visibleInstances.size(), noOccurrence);
    size_t visibleOccurrenceCount = 0;
    for (const CadDrawItem& item : plan.shadedItems) {
        if (!item.instanceCount ||
                item.partIndex >= plan.partBindings.size() ||
                item.baseInstance >= plan.visibleInstances.size() ||
                item.instanceCount >
                    plan.visibleInstances.size() - item.baseInstance)
            continue;
        const CadPartBinding& binding =
            plan.partBindings[item.partIndex];
        const PartGeometry *geometry = binding.geometry.get();
        if (!geometry || !geometry->shaded)
            return rejectIndirect(
                2, "visible part has no shaded geometry");
        const TriMesh& mesh = *geometry->shaded;
        for (uint32_t i = 0; i < item.instanceCount; ++i) {
            const size_t visibleIndex = item.baseInstance + i;
            if (!drawItemOwnsInstance(plan, item, visibleIndex))
                continue;
            const CadVisibleInstance& instance =
                plan.visibleInstances[visibleIndex];
            if (isHiddenInstance(plan, visibleIndex) ||
                    isSubpixelProxyInstance(plan, visibleIndex) ||
                    isBoxOutsideFrustum(
                        instance.wbMin, instance.wbMax, fp))
                continue;
            if (!visibleMask[visibleIndex]) {
                visibleMask[visibleIndex] = 1u;
                nextVisibleOccurrence[visibleIndex] =
                    firstVisibleOccurrence[item.partIndex];
                firstVisibleOccurrence[item.partIndex] =
                    static_cast<uint32_t>(visibleIndex);
                ++visibleOccurrenceCount;
            }
            const uint8_t requested = mesh.isProgressive() ?
                progressiveLevel(
                    assembly.effectiveProgressiveLodLevel(
                        instance.lodLevel),
                    mesh.progressiveMinimumLevel,
                    mesh.progressiveResidentLevel) : 15u;
            if (!visiblePart[item.partIndex]) {
                visiblePart[item.partIndex] = 1u;
                visiblePartIndices.push_back(item.partIndex);
            }
            visibleMaximumLod[item.partIndex] =
                std::max(visibleMaximumLod[item.partIndex], requested);
        }
    }
    visibilityCompleted = IndirectClock::now();
    if (!visibleOccurrenceCount) {
        lastIndirectStatus_ = 0;
        return true;
    }

    auto& requestedVertexCounts = indirectRequestedVertexCounts_;
    requestedVertexCounts.assign(plan.partBindings.size(), 0u);
    auto& requestedIndexCounts = indirectRequestedIndexCounts_;
    requestedIndexCounts.assign(plan.partBindings.size(), 0u);
    auto& atlasBindings = indirectAtlasBindings_;
    atlasBindings.assign(plan.partBindings.size(), nullptr);
    /*
     * Mark every retained consumer before admitting any new allocation.
     * Without this phase, pressure while processing part N could evict part
     * N+1 merely because it had not yet been encountered in this frame,
     * creating a perpetual evict/re-upload cycle at the memory ceiling.
     *
     * The touch also validates generation and retained prefix capacity.  An
     * unchanged frame can therefore reuse the returned bindings directly,
     * avoiding a second full-table upsert pass and a third lookup pass over
     * tens of thousands of unique parts.
     */
    for (const uint32_t partIndex : visiblePartIndices) {
        const CadPartBinding& binding =
            plan.partBindings[partIndex];
        const PartGeometry *geometry = binding.geometry.get();
        if (!geometry || !geometry->shaded)
            return rejectIndirect(
                2, "visible part has no shaded geometry");
        const TriMesh& mesh = *geometry->shaded;
        const uint8_t requested = visibleMaximumLod[partIndex];
        const size_t vertexCount = mesh.isProgressive() ?
            mesh.positionCountAtLevel(requested) : mesh.positions.size();
        const size_t indexCount = mesh.isProgressive() ?
            mesh.indexCountAtLevel(requested) : mesh.indices.size();
        if (!vertexCount || !indexCount ||
                vertexCount > std::numeric_limits<uint32_t>::max() ||
                indexCount > std::numeric_limits<uint32_t>::max())
            return rejectIndirect(3, "invalid retained prefix counts");
        requestedVertexCounts[partIndex] =
            static_cast<uint32_t>(vertexCount);
        requestedIndexCounts[partIndex] =
            static_cast<uint32_t>(indexCount);
        atlasBindings[partIndex] =
            gpuRes_->touchTriangleAtlasPart(
                binding.part, binding.generation,
                !mesh.normals.empty(),
                requestedVertexCounts[partIndex],
                requestedIndexCounts[partIndex]);
    }
    protectionCompleted = IndirectClock::now();

    /*
     * Make each unique visible part's richest requested prefix resident.
     * Occurrences at smaller levels select smaller command counts and
     * independent quantization attributes from the same cumulative arrays.
     */
    auto& pressureProxyPoints = indirectPressureProxyPoints_;
    pressureProxyPoints.clear();
    indirectPrepared_.pressureProxySourceInstanceIndices.swap(
        indirectPressureProxySourceInstanceIndices_);
    auto& pressureProxySourceInstanceIndices =
        indirectPressureProxySourceInstanceIndices_;
    pressureProxySourceInstanceIndices.clear();
    if (pressureProxySourceInstanceIndices.capacity() <
            visibleOccurrenceCount)
        pressureProxySourceInstanceIndices.reserve(
            visibleOccurrenceCount);
    for (const uint32_t partIndex : visiblePartIndices) {
        if (!visiblePart[partIndex])
            continue;
        const CadPartBinding& binding =
            plan.partBindings[partIndex];
        const PartId partId = binding.part;
        const PartGeometry *geometry = binding.geometry.get();
        if (!geometry || !geometry->shaded)
            return false;
        const TriMesh& mesh = *geometry->shaded;
        const uint32_t vertexCount =
            requestedVertexCounts[partIndex];
        const uint32_t indexCount =
            requestedIndexCounts[partIndex];
        if (atlasBindings[partIndex])
            continue;
        const CadTriangleAtlasPart *admitted =
            gpuRes_->upsertTriangleAtlasPart(
                partId, binding.generation,
                packedVec3fData(mesh.positions),
                packedVec3fData(mesh.normals),
                vertexCount,
                mesh.indices.data(),
                indexCount,
                mesh.isProgressive(), glue, caps_);
        if (!admitted) {
            if (std::getenv("OBOL_CAD_INDIRECT_DEBUG"))
                std::fprintf(stderr,
                    "CadRendererGL indirect atlas admission failed "
                    "part=%016llx:%016llx vertices=%zu indices=%zu "
                    "allocated=%zu live=%zu pages=%zu parts=%zu\n",
                    static_cast<unsigned long long>(partId.w0),
                    static_cast<unsigned long long>(partId.w1),
                    static_cast<size_t>(vertexCount),
                    static_cast<size_t>(indexCount),
                    gpuRes_->triangleAtlasAllocatedBytes(),
                    gpuRes_->triangleAtlasLiveBytes(),
                    gpuRes_->triangleAtlasPageCount(),
                    gpuRes_->triangleAtlasPartCount());
            /*
             * BObol LoD meshes explicitly opt into a bounded, hole-free
             * pressure representation.  Keep already admitted geometry and
             * replace only this unadmitted part's visible occurrences with
             * one colored point each.  Rejecting the entire retained path
             * here rebuilt every visible mesh on the CPU and could revive
             * fallback-like visuals for a single failed allocation.
             *
             * General CAD geometry has no such contract, so preserve the
             * correctness fallback for non-eligible parts.
             */
            if (!binding.subpixelProxyEligible)
                return rejectIndirect(4, "triangle atlas admission");

            SbVec3f localCenter(0.0f, 0.0f, 0.0f);
            for (const SbVec3f& corner : binding.subpixelProxyCorners)
                localCenter += corner;
            localCenter /= 8.0f;
            for (uint32_t visibleIndex =
                    firstVisibleOccurrence[partIndex];
                    visibleIndex != noOccurrence;
                    visibleIndex = nextVisibleOccurrence[visibleIndex]) {
                if (!visibleMask[visibleIndex])
                    continue;
                const CadVisibleInstance& instance =
                    plan.visibleInstances[visibleIndex];
                CadSubpixelProxyPoint replacement;
                replacement.position = transformedFlatPoint(
                    localCenter, instance.transform);
                replacement.rgba = instance.rgba;
                replacement.instanceId = instance.instanceId;
                replacement.flags = instance.flags;
                pressureProxyPoints.push_back(replacement);
                pressureProxySourceInstanceIndices.push_back(
                    visibleIndex);
                visibleMask[visibleIndex] = 0u;
                --visibleOccurrenceCount;
            }
            visiblePart[partIndex] = 0u;
        } else {
            /*
             * unordered_map rehashing invalidates iterators, not pointers or
             * references to elements.  The admission sweep only erases parts
             * that were not protected earlier in this frame, so this binding
             * remains stable through all later admissions.
             */
            atlasBindings[partIndex] = admitted;
        }
    }
    admissionCompleted = IndirectClock::now();

    /*
     * The preceding prepared frame is no longer replayable once exact
     * preparation begins.  Reuse its per-page command capacities as the
     * scratch target for this build.  Page ids are dense atlas slots, so a
     * vector lookup avoids one red-black-tree allocation per page as well.
     */
    if (!indirectPrepared_.pages.empty())
        indirectPageWorkScratch_.swap(indirectPrepared_.pages);
    indirectPrepared_.pages.clear();
    for (IndirectPageWork& work : indirectPageWorkScratch_) {
        work.ordinary.clear();
        work.culled.clear();
    }
    const size_t atlasPageCount = gpuRes_->triangleAtlasPageCount();
    indirectPageWorkSlotByPage_.assign(
        atlasPageCount, std::numeric_limits<uint32_t>::max());
    for (size_t i = 0; i < indirectPageWorkScratch_.size(); ++i) {
        const uint32_t page = indirectPageWorkScratch_[i].page;
        if (page < indirectPageWorkSlotByPage_.size())
            indirectPageWorkSlotByPage_[page] =
                static_cast<uint32_t>(i);
    }
    const auto pageWorkFor = [&](uint32_t page) ->
            IndirectPageWork& {
        if (page >= indirectPageWorkSlotByPage_.size())
            indirectPageWorkSlotByPage_.resize(
                static_cast<size_t>(page) + 1u,
                std::numeric_limits<uint32_t>::max());
        uint32_t& slot = indirectPageWorkSlotByPage_[page];
        if (slot == std::numeric_limits<uint32_t>::max()) {
            slot = static_cast<uint32_t>(
                indirectPageWorkScratch_.size());
            IndirectPageWork work;
            work.page = page;
            indirectPageWorkScratch_.push_back(std::move(work));
        }
        return indirectPageWorkScratch_[slot];
    };
    const uint32_t noPreparedSlot =
        std::numeric_limits<uint32_t>::max();
    indirectCommandIndexByPart_.assign(
        plan.partBindings.size(), noPreparedSlot);
    indirectCommandCullByPart_.assign(
        plan.partBindings.size(), 0u);
    indirectPackedInstanceByPart_.assign(
        plan.partBindings.size(), noPreparedSlot);
    /*
     * Recycle the preceding prepared frame's instance storage as scratch.
     * A cache miss therefore does not allocate another scene-sized array,
     * and publication below is a constant-time swap.
     */
    indirectPrepared_.instances.swap(indirectInstances_);
    auto& instances = indirectInstances_;
    instances.clear();
    if (instances.capacity() < visibleOccurrenceCount)
        instances.reserve(visibleOccurrenceCount);
    indirectPrepared_.sourceInstanceIndices.swap(
        indirectSourceInstanceIndices_);
    auto& sourceInstanceIndices = indirectSourceInstanceIndices_;
    sourceInstanceIndices.clear();
    if (sourceInstanceIndices.capacity() < visibleOccurrenceCount)
        sourceInstanceIndices.reserve(visibleOccurrenceCount);
    uint64_t renderedTriangleCount = 0;
    /*
     * Protection and admission both return stable element pointers.  Use
     * those direct bindings below instead of performing a second hash-table
     * lookup for every visible part after an insertion.
     */
    for (const CadDrawItem& item : plan.shadedItems) {
        if (!item.instanceCount ||
                item.partIndex >= plan.partBindings.size() ||
                item.baseInstance >= plan.visibleInstances.size() ||
                item.instanceCount >
                    plan.visibleInstances.size() - item.baseInstance)
            continue;
        /*
         * Admission is intentionally view-aware.  A part whose occurrences
         * are all outside the frustum or represented by aggregate proxy
         * points has no atlas binding this frame and contributes no command.
         * Test that expected absence before resolving the binding; treating
         * it as an error made one culled/subpixel part throw the entire scene
         * into the CPU-flattened fallback.
         */
        if (!visiblePart[item.partIndex])
            continue;
        const PartGeometry *geometry =
            plan.partBindings[item.partIndex].geometry.get();
        const CadTriangleAtlasPart *atlas =
            atlasBindings[item.partIndex];
        if (!geometry || !geometry->shaded || !atlas)
            return rejectIndirect(5, "missing admitted atlas binding");
        const TriMesh& mesh = *geometry->shaded;
        const bool progressive = mesh.isProgressive();
        const uint32_t baseInstance =
            static_cast<uint32_t>(instances.size());
        uint8_t commandLevel = 15u;
        size_t commandCount = 0;
        for (uint32_t i = 0; i < item.instanceCount; ++i) {
            const size_t instanceIndex = item.baseInstance + i;
            if (!drawItemOwnsInstance(
                    plan, item, instanceIndex) ||
                    !visibleMask[instanceIndex])
                continue;
            const CadVisibleInstance& source =
                plan.visibleInstances[instanceIndex];
            uint8_t level = progressive ?
                progressiveLevel(
                    assembly.effectiveProgressiveLodLevel(source.lodLevel),
                    mesh.progressiveMinimumLevel,
                    mesh.progressiveResidentLevel) : 15;
            /*
             * Allocation pressure may deliberately retain a correct coarser
             * cumulative prefix.  Clamp to the richest level wholly resident
             * instead of failing the entire frame into a world-space rebuild.
             */
            while (progressive &&
                    level > mesh.progressiveMinimumLevel &&
                    (mesh.positionCountAtLevel(level) >
                         atlas->vertexCount ||
                     mesh.indexCountAtLevel(level) > atlas->indexCount))
                --level;
            const size_t count = progressive ?
                mesh.indexCountAtLevel(level) : mesh.indices.size();
            if (!count || count > atlas->indexCount ||
                    count > std::numeric_limits<uint32_t>::max())
                return rejectIndirect(6, "invalid indirect draw count");
            if (commandCount && level != commandLevel)
                return rejectIndirect(
                    7, "mixed levels in one draw run");
            commandLevel = level;
            commandCount = count;

            InstVertex target = {};
            std::memcpy(target.transform, source.transform.data(),
                        16 * sizeof(float));
            target.color[0] = source.rgba[0] / 255.0f;
            target.color[1] = source.rgba[1] / 255.0f;
            target.color[2] = source.rgba[2] / 255.0f;
            target.color[3] = source.rgba[3] / 255.0f;
            const SbVec3f minimum = progressive ?
                mesh.progressiveQuantizationMinimum : SbVec3f(0, 0, 0);
            const SbVec3f maximum = progressive ?
                mesh.progressiveQuantizationMaximum : SbVec3f(0, 0, 0);
            for (int axis = 0; axis < 3; ++axis) {
                target.popMinLevel[axis] = minimum[axis];
                target.popMaxFlags[axis] = maximum[axis];
            }
            target.popMinLevel[3] = static_cast<float>(level);
            target.popMaxFlags[3] =
                (!mesh.normals.empty() ? 1.0f : 0.0f) +
                (progressive ? 2.0f : 0.0f) +
                ((source.flags & CadInstanceHidden) ? 4.0f : 0.0f);
            instances.push_back(target);
            sourceInstanceIndices.push_back(
                static_cast<uint32_t>(instanceIndex));
        }
        const uint32_t instanceCount =
            static_cast<uint32_t>(instances.size()) - baseInstance;
        if (!instanceCount)
            continue;
        if (!commandCount ||
                atlas->vertices.first >
                    static_cast<uint32_t>(
                        std::numeric_limits<int32_t>::max()))
            return rejectIndirect(8, "invalid atlas command base");

        CadDrawElementsIndirectCommand command;
        command.count = static_cast<uint32_t>(commandCount);
        command.instanceCount = instanceCount;
        command.firstIndex = atlas->indices.first;
        command.baseVertex = static_cast<int32_t>(atlas->vertices.first);
        command.baseInstance = baseInstance;
        renderedTriangleCount +=
            static_cast<uint64_t>(command.count / 3u) *
            static_cast<uint64_t>(command.instanceCount);
        IndirectPageWork& work = pageWorkFor(atlas->page);
        auto& commands =
            item.cullBackfaces ? work.culled : work.ordinary;
        if (item.partIndex < indirectCommandIndexByPart_.size()) {
            /*
             * A unique progressive part has exactly one active command and
             * one packed occurrence.  Shared parts intentionally retain the
             * sentinel and use the exact path when their LoD runs change.
             */
            if (item.instanceCount == 1u && instanceCount == 1u &&
                    indirectCommandIndexByPart_[item.partIndex] ==
                        noPreparedSlot) {
                indirectCommandIndexByPart_[item.partIndex] =
                    static_cast<uint32_t>(commands.size());
                indirectCommandCullByPart_[item.partIndex] =
                    item.cullBackfaces ? 1u : 0u;
                indirectPackedInstanceByPart_[item.partIndex] =
                    baseInstance;
            } else {
                indirectCommandIndexByPart_[item.partIndex] =
                    noPreparedSlot;
                indirectPackedInstanceByPart_[item.partIndex] =
                    noPreparedSlot;
            }
        }
        commands.push_back(command);
    }
    const bool havePageWork = std::any_of(
        indirectPageWorkScratch_.begin(),
        indirectPageWorkScratch_.end(),
        [](const IndirectPageWork& work) {
            return !work.ordinary.empty() || !work.culled.empty();
        });
    if (!havePageWork && pressureProxyPoints.empty())
        return rejectIndirect(9, "empty visible page work");

    /*
     * Preflight every page before issuing any draws.  Command streams larger
     * than a page's fixed scratch buffer are submitted in bounded chunks.
     * No recoverable rejection is permitted after the prepared frame becomes
     * visible to replay.
     */
    for (const IndirectPageWork& work :
            indirectPageWorkScratch_) {
        if (work.ordinary.empty() && work.culled.empty())
            continue;
        const CadTriangleAtlasPage *page =
            gpuRes_->triangleAtlasPage(work.page);
        if (!page || !page->indirectBuf || !page->indirectCapacity ||
                (work.ordinary.empty() && work.culled.empty()))
            return rejectIndirect(11, "indirect page preflight");
    }
    commandsCompleted = IndirectClock::now();

    /*
     * Publish an immutable CPU submission record.  A replay still touches
     * every demanded atlas part, which both protects it from reclamation and
     * verifies that generation, prefix capacity, page, and offsets remain
     * valid.  It can then skip visibility resolution, per-occurrence LoD,
     * instance packing, command construction, sorting, and proxy packing.
     */
    IndirectPreparedFrame& prepared = indirectPrepared_;
    prepared.valid = false;
    prepared.contextId = glue->contextid;
    prepared.planRevision = plan.revision;
    prepared.geometryRevision = plan.geometryRevision;
    prepared.shadedLayoutRevision =
        plan.shadedLayoutRevision;
    prepared.shadedLodRevision =
        plan.shadedLodRevision;
    prepared.appendRevision =
        plan.appendRevision;
    prepared.partGeometryRevision =
        plan.partGeometryRevision;
    prepared.instanceAttributeRevision =
        plan.instanceAttributeRevision;
    prepared.subpixelProxyRevision = plan.subpixelProxyRevision;
    prepared.progressiveLodCeiling =
        assembly.progressiveLodCeiling.getValue();
    prepared.viewProj = viewProj;
    prepared.renderedTriangleCount = renderedTriangleCount;
    prepared.instanceUploadSerial = 0;
    prepared.atlasRevision = gpuRes_->triangleAtlasRevision();
    prepared.atlasValidationCountdown = 30u;
    prepared.cameraMotionReplayCount = 0;

    prepared.parts.clear();
    if (prepared.parts.capacity() < visiblePartIndices.size())
        prepared.parts.reserve(visiblePartIndices.size());
    prepared.partByPlanPartIndex.assign(
        plan.partBindings.size(),
        std::numeric_limits<uint32_t>::max());
    for (const uint32_t partIndex : visiblePartIndices) {
        if (!visiblePart[partIndex])
            continue;
        const CadPartBinding& binding = plan.partBindings[partIndex];
        const PartGeometry *geometry = binding.geometry.get();
        const CadTriangleAtlasPart *atlas = atlasBindings[partIndex];
        if (!geometry || !geometry->shaded || !atlas)
            return rejectIndirect(11, "prepared part binding");
        IndirectPreparedPart demand;
        demand.part = binding.part;
        demand.partIndex = partIndex;
        demand.generation = binding.generation;
        demand.vertexCount = requestedVertexCounts[partIndex];
        demand.indexCount = requestedIndexCounts[partIndex];
        demand.page = atlas->page;
        demand.vertexFirst = atlas->vertices.first;
        demand.indexFirst = atlas->indices.first;
        demand.hasNormals = !geometry->shaded->normals.empty();
        if (partIndex < indirectPackedInstanceByPart_.size())
            demand.packedInstance =
                indirectPackedInstanceByPart_[partIndex];
        if (partIndex < indirectCommandIndexByPart_.size())
            demand.commandIndex =
                indirectCommandIndexByPart_[partIndex];
        if (partIndex < indirectCommandCullByPart_.size())
            demand.commandCulled =
                indirectCommandCullByPart_[partIndex] != 0u;
        prepared.partByPlanPartIndex[partIndex] =
            static_cast<uint32_t>(prepared.parts.size());
        prepared.parts.push_back(demand);
    }

    /*
     * Remove atlas holes/stale pages without releasing the vector storage of
     * active pages.  Moving active work into the prepared store transfers
     * capacities wholesale; the next exact build swaps them back.
     */
    indirectPageWorkScratch_.erase(
        std::remove_if(
            indirectPageWorkScratch_.begin(),
            indirectPageWorkScratch_.end(),
            [](const IndirectPageWork& work) {
                return work.ordinary.empty() && work.culled.empty();
            }),
        indirectPageWorkScratch_.end());
    prepared.pages.swap(indirectPageWorkScratch_);
    prepared.instances.swap(instances);
    prepared.sourceInstanceIndices.swap(sourceInstanceIndices);
    prepared.pressureProxyPoints.swap(pressureProxyPoints);
    prepared.pressureProxySourceInstanceIndices.swap(
        pressureProxySourceInstanceIndices);
    prepared.appendPatchAnchorInstanceCount =
        prepared.instances.size();
    prepared.instanceIndexBySource.assign(
        plan.visibleInstances.size(),
        std::numeric_limits<uint32_t>::max());
    for (size_t i = 0; i < prepared.sourceInstanceIndices.size(); ++i) {
        const uint32_t sourceIndex =
            prepared.sourceInstanceIndices[i];
        if (sourceIndex >= prepared.instanceIndexBySource.size()) {
            prepared.valid = false;
            return rejectIndirect(
                11, "prepared instance reverse index");
        }
        prepared.instanceIndexBySource[sourceIndex] =
            static_cast<uint32_t>(i);
    }
    prepared.pressureProxyIndexBySource.assign(
        plan.visibleInstances.size(),
        std::numeric_limits<uint32_t>::max());
    for (size_t i = 0;
            i < prepared.pressureProxySourceInstanceIndices.size(); ++i) {
        const uint32_t sourceIndex =
            prepared.pressureProxySourceInstanceIndices[i];
        if (sourceIndex >=
                prepared.pressureProxyIndexBySource.size()) {
            prepared.valid = false;
            return rejectIndirect(
                11, "prepared proxy reverse index");
        }
        prepared.pressureProxyIndexBySource[sourceIndex] =
            static_cast<uint32_t>(i);
    }
    prepared.valid = true;

    pressureProxyPointsView_ = &prepared.pressureProxyPoints;
    pressureProxyAppendOnly_ = false;
    ++pressureProxyRevision_;
    if (!pressureProxyRevision_)
        pressureProxyRevision_ = 1;
    lastRenderedTriangleCount_ = renderedTriangleCount;

    const bool submitted =
        submitIndirectPrepared(glue, viewProj, viewVolume);
    if (!submitted) {
        prepared.valid = false;
        pressureProxyPointsView_ = nullptr;
        return false;
    }
    if (lastIndirectStatus_ != 0)
        prepared.valid = false;

    /*
     * The atlas is now the only shaded geometry owner for this context.
     * Retaining the former expanded world-space and per-part triangle VBOs
     * would defeat the unified memory ceiling.
     */
    gpuRes_->releaseFlatShaded(glue);
    gpuRes_->releaseStandaloneTriangles(glue);
    if (lastIndirectStatus_ == 0)
        reportedIndirectStatus_ = 0;
    if (std::getenv("OBOL_CAD_RENDER_TIMING")) {
        const auto completed = IndirectClock::now();
        const auto milliseconds = [](auto begin, auto end) {
            return std::chrono::duration<double, std::milli>(
                end - begin).count();
        };
        const double total =
            milliseconds(indirectStarted, completed);
        if (total >= 10.0)
            std::fprintf(stderr,
                "CadRendererGL exact indirect total=%.3fms "
                "visibility=%.3fms protect=%.3fms admit=%.3fms "
                "commands=%.3fms publish-submit=%.3fms "
                "source_instances=%zu visible_instances=%zu "
                "visible_parts=%zu triangles=%llu\n",
                total,
                milliseconds(indirectStarted,
                    visibilityCompleted),
                milliseconds(visibilityCompleted,
                    protectionCompleted),
                milliseconds(protectionCompleted,
                    admissionCompleted),
                milliseconds(admissionCompleted,
                    commandsCompleted),
                milliseconds(commandsCompleted, completed),
                plan.visibleInstances.size(),
                visibleOccurrenceCount,
                visiblePartIndices.size(),
                static_cast<unsigned long long>(
                    renderedTriangleCount));
    }
    if (std::getenv("OBOL_CAD_INDIRECT_DEBUG")) {
        static uint32_t lastDebugContext = UINT32_MAX;
        static size_t lastDebugParts = 0;
        const size_t parts = gpuRes_->triangleAtlasPartCount();
        if (lastDebugContext != glue->contextid ||
                parts >= lastDebugParts + 4096u) {
            std::fprintf(stderr,
                "CadRendererGL indirect rendered context=%u commands=%zu "
                "pages=%zu parts=%zu allocated=%zu live=%zu\n",
                glue->contextid, plan.shadedItems.size(),
                gpuRes_->triangleAtlasPageCount(), parts,
                gpuRes_->triangleAtlasAllocatedBytes(),
                gpuRes_->triangleAtlasLiveBytes());
            lastDebugContext = glue->contextid;
            lastDebugParts = parts;
        }
    }
    return true;
}

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
    // Build per-instance vertex data (transform + colour)
    const size_t nInst = plan.visibleInstances.size();
    if (nInst == 0) return;
    uint64_t renderedTriangleCount = 0;

    std::vector<InstVertex> instData(nInst);
    for (size_t i = 0; i < nInst; ++i) {
        const auto& vi = plan.visibleInstances[i];
        std::memcpy(instData[i].transform, vi.transform.data(), 16 * sizeof(float));
        instData[i].color[0] = vi.rgba[0] / 255.0f;
        instData[i].color[1] = vi.rgba[1] / 255.0f;
        instData[i].color[2] = vi.rgba[2] / 255.0f;
        instData[i].color[3] = vi.rgba[3] / 255.0f;
    }

    gpuRes_->uploadTransientInstanceData(
        instData.data(),
        static_cast<GLsizeiptr>(nInst * sizeof(InstVertex)),
        glue);

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
            if (solidWireOnly && item.customWireStyle) continue;
            CadWireGpu* w = gpuRes_->wireFor(item.rep.part);
            if (!w || w->segCount == 0) continue;
            const size_t levelInstanceIndex =
                firstDrawItemInstance(plan, item);
            if (levelInstanceIndex >= plan.visibleInstances.size())
                continue;
            const PartGeometry *geometry =
                assembly.partGeometry(item.rep.part);
            const WireRep *progressive =
                geometry && geometry->wire &&
                geometry->wire->isProgressive() ?
                &*geometry->wire : nullptr;
            const CadVisibleInstance& levelInstance =
                plan.visibleInstances[levelInstanceIndex];
            const uint8_t level = progressive ?
                progressiveLevel(
                    assembly.effectiveProgressiveLodLevel(
                        levelInstance.lodLevel),
                    progressive->progressiveMinimumLevel,
                    progressive->progressiveResidentLevel) : 15;
            const int variant = progressive && level < 15 ? 1 : 0;
            const WireLocations& loc = locations[variant];
            if (!programs[variant])
                continue;
            if (activeProgram != programs[variant]) {
                activeProgram = programs[variant];
                glue->glUseProgramObjectARB(activeProgram);
                glue->glUniformMatrix4fvARB(
                    loc.viewProjection, 1, GL_FALSE, vp);
            }
            if (variant) {
                uploadProgressivePositionUniforms(
                    glue, loc.encodeScale, loc.decodeScale, loc.minimum,
                    level,
                    progressive->progressiveQuantizationMinimum,
                    progressive->progressiveQuantizationMaximum);
            }
            const GLsizei segmentCount = progressiveWireSegmentCount(
                assembly, item.rep.part, levelInstance, w->segCount);
            if (segmentCount <= 0)
                continue;
            uint32_t runStart = 0;
            while (runStart < item.instanceCount) {
                while (runStart < item.instanceCount &&
                    (!drawItemOwnsInstance(
                        plan, item, item.baseInstance + runStart) ||
                     isHiddenInstance(
                        plan, item.baseInstance + runStart) ||
                     isSubpixelProxyInstance(
                        plan, item.baseInstance + runStart)))
                    ++runStart;
                if (runStart == item.instanceCount)
                    break;
                uint32_t runEnd = runStart + 1;
                while (runEnd < item.instanceCount &&
                    drawItemOwnsInstance(
                        plan, item, item.baseInstance + runEnd) &&
                    !isHiddenInstance(
                        plan, item.baseInstance + runEnd) &&
                    !isSubpixelProxyInstance(
                        plan, item.baseInstance + runEnd))
                    ++runEnd;

                const uint32_t baseInstance = item.baseInstance + runStart;
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
                                                GL_LINES, 0, segmentCount * 2,
                                                runCount);
                } else {
                    glue->glDrawElementsInstanced(
                                                  GL_LINES, segmentCount * 2,
                                                  GL_UNSIGNED_INT, nullptr,
                                                  runCount);
                }

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
        }

        glue->glUseProgramObjectARB(0);
        restoreWireRasterState(glue, rasterState, caps_.hasLineStipple);
    }

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
            CadTriGpu* t = gpuRes_->triFor(item.rep.part);
            if (!t || t->idxCount == 0) continue;
            const size_t levelInstanceIndex =
                firstDrawItemInstance(plan, item);
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
                progressiveLevel(
                    assembly.effectiveProgressiveLodLevel(
                        levelInstance.lodLevel),
                    progressive->progressiveMinimumLevel,
                    progressive->progressiveResidentLevel) : 15;
            const int variant = progressive && level < 15 ? 1 : 0;
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
                    level,
                    progressive->progressiveQuantizationMinimum,
                    progressive->progressiveQuantizationMaximum);
            }
            const GLsizei indexCount = progressiveTriangleIndexCount(
                assembly, item.rep.part, levelInstance, t->idxCount);
            if (indexCount <= 0)
                continue;

            setCadBackfaceCulling(glue, item.cullBackfaces);
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
                while (runStart < item.instanceCount &&
                        (!drawItemOwnsInstance(
                            plan, item,
                            item.baseInstance + runStart) ||
                         isHiddenInstance(
                            plan, item.baseInstance + runStart) ||
                         isSubpixelProxyInstance(
                            plan, item.baseInstance + runStart)))
                    ++runStart;
                if (runStart == item.instanceCount)
                    break;
                uint32_t runEnd = runStart + 1;
                while (runEnd < item.instanceCount &&
                        drawItemOwnsInstance(
                            plan, item,
                            item.baseInstance + runEnd) &&
                        !isHiddenInstance(
                            plan, item.baseInstance + runEnd) &&
                        !isSubpixelProxyInstance(
                            plan, item.baseInstance + runEnd))
                    ++runEnd;
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
        }

        glue->glUseProgramObjectARB(0);
        lastRenderedTriangleCount_ = renderedTriangleCount;
    }
}

// ---------------------------------------------------------------------------
// releaseGpuResources()
// ---------------------------------------------------------------------------

void CadRendererGL::releaseGpuResources(const SoGLContext* glue)
{
    if (glue) {
        releaseContext(glue->contextid, glue);
    } else {
        for (auto &entry : gpuResources_)
            entry.second->releaseAll(nullptr);
        gpuResources_.clear();
        gpuRes_ = nullptr;
        gpuContextId_ = 0;
    }
    shaders_ = ShaderPrograms();
    capsDetected_ = false;
    shadersContextId_ = 0;
}

} // namespace internal
} // namespace Obol
