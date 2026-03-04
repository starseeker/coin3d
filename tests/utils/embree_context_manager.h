/*
 * embree_context_manager.h
 *
 * SoEmbreeContextManager – a SoDB::ContextManager specialization that uses
 * Intel Embree 4 for CPU ray-triangle intersection instead of OpenGL.
 *
 * Scene collection (geometry extraction, proxy shapes, text overlays, lights)
 * is handled by SoRaytracerSceneCollector from the Obol library — the same
 * generic infrastructure used by SoNanoRTContextManager.  Only the Embree-
 * specific BVH construction and ray-triangle intersection differ.
 *
 * Usage:
 *
 *   #include "embree_context_manager.h"
 *
 *   static SoEmbreeContextManager embree_mgr;
 *   SoDB::init(&embree_mgr);
 *   SoNodeKit::init();
 *   SoInteraction::init();
 *
 *   SbViewportRegion vp(800, 600);
 *   SoOffscreenRenderer renderer(vp);
 *   renderer.setComponents(SoOffscreenRenderer::RGB);
 *   renderer.render(root);
 *   renderer.writeToRGB("out.rgb");
 *
 * Rendering features:
 *   - All features from SoRaytracerSceneCollector (proxy shapes, HUD, …)
 *   - Perspective and orthographic cameras
 *   - Directional, point, and spot lights with Phong shading
 *   - Hard shadows via rtcOccluded1()
 *   - Specular reflection bounces
 *   - Anti-aliasing (jittered multi-sample, from SoRaytracingParams)
 *   - SoText2, SoHUDLabel, SoHUDButton pixel overlays
 *
 * Requires: embree4/rtcore.h (libembree-dev on Ubuntu / embree4 on Fedora)
 */

#ifndef OBOL_EMBREE_CONTEXT_MANAGER_H
#define OBOL_EMBREE_CONTEXT_MANAGER_H

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

// ---- Embree 4 ---------------------------------------------------------------
#include <embree4/rtcore.h>

// ---- Standard library -------------------------------------------------------
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <cstring>

// ==========================================================================
// Math helpers (shared with nanort path; kept minimal and self-contained)
// ==========================================================================

static inline float emb_clamp01(float v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}
static inline float emb_dot3(const float * a, const float * b) {
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}
static inline void emb_normalize3(float v[3]) {
    const float len = std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    if (len > 1e-6f) { v[0] /= len; v[1] /= len; v[2] /= len; }
}

static inline uint32_t emb_xorshift32(uint32_t s) {
    s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s;
}
static inline float emb_rand01(uint32_t & s) {
    s = emb_xorshift32(s);
    return static_cast<float>(s) * (1.0f / 4294967296.0f);
}

// ==========================================================================
// Embree scene wrapper
// ==========================================================================
// Manages the Embree device/scene lifetime and provides convenience helpers
// for building from SoRtTriangle data and querying ray hits / occlusion.

struct EmbreeScene {
    RTCDevice device = nullptr;
    RTCScene  scene  = nullptr;

    // Flat vertex buffer mirroring the SoRtTriangle positions so Embree can
    // reference it directly (Embree holds a pointer, so lifetime must match).
    std::vector<float>        vertices; // 9 floats/triangle
    std::vector<unsigned int> indices;  // 3 indices/triangle

    EmbreeScene() = default;

    ~EmbreeScene() { destroy(); }

    void destroy() {
        if (scene)  { rtcReleaseScene(scene);  scene  = nullptr; }
        if (device) { rtcReleaseDevice(device); device = nullptr; }
    }

    // Build from a collected triangle list.  Returns true on success.
    bool build(const std::vector<SoRtTriangle> & tris)
    {
        destroy();

        const size_t n = tris.size();
        if (n == 0) return false;

        device = rtcNewDevice(nullptr);
        if (!device) return false;

        scene = rtcNewScene(device);
        if (!scene) { destroy(); return false; }

        // Populate flat buffers
        vertices.resize(n * 9);
        indices.resize(n * 3);
        for (size_t i = 0; i < n; ++i) {
            for (int v = 0; v < 3; ++v) {
                const size_t base = 9 * i + 3 * v;
                vertices[base + 0] = tris[i].pos[v][0];
                vertices[base + 1] = tris[i].pos[v][1];
                vertices[base + 2] = tris[i].pos[v][2];
                indices[3 * i + v] = static_cast<unsigned int>(3 * i + v);
            }
        }

        // Register a single triangle geometry
        RTCGeometry geom = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_TRIANGLE);

        rtcSetSharedGeometryBuffer(geom, RTC_BUFFER_TYPE_VERTEX, 0,
                                   RTC_FORMAT_FLOAT3,
                                   vertices.data(),
                                   0,
                                   sizeof(float) * 3,
                                   n * 3);

        rtcSetSharedGeometryBuffer(geom, RTC_BUFFER_TYPE_INDEX, 0,
                                   RTC_FORMAT_UINT3,
                                   indices.data(),
                                   0,
                                   sizeof(unsigned int) * 3,
                                   n);

        rtcCommitGeometry(geom);
        rtcAttachGeometry(scene, geom);
        rtcReleaseGeometry(geom);

        rtcCommitScene(scene);
        return true;
    }

    // Cast a primary ray; returns true on hit and fills rayhit.
    bool intersect(RTCRayHit & rayhit) const
    {
        RTCIntersectArguments args;
        rtcInitIntersectArguments(&args);
        rtcIntersect1(scene, &rayhit, &args);
        return rayhit.hit.geomID != RTC_INVALID_GEOMETRY_ID;
    }

    // Occlusion query: returns true if the ray is blocked.
    bool occluded(RTCRay & ray) const
    {
        RTCOccludedArguments args;
        rtcInitOccludedArguments(&args);
        rtcOccluded1(scene, &ray, &args);
        // rtcOccluded1 sets ray.tfar to -inf on occlusion
        return ray.tfar < 0.0f;
    }
};

// ==========================================================================
// Phong shading helpers (Embree version — shadow test via rtcOccluded1)
// ==========================================================================

static void emb_phong(const float * N, const float * V, const float * L,
                       const SoRtMaterial & mat,
                       const float * lightRGB, float lightIntensity,
                       float out[3])
{
    const float NdotL = emb_clamp01(emb_dot3(N, L));

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
        const float VdotR = emb_clamp01(emb_dot3(V, R));
        const float spec  = std::pow(VdotR, specExp);
        out[0] += mat.specular[0] * lightRGB[0] * lightIntensity * spec;
        out[1] += mat.specular[1] * lightRGB[1] * lightIntensity * spec;
        out[2] += mat.specular[2] * lightRGB[2] * lightIntensity * spec;
    }
}

static bool emb_is_shadowed(const EmbreeScene & es,
                              const float hitPt[3],
                              const float N[3],
                              const float L[3],
                              float maxT)
{
    const float kEps = 1e-3f;
    RTCRay shadowRay;
    shadowRay.org_x = hitPt[0] + N[0] * kEps;
    shadowRay.org_y = hitPt[1] + N[1] * kEps;
    shadowRay.org_z = hitPt[2] + N[2] * kEps;
    shadowRay.dir_x = L[0];
    shadowRay.dir_y = L[1];
    shadowRay.dir_z = L[2];
    shadowRay.tnear = kEps;
    shadowRay.tfar  = maxT - kEps;
    shadowRay.mask  = static_cast<unsigned int>(-1);
    shadowRay.id    = 0;
    shadowRay.flags = 0;
    shadowRay.time  = 0.0f;
    return es.occluded(shadowRay);
}

static void emb_shade(const EmbreeScene & es,
                       const std::vector<float> & interpNormals,
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
            emb_normalize3(L);
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
            const float cosSurface  = -emb_dot3(li.dir, L);
            if (cosSurface < cosHalfCone) continue;

            const float spotFactor = (li.dropOffRate > 0.0f)
                ? std::pow(cosSurface, li.dropOffRate) : 1.0f;
            const float d2  = dist * dist;
            attenuation = li.intensity * spotFactor / (1.0f + d2);
            shadowMaxT  = dist;
        }

        if (shadowsEnabled && emb_is_shadowed(es, hitPt, N, L, shadowMaxT))
            continue;

        emb_phong(N, V, L, mat, li.rgb, attenuation, out);
    }

    if (lights.empty()) {
        const float NdotV = emb_clamp01(emb_dot3(N, V));
        out[0] = mat.diffuse[0] * (ambientFill + (1.0f - ambientFill) * NdotV);
        out[1] = mat.diffuse[1] * (ambientFill + (1.0f - ambientFill) * NdotV);
        out[2] = mat.diffuse[2] * (ambientFill + (1.0f - ambientFill) * NdotV);
    }
}

// Recursive ray trace (used for reflection bounces).
static void emb_trace(const EmbreeScene & es,
                       const std::vector<SoRtTriangle> & tris,
                       const std::vector<float> & normals,
                       const float org[3], const float dir[3],
                       const std::vector<SoRtLightInfo> & lights,
                       float ambientFill,
                       bool shadowsEnabled,
                       int depth,
                       const float bgColor[3],
                       float out[3])
{
    RTCRayHit rayhit;
    rayhit.ray.org_x = org[0]; rayhit.ray.org_y = org[1]; rayhit.ray.org_z = org[2];
    rayhit.ray.dir_x = dir[0]; rayhit.ray.dir_y = dir[1]; rayhit.ray.dir_z = dir[2];
    rayhit.ray.tnear = 0.001f;
    rayhit.ray.tfar  = 1.0e30f;
    rayhit.ray.mask  = static_cast<unsigned int>(-1);
    rayhit.ray.id    = 0;
    rayhit.ray.flags = 0;
    rayhit.ray.time  = 0.0f;
    rayhit.hit.geomID = RTC_INVALID_GEOMETRY_ID;
    rayhit.hit.primID = RTC_INVALID_GEOMETRY_ID;
    rayhit.hit.instID[0] = RTC_INVALID_GEOMETRY_ID;

    if (!es.intersect(rayhit)) {
        out[0] = bgColor[0]; out[1] = bgColor[1]; out[2] = bgColor[2];
        return;
    }

    const unsigned int fid = rayhit.hit.primID;
    const float w1 = rayhit.hit.u;
    const float w2 = rayhit.hit.v;
    const float w0 = 1.0f - w1 - w2;

    const float * n0 = normals.data() + 9 * fid + 0;
    const float * n1 = normals.data() + 9 * fid + 3;
    const float * n2 = normals.data() + 9 * fid + 6;
    float N[3] = {
        w0*n0[0] + w1*n1[0] + w2*n2[0],
        w0*n0[1] + w1*n1[1] + w2*n2[1],
        w0*n0[2] + w1*n1[2] + w2*n2[2]
    };
    emb_normalize3(N);

    const float V[3] = { -dir[0], -dir[1], -dir[2] };

    const float t = rayhit.ray.tfar;
    const float hitPt[3] = {
        org[0] + dir[0] * t,
        org[1] + dir[1] * t,
        org[2] + dir[2] * t
    };

    const SoRtMaterial & mat = tris[fid].mat;
    std::vector<float> dummy; // unused by emb_shade
    emb_shade(es, dummy, hitPt, N, V, mat, lights,
              ambientFill, shadowsEnabled, out);

    if (depth > 0) {
        const float specLum = (mat.specular[0] * 0.2126f +
                               mat.specular[1] * 0.7152f +
                               mat.specular[2] * 0.0722f) * mat.shininess;
        if (specLum > 0.01f) {
            const float NdotI = emb_dot3(N, dir);
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
            emb_trace(es, tris, normals, Rorg, Rdir, lights,
                      ambientFill, shadowsEnabled, depth - 1, bgColor,
                      reflColor);
            out[0] += mat.specular[0] * specLum * reflColor[0];
            out[1] += mat.specular[1] * specLum * reflColor[1];
            out[2] += mat.specular[2] * specLum * reflColor[2];
        }
    }
}

// ==========================================================================
// SoEmbreeContextManager
// ==========================================================================

class SoEmbreeContextManager : public SoDB::ContextManager {
public:
    // -----------------------------------------------------------------------
    // GL context lifecycle – no-op (no GL context required)
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

    void resetCache() {
        collector_.resetCache();
        embreeScene_.destroy();
        normals_.clear();
        cachedShadowsEnabled_    = false;
        cachedMaxBouncesAllowed_ = 0;
        cachedSamplesPerPixel_   = 1;
        cachedAmbientFill_       = 0.20f;
    }

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

        // --- 2. Scene collection / cache management -------------------------
        if (collector_.needsRebuild(scene, cam)) {
            collector_.reset();
            collector_.collect(scene, vp, proxyVp);

            const std::vector<SoRtTriangle> & tris = collector_.getTriangles();

            // Build Embree BVH
            embreeScene_.destroy();
            normals_.clear();
            if (!tris.empty()) {
                if (!embreeScene_.build(tris)) return FALSE;

                // Build the per-triangle normal array for interpolation
                // (stored separately from Embree's vertex buffer)
                const size_t n = tris.size();
                normals_.resize(n * 9);
                for (size_t i = 0; i < n; ++i) {
                    for (int v = 0; v < 3; ++v) {
                        const size_t base = 9 * i + 3 * v;
                        normals_[base + 0] = tris[i].norm[v][0];
                        normals_[base + 1] = tris[i].norm[v][1];
                        normals_[base + 2] = tris[i].norm[v][2];
                    }
                }
            }

            // Cache SoRaytracingParams
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
                    if (cachedSamplesPerPixel_  < 1) cachedSamplesPerPixel_ = 1;
                    if (cachedMaxBouncesAllowed_ < 0) cachedMaxBouncesAllowed_ = 0;
                    if (cachedAmbientFill_ < 0.0f) cachedAmbientFill_ = 0.0f;
                    if (cachedAmbientFill_ > 1.0f) cachedAmbientFill_ = 1.0f;
                }
            }

            collector_.updateCacheKeysAfterRebuild(scene, cam);
        } else {
            collector_.collectOverlaysOnly(scene, vp);
        }

        const bool  shadowsEnabled    = cachedShadowsEnabled_;
        const int   maxBouncesAllowed = cachedMaxBouncesAllowed_;
        const int   samplesPerPixel   = cachedSamplesPerPixel_;
        const float ambientFill       = cachedAmbientFill_;

        const std::vector<SoRtTriangle>  & tris   = collector_.getTriangles();
        const std::vector<SoRtLightInfo> & lights  = collector_.getLights();

        // --- 3. Raytrace ----------------------------------------------------
        if (!tris.empty() && embreeScene_.scene) {
            if (!cam) return FALSE;

            const float aspect_ratio =
                static_cast<float>(width) / static_cast<float>(height);
            SbViewVolume vv = cam->getViewVolume(aspect_ratio);
            if (aspect_ratio < 1.0f) vv.scale(1.0f / aspect_ratio);

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
                            jx = emb_rand01(rngState);
                            jy = emb_rand01(rngState);
                        }
                        const float nx = (fx + jx) / fw;
                        const float ny = (fy + jy) / fh;

                        SbVec3f p0, p1;
                        vv.projectPointToLine(SbVec2f(nx, ny), p0, p1);
                        SbVec3f d = p1 - p0;
                        d.normalize();

                        RTCRayHit rayhit;
                        rayhit.ray.org_x = p0[0]; rayhit.ray.org_y = p0[1]; rayhit.ray.org_z = p0[2];
                        rayhit.ray.dir_x = d[0];  rayhit.ray.dir_y = d[1];  rayhit.ray.dir_z = d[2];
                        rayhit.ray.tnear = 0.001f;
                        rayhit.ray.tfar  = 1.0e30f;
                        rayhit.ray.mask  = static_cast<unsigned int>(-1);
                        rayhit.ray.id    = 0;
                        rayhit.ray.flags = 0;
                        rayhit.ray.time  = 0.0f;
                        rayhit.hit.geomID = RTC_INVALID_GEOMETRY_ID;
                        rayhit.hit.primID = RTC_INVALID_GEOMETRY_ID;
                        rayhit.hit.instID[0] = RTC_INVALID_GEOMETRY_ID;

                        if (!embreeScene_.intersect(rayhit)) continue;
                        ++hitSamples;

                        const unsigned int fid = rayhit.hit.primID;
                        const float w1 = rayhit.hit.u;
                        const float w2 = rayhit.hit.v;
                        const float w0 = 1.0f - w1 - w2;

                        const float * n0 = normals_.data() + 9 * fid + 0;
                        const float * n1 = normals_.data() + 9 * fid + 3;
                        const float * n2 = normals_.data() + 9 * fid + 6;
                        float N[3] = {
                            w0*n0[0] + w1*n1[0] + w2*n2[0],
                            w0*n0[1] + w1*n1[1] + w2*n2[1],
                            w0*n0[2] + w1*n1[2] + w2*n2[2]
                        };
                        emb_normalize3(N);

                        const float dir[3] = { d[0], d[1], d[2] };
                        const float V[3]   = { -dir[0], -dir[1], -dir[2] };

                        const float t = rayhit.ray.tfar;
                        const float hitPt[3] = {
                            p0[0] + dir[0] * t,
                            p0[1] + dir[1] * t,
                            p0[2] + dir[2] * t
                        };

                        const SoRtMaterial & mat = tris[fid].mat;

                        float px[3] = { 0.0f, 0.0f, 0.0f };
                        std::vector<float> dummy;
                        emb_shade(embreeScene_, dummy, hitPt, N, V, mat,
                                  lights, ambientFill, shadowsEnabled, px);

                        if (maxBouncesAllowed > 0) {
                            const float specLum =
                                (mat.specular[0] * 0.2126f +
                                 mat.specular[1] * 0.7152f +
                                 mat.specular[2] * 0.0722f) * mat.shininess;
                            if (specLum > 0.01f) {
                                const float NdotI = emb_dot3(N, dir);
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
                                emb_trace(embreeScene_, tris, normals_,
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
                        emb_clamp01(accum[0] * inv_s) * 255.0f);
                    pixels[idx + 1] = static_cast<unsigned char>(
                        emb_clamp01(accum[1] * inv_s) * 255.0f);
                    pixels[idx + 2] = static_cast<unsigned char>(
                        emb_clamp01(accum[2] * inv_s) * 255.0f);
                    if (nrcomponents == 4) pixels[idx + 3] = 255;
                }
            }
        }

        // --- 4. Composite text/HUD overlays ---------------------------------
        collector_.compositeOverlays(pixels, width, height, nrcomponents);

        collector_.updateCameraId(cam);
        return TRUE;
    }

private:
    SoRaytracerSceneCollector collector_;
    EmbreeScene               embreeScene_;
    std::vector<float>        normals_;   // per-triangle vertex normals (9 floats/tri)
    bool                      cachedShadowsEnabled_    = false;
    int                       cachedMaxBouncesAllowed_ = 0;
    int                       cachedSamplesPerPixel_   = 1;
    float                     cachedAmbientFill_       = 0.20f;
    unsigned int              displayVpW_              = 0;
    unsigned int              displayVpH_              = 0;
};

#endif // OBOL_EMBREE_CONTEXT_MANAGER_H
