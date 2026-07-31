/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the conditions in COPYING are
 * met.
\**************************************************************************/

#include "CadRendererGL.h"
#include "CadShaderSources.h"

#include <Inventor/system/gl.h>
#include "glue/glp.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace Obol {
namespace internal {

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

} // namespace internal
} // namespace Obol
