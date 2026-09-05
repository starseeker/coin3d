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
#include "CadIdentityCounter.h"
#include "CadWireSource.h"
#include "CadRendererConfiguration.h"
#include "CadResolvedDraw.h"
#include "CadShaderSources.h"

#include <Obol/cad/SoCADAssembly.h>

#include <Inventor/misc/SoContextHandler.h>
#include <Inventor/elements/SoGLCacheContextElement.h>
#include <Inventor/system/gl.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include "glue/glp.h"

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>
#include <atomic>
#include <cassert>
#include <mutex>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <chrono>
#include <string>

#ifndef GL_DRAW_INDIRECT_BUFFER
#define GL_DRAW_INDIRECT_BUFFER 0x8F3F
#endif
#ifndef GL_DRAW_INDIRECT_BUFFER_BINDING
#define GL_DRAW_INDIRECT_BUFFER_BINDING 0x8F43
#endif

static_assert(sizeof(SbVec3f) == 3 * sizeof(float),
              "SbVec3f must remain tightly packed for CAD GPU uploads");

/* The world-space flat-wire executor is a useful OSMesa acceleration for
 * moderate occurrence populations.  Above this threshold, its temporary
 * atlas competes with the retained scene under the software renderer's
 * address-space budget; use retained ranges directly. */
static const size_t maximumSoftwareFlatWireInstances = 16384u;

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

static uint64_t
cadSaturatingWorkAdd(uint64_t left, uint64_t right)
{
    return right > UINT64_MAX - left ? UINT64_MAX : left + right;
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
                          bool indirect, bool compatibilityStack)
        : glue_(context), hasVbo_(vbo), hasVao_(vao),
          hasIndirect_(indirect)
    {
        if (compatibilityStack) {
            valid_ = false;
        }
        if (compatibilityStack && glue_->glPushAttrib &&
                glue_->glPopAttrib && glue_->glPushClientAttrib &&
                glue_->glPopClientAttrib) {
            GLint stackDepth = 0;
            GLint pushedDepth = 0;
            glue_->glGetIntegerv(GL_ATTRIB_STACK_DEPTH, &stackDepth);
            glue_->glPushAttrib(
                GL_CURRENT_BIT | GL_ENABLE_BIT | GL_LIGHTING_BIT |
                GL_LINE_BIT | GL_POINT_BIT | GL_POLYGON_BIT |
                GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
                GL_TRANSFORM_BIT);
            glue_->glGetIntegerv(GL_ATTRIB_STACK_DEPTH, &pushedDepth);
            serverCompatibilityStack_ = pushedDepth == stackDepth + 1;

            if (serverCompatibilityStack_) {
                glue_->glGetIntegerv(
                    GL_CLIENT_ATTRIB_STACK_DEPTH, &stackDepth);
                glue_->glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
                glue_->glGetIntegerv(
                    GL_CLIENT_ATTRIB_STACK_DEPTH, &pushedDepth);
                clientCompatibilityStack_ = pushedDepth == stackDepth + 1;
            }
            valid_ = serverCompatibilityStack_ &&
                clientCompatibilityStack_;
        }
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
        if (clientCompatibilityStack_)
            glue_->glPopClientAttrib();
        if (serverCompatibilityStack_)
            glue_->glPopAttrib();
    }

    bool valid() const { return valid_; }

private:
    const SoGLContext *glue_;
    bool hasProgram_ = false;
    bool hasVbo_ = false;
    bool hasVao_ = false;
    bool hasIndirect_ = false;
    bool serverCompatibilityStack_ = false;
    bool clientCompatibilityStack_ = false;
    bool valid_ = true;
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

/*
 * Deep state validation is opt-in because glGet* may serialize a hardware
 * driver.  It is intended for renderer-route conformance tests and graphical
 * diagnostics, never authoritative FPS measurements.  The ordinary guard
 * above performs the restoration; this guard proves that all early returns
 * and specialized executors honored the boundary.
 */
class CadGLStateValidationGuard {
    struct Snapshot {
        Snapshot() = default;

        Snapshot(const SoGLContext *context, bool isCompatibility, bool vbo,
                 bool vao, bool indirect)
            : valid(context != nullptr), compatibility(isCompatibility),
              hasVbo(vbo), hasVao(vao), hasIndirect(indirect)
        {
            if (!valid)
                return;
            glue = context;
#ifdef GL_CURRENT_PROGRAM
            if (glue->glUseProgramObjectARB)
                glue->glGetIntegerv(GL_CURRENT_PROGRAM, &program);
#endif
#ifdef GL_VERTEX_ARRAY_BINDING
            if (hasVao)
                glue->glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vertexArray);
#endif
#if defined(GL_ARRAY_BUFFER_BINDING) && defined(GL_ELEMENT_ARRAY_BUFFER_BINDING)
            if (hasVbo) {
                glue->glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer);
                glue->glGetIntegerv(
                    GL_ELEMENT_ARRAY_BUFFER_BINDING, &elementBuffer);
            }
#endif
            if (hasIndirect)
                glue->glGetIntegerv(
                    GL_DRAW_INDIRECT_BUFFER_BINDING, &indirectBuffer);
            glue->glGetIntegerv(GL_MATRIX_MODE, &matrixMode);
            glue->glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
            polygonOffset = glue->glIsEnabled(GL_POLYGON_OFFSET_FILL);
            glue->glGetFloatv(
                GL_POLYGON_OFFSET_FACTOR, &polygonOffsetFactor);
            glue->glGetFloatv(
                GL_POLYGON_OFFSET_UNITS, &polygonOffsetUnits);
            cull = glue->glIsEnabled(GL_CULL_FACE);
            glue->glGetIntegerv(GL_FRONT_FACE, &frontFace);
            glue->glGetIntegerv(GL_CULL_FACE_MODE, &cullFace);
            glue->glGetFloatv(GL_LINE_WIDTH, &lineWidth);
            glue->glGetFloatv(GL_POINT_SIZE, &pointSize);
            glue->glGetIntegerv(GL_POLYGON_MODE, polygonMode);
            if (compatibility) {
                lighting = glue->glIsEnabled(GL_LIGHTING);
                colorMaterial = glue->glIsEnabled(GL_COLOR_MATERIAL);
                vertexClientArray = glue->glIsEnabled(GL_VERTEX_ARRAY);
                normalClientArray = glue->glIsEnabled(GL_NORMAL_ARRAY);
                colorClientArray = glue->glIsEnabled(GL_COLOR_ARRAY);
                glue->glGetIntegerv(
                    GL_LIGHT_MODEL_TWO_SIDE, &twoSidedLighting);
                glue->glGetFloatv(GL_CURRENT_COLOR, currentColor);
            }
        }

        bool equals(const Snapshot& other) const
        {
            return valid == other.valid &&
                compatibility == other.compatibility &&
                hasVbo == other.hasVbo &&
                hasVao == other.hasVao &&
                hasIndirect == other.hasIndirect &&
                program == other.program &&
                vertexArray == other.vertexArray &&
                arrayBuffer == other.arrayBuffer &&
                elementBuffer == other.elementBuffer &&
                indirectBuffer == other.indirectBuffer &&
                matrixMode == other.matrixMode &&
                std::memcmp(colorMask, other.colorMask,
                    sizeof(colorMask)) == 0 &&
                polygonOffset == other.polygonOffset &&
                polygonOffsetFactor == other.polygonOffsetFactor &&
                polygonOffsetUnits == other.polygonOffsetUnits &&
                cull == other.cull &&
                frontFace == other.frontFace &&
                cullFace == other.cullFace &&
                lineWidth == other.lineWidth &&
                pointSize == other.pointSize &&
                polygonMode[0] == other.polygonMode[0] &&
                polygonMode[1] == other.polygonMode[1] &&
                lighting == other.lighting &&
                colorMaterial == other.colorMaterial &&
                vertexClientArray == other.vertexClientArray &&
                normalClientArray == other.normalClientArray &&
                colorClientArray == other.colorClientArray &&
                twoSidedLighting == other.twoSidedLighting &&
                std::memcmp(currentColor, other.currentColor,
                    sizeof(currentColor)) == 0;
        }

        const SoGLContext *glue = nullptr;
        bool valid = false;
        bool compatibility = false;
        bool hasVbo = false;
        bool hasVao = false;
        bool hasIndirect = false;
        GLint program = 0;
        GLint vertexArray = 0;
        GLint arrayBuffer = 0;
        GLint elementBuffer = 0;
        GLint indirectBuffer = 0;
        GLint matrixMode = GL_MODELVIEW;
        GLboolean colorMask[4] = {
            GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
        GLboolean polygonOffset = GL_FALSE;
        GLfloat polygonOffsetFactor = 0.0f;
        GLfloat polygonOffsetUnits = 0.0f;
        GLboolean cull = GL_FALSE;
        GLint frontFace = GL_CCW;
        GLint cullFace = GL_BACK;
        GLfloat lineWidth = 1.0f;
        GLfloat pointSize = 1.0f;
        GLint polygonMode[2] = {GL_FILL, GL_FILL};
        GLboolean lighting = GL_FALSE;
        GLboolean colorMaterial = GL_FALSE;
        GLboolean vertexClientArray = GL_FALSE;
        GLboolean normalClientArray = GL_FALSE;
        GLboolean colorClientArray = GL_FALSE;
        GLint twoSidedLighting = GL_FALSE;
        GLfloat currentColor[4] = {
            1.0f, 1.0f, 1.0f, 1.0f};
    };

public:
    CadGLStateValidationGuard(const SoGLContext *context, bool enabled,
                              bool compatibility, bool vbo, bool vao,
                              bool indirect)
        : enabled_(enabled),
          before_(enabled ?
              Snapshot(context, compatibility, vbo, vao, indirect) :
              Snapshot())
    {
    }

    ~CadGLStateValidationGuard()
    {
        if (!enabled_ || !before_.valid)
            return;
        const Snapshot after(
            before_.glue, before_.compatibility, before_.hasVbo,
            before_.hasVao, before_.hasIndirect);
        if (before_.equals(after))
            return;
        std::fprintf(stderr,
            "CadRendererGL violated its direct-GL state boundary "
            "(set OBOL_CAD_VALIDATE_GL_STATE only for diagnostics)\n");
        std::abort();
    }

private:
    bool enabled_ = false;
    Snapshot before_;
};

/* GL_TIME_ELAPSED queries are deliberately harvested on a later render.
 * Ending a query is non-blocking; reading it here would move the driver's
 * deferred raster wait directly back onto the GUI thread. */
class CadGpuTimerGuard {
public:
    CadGpuTimerGuard(Obol::internal::CadGpuResources *resources,
                     const SoGLContext *context,
                     const uint64_t *triangles,
                     float pointProxyPixelThreshold)
        : resources_(resources), glue_(context), triangles_(triangles),
          pointProxyPixelThreshold_(pointProxyPixelThreshold)
    {
        active_ = resources_ &&
            resources_->beginFrameGpuTimer(glue_);
    }

    ~CadGpuTimerGuard()
    {
        if (active_)
            resources_->endFrameGpuTimer(
                triangles_ ? *triangles_ : 0,
                pointProxyPixelThreshold_, glue_);
    }

private:
    Obol::internal::CadGpuResources *resources_ = nullptr;
    const SoGLContext *glue_ = nullptr;
    const uint64_t *triangles_ = nullptr;
    float pointProxyPixelThreshold_ = 1.0f;
    bool active_ = false;
};

// ---------------------------------------------------------------------------
// Attribute locations for instanced vertex attributes
// Attribute 0: a_pos (vec3)
// Attribute 1: a_norm (vec3)  -- shaded only
// Attributes 2..5: a_instTransform (mat4 = 4 × vec4 columns)
// Attribute 6: a_instColor (vec4)
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
    GLuint proxyShaded = 0;
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
        if (programs.proxyShaded)
            glue->glDeleteObjectARB(programs.proxyShaded);
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

void
deleteDeferredCadGpuResources(void *closure, uint32_t contextId)
{
    std::unique_ptr<CadGpuResources> resources(
        static_cast<CadGpuResources *>(closure));
    resources->releaseAll(
        SoGLContext_instance(static_cast<int>(contextId)));
}

} // namespace

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

thread_local SoGLRenderAction *CadRendererGL::activeRenderAction_ = nullptr;

bool
CadRendererGL::renderInterrupted() const
{
    if (activeRenderInterrupted_)
        return true;
    if (activeRenderAction_ && activeRenderAction_->abortNow()) {
        activeRenderInterrupted_ = true;
        return true;
    }
    return false;
}

bool
CadRendererGL::renderInterruptedAfter(size_t& workCounter, size_t work) const
{
    static constexpr size_t safePointStride = 256u;
    if (!activeRenderAction_)
        return false;
    if (workCounter < safePointStride &&
            work < safePointStride - workCounter) {
        workCounter += work;
        return false;
    }
    workCounter = 0;
    return renderInterrupted();
}

CadRendererGL::CadRendererGL() :
    configuration_(new CadRendererConfiguration)
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

bool
CadRendererGL::softwareGlslRequested() const
{
    return configuration_ && configuration_->softwareGlsl;
}

bool
CadRendererGL::cadLightDebugRequested() const
{
    return configuration_ && configuration_->lightDebug;
}

const char *
CadRendererGL::cadShaderDebugMode() const
{
    return !configuration_ || configuration_->shaderDebug.empty() ?
        nullptr : configuration_->shaderDebug.c_str();
}

void
CadRendererGL::setAmbientLight(float red, float green, float blue,
                               float intensity)
{
    const float level = std::isfinite(intensity) ?
        std::max(0.0f, std::min(1.0f, intensity)) : 0.0f;
    const float color[3] = {red, green, blue};
    for (size_t i = 0; i < 3; ++i) {
        const float component = std::isfinite(color[i]) ?
            std::max(0.0f, std::min(1.0f, color[i])) : 0.0f;
        this->ambientLight_[i] = component * level;
    }
}

void
CadRendererGL::uploadAmbientLight(const SoGLContext* glue, GLuint program)
{
    const GLint location =
        glue->glGetUniformLocationARB(program, "u_ambient");
    if (location >= 0)
        glue->glUniform3fvARB(location, 1, this->ambientLight_);
}

void
CadRendererGL::uploadAssemblyTransform(
        const SoGLContext* glue, GLuint program)
{
    const GLint modelLocation =
        glue->glGetUniformLocationARB(program, "u_rootModel");
    if (modelLocation >= 0)
        glue->glUniformMatrix4fvARB(
            modelLocation, 1, GL_FALSE, assemblyModel_[0]);

    const GLint normalLocation =
        glue->glGetUniformLocationARB(program, "u_rootNormalMatrix");
    if (normalLocation >= 0) {
        const SbMatrix normal = assemblyModel_.inverse().transpose();
        float packed[9];
        for (int row = 0; row < 3; ++row)
            for (int column = 0; column < 3; ++column)
                packed[row * 3 + column] = normal[row][column];
        glue->glUniformMatrix3fvARB(
            normalLocation, 1, GL_FALSE, packed);
    }
}

void
CadRendererGL::uploadLights(const SoGLContext* glue, GLuint program)
{
    this->uploadAssemblyTransform(glue, program);
    // Build parallel arrays for the shaded shader's u_light* uniforms.  Fall
    // back to a single fixed directional light when no scene lights are set.
    int   type[kMaxLights];
    float vec[kMaxLights * 3];
    float axis[kMaxLights * 3];
    float color[kMaxLights * 3];
    float attenuation[kMaxLights * 3];
    float cosCut[kMaxLights];
    float exponent[kMaxLights];
    int n = 0;
    if (!this->lightsSupplied_) {
        type[0] = 0;
        vec[0] = kLightDir[0]; vec[1] = kLightDir[1]; vec[2] = kLightDir[2];
        axis[0] = 0.0f; axis[1] = 0.0f; axis[2] = -1.0f;
        color[0] = 1.0f; color[1] = 1.0f; color[2] = 1.0f;
        attenuation[0] = 1.0f;
        attenuation[1] = 0.0f;
        attenuation[2] = 0.0f;
        cosCut[0] = -2.0f;
        exponent[0] = 0.0f;
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
            attenuation[n * 3 + 0] = l.attenuation[0];
            attenuation[n * 3 + 1] = l.attenuation[1];
            attenuation[n * 3 + 2] = l.attenuation[2];
            cosCut[n] = l.cosCutoff;
            exponent[n] = l.spotExponent;
            ++n;
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
    const GLint attenuationLocation =
        glue->glGetUniformLocationARB(program, "u_latten");
    const GLint exponentLocation =
        glue->glGetUniformLocationARB(program, "u_lexponent");
    glue->glUniform1iARB(countLocation, n);
    if (n > 0) {
        glue->glUniform1ivARB(typeLocation, n, type);
        glue->glUniform3fvARB(vectorLocation, n, vec);
        glue->glUniform3fvARB(axisLocation, n, axis);
        glue->glUniform3fvARB(colorLocation, n, color);
        glue->glUniform3fvARB(attenuationLocation, n, attenuation);
        glue->glUniform1fvARB(cutoffLocation, n, cosCut);
        glue->glUniform1fvARB(exponentLocation, n, exponent);
    }
    this->uploadAmbientLight(glue, program);
    if (cadLightDebugRequested()) {
        static std::atomic<unsigned int> reportCount{0};
        if (reportCount.fetch_add(1, std::memory_order_relaxed) < 32) {
            std::fprintf(stderr,
                "CadRendererGL uploadLights program=%u n=%d "
                "locations={count=%d type=%d vec=%d axis=%d color=%d cos=%d} "
                "l0={type=%d vec=(%.9g,%.9g,%.9g) "
                "color=(%.9g,%.9g,%.9g)}\n",
                program, n, countLocation, typeLocation, vectorLocation,
                axisLocation, colorLocation, cutoffLocation,
                n > 0 ? type[0] : -1,
                n > 0 ? vec[0] : 0.0f,
                n > 0 ? vec[1] : 0.0f,
                n > 0 ? vec[2] : 0.0f,
                n > 0 ? color[0] : 0.0f,
                n > 0 ? color[1] : 0.0f,
                n > 0 ? color[2] : 0.0f);
        }
    }
}

void
CadRendererGL::uploadFixedLights(const SoGLContext* glue)
{
    if (!glue)
        return;

    const GLfloat modelAmbient[4] = {
        this->ambientLight_[0], this->ambientLight_[1],
        this->ambientLight_[2], 1.0f
    };
    SoGLContext_glLightModelfv(
        glue, GL_LIGHT_MODEL_AMBIENT, modelAmbient);

    GlLight fallback;
    fallback.type = 0;
    fallback.vec[0] = kLightDir[0];
    fallback.vec[1] = kLightDir[1];
    fallback.vec[2] = kLightDir[2];
    const GlLight *active = this->lights_.data();
    size_t activeCount = this->lights_.size();
    if (!this->lightsSupplied_) {
        active = &fallback;
        activeCount = 1;
    }
    activeCount = std::min(activeCount, static_cast<size_t>(kMaxLights));

    const GLfloat black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    constexpr float radiansToDegrees =
        57.295779513082320876798154814105f;
    for (int i = 0; i < kMaxLights; ++i) {
        const GLenum slot = static_cast<GLenum>(GL_LIGHT0 + i);
        if (static_cast<size_t>(i) >= activeCount) {
            SoGLContext_glDisable(glue, slot);
            continue;
        }

        GlLight light = active[i];
        /* Fixed-function GL transforms a light by the model-view matrix
         * current when glLight is called.  That matrix includes the enclosing
         * assembly model transform, so express world-space scene lights in
         * assembly-local space first; the two transforms then cancel. */
        const SbMatrix worldToAssembly = assemblyModel_.inverse();
        SbVec3f vector(light.vec);
        SbVec3f transformed;
        if (light.type == 0)
            worldToAssembly.multDirMatrix(vector, transformed);
        else
            worldToAssembly.multVecMatrix(vector, transformed);
        if (light.type == 0 && transformed.normalize() == 0.0f)
            transformed.setValue(kLightDir);
        light.vec[0] = transformed[0];
        light.vec[1] = transformed[1];
        light.vec[2] = transformed[2];
        if (light.type == 2) {
            SbVec3f axis(light.axis);
            worldToAssembly.multDirMatrix(axis, transformed);
            if (transformed.normalize() != 0.0f) {
                light.axis[0] = transformed[0];
                light.axis[1] = transformed[1];
                light.axis[2] = transformed[2];
            }
        }
        const GLfloat color[4] = {
            light.color[0], light.color[1], light.color[2], 1.0f
        };
        const GLfloat position[4] = {
            light.vec[0], light.vec[1], light.vec[2],
            light.type == 0 ? 0.0f : 1.0f
        };
        SoGLContext_glLightfv(glue, slot, GL_AMBIENT, black);
        SoGLContext_glLightfv(glue, slot, GL_DIFFUSE, color);
        SoGLContext_glLightfv(glue, slot, GL_SPECULAR, color);
        SoGLContext_glLightfv(glue, slot, GL_POSITION, position);
        SoGLContext_glLightf(glue, slot, GL_CONSTANT_ATTENUATION,
            light.attenuation[0]);
        SoGLContext_glLightf(glue, slot, GL_LINEAR_ATTENUATION,
            light.attenuation[1]);
        SoGLContext_glLightf(glue, slot, GL_QUADRATIC_ATTENUATION,
            light.attenuation[2]);
        SoGLContext_glLightf(glue, slot, GL_SPOT_EXPONENT,
            light.spotExponent);
        if (light.type == 2) {
            const GLfloat direction[3] = {
                light.axis[0], light.axis[1], light.axis[2]
            };
            SoGLContext_glLightfv(
                glue, slot, GL_SPOT_DIRECTION, direction);
            const float cosine = std::max(-1.0f,
                std::min(1.0f, light.cosCutoff));
            SoGLContext_glLightf(
                glue, slot, GL_SPOT_CUTOFF,
                std::acos(cosine) * radiansToDegrees);
        } else {
            SoGLContext_glLightf(glue, slot, GL_SPOT_CUTOFF, 180.0f);
        }
        SoGLContext_glEnable(glue, slot);
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
    {
        std::lock_guard<std::mutex> guard(cadRendererRegistryMutex);
        cadRendererRegistry.erase(this);
    }

    std::lock_guard<std::recursive_mutex> resourceLock(resourceMutex_);
    for (auto& [contextId, resources] : gpuResources_) {
        if (!resources)
            continue;
        try {
            SoGLCacheContextElement::scheduleDeleteCallback(
                contextId, deleteDeferredCadGpuResources, resources.get());
            (void)resources.release();
        } catch (const std::bad_alloc&) {
            /* No context is guaranteed current in this destructor.  Drop CPU
             * ownership safely; the GL driver reclaims the names with the
             * context in this exceptional allocation-failure path. */
            resources->releaseAll(nullptr);
        }
    }
    gpuResources_.clear();
    gpuRes_ = nullptr;
    gpuContextId_ = 0;
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
    std::lock_guard<std::recursive_mutex> resourceLock(resourceMutex_);
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
            shaders_.proxyShaded = found->second.proxyShaded;
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
            programs.proxyShaded = shaders_.proxyShaded;
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

void CadRendererGL::noteRenderPreparation(const char *reason)
{
    Obol::internal::cadAdvanceIdentity(renderPreparationSerial_);
    if (configuration_ && configuration_->patchDebug)
        std::fprintf(stderr,
            "CadRendererGL preparation serial=%llu reason=%s\n",
            static_cast<unsigned long long>(renderPreparationSerial_),
            reason ? reason : "unknown");
}

const Obol::CadViewState&
CadRendererGL::activeViewState() const
{
    assert(activeViewState_ != nullptr);
    return *activeViewState_;
}

uint8_t
CadRendererGL::effectiveProgressiveCut(uint8_t requested) const
{
    return Obol::cadEffectiveProgressiveCut(
        activeViewState(), requested);
}

uint8_t
CadRendererGL::effectiveProgressiveCut(
        Obol::PartId part, uint8_t requested) const
{
    return Obol::cadEffectiveProgressiveCut(
        activeViewState(), part, requested);
}

uint8_t
CadRendererGL::maximumEffectiveProgressiveCut(uint8_t requested) const
{
    return Obol::cadMaximumEffectiveProgressiveCut(
        activeViewState(), requested);
}

Obol::CadPresentationPreparationTarget
CadRendererGL::preparationTarget(
        Obol::CadPresentationPreparationKind kind,
        uint32_t contextId, uint64_t planRevision,
        uint64_t geometryRevision, int progressiveCutCeiling,
        float progressiveCutNextFraction,
        const SbMatrix& viewProjection,
        const Obol::CadPresentationPreparationSnapshot *retained)
{
    Obol::CadPresentationPreparationTarget target;
    target.kind = kind;
    target.viewId = activeViewState().viewId;
    target.contextId = contextId;
    target.planRevision = planRevision;
    target.geometryRevision = geometryRevision;
    target.progressiveCutCeiling = progressiveCutCeiling;
    std::memcpy(&target.progressiveCutNextFractionBits,
        &progressiveCutNextFraction,
        sizeof(target.progressiveCutNextFractionBits));
    for (size_t i = 0; i < target.viewProjectionBits.size(); ++i)
        std::memcpy(&target.viewProjectionBits[i],
            viewProjection[0] + i, sizeof(target.viewProjectionBits[i]));
    if (retained && retained->hasTarget()) {
        Obol::CadPresentationPreparationTarget active = retained->target;
        active.obligationRevision = 0;
        if (active == target)
            return retained->target;
    }
    if (presentationPreparation_.state ==
            Obol::CadPresentationPreparationState::Preparing) {
        Obol::CadPresentationPreparationTarget active =
            presentationPreparation_.target;
        active.obligationRevision = 0;
        if (active == target)
            return presentationPreparation_.target;
    }
    target.obligationRevision = Obol::internal::cadTakeNonzeroIdentity(
        nextPreparationRevision_);
    return target;
}

void
CadRendererGL::publishPreparation(
        const Obol::CadPresentationPreparationTarget& target,
        Obol::CadPresentationPreparationState state,
        uint64_t totalUnits, uint64_t completedUnits,
        uint64_t reservedBytes)
{
    Obol::CadPresentationPreparationSnapshot next;
    next.target = target;
    next.state = state;
    next.totalUnits = totalUnits;
    next.completedUnits = std::min(completedUnits, totalUnits);
    next.reservedBytes = reservedBytes;

    /* An exact target may only move forward.  Replacing the target starts a
     * new obligation; replaying the same target can never make completed
     * retained work disappear from the host-visible certificate. */
    if (presentationPreparation_.target == target &&
            presentationPreparation_.hasTarget()) {
        next.totalUnits = presentationPreparation_.totalUnits;
        next.completedUnits = std::max(
            next.completedUnits,
            presentationPreparation_.completedUnits);
        next.reservedBytes = std::max(
            next.reservedBytes,
            presentationPreparation_.reservedBytes);
    }
    presentationPreparation_ = next;
}

void
CadRendererGL::publishFixedCutPreparation(
        Obol::CadPresentationPreparationSnapshot& retained,
        const Obol::CadPresentationPreparationTarget& target,
        uint64_t totalUnits, uint64_t completedUnits,
        uint64_t reservedBytes, bool complete)
{
    /* Fixed-function executors prepare a finite, ordered set of occurrence
     * requests.  Credit only successful buffer publication, not traversal or
     * cache lookup.  Keep the frontier even after completion: eviction and
     * re-upload of an earlier request must not mint another retry for the
     * same camera/cut.  Other executor phases may use the public snapshot
     * between calls, so the retained frontier owns this monotonicity. */
    reservedBytes = cadSaturatingWorkAdd(reservedBytes,
        gpuRes_ ? gpuRes_->resourceSnapshot().progressiveCutBufferBytes : 0);
    if (retained.hasTarget() && retained.target == target) {
        completedUnits = std::max(completedUnits, retained.completedUnits);
        reservedBytes = std::max(reservedBytes, retained.reservedBytes);
        complete = complete || retained.state ==
            Obol::CadPresentationPreparationState::Complete;
    }
    publishPreparation(target, complete ?
        Obol::CadPresentationPreparationState::Complete :
        Obol::CadPresentationPreparationState::Preparing,
        totalUnits, complete ? totalUnits : completedUnits, reservedBytes);
    retained = presentationPreparation_;
}

Obol::CadPresentationPreparationSnapshot
CadRendererGL::presentationPreparationSnapshot() const
{
    Obol::CadPresentationPreparationSnapshot snapshot =
        presentationPreparation_;
    if (!indirectPreparation_.active)
        return snapshot;
    snapshot.state = indirectPreparation_.phase ==
            IndirectPreparationPhase::Submit ?
        Obol::CadPresentationPreparationState::Complete :
        Obol::CadPresentationPreparationState::Preparing;
    snapshot.totalUnits = indirectPreparation_.totalUnits;
    snapshot.completedUnits = std::min(
        indirectPreparation_.completedUnits,
        indirectPreparation_.totalUnits);
    snapshot.reservedBytes = indirectPreparation_.requestedLiveBytes;
    return snapshot;
}

void CadRendererGL::ensurePartUploaded(
        PartId pid, const SoCADAssembly& assembly, uint64_t gen,
        uint8_t requestedCut, const SoGLContext* glue)
{
    // Retrieve part geometry from the assembly
    const Obol::PartGeometry* geom = assembly.partGeometry(pid);
    if (!geom) return;
    const bool progressive =
        (geom->wire.has_value() &&
            Obol::internal::cadWireSourceIsProgressive(*geom->wire)) ||
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

    const Obol::TriMesh *derivedWireMesh =
        geom->wire.has_value() && geom->wire->derivesTriangleEdges() ?
            geom->wire->triangleEdges() : nullptr;
    const Obol::TriMesh *shadedMesh = geom->shaded.has_value() ?
        &*geom->shaded : nullptr;
    if (geom->wire.has_value()) {
        const Obol::WireRep& wire = *geom->wire;
        if ((derivedWireMesh &&
                (!wire.segmentPoints.empty() || !wire.polylines.empty() ||
                 !wire.segmentIds.empty())) ||
            (!derivedWireMesh &&
                (wire.segmentPoints.size() % 2u != 0u ||
                 (!wire.segmentIds.empty() && wire.segmentIds.size() !=
                     wire.segmentCount()))))
            return;
    }
    /*
     * System GL can draw a wire-only triangle source directly in polygon
     * line mode.  Software GL instead consumes the compact GL_LINES stream
     * assembled below; uploading a second, unused triangle copy doubled the
     * resident memory of large wire-only meshes.
     */
    const Obol::TriMesh *triangleUploadMesh = shadedMesh ? shadedMesh :
        (caps_.isSoftwareRenderer ? nullptr : derivedWireMesh);
    const bool triangleUploadProgressive =
        triangleUploadMesh && triangleUploadMesh->isProgressive();
    const uint64_t triangleProgressiveLineage =
        triangleUploadProgressive ? triangleUploadMesh->progressiveLineage :
            0u;
    const auto requestedCounts = [requestedCut](
            const Obol::TriMesh *mesh) {
        std::pair<size_t, size_t> counts(0u, 0u);
        if (!mesh)
            return counts;
        counts.first = mesh->isProgressive() ?
            mesh->indexCountAtCut(requestedCut) : mesh->indices.size();
        counts.second = mesh->isProgressive() ?
            mesh->positionCountAtCut(requestedCut) : mesh->positions.size();
        return counts;
    };
    const std::pair<size_t, size_t> derivedWireCounts =
        requestedCounts(derivedWireMesh);
    const size_t requiredDerivedIndexCount = derivedWireCounts.first;
    const size_t requiredDerivedPositionCount = derivedWireCounts.second;
    const std::pair<size_t, size_t> triangleUploadCounts =
        requestedCounts(triangleUploadMesh);
    const size_t requiredTriangleIndexCount = triangleUploadCounts.first;
    const size_t requiredTrianglePositionCount = triangleUploadCounts.second;

    size_t requiredExplicitWirePointCount = 0u;
    size_t requiredExplicitWireIndexCount = 0u;
    size_t flatWirePointCount = 0u;
    size_t polylineWirePointCount = 0u;
    size_t polylineWireSegmentCount = 0u;
    if (geom->wire.has_value() && !derivedWireMesh) {
        const Obol::WireRep& wire = *geom->wire;
        if (wire.segmentCount() > SIZE_MAX / 2u)
            return;
        flatWirePointCount = wire.segmentCount() * 2u;
        for (const Obol::WirePolyline& polyline : wire.polylines) {
            if (polyline.points.size() >
                    SIZE_MAX - polylineWirePointCount)
                return;
            polylineWirePointCount += polyline.points.size();
            if (polyline.points.size() >= 2u) {
                const size_t segments = polyline.points.size() - 1u;
                if (segments > SIZE_MAX - polylineWireSegmentCount)
                    return;
                polylineWireSegmentCount += segments;
            }
        }
        if (flatWirePointCount >
                SIZE_MAX - polylineWirePointCount)
            return;
        requiredExplicitWirePointCount =
            flatWirePointCount + polylineWirePointCount;
        if (!wire.polylines.empty()) {
            if (polylineWireSegmentCount >
                    (SIZE_MAX - flatWirePointCount) / 2u)
                return;
            requiredExplicitWireIndexCount = flatWirePointCount +
                polylineWireSegmentCount * 2u;
        }
    }

    if ((derivedWireMesh &&
            (geom->wire->triangleEdgeSegmentCount !=
                 derivedWireMesh->indices.size() ||
             requiredDerivedIndexCount % 3u != 0u ||
             (requiredDerivedIndexCount > 0u &&
                 requiredDerivedPositionCount == 0u))) ||
        (triangleUploadMesh &&
            (requiredTriangleIndexCount % 3u != 0u ||
             (requiredTriangleIndexCount > 0u &&
                 requiredTrianglePositionCount == 0u) ||
             (!triangleUploadMesh->normals.empty() &&
                 triangleUploadMesh->normals.size() !=
                     triangleUploadMesh->positions.size()))))
        return;
    if (derivedWireMesh &&
            (requiredDerivedIndexCount > SIZE_MAX / 2u ||
             requiredDerivedPositionCount > static_cast<size_t>(
                 std::numeric_limits<GLsizei>::max()) ||
             requiredDerivedIndexCount > static_cast<size_t>(
                 std::numeric_limits<GLsizei>::max())))
        return;
    if (triangleUploadMesh &&
            (requiredTrianglePositionCount > static_cast<size_t>(
                 std::numeric_limits<GLsizei>::max()) ||
             requiredTriangleIndexCount > static_cast<size_t>(
                 std::numeric_limits<GLsizei>::max())))
        return;
    const size_t requiredDerivedWireIndexCount = derivedWireMesh ?
        requiredDerivedIndexCount * 2u : 0u;
    const size_t maximumWirePointCount = std::min<size_t>(
        static_cast<size_t>(std::numeric_limits<GLsizei>::max()),
        static_cast<size_t>(UINT32_MAX));
    if (requiredDerivedWireIndexCount > static_cast<size_t>(
            std::numeric_limits<GLsizei>::max()) ||
        requiredExplicitWirePointCount > maximumWirePointCount ||
        requiredExplicitWireIndexCount > static_cast<size_t>(
            std::numeric_limits<GLsizei>::max()) ||
        requiredExplicitWirePointCount > SIZE_MAX / 3u)
        return;

    /* Test exact required counts before validating or materializing an
     * index prefix.  A stable frame must be O(1) in mesh size. */
    const size_t requiredWirePointCount =
        derivedWireMesh && caps_.isSoftwareRenderer ?
            requiredDerivedPositionCount : requiredExplicitWirePointCount;
    const size_t requiredWireIndexCount =
        derivedWireMesh && caps_.isSoftwareRenderer ?
            requiredDerivedWireIndexCount : requiredExplicitWireIndexCount;
    if (gpuRes_->isUpToDate(
            pid, gen,
            static_cast<GLsizei>(requiredWirePointCount),
            static_cast<GLsizei>(requiredWireIndexCount),
            static_cast<GLsizei>(requiredTrianglePositionCount),
            static_cast<GLsizei>(requiredTriangleIndexCount)))
        return;

    // Build CPU-side flat arrays for indexed wire geometry.  Pure flat
    // segment-pair input can be uploaded directly from WireRep::segmentPoints.
    std::vector<float>    wirePos;
    std::vector<uint32_t> wireSegIdx;
    const float*          pWirePos = nullptr;
    const uint32_t*       pWireSeg = nullptr;
    GLsizei               wirePointCount = 0;
    GLsizei               wireSegIdxCount = 0;
    if (geom->wire.has_value() &&
            !geom->wire->derivesTriangleEdges()) {
        const auto& wr = *geom->wire;
        /*
         * Progressive wire levels are independent immutable ranges, not a
         * prefix.  Retain the complete range table on the GPU once and let
         * draw submission select first/count.  Uploading one selected range
         * and later appending a different range corrupts the payload because
         * the two simplifications need not share an ordered prefix.
         */
        const size_t flatPointFirst = 0u;
        if (flatWirePointCount > 0 && wr.polylines.empty()) {
            pWirePos = packedVec3fData(wr.segmentPoints) +
                flatPointFirst * 3;
            wirePointCount =
                static_cast<GLsizei>(flatWirePointCount);
        } else {
            wirePos.reserve(requiredExplicitWirePointCount * 3u);
            wireSegIdx.reserve(requiredExplicitWireIndexCount);

            const size_t flatPointEnd =
                flatPointFirst + flatWirePointCount;
            for (size_t i = flatPointFirst;
                    i + 1 < flatPointEnd; i += 2) {
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
    } else if (geom->wire.has_value() &&
            geom->wire->derivesTriangleEdges() &&
            caps_.isSoftwareRenderer) {
        /* Mesa's software triangle setup is materially slower than its line
         * rasterizer.  Reuse the immutable mesh positions and derive only a
         * compact GL_LINES index stream (two uint32 values per edge).  This
         * avoids six copied SbVec3f endpoints per triangle while preserving
         * the software backend's proven line path. */
        const Obol::TriMesh& mesh = *geom->wire->triangleEdges();
        wireSegIdx.resize(requiredDerivedWireIndexCount);
        for (size_t index = 0;
                index + 2 < requiredDerivedIndexCount; index += 3) {
            const uint32_t first = mesh.indices[index];
            const uint32_t second = mesh.indices[index + 1];
            const uint32_t third = mesh.indices[index + 2];
            /* The active progressive position prefix, rather than the
             * richest resident vector, is the valid index domain for this
             * cut.  Testing the full vector allowed malformed cut metadata
             * to submit out-of-range VBO reads and transient far-away
             * vertices on drivers which did not happen to return zeros. */
            if (first >= requiredDerivedPositionCount ||
                    second >= requiredDerivedPositionCount ||
                    third >= requiredDerivedPositionCount)
                return;
            const size_t target = index * 2u;
            wireSegIdx[target] = first;
            wireSegIdx[target + 1] = second;
            wireSegIdx[target + 2] = second;
            wireSegIdx[target + 3] = third;
            wireSegIdx[target + 4] = third;
            wireSegIdx[target + 5] = first;
        }
        pWirePos = packedVec3fData(mesh.positions);
        pWireSeg = wireSegIdx.data();
        wirePointCount =
            static_cast<GLsizei>(requiredDerivedPositionCount);
        wireSegIdxCount = static_cast<GLsizei>(wireSegIdx.size());
    }

    // Triangle mesh vectors already use packed SbVec3f storage.
    const float*    pTriPos = nullptr;
    const float*    pTriNorm = nullptr;
    const uint32_t* pTriIdx = nullptr;
    GLsizei         triPosCount = 0;
    GLsizei         triIdxCount = 0;
    if (triangleUploadMesh) {
        const auto& mesh = *triangleUploadMesh;
        for (size_t index = 0; index < requiredTriangleIndexCount; ++index) {
            if (mesh.indices[index] >= requiredTrianglePositionCount)
                return;
        }
        pTriPos = packedVec3fData(mesh.positions);
        pTriNorm = packedVec3fData(mesh.normals);
        pTriIdx = mesh.indices.empty() ? nullptr : mesh.indices.data();
        triPosCount =
            static_cast<GLsizei>(requiredTrianglePositionCount);
        triIdxCount = static_cast<GLsizei>(requiredTriangleIndexCount);
    }

    const bool wireUploadProgressive = wirePointCount > 0 &&
        geom->wire.has_value() &&
        Obol::internal::cadWireSourceIsProgressive(*geom->wire);
    const uint64_t wireProgressiveLineage = wireUploadProgressive ?
        Obol::internal::cadWireSourceProgressiveLineage(*geom->wire) : 0u;

    /*
     * A richer cumulative prefix already on the GPU remains valid when the
     * view asks for less.  Never shrink the retained ordinary buffers merely
     * to enter interaction; only defer appending newly resident tails.
     */
    if (wireUploadProgressive &&
            gpuRes_->hasCompatibleProgressiveWirePrefix(
                pid, wireProgressiveLineage)) {
        if (const CadWireGpu *wire = gpuRes_->wireFor(pid)) {
            wirePointCount = std::max(wirePointCount, wire->vertCount);
            wireSegIdxCount = std::max(wireSegIdxCount, wire->idxCount);
        }
    }
    if (triangleUploadProgressive &&
            gpuRes_->hasCompatibleProgressiveTrianglePrefix(
                pid, triangleProgressiveLineage)) {
        if (const CadTriGpu *tri = gpuRes_->triFor(pid)) {
            triPosCount = std::max(triPosCount, tri->vertCount);
            triIdxCount = std::max(triIdxCount, tri->idxCount);
        }
    }

    /*
     * A retained software wire buffer may be richer than the cut which
     * initially built wireSegIdx above.  The draw buffer deliberately keeps
     * that richer prefix, but upload must never advertise elements past the
     * temporary CPU index stream.  Apart from being an out-of-bounds read,
     * the old ordering produced transient invalid line indices after a view
     * coarsened and then reused a richer resident prefix.
     */
    if (derivedWireMesh && caps_.isSoftwareRenderer &&
            wireSegIdxCount > static_cast<GLsizei>(wireSegIdx.size())) {
        if (wireSegIdxCount < 0 || wireSegIdxCount % 6 != 0)
            return;
        const size_t retainedIndexCount =
            static_cast<size_t>(wireSegIdxCount) / 2u;
        if (retainedIndexCount > derivedWireMesh->indices.size() ||
                retainedIndexCount % 3u != 0u)
            return;
        wireSegIdx.resize(static_cast<size_t>(wireSegIdxCount));
        for (size_t index = 0; index < retainedIndexCount; index += 3u) {
            const uint32_t first = derivedWireMesh->indices[index];
            const uint32_t second = derivedWireMesh->indices[index + 1u];
            const uint32_t third = derivedWireMesh->indices[index + 2u];
            if (first >= static_cast<size_t>(wirePointCount) ||
                    second >= static_cast<size_t>(wirePointCount) ||
                    third >= static_cast<size_t>(wirePointCount))
                return;
            const size_t target = index * 2u;
            wireSegIdx[target] = first;
            wireSegIdx[target + 1u] = second;
            wireSegIdx[target + 2u] = second;
            wireSegIdx[target + 3u] = third;
            wireSegIdx[target + 4u] = third;
            wireSegIdx[target + 5u] = first;
        }
        pWireSeg = wireSegIdx.data();
    }
    if (gpuRes_->isUpToDate(
            pid, gen, wirePointCount, wireSegIdxCount,
            triPosCount, triIdxCount))
        return;

    noteRenderPreparation("ordinary-part-upload");
    gpuRes_->upload(pid,
                   pPointPos, pointCount,
                   pWirePos,  wirePointCount,
                   pWireSeg,  wireSegIdxCount,
                   pTriPos,   triPosCount,
                   pTriNorm,
                   pTriIdx,   triIdxCount,
                   gen,
                   wireUploadProgressive,
                   wireProgressiveLineage,
                   triangleUploadProgressive,
                   triangleProgressiveLineage,
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

static uint8_t maximumRequestedCut(
        const CadFramePlan& plan, const Obol::CadViewState& viewState,
        PartId part)
{
    const auto found = plan.partPresentation.find(part);
    return found != plan.partPresentation.end() ?
        Obol::cadMaximumEffectiveProgressiveCut(
            viewState, found->second.maximumRequestedCut) :
        Obol::ProgressiveCutUnspecified;
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
    size_t deadlineWork = 256u;

    for (const CadDrawItem& item : plan.pointItems) {
        if (renderInterruptedAfter(deadlineWork))
            return;
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
    bool interrupted = false;

    if (fixedFunction) {
        glue->glMatrixMode(GL_PROJECTION);
        glue->glPushMatrix();
        glue->glLoadIdentity();
        glue->glMatrixMode(GL_MODELVIEW);
        glue->glPushMatrix();
        const GLboolean wasLighting = glue->glIsEnabled(GL_LIGHTING);
        glue->glDisable(GL_LIGHTING);
        configureFixedClientArrays(glue, false, false);

        for (const CadDrawItem& item : plan.pointItems) {
            if (renderInterruptedAfter(deadlineWork)) {
                interrupted = true;
                break;
            }
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
                if (renderInterruptedAfter(deadlineWork)) {
                    interrupted = true;
                    break;
                }
                const size_t instanceIndex = item.baseInstance + i;
                if (!cadInstanceDrawable(
                        plan, item, instanceIndex, CadDrawChannel::Points))
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
                    if (gpu && renderInterruptedAfter(deadlineWork)) {
                        interrupted = true;
                        break;
                    }
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
                if (interrupted)
                    break;
            }
            if (interrupted)
                break;
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
            if (renderInterruptedAfter(deadlineWork)) {
                interrupted = true;
                break;
            }
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
                if (renderInterruptedAfter(deadlineWork)) {
                    interrupted = true;
                    break;
                }
                const size_t instanceIndex = item.baseInstance + i;
                if (!cadInstanceDrawable(
                        plan, item, instanceIndex, CadDrawChannel::Points))
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
                    if (renderInterruptedAfter(deadlineWork)) {
                        interrupted = true;
                        break;
                    }
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
                if (interrupted)
                    break;
            }
            if (gpu->vao && glue->glBindVertexArray)
                glue->glBindVertexArray(0);
            else {
                glue->glDisableVertexAttribArrayARB(static_cast<GLuint>(locPos));
                glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
            }
            if (interrupted)
                break;
        }
        glue->glUseProgramObjectARB(0);
    }
    glue->glPointSize(savedPointSize);
}

bool CadRendererGL::wireRepHasUncollapsedInstances(
        const CadFramePlan& plan, PartId part)
{
    const auto found = plan.partPresentation.find(part);
    return found != plan.partPresentation.end() &&
        found->second.wireHasUncollapsedInstances;
}

const std::vector<CadSubpixelProxyPoint>&
CadRendererGL::presentationSubpixelProxyPoints(
        const CadFramePlan& plan, const SoGLContext* glue,
        const SbMatrix& viewProj)
{
    /*
     * One batched aggregate stream is already efficient on a hardware driver,
     * where preserving one representative per visible occurrence gives the
     * best visual density.  In fixed-function software GL, however, each
     * point or box still crosses the software transform/raster path.  Bound
     * that work by keeping one deterministic representative per small screen
     * bin.  At equal selection/style priority a box wins over a point, so the
     * bounded stream preserves a recognizable extent where one is available.
     *
     * The source stream is intentionally left unchanged: it remains the
     * exact per-occurrence presentation record used by coverage accounting,
     * sparse styling, selection, and picking.  This is strictly a renderer
     * local optimization of the vertices actually submitted to GL.
    */
    static constexpr size_t minimumSourcePoints = 4096u;
    static constexpr size_t maximumScreenBins = 32768u;
    static constexpr float minimumClipW = 1.0e-20f;
    const std::vector<CadSubpixelProxyPoint>& source =
        plan.subpixelProxyPoints;
    if (!caps_.isSoftwareRenderer || source.size() < minimumSourcePoints)
        return source;

    std::array<GLint, 4> viewport = {0, 0, 0, 0};
    glue->glGetIntegerv(GL_VIEWPORT, viewport.data());
    if (viewport[2] <= 1 || viewport[3] <= 1)
        return source;

    if (softwareBinnedSubpixelValid_ &&
            softwareBinnedSubpixelSourceRevision_ ==
                plan.subpixelProxyRevision &&
            softwareBinnedSubpixelAttributeRevision_ ==
                plan.instanceAttributeRevision &&
            softwareBinnedSubpixelViewProj_ == viewProj &&
            softwareBinnedSubpixelViewport_ == viewport &&
            softwareBinnedSubpixelContextId_ == glue->contextid)
        return softwareBinnedSubpixelProxyPoints_;

    const size_t viewportWidth = static_cast<size_t>(viewport[2]);
    const size_t viewportHeight = static_cast<size_t>(viewport[3]);
    size_t binCountX = viewportWidth;
    size_t binCountY = viewportHeight;
    if (viewportWidth > maximumScreenBins / viewportHeight) {
        const double aspect = static_cast<double>(viewportWidth) /
            static_cast<double>(viewportHeight);
        binCountX = std::min(viewportWidth, maximumScreenBins);
        binCountX = std::max<size_t>(1u, std::min(binCountX,
            static_cast<size_t>(std::sqrt(
                static_cast<double>(maximumScreenBins) * aspect))));
        binCountY = std::max<size_t>(1u, std::min(viewportHeight,
            maximumScreenBins / binCountX));
    }
    /* Ceiling division ensures the actual occupied grid cannot have more
     * cells than binCountX * binCountY, which is bounded above by the named
     * presentation cap even for a very wide or very tall viewport. */
    const GLint binWidth = static_cast<GLint>((viewportWidth + binCountX - 1u) /
        binCountX);
    const GLint binHeight = static_cast<GLint>((viewportHeight + binCountY - 1u) /
        binCountY);
    const auto pointVisible = [](const CadSubpixelProxyPoint& point) {
        return !(point.flags & CadInstanceHidden);
    };
    const auto binKey = [](GLint x, GLint y) {
        return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) |
            static_cast<uint32_t>(y);
    };
    const auto visualPriority = [](const CadSubpixelProxyPoint& point) {
        uint32_t priority = 0u;
        if (point.flags & CadInstanceColorOverride)
            priority |= 1u;
        if (point.flags & CadInstanceHovered)
            priority |= 2u;
        if (point.flags & CadInstanceSelected)
            priority |= 4u;
        return priority;
    };
    const auto shapePriority = [](const CadSubpixelProxyPoint& point) {
        return point.shape == CadAggregateProxyShape::Box ? 1u : 0u;
    };
    const auto stableBefore = [](const InstanceId& left,
                                 const InstanceId& right) {
        return left.w0 != right.w0 ? left.w0 < right.w0 :
            left.w1 < right.w1;
    };
    std::unordered_map<uint64_t, size_t> pointByBin;
    pointByBin.reserve(std::min(source.size(), maximumScreenBins));
    std::vector<float> depthByBin;
    depthByBin.reserve(std::min(source.size(), maximumScreenBins));
    softwareBinnedSubpixelProxyPoints_.clear();
    softwareBinnedSubpixelProxyPoints_.reserve(
        std::min(source.size(), maximumScreenBins));

    for (const CadSubpixelProxyPoint& point : source) {
        if (!pointVisible(point))
            continue;
        SbVec4f clip;
        viewProj.multVecMatrix(SbVec4f(point.position[0], point.position[1],
            point.position[2], 1.0f), clip);
        if (!std::isfinite(clip[0]) || !std::isfinite(clip[1]) ||
                !std::isfinite(clip[2]) || !std::isfinite(clip[3]) ||
                clip[3] <= minimumClipW) {
            /* Never let a defensive optimization suppress an otherwise
             * valid logical proxy if an unusual projection reaches here. */
            softwareBinnedSubpixelProxyPoints_.push_back(point);
            depthByBin.push_back(0.0f);
            continue;
        }
        const float inverseW = 1.0f / clip[3];
        const float normalizedX = clip[0] * inverseW * 0.5f + 0.5f;
        const float normalizedY = clip[1] * inverseW * 0.5f + 0.5f;
        if (!std::isfinite(normalizedX) || !std::isfinite(normalizedY)) {
            softwareBinnedSubpixelProxyPoints_.push_back(point);
            depthByBin.push_back(0.0f);
            continue;
        }
        const GLint screenX = static_cast<GLint>(std::floor(normalizedX *
            static_cast<float>(viewport[2] - 1)));
        const GLint screenY = static_cast<GLint>(std::floor(normalizedY *
            static_cast<float>(viewport[3] - 1)));
        const uint64_t key = binKey(screenX / binWidth,
            screenY / binHeight);
        const auto found = pointByBin.find(key);
        const float depth = clip[2] * inverseW;
        if (found == pointByBin.end()) {
            pointByBin.emplace(key,
                softwareBinnedSubpixelProxyPoints_.size());
            softwareBinnedSubpixelProxyPoints_.push_back(point);
            depthByBin.push_back(depth);
            continue;
        }
        const size_t index = found->second;
        const CadSubpixelProxyPoint& current =
            softwareBinnedSubpixelProxyPoints_[index];
        const uint32_t candidatePriority = visualPriority(point);
        const uint32_t currentPriority = visualPriority(current);
        const uint32_t candidateShapePriority = shapePriority(point);
        const uint32_t currentShapePriority = shapePriority(current);
        if (candidatePriority > currentPriority ||
                (candidatePriority == currentPriority &&
                 (candidateShapePriority > currentShapePriority ||
                  (candidateShapePriority == currentShapePriority &&
                   (depth < depthByBin[index] ||
                    (depth == depthByBin[index] && stableBefore(
                        point.instanceId, current.instanceId))))))) {
            softwareBinnedSubpixelProxyPoints_[index] = point;
            depthByBin[index] = depth;
        }
    }

    softwareBinnedSubpixelSourceRevision_ = plan.subpixelProxyRevision;
    softwareBinnedSubpixelAttributeRevision_ =
        plan.instanceAttributeRevision;
    softwareBinnedSubpixelViewProj_ = viewProj;
    softwareBinnedSubpixelViewport_ = viewport;
    softwareBinnedSubpixelContextId_ = glue->contextid;
    softwareBinnedSubpixelValid_ = true;
    Obol::internal::cadAdvanceIdentity(
        softwareBinnedSubpixelPresentationRevision_);
    return softwareBinnedSubpixelProxyPoints_;
}

const std::vector<CadSubpixelProxyPoint>&
CadRendererGL::subpixelProxyPresentationPoints(
        const CadFramePlan& plan, const SoGLContext* glue,
        const SbMatrix& viewProj)
{
    std::lock_guard<std::recursive_mutex> resourceLock(resourceMutex_);
    if (!ensureReady(glue))
        return plan.subpixelProxyPoints;
    return presentationSubpixelProxyPoints(plan, glue, viewProj);
}

void CadRendererGL::renderAggregateProxies(
        const CadFramePlan& plan, const SoGLContext* glue,
        const SbMatrix& viewProj, const SbMatrix& viewMatrix,
        const SbMatrix& projectionMatrix)
{
    const std::vector<CadSubpixelProxyPoint>& planPoints =
        presentationSubpixelProxyPoints(plan, glue, viewProj);
    const std::vector<CadSubpixelProxyPoint>& pressurePoints =
        pressureProxyPoints();
    const auto pointVisible = [](const CadSubpixelProxyPoint& point) {
        return !(point.flags & CadInstanceHidden);
    };
    const auto rasterPoint = [&](const CadSubpixelProxyPoint& point) {
        return pointVisible(point) &&
            point.shape == CadAggregateProxyShape::Point;
    };
    const auto rasterBox = [&](const CadSubpixelProxyPoint& point) {
        return pointVisible(point) &&
            point.shape == CadAggregateProxyShape::Box;
    };
    const bool includePlan = std::any_of(
        planPoints.begin(), planPoints.end(), pointVisible);
    const bool includePressure = std::any_of(
        pressurePoints.begin(), pressurePoints.end(), pointVisible);
    if (!includePlan && !includePressure)
        return;
    lastSubpixelProxyDrawPointCount_ = static_cast<size_t>(std::count_if(
        planPoints.begin(), planPoints.end(), rasterPoint)) +
        static_cast<size_t>(std::count_if(
            pressurePoints.begin(), pressurePoints.end(), rasterPoint));

    const Obol::CadDrawMode drawMode = activeViewState().drawMode;
    const bool drawBoxLines = Obol::cadDrawModeHasWire(drawMode);
    const bool drawBoxFaces = Obol::cadDrawModeHasShaded(drawMode);

    const bool useVbo = caps_.canUseVbo();
    const bool fixedFunction =
        (caps_.isSoftwareRenderer && !softwareGlslRequested()) ||
        !shaders_.proxyPoint || (drawBoxFaces && !shaders_.proxyShaded) ||
        !useVbo;
    GLfloat savedPointSize = 1.0f;
    glue->glGetFloatv(GL_POINT_SIZE, &savedPointSize);
    glue->glPointSize(1.0f);

    if (!useVbo) {
        glue->glMatrixMode(GL_PROJECTION);
        glue->glPushMatrix();
        glue->glLoadMatrixf(projectionMatrix[0]);
        glue->glMatrixMode(GL_MODELVIEW);
        glue->glPushMatrix();
        glue->glLoadMatrixf(viewMatrix[0]);
        const GLboolean wasLighting = glue->glIsEnabled(GL_LIGHTING);
        const GLboolean wasColorMaterial =
            glue->glIsEnabled(GL_COLOR_MATERIAL);
        GLint wasTwoSidedLighting = GL_FALSE;
        GLint savedShadeModel = GL_SMOOTH;
        glue->glGetIntegerv(
            GL_LIGHT_MODEL_TWO_SIDE, &wasTwoSidedLighting);
        glue->glGetIntegerv(GL_SHADE_MODEL, &savedShadeModel);
        glue->glDisable(GL_LIGHTING);
        glue->glBegin(GL_POINTS);
        uint64_t submittedPoints = 0;
        const std::vector<CadSubpixelProxyPoint> *proxyStreams[] = {
            &planPoints, &pressurePoints
        };
        for (const auto *stream : proxyStreams) {
            for (const CadSubpixelProxyPoint& point : *stream) {
                if (!rasterPoint(point))
                    continue;
                glue->glColor4ub(
                    point.rgba[0], point.rgba[1], point.rgba[2],
                    point.rgba[3]);
                glue->glVertex3f(
                    point.position[0], point.position[1], point.position[2]);
                submittedPoints = cadSaturatingWorkAdd(submittedPoints, 1);
            }
        }
        glue->glEnd();
        uint64_t submittedLineVertices = 0;
        if (drawBoxLines) {
            glue->glBegin(GL_LINES);
            for (const auto *stream : proxyStreams) {
                for (const CadSubpixelProxyPoint& point : *stream) {
                    if (!rasterBox(point))
                        continue;
                    glue->glColor4ub(
                        point.rgba[0], point.rgba[1], point.rgba[2],
                        point.rgba[3]);
                    cadForEachAggregateProxyBoxVertex(
                        point, [&](const SbVec3f& vertex) {
                        glue->glVertex3f(vertex[0], vertex[1], vertex[2]);
                        submittedLineVertices = cadSaturatingWorkAdd(
                            submittedLineVertices, 1);
                    });
                }
            }
            glue->glEnd();
        }
        uint64_t submittedTriangleVertices = 0;
        if (drawBoxFaces) {
            uploadFixedLights(glue);
            /* Every box triangle has one face normal and one instance color.
             * Flat shading is therefore visually identical while avoiding
             * smooth-fragment interpolation in software GL. */
            glue->glShadeModel(GL_FLAT);
            glue->glEnable(GL_LIGHTING);
            glue->glEnable(GL_COLOR_MATERIAL);
            glue->glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
            glue->glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
            glue->glBegin(GL_TRIANGLES);
            for (const auto *stream : proxyStreams) {
                for (const CadSubpixelProxyPoint& point : *stream) {
                    if (!rasterBox(point))
                        continue;
                    glue->glColor4ub(
                        point.rgba[0], point.rgba[1], point.rgba[2],
                        point.rgba[3]);
                    cadForEachAggregateProxyBoxTriangleVertex(
                        point,
                        [&](const SbVec3f& vertex, const SbVec3f& normal) {
                        glue->glNormal3f(normal[0], normal[1], normal[2]);
                        glue->glVertex3f(vertex[0], vertex[1], vertex[2]);
                        submittedTriangleVertices = cadSaturatingWorkAdd(
                            submittedTriangleVertices, 1);
                    });
                }
            }
            glue->glEnd();
        }
        /* Aggregate proxies are one batched primitive stream, not retained CAD
         * occurrences with per-instance draw overhead.  Count their vertex
         * work while deliberately leaving occurrenceCount unchanged. */
        lastRenderedWork_.positionCount = cadSaturatingWorkAdd(
            lastRenderedWork_.positionCount,
            cadSaturatingWorkAdd(submittedPoints,
                cadSaturatingWorkAdd(submittedLineVertices,
                    submittedTriangleVertices)));
        lastRenderedWork_.lineCount = cadSaturatingWorkAdd(
            lastRenderedWork_.lineCount, submittedLineVertices / 2u);
        lastRenderedWork_.triangleCount = cadSaturatingWorkAdd(
            lastRenderedWork_.triangleCount,
            submittedTriangleVertices / 3u);
        lastRenderedTriangleCount_ = cadSaturatingWorkAdd(
            lastRenderedTriangleCount_, submittedTriangleVertices / 3u);
        if (wasColorMaterial) glue->glEnable(GL_COLOR_MATERIAL);
        else glue->glDisable(GL_COLOR_MATERIAL);
        glue->glLightModeli(
            GL_LIGHT_MODEL_TWO_SIDE, wasTwoSidedLighting);
        glue->glShadeModel(savedShadeModel);
        if (wasLighting) glue->glEnable(GL_LIGHTING);
        else glue->glDisable(GL_LIGHTING);
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
            std::vector<float>& normals,
            std::vector<uint8_t>& colors,
            size_t& packedPointCount,
            size_t& packedLineVertexCount) {
        begin = std::min(begin, points.size());
        end = std::min(end, points.size());
        positions.clear();
        normals.clear();
        colors.clear();
        packedPointCount = 0;
        packedLineVertexCount = 0;
        const size_t recordCount = end - begin;
        positions.reserve(recordCount * 3u);
        colors.reserve(recordCount * 4u);
        for (size_t i = begin; i < end; ++i) {
            const CadSubpixelProxyPoint& point = points[i];
            if (!rasterPoint(point))
                continue;
            appendPackedPoint(positions, point.position);
            colors.insert(
                colors.end(), point.rgba.begin(), point.rgba.end());
            ++packedPointCount;
        }
        if (drawBoxLines) {
            for (size_t i = begin; i < end; ++i) {
                const CadSubpixelProxyPoint& point = points[i];
                if (!rasterBox(point))
                    continue;
                cadForEachAggregateProxyBoxVertex(
                    point, [&](const SbVec3f& vertex) {
                    appendPackedPoint(positions, vertex);
                    colors.insert(colors.end(), point.rgba.begin(),
                        point.rgba.end());
                    ++packedLineVertexCount;
                });
            }
        }
        if (drawBoxFaces) {
            for (size_t i = begin; i < end; ++i) {
                const CadSubpixelProxyPoint& point = points[i];
                if (!rasterBox(point))
                    continue;
                cadForEachAggregateProxyBoxTriangleVertex(
                    point, [&](const SbVec3f& vertex, const SbVec3f& normal) {
                    appendPackedPoint(positions, vertex);
                    appendPackedPoint(normals, normal);
                    colors.insert(colors.end(), point.rgba.begin(),
                        point.rgba.end());
                });
            }
        }
    };

    /*
     * Aggregate points retain stable slots across selection, style, and
     * visibility changes.  Keep every authorizing input exact so changed
     * colors, hidden flags, draw representation, or software binning cannot
     * collide with an older GPU batch.
     */
    CadSubpixelProxyStamp planUploadStamp;
    planUploadStamp.sourceRevision = plan.subpixelProxyRevision;
    planUploadStamp.attributeRevision = plan.instanceAttributeRevision;
    planUploadStamp.drawMode = drawMode;
    planUploadStamp.softwarePresentation =
        caps_.isSoftwareRenderer && softwareBinnedSubpixelValid_;
    if (planUploadStamp.softwarePresentation)
        planUploadStamp.presentationRevision =
            softwareBinnedSubpixelPresentationRevision_;

    CadSubpixelProxyStamp pressureUploadStamp;
    pressureUploadStamp.sourceRevision = pressureProxyRevision_;
    pressureUploadStamp.drawMode = drawMode;

    CadSubpixelProxyStamp pressureAppendBaseStamp = pressureUploadStamp;
    pressureAppendBaseStamp.sourceRevision =
        pressureProxyAppendBaseRevision_;

    const CadSubpixelProxyGpu& cachedPlan =
        gpuRes_->subpixelProxyPoints();
    if (includePlan &&
            (cachedPlan.stamp != planUploadStamp ||
             !cachedPlan.posBuf || !cachedPlan.colorBuf ||
             (drawBoxFaces && !cachedPlan.normBuf))) {
        std::vector<float> positions;
        std::vector<float> normals;
        std::vector<uint8_t> colors;
        size_t packedPointCount = 0;
        size_t packedLineVertexCount = 0;
        packPointRange(planPoints, 0, planPoints.size(), positions, normals,
            colors, packedPointCount, packedLineVertexCount);
        if (!positions.empty())
            gpuRes_->uploadSubpixelProxyBatch(
                planUploadStamp, positions, normals, colors,
                packedPointCount, packedLineVertexCount, glue, caps_);
    }

    const CadSubpixelProxyGpu& cachedPressure =
        gpuRes_->pressureProxyPoints();
    if (includePressure &&
            (cachedPressure.stamp != pressureUploadStamp ||
             !cachedPressure.posBuf || !cachedPressure.colorBuf ||
             (drawBoxFaces && !cachedPressure.normBuf))) {
        std::vector<float> positions;
        std::vector<float> normals;
        std::vector<uint8_t> colors;
        size_t packedPointCount = 0;
        size_t packedLineVertexCount = 0;
        bool appended = false;
        if (pressureProxyAppendOnly_ &&
                pressureProxyAppendBegin_ <= pressurePoints.size() &&
                pressureProxyAppendBegin_ <=
                    static_cast<size_t>(
                        std::numeric_limits<GLsizei>::max()) &&
                cachedPressure.stamp == pressureAppendBaseStamp &&
                cachedPressure.count ==
                    static_cast<GLsizei>(
                        pressureProxyAppendBegin_)) {
            packPointRange(
                pressurePoints, pressureProxyAppendBegin_,
                pressurePoints.size(), positions, normals, colors,
                packedPointCount, packedLineVertexCount);
            /*
             * A hidden tail would make packed GPU slots diverge from source
             * slots, so conservatively refresh the complete stream in that
             * uncommon presentation-update case.
             */
            if (positions.size() / 3u ==
                    pressurePoints.size() -
                        pressureProxyAppendBegin_) {
                appended = gpuRes_->appendPressureProxyPoints(
                    pressureAppendBaseStamp,
                    pressureUploadStamp,
                    positions, colors, glue);
            }
        }
        if (!appended) {
            packPointRange(
                pressurePoints, 0, pressurePoints.size(),
                positions, normals, colors, packedPointCount,
                packedLineVertexCount);
            if (!positions.empty())
                gpuRes_->uploadPressureProxyBatch(
                    pressureUploadStamp, positions, normals, colors,
                    packedPointCount, packedLineVertexCount, glue, caps_);
        }
        if (gpuRes_->pressureProxyPoints().stamp == pressureUploadStamp)
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
        glue->glLoadMatrixf(projectionMatrix[0]);
        glue->glMatrixMode(GL_MODELVIEW);
        glue->glPushMatrix();
        glue->glLoadMatrixf(viewMatrix[0]);
        const GLboolean wasLighting = glue->glIsEnabled(GL_LIGHTING);
        const GLboolean wasColorMaterial =
            glue->glIsEnabled(GL_COLOR_MATERIAL);
        GLint wasTwoSidedLighting = GL_FALSE;
        GLint savedShadeModel = GL_SMOOTH;
        glue->glGetIntegerv(
            GL_LIGHT_MODEL_TWO_SIDE, &wasTwoSidedLighting);
        glue->glGetIntegerv(GL_SHADE_MODEL, &savedShadeModel);
        glue->glDisable(GL_LIGHTING);
        configureFixedClientArrays(glue, false, true);
        for (size_t i = 0; i < streamCount; ++i) {
            const CadSubpixelProxyGpu& gpu = *streams[i];
            glue->glBindBuffer(GL_ARRAY_BUFFER, gpu.posBuf);
            glue->glVertexPointer(
                3, GL_FLOAT, 3 * sizeof(float), nullptr);
            glue->glBindBuffer(GL_ARRAY_BUFFER, gpu.colorBuf);
            glue->glColorPointer(
                4, GL_UNSIGNED_BYTE, 4 * sizeof(uint8_t), nullptr);
            if (gpu.pointCount > 0)
                glue->glDrawArrays(GL_POINTS, 0, gpu.pointCount);
            if (gpu.lineVertexCount > 0)
                glue->glDrawArrays(
                    GL_LINES, gpu.pointCount, gpu.lineVertexCount);
            if (gpu.triangleVertexCount > 0 && gpu.normBuf) {
                const GLsizei triangleFirst =
                    gpu.pointCount + gpu.lineVertexCount;
                uploadFixedLights(glue);
                glue->glEnable(GL_LIGHTING);
                glue->glEnable(GL_COLOR_MATERIAL);
                glue->glColorMaterial(
                    GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
                glue->glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
                glue->glShadeModel(GL_FLAT);
                configureFixedClientArrays(glue, true, true);
                glue->glBindBuffer(GL_ARRAY_BUFFER, gpu.posBuf);
                glue->glVertexPointer(
                    3, GL_FLOAT, 3 * sizeof(float),
                    reinterpret_cast<const void *>(
                        static_cast<uintptr_t>(triangleFirst) *
                        3u * sizeof(float)));
                glue->glBindBuffer(GL_ARRAY_BUFFER, gpu.normBuf);
                glue->glNormalPointer(
                    GL_FLOAT, 3 * sizeof(float), nullptr);
                glue->glBindBuffer(GL_ARRAY_BUFFER, gpu.colorBuf);
                glue->glColorPointer(
                    4, GL_UNSIGNED_BYTE, 4 * sizeof(uint8_t),
                    reinterpret_cast<const void *>(
                        static_cast<uintptr_t>(triangleFirst) *
                        4u * sizeof(uint8_t)));
                glue->glDrawArrays(
                    GL_TRIANGLES, 0, gpu.triangleVertexCount);
                glue->glDisable(GL_LIGHTING);
                configureFixedClientArrays(glue, false, true);
            }
            lastRenderedWork_.positionCount = cadSaturatingWorkAdd(
                lastRenderedWork_.positionCount,
                static_cast<uint64_t>(gpu.count));
            lastRenderedWork_.lineCount = cadSaturatingWorkAdd(
                lastRenderedWork_.lineCount,
                static_cast<uint64_t>(gpu.lineVertexCount / 2));
            lastRenderedWork_.triangleCount = cadSaturatingWorkAdd(
                lastRenderedWork_.triangleCount,
                static_cast<uint64_t>(gpu.triangleVertexCount / 3));
            lastRenderedTriangleCount_ = cadSaturatingWorkAdd(
                lastRenderedTriangleCount_,
                static_cast<uint64_t>(gpu.triangleVertexCount / 3));
        }
        glue->glDisableClientState(GL_NORMAL_ARRAY);
        glue->glDisableClientState(GL_COLOR_ARRAY);
        glue->glDisableClientState(GL_VERTEX_ARRAY);
        glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
        if (wasColorMaterial) glue->glEnable(GL_COLOR_MATERIAL);
        else glue->glDisable(GL_COLOR_MATERIAL);
        glue->glLightModeli(
            GL_LIGHT_MODEL_TWO_SIDE, wasTwoSidedLighting);
        glue->glShadeModel(savedShadeModel);
        if (wasLighting) glue->glEnable(GL_LIGHTING);
        else glue->glDisable(GL_LIGHTING);
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
            if (gpu.pointCount > 0)
                glue->glDrawArrays(GL_POINTS, 0, gpu.pointCount);
            if (gpu.lineVertexCount > 0)
                glue->glDrawArrays(
                    GL_LINES, gpu.pointCount, gpu.lineVertexCount);
            lastRenderedWork_.positionCount = cadSaturatingWorkAdd(
                lastRenderedWork_.positionCount,
                static_cast<uint64_t>(gpu.count));
            lastRenderedWork_.lineCount = cadSaturatingWorkAdd(
                lastRenderedWork_.lineCount,
                static_cast<uint64_t>(gpu.lineVertexCount / 2));
            if (gpu.vao && glue->glBindVertexArray) {
                glue->glBindVertexArray(0);
            } else {
                glue->glDisableVertexAttribArrayARB(1);
                glue->glDisableVertexAttribArrayARB(0);
                glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
            }
        }
        glue->glUseProgramObjectARB(0);
        if (drawBoxFaces) {
            glue->glUseProgramObjectARB(shaders_.proxyShaded);
            const GLint shadedViewProjection =
                glue->glGetUniformLocationARB(
                    shaders_.proxyShaded, "u_viewProj");
            glue->glUniformMatrix4fvARB(
                shadedViewProjection, 1, GL_FALSE, viewProj[0]);
            glue->glUniform1iARB(
                glue->glGetUniformLocationARB(
                    shaders_.proxyShaded, "u_hasNorm"), 1);
            uploadLights(glue, shaders_.proxyShaded);
            uploadAmbientLight(glue, shaders_.proxyShaded);
            for (size_t i = 0; i < streamCount; ++i) {
                const CadSubpixelProxyGpu& gpu = *streams[i];
                if (gpu.triangleVertexCount <= 0 || !gpu.normBuf)
                    continue;
                const GLsizei triangleFirst =
                    gpu.pointCount + gpu.lineVertexCount;
                glue->glBindBuffer(GL_ARRAY_BUFFER, gpu.posBuf);
                glue->glVertexAttribPointerARB(
                    0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float),
                    reinterpret_cast<const void *>(
                        static_cast<uintptr_t>(triangleFirst) *
                        3u * sizeof(float)));
                glue->glEnableVertexAttribArrayARB(0);
                glue->glBindBuffer(GL_ARRAY_BUFFER, gpu.normBuf);
                glue->glVertexAttribPointerARB(
                    1, 3, GL_FLOAT, GL_FALSE,
                    3 * sizeof(float), nullptr);
                glue->glEnableVertexAttribArrayARB(1);
                glue->glBindBuffer(GL_ARRAY_BUFFER, gpu.colorBuf);
                glue->glVertexAttribPointerARB(
                    2, 4, GL_UNSIGNED_BYTE, GL_TRUE,
                    4 * sizeof(uint8_t),
                    reinterpret_cast<const void *>(
                        static_cast<uintptr_t>(triangleFirst) *
                        4u * sizeof(uint8_t)));
                glue->glEnableVertexAttribArrayARB(2);
                glue->glDrawArrays(
                    GL_TRIANGLES, 0, gpu.triangleVertexCount);
                lastRenderedWork_.triangleCount = cadSaturatingWorkAdd(
                    lastRenderedWork_.triangleCount,
                    static_cast<uint64_t>(gpu.triangleVertexCount / 3));
                lastRenderedTriangleCount_ = cadSaturatingWorkAdd(
                    lastRenderedTriangleCount_,
                    static_cast<uint64_t>(gpu.triangleVertexCount / 3));
                glue->glDisableVertexAttribArrayARB(2);
                glue->glDisableVertexAttribArrayARB(1);
                glue->glDisableVertexAttribArrayARB(0);
            }
            glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
            glue->glUseProgramObjectARB(0);
        }
    }
    glue->glPointSize(savedPointSize);
}

void CadRendererGL::completeDirectSoftwareWireFrame(
        const Obol::CadRenderedWork& work, uint32_t contextId,
        size_t subpixelProxyDrawPointCount)
{
    std::lock_guard<std::recursive_mutex> resourceLock(resourceMutex_);
    /*
     * Direct software rasterization is an executor, not a side channel.  In
     * particular, publish the same exact-work and completed-resource-frame
     * contract as render().  Leaving the preceding renderer record in place
     * made the host observe zero/stale work and continuously resubmit an LoD
     * request even though the software framebuffer had been completed.
     */
    pressureProxyPointsView_ = nullptr;
    pressureProxyPoints_.clear();
    atlasAdmissionPressure_ = false;
    lastRenderedTriangleCount_ = 0;
    lastSubpixelProxyDrawPointCount_ = subpixelProxyDrawPointCount;
    lastRenderedWork_ = work;
    lastRenderedWork_.exact = true;
    lastRenderTier_ = 0;
    lastIndirectStatus_ = -1;
    lastRenderUsedPreparedReplay_ = false;

    Obol::CadGpuResourceSnapshot snapshot;
    const auto resources = gpuResources_.find(contextId);
    if (resources != gpuResources_.end() && resources->second)
        snapshot = resources->second->resourceSnapshot();
    Obol::internal::cadAdvanceIdentity(completedResourceFrameSerial_);
    snapshot.frameSerial = completedResourceFrameSerial_;
    snapshot.pressureProxyCount = 0;
    snapshot.atlasAdmissionPressure = false;
    lastGpuResourceSnapshot_ = snapshot;
}

void CadRendererGL::render(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const Obol::CadViewState& viewState,
        SoGLRenderAction*    action,
        const SoGLContext*   glue,
        const SbMatrix&      viewProj,
        const SbMatrix&      assemblyModel,
        const SbMatrix&      viewMatrix,
        const SbMatrix&      projectionMatrix,
        const SbViewVolume&  viewVolume,
        bool                  fixedFunctionClipPlanes,
        const std::unordered_map<PartId, uint64_t,
                                 std::hash<PartId>>& partGenMap)
{
    std::lock_guard<std::recursive_mutex> resourceLock(resourceMutex_);
    assemblyModel_ = assemblyModel;
    activeRenderInterrupted_ = false;
    const Obol::CadViewState *previousViewState = activeViewState_;
    activeViewState_ = &viewState;
    struct RestoreActiveViewState {
        const Obol::CadViewState *&slot;
        const Obol::CadViewState *previous;
        ~RestoreActiveViewState() { slot = previous; }
    } restoreActiveViewState{activeViewState_, previousViewState};
    SoGLRenderAction *previousAction = activeRenderAction_;
    activeRenderAction_ = action;
    struct RestoreActiveRenderAction {
        SoGLRenderAction *&slot;
        SoGLRenderAction *previous;
        ~RestoreActiveRenderAction() { slot = previous; }
    } restoreActiveRenderAction{activeRenderAction_, previousAction};

    using RenderClock = std::chrono::steady_clock;
    const CadRendererConfiguration& configuration = *configuration_;
    const bool renderTimingEnabled = configuration.renderTiming;
    const auto renderStarted = renderTimingEnabled ?
        RenderClock::now() : RenderClock::time_point();
    // Pressure proxies describe only the current camera and atlas admission
    // result.  Never let an early return expose a prior frame's replacements.
    pressureProxyPointsView_ = nullptr;
    pressureProxyPoints_.clear();
    lastRenderedTriangleCount_ = 0;
    lastSubpixelProxyDrawPointCount_ = 0;
    lastRenderedWork_ = Obol::CadRenderedWork();
    lastRenderedWork_.viewState = viewState;
    lastRenderUsedPreparedReplay_ = false;
    atlasAdmissionPressure_ = false;
    if (!gpuRes_ || gpuContextId_ != glue->contextid ||
            !capsDetected_ || shadersContextId_ != glue->contextid)
        noteRenderPreparation("renderer-initialization");
    if (!ensureReady(glue)) return;
    const bool forceFixedClipPlanes = fixedFunctionClipPlanes &&
        caps_.canUseFixedVbo();
    const auto publishResourceSnapshot = [&]() {
        Obol::CadGpuResourceSnapshot snapshot =
            gpuRes_->resourceSnapshot();
        Obol::internal::cadAdvanceIdentity(completedResourceFrameSerial_);
        snapshot.frameSerial = completedResourceFrameSerial_;
        snapshot.pressureProxyCount = lastPressureProxyCount();
        snapshot.atlasAdmissionPressure = atlasAdmissionPressure_ ||
            snapshot.pressureProxyCount > 0;
        lastGpuResourceSnapshot_ = snapshot;
    };
    if (plan.visibleInstances.empty()) {
        /* Empty is a complete presentation, not an interrupted one.  Hosts
         * use the exact bit to decide whether an offscreen buffer may replace
         * the preceding completed frame. */
        lastRenderedWork_.viewState = viewState;
        lastRenderedWork_.exact = true;
        lastRenderTier_ = 0;
        publishResourceSnapshot();
        return;
    }
    CadGpuTimerGuard gpuTimer(
        gpuRes_, glue, &lastRenderedTriangleCount_,
        viewState.pointProxyPixelThreshold);
    CadGLStateValidationGuard validateState(
        glue, configuration.validateGlState,
        caps_.compatibilityProfile, caps_.hasVBO,
        caps_.hasVAO && glue->glBindVertexArray != nullptr,
        caps_.hasMultiDrawIndirect);
    gpuRes_->beginProgressiveFrame();
    gpuRes_->beginTriangleAtlasFrame();
    bool frameResourcesOpen = true;
    const auto finishFrameResources = [&](bool publish) {
        if (!frameResourcesOpen)
            return;
        gpuRes_->endTriangleAtlasFrame(glue);
        gpuRes_->endProgressiveFrame(glue);
        frameResourcesOpen = false;
        if (publish) {
            lastRenderedWork_.viewState = viewState;
            lastRenderedWork_.exact = true;
            publishResourceSnapshot();
        }
    };
    const auto finishInterruptedFrame = [&]() {
        if (!renderInterrupted())
            return false;
        finishFrameResources(false);
        return true;
    };

    CadDirectGLStateGuard directState(
        glue, caps_.hasVBO,
        caps_.hasVAO && glue->glBindVertexArray != nullptr,
        caps_.hasMultiDrawIndirect,
        caps_.compatibilityProfile &&
            (caps_.isSoftwareRenderer || !caps_.canUseVbo()));
    if (!directState.valid()) {
        atlasAdmissionPressure_ = true;
        activeRenderInterrupted_ = true;
        finishFrameResources(false);
        return;
    }

    // SoCADAssembly has synchronized Coin's lazy shape state before entering
    // this direct-GL renderer.  Keep all internal cull changes local and
    // restore the raw state on every return path.
    CadCullRasterGuard cullRaster(glue);

    const bool flatShadedEnabled = configuration.flatShaded;
    const bool canUseFlatShaded = caps_.isSoftwareRenderer ?
        caps_.canUseFixedVbo() : (caps_.canUseVbo() && shaders_.shaded);
    const bool hiddenLine =
        viewState.drawMode == Obol::CadDrawMode::HiddenLine;
    const bool retainedProgressive = assembly.hasProgressivePartLod();
    bool adaptiveShadedRanges = false;
    for (const CadDrawItem& item : plan.shadedItems) {
        if (item.partIndex >= plan.partBindings.size())
            continue;
        const CadPartBinding& binding = plan.partBindings[item.partIndex];
        if (binding.geometry && binding.geometry->shaded &&
                binding.geometry->shaded->hasAdaptiveProgressiveClusters()) {
            adaptiveShadedRanges = true;
            break;
        }
    }
    bool adaptiveWireRanges = false;
    for (const CadDrawItem& item : plan.wireItems) {
        if (item.partIndex >= plan.partBindings.size())
            continue;
        const CadPartBinding& binding = plan.partBindings[item.partIndex];
        if (binding.geometry && binding.geometry->wire &&
                binding.geometry->wire->hasAdaptiveProgressiveClusters()) {
            adaptiveWireRanges = true;
            break;
        }
    }
    const bool progressiveWireShaderReady =
        !retainedProgressive || shaders_.wirePop;
    const bool progressiveShadedShaderReady =
        !retainedProgressive || shaders_.shadedPop;
    const bool progressiveWireInstShaderReady =
        !retainedProgressive || shaders_.wirePopInst;
    const bool progressiveShadedInstShaderReady =
        !retainedProgressive || shaders_.shadedPopInst;
    const bool useFlatShaded = !forceFixedClipPlanes &&
        flatShadedEnabled && canUseFlatShaded &&
        ((caps_.isSoftwareRenderer && adaptiveShadedRanges) ||
         (!adaptiveShadedRanges &&
          (hiddenLine || plan.shadedItems.size() >= 128)));

    try {
    renderPoints(plan, assembly, glue, viewProj, partGenMap);
    if (finishInterruptedFrame()) {
        return;
    }
    const auto pointsCompleted = renderTimingEnabled ?
        RenderClock::now() : RenderClock::time_point();

    bool indexedTriangleWire = false;
    bool explicitWire = false;
    for (const CadDrawItem& item : plan.wireItems) {
        indexedTriangleWire = indexedTriangleWire ||
            item.rep.type == CadRepType::Triangles;
        explicitWire = explicitWire ||
            item.rep.type == CadRepType::WireSegments;
    }
    if (!forceFixedClipPlanes &&
            viewState.drawMode == Obol::CadDrawMode::Wireframe &&
            indexedTriangleWire) {
        if (!renderIndexedTriangleWire(plan, assembly, glue, viewProj,
                viewMatrix, projectionMatrix)) {
            finishFrameResources(false);
            return;
        }
        if (finishInterruptedFrame())
            return;
        if (!explicitWire) {
            lastRenderTier_ = 1;
            renderAggregateProxies(
                plan, glue, viewProj, viewMatrix, projectionMatrix);
            if (finishInterruptedFrame())
                return;
            finishFrameResources(true);
            return;
        }
    }

    if (hiddenLine) {
        if (useFlatShaded) {
            const uint64_t workTriangleSnapshot =
                lastRenderedTriangleCount_;
            const Obol::CadRenderedWork workSnapshot =
                lastRenderedWork_;
            SoGLContext_glColorMask(glue, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            SoGLContext_glEnable(glue, GL_POLYGON_OFFSET_FILL);
            SoGLContext_glPolygonOffset(glue, 1.0f, 1.0f);
            const bool depthRendered = renderFlatShaded(
                plan, glue, viewProj, viewMatrix, projectionMatrix,
                true);
            SoGLContext_glDisable(glue, GL_POLYGON_OFFSET_FILL);
            SoGLContext_glColorMask(glue, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            if (finishInterruptedFrame())
                return;
            const bool triangleEdgesRendered = depthRendered &&
                renderFlatTriangleEdges(plan, glue, viewProj, viewMatrix,
                                        projectionMatrix);
            if (finishInterruptedFrame())
                return;
            const bool explicitWireRendered = plan.wireItems.empty() ||
                renderFlatWire(plan, glue, viewProj);
            if (finishInterruptedFrame())
                return;
            if (triangleEdgesRendered && explicitWireRendered) {
                lastRenderTier_ = 3;
                renderAggregateProxies(
                    plan, glue, viewProj, viewMatrix, projectionMatrix);
                finishFrameResources(true);
                return;
            }
            lastRenderedTriangleCount_ = workTriangleSnapshot;
            lastRenderedWork_ = workSnapshot;
        }

        // The per-part fallback needs retained representations.
        for (const auto& repKey : plan.requiredReps) {
            if (finishInterruptedFrame()) {
                return;
            }
            if (repKey.type == CadRepType::WireSegments &&
                    !wireRepHasUncollapsedInstances(plan, repKey.part))
                continue;
            auto genIt = partGenMap.find(repKey.part);
            uint64_t gen = (genIt != partGenMap.end()) ? genIt->second : 0;
            ensurePartUploaded(
                repKey.part, assembly, gen,
                maximumRequestedCut(
                    plan, activeViewState(), repKey.part), glue);
        }
        SoGLContext_glColorMask(glue, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        SoGLContext_glEnable(glue, GL_POLYGON_OFFSET_FILL);
        SoGLContext_glPolygonOffset(glue, 1.0f, 1.0f);
        if (!forceFixedClipPlanes && caps_.canUseVbo() && shaders_.shaded &&
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
        if (finishInterruptedFrame())
            return;

        if (!forceFixedClipPlanes && !adaptiveWireRanges &&
                caps_.canUseInstanced() && shaders_.wireInst &&
                progressiveWireInstShaderReady) {
            lastRenderTier_ = 2;
            renderInstanced(plan, assembly, glue, viewProj, viewVolume,
                            partGenMap,
                            true, false, false);
        } else if (!forceFixedClipPlanes && caps_.canUseVbo() && shaders_.wire &&
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
        if (finishInterruptedFrame())
            return;
        renderAggregateProxies(
            plan, glue, viewProj, viewMatrix, projectionMatrix);
        if (finishInterruptedFrame())
            return;
        finishFrameResources(true);
        return;
    }

    const bool indirectEnabled = !forceFixedClipPlanes &&
        configuration.indirect;
    if (!indirectStatusReported_ &&
            configuration.indirectDebug &&
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
        !adaptiveShadedRanges && indirectEnabled &&
        plan.shadedItems.size() >= 128 &&
        renderIndirectShaded(
            plan, glue, viewProj, viewVolume);
    if (finishInterruptedFrame()) {
        if (!indirectPolygonOffsetWasEnabled)
            SoGLContext_glDisable(glue, GL_POLYGON_OFFSET_FILL);
        return;
    }
    if (!indirectPolygonOffsetWasEnabled)
        SoGLContext_glDisable(glue, GL_POLYGON_OFFSET_FILL);
    const auto shadedCompleted = renderTimingEnabled ?
        RenderClock::now() : RenderClock::time_point();
    if (indirectShadedRendered) {
        bool wireRendered = plan.wireItems.empty();
        if (!wireRendered) {
            const bool flatWireEnabled = configuration.flatWire;
            const bool canUseFlatWire = caps_.isSoftwareRenderer ?
                caps_.canUseFixedVbo() :
                (caps_.canUseVbo() && shaders_.wire);
            size_t wireInstanceCount = 0;
            for (const CadDrawItem& item : plan.wireItems)
                wireInstanceCount +=
                    cadDrawableInstanceCount(
                        plan, item, CadDrawChannel::Wire);
            const bool preferFlatWire = caps_.isSoftwareRenderer ?
                (wireInstanceCount >= 128 &&
                 wireInstanceCount <= maximumSoftwareFlatWireInstances) :
                plan.wireItems.size() >= 128;
            wireRendered = !adaptiveWireRanges &&
                flatWireEnabled && preferFlatWire &&
                canUseFlatWire &&
                renderFlatWire(plan, glue, viewProj);
            if (finishInterruptedFrame())
                return;
        }
        if (!wireRendered) {
            for (const CadRepKey& repKey : plan.requiredReps) {
                if (finishInterruptedFrame()) {
                    return;
                }
                if (repKey.type != CadRepType::WireSegments ||
                        !wireRepHasUncollapsedInstances(
                            plan, repKey.part))
                    continue;
                const auto generation = partGenMap.find(repKey.part);
                ensurePartUploaded(
                    repKey.part, assembly,
                    generation == partGenMap.end() ?
                        0 : generation->second,
                    maximumRequestedCut(
                        plan, activeViewState(), repKey.part),
                    glue);
            }
            if (!adaptiveWireRanges && caps_.canUseInstanced() && shaders_.wireInst &&
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
            if (finishInterruptedFrame())
                return;
        }
        const auto wireCompleted = renderTimingEnabled ?
            RenderClock::now() : RenderClock::time_point();
        lastRenderTier_ = 6;
        renderAggregateProxies(
            plan, glue, viewProj, viewMatrix, projectionMatrix);
        if (finishInterruptedFrame())
            return;
        finishFrameResources(true);
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
                    "proxies=%zu\n",
                    total,
                    milliseconds(renderStarted, pointsCompleted),
                    milliseconds(pointsCompleted, shadedCompleted),
                    milliseconds(shadedCompleted, wireCompleted),
                    milliseconds(wireCompleted, completed),
                    plan.visibleInstances.size(),
                    plan.wireItems.size(),
                    plan.subpixelProxyPoints.size() +
                        pressureProxyPoints().size());
        }
        return;
    }

    bool flatShadedRendered = false;
    const uint64_t flatTriangleSnapshot =
        lastRenderedTriangleCount_;
    const Obol::CadRenderedWork flatWorkSnapshot =
        lastRenderedWork_;
    if (useFlatShaded) {
        const GLboolean polygonOffsetWasEnabled =
            glue->glIsEnabled(GL_POLYGON_OFFSET_FILL);
        if (!polygonOffsetWasEnabled) {
            SoGLContext_glEnable(glue, GL_POLYGON_OFFSET_FILL);
            SoGLContext_glPolygonOffset(glue, 1.0f, 1.0f);
        }
        flatShadedRendered = renderFlatShaded(
            plan, glue, viewProj, viewMatrix, projectionMatrix,
            false);
        if (!polygonOffsetWasEnabled)
            SoGLContext_glDisable(glue, GL_POLYGON_OFFSET_FILL);
        if (finishInterruptedFrame())
            return;
    }
    if (flatShadedRendered) {
        const bool flatWireCompleted = plan.wireItems.empty() ||
            renderFlatWire(plan, glue, viewProj);
        if (finishInterruptedFrame())
            return;
        if (flatWireCompleted) {
            lastRenderTier_ = 4;
            renderAggregateProxies(
                plan, glue, viewProj, viewMatrix, projectionMatrix);
            if (finishInterruptedFrame())
                return;
            finishFrameResources(true);
            return;
        }
    }
    lastRenderedTriangleCount_ = flatTriangleSnapshot;
    lastRenderedWork_ = flatWorkSnapshot;

    /*
     * Wire and shaded work are independent presentation channels.  In
     * particular, thousands of conservative fallback boxes must remain one
     * flat world-space batch even after the first few retained PoP meshes
     * appear in the shaded channel.  The old all-or-nothing gate disabled
     * this path as soon as any progressive part existed, turning a 50k-leaf
     * cold scene back into tens of thousands of VBO uploads and draw calls.
     */
    const bool flatWireEnabled = configuration.flatWire;
    const bool canUseFlatWire = caps_.isSoftwareRenderer ?
        caps_.canUseFixedVbo() : (caps_.canUseVbo() && shaders_.wire);
    size_t wireInstanceCount = 0;
    for (const CadDrawItem& item : plan.wireItems)
        wireInstanceCount += cadDrawableInstanceCount(
            plan, item, CadDrawChannel::Wire);
    /*
     * Hardware should prefer true instancing when many AABBs share the unit
     * cube part.  OSMesa's fixed-function software path is faster with one
     * flattened world-space buffer, so its threshold is occurrence count
     * rather than unique-part count.
     */
    const bool preferFlatWire = caps_.isSoftwareRenderer ?
        (wireInstanceCount >= 128 &&
         wireInstanceCount <= maximumSoftwareFlatWireInstances) :
        plan.wireItems.size() >= 128;
    const bool flatWireRendered = !forceFixedClipPlanes &&
        !adaptiveWireRanges && flatWireEnabled &&
        preferFlatWire && canUseFlatWire &&
        renderFlatWire(plan, glue, viewProj);
    if (finishInterruptedFrame())
        return;

    // Upload only the representations still needed by the per-part paths.
    for (const auto& repKey : plan.requiredReps) {
        if (finishInterruptedFrame()) {
            return;
        }
        if (repKey.type == CadRepType::WireSegments &&
                (flatWireRendered ||
                 !wireRepHasUncollapsedInstances(plan, repKey.part)))
            continue;
        auto genIt = partGenMap.find(repKey.part);
        uint64_t gen = (genIt != partGenMap.end()) ? genIt->second : 0;
        ensurePartUploaded(
            repKey.part, assembly, gen,
            maximumRequestedCut(
                plan, activeViewState(), repKey.part), glue);
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
    } else if (forceFixedClipPlanes ||
            (caps_.isSoftwareRenderer && !softwareGlslRequested() &&
             caps_.canUseFixedVbo())) {
        lastRenderTier_ = 1;
        renderFixedVboLoop(plan, assembly, glue, viewProj, viewMatrix,
                           projectionMatrix, !flatWireRendered, true);
    } else if (!adaptiveShadedRanges && !adaptiveWireRanges &&
            caps_.canUseInstanced() &&
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
    if (finishInterruptedFrame())
        return;
    renderAggregateProxies(
        plan, glue, viewProj, viewMatrix, projectionMatrix);
    if (finishInterruptedFrame())
        return;
    finishFrameResources(true);
    } catch (const std::bad_alloc &) {
        /* CPU staging and renderer bookkeeping are admitted work just like
         * GPU buffers.  Process-address or backend pressure must reject this
         * candidate frame atomically, never escape through Coin and terminate
         * the application.  The owner retains its preceding completed image;
         * the normal interrupted-frame feedback selects a cheaper successor. */
        atlasAdmissionPressure_ = true;
        activeRenderInterrupted_ = true;
        finishFrameResources(false);
    }
}


// ---------------------------------------------------------------------------
// releaseGpuResources()
// ---------------------------------------------------------------------------

void CadRendererGL::releaseGpuResources(const SoGLContext* glue)
{
    std::lock_guard<std::recursive_mutex> resourceLock(resourceMutex_);
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
