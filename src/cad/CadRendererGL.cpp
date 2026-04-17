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

#include <obol/cad/SoCADAssembly.h>

#include <Inventor/system/gl.h>
#include <Inventor/SbVec3f.h>
#include "glue/glp.h"

#include <cstring>
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>
#include <cassert>

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
    "    gl_Position = u_viewProj * u_model * vec4(a_pos, 1.0);\n"
    "    v_color = u_color;\n"
    "}\n";

static const char * kWireFS1 =
    "varying vec4 v_color;\n"
    "void main() {\n"
    "    gl_FragColor = v_color;\n"
    "}\n";

// Shaded pass: simple directional light in world space
static const char * kShadedVS1 =
    "attribute vec3 a_pos;\n"
    "attribute vec3 a_norm;\n"
    "uniform mat4  u_model;\n"
    "uniform mat4  u_viewProj;\n"
    "uniform vec4  u_color;\n"
    "uniform int   u_hasNorm;\n"
    "varying vec3  v_norm;\n"
    "varying vec4  v_color;\n"
    "void main() {\n"
    "    gl_Position = u_viewProj * u_model * vec4(a_pos, 1.0);\n"
    "    if (u_hasNorm != 0) {\n"
    "        v_norm = mat3(u_model[0].xyz, u_model[1].xyz, u_model[2].xyz) * a_norm;\n"
    "    } else {\n"
    "        v_norm = vec3(0.0, 0.0, 1.0);\n"
    "    }\n"
    "    v_color = u_color;\n"
    "}\n";

static const char * kShadedFS1 =
    "uniform vec3 u_lightDir;\n"
    "varying vec3 v_norm;\n"
    "varying vec4 v_color;\n"
    "void main() {\n"
    "    vec3 n = normalize(v_norm);\n"
    "    float nDotL = max(0.0, dot(n, u_lightDir));\n"
    "    vec3 ambient = v_color.rgb * 0.25;\n"
    "    vec3 diffuse = v_color.rgb * nDotL * 0.75;\n"
    "    gl_FragColor = vec4(ambient + diffuse, v_color.a);\n"
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
    "out vec4 v_color;\n"
    "void main() {\n"
    "    gl_Position = u_viewProj * a_instTransform * vec4(a_pos, 1.0);\n"
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
    "uniform vec3 u_lightDir;\n"
    "in  vec3 v_norm;\n"
    "in  vec4 v_color;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "    vec3 n = normalize(v_norm);\n"
    "    float nDotL = max(0.0, dot(n, u_lightDir));\n"
    "    vec3 ambient = v_color.rgb * 0.25;\n"
    "    vec3 diffuse = v_color.rgb * nDotL * 0.75;\n"
    "    fragColor = vec4(ambient + diffuse, v_color.a);\n"
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

namespace obol {
namespace internal {

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

CadRendererGL::CadRendererGL()  = default;
CadRendererGL::~CadRendererGL() = default;

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
        GLuint vs = compileShader(glue, GL_VERTEX_SHADER_ARB, kShadedVS1);
        GLuint fs = compileShader(glue, GL_FRAGMENT_SHADER_ARB, kShadedFS1);
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

void CadRendererGL::deleteShaders(const SoGLContext* glue)
{
    if (!glue) return;
    if (shaders_.wire)      { glue->glDeleteObjectARB(shaders_.wire);      shaders_.wire      = 0; }
    if (shaders_.shaded)    { glue->glDeleteObjectARB(shaders_.shaded);    shaders_.shaded    = 0; }
    if (shaders_.wireInst)  { glue->glDeleteObjectARB(shaders_.wireInst);  shaders_.wireInst  = 0; }
    if (shaders_.shadedInst){ glue->glDeleteObjectARB(shaders_.shadedInst);shaders_.shadedInst= 0; }
}

// ---------------------------------------------------------------------------
// ensureReady()
// ---------------------------------------------------------------------------

bool CadRendererGL::ensureReady(const SoGLContext* glue)
{
    if (!glue) return false;

    if (!capsDetected_) {
        caps_ = CadGLCaps::detect(glue);
        capsDetected_ = true;
    }
    // Always compile shaders when shader objects are available; they are used
    // by renderVboLoop / renderInstanced.  If hasGLSLDraw is false the render()
    // dispatch falls through to renderImmediateMode instead.
    if (caps_.hasShaderObjects &&
            (shaders_.wire == 0 || shadersContextId_ != glue->contextid)) {
        deleteShaders(glue);
        shadersContextId_ = glue->contextid;
        compileAllShaders(glue);
    }
    return true;
}

// ---------------------------------------------------------------------------
// ensurePartUploaded()
// ---------------------------------------------------------------------------

void CadRendererGL::ensurePartUploaded(PartId pid, const SoCADAssembly& assembly,
                                       uint64_t gen, const SoGLContext* glue)
{
    // Fast path: GPU data is already current — skip expensive array building.
    if (gpuRes_.isUpToDate(pid, gen)) return;

    // Retrieve part geometry from the assembly
    const obol::PartGeometry* geom = assembly.partGeometry(pid);
    if (!geom) return;

    // Build CPU-side flat arrays for wire geometry
    std::vector<float>    wirePos;
    std::vector<uint32_t> wireSegIdx;
    if (geom->wire.has_value()) {
        const auto& wr = *geom->wire;
        for (const auto& poly : wr.polylines) {
            if (poly.points.size() < 2) continue;
            const uint32_t base = static_cast<uint32_t>(wirePos.size() / 3);
            for (const auto& pt : poly.points) {
                wirePos.push_back(pt[0]);
                wirePos.push_back(pt[1]);
                wirePos.push_back(pt[2]);
            }
            // Emit a segment (pair of indices) for each consecutive pair
            for (uint32_t i = 0; i + 1 < poly.points.size(); ++i) {
                wireSegIdx.push_back(base + i);
                wireSegIdx.push_back(base + i + 1);
            }
        }
    }

    // Flatten triangle geometry
    std::vector<float>    triPos;
    std::vector<float>    triNorm;
    std::vector<uint32_t> triIdx;
    if (geom->shaded.has_value()) {
        const auto& mesh = *geom->shaded;
        for (const auto& p : mesh.positions) {
            triPos.push_back(p[0]);
            triPos.push_back(p[1]);
            triPos.push_back(p[2]);
        }
        if (!mesh.normals.empty()) {
            for (const auto& n : mesh.normals) {
                triNorm.push_back(n[0]);
                triNorm.push_back(n[1]);
                triNorm.push_back(n[2]);
            }
        }
        triIdx = mesh.indices;
    }

    const float*    pWirePos  = wirePos.empty()    ? nullptr : wirePos.data();
    const uint32_t* pWireSeg  = wireSegIdx.empty() ? nullptr : wireSegIdx.data();
    const float*    pTriPos   = triPos.empty()     ? nullptr : triPos.data();
    const float*    pTriNorm  = triNorm.empty()    ? nullptr : triNorm.data();
    const uint32_t* pTriIdx   = triIdx.empty()     ? nullptr : triIdx.data();

    gpuRes_.upload(pid,
                   pWirePos,  static_cast<GLsizei>(wirePos.size() / 3),
                   pWireSeg,  static_cast<GLsizei>(wireSegIdx.size()),
                   pTriPos,   static_cast<GLsizei>(triPos.size() / 3),
                   pTriNorm,
                   pTriIdx,   static_cast<GLsizei>(triIdx.size()),
                   gen,
                   glue, caps_);
}

// ---------------------------------------------------------------------------
// getLodCachedIndices() – CPU LoD index cache
// ---------------------------------------------------------------------------

const std::vector<uint32_t>*
CadRendererGL::getLodCachedIndices(PartId pid, uint8_t level, uint64_t gen,
                                   const SoCADAssembly& assembly)
{
    auto& entry = lodCache_[pid];
    if (entry.generation != gen) {
        entry.generation = gen;
        entry.byLevel.clear();
    }
    auto it = entry.byLevel.find(level);
    if (it != entry.byLevel.end()) return &it->second;

    // Delegate to the assembly's LoD-index cache (which holds the filtered
    // index list for the given (pid, level) pair).
    const std::vector<uint32_t>* src = assembly.getLodFilteredIndices(pid, level);
    if (!src) return nullptr;

    entry.byLevel[level] = *src;
    return &entry.byLevel[level];
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
        // Negative vertex: corner most opposed to the plane normal.
        float nx = (fp.planes[i][0] < 0.0f) ? wbMax[0] : wbMin[0];
        float ny = (fp.planes[i][1] < 0.0f) ? wbMax[1] : wbMin[1];
        float nz = (fp.planes[i][2] < 0.0f) ? wbMax[2] : wbMin[2];
        if (fp.planes[i][0]*nx + fp.planes[i][1]*ny + fp.planes[i][2]*nz + fp.planes[i][3] < 0.0f)
            return true; // Completely outside this plane.
    }
    return false;
}

// Compute a POP LoD level based on camera-to-object distance and object radius.
// Returns 255 (full detail) when very close; decreases as the object shrinks.
static uint8_t computeLodLevel(float dist, float radius) noexcept
{
    if (radius < 1e-6f) return 255;
    float ratio = dist / radius; // 0 = touching; large = tiny/far
    // Mapping: ratio ≈ 0 → 255; ratio = 2 → ~170; ratio = 50 → ~10
    float level = 255.0f / (1.0f + ratio * 0.5f);
    return static_cast<uint8_t>(std::min(255.0f, std::max(0.0f, level)));
}

void CadRendererGL::render(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const SoGLContext*   glue,
        const SbMatrix&      viewProj,
        const SbVec3f&       cameraPos,
        bool                 lodEnabled,
        const std::unordered_map<PartId, uint64_t,
                                 std::hash<PartId>>& partGenMap)
{
    if (!ensureReady(glue)) return;
    if (plan.visibleInstances.empty()) return;

    // Upload geometry for all required part reps
    for (const auto& repKey : plan.requiredReps) {
        auto genIt = partGenMap.find(repKey.part);
        uint64_t gen = (genIt != partGenMap.end()) ? genIt->second : 0;
        ensurePartUploaded(repKey.part, assembly, gen, glue);
    }

    if (caps_.canUseInstanced() && shaders_.wireInst && shaders_.shadedInst) {
        lastRenderTier_ = 2;
        renderInstanced(plan, assembly, glue, viewProj, partGenMap);
    } else if (caps_.canUseVbo() && shaders_.wire) {
        lastRenderTier_ = 1;
        renderVboLoop(plan, assembly, glue, viewProj, cameraPos, lodEnabled, partGenMap);
    } else {
        lastRenderTier_ = 0;
        renderImmediateMode(plan, assembly, glue, viewProj, cameraPos, lodEnabled, partGenMap);
    }
}

// ---------------------------------------------------------------------------
// Tier-1: VBO-loop rendering (GL 2.0+)
// ---------------------------------------------------------------------------

// Bind a wire VBO, set up attribute 0 (position), draw segments.
// Works with or without VAO.
static void bindAndDrawWire(const CadWireGpu* w, const SoGLContext* glue,
                             GLint locPos)
{
    if (!w || w->segCount == 0) return;

    if (w->vao && glue->glBindVertexArray) {
        glue->glBindVertexArray(w->vao);
    } else {
        glue->glBindBuffer(GL_ARRAY_BUFFER, w->posBuf);
        glue->glVertexAttribPointerARB(static_cast<GLuint>(locPos), 3,
                                       GL_FLOAT, GL_FALSE,
                                       3 * sizeof(float), nullptr);
        glue->glEnableVertexAttribArrayARB(static_cast<GLuint>(locPos));
        glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, w->segIdxBuf);
    }

    glue->glDrawElements(GL_LINES,
                         w->segCount * 2,
                         GL_UNSIGNED_INT,
                         nullptr);
    {
        GLenum err = glGetError();
        if (err != GL_NO_ERROR)
            std::fprintf(stderr, "CadRendererGL: glDrawElements error: 0x%x\n", err);
    }

    if (w->vao && glue->glBindVertexArray) {
        glue->glBindVertexArray(0);
    } else {
        glue->glDisableVertexAttribArrayARB(static_cast<GLuint>(locPos));
        glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
        glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
}

// Bind a tri VBO, set up attributes 0 (position) and 1 (normal), draw.
static void bindAndDrawTri(const CadTriGpu* t, const SoGLContext* glue,
                            GLint locPos, GLint locNorm, bool& hasNorm)
{
    if (!t || t->idxCount == 0) return;

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

    glue->glDrawElements(GL_TRIANGLES, t->idxCount, GL_UNSIGNED_INT, nullptr);

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
        const SoCADAssembly& /*assembly*/,
        const SoGLContext*   glue,
        const SbMatrix&      viewProj,
        const SbVec3f&       /*cameraPos*/,
        bool                 /*lodEnabled*/,
        const std::unordered_map<PartId, uint64_t,
                                 std::hash<PartId>>& /*partGenMap*/)
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
        glue->glUseProgramObjectARB(shaders_.wire);

        GLint locVP    = glue->glGetUniformLocationARB(shaders_.wire, "u_viewProj");
        GLint locModel = glue->glGetUniformLocationARB(shaders_.wire, "u_model");
        GLint locColor = glue->glGetUniformLocationARB(shaders_.wire, "u_color");

        glue->glUniformMatrix4fvARB(locVP, 1, GL_FALSE, vpData);

        for (const auto& item : plan.wireItems) {
            const CadWireGpu* w = gpuRes_.wireFor(item.rep.part);
            if (!w) continue;

            for (uint32_t i = 0; i < item.instanceCount; ++i) {
                const auto& inst = plan.visibleInstances[item.baseInstance + i];
                if (isBoxOutsideFrustum(inst.wbMin, inst.wbMax, fp)) continue;

                glue->glUniformMatrix4fvARB(locModel, 1, GL_FALSE,
                                            inst.transform.data());
                float rgba[4] = {
                    inst.rgba[0] / 255.0f, inst.rgba[1] / 255.0f,
                    inst.rgba[2] / 255.0f, inst.rgba[3] / 255.0f
                };
                glue->glUniform4fvARB(locColor, 1, rgba);
                bindAndDrawWire(w, glue, locPos);
            }
        }

        glue->glUseProgramObjectARB(0);
    }

    // --- Shaded pass ---
    if (!plan.shadedItems.empty()) {
        glue->glUseProgramObjectARB(shaders_.shaded);

        GLint locVP      = glue->glGetUniformLocationARB(shaders_.shaded, "u_viewProj");
        GLint locModel   = glue->glGetUniformLocationARB(shaders_.shaded, "u_model");
        GLint locColor   = glue->glGetUniformLocationARB(shaders_.shaded, "u_color");
        GLint locLight   = glue->glGetUniformLocationARB(shaders_.shaded, "u_lightDir");
        GLint locHasNorm = glue->glGetUniformLocationARB(shaders_.shaded, "u_hasNorm");

        glue->glUniformMatrix4fvARB(locVP, 1, GL_FALSE, vpData);
        glue->glUniform3fvARB(locLight, 1, kLightDir);

        for (const auto& item : plan.shadedItems) {
            const CadTriGpu* t = gpuRes_.triFor(item.rep.part);
            if (!t) continue;

            bool hasNorm = (t->normBuf != 0);
            glue->glUniform1iARB(locHasNorm, hasNorm ? 1 : 0);

            for (uint32_t i = 0; i < item.instanceCount; ++i) {
                const auto& inst = plan.visibleInstances[item.baseInstance + i];
                if (isBoxOutsideFrustum(inst.wbMin, inst.wbMax, fp)) continue;

                glue->glUniformMatrix4fvARB(locModel, 1, GL_FALSE,
                                            inst.transform.data());
                float rgba[4] = {
                    inst.rgba[0] / 255.0f, inst.rgba[1] / 255.0f,
                    inst.rgba[2] / 255.0f, inst.rgba[3] / 255.0f
                };
                glue->glUniform4fvARB(locColor, 1, rgba);

                bindAndDrawTri(t, glue, locPos, locNorm, hasNorm);
            }
        }

        glue->glUseProgramObjectARB(0);
    }
}

// ---------------------------------------------------------------------------
// Tier-0: immediate-mode fallback (GL 1.1, Mesa 7.x swrast)
// ---------------------------------------------------------------------------

void CadRendererGL::renderImmediateMode(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const SoGLContext*   /*glue*/,
        const SbMatrix&      viewProj,
        const SbVec3f&       cameraPos,
        bool                 lodEnabled,
        const std::unordered_map<PartId, uint64_t,
                                 std::hash<PartId>>& partGenMap)
{
    // Push GL matrix state, set up the view-projection matrix in MODELVIEW
    // (the fixed-function pipeline applies MODELVIEW * PROJECTION to each vertex).
    // We fold the per-instance transform into MODELVIEW = viewProj * model.
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();  // Identity projection – we bake VP into MODELVIEW

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    // Disable lighting so glColor4f controls the final colour
    GLboolean wasLighting = glIsEnabled(GL_LIGHTING);
    glDisable(GL_LIGHTING);

    // Extract frustum planes for per-instance culling.
    const FrustumPlanes fp = extractFrustumPlanes(viewProj);

    // --- Wire pass ---
    for (const auto& item : plan.wireItems) {
        const obol::PartGeometry* geom = assembly.partGeometry(item.rep.part);
        if (!geom || !geom->wire.has_value()) continue;
        const obol::WireRep& wire = *geom->wire;

        for (uint32_t ii = 0; ii < item.instanceCount; ++ii) {
            const auto& inst = plan.visibleInstances[item.baseInstance + ii];
            if (isBoxOutsideFrustum(inst.wbMin, inst.wbMax, fp)) continue;

            // Build the combined VP * M matrix in OI convention,
            // then load it as the GL MODELVIEW.
            // OI row-major → glLoadMatrixf reads as column-major → gives transpose
            // which is exactly what the fixed-function pipeline needs.
            SbMatrix model;
            // inst.transform is a flat float[16] copy of OI localToRoot
            model.setValue(inst.transform.data());
            SbMatrix mvp = model;
            mvp.multRight(viewProj);
            glLoadMatrixf(mvp[0]); // OI[0] = start of float[4][4]; GL reads column-major

            glColor4ub(inst.rgba[0], inst.rgba[1], inst.rgba[2], inst.rgba[3]);

            for (const auto& poly : wire.polylines) {
                if (poly.points.size() < 2) continue;
                glBegin(GL_LINE_STRIP);
                for (const auto& pt : poly.points) {
                    glVertex3f(pt[0], pt[1], pt[2]);
                }
                glEnd();
            }
        }
    }

    // --- Shaded pass ---
    if (wasLighting) glEnable(GL_LIGHTING);
    // Simple Phong-like: enable lighting for shaded items if lighting was on,
    // else just draw with flat colour.
    for (const auto& item : plan.shadedItems) {
        const obol::PartGeometry* geom = assembly.partGeometry(item.rep.part);
        if (!geom || !geom->shaded.has_value()) continue;
        const obol::TriMesh& mesh = *geom->shaded;

        const bool hasNorm = !mesh.normals.empty();

        // Compute LoD-filtered indices for this part if enabled.
        // All instances share the same part, so determine a representative
        // LoD level by using the first visible instance's world-bounds centre.
        const std::vector<uint32_t>* lodIdx = nullptr;
        if (lodEnabled && item.instanceCount > 0) {
            const auto& rep = plan.visibleInstances[item.baseInstance];
            float cx = (rep.wbMin[0] + rep.wbMax[0]) * 0.5f;
            float cy = (rep.wbMin[1] + rep.wbMax[1]) * 0.5f;
            float cz = (rep.wbMin[2] + rep.wbMax[2]) * 0.5f;
            float dx = rep.wbMax[0] - rep.wbMin[0];
            float dy = rep.wbMax[1] - rep.wbMin[1];
            float dz = rep.wbMax[2] - rep.wbMin[2];
            float radius = std::sqrt(dx*dx + dy*dy + dz*dz) * 0.5f;
            float distX = cx - cameraPos[0];
            float distY = cy - cameraPos[1];
            float distZ = cz - cameraPos[2];
            float dist  = std::sqrt(distX*distX + distY*distY + distZ*distZ);
            uint8_t level = computeLodLevel(dist, radius);
            if (level < 255) {
                auto genIt = partGenMap.find(item.rep.part);
                uint64_t gen = (genIt != partGenMap.end()) ? genIt->second : 0;
                lodIdx = getLodCachedIndices(item.rep.part, level, gen, assembly);
            }
        }

        const std::vector<uint32_t>& drawIdx = lodIdx ? *lodIdx : mesh.indices;

        for (uint32_t ii = 0; ii < item.instanceCount; ++ii) {
            const auto& inst = plan.visibleInstances[item.baseInstance + ii];
            if (isBoxOutsideFrustum(inst.wbMin, inst.wbMax, fp)) continue;

            SbMatrix model;
            model.setValue(inst.transform.data());
            SbMatrix mvp = model;
            mvp.multRight(viewProj);
            glLoadMatrixf(mvp[0]);

            glColor4ub(inst.rgba[0], inst.rgba[1], inst.rgba[2], inst.rgba[3]);

            glBegin(GL_TRIANGLES);
            for (size_t t = 0; t + 2 < drawIdx.size(); t += 3) {
                for (int k = 0; k < 3; ++k) {
                    uint32_t idx = drawIdx[t + k];
                    if (hasNorm && idx < mesh.normals.size()) {
                        const auto& n = mesh.normals[idx];
                        glNormal3f(n[0], n[1], n[2]);
                    }
                    const auto& p = mesh.positions[idx];
                    glVertex3f(p[0], p[1], p[2]);
                }
            }
            glEnd();
        }
    }

    // Restore matrix state
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
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
                                 std::hash<PartId>>& /*partGenMap*/)
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

    gpuRes_.uploadInstanceData(instData.data(),
                                static_cast<GLsizeiptr>(nInst * sizeof(InstVertex)),
                                glue);

    const GLuint instVbo = gpuRes_.instanceVbo();
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
        glue->glUseProgramObjectARB(shaders_.wireInst);

        GLint locVP  = glue->glGetUniformLocationARB(shaders_.wireInst, "u_viewProj");
        GLint locPos = glue->glGetAttribLocationARB(shaders_.wireInst,  "a_pos");
        if (locPos < 0) locPos = 0;

        glue->glUniformMatrix4fvARB(locVP, 1, GL_FALSE, vp);

        for (const auto& item : plan.wireItems) {
            const CadWireGpu* w = gpuRes_.wireFor(item.rep.part);
            if (!w || w->segCount == 0) continue;

            if (w->vao && glue->glBindVertexArray) {
                // Bind the part VAO first, then set up the instanced attribs
                // inside that VAO so they are active for the draw call.
                glue->glBindVertexArray(w->vao);
                bindInstAttribs(item.baseInstance);
            } else {
                glue->glBindBuffer(GL_ARRAY_BUFFER, w->posBuf);
                glue->glVertexAttribPointerARB(static_cast<GLuint>(locPos), 3,
                                               GL_FLOAT, GL_FALSE,
                                               3 * sizeof(float), nullptr);
                glue->glEnableVertexAttribArrayARB(static_cast<GLuint>(locPos));
                glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, w->segIdxBuf);
                bindInstAttribs(item.baseInstance);
            }

            glue->glDrawElementsInstanced(GL_LINES,
                                          w->segCount * 2,
                                          GL_UNSIGNED_INT,
                                          nullptr,
                                          static_cast<GLsizei>(item.instanceCount));

            if (w->vao && glue->glBindVertexArray) {
                unbindInstAttribs();
                glue->glBindVertexArray(0);
            } else {
                unbindInstAttribs();
                glue->glDisableVertexAttribArrayARB(static_cast<GLuint>(locPos));
                glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
                glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            }
        }

        glue->glUseProgramObjectARB(0);
    }

    // --- Shaded pass ---
    if (!plan.shadedItems.empty()) {
        glue->glUseProgramObjectARB(shaders_.shadedInst);

        GLint locVP      = glue->glGetUniformLocationARB(shaders_.shadedInst, "u_viewProj");
        GLint locLight   = glue->glGetUniformLocationARB(shaders_.shadedInst, "u_lightDir");
        GLint locHasNorm = glue->glGetUniformLocationARB(shaders_.shadedInst, "u_hasNorm");
        GLint locPos     = glue->glGetAttribLocationARB(shaders_.shadedInst, "a_pos");
        GLint locNorm    = glue->glGetAttribLocationARB(shaders_.shadedInst, "a_norm");
        if (locPos < 0) locPos = 0;

        glue->glUniformMatrix4fvARB(locVP, 1, GL_FALSE, vp);
        glue->glUniform3fvARB(locLight, 1, kLightDir);

        for (const auto& item : plan.shadedItems) {
            const CadTriGpu* t = gpuRes_.triFor(item.rep.part);
            if (!t || t->idxCount == 0) continue;

            glue->glUniform1iARB(locHasNorm, (t->normBuf != 0) ? 1 : 0);

            if (t->vao && glue->glBindVertexArray) {
                glue->glBindVertexArray(t->vao);
                bindInstAttribs(item.baseInstance);
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
                unbindInstAttribs();
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
    deleteShaders(glue);
    gpuRes_.releaseAll(glue);
    capsDetected_ = false;
    shadersContextId_ = 0;
}

} // namespace internal
} // namespace obol
