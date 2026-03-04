/*
 * nanort_context_manager.h
 *
 * SoNanoRTContextManager – a SoDB::ContextManager specialization that uses
 * nanort for CPU ray-triangle intersection instead of OpenGL.
 *
 * Scene collection (geometry extraction, proxy shapes, text overlays, lights)
 * is handled by SoRaytracerSceneCollector from the Obol library.  Only the
 * nanort-specific BVH construction, ray-triangle intersection, and Phong
 * shading remain here.
 *
 * Usage:
 *
 *   #include "nanort_context_manager.h"
 *
 *   static SoNanoRTContextManager nrt_mgr;
 *   SoDB::init(&nrt_mgr);
 *   SoNodeKit::init();
 *   SoInteraction::init();
 *
 *   // ... build scene graph ...
 *
 *   SbViewportRegion vp(800, 600);
 *   SoOffscreenRenderer renderer(vp);
 *   renderer.setComponents(SoOffscreenRenderer::RGB);
 *   renderer.render(root);          // calls nrt_mgr.renderScene() internally
 *   renderer.writeToRGB("out.rgb"); // writes the raytraced pixels
 *
 * Obol APIs used internally (via SoRaytracerSceneCollector):
 *   - SoCallbackAction  – extract triangles, normals, materials from scene graph
 *   - SoSearchAction    – find camera, SoRaytracingParams in scene graph
 *   - SbViewportRegion  – viewport for camera setup
 *   - SoCamera          – get view volume for ray generation
 *   - SbViewVolume      – projectPointToLine() for per-pixel ray directions
 *   - SbMatrix          – transform vertices/normals/positions to world space
 *   - SoRaytracingParams – rendering hints (shadows, reflections, AA, ambient)
 *
 * Rendering features implemented:
 *   - Perspective and orthographic cameras (via SoCamera / SbViewVolume)
 *   - All light types (via SoRaytracerSceneCollector light collection)
 *   - SoMaterial: diffuse, specular, emissive, ambient, shininess
 *   - All triangle-generating shapes via SoRaytracerSceneCollector
 *   - SoRaytracingParams: hard shadows, specular reflections, AA, ambient fill
 *   - SoText2, SoHUDLabel, SoHUDButton overlays via SoRaytracerSceneCollector
 *
 * Dependencies:
 *   - SoRaytracerSceneCollector (Obol library)
 *   - nanort.h (external/nanort/nanort.h)
 */

#ifndef OBOL_NANORT_CONTEXT_MANAGER_H
#define OBOL_NANORT_CONTEXT_MANAGER_H

// ---- Obol generic raytracing infrastructure ---------------------------------
#include <Inventor/SoRaytracerSceneCollector.h>
#include <Inventor/SoDB.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbViewVolume.h>
#include <Inventor/SbVec2f.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/nodes/SoCamera.h>
#include <Inventor/nodes/SoRaytracingParams.h>

// ---- nanort -----------------------------------------------------------------
#include "nanort.h"

// ---- Standard library -------------------------------------------------------
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>

// ==========================================================================
// NrtScene: nanort BVH built from SoRtTriangle data
// ==========================================================================
// Holds the flat vertex/face/normal arrays consumed by nanort plus the built
// BVH acceleration structure.  Material and interpolated-normal lookups use
// the SoRtTriangle vector from SoRaytracerSceneCollector directly so this
// struct only owns the intersection-query data.

struct NrtScene {
    std::vector<float>        vertices; // 9 floats/triangle (3 verts × xyz)
    std::vector<unsigned int> faces;    // 3 indices/triangle (sequential)
    std::vector<float>        normals;  // 9 floats/triangle
    nanort::BVHAccel<float>   accel;

    bool build(const std::vector<SoRtTriangle> & tris)
    {
        const size_t n = tris.size();
        if (n == 0) return false;

        vertices.resize(n * 9);
        normals.resize(n * 9);
        faces.resize(n * 3);

        for (size_t i = 0; i < n; ++i) {
            for (int v = 0; v < 3; ++v) {
                const size_t base = 9 * i + 3 * v;
                vertices[base + 0] = tris[i].pos[v][0];
                vertices[base + 1] = tris[i].pos[v][1];
                vertices[base + 2] = tris[i].pos[v][2];
                normals[base + 0]  = tris[i].norm[v][0];
                normals[base + 1]  = tris[i].norm[v][1];
                normals[base + 2]  = tris[i].norm[v][2];
                faces[3 * i + v]   = static_cast<unsigned int>(3 * i + v);
            }
        }

        nanort::BVHBuildOptions<float> opts;
        opts.cache_bbox = false;
        nanort::TriangleMesh<float> tmesh(vertices.data(), faces.data(),
                                          sizeof(float) * 3);
        nanort::TriangleSAHPred<float> tpred(vertices.data(), faces.data(),
                                              sizeof(float) * 3);
        return accel.Build(static_cast<unsigned int>(n), tmesh, tpred, opts);
    }
};

// ==========================================================================
// Math helpers
// ==========================================================================

static inline float nrt_clamp01(float v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}
static inline float nrt_dot3(const float * a, const float * b) {
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}
static inline void nrt_normalize3(float v[3]) {
    const float len = std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    if (len > 1e-6f) { v[0] /= len; v[1] /= len; v[2] /= len; }
}

// Minimal xorshift32 PRNG for AA jitter (no allocations, no state setup).
static inline uint32_t nrt_xorshift32(uint32_t s) {
    s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s;
}
static inline float nrt_rand01(uint32_t & s) {
    s = nrt_xorshift32(s);
    return static_cast<float>(s) * (1.0f / 4294967296.0f);
}

// ==========================================================================
// Phong shading
// ==========================================================================

// Phong shading contribution from one light.
static void nrt_phong(const float * N, const float * V, const float * L,
                      const SoRtMaterial & mat,
                      const float * lightRGB, float lightIntensity,
                      float out[3])
{
    const float NdotL = nrt_clamp01(nrt_dot3(N, L));

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
        const float VdotR = nrt_clamp01(nrt_dot3(V, R));
        const float spec  = std::pow(VdotR, specExp);
        out[0] += mat.specular[0] * lightRGB[0] * lightIntensity * spec;
        out[1] += mat.specular[1] * lightRGB[1] * lightIntensity * spec;
        out[2] += mat.specular[2] * lightRGB[2] * lightIntensity * spec;
    }
}

// Shadow ray test: returns true if hitPt is in shadow from direction L.
static bool nrt_is_shadowed(const NrtScene & scene,
                             const nanort::TriangleIntersector<float> & isect,
                             const float hitPt[3],
                             const float N[3],
                             const float L[3],
                             float maxT)
{
    nanort::Ray<float> shadowRay;
    const float kEps = 1e-3f;
    shadowRay.org[0] = hitPt[0] + N[0] * kEps;
    shadowRay.org[1] = hitPt[1] + N[1] * kEps;
    shadowRay.org[2] = hitPt[2] + N[2] * kEps;
    shadowRay.dir[0] = L[0];
    shadowRay.dir[1] = L[1];
    shadowRay.dir[2] = L[2];
    shadowRay.min_t  = kEps;
    shadowRay.max_t  = maxT - kEps;

    nanort::TriangleIntersection<float> si;
    return scene.accel.Traverse(shadowRay, isect, &si);
}

// Shade a hit point given surface data and scene lights.
static void nrt_shade(const NrtScene & scene,
                       const nanort::TriangleIntersector<float> & isect,
                       const float hitPt[3],
                       const float N[3],
                       const float V[3],
                       const SoRtMaterial & mat,
                       const std::vector<SoRtLightInfo> & lights,
                       float ambientFill,
                       bool shadowsEnabled,
                       float out[3])
{
    out[0] = mat.emission[0] + ambientFill * mat.ambient[0];
    out[1] = mat.emission[1] + ambientFill * mat.ambient[1];
    out[2] = mat.emission[2] + ambientFill * mat.ambient[2];

    for (const SoRtLightInfo & li : lights) {
        float L[3];
        float attenuation = li.intensity;
        float shadowMaxT  = 1.0e30f;

        if (li.type == SO_RT_DIRECTIONAL) {
            L[0] = -li.dir[0]; L[1] = -li.dir[1]; L[2] = -li.dir[2];
            nrt_normalize3(L);
        } else if (li.type == SO_RT_POINT) {
            L[0] = li.pos[0] - hitPt[0];
            L[1] = li.pos[1] - hitPt[1];
            L[2] = li.pos[2] - hitPt[2];
            const float dist = std::sqrt(L[0]*L[0] + L[1]*L[1] + L[2]*L[2]);
            if (dist < 1e-6f) continue;
            L[0] /= dist; L[1] /= dist; L[2] /= dist;
            const float d2 = dist * dist;
            attenuation = li.intensity / (1.0f + d2);
            shadowMaxT  = dist;
        } else { // SO_RT_SPOT
            L[0] = li.pos[0] - hitPt[0];
            L[1] = li.pos[1] - hitPt[1];
            L[2] = li.pos[2] - hitPt[2];
            const float dist = std::sqrt(L[0]*L[0] + L[1]*L[1] + L[2]*L[2]);
            if (dist < 1e-6f) continue;
            L[0] /= dist; L[1] /= dist; L[2] /= dist;

            const float cosHalfCone = std::cos(li.cutOffAngle);
            const float cosSurface  = -nrt_dot3(li.dir, L);
            if (cosSurface < cosHalfCone) continue;

            const float spotFactor = (li.dropOffRate > 0.0f)
                ? std::pow(cosSurface, li.dropOffRate) : 1.0f;
            const float d2  = dist * dist;
            attenuation = li.intensity * spotFactor / (1.0f + d2);
            shadowMaxT  = dist;
        }

        if (shadowsEnabled && nrt_is_shadowed(scene, isect, hitPt, N, L, shadowMaxT))
            continue;

        nrt_phong(N, V, L, mat, li.rgb, attenuation, out);
    }

    if (lights.empty()) {
        const float NdotV = nrt_clamp01(nrt_dot3(N, V));
        out[0] = mat.diffuse[0] * (ambientFill + (1.0f - ambientFill) * NdotV);
        out[1] = mat.diffuse[1] * (ambientFill + (1.0f - ambientFill) * NdotV);
        out[2] = mat.diffuse[2] * (ambientFill + (1.0f - ambientFill) * NdotV);
    }
}

// Trace a ray and return RGB colour, with optional reflection bounces.
static void nrt_trace(const NrtScene & scene,
                       const nanort::TriangleIntersector<float> & intersector,
                       const std::vector<SoRtTriangle> & tris,
                       const float org[3], const float dir[3],
                       const std::vector<SoRtLightInfo> & lights,
                       float ambientFill,
                       bool shadowsEnabled,
                       int depth,
                       const float bgColor[3],
                       float out[3])
{
    nanort::Ray<float> ray;
    ray.org[0] = org[0]; ray.org[1] = org[1]; ray.org[2] = org[2];
    ray.dir[0] = dir[0]; ray.dir[1] = dir[1]; ray.dir[2] = dir[2];
    ray.min_t  = 0.001f;
    ray.max_t  = 1.0e30f;

    nanort::TriangleIntersection<float> isect;
    if (!scene.accel.Traverse(ray, intersector, &isect)) {
        out[0] = bgColor[0]; out[1] = bgColor[1]; out[2] = bgColor[2];
        return;
    }

    const unsigned int fid = isect.prim_id;
    const float w0 = 1.0f - isect.u - isect.v;
    const float w1 = isect.u;
    const float w2 = isect.v;

    const float * n0 = scene.normals.data() + 9 * fid + 0;
    const float * n1 = scene.normals.data() + 9 * fid + 3;
    const float * n2 = scene.normals.data() + 9 * fid + 6;
    float N[3] = {
        w0*n0[0] + w1*n1[0] + w2*n2[0],
        w0*n0[1] + w1*n1[1] + w2*n2[1],
        w0*n0[2] + w1*n1[2] + w2*n2[2]
    };
    nrt_normalize3(N);

    const float V[3] = { -dir[0], -dir[1], -dir[2] };

    const float hitPt[3] = {
        org[0] + dir[0] * isect.t,
        org[1] + dir[1] * isect.t,
        org[2] + dir[2] * isect.t
    };

    const SoRtMaterial & mat = tris[fid].mat;

    nrt_shade(scene, intersector, hitPt, N, V, mat, lights,
              ambientFill, shadowsEnabled, out);

    if (depth > 0) {
        const float specLum = (mat.specular[0] * 0.2126f +
                               mat.specular[1] * 0.7152f +
                               mat.specular[2] * 0.0722f) * mat.shininess;
        if (specLum > 0.01f) {
            const float NdotI = nrt_dot3(N, dir);
            const float Rdir[3] = {
                dir[0] - 2.0f * NdotI * N[0],
                dir[1] - 2.0f * NdotI * N[1],
                dir[2] - 2.0f * NdotI * N[2]
            };
            const float kEps = 1e-3f;
            const float Rorg[3] = {
                hitPt[0] + N[0] * kEps,
                hitPt[1] + N[1] * kEps,
                hitPt[2] + N[2] * kEps
            };
            float reflColor[3];
            nrt_trace(scene, intersector, tris, Rorg, Rdir, lights,
                      ambientFill, shadowsEnabled, depth - 1, bgColor,
                      reflColor);
            out[0] += mat.specular[0] * specLum * reflColor[0];
            out[1] += mat.specular[1] * specLum * reflColor[1];
            out[2] += mat.specular[2] * specLum * reflColor[2];
        }
    }
}

// ==========================================================================
// SoNanoRTContextManager
// ==========================================================================
//
// Scene collection is delegated to SoRaytracerSceneCollector which manages
// the SoCallbackAction setup, proxy geometry, text overlays, light extraction,
// and scene-change cache.  This class retains only nanort-specific state:
// the built BVH (NrtScene) and the cached SoRaytracingParams settings.
//
// Thread safety: not thread-safe; use from a single thread.

class SoNanoRTContextManager : public SoDB::ContextManager {
public:
    // -----------------------------------------------------------------------
    // GL context lifecycle – no-op (no GL needed for this renderer)
    // -----------------------------------------------------------------------
    void * createOffscreenContext(unsigned int, unsigned int) override
    { return nullptr; }

    SbBool makeContextCurrent(void *) override
    { return FALSE; }

    void restorePreviousContext(void *) override {}

    void destroyContext(void *) override {}

    // -----------------------------------------------------------------------
    // Cache management
    // -----------------------------------------------------------------------

    // Discard the cached BVH and scene-collection state so the next
    // renderScene() call unconditionally rebuilds everything.  Call this
    // whenever the scene root is replaced to prevent false cache hits
    // caused by pointer aliasing.
    void resetCache() {
        collector_.resetCache();
        cachedNrtScene_ = NrtScene();
        cachedShadowsEnabled_    = false;
        cachedMaxBouncesAllowed_ = 0;
        cachedSamplesPerPixel_   = 1;
        cachedAmbientFill_       = 0.20f;
    }

    // -----------------------------------------------------------------------
    // Display viewport for proxy geometry sizing
    // -----------------------------------------------------------------------

    // Inform the renderer of the full display dimensions so that line/point/
    // cylinder proxy geometry is always sized for the real display resolution
    // even when renderScene() is called at a reduced (coarse) resolution.
    // Pass (0, 0) to revert to using the render dimensions (the default).
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

        const SbViewportRegion vp(static_cast<short>(width),
                                  static_cast<short>(height));
        const SbViewportRegion proxyVp =
            (displayVpW_ > 0 && displayVpH_ > 0)
            ? SbViewportRegion(static_cast<short>(displayVpW_),
                               static_cast<short>(displayVpH_))
            : vp;

        // --- 1. Find camera (needed for the cache check) --------------------
        SoCamera * cam = nullptr;
        {
            SoSearchAction sa;
            sa.setType(SoCamera::getClassTypeId());
            sa.setInterest(SoSearchAction::FIRST);
            sa.apply(scene);
            if (sa.getPath())
                cam = static_cast<SoCamera *>(sa.getPath()->getTail());
        }

        // --- 2. Geometry collection / cache management ----------------------
        if (collector_.needsRebuild(scene, cam)) {
            // Full traversal: geometry + lights + overlays
            collector_.reset();
            collector_.collect(scene, vp, proxyVp);

            // Build nanort BVH from the collected triangles
            cachedNrtScene_ = NrtScene();
            if (!collector_.getTriangles().empty()) {
                if (!cachedNrtScene_.build(collector_.getTriangles()))
                    return FALSE;
            }

            // Cache SoRaytracingParams (any change bumps root nodeId → rebuild)
            cachedShadowsEnabled_    = false;
            cachedMaxBouncesAllowed_ = 0;
            cachedSamplesPerPixel_   = 1;
            cachedAmbientFill_       = 0.20f;
            {
                SoSearchAction sa;
                sa.setType(SoRaytracingParams::getClassTypeId());
                sa.setInterest(SoSearchAction::FIRST);
                sa.apply(scene);
                if (sa.getPath()) {
                    const SoRaytracingParams * rp =
                        static_cast<const SoRaytracingParams *>(
                            sa.getPath()->getTail());
                    cachedShadowsEnabled_    =
                        rp->shadowsEnabled.getValue() != FALSE;
                    cachedMaxBouncesAllowed_ =
                        rp->maxReflectionBounces.getValue();
                    cachedSamplesPerPixel_   =
                        rp->samplesPerPixel.getValue();
                    cachedAmbientFill_       =
                        rp->ambientIntensity.getValue();
                    if (cachedSamplesPerPixel_ < 1) cachedSamplesPerPixel_ = 1;
                    if (cachedMaxBouncesAllowed_ < 0) cachedMaxBouncesAllowed_ = 0;
                    if (cachedAmbientFill_ < 0.0f) cachedAmbientFill_ = 0.0f;
                    if (cachedAmbientFill_ > 1.0f) cachedAmbientFill_ = 1.0f;
                }
            }

            collector_.updateCacheKeysAfterRebuild(scene, cam);
        } else {
            // Cache hit: only regenerate text/HUD overlays
            // (screen-space positions depend on the current camera/viewport)
            collector_.collectOverlaysOnly(scene, vp);
        }

        // --- 3. Rendering parameters (from cache) ---------------------------
        const bool  shadowsEnabled    = cachedShadowsEnabled_;
        const int   maxBouncesAllowed = cachedMaxBouncesAllowed_;
        const int   samplesPerPixel   = cachedSamplesPerPixel_;
        const float ambientFill       = cachedAmbientFill_;

        const std::vector<SoRtTriangle>  & tris   = collector_.getTriangles();
        const std::vector<SoRtLightInfo> & lights  = collector_.getLights();

        // --- 4. Raytrace ----------------------------------------------------
        if (!tris.empty()) {
            if (!cam) return FALSE;

            const float aspect_ratio =
                static_cast<float>(width) / static_cast<float>(height);
            SbViewVolume vv = cam->getViewVolume(aspect_ratio);
            if (aspect_ratio < 1.0f) vv.scale(1.0f / aspect_ratio);

            NrtScene & nrtScene = cachedNrtScene_;
            nanort::TriangleIntersector<float> intersector(
                nrtScene.vertices.data(), nrtScene.faces.data(),
                sizeof(float) * 3);

            uint32_t rngState = 0xDEADBEEFu;

            for (unsigned int y = 0; y < height; ++y) {
                for (unsigned int x = 0; x < width; ++x) {
                    const float fx = static_cast<float>(x);
                    const float fy = static_cast<float>(y);
                    const float fw = static_cast<float>(width);
                    const float fh = static_cast<float>(height);

                    float accum[3] = { 0.0f, 0.0f, 0.0f };
                    int hitSamples = 0;

                    for (int s = 0; s < samplesPerPixel; ++s) {
                        float jx = 0.5f, jy = 0.5f;
                        if (samplesPerPixel > 1) {
                            jx = nrt_rand01(rngState);
                            jy = nrt_rand01(rngState);
                        }
                        const float nx = (fx + jx) / fw;
                        const float ny = (fy + jy) / fh;

                        SbVec3f p0, p1;
                        vv.projectPointToLine(SbVec2f(nx, ny), p0, p1);
                        SbVec3f d = p1 - p0;
                        d.normalize();

                        nanort::Ray<float> ray;
                        ray.org[0] = p0[0]; ray.org[1] = p0[1]; ray.org[2] = p0[2];
                        ray.dir[0] = d[0];  ray.dir[1] = d[1];  ray.dir[2] = d[2];
                        ray.min_t  = 0.001f;
                        ray.max_t  = 1.0e30f;

                        nanort::TriangleIntersection<float> isect;
                        if (!nrtScene.accel.Traverse(ray, intersector, &isect))
                            continue;
                        ++hitSamples;

                        const unsigned int fid = isect.prim_id;
                        const float w0 = 1.0f - isect.u - isect.v;
                        const float w1 = isect.u;
                        const float w2 = isect.v;

                        const float * n0 = nrtScene.normals.data() + 9 * fid + 0;
                        const float * n1 = nrtScene.normals.data() + 9 * fid + 3;
                        const float * n2 = nrtScene.normals.data() + 9 * fid + 6;
                        float N[3] = {
                            w0*n0[0] + w1*n1[0] + w2*n2[0],
                            w0*n0[1] + w1*n1[1] + w2*n2[1],
                            w0*n0[2] + w1*n1[2] + w2*n2[2]
                        };
                        nrt_normalize3(N);

                        const float dir[3] = { d[0], d[1], d[2] };
                        const float V[3]   = { -dir[0], -dir[1], -dir[2] };

                        const float hitPt[3] = {
                            p0[0] + dir[0] * isect.t,
                            p0[1] + dir[1] * isect.t,
                            p0[2] + dir[2] * isect.t
                        };

                        const SoRtMaterial & mat = tris[fid].mat;

                        float px[3] = { 0.0f, 0.0f, 0.0f };
                        nrt_shade(nrtScene, intersector, hitPt, N, V, mat,
                                  lights, ambientFill, shadowsEnabled, px);

                        if (maxBouncesAllowed > 0) {
                            const float specLum =
                                (mat.specular[0] * 0.2126f +
                                 mat.specular[1] * 0.7152f +
                                 mat.specular[2] * 0.0722f) * mat.shininess;
                            if (specLum > 0.01f) {
                                const float NdotI = nrt_dot3(N, dir);
                                const float Rdir[3] = {
                                    dir[0] - 2.0f * NdotI * N[0],
                                    dir[1] - 2.0f * NdotI * N[1],
                                    dir[2] - 2.0f * NdotI * N[2]
                                };
                                const float kEps = 1e-3f;
                                const float Rorg[3] = {
                                    hitPt[0] + N[0] * kEps,
                                    hitPt[1] + N[1] * kEps,
                                    hitPt[2] + N[2] * kEps
                                };
                                float reflColor[3];
                                nrt_trace(nrtScene, intersector, tris,
                                          Rorg, Rdir, lights,
                                          ambientFill, shadowsEnabled,
                                          maxBouncesAllowed - 1,
                                          background_rgb, reflColor);
                                px[0] += mat.specular[0] * specLum * reflColor[0];
                                px[1] += mat.specular[1] * specLum * reflColor[1];
                                px[2] += mat.specular[2] * specLum * reflColor[2];
                            }
                        }

                        accum[0] += px[0];
                        accum[1] += px[1];
                        accum[2] += px[2];
                    }

                    if (hitSamples == 0) continue;

                    const float inv_s = 1.0f / static_cast<float>(hitSamples);
                    const size_t idx  = (y * width + x) * nrcomponents;
                    pixels[idx + 0] = static_cast<unsigned char>(
                        nrt_clamp01(accum[0] * inv_s) * 255.0f);
                    pixels[idx + 1] = static_cast<unsigned char>(
                        nrt_clamp01(accum[1] * inv_s) * 255.0f);
                    pixels[idx + 2] = static_cast<unsigned char>(
                        nrt_clamp01(accum[2] * inv_s) * 255.0f);
                    if (nrcomponents == 4) pixels[idx + 3] = 255;
                }
            }
        }

        // --- 5. Composite text/HUD overlays ---------------------------------
        collector_.compositeOverlays(pixels, width, height, nrcomponents);

        collector_.updateCameraId(cam);
        return TRUE;
    }

private:
    SoRaytracerSceneCollector collector_;
    NrtScene                  cachedNrtScene_;
    bool                      cachedShadowsEnabled_    = false;
    int                       cachedMaxBouncesAllowed_ = 0;
    int                       cachedSamplesPerPixel_   = 1;
    float                     cachedAmbientFill_       = 0.20f;
    unsigned int              displayVpW_              = 0;
    unsigned int              displayVpH_              = 0;
};

#endif // OBOL_NANORT_CONTEXT_MANAGER_H
