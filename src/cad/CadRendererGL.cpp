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
    CadDirectGLStateGuard(const SoGLContext *context, bool vbo, bool vao)
        : glue_(context), hasVbo_(vbo), hasVao_(vao)
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
    GLint program_ = 0;
    GLint vao_ = 0;
    GLint arrayBuffer_ = 0;
    GLint elementBuffer_ = 0;
    GLint matrixMode_ = GL_MODELVIEW;
    GLboolean colorMask_[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
    GLboolean polygonOffsetEnabled_ = GL_FALSE;
    GLfloat polygonOffsetFactor_ = 0.0f;
    GLfloat polygonOffsetUnits_ = 0.0f;
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
    "vec3 popPosition(vec3 p) {\n"
    "    vec3 scaled = (p - u_popMin) * u_popEncodeScale;\n"
    "    vec3 low = floor(scaled);\n"
    "    vec3 high = ceil(scaled);\n"
    "    return (low + high) * u_popDecodeScale + u_popMin;\n"
    "}\n"
    "void main() {\n"
    "    vec4 wp = u_model * vec4(popPosition(a_pos), 1.0);\n"
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
    "vec3 popPosition(vec3 p) {\n"
    "    vec3 scaled = (p - u_popMin) * u_popEncodeScale;\n"
    "    vec3 low = floor(scaled);\n"
    "    vec3 high = ceil(scaled);\n"
    "    return (low + high) * u_popDecodeScale + u_popMin;\n"
    "}\n"
    "void main() {\n"
    "    vec4 wp = u_model * vec4(popPosition(a_pos), 1.0);\n"
    "    gl_Position = u_viewProj * wp;\n"
    "    v_worldPos = wp.xyz;\n"
    "    v_norm = mat3(u_model[0].xyz, u_model[1].xyz, u_model[2].xyz) * a_norm;\n"
    "    v_color = u_color;\n"
    "}\n";

static const char * kShadedFS1 =
    "uniform int   u_numLights;\n"
    "uniform int   u_hasNorm;\n"
    "uniform int   u_ltype[8];\n"
    "uniform vec3  u_lvec[8];\n"
    "uniform vec3  u_laxis[8];\n"
    "uniform vec3  u_lcolor[8];\n"
    "uniform float u_lcos[8];\n"
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
    "    }\n"
    "    vec3 col = v_color.rgb * 0.25;\n"
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
    "        col += v_color.rgb * u_lcolor[i] * (ndl * 0.75 * atten);\n"
    "    }\n"
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

static const char * kShadedFS2 =
    "#version 140\n"
    "uniform int   u_numLights;\n"
    "uniform int   u_hasNorm;\n"
    "uniform int   u_ltype[8];\n"
    "uniform vec3  u_lvec[8];\n"
    "uniform vec3  u_laxis[8];\n"
    "uniform vec3  u_lcolor[8];\n"
    "uniform float u_lcos[8];\n"
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
    "    }\n"
    "    vec3 col = v_color.rgb * 0.25;\n"
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
    "        col += v_color.rgb * u_lcolor[i] * (ndl * 0.75 * atten);\n"
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
    GLuint wireInst = 0;
    GLuint shadedInst = 0;
};

std::mutex sharedCadShadersMutex;
std::unordered_map<uint32_t, SharedCadShaders> sharedCadShaders;
std::once_flag sharedCadShadersCallbackOnce;
std::mutex cadRendererRegistryMutex;
std::unordered_set<CadRendererGL *> cadRendererRegistry;
std::once_flag cadRendererRegistryCallbackOnce;

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
        if (programs.wireInst) glue->glDeleteObjectARB(programs.wireInst);
        if (programs.shadedInst) glue->glDeleteObjectARB(programs.shadedInst);
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
            GLuint vs = compileShader(glue, GL_VERTEX_SHADER_ARB, kShadedVS2);
            GLuint fs = compileShader(glue, GL_FRAGMENT_SHADER_ARB, kShadedFS2);
            if (vs && fs) {
                shaders_.shadedInst = linkProgram(glue, vs, fs);
            }
            if (vs) glue->glDeleteObjectARB(vs);
            if (fs) glue->glDeleteObjectARB(fs);
        }
        // If instanced shaders failed, fall back to Tier-1
        if (!shaders_.wireInst || !shaders_.shadedInst) {
            if (shaders_.wireInst)   glue->glDeleteObjectARB(shaders_.wireInst);
            if (shaders_.shadedInst) glue->glDeleteObjectARB(shaders_.shadedInst);
            shaders_.wireInst   = 0;
            shaders_.shadedInst = 0;
        }
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
            shaders_.wireInst = found->second.wireInst;
            shaders_.shadedInst = found->second.shadedInst;
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
            programs.wireInst = shaders_.wireInst;
            programs.shadedInst = shaders_.shadedInst;
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
        size_t requiredPositionCount = mesh.positions.size();
        if (mesh.isProgressive() && requiredIndexCount > 0) {
            uint32_t maximumIndex = 0;
            for (size_t i = 0; i < requiredIndexCount; ++i) {
                if (mesh.indices[i] >= mesh.positions.size())
                    return;
                maximumIndex = std::max(maximumIndex, mesh.indices[i]);
            }
            requiredPositionCount =
                static_cast<size_t>(maximumIndex) + 1;
        }
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
        const CadFramePlan& plan, PartId part)
{
    uint8_t requested = 0;
    bool found = false;
    const auto consider = [&](const CadDrawItem& item) {
        if (!(item.rep.part == part))
            return;
        for (uint32_t i = 0; i < item.instanceCount; ++i) {
            const size_t visibleIndex = item.baseInstance + i;
            if (visibleIndex >= plan.visibleInstances.size())
                continue;
            requested = std::max(
                requested, plan.visibleInstances[visibleIndex].lodLevel);
            found = true;
        }
    };
    for (const CadDrawItem& item : plan.wireItems)
        consider(item);
    for (const CadDrawItem& item : plan.shadedItems)
        consider(item);
    return found ? requested : 15;
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
                const CadVisibleInstance& inst =
                    plan.visibleInstances[item.baseInstance + i];
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
                const CadVisibleInstance& inst =
                    plan.visibleInstances[item.baseInstance + i];
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

bool CadRendererGL::wireRepHasUncollapsedInstances(
        const CadFramePlan& plan, PartId part)
{
    for (const CadDrawItem& item : plan.wireItems) {
        if (!(item.rep.part == part)) continue;
        for (uint32_t i = 0; i < item.instanceCount; ++i) {
            if (!isSubpixelProxyInstance(plan, item.baseInstance + i))
                return true;
        }
    }
    return false;
}

void CadRendererGL::renderSubpixelProxyPoints(
        const CadFramePlan& plan, const SoGLContext* glue,
        const SbMatrix& viewProj)
{
    if (plan.subpixelProxyPoints.empty())
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

    const CadSubpixelProxyGpu& cached = gpuRes_->subpixelProxyPoints();
    if (cached.revision != plan.subpixelProxyRevision || !cached.posBuf ||
            !cached.colorBuf) {
        std::vector<float> positions;
        std::vector<uint8_t> colors;
        positions.reserve(plan.subpixelProxyPoints.size() * 3u);
        colors.reserve(plan.subpixelProxyPoints.size() * 4u);
        for (const CadSubpixelProxyPoint& point : plan.subpixelProxyPoints) {
            appendPackedPoint(positions, point.position);
            colors.insert(colors.end(), point.rgba.begin(), point.rgba.end());
        }
        gpuRes_->uploadSubpixelProxyPoints(plan.subpixelProxyRevision,
                                           positions, colors, glue, caps_);
    }
    const CadSubpixelProxyGpu& gpu = gpuRes_->subpixelProxyPoints();
    if (!gpu.posBuf || !gpu.colorBuf || gpu.count <= 0) {
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
        glue->glBindBuffer(GL_ARRAY_BUFFER, gpu.posBuf);
        glue->glVertexPointer(3, GL_FLOAT, 3 * sizeof(float), nullptr);
        glue->glBindBuffer(GL_ARRAY_BUFFER, gpu.colorBuf);
        glue->glColorPointer(4, GL_UNSIGNED_BYTE, 4 * sizeof(uint8_t),
                             nullptr);
        glue->glDrawArrays(GL_POINTS, 0, gpu.count);
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
        if (gpu.vao && glue->glBindVertexArray) {
            glue->glBindVertexArray(gpu.vao);
        } else {
            glue->glBindBuffer(GL_ARRAY_BUFFER, gpu.posBuf);
            glue->glVertexAttribPointerARB(0, 3, GL_FLOAT, GL_FALSE,
                                           3 * sizeof(float), nullptr);
            glue->glEnableVertexAttribArrayARB(0);
            glue->glBindBuffer(GL_ARRAY_BUFFER, gpu.colorBuf);
            glue->glVertexAttribPointerARB(1, 4, GL_UNSIGNED_BYTE, GL_TRUE,
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
        const std::unordered_map<PartId, uint64_t,
                                 std::hash<PartId>>& partGenMap)
{
    if (!ensureReady(glue)) return;
    if (plan.visibleInstances.empty()) return;
    gpuRes_->beginProgressiveFrame();

    CadDirectGLStateGuard directState(
        glue, caps_.hasVBO,
        caps_.hasVAO && glue->glBindVertexArray != nullptr);

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
    const bool useFlatShaded = flatShadedEnabled && canUseFlatShaded &&
        !retainedProgressive &&
        (hiddenLine || plan.shadedItems.size() >= 128);

    renderPoints(plan, assembly, glue, viewProj, partGenMap);

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
                maximumRequestedLod(plan, repKey.part), glue);
        }
        CadFramePlan depthPlan = plan;
        depthPlan.wireItems.clear();
        CadFramePlan wirePlan = plan;
        wirePlan.shadedItems.clear();

        SoGLContext_glColorMask(glue, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        SoGLContext_glEnable(glue, GL_POLYGON_OFFSET_FILL);
        SoGLContext_glPolygonOffset(glue, 1.0f, 1.0f);
        if (caps_.canUseVbo() && shaders_.shaded &&
                progressiveShadedShaderReady) {
            renderVboLoop(depthPlan, assembly, glue, viewProj, false, true);
        } else if (caps_.canUseFixedVbo()) {
            renderFixedVboLoop(depthPlan, assembly, glue, viewProj, viewMatrix,
                               projectionMatrix);
        } else {
            renderImmediateMode(depthPlan, assembly, glue, viewProj, viewMatrix,
                                projectionMatrix);
        }
        SoGLContext_glDisable(glue, GL_POLYGON_OFFSET_FILL);
        SoGLContext_glColorMask(glue, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

        if (!retainedProgressive &&
                caps_.canUseInstanced() && shaders_.wireInst) {
            lastRenderTier_ = 2;
            renderInstanced(wirePlan, assembly, glue, viewProj, partGenMap,
                            false, false);
        } else if (caps_.canUseVbo() && shaders_.wire &&
                progressiveWireShaderReady) {
            lastRenderTier_ = 1;
            renderVboLoop(wirePlan, assembly, glue, viewProj, false, false);
        } else if (caps_.canUseFixedVbo()) {
            lastRenderTier_ = 1;
            renderFixedVboLoop(wirePlan, assembly, glue, viewProj, viewMatrix,
                               projectionMatrix);
        } else {
            lastRenderTier_ = 0;
            renderImmediateMode(wirePlan, assembly, glue, viewProj, viewMatrix,
                                projectionMatrix);
        }
        renderSubpixelProxyPoints(plan, glue, viewProj);
        gpuRes_->endProgressiveFrame(glue);
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
            gpuRes_->endProgressiveFrame(glue);
            return;
        }
    }

    // Upload geometry for the per-part rendering paths only after the flat
    // aggregate paths have declined the frame.
    for (const auto& repKey : plan.requiredReps) {
        if (repKey.type == CadRepType::WireSegments &&
                !wireRepHasUncollapsedInstances(plan, repKey.part))
            continue;
        auto genIt = partGenMap.find(repKey.part);
        uint64_t gen = (genIt != partGenMap.end()) ? genIt->second : 0;
        ensurePartUploaded(
            repKey.part, assembly, gen,
            maximumRequestedLod(plan, repKey.part), glue);
    }

    const bool needsShadedLod =
        retainedProgressive && !plan.shadedItems.empty();

    const char *flatWireEnv = std::getenv("OBOL_CAD_FLAT_WIRE");
    const bool flatWireEnabled = flatWireEnv ? flatWireEnv[0] != '0' : true;
    const bool canUseFlatWire = caps_.isSoftwareRenderer ?
        caps_.canUseFixedVbo() : (caps_.canUseVbo() && shaders_.wire);
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

    if (flatWireEnabled && !retainedProgressive &&
            plan.shadedItems.empty() &&
            plan.wireItems.size() >= 128 &&
            canUseFlatWire &&
            renderFlatWire(plan, assembly, glue, viewProj)) {
        lastRenderTier_ = 3;
    } else if (caps_.isSoftwareRenderer && !softwareGlslRequested() &&
            caps_.canUseFixedVbo()) {
        lastRenderTier_ = 1;
        renderFixedVboLoop(plan, assembly, glue, viewProj, viewMatrix,
                           projectionMatrix);
    } else if (!needsShadedLod && caps_.canUseInstanced() &&
            shaders_.wireInst && shaders_.shadedInst) {
        lastRenderTier_ = 2;
        renderInstanced(plan, assembly, glue, viewProj, partGenMap,
                        false, true);
    } else if (caps_.canUseVbo() && shaders_.wire &&
            progressiveWireShaderReady && progressiveShadedShaderReady) {
        lastRenderTier_ = 1;
        renderVboLoop(plan, assembly, glue, viewProj, false, true);
    } else if (caps_.canUseFixedVbo()) {
        lastRenderTier_ = 1;
        renderFixedVboLoop(plan, assembly, glue, viewProj, viewMatrix,
                           projectionMatrix);
    } else {
        lastRenderTier_ = 0;
        renderImmediateMode(plan, assembly, glue, viewProj, viewMatrix,
                            projectionMatrix);
    }

    if (!plan.shadedItems.empty() && !polygonOffsetWasEnabled)
        SoGLContext_glDisable(glue, GL_POLYGON_OFFSET_FILL);
    renderSubpixelProxyPoints(plan, glue, viewProj);
    gpuRes_->endProgressiveFrame(glue);
}

namespace {

struct FlatWireStyleKey {
    uint32_t rgba = 0;
    uint32_t widthBits = 0;
    uint16_t pattern = 0xffffu;
    uint16_t factor = 1u;

    bool operator<(const FlatWireStyleKey& other) const noexcept {
        if (rgba != other.rgba) return rgba < other.rgba;
        if (widthBits != other.widthBits) return widthBits < other.widthBits;
        if (pattern != other.pattern) return pattern < other.pattern;
        return factor < other.factor;
    }
};

static FlatWireStyleKey flatWireStyleKey(const CadVisibleInstance& inst)
{
    FlatWireStyleKey key;
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

static SbVec3f transformedFlatNormal(
        const SbVec3f& normal,
        const std::array<float, 16>& matrix)
{
    SbMatrix transform;
    transform.setValue(matrix.data());
    const SbMatrix normalMatrix = transform.inverse().transpose();
    SbVec3f result;
    normalMatrix.multDirMatrix(normal, result);
    if (result.sqrLength() > 0.0f)
        result.normalize();
    else
        result.setValue(0.0f, 0.0f, 1.0f);
    return result;
}

static uint32_t flatRgbaKey(const CadVisibleInstance& inst)
{
    return static_cast<uint32_t>(inst.rgba[0]) |
           (static_cast<uint32_t>(inst.rgba[1]) << 8) |
           (static_cast<uint32_t>(inst.rgba[2]) << 16) |
           (static_cast<uint32_t>(inst.rgba[3]) << 24);
}

} // namespace

bool CadRendererGL::renderFlatWire(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const SoGLContext* glue,
        const SbMatrix& viewProj)
{
    constexpr size_t maxPositionBytes = 256u * 1024u * 1024u;
    const uint64_t presentationRevision = plan.subpixelProxyRevision ?
        plan.subpixelProxyRevision : plan.revision;
    const CadFlatWireGpu& cached = gpuRes_->flatWire();
    if (cached.planRevision != presentationRevision) {
        // Proxy membership changes with the camera, so the flattened buffer
        // must be rebuilt even when the underlying assembly is unchanged.
        const bool rebuildGeometry = true;
        size_t pointCount = 0;
        for (const CadDrawItem& item : plan.wireItems) {
            const Obol::PartGeometry *geom = assembly.partGeometry(item.rep.part);
            if (!geom || !geom->wire.has_value()) continue;
            const Obol::WireRep& wire = *geom->wire;
            size_t segments = wire.segmentCount();
            for (const Obol::WirePolyline& poly : wire.polylines)
                if (poly.points.size() >= 2)
                    segments += poly.points.size() - 1;
            if (segments == 0) continue;
            for (uint32_t ii = 0; ii < item.instanceCount; ++ii) {
                if (isSubpixelProxyInstance(plan, item.baseInstance + ii))
                    continue;
                const size_t maxPoints =
                    maxPositionBytes / (3 * sizeof(float));
                if (segments > maxPoints / 2 ||
                        pointCount > maxPoints - segments * 2)
                    return false;
                pointCount += segments * 2;
            }
        }

        std::vector<float> positions;
        if (rebuildGeometry)
            positions.resize(pointCount * 3);
        size_t positionOffset = 0;
        std::vector<CadFlatWireGroup> groups;
        FlatWireStyleKey activeKey;
        bool haveGroup = false;
        size_t vertexOffset = 0;
        for (const CadDrawItem& item : plan.wireItems) {
            const Obol::PartGeometry *geom = assembly.partGeometry(item.rep.part);
            if (!geom || !geom->wire.has_value()) continue;
            const Obol::WireRep& wire = *geom->wire;
            size_t segments = wire.segmentCount();
            for (const Obol::WirePolyline& poly : wire.polylines)
                if (poly.points.size() >= 2)
                    segments += poly.points.size() - 1;
            const size_t instanceVertices = segments * 2;
            for (uint32_t ii = 0; ii < item.instanceCount; ++ii) {
                const size_t visibleIndex = item.baseInstance + ii;
                if (isSubpixelProxyInstance(plan, visibleIndex))
                    continue;
                const CadVisibleInstance& inst =
                    plan.visibleInstances[visibleIndex];
                const FlatWireStyleKey key = flatWireStyleKey(inst);
                if (!haveGroup || key < activeKey || activeKey < key) {
                    CadFlatWireGroup group;
                    group.first = static_cast<GLint>(vertexOffset);
                    group.lineWidth = inst.lineWidth;
                    group.linePattern = inst.linePattern;
                    group.linePatternFactor = inst.linePatternFactor;
                    std::copy(inst.rgba.begin(), inst.rgba.end(), group.rgba);
                    groups.push_back(group);
                    activeKey = key;
                    haveGroup = true;
                }
                groups.back().count += static_cast<GLsizei>(instanceVertices);
                vertexOffset += instanceVertices;
                if (rebuildGeometry) {
                    for (size_t p = 0; p + 1 < wire.segmentPoints.size(); p += 2) {
                        writeTransformedFlatPoint(positions, positionOffset,
                                                  wire.segmentPoints[p], inst.transform);
                        writeTransformedFlatPoint(positions, positionOffset,
                                                  wire.segmentPoints[p + 1], inst.transform);
                    }
                    for (const Obol::WirePolyline& poly : wire.polylines) {
                        for (size_t p = 0; p + 1 < poly.points.size(); ++p) {
                            writeTransformedFlatPoint(positions, positionOffset,
                                                      poly.points[p], inst.transform);
                            writeTransformedFlatPoint(positions, positionOffset,
                                                      poly.points[p + 1], inst.transform);
                        }
                    }
                }
            }
        }
        if (pointCount == 0 || groups.empty()) return false;
        if (rebuildGeometry)
            gpuRes_->uploadFlatWire(presentationRevision, plan.geometryRevision,
                                    positions, groups, glue, caps_);
        else
            gpuRes_->updateFlatWireGroups(presentationRevision, groups);
    }

    const CadFlatWireGpu& flat = gpuRes_->flatWire();
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
        glue->glDrawArrays(GL_LINES, group.first, group.count);
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
    const CadFlatShadedGpu& cached = gpuRes_->flatShaded();
    if (cached.planRevision != plan.revision) {
        const bool rebuildGeometry =
            cached.geometryRevision != plan.geometryRevision;
        struct Occurrence {
            const CadDrawItem *item;
            uint32_t instanceOffset;
        };
        std::vector<Occurrence> occurrences;
        size_t vertexCount = 0;
        for (const CadDrawItem& item : plan.shadedItems) {
            const Obol::PartGeometry *geom = assembly.partGeometry(item.rep.part);
            if (!geom || !geom->shaded.has_value()) continue;
            const size_t count = geom->shaded->indices.size();
            if (count == 0 || count > maxVertexBytes / (6 * sizeof(float)))
                return false;
            if (item.instanceCount >
                    (maxVertexBytes / (6 * sizeof(float))) / count)
                return false;
            vertexCount += count * item.instanceCount;
            if (vertexCount > maxVertexBytes / (6 * sizeof(float)))
                return false;
            for (uint32_t ii = 0; ii < item.instanceCount; ++ii)
                occurrences.push_back(Occurrence{&item, ii});
        }
        std::vector<float> positions;
        std::vector<float> normals;
        if (rebuildGeometry) {
            positions.resize(vertexCount * 3);
            normals.resize(vertexCount * 3);
        }
        size_t shadedOffset = 0;
        std::vector<CadFlatShadedGroup> groups;
        uint32_t activeColor = 0;
        bool haveGroup = false;
        size_t vertexOffset = 0;
        for (const Occurrence& occurrence : occurrences) {
            const CadDrawItem& item = *occurrence.item;
            const CadVisibleInstance& inst = plan.visibleInstances[
                item.baseInstance + occurrence.instanceOffset];
            const uint32_t color = flatRgbaKey(inst);
            if (!haveGroup || color != activeColor ||
                    occurrence.item->cullBackfaces !=
                        groups.back().cullBackfaces) {
                CadFlatShadedGroup group;
                group.first = static_cast<GLint>(vertexOffset);
                std::copy(inst.rgba.begin(), inst.rgba.end(), group.rgba);
                group.cullBackfaces = occurrence.item->cullBackfaces;
                groups.push_back(group);
                activeColor = color;
                haveGroup = true;
            }

            const Obol::TriMesh& mesh =
                *assembly.partGeometry(item.rep.part)->shaded;
            const bool hasVertexNormals =
                mesh.normals.size() == mesh.positions.size();
            groups.back().count += static_cast<GLsizei>(mesh.indices.size());
            vertexOffset += mesh.indices.size();
            if (!rebuildGeometry)
                continue;
            for (size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
                const uint32_t ia = mesh.indices[t];
                const uint32_t ib = mesh.indices[t + 1];
                const uint32_t ic = mesh.indices[t + 2];
                if (ia >= mesh.positions.size() || ib >= mesh.positions.size() ||
                        ic >= mesh.positions.size())
                    return false;
                const SbVec3f a = transformedFlatPoint(mesh.positions[ia],
                                                       inst.transform);
                const SbVec3f b = transformedFlatPoint(mesh.positions[ib],
                                                       inst.transform);
                const SbVec3f c = transformedFlatPoint(mesh.positions[ic],
                                                       inst.transform);
                SbVec3f normal = (b - a).cross(c - a);
                if (normal.sqrLength() > 0.0f)
                    normal.normalize();
                else
                    normal.setValue(0.0f, 0.0f, 1.0f);
                const SbVec3f triangle[3] = {a, b, c};
                const SbVec3f triangleNormals[3] = {
                    hasVertexNormals ?
                        transformedFlatNormal(mesh.normals[ia], inst.transform) : normal,
                    hasVertexNormals ?
                        transformedFlatNormal(mesh.normals[ib], inst.transform) : normal,
                    hasVertexNormals ?
                        transformedFlatNormal(mesh.normals[ic], inst.transform) : normal
                };
                for (size_t vertex = 0; vertex < 3; ++vertex) {
                    const SbVec3f& point = triangle[vertex];
                    const SbVec3f& vertexNormal = triangleNormals[vertex];
                    positions[shadedOffset] = point[0];
                    normals[shadedOffset++] = vertexNormal[0];
                    positions[shadedOffset] = point[1];
                    normals[shadedOffset++] = vertexNormal[1];
                    positions[shadedOffset] = point[2];
                    normals[shadedOffset++] = vertexNormal[2];
                }
            }
        }
        if (vertexCount == 0 || groups.empty()) return false;
        if (rebuildGeometry)
            gpuRes_->uploadFlatShaded(plan.revision, plan.geometryRevision,
                                      positions, normals, groups, glue, caps_);
        else
            gpuRes_->updateFlatShadedGroups(plan.revision, groups);
    }

    const CadFlatShadedGpu& flat = gpuRes_->flatShaded();
    if (!flat.posBuf || !flat.normBuf || flat.groups.empty()) return false;

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
            glue->glDrawArrays(GL_TRIANGLES, group.first, group.count);
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
        glue->glDrawArrays(GL_TRIANGLES, group.first, group.count);
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
            glue->glDrawArrays(GL_TRIANGLES, group.first, group.count);
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
            glue->glDrawArrays(GL_TRIANGLES, group.first, group.count);
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
        instance.lodLevel));
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
        instance.lodLevel));
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
    if (!plan.wireItems.empty()) {
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
                if (isSubpixelProxyInstance(plan, visibleIndex)) continue;
                const auto& inst = plan.visibleInstances[visibleIndex];
                if (isBoxOutsideFrustum(inst.wbMin, inst.wbMax, fp)) continue;

                const uint8_t level = progressive ?
                    progressiveLevel(
                        inst.lodLevel, progressive->progressiveMinimumLevel,
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
            GLint encodeScale = -1;
            GLint decodeScale = -1;
            GLint minimum = -1;
        };
        const GLuint programs[2] = {shaders_.shaded, shaders_.shadedPop};
        ShadedLocations locations[2];
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
            locations[variant].hasNormal = glue->glGetUniformLocationARB(
                programs[variant], "u_hasNorm");
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
                const auto& inst = plan.visibleInstances[item.baseInstance + i];
                if (isBoxOutsideFrustum(inst.wbMin, inst.wbMax, fp)) continue;

                const uint8_t level = progressive ?
                    progressiveLevel(
                        inst.lodLevel, progressive->progressiveMinimumLevel,
                        progressive->progressiveResidentLevel) : 15;
                const int variant = progressive && level < 15 ? 1 : 0;
                const ShadedLocations& loc = locations[variant];
                if (activeProgram != programs[variant]) {
                    activeProgram = programs[variant];
                    glue->glUseProgramObjectARB(activeProgram);
                    glue->glUniformMatrix4fvARB(
                        loc.viewProjection, 1, GL_FALSE, vpData);
                    if (!lightsUploaded[variant]) {
                        this->uploadLights(glue, activeProgram);
                        lightsUploaded[variant] = true;
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
        const SbMatrix& projectionMatrix)
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

    for (const auto& item : plan.wireItems) {
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
            if (isSubpixelProxyInstance(plan, visibleIndex)) continue;
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
                    inst.lodLevel, progressive->progressiveMinimumLevel,
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
    if (!plan.shadedItems.empty()) {
        // CAD shading must not depend on the caller enabling GL_LIGHTING.
        glue->glEnable(GL_LIGHTING);
        glue->glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
    }
    glue->glDisable(GL_COLOR_MATERIAL);
    glue->glEnableClientState(GL_VERTEX_ARRAY);
    for (const auto& item : plan.shadedItems) {
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
            const auto& inst = plan.visibleInstances[item.baseInstance + i];
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
                    inst.lodLevel, progressive->progressiveMinimumLevel,
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
        const SbMatrix&      projectionMatrix)
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
    for (const auto& item : plan.wireItems) {
        const Obol::PartGeometry* geom = assembly.partGeometry(item.rep.part);
        if (!geom || !geom->wire.has_value()) continue;
        const Obol::WireRep& wire = *geom->wire;

        for (uint32_t ii = 0; ii < item.instanceCount; ++ii) {
            const size_t visibleIndex = item.baseInstance + ii;
            if (isSubpixelProxyInstance(plan, visibleIndex)) continue;
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
                wire.segmentCountAtLevel(inst.lodLevel) * 2;
            const uint8_t drawLevel = progressiveLevel(
                inst.lodLevel, wire.progressiveMinimumLevel,
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
    if (!plan.shadedItems.empty()) {
        // Shaded CAD geometry always uses its normals, regardless of the
        // lighting state inherited from the surrounding scene graph.
        glue->glEnable(GL_LIGHTING);
        glue->glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
    }
    glue->glDisable(GL_COLOR_MATERIAL);
    for (const auto& item : plan.shadedItems) {
        const Obol::PartGeometry* geom = assembly.partGeometry(item.rep.part);
        if (!geom || !geom->shaded.has_value()) continue;
        const Obol::TriMesh& mesh = *geom->shaded;

        setCadBackfaceCulling(glue, item.cullBackfaces);
        const bool hasNorm = !mesh.normals.empty();

        for (uint32_t ii = 0; ii < item.instanceCount; ++ii) {
            const auto& inst = plan.visibleInstances[item.baseInstance + ii];
            if (isBoxOutsideFrustum(inst.wbMin, inst.wbMax, fp)) continue;

            SbMatrix model;
            model.setValue(inst.transform.data());
            SbMatrix modelView = model;
            modelView.multRight(viewMatrix);
            glue->glLoadMatrixf(modelView[0]);

            setImmediateMaterialFromRgba(glue, inst.rgba.data());

            const std::vector<uint32_t>& drawIdx = mesh.indices;
            const size_t drawIndexCount =
                mesh.indexCountAtLevel(inst.lodLevel);
            const uint8_t drawLevel = progressiveLevel(
                inst.lodLevel, mesh.progressiveMinimumLevel,
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

void CadRendererGL::renderInstanced(
        const CadFramePlan& plan,
        const SoCADAssembly& /*assembly*/,
        const SoGLContext*   glue,
        const SbMatrix&      viewProj,
        const std::unordered_map<PartId, uint64_t,
                                 std::hash<PartId>>& /*partGenMap*/,
        bool solidWireOnly,
        bool drawShaded)
{
    // Build per-instance vertex data (transform + colour)
    const size_t nInst = plan.visibleInstances.size();
    if (nInst == 0) return;

    std::vector<InstVertex> instData(nInst);
    for (size_t i = 0; i < nInst; ++i) {
        const auto& vi = plan.visibleInstances[i];
        std::memcpy(instData[i].transform, vi.transform.data(), 16 * sizeof(float));
        instData[i].color[0] = vi.rgba[0] / 255.0f;
        instData[i].color[1] = vi.rgba[1] / 255.0f;
        instData[i].color[2] = vi.rgba[2] / 255.0f;
        instData[i].color[3] = vi.rgba[3] / 255.0f;
    }

    gpuRes_->uploadInstanceData(instData.data(),
                                static_cast<GLsizeiptr>(nInst * sizeof(InstVertex)),
                                glue);

    const GLuint instVbo = gpuRes_->instanceVbo();
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
    if (!plan.wireItems.empty()) {
        const CadWireRasterState rasterState = captureWireRasterState(
            glue, caps_.hasLineStipple);
        glue->glUseProgramObjectARB(shaders_.wireInst);

        GLint locVP  = glue->glGetUniformLocationARB(shaders_.wireInst, "u_viewProj");
        GLint locPos = glue->glGetAttribLocationARB(shaders_.wireInst,  "a_pos");
        if (locPos < 0) locPos = 0;

        glue->glUniformMatrix4fvARB(locVP, 1, GL_FALSE, vp);

        for (const auto& item : plan.wireItems) {
            if (solidWireOnly && item.customWireStyle) continue;
            CadWireGpu* w = gpuRes_->wireFor(item.rep.part);
            if (!w || w->segCount == 0) continue;
            uint32_t runStart = 0;
            while (runStart < item.instanceCount) {
                while (runStart < item.instanceCount &&
                    isSubpixelProxyInstance(plan, item.baseInstance + runStart))
                    ++runStart;
                if (runStart == item.instanceCount)
                    break;
                uint32_t runEnd = runStart + 1;
                while (runEnd < item.instanceCount &&
                    !isSubpixelProxyInstance(plan, item.baseInstance + runEnd))
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
                    glue->glVertexAttribPointerARB(static_cast<GLuint>(locPos), 3,
                                                   GL_FLOAT, GL_FALSE,
                                                   3 * sizeof(float), nullptr);
                    glue->glEnableVertexAttribArrayARB(static_cast<GLuint>(locPos));
                    if (!w->sequentialSegments)
                        glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, w->segIdxBuf);
                    bindInstAttribs(baseInstance);
                }

                const GLsizei runCount = static_cast<GLsizei>(runEnd - runStart);
                if (w->sequentialSegments) {
                    glue->glDrawArraysInstanced(GL_LINES, 0, w->segCount * 2,
                                                runCount);
                } else {
                    glue->glDrawElementsInstanced(GL_LINES, w->segCount * 2,
                                                  GL_UNSIGNED_INT, nullptr,
                                                  runCount);
                }

                if (w->vao && glue->glBindVertexArray) {
                    glue->glBindVertexArray(0);
                } else {
                    unbindInstAttribs();
                    glue->glDisableVertexAttribArrayARB(static_cast<GLuint>(locPos));
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
        glue->glUseProgramObjectARB(shaders_.shadedInst);

        GLint locVP      = glue->glGetUniformLocationARB(shaders_.shadedInst, "u_viewProj");
        GLint locHasNorm = glue->glGetUniformLocationARB(shaders_.shadedInst, "u_hasNorm");
        GLint locPos     = glue->glGetAttribLocationARB(shaders_.shadedInst, "a_pos");
        GLint locNorm    = glue->glGetAttribLocationARB(shaders_.shadedInst, "a_norm");
        if (locPos < 0) locPos = 0;

        glue->glUniformMatrix4fvARB(locVP, 1, GL_FALSE, vp);
        this->uploadLights(glue, shaders_.shadedInst);

        for (const auto& item : plan.shadedItems) {
            CadTriGpu* t = gpuRes_->triFor(item.rep.part);
            if (!t || t->idxCount == 0) continue;

            setCadBackfaceCulling(glue, item.cullBackfaces);
            glue->glUniform1iARB(locHasNorm, (t->normBuf != 0) ? 1 : 0);

            if (t->vao && glue->glBindVertexArray) {
                glue->glBindVertexArray(t->vao);
                if (t->instanceVbo != instVbo ||
                        t->instanceBase != item.baseInstance) {
                    bindInstAttribs(item.baseInstance);
                    t->instanceVbo = instVbo;
                    t->instanceBase = item.baseInstance;
                }
            } else {
                glue->glBindBuffer(GL_ARRAY_BUFFER, t->posBuf);
                glue->glVertexAttribPointerARB(static_cast<GLuint>(locPos), 3,
                                               GL_FLOAT, GL_FALSE,
                                               3 * sizeof(float), nullptr);
                glue->glEnableVertexAttribArrayARB(static_cast<GLuint>(locPos));
                if (t->normBuf && locNorm >= 0) {
                    glue->glBindBuffer(GL_ARRAY_BUFFER, t->normBuf);
                    glue->glVertexAttribPointerARB(static_cast<GLuint>(locNorm), 3,
                                                   GL_FLOAT, GL_FALSE,
                                                   3 * sizeof(float), nullptr);
                    glue->glEnableVertexAttribArrayARB(static_cast<GLuint>(locNorm));
                }
                glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, t->idxBuf);
                bindInstAttribs(item.baseInstance);
            }

            glue->glDrawElementsInstanced(GL_TRIANGLES,
                                          t->idxCount,
                                          GL_UNSIGNED_INT,
                                          nullptr,
                                          static_cast<GLsizei>(item.instanceCount));

            if (t->vao && glue->glBindVertexArray) {
                glue->glBindVertexArray(0);
            } else {
                unbindInstAttribs();
                if (t->normBuf && locNorm >= 0) {
                    glue->glDisableVertexAttribArrayARB(static_cast<GLuint>(locNorm));
                }
                glue->glDisableVertexAttribArrayARB(static_cast<GLuint>(locPos));
                glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
                glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            }
        }

        glue->glUseProgramObjectARB(0);
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
