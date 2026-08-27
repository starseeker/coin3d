/*
 * SoNanoRTContextManagerP.h
 *
 * SoNanoRTContextManager – an application context manager that uses
 * nanort for CPU ray-triangle intersection instead of OpenGL.
 *
 * Scene collection (geometry extraction, proxy shapes, text overlays, lights)
 * is handled by SoSceneCollector from the Obol library.  Only the
 * nanort-specific BVH construction, ray-triangle intersection, and Phong
 * shading remain here.
 *
 * Usage:
 *
 *   #include <Obol/render/SoNanoRTContextManager.h>
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
 * Obol APIs used internally (via SoSceneCollector):
 *   - SoCallbackAction  – extract triangles, normals, materials from scene graph
 *   - SoSearchAction    – find camera, SoSceneRendererParams in scene graph
 *   - SbViewportRegion  – viewport for camera setup
 *   - SoCamera          – get view volume for ray generation
 *   - SbViewVolume      – projectPointToLine() for per-pixel ray directions
 *   - SbMatrix          – transform vertices/normals/positions to world space
 *   - SoSceneRendererParams – rendering hints (shadows, reflections, AA, ambient)
 *
 * Rendering features implemented:
 *   - Perspective and orthographic cameras (via SoCamera / SbViewVolume)
 *   - All light types (via SoSceneCollector light collection)
 *   - SoMaterial: diffuse, specular, emissive, ambient, shininess
 *   - All triangle-generating shapes via SoSceneCollector
 *   - SoSceneRendererParams: hard shadows, specular reflections, AA, ambient fill
 *   - SoText2, SoHUDLabel, SoHUDButton overlays via SoSceneCollector
 *
 * Dependencies:
 *   - SoSceneCollector (Obol library)
 *   - nanort.h (external/nanort/nanort.h)
 */

#ifndef OBOL_SO_NANORT_CONTEXT_MANAGER_P_H
#define OBOL_SO_NANORT_CONTEXT_MANAGER_P_H

#include <Obol/render/SoNanoRTContextManager.h>

// ---- Obol generic raytracing infrastructure ---------------------------------
#include <Inventor/SoSceneCollector.h>
#include <Inventor/SoDB.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbViewVolume.h>
#include <Inventor/SbVec2f.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/nodes/SoCamera.h>
#include <Inventor/nodes/SoSceneRendererParams.h>

// ---- nanort -----------------------------------------------------------------
#include "nanort.h"

// ---- Standard library -------------------------------------------------------
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>

// These types are members of SoNanoRTContextManager::Impl below, so they need
// a named linkage context. GCC diagnoses anonymous-namespace member types as
// -Wsubobject-linkage when this private header is compiled in a unity TU.
namespace ObolNanoRTDetail {

// ==========================================================================
// NrtScene: nanort BVH built from SoScTriangle data
// ==========================================================================
// Holds the flat vertex/face/normal arrays consumed by nanort plus the built
// BVH acceleration structure.  Material and interpolated-normal lookups use
// the SoScTriangle vector from SoSceneCollector directly so this
// struct only owns the intersection-query data.

struct NrtScene {
    std::vector<float>        vertices; // 9 floats/triangle (3 verts × xyz)
    std::vector<unsigned int> faces;    // 3 indices/triangle (sequential)
    std::vector<float>        normals;  // 9 floats/triangle
    nanort::BVHAccel<float>   accel;

    bool build(const std::vector<SoScTriangle> & tris)
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
                      const SoScMaterial & mat,
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
                       const SoScMaterial & mat,
                       const std::vector<SoScLightInfo> & lights,
                       float ambientFill,
                       bool shadowsEnabled,
                       float out[3])
{
    out[0] = mat.emission[0] + ambientFill * mat.ambient[0];
    out[1] = mat.emission[1] + ambientFill * mat.ambient[1];
    out[2] = mat.emission[2] + ambientFill * mat.ambient[2];

    for (const SoScLightInfo & li : lights) {
        float L[3];
        float attenuation = li.intensity;
        float shadowMaxT  = 1.0e30f;

        if (li.type == SO_SC_DIRECTIONAL) {
            L[0] = -li.dir[0]; L[1] = -li.dir[1]; L[2] = -li.dir[2];
            nrt_normalize3(L);
        } else if (li.type == SO_SC_POINT) {
            L[0] = li.pos[0] - hitPt[0];
            L[1] = li.pos[1] - hitPt[1];
            L[2] = li.pos[2] - hitPt[2];
            const float dist = std::sqrt(L[0]*L[0] + L[1]*L[1] + L[2]*L[2]);
            if (dist < 1e-6f) continue;
            L[0] /= dist; L[1] /= dist; L[2] /= dist;
            const float d2 = dist * dist;
            attenuation = li.intensity / (1.0f + d2);
            shadowMaxT  = dist;
        } else { // SO_SC_SPOT
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
                       const std::vector<SoScTriangle> & tris,
                       const float org[3], const float dir[3],
                       const std::vector<SoScLightInfo> & lights,
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

    const SoScMaterial & mat = tris[fid].mat;

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
// Persistent row executor
// ==========================================================================

class NrtRowExecutor {
public:
    NrtRowExecutor() = default;

    ~NrtRowExecutor()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
            ++generation_;
        }
        workAvailable_.notify_all();
        for (std::thread & worker : workers_) worker.join();
    }

    NrtRowExecutor(const NrtRowExecutor &) = delete;
    NrtRowExecutor & operator=(const NrtRowExecutor &) = delete;

    void run(unsigned int rows, std::function<void(unsigned int)> task)
    {
        if (rows == 0) return;
        this->ensureWorkers(rows);
        if (workers_.empty()) {
            for (unsigned int row = 0; row < rows; ++row) task(row);
            return;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            task_ = std::move(task);
            rowCount_ = rows;
            nextRow_.store(0, std::memory_order_relaxed);
            cancelled_.store(false, std::memory_order_relaxed);
            exception_ = nullptr;
            activeWorkers_ = workers_.size();
            ++generation_;
        }
        workAvailable_.notify_all();
        this->processRows();

        std::unique_lock<std::mutex> lock(mutex_);
        workComplete_.wait(lock, [this] { return activeWorkers_ == 0; });
        std::exception_ptr failure = exception_;
        task_ = {};
        lock.unlock();
        if (failure) std::rethrow_exception(failure);
    }

private:
    void ensureWorkers(unsigned int rows)
    {
        const unsigned int concurrency = std::thread::hardware_concurrency();
        const unsigned int desired = concurrency > 1
            ? std::min(concurrency - 1, rows - 1)
            : 0;
        try {
            workers_.reserve(desired);
            while (workers_.size() < desired) {
                workers_.emplace_back([this] { this->workerLoop(); });
            }
        }
        catch (...) {
            // Keep workers already created.  The caller thread can always
            // render every row if worker creation is partially unavailable.
        }
    }

    void processRows()
    {
        try {
            while (!cancelled_.load(std::memory_order_relaxed)) {
                const unsigned int row =
                    nextRow_.fetch_add(1, std::memory_order_relaxed);
                if (row >= rowCount_) break;
                task_(row);
            }
        }
        catch (...) {
            cancelled_.store(true, std::memory_order_relaxed);
            std::lock_guard<std::mutex> lock(mutex_);
            if (!exception_) exception_ = std::current_exception();
        }
    }

    void workerLoop()
    {
        size_t observedGeneration = 0;
        for (;;) {
            {
                std::unique_lock<std::mutex> lock(mutex_);
                workAvailable_.wait(lock, [this, observedGeneration] {
                    return stopping_ || generation_ != observedGeneration;
                });
                if (stopping_) return;
                observedGeneration = generation_;
            }
            this->processRows();
            {
                std::lock_guard<std::mutex> lock(mutex_);
                --activeWorkers_;
                if (activeWorkers_ == 0) workComplete_.notify_one();
            }
        }
    }

    std::vector<std::thread> workers_;
    std::mutex mutex_;
    std::condition_variable workAvailable_;
    std::condition_variable workComplete_;
    std::function<void(unsigned int)> task_;
    std::atomic<unsigned int> nextRow_{0};
    std::atomic<bool> cancelled_{false};
    unsigned int rowCount_ = 0;
    size_t activeWorkers_ = 0;
    size_t generation_ = 0;
    bool stopping_ = false;
    std::exception_ptr exception_;
};

} // namespace ObolNanoRTDetail

using ObolNanoRTDetail::NrtRowExecutor;
using ObolNanoRTDetail::NrtScene;
using ObolNanoRTDetail::nrt_clamp01;
using ObolNanoRTDetail::nrt_dot3;
using ObolNanoRTDetail::nrt_normalize3;
using ObolNanoRTDetail::nrt_rand01;
using ObolNanoRTDetail::nrt_shade;
using ObolNanoRTDetail::nrt_trace;

// ==========================================================================
// SoNanoRTContextManager
// ==========================================================================
//
// Scene collection is delegated to SoSceneCollector which manages
// the SoCallbackAction setup, proxy geometry, text overlays, light extraction,
// and scene-change cache.  This class retains only nanort-specific state:
// the built BVH (NrtScene) and the cached SoSceneRendererParams settings.
//
// Thread safety: not thread-safe; use from a single thread.

class SoNanoRTContextManager::Impl {
public:
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
        cachedRenderW_           = 0;
        cachedRenderH_           = 0;
        cachedProxyW_            = 0;
        cachedProxyH_            = 0;
    }

    // -----------------------------------------------------------------------
    // Display viewport for proxy geometry sizing
    // -----------------------------------------------------------------------

    // Inform the renderer of the full display dimensions so that line/point/
    // cylinder proxy geometry is always sized for the real display resolution
    // even when renderScene() is called at a reduced (coarse) resolution.
    // Pass (0, 0) to revert to using the render dimensions (the default).
    void setDisplayViewport(unsigned int w, unsigned int h) {
        if (displayVpW_ == w && displayVpH_ == h) return;
        displayVpW_ = w;
        displayVpH_ = h;
        this->resetCache();
    }

    // -----------------------------------------------------------------------
    // Rendering path
    // -----------------------------------------------------------------------
    SbBool renderScene(SoNode * scene,
                       unsigned int width, unsigned int height,
                       unsigned char * pixels,
                       unsigned int nrcomponents,
                       const float background_rgb[3])
    {
        if (!scene) return FALSE;

        const unsigned int proxyW = displayVpW_ > 0 ? displayVpW_ : width;
        const unsigned int proxyH = displayVpH_ > 0 ? displayVpH_ : height;
        if (cachedRenderW_ != width || cachedRenderH_ != height ||
            cachedProxyW_ != proxyW || cachedProxyH_ != proxyH) {
            this->resetCache();
            cachedRenderW_ = width;
            cachedRenderH_ = height;
            cachedProxyW_ = proxyW;
            cachedProxyH_ = proxyH;
        }

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

            // Cache SoSceneRendererParams (any change bumps root nodeId → rebuild)
            cachedShadowsEnabled_    = false;
            cachedMaxBouncesAllowed_ = 0;
            cachedSamplesPerPixel_   = 1;
            cachedAmbientFill_       = 0.20f;
            {
                SoSearchAction sa;
                sa.setType(SoSceneRendererParams::getClassTypeId());
                sa.setInterest(SoSearchAction::FIRST);
                sa.apply(scene);
                if (sa.getPath()) {
                    const SoSceneRendererParams * rp =
                        static_cast<const SoSceneRendererParams *>(
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

        const std::vector<SoScTriangle>  & tris   = collector_.getTriangles();
        const std::vector<SoScLightInfo> & lights  = collector_.getLights();

        // --- 4. Raytrace ----------------------------------------------------
        if (!tris.empty()) {
            if (!cam) return FALSE;

            const float aspect_ratio =
                static_cast<float>(width) / static_cast<float>(height);
            SbViewVolume vv = cam->getViewVolume(aspect_ratio);
            if (aspect_ratio < 1.0f) vv.scale(1.0f / aspect_ratio);

            NrtScene & nrtScene = cachedNrtScene_;
            /* NOTE: TriangleIntersector has mutable per-call state (ray_org_,
             * ray_coeff_, etc.) so it MUST NOT be shared across threads.
             * Each worker thread creates its own intersector from the same
             * (read-only) vertex/face pointers; construction is O(1). */

            /* Precompute 4 corner rays once per frame.
             * projectPointToLine() is linear in (nx,ny) for both perspective
             * and orthographic projections, so bilinear interpolation over
             * the 4 corners is exact for any pixel within the frame.
             * This eliminates one projectPointToLine() call per pixel
             * (replacing ~width*height double-precision ray computations with
             * 4 calls + simple float arithmetic). */
            SbVec3f corner_p0[4], corner_p1[4];
            vv.projectPointToLine(SbVec2f(0.0f, 0.0f), corner_p0[0], corner_p1[0]);
            vv.projectPointToLine(SbVec2f(1.0f, 0.0f), corner_p0[1], corner_p1[1]);
            vv.projectPointToLine(SbVec2f(0.0f, 1.0f), corner_p0[2], corner_p1[2]);
            vv.projectPointToLine(SbVec2f(1.0f, 1.0f), corner_p0[3], corner_p1[3]);

            /* Decompose corners into additive basis for per-pixel interpolation:
             *   p(nx,ny) = A + B*nx + C*ny + D*nx*ny
             * For the resolutions used by the coarse-render calibration the
             * bilinear term D is typically very small (it vanishes completely
             * for the common case of a symmetric perspective frustum), but is
             * included for correctness with asymmetric frusta. */
            const float fw = static_cast<float>(width);
            const float fh = static_cast<float>(height);

            /* Lambda: render rows [y0, y1) into pixels[]. All captures are by
             * value or reference to variables that outlive the join() call.
             * A thread-local TriangleIntersector is created inside the lambda
             * because nanort::TriangleIntersector holds mutable per-call state
             * (ray coefficients, hit record) that is not thread-safe to share. */
            auto renderBand = [&](unsigned int y0, unsigned int y1) {
                /* Per-thread intersector – constructed from the same read-only
                 * vertex/face arrays that live in nrtScene (O(1) to create). */
                nanort::TriangleIntersector<float> intersector(
                    nrtScene.vertices.data(), nrtScene.faces.data(),
                    sizeof(float) * 3);
                for (unsigned int y = y0; y < y1; ++y) {
                /* Each thread/row uses its own RNG state seeded from y so
                 * AA jitter is deterministic and threads don't share state.
                 * 2654435761 is the Knuth multiplicative hash constant
                 * (2^32 / phi ≈ 2654435769 rounded to nearest odd prime) which
                 * spreads row indices across the 32-bit range before XOR. */
                uint32_t rngState = 0xDEADBEEFu ^ (static_cast<uint32_t>(y) * 2654435761u);

                for (unsigned int x = 0; x < width; ++x) {
                    const float fx = static_cast<float>(x);
                    const float fy = static_cast<float>(y);

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

                        /* Bilinear interpolation of the precomputed corner rays.
                         * (1-nx)*(1-ny)*c[0] + nx*(1-ny)*c[1] + (1-nx)*ny*c[2] + nx*ny*c[3]
                         * Rearranged to use 4 multiplications instead of 8: */
                        const float w00 = (1.0f - nx) * (1.0f - ny);
                        const float w10 = nx            * (1.0f - ny);
                        const float w01 = (1.0f - nx)  * ny;
                        const float w11 = nx            * ny;
                        float p0x = w00*corner_p0[0][0] + w10*corner_p0[1][0]
                                  + w01*corner_p0[2][0] + w11*corner_p0[3][0];
                        float p0y = w00*corner_p0[0][1] + w10*corner_p0[1][1]
                                  + w01*corner_p0[2][1] + w11*corner_p0[3][1];
                        float p0z = w00*corner_p0[0][2] + w10*corner_p0[1][2]
                                  + w01*corner_p0[2][2] + w11*corner_p0[3][2];
                        float p1x = w00*corner_p1[0][0] + w10*corner_p1[1][0]
                                  + w01*corner_p1[2][0] + w11*corner_p1[3][0];
                        float p1y = w00*corner_p1[0][1] + w10*corner_p1[1][1]
                                  + w01*corner_p1[2][1] + w11*corner_p1[3][1];
                        float p1z = w00*corner_p1[0][2] + w10*corner_p1[1][2]
                                  + w01*corner_p1[2][2] + w11*corner_p1[3][2];

                        float dx = p1x - p0x, dy_ = p1y - p0y, dz = p1z - p0z;
                        const float invLen = 1.0f / std::sqrt(dx*dx + dy_*dy_ + dz*dz);
                        dx *= invLen; dy_ *= invLen; dz *= invLen;

                        nanort::Ray<float> ray;
                        ray.org[0] = p0x; ray.org[1] = p0y; ray.org[2] = p0z;
                        ray.dir[0] = dx;  ray.dir[1] = dy_;  ray.dir[2] = dz;
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

                        const float dir[3] = { dx, dy_, dz };
                        const float V[3]   = { -dx, -dy_, -dz };

                        const float hitPt[3] = {
                            p0x + dx * isect.t,
                            p0y + dy_ * isect.t,
                            p0z + dz * isect.t
                        };

                        const SoScMaterial & mat = tris[fid].mat;

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
                    const float red = nrt_clamp01(accum[0] * inv_s);
                    const float green = nrt_clamp01(accum[1] * inv_s);
                    const float blue = nrt_clamp01(accum[2] * inv_s);
                    if (nrcomponents <= 2) {
                        // Match SoOffscreenRenderer's GL luminance conversion.
                        pixels[idx] = static_cast<unsigned char>(
                            std::min(red * 76.0f + green * 150.0f +
                                     blue * 29.0f, 255.0f));
                        if (nrcomponents == 2) pixels[idx + 1] = 255;
                    } else {
                        pixels[idx + 0] = static_cast<unsigned char>(red * 255.0f);
                        pixels[idx + 1] = static_cast<unsigned char>(green * 255.0f);
                        pixels[idx + 2] = static_cast<unsigned char>(blue * 255.0f);
                        if (nrcomponents == 4) pixels[idx + 3] = 255;
                    }
                }
                } /* end row loop inside renderBand */
            }; /* end renderBand lambda */

            rowExecutor_.run(height, [&](unsigned int row) {
                renderBand(row, row + 1);
            });
        }

        // --- 5. Composite text/HUD overlays ---------------------------------
        collector_.compositeOverlays(pixels, width, height, nrcomponents);

        collector_.updateCameraId(cam, scene);
        return TRUE;
    }

private:
    SoSceneCollector collector_;
    NrtScene                  cachedNrtScene_;
    bool                      cachedShadowsEnabled_    = false;
    int                       cachedMaxBouncesAllowed_ = 0;
    int                       cachedSamplesPerPixel_   = 1;
    float                     cachedAmbientFill_       = 0.20f;
    unsigned int              displayVpW_              = 0;
    unsigned int              displayVpH_              = 0;
    unsigned int              cachedRenderW_           = 0;
    unsigned int              cachedRenderH_           = 0;
    unsigned int              cachedProxyW_            = 0;
    unsigned int              cachedProxyH_            = 0;
    NrtRowExecutor            rowExecutor_;
};

#endif // OBOL_SO_NANORT_CONTEXT_MANAGER_P_H
