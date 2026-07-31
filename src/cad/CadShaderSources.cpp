/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the conditions in COPYING are
 * met.
\**************************************************************************/

#include "CadShaderSources.h"

namespace Obol {
namespace internal {

// GLSL shader sources – Tier 1 (GL 2.0 / GLSL 1.10, no #version directive)
// ---------------------------------------------------------------------------

// Wire pass: no lighting, colour from uniform
const char * const kWireVS1 =
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

const char * const kWirePopVS1 =
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

const char * const kWireFS1 =
    "varying vec4 v_color;\n"
    "void main() {\n"
    "    gl_FragColor = v_color;\n"
    "}\n";

// View-local proxy points carry their own per-occurrence colour so thousands
// of differently coloured AABB/OBB replacements remain one draw call.
const char * const kProxyPointVS1 =
    "attribute vec3 a_pos;\n"
    "attribute vec4 a_color;\n"
    "uniform mat4 u_viewProj;\n"
    "varying vec4 v_color;\n"
    "void main() {\n"
    "    gl_Position = u_viewProj * vec4(a_pos, 1.0);\n"
    "    v_color = a_color;\n"
    "}\n";

// Shaded pass: multi-light (directional/point/spot) in world space
const char * const kShadedVS1 =
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

const char * const kShadedPopVS1 =
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
const char * const kShadedFaceVS1 =
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

const char * const kShadedPopFaceVS1 =
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
const char * const kShadedFS1 =
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
const char * const kShadedDirectionalNormFS1 =
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

const char * const kShadedDirectionalFaceFS1 =
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

const char * const kShadedFaceDebugFS1 =
    "void main() {\n"
    "    gl_FragColor = gl_FrontFacing ?\n"
    "        vec4(1.0, 0.0, 0.0, 1.0) : vec4(0.0, 0.0, 1.0, 1.0);\n"
    "}\n";

const char * const kShadedNormalDebugFS1 =
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

const char * const kWireVS2 =
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

const char * const kWirePopVS2 =
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

const char * const kWireFS2 =
    "#version 140\n"
    "in  vec4 v_color;\n"
    "out vec4 fragColor;\n"
    "void main() { fragColor = v_color; }\n";

const char * const kShadedVS2 =
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

const char * const kShadedPopVS2 =
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

const char * const kShadedFS2 =
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
const char * const kShadedIndirectVS2 =
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

const char * const kShadedIndirectFS2 =
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


} // namespace internal
} // namespace Obol
