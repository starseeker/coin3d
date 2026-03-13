/*
 * vulkan_context_manager.h
 *
 * SoVulkanContextManager – a SoDB::ContextManager that uses the Vulkan API
 * for hardware-accelerated offscreen rasterisation.
 *
 * Scene collection is delegated to SoRaytracerSceneCollector (the same
 * infrastructure used by SoNanoRTContextManager and SoEmbreeContextManager).
 * Per-vertex Phong lighting is pre-computed on the CPU from the collected
 * light and material data; the GPU only performs vertex transformation,
 * rasterisation, and depth testing.
 *
 * Coordinate-system notes:
 *   Vulkan NDC has y+ pointing downward, while the OpenGL convention used by
 *   SbViewVolume has y+ pointing upward.  Using the raw OpenGL view-projection
 *   matrix in Vulkan causes the image to appear vertically flipped in the
 *   framebuffer.  SoOffscreenRenderer::getBuffer() returns pixels in
 *   bottom-to-top (OpenGL) row order, so this "double inversion" results in
 *   correct orientation: no explicit row flip is required.  The depth range is
 *   remapped from OpenGL's [-1,1] to Vulkan's [0,1] by post-multiplying a
 *   small correction matrix before passing the combined MVP to the shader.
 *
 * Usage:
 *
 *   #include "vulkan_context_manager.h"
 *
 *   static SoVulkanContextManager vk_mgr;
 *   SoDB::init(&vk_mgr);
 *   SoNodeKit::init();
 *   SoInteraction::init();
 *
 *   SbViewportRegion vp(800, 600);
 *   SoOffscreenRenderer renderer(vp);
 *   renderer.setComponents(SoOffscreenRenderer::RGB);
 *   renderer.render(root);
 *   renderer.writeToRGB("out.rgb");
 *
 * Requires:
 *   - Vulkan 1.0 loader (libvulkan.so / vulkan-1.dll) at link time
 *   - At least one Vulkan physical device at run time (hardware GPU or
 *     software renderer such as Mesa's lavapipe / llvmpipe)
 *   - If no device is found, renderScene() logs a warning once and returns
 *     FALSE so that SoOffscreenRenderer can fall back to the GL path.
 */

#ifndef OBOL_VULKAN_CONTEXT_MANAGER_H
#define OBOL_VULKAN_CONTEXT_MANAGER_H

// ---- Obol generic raytracing infrastructure ---------------------------------
#include <Inventor/SoRaytracerSceneCollector.h>
#include <Inventor/SoDB.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbViewVolume.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/nodes/SoCamera.h>
#include <Inventor/nodes/SoRaytracingParams.h>

// ---- Vulkan -----------------------------------------------------------------
#include <vulkan/vulkan.h>

// ---- Standard library -------------------------------------------------------
#include <vector>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <algorithm>

// ==========================================================================
// Math helpers
// ==========================================================================

static inline float vk_clamp01(float v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}
static inline float vk_dot3(const float * a, const float * b) {
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}
static inline void vk_normalize3(float v[3]) {
    const float len = std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    if (len > 1e-6f) { v[0] /= len; v[1] /= len; v[2] /= len; }
}

// ==========================================================================
// CPU-side Phong shading (no shadows – rasterisation backend)
// ==========================================================================
// All light and material data is collected by SoRaytracerSceneCollector.
// The resulting pre-lit RGBA colour is stored in each vertex so the
// GPU performs only interpolation and depth testing.

static void vk_phong_contrib(const float * N, const float * V, const float * L,
                              const SoRtMaterial & mat,
                              const float * lightRGB, float lightIntensity,
                              float out[3])
{
    const float NdotL = vk_clamp01(vk_dot3(N, L));
    out[0] += mat.diffuse[0] * lightRGB[0] * lightIntensity * NdotL;
    out[1] += mat.diffuse[1] * lightRGB[1] * lightIntensity * NdotL;
    out[2] += mat.diffuse[2] * lightRGB[2] * lightIntensity * NdotL;

    const float specExp = mat.shininess * 128.0f;
    if (NdotL > 0.0f && specExp > 0.0f) {
        const float R[3] = {
            2.0f * NdotL * N[0] - L[0],
            2.0f * NdotL * N[1] - L[1],
            2.0f * NdotL * N[2] - L[2]
        };
        const float VdotR = vk_clamp01(vk_dot3(V, R));
        const float spec  = std::pow(VdotR, specExp);
        out[0] += mat.specular[0] * lightRGB[0] * lightIntensity * spec;
        out[1] += mat.specular[1] * lightRGB[1] * lightIntensity * spec;
        out[2] += mat.specular[2] * lightRGB[2] * lightIntensity * spec;
    }
}

// Compute the shaded RGBA colour for a surface point given its normal,
// view direction, and the scene's light list.
static void vk_shade(const float pos[3], const float N[3], const float V[3],
                     const SoRtMaterial & mat,
                     const std::vector<SoRtLightInfo> & lights,
                     float ambientFill,
                     float out[4])
{
    out[0] = mat.emission[0] + ambientFill * mat.ambient[0];
    out[1] = mat.emission[1] + ambientFill * mat.ambient[1];
    out[2] = mat.emission[2] + ambientFill * mat.ambient[2];
    out[3] = 1.0f;

    for (const SoRtLightInfo & li : lights) {
        float L[3];
        float attenuation = li.intensity;

        if (li.type == SO_RT_DIRECTIONAL) {
            L[0] = -li.dir[0]; L[1] = -li.dir[1]; L[2] = -li.dir[2];
            vk_normalize3(L);
        } else if (li.type == SO_RT_POINT) {
            L[0] = li.pos[0] - pos[0];
            L[1] = li.pos[1] - pos[1];
            L[2] = li.pos[2] - pos[2];
            const float dist = std::sqrt(L[0]*L[0] + L[1]*L[1] + L[2]*L[2]);
            if (dist < 1e-6f) continue;
            L[0] /= dist; L[1] /= dist; L[2] /= dist;
            attenuation = li.intensity / (1.0f + dist * dist);
        } else { // SO_RT_SPOT
            L[0] = li.pos[0] - pos[0];
            L[1] = li.pos[1] - pos[1];
            L[2] = li.pos[2] - pos[2];
            const float dist = std::sqrt(L[0]*L[0] + L[1]*L[1] + L[2]*L[2]);
            if (dist < 1e-6f) continue;
            L[0] /= dist; L[1] /= dist; L[2] /= dist;
            const float cosHalfCone = std::cos(li.cutOffAngle);
            const float cosSurface  = -vk_dot3(li.dir, L);
            if (cosSurface < cosHalfCone) continue;
            const float spotFactor = (li.dropOffRate > 0.0f)
                ? std::pow(cosSurface, li.dropOffRate) : 1.0f;
            attenuation = li.intensity * spotFactor / (1.0f + dist * dist);
        }

        vk_phong_contrib(N, V, L, mat, li.rgb, attenuation, out);
    }

    if (lights.empty()) {
        const float NdotV = vk_clamp01(vk_dot3(N, V));
        out[0] = mat.diffuse[0] * (ambientFill + (1.0f - ambientFill) * NdotV);
        out[1] = mat.diffuse[1] * (ambientFill + (1.0f - ambientFill) * NdotV);
        out[2] = mat.diffuse[2] * (ambientFill + (1.0f - ambientFill) * NdotV);
    }

    out[0] = vk_clamp01(out[0]);
    out[1] = vk_clamp01(out[1]);
    out[2] = vk_clamp01(out[2]);
}

// ==========================================================================
// Embedded SPIR-V shaders
// ==========================================================================
// Compiled from:
//   Vertex shader: passes inPos (location 0) through the MVP push constant
//     and forwards inColor (location 1) to the fragment stage unchanged.
//   Fragment shader: outputs the interpolated colour directly.
//
// Source (vertex):
//   #version 450
//   layout(push_constant) uniform PC { mat4 viewProj; } pc;
//   layout(location = 0) in vec3 inPos;
//   layout(location = 1) in vec4 inColor;
//   layout(location = 0) out vec4 fragColor;
//   void main() {
//       gl_Position = pc.viewProj * vec4(inPos, 1.0);
//       fragColor = inColor;
//   }
//
// Source (fragment):
//   #version 450
//   layout(location = 0) in vec4 fragColor;
//   layout(location = 0) out vec4 outColor;
//   void main() { outColor = fragColor; }

static const uint32_t kVulkanVertSPV[] = {
    0x07230203u, 0x00010000u, 0x0008000bu, 0x00000027u, 0x00000000u, 0x00020011u, 0x00000001u, 0x0006000bu,
    0x00000001u, 0x4c534c47u, 0x6474732eu, 0x3035342eu, 0x00000000u, 0x0003000eu, 0x00000000u, 0x00000001u,
    0x0009000fu, 0x00000000u, 0x00000004u, 0x6e69616du, 0x00000000u, 0x0000000du, 0x00000019u, 0x00000023u,
    0x00000025u, 0x00030003u, 0x00000002u, 0x000001c2u, 0x00040005u, 0x00000004u, 0x6e69616du, 0x00000000u,
    0x00060005u, 0x0000000bu, 0x505f6c67u, 0x65567265u, 0x78657472u, 0x00000000u, 0x00060006u, 0x0000000bu,
    0x00000000u, 0x505f6c67u, 0x7469736fu, 0x006e6f69u, 0x00070006u, 0x0000000bu, 0x00000001u, 0x505f6c67u,
    0x746e696fu, 0x657a6953u, 0x00000000u, 0x00070006u, 0x0000000bu, 0x00000002u, 0x435f6c67u, 0x4470696cu,
    0x61747369u, 0x0065636eu, 0x00070006u, 0x0000000bu, 0x00000003u, 0x435f6c67u, 0x446c6c75u, 0x61747369u,
    0x0065636eu, 0x00030005u, 0x0000000du, 0x00000000u, 0x00030005u, 0x00000011u, 0x00004350u, 0x00060006u,
    0x00000011u, 0x00000000u, 0x77656976u, 0x6a6f7250u, 0x00000000u, 0x00030005u, 0x00000013u, 0x00006370u,
    0x00040005u, 0x00000019u, 0x6f506e69u, 0x00000073u, 0x00050005u, 0x00000023u, 0x67617266u, 0x6f6c6f43u,
    0x00000072u, 0x00040005u, 0x00000025u, 0x6f436e69u, 0x00726f6cu, 0x00030047u, 0x0000000bu, 0x00000002u,
    0x00050048u, 0x0000000bu, 0x00000000u, 0x0000000bu, 0x00000000u, 0x00050048u, 0x0000000bu, 0x00000001u,
    0x0000000bu, 0x00000001u, 0x00050048u, 0x0000000bu, 0x00000002u, 0x0000000bu, 0x00000003u, 0x00050048u,
    0x0000000bu, 0x00000003u, 0x0000000bu, 0x00000004u, 0x00030047u, 0x00000011u, 0x00000002u, 0x00040048u,
    0x00000011u, 0x00000000u, 0x00000005u, 0x00050048u, 0x00000011u, 0x00000000u, 0x00000007u, 0x00000010u,
    0x00050048u, 0x00000011u, 0x00000000u, 0x00000023u, 0x00000000u, 0x00040047u, 0x00000019u, 0x0000001eu,
    0x00000000u, 0x00040047u, 0x00000023u, 0x0000001eu, 0x00000000u, 0x00040047u, 0x00000025u, 0x0000001eu,
    0x00000001u, 0x00020013u, 0x00000002u, 0x00030021u, 0x00000003u, 0x00000002u, 0x00030016u, 0x00000006u,
    0x00000020u, 0x00040017u, 0x00000007u, 0x00000006u, 0x00000004u, 0x00040015u, 0x00000008u, 0x00000020u,
    0x00000000u, 0x0004002bu, 0x00000008u, 0x00000009u, 0x00000001u, 0x0004001cu, 0x0000000au, 0x00000006u,
    0x00000009u, 0x0006001eu, 0x0000000bu, 0x00000007u, 0x00000006u, 0x0000000au, 0x0000000au, 0x00040020u,
    0x0000000cu, 0x00000003u, 0x0000000bu, 0x0004003bu, 0x0000000cu, 0x0000000du, 0x00000003u, 0x00040015u,
    0x0000000eu, 0x00000020u, 0x00000001u, 0x0004002bu, 0x0000000eu, 0x0000000fu, 0x00000000u, 0x00040018u,
    0x00000010u, 0x00000007u, 0x00000004u, 0x0003001eu, 0x00000011u, 0x00000010u, 0x00040020u, 0x00000012u,
    0x00000009u, 0x00000011u, 0x0004003bu, 0x00000012u, 0x00000013u, 0x00000009u, 0x00040020u, 0x00000014u,
    0x00000009u, 0x00000010u, 0x00040017u, 0x00000017u, 0x00000006u, 0x00000003u, 0x00040020u, 0x00000018u,
    0x00000001u, 0x00000017u, 0x0004003bu, 0x00000018u, 0x00000019u, 0x00000001u, 0x0004002bu, 0x00000006u,
    0x0000001bu, 0x3f800000u, 0x00040020u, 0x00000021u, 0x00000003u, 0x00000007u, 0x0004003bu, 0x00000021u,
    0x00000023u, 0x00000003u, 0x00040020u, 0x00000024u, 0x00000001u, 0x00000007u, 0x0004003bu, 0x00000024u,
    0x00000025u, 0x00000001u, 0x00050036u, 0x00000002u, 0x00000004u, 0x00000000u, 0x00000003u, 0x000200f8u,
    0x00000005u, 0x00050041u, 0x00000014u, 0x00000015u, 0x00000013u, 0x0000000fu, 0x0004003du, 0x00000010u,
    0x00000016u, 0x00000015u, 0x0004003du, 0x00000017u, 0x0000001au, 0x00000019u, 0x00050051u, 0x00000006u,
    0x0000001cu, 0x0000001au, 0x00000000u, 0x00050051u, 0x00000006u, 0x0000001du, 0x0000001au, 0x00000001u,
    0x00050051u, 0x00000006u, 0x0000001eu, 0x0000001au, 0x00000002u, 0x00070050u, 0x00000007u, 0x0000001fu,
    0x0000001cu, 0x0000001du, 0x0000001eu, 0x0000001bu, 0x00050091u, 0x00000007u, 0x00000020u, 0x00000016u,
    0x0000001fu, 0x00050041u, 0x00000021u, 0x00000022u, 0x0000000du, 0x0000000fu, 0x0003003eu, 0x00000022u,
    0x00000020u, 0x0004003du, 0x00000007u, 0x00000026u, 0x00000025u, 0x0003003eu, 0x00000023u, 0x00000026u,
    0x000100fdu, 0x00010038u,
};

static const uint32_t kVulkanFragSPV[] = {
    0x07230203u, 0x00010000u, 0x0008000bu, 0x0000000du, 0x00000000u, 0x00020011u, 0x00000001u, 0x0006000bu,
    0x00000001u, 0x4c534c47u, 0x6474732eu, 0x3035342eu, 0x00000000u, 0x0003000eu, 0x00000000u, 0x00000001u,
    0x0007000fu, 0x00000004u, 0x00000004u, 0x6e69616du, 0x00000000u, 0x00000009u, 0x0000000bu, 0x00030010u,
    0x00000004u, 0x00000007u, 0x00030003u, 0x00000002u, 0x000001c2u, 0x00040005u, 0x00000004u, 0x6e69616du,
    0x00000000u, 0x00050005u, 0x00000009u, 0x4374756fu, 0x726f6c6fu, 0x00000000u, 0x00050005u, 0x0000000bu,
    0x67617266u, 0x6f6c6f43u, 0x00000072u, 0x00040047u, 0x00000009u, 0x0000001eu, 0x00000000u, 0x00040047u,
    0x0000000bu, 0x0000001eu, 0x00000000u, 0x00020013u, 0x00000002u, 0x00030021u, 0x00000003u, 0x00000002u,
    0x00030016u, 0x00000006u, 0x00000020u, 0x00040017u, 0x00000007u, 0x00000006u, 0x00000004u, 0x00040020u,
    0x00000008u, 0x00000003u, 0x00000007u, 0x0004003bu, 0x00000008u, 0x00000009u, 0x00000003u, 0x00040020u,
    0x0000000au, 0x00000001u, 0x00000007u, 0x0004003bu, 0x0000000au, 0x0000000bu, 0x00000001u, 0x00050036u,
    0x00000002u, 0x00000004u, 0x00000000u, 0x00000003u, 0x000200f8u, 0x00000005u, 0x0004003du, 0x00000007u,
    0x0000000cu, 0x0000000bu, 0x0003003eu, 0x00000009u, 0x0000000cu, 0x000100fdu, 0x00010038u,
};

// ==========================================================================
// Vertex layout
// ==========================================================================

struct VkVtx {
    float pos[3];   // world-space position
    float col[4];   // pre-lit RGBA [0,1]
};
static_assert(sizeof(VkVtx) == 7 * sizeof(float), "VkVtx size mismatch");

// ==========================================================================
// SoVulkanContextManager
// ==========================================================================

class SoVulkanContextManager : public SoDB::ContextManager {
public:
    ~SoVulkanContextManager() override { cleanup(); }

    // -----------------------------------------------------------------------
    // GL context lifecycle – no-op (no GL context needed for this renderer)
    // -----------------------------------------------------------------------
    void * createOffscreenContext(unsigned int, unsigned int) override
    { return nullptr; }

    SbBool makeContextCurrent(void *) override  { return FALSE; }
    void   restorePreviousContext(void *) override {}
    void   destroyContext(void *) override {}

    // -----------------------------------------------------------------------
    // Cache management
    // -----------------------------------------------------------------------

    // Discard geometry cache so the next renderScene() unconditionally
    // rebuilds the vertex buffer.  Call when the scene root is replaced.
    void resetCache() {
        collector_.resetCache();
    }

    // Display-viewport hint for proxy geometry sizing (optional).
    void setDisplayViewport(unsigned int w, unsigned int h) {
        displayVpW_ = w;
        displayVpH_ = h;
    }

    // -----------------------------------------------------------------------
    // Rendering path
    // -----------------------------------------------------------------------
    SbBool renderScene(SoNode * scene,
                       unsigned int width, unsigned int height,
                       unsigned char * pixels,
                       unsigned int nrcomponents,
                       const float background_rgb[3]) override
    {
        if (!scene) return FALSE;

        // --- 0. Initialise Vulkan (once) ------------------------------------
        if (!ensureInit()) return FALSE;

        // --- 1. Find camera -------------------------------------------------
        SoCamera * cam = nullptr;
        {
            SoSearchAction sa;
            sa.setType(SoCamera::getClassTypeId());
            sa.setInterest(SoSearchAction::FIRST);
            sa.apply(scene);
            if (sa.getPath())
                cam = static_cast<SoCamera *>(sa.getPath()->getTail());
        }

        // --- 2. Geometry collection / cache ---------------------------------
        const SbViewportRegion vp(static_cast<short>(width),
                                  static_cast<short>(height));
        const SbViewportRegion proxyVp =
            (displayVpW_ > 0 && displayVpH_ > 0)
            ? SbViewportRegion(static_cast<short>(displayVpW_),
                               static_cast<short>(displayVpH_))
            : vp;

        if (collector_.needsRebuild(scene, cam)) {
            collector_.reset();
            collector_.collect(scene, vp, proxyVp);

            // Cache SoRaytracingParams
            cachedAmbientFill_ = 0.20f;
            {
                SoSearchAction sa;
                sa.setType(SoRaytracingParams::getClassTypeId());
                sa.setInterest(SoSearchAction::FIRST);
                sa.apply(scene);
                if (sa.getPath()) {
                    const SoRaytracingParams * rp =
                        static_cast<const SoRaytracingParams *>(
                            sa.getPath()->getTail());
                    cachedAmbientFill_ = rp->ambientIntensity.getValue();
                    if (cachedAmbientFill_ < 0.0f) cachedAmbientFill_ = 0.0f;
                    if (cachedAmbientFill_ > 1.0f) cachedAmbientFill_ = 1.0f;
                }
            }
            collector_.updateCacheKeysAfterRebuild(scene, cam);
        } else {
            collector_.collectOverlaysOnly(scene, vp);
        }

        const std::vector<SoRtTriangle>  & tris   = collector_.getTriangles();
        const std::vector<SoRtLightInfo> & lights  = collector_.getLights();
        const float ambientFill = cachedAmbientFill_;

        // --- 3. Build view-projection matrix --------------------------------
        // SbMatrix uses the row-vector convention: v' = v * M.
        // GLSL uses column-vector convention: v' = M * v.
        // To convert: GLSL needs M^T (where M is the SbMatrix).
        //
        // The combined transform in row-vector order is:
        //   v_clip_vk = v_world * affine * proj * zRemap
        // where affine = world→eye, proj = eye→OpenGL clip, and
        // zRemap remaps OpenGL z [-1,1] to Vulkan z [0,1].
        //
        // Passed to GLSL as M_glsl = (affine * proj * zRemap)^T (column-major).
        // The y-axis sign flip between OpenGL and Vulkan NDC is left
        // uncorrected; it produces bottom-to-top row order in the Vulkan
        // framebuffer which already matches SoOffscreenRenderer's convention.
        float vp_matrix[16]; // column-major for GLSL
        SbVec3f eyePos(0, 0, 0);
        bool ortho = false;

        if (cam) {
            const float aspect =
                static_cast<float>(width) / static_cast<float>(height);
            SbViewVolume vv = cam->getViewVolume(aspect);
            if (aspect < 1.0f) vv.scale(1.0f / aspect);

            SbMatrix affine, proj;
            vv.getMatrices(affine, proj);

            // z-remap (row-vector convention): v' = v_clip * zRemap
            // We want z_clip_vk = 0.5 * z_clip_gl + 0.5 * w_clip
            // In row-vector: z_out[j=2] = sum_i(v[i] * zRemap[i][2])
            //   → zRemap[2][2] = 0.5 (z_clip contributes 0.5 to z_out)
            //   → zRemap[3][2] = 0.5 (w_clip contributes 0.5 to z_out)
            //   → identity elsewhere
            float remap[4][4] = {
                {1,0,0,0},
                {0,1,0,0},
                {0,0,0.5f,0},  // z contribution to z_out
                {0,0,0.5f,1}   // w contribution to z_out, w_out unchanged
            };
            SbMatrix zRemap(remap);

            // combined = affine * proj * zRemap  (row-vector order: apply left-to-right)
            SbMatrix combined = affine * proj * zRemap;

            // Transpose SbMatrix (row-vector) to GLSL column-major:
            // M_glsl[r][c] = combined^T[r][c] = combined[c][r]
            // In column-major storage: vp_matrix[c*4+r] = combined[c][r]
            const float (* const m)[4] =
                reinterpret_cast<const float (*)[4]>(combined.getValue());
            for (int c = 0; c < 4; ++c)
                for (int r = 0; r < 4; ++r)
                    vp_matrix[c * 4 + r] = m[c][r];  // SbMatrix[c][r] → GLSL col-major[r,c]: transposes row-vector to column-vector

            eyePos = vv.getProjectionPoint();
            ortho  = (vv.getProjectionType() == SbViewVolume::ORTHOGRAPHIC);
            if (ortho) {
                // For orthographic cameras the view direction is constant.
                // Approximate eye position as far behind the scene.
                SbVec3f dir = vv.getProjectionDirection();
                eyePos = vv.getProjectionPoint() - dir * 1e6f;
            }
        } else {
            // No camera: identity MVP (objects render at their world coords)
            for (int i = 0; i < 16; ++i) vp_matrix[i] = (i % 5 == 0) ? 1.0f : 0.0f;
        }

        // --- 4. Build vertex buffer (CPU Phong pre-bake) --------------------
        std::vector<VkVtx> verts;
        verts.reserve(tris.size() * 3);

        for (const SoRtTriangle & tri : tris) {
            for (int v = 0; v < 3; ++v) {
                VkVtx vtx;
                vtx.pos[0] = tri.pos[v][0];
                vtx.pos[1] = tri.pos[v][1];
                vtx.pos[2] = tri.pos[v][2];

                // Normal (already world-space, may need normalization)
                float N[3] = { tri.norm[v][0], tri.norm[v][1], tri.norm[v][2] };
                vk_normalize3(N);

                // View vector from surface point toward eye
                float V[3] = {
                    eyePos[0] - vtx.pos[0],
                    eyePos[1] - vtx.pos[1],
                    eyePos[2] - vtx.pos[2]
                };
                vk_normalize3(V);

                vk_shade(vtx.pos, N, V, tri.mat, lights, ambientFill, vtx.col);
                verts.push_back(vtx);
            }
        }

        // --- 5. Ensure framebuffer dimensions match -------------------------
        if (!ensureFramebuffer(width, height, background_rgb)) return FALSE;

        // --- 6. Upload vertices to Vulkan buffer ----------------------------
        const VkDeviceSize vertBytes =
            static_cast<VkDeviceSize>(verts.size() * sizeof(VkVtx));

        if (vertBytes > 0) {
            if (!ensureVertexBuffer(vertBytes)) return FALSE;
            void * mapped = nullptr;
            if (vkMapMemory(device_, vertMem_, 0, vertBytes, 0, &mapped)
                    != VK_SUCCESS)
                return FALSE;
            memcpy(mapped, verts.data(), static_cast<size_t>(vertBytes));
            vkUnmapMemory(device_, vertMem_);
        }

        // --- 7. Record and submit render commands ---------------------------
        if (!submitRender(width, height, background_rgb,
                          vp_matrix, verts.size()))
            return FALSE;

        // --- 8. Copy pixels from staging buffer to output -------------------
        const size_t total_px = static_cast<size_t>(width) * height;
        const size_t rowBytes  = static_cast<size_t>(width) * 4; // RGBA

        void * mapped = nullptr;
        if (vkMapMemory(device_, stagingMem_, 0, VK_WHOLE_SIZE, 0, &mapped)
                != VK_SUCCESS)
            return FALSE;

        const uint8_t * src = static_cast<const uint8_t *>(mapped);
        for (size_t i = 0; i < total_px; ++i) {
            const uint8_t * sp = src + i * 4;
            uint8_t       * dp = pixels + i * nrcomponents;
            dp[0] = sp[0];
            dp[1] = sp[1];
            dp[2] = sp[2];
            if (nrcomponents == 4) dp[3] = sp[3];
        }
        vkUnmapMemory(device_, stagingMem_);

        // --- 9. Composite text/HUD overlays ---------------------------------
        collector_.compositeOverlays(pixels, width, height, nrcomponents);
        collector_.updateCameraId(cam);

        return TRUE;
    }

private:

    // -----------------------------------------------------------------------
    // Vulkan object handles
    // -----------------------------------------------------------------------
    VkInstance       instance_   = VK_NULL_HANDLE;
    VkPhysicalDevice physDev_    = VK_NULL_HANDLE;
    VkDevice         device_     = VK_NULL_HANDLE;
    VkQueue          queue_      = VK_NULL_HANDLE;
    uint32_t         queueFam_   = UINT32_MAX;
    VkCommandPool    cmdPool_    = VK_NULL_HANDLE;
    VkCommandBuffer  cmdBuf_     = VK_NULL_HANDLE;
    VkRenderPass     renderPass_ = VK_NULL_HANDLE;
    VkPipelineLayout pipeLayout_ = VK_NULL_HANDLE;
    VkPipeline       pipeline_   = VK_NULL_HANDLE;
    VkFence          fence_      = VK_NULL_HANDLE;

    // Per-render-size resources
    unsigned int     imgW_        = 0;
    unsigned int     imgH_        = 0;
    VkImage          colorImg_    = VK_NULL_HANDLE;
    VkDeviceMemory   colorMem_    = VK_NULL_HANDLE;
    VkImageView      colorView_   = VK_NULL_HANDLE;
    VkImage          depthImg_    = VK_NULL_HANDLE;
    VkDeviceMemory   depthMem_    = VK_NULL_HANDLE;
    VkImageView      depthView_   = VK_NULL_HANDLE;
    VkFramebuffer    framebuf_    = VK_NULL_HANDLE;
    VkBuffer         stagingBuf_  = VK_NULL_HANDLE;
    VkDeviceMemory   stagingMem_  = VK_NULL_HANDLE;

    // Vertex buffer (resized as needed)
    VkBuffer         vertBuf_     = VK_NULL_HANDLE;
    VkDeviceMemory   vertMem_     = VK_NULL_HANDLE;
    VkDeviceSize     vertBufCap_  = 0;

    // Initialisation state
    bool             initDone_    = false;
    bool             initFailed_  = false;

    // Scene collector and cached parameters
    SoRaytracerSceneCollector collector_;
    float                     cachedAmbientFill_ = 0.20f;
    unsigned int              displayVpW_        = 0;
    unsigned int              displayVpH_        = 0;

    // -----------------------------------------------------------------------
    // Memory type helper
    // -----------------------------------------------------------------------
    uint32_t findMemType(uint32_t typeBits,
                         VkMemoryPropertyFlags props) const
    {
        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(physDev_, &memProps);
        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
            if ((typeBits & (1u << i)) &&
                    (memProps.memoryTypes[i].propertyFlags & props) == props)
                return i;
        }
        return UINT32_MAX;
    }

    // -----------------------------------------------------------------------
    // Buffer helper
    // -----------------------------------------------------------------------
    bool createBuffer(VkDeviceSize size,
                      VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags memProps,
                      VkBuffer & buf,
                      VkDeviceMemory & mem)
    {
        VkBufferCreateInfo bi{};
        bi.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bi.size        = size;
        bi.usage       = usage;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device_, &bi, nullptr, &buf) != VK_SUCCESS)
            return false;

        VkMemoryRequirements req;
        vkGetBufferMemoryRequirements(device_, buf, &req);
        const uint32_t mt = findMemType(req.memoryTypeBits, memProps);
        if (mt == UINT32_MAX) {
            vkDestroyBuffer(device_, buf, nullptr); buf = VK_NULL_HANDLE;
            return false;
        }

        VkMemoryAllocateInfo ai{};
        ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize  = req.size;
        ai.memoryTypeIndex = mt;
        if (vkAllocateMemory(device_, &ai, nullptr, &mem) != VK_SUCCESS) {
            vkDestroyBuffer(device_, buf, nullptr); buf = VK_NULL_HANDLE;
            return false;
        }
        vkBindBufferMemory(device_, buf, mem, 0);
        return true;
    }

    // -----------------------------------------------------------------------
    // Image helper
    // -----------------------------------------------------------------------
    bool createImage(uint32_t w, uint32_t h,
                     VkFormat fmt,
                     VkImageUsageFlags usage,
                     VkMemoryPropertyFlags memProps,
                     VkImage & img,
                     VkDeviceMemory & mem)
    {
        VkImageCreateInfo ii{};
        ii.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ii.imageType     = VK_IMAGE_TYPE_2D;
        ii.format        = fmt;
        ii.extent        = {w, h, 1};
        ii.mipLevels     = 1;
        ii.arrayLayers   = 1;
        ii.samples       = VK_SAMPLE_COUNT_1_BIT;
        ii.tiling        = VK_IMAGE_TILING_OPTIMAL;
        ii.usage         = usage;
        ii.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (vkCreateImage(device_, &ii, nullptr, &img) != VK_SUCCESS)
            return false;

        VkMemoryRequirements req;
        vkGetImageMemoryRequirements(device_, img, &req);
        const uint32_t mt = findMemType(req.memoryTypeBits, memProps);
        if (mt == UINT32_MAX) {
            vkDestroyImage(device_, img, nullptr); img = VK_NULL_HANDLE;
            return false;
        }

        VkMemoryAllocateInfo ai{};
        ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize  = req.size;
        ai.memoryTypeIndex = mt;
        if (vkAllocateMemory(device_, &ai, nullptr, &mem) != VK_SUCCESS) {
            vkDestroyImage(device_, img, nullptr); img = VK_NULL_HANDLE;
            return false;
        }
        vkBindImageMemory(device_, img, mem, 0);
        return true;
    }

    // -----------------------------------------------------------------------
    // ImageView helper
    // -----------------------------------------------------------------------
    bool createImageView(VkImage img, VkFormat fmt,
                         VkImageAspectFlags aspect,
                         VkImageView & view)
    {
        VkImageViewCreateInfo vi{};
        vi.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image    = img;
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format   = fmt;
        vi.subresourceRange.aspectMask     = aspect;
        vi.subresourceRange.baseMipLevel   = 0;
        vi.subresourceRange.levelCount     = 1;
        vi.subresourceRange.baseArrayLayer = 0;
        vi.subresourceRange.layerCount     = 1;
        return vkCreateImageView(device_, &vi, nullptr, &view) == VK_SUCCESS;
    }

    // -----------------------------------------------------------------------
    // Initialise Vulkan (called once on the first renderScene())
    // -----------------------------------------------------------------------
    bool ensureInit()
    {
        if (initDone_) return !initFailed_;
        initDone_   = true;
        initFailed_ = true; // assume failure until we succeed

        // Instance
        VkApplicationInfo appInfo{};
        appInfo.sType      = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pEngineName = "Obol";
        appInfo.apiVersion  = VK_API_VERSION_1_0;

        VkInstanceCreateInfo instCI{};
        instCI.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instCI.pApplicationInfo = &appInfo;
        if (vkCreateInstance(&instCI, nullptr, &instance_) != VK_SUCCESS) {
            fprintf(stderr,
                "SoVulkanContextManager: vkCreateInstance failed – "
                "check that a Vulkan loader and ICD are installed.\n");
            return false;
        }

        // Physical device – prefer discrete GPU; fall back to any device
        uint32_t devCount = 0;
        vkEnumeratePhysicalDevices(instance_, &devCount, nullptr);
        if (devCount == 0) {
            fprintf(stderr,
                "SoVulkanContextManager: no Vulkan physical devices found. "
                "Install a Vulkan ICD (e.g. mesa-vulkan-drivers or "
                "nvidia-driver) or set VK_ICD_FILENAMES.\n");
            return false;
        }
        std::vector<VkPhysicalDevice> devs(devCount);
        vkEnumeratePhysicalDevices(instance_, &devCount, devs.data());

        // Pick a device: prefer one with a graphics queue
        physDev_ = VK_NULL_HANDLE;
        for (VkPhysicalDevice pd : devs) {
            uint32_t qfCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(pd, &qfCount, nullptr);
            std::vector<VkQueueFamilyProperties> qfProps(qfCount);
            vkGetPhysicalDeviceQueueFamilyProperties(pd, &qfCount, qfProps.data());
            for (uint32_t i = 0; i < qfCount; ++i) {
                if (qfProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                    physDev_  = pd;
                    queueFam_ = i;
                    break;
                }
            }
            if (physDev_ != VK_NULL_HANDLE) break;
        }
        if (physDev_ == VK_NULL_HANDLE) {
            fprintf(stderr,
                "SoVulkanContextManager: no Vulkan device with graphics queue found.\n");
            return false;
        }

        {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(physDev_, &props);
            fprintf(stderr, "SoVulkanContextManager: using device '%s'\n",
                    props.deviceName);
        }

        // Logical device + queue
        const float qPriority = 1.0f;
        VkDeviceQueueCreateInfo qCI{};
        qCI.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qCI.queueFamilyIndex = queueFam_;
        qCI.queueCount       = 1;
        qCI.pQueuePriorities = &qPriority;

        VkDeviceCreateInfo devCI{};
        devCI.sType                = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        devCI.queueCreateInfoCount = 1;
        devCI.pQueueCreateInfos    = &qCI;
        if (vkCreateDevice(physDev_, &devCI, nullptr, &device_) != VK_SUCCESS) {
            fprintf(stderr,
                "SoVulkanContextManager: vkCreateDevice failed.\n");
            return false;
        }
        vkGetDeviceQueue(device_, queueFam_, 0, &queue_);

        // Command pool
        VkCommandPoolCreateInfo cpCI{};
        cpCI.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cpCI.queueFamilyIndex = queueFam_;
        cpCI.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        if (vkCreateCommandPool(device_, &cpCI, nullptr, &cmdPool_)
                != VK_SUCCESS)
            return false;

        // Command buffer
        VkCommandBufferAllocateInfo cbAI{};
        cbAI.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cbAI.commandPool        = cmdPool_;
        cbAI.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cbAI.commandBufferCount = 1;
        if (vkAllocateCommandBuffers(device_, &cbAI, &cmdBuf_) != VK_SUCCESS)
            return false;

        // Render pass
        if (!createRenderPass()) return false;

        // Pipeline
        if (!createPipeline()) return false;

        // Fence
        VkFenceCreateInfo fCI{};
        fCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        if (vkCreateFence(device_, &fCI, nullptr, &fence_) != VK_SUCCESS)
            return false;

        initFailed_ = false;
        return true;
    }

    // -----------------------------------------------------------------------
    // Render pass (colour + depth)
    // -----------------------------------------------------------------------
    bool createRenderPass()
    {
        VkAttachmentDescription attachments[2]{};

        // Colour
        attachments[0].format         = VK_FORMAT_R8G8B8A8_UNORM;
        attachments[0].samples        = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout    = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

        // Depth
        attachments[1].format         = VK_FORMAT_D32_SFLOAT;
        attachments[1].samples        = VK_SAMPLE_COUNT_1_BIT;
        attachments[1].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[1].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount    = 1;
        subpass.pColorAttachments       = &colRef;
        subpass.pDepthStencilAttachment = &depRef;

        // Subpass dependency: ensure writes are complete before transfer
        VkSubpassDependency dep{};
        dep.srcSubpass    = 0;
        dep.dstSubpass    = VK_SUBPASS_EXTERNAL;
        dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.dstStageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dep.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        VkRenderPassCreateInfo rpCI{};
        rpCI.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpCI.attachmentCount = 2;
        rpCI.pAttachments    = attachments;
        rpCI.subpassCount    = 1;
        rpCI.pSubpasses      = &subpass;
        rpCI.dependencyCount = 1;
        rpCI.pDependencies   = &dep;

        return vkCreateRenderPass(device_, &rpCI, nullptr, &renderPass_)
               == VK_SUCCESS;
    }

    // -----------------------------------------------------------------------
    // Graphics pipeline
    // -----------------------------------------------------------------------
    bool createPipeline()
    {
        // Shader modules
        VkShaderModule vertModule = VK_NULL_HANDLE;
        VkShaderModule fragModule = VK_NULL_HANDLE;
        {
            VkShaderModuleCreateInfo smCI{};
            smCI.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            smCI.codeSize = sizeof(kVulkanVertSPV);
            smCI.pCode    = kVulkanVertSPV;
            if (vkCreateShaderModule(device_, &smCI, nullptr, &vertModule)
                    != VK_SUCCESS)
                return false;

            smCI.codeSize = sizeof(kVulkanFragSPV);
            smCI.pCode    = kVulkanFragSPV;
            if (vkCreateShaderModule(device_, &smCI, nullptr, &fragModule)
                    != VK_SUCCESS) {
                vkDestroyShaderModule(device_, vertModule, nullptr);
                return false;
            }
        }

        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertModule;
        stages[0].pName  = "main";
        stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragModule;
        stages[1].pName  = "main";

        // Vertex input: binding 0, per-vertex data (VkVtx)
        VkVertexInputBindingDescription binding{};
        binding.binding   = 0;
        binding.stride    = sizeof(VkVtx);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription attrs[2]{};
        attrs[0].location = 0; attrs[0].binding = 0;
        attrs[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
        attrs[0].offset   = offsetof(VkVtx, pos);
        attrs[1].location = 1; attrs[1].binding = 0;
        attrs[1].format   = VK_FORMAT_R32G32B32A32_SFLOAT;
        attrs[1].offset   = offsetof(VkVtx, col);

        VkPipelineVertexInputStateCreateInfo viSCI{};
        viSCI.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        viSCI.vertexBindingDescriptionCount   = 1;
        viSCI.pVertexBindingDescriptions      = &binding;
        viSCI.vertexAttributeDescriptionCount = 2;
        viSCI.pVertexAttributeDescriptions    = attrs;

        VkPipelineInputAssemblyStateCreateInfo iaSCI{};
        iaSCI.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        iaSCI.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // Viewport / scissor are dynamic so we don't hard-code dimensions here
        VkPipelineViewportStateCreateInfo vpSCI{};
        vpSCI.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vpSCI.viewportCount = 1;
        vpSCI.scissorCount  = 1;

        VkPipelineRasterizationStateCreateInfo rasterSCI{};
        rasterSCI.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterSCI.polygonMode = VK_POLYGON_MODE_FILL;
        rasterSCI.cullMode    = VK_CULL_MODE_NONE;      // no backface culling
        rasterSCI.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterSCI.lineWidth   = 1.0f;

        VkPipelineMultisampleStateCreateInfo msSCI{};
        msSCI.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        msSCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo dsSCI{};
        dsSCI.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        dsSCI.depthTestEnable  = VK_TRUE;
        dsSCI.depthWriteEnable = VK_TRUE;
        dsSCI.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL;

        VkPipelineColorBlendAttachmentState cbAttach{};
        cbAttach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo cbSCI{};
        cbSCI.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        cbSCI.attachmentCount = 1;
        cbSCI.pAttachments    = &cbAttach;

        const VkDynamicState dynStates[] = {
            VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR
        };
        VkPipelineDynamicStateCreateInfo dynSCI{};
        dynSCI.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynSCI.dynamicStateCount = 2;
        dynSCI.pDynamicStates    = dynStates;

        // Push constant: one float[16] matrix (64 bytes) in vertex stage
        VkPushConstantRange pcRange{};
        pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pcRange.offset     = 0;
        pcRange.size       = 64; // sizeof(float) * 16

        VkPipelineLayoutCreateInfo plCI{};
        plCI.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        plCI.pushConstantRangeCount = 1;
        plCI.pPushConstantRanges    = &pcRange;
        if (vkCreatePipelineLayout(device_, &plCI, nullptr, &pipeLayout_)
                != VK_SUCCESS) {
            vkDestroyShaderModule(device_, vertModule, nullptr);
            vkDestroyShaderModule(device_, fragModule, nullptr);
            return false;
        }

        VkGraphicsPipelineCreateInfo gpCI{};
        gpCI.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        gpCI.stageCount          = 2;
        gpCI.pStages             = stages;
        gpCI.pVertexInputState   = &viSCI;
        gpCI.pInputAssemblyState = &iaSCI;
        gpCI.pViewportState      = &vpSCI;
        gpCI.pRasterizationState = &rasterSCI;
        gpCI.pMultisampleState   = &msSCI;
        gpCI.pDepthStencilState  = &dsSCI;
        gpCI.pColorBlendState    = &cbSCI;
        gpCI.pDynamicState       = &dynSCI;
        gpCI.layout              = pipeLayout_;
        gpCI.renderPass          = renderPass_;
        gpCI.subpass             = 0;

        const bool ok = vkCreateGraphicsPipelines(
            device_, VK_NULL_HANDLE, 1, &gpCI, nullptr, &pipeline_)
            == VK_SUCCESS;

        vkDestroyShaderModule(device_, vertModule, nullptr);
        vkDestroyShaderModule(device_, fragModule, nullptr);
        return ok;
    }

    // -----------------------------------------------------------------------
    // Framebuffer + staging buffer (re-created when dimensions change)
    // -----------------------------------------------------------------------
    void destroyFramebuffer()
    {
        if (framebuf_)   { vkDestroyFramebuffer(device_, framebuf_, nullptr);   framebuf_   = VK_NULL_HANDLE; }
        if (depthView_)  { vkDestroyImageView(device_, depthView_, nullptr);    depthView_  = VK_NULL_HANDLE; }
        if (depthImg_)   { vkDestroyImage(device_, depthImg_, nullptr);         depthImg_   = VK_NULL_HANDLE; }
        if (depthMem_)   { vkFreeMemory(device_, depthMem_, nullptr);           depthMem_   = VK_NULL_HANDLE; }
        if (colorView_)  { vkDestroyImageView(device_, colorView_, nullptr);    colorView_  = VK_NULL_HANDLE; }
        if (colorImg_)   { vkDestroyImage(device_, colorImg_, nullptr);         colorImg_   = VK_NULL_HANDLE; }
        if (colorMem_)   { vkFreeMemory(device_, colorMem_, nullptr);           colorMem_   = VK_NULL_HANDLE; }
        if (stagingBuf_) { vkDestroyBuffer(device_, stagingBuf_, nullptr);      stagingBuf_ = VK_NULL_HANDLE; }
        if (stagingMem_) { vkFreeMemory(device_, stagingMem_, nullptr);         stagingMem_ = VK_NULL_HANDLE; }
        imgW_ = imgH_ = 0;
    }

    bool ensureFramebuffer(unsigned int w, unsigned int h,
                           const float * /*bg*/)
    {
        if (w == imgW_ && h == imgH_) return true;
        destroyFramebuffer();

        // Colour image (device-local, transfer-src capable)
        if (!createImage(w, h, VK_FORMAT_R8G8B8A8_UNORM,
                         VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                         VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                         colorImg_, colorMem_))
            return false;
        if (!createImageView(colorImg_, VK_FORMAT_R8G8B8A8_UNORM,
                             VK_IMAGE_ASPECT_COLOR_BIT, colorView_))
            return false;

        // Depth image
        if (!createImage(w, h, VK_FORMAT_D32_SFLOAT,
                         VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                         depthImg_, depthMem_))
            return false;
        if (!createImageView(depthImg_, VK_FORMAT_D32_SFLOAT,
                             VK_IMAGE_ASPECT_DEPTH_BIT, depthView_))
            return false;

        // Framebuffer
        VkImageView fbViews[] = {colorView_, depthView_};
        VkFramebufferCreateInfo fbCI{};
        fbCI.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbCI.renderPass      = renderPass_;
        fbCI.attachmentCount = 2;
        fbCI.pAttachments    = fbViews;
        fbCI.width           = w;
        fbCI.height          = h;
        fbCI.layers          = 1;
        if (vkCreateFramebuffer(device_, &fbCI, nullptr, &framebuf_)
                != VK_SUCCESS)
            return false;

        // Staging buffer (host-visible, for pixel readback)
        const VkDeviceSize stagBytes =
            static_cast<VkDeviceSize>(w) * h * 4; // RGBA
        if (!createBuffer(stagBytes,
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                          stagingBuf_, stagingMem_))
            return false;

        imgW_ = w;
        imgH_ = h;
        return true;
    }

    // -----------------------------------------------------------------------
    // Vertex buffer (grown as needed)
    // -----------------------------------------------------------------------
    bool ensureVertexBuffer(VkDeviceSize needed)
    {
        if (needed <= vertBufCap_) return true;

        if (vertBuf_) {
            vkDestroyBuffer(device_, vertBuf_, nullptr); vertBuf_ = VK_NULL_HANDLE;
            vkFreeMemory(device_, vertMem_, nullptr);    vertMem_ = VK_NULL_HANDLE;
            vertBufCap_ = 0;
        }
        // Allocate with 50% headroom
        const VkDeviceSize cap =
            static_cast<VkDeviceSize>(static_cast<double>(needed) * 1.5);
        if (!createBuffer(cap,
                          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                          vertBuf_, vertMem_))
            return false;
        vertBufCap_ = cap;
        return true;
    }

    // -----------------------------------------------------------------------
    // Record, submit, and wait for one render command buffer
    // -----------------------------------------------------------------------
    bool submitRender(unsigned int w, unsigned int h,
                      const float * bg,
                      const float vp_matrix[16],
                      size_t vertCount)
    {
        vkResetCommandBuffer(cmdBuf_, 0);

        VkCommandBufferBeginInfo cbBI{};
        cbBI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cbBI.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (vkBeginCommandBuffer(cmdBuf_, &cbBI) != VK_SUCCESS) return false;

        // Clear values: background colour + depth=1
        VkClearValue clears[2]{};
        clears[0].color        = {{bg[0], bg[1], bg[2], 1.0f}};
        clears[1].depthStencil = {1.0f, 0};

        VkRenderPassBeginInfo rpBI{};
        rpBI.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpBI.renderPass        = renderPass_;
        rpBI.framebuffer       = framebuf_;
        rpBI.renderArea.extent = {w, h};
        rpBI.clearValueCount   = 2;
        rpBI.pClearValues      = clears;

        vkCmdBeginRenderPass(cmdBuf_, &rpBI, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{};
        viewport.width    = static_cast<float>(w);
        viewport.height   = static_cast<float>(h);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmdBuf_, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.extent = {w, h};
        vkCmdSetScissor(cmdBuf_, 0, 1, &scissor);

        vkCmdBindPipeline(cmdBuf_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
        vkCmdPushConstants(cmdBuf_, pipeLayout_,
                           VK_SHADER_STAGE_VERTEX_BIT, 0, 64, vp_matrix);

        if (vertCount > 0 && vertBuf_ != VK_NULL_HANDLE) {
            const VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(cmdBuf_, 0, 1, &vertBuf_, &offset);
            vkCmdDraw(cmdBuf_, static_cast<uint32_t>(vertCount), 1, 0, 0);
        }

        vkCmdEndRenderPass(cmdBuf_);

        // Copy colour image to staging buffer
        VkBufferImageCopy region{};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent                 = {w, h, 1};
        vkCmdCopyImageToBuffer(cmdBuf_,
                               colorImg_,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               stagingBuf_,
                               1, &region);

        if (vkEndCommandBuffer(cmdBuf_) != VK_SUCCESS) return false;

        // Submit
        VkSubmitInfo si{};
        si.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.commandBufferCount = 1;
        si.pCommandBuffers    = &cmdBuf_;
        if (vkQueueSubmit(queue_, 1, &si, fence_) != VK_SUCCESS) return false;

        // Wait for completion
        const VkResult wr = vkWaitForFences(
            device_, 1, &fence_, VK_TRUE, UINT64_MAX);
        vkResetFences(device_, 1, &fence_);
        return wr == VK_SUCCESS;
    }

    // -----------------------------------------------------------------------
    // Release all Vulkan resources
    // -----------------------------------------------------------------------
    void cleanup()
    {
        if (!instance_) return;

        if (device_) {
            vkDeviceWaitIdle(device_);
            destroyFramebuffer();
            if (vertBuf_)    vkDestroyBuffer(device_, vertBuf_, nullptr);
            if (vertMem_)    vkFreeMemory(device_, vertMem_, nullptr);
            if (fence_)      vkDestroyFence(device_, fence_, nullptr);
            if (pipeline_)   vkDestroyPipeline(device_, pipeline_, nullptr);
            if (pipeLayout_) vkDestroyPipelineLayout(device_, pipeLayout_, nullptr);
            if (renderPass_) vkDestroyRenderPass(device_, renderPass_, nullptr);
            if (cmdPool_)    vkDestroyCommandPool(device_, cmdPool_, nullptr);
            vkDestroyDevice(device_, nullptr);
        }
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
        device_   = VK_NULL_HANDLE;
    }
};

#endif // OBOL_VULKAN_CONTEXT_MANAGER_H
