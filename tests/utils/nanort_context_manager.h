/*
 * nanort_context_manager.h
 *
 * SoNanoRTContextManager – a SoDB::ContextManager specialization that uses
 * nanort for CPU ray-triangle intersection instead of OpenGL.
 *
 * This is the reference implementation of the generalized context-manager
 * rendering path introduced in SoDB::ContextManager::renderScene().  By
 * providing this as the context manager at SoDB::init() time, the entire
 * SoOffscreenRenderer pipeline (render() → getBuffer() → writeToRGB()) works
 * without any OpenGL context.
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
 * Obol APIs used internally:
 *   - SoCallbackAction  – extract triangles, normals, materials from scene graph
 *   - SoSearchAction    – find camera and directional lights in scene graph
 *   - SbViewportRegion  – viewport for camera setup
 *   - SoCamera          – get view volume for ray generation
 *   - SbViewVolume      – projectPointToLine() for per-pixel ray directions
 *   - SbMatrix          – transform vertices/normals to world space
 *
 * Rendering features implemented:
 *   - Perspective and orthographic cameras (via SoCamera / SbViewVolume)
 *   - Directional lights (SoDirectionalLight) – Phong diffuse + specular
 *   - SoMaterial: diffuse, specular, emissive, ambient, shininess
 *   - All triangle-generating shapes (SoSphere, SoCube, SoCone, SoCylinder,
 *     SoFaceSet, SoIndexedFaceSet, SoVertexProperty, …)
 *   - Any shape-local or per-face material bindings
 *   - All rigid-body transforms (SoTranslation, SoRotation, SoScale, …)
 *   - SoSeparator / SoGroup scene-graph structure
 *
 * Rendering features NOT supported (fall back to rendering nothing extra):
 *   - SoText2 (screen-aligned 2D text; requires rasterisation;
 *     use SoText2::getTextQuad() to obtain a bounding quad for the text)
 *   - Textures (texture images not applied)
 *   - SoGLRenderAction-specific effects (shadow maps, FBOs, GLSL shaders)
 *   - SoPath rendering (only SoNode* root is handled; SoPath falls through
 *     to GL unless an SoNode is available)
 *
 * Rendering features with proxy-geometry fallback:
 *   - SoLineSet, SoIndexedLineSet – rendered as thin cylinders via
 *     SoLineSet::createCylinderProxy() / SoIndexedLineSet::createCylinderProxy()
 *   - SoPointSet – rendered as small spheres via SoPointSet::createSphereProxy()
 *   - SoText3 – uses generatePrimitives() which produces extruded triangle
 *     geometry directly; no extra work needed
 *
 * Dependencies:
 *   - nanort.h (external/nanort/nanort.h)
 *   - stb_image_write.h is NOT needed here (pixel writing is done by Coin)
 */

#ifndef COIN_NANORT_CONTEXT_MANAGER_H
#define COIN_NANORT_CONTEXT_MANAGER_H

// ---- Obol/Coin includes ---------------------------------------------------
#include <Inventor/SoDB.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbViewVolume.h>
#include <Inventor/SbVec2f.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbColor.h>
#include <Inventor/actions/SoCallbackAction.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/nodes/SoCamera.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoShape.h>
#include <Inventor/nodes/SoLineSet.h>
#include <Inventor/nodes/SoIndexedLineSet.h>
#include <Inventor/nodes/SoPointSet.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoMatrixTransform.h>
#include <Inventor/elements/SoCoordinateElement.h>
#include <Inventor/elements/SoLineWidthElement.h>
#include <Inventor/elements/SoPointSizeElement.h>
#include <Inventor/elements/SoViewVolumeElement.h>
#include <Inventor/elements/SoViewportRegionElement.h>
#include <Inventor/SoPrimitiveVertex.h>
#include <Inventor/SoPath.h>

// ---- nanort --------------------------------------------------------------
#include "nanort.h"

// ---- Standard library ----------------------------------------------------
#include <vector>
#include <cmath>
#include <cstring>
#include <algorithm>

// ==========================================================================
// Internal scene-data structures
// ==========================================================================

struct NrtMaterial {
    float diffuse[3];   // RGB [0,1]
    float specular[3];  // RGB [0,1]
    float ambient[3];   // RGB [0,1]
    float emission[3];  // RGB [0,1]
    float shininess;    // [0,1] – multiply by 128 for Phong exponent
};

struct NrtTriangle {
    float pos[3][3];    // world-space positions  [vertex 0/1/2][x/y/z]
    float norm[3][3];   // world-space normals    [vertex 0/1/2][x/y/z]
    NrtMaterial mat;
};

struct NrtSceneCollector {
    std::vector<NrtTriangle> tris;
};

// ==========================================================================
// Triangle callback – called by SoCallbackAction for every triangle
// ==========================================================================

static void nrtTriangleCB(void * userdata,
                          SoCallbackAction * action,
                          const SoPrimitiveVertex * v0,
                          const SoPrimitiveVertex * v1,
                          const SoPrimitiveVertex * v2)
{
    NrtSceneCollector * col = static_cast<NrtSceneCollector *>(userdata);

    const SbMatrix & mm = action->getModelMatrix();
    SbMatrix normalMat   = mm.inverse().transpose();

    NrtTriangle tri;
    const SoPrimitiveVertex * verts[3] = { v0, v1, v2 };
    for (int i = 0; i < 3; ++i) {
        SbVec3f wp, wn;
        mm.multVecMatrix(verts[i]->getPoint(),   wp);
        normalMat.multDirMatrix(verts[i]->getNormal(), wn);
        wn.normalize();
        tri.pos[i][0]  = wp[0];  tri.pos[i][1]  = wp[1];  tri.pos[i][2]  = wp[2];
        tri.norm[i][0] = wn[0];  tri.norm[i][1] = wn[1];  tri.norm[i][2] = wn[2];
    }

    SbColor ambient, diffuse, specular, emission;
    float shininess, transparency;
    action->getMaterial(ambient, diffuse, specular, emission,
                        shininess, transparency,
                        v0->getMaterialIndex());

    tri.mat.diffuse[0]  = diffuse[0];  tri.mat.diffuse[1]  = diffuse[1];  tri.mat.diffuse[2]  = diffuse[2];
    tri.mat.specular[0] = specular[0]; tri.mat.specular[1] = specular[1]; tri.mat.specular[2] = specular[2];
    tri.mat.ambient[0]  = ambient[0];  tri.mat.ambient[1]  = ambient[1];  tri.mat.ambient[2]  = ambient[2];
    tri.mat.emission[0] = emission[0]; tri.mat.emission[1] = emission[1]; tri.mat.emission[2] = emission[2];
    tri.mat.shininess   = shininess;

    col->tris.push_back(tri);
}

// ==========================================================================
// Proxy-geometry callbacks – fired for shape types that produce no triangles
// ==========================================================================
// These pre-callbacks intercept SoLineSet, SoIndexedLineSet and SoPointSet
// nodes during SoCallbackAction traversal and substitute rendered proxy
// geometry (cylinders for lines, spheres for points) so that ray-tracing
// backends can visualise them.

struct NrtProxyData {
    NrtSceneCollector * collector;
    SbViewportRegion    vp;
};

// Helper: wrap \a proxy in a separator that injects the current material and
// model matrix from \a action, then collect its triangles into \a collector.
static void nrtCollectProxy(SoCallbackAction * action,
                            NrtProxyData * data,
                            SoSeparator * proxy)
{
    // Transfer the material currently on the traversal state.
    SbColor ambient, diffuse, specular, emission;
    float   shininess, transparency;
    action->getMaterial(ambient, diffuse, specular, emission,
                        shininess, transparency, 0);

    SoMaterial * mat = new SoMaterial;
    mat->ambientColor .setValue(ambient);
    mat->diffuseColor .setValue(diffuse);
    mat->specularColor.setValue(specular);
    mat->emissiveColor.setValue(emission);
    mat->shininess    .setValue(shininess);
    mat->transparency .setValue(transparency);

    // Apply the current model matrix so the proxy ends up in world space.
    SoMatrixTransform * mt = new SoMatrixTransform;
    mt->matrix.setValue(action->getModelMatrix());

    SoSeparator * wrapper = new SoSeparator;
    wrapper->ref();
    wrapper->addChild(mat);
    wrapper->addChild(mt);
    wrapper->addChild(proxy);

    SoCallbackAction subCba(data->vp);
    subCba.addTriangleCallback(SoShape::getClassTypeId(),
                               nrtTriangleCB, data->collector);
    subCba.apply(wrapper);

    wrapper->unref();
}

// Compute the world-space radius for a line or point given the current state.
static float nrtLineWorldRadius(SoCallbackAction * action,
                                float sizePx,
                                float viewportHeightPx)
{
    const SbViewVolume & vv =
        SoViewVolumeElement::get(action->getState());
    float worldHeight = vv.getHeight();
    // For perspective cameras vv.getHeight() is the frustum height at the
    // near plane, which is typically very small.  Scale to the viewing
    // distance of the current object so that the proxy radius corresponds
    // to sizePx pixels at that depth.
    if (vv.getProjectionType() == SbViewVolume::PERSPECTIVE) {
        const float nearDist = vv.getNearDist();
        if (nearDist > 1e-6f) {
            // Estimate the object's world-space position from the model
            // matrix translation column, then measure its distance from
            // the camera projection point.
            const SbMatrix & mm = action->getModelMatrix();
            const SbVec3f objPos(mm[3][0], mm[3][1], mm[3][2]);
            const float dist = (objPos - vv.getProjectionPoint()).length();
            const float refDist = (dist > nearDist) ? dist : nearDist;
            worldHeight = worldHeight * refDist / nearDist;
        }
    }
    return sizePx * worldHeight / viewportHeightPx * 0.5f;
}

static SoCallbackAction::Response
nrtLineSetPreCB(void * ud, SoCallbackAction * action, const SoNode * node)
{
    NrtProxyData * data = static_cast<NrtProxyData *>(ud);
    const SoLineSet * ls = static_cast<const SoLineSet *>(node);

    const SoCoordinateElement * coords =
        SoCoordinateElement::getInstance(action->getState());
    if (!coords || coords->getNum() == 0)
        return SoCallbackAction::CONTINUE;

    // SoLineWidthElement default is 0 (meaning "use GL default of 1px")
    float lineW = SoLineWidthElement::get(action->getState());
    if (lineW <= 0.0f) lineW = 1.0f;
    const float vpH    = static_cast<float>(
        data->vp.getViewportSizePixels()[1]);
    const float radius = nrtLineWorldRadius(action, lineW, vpH);

    SoSeparator * proxy = ls->createCylinderProxy(coords, radius);
    proxy->ref();
    nrtCollectProxy(action, data, proxy);
    proxy->unref();

    return SoCallbackAction::PRUNE;
}

static SoCallbackAction::Response
nrtIndexedLineSetPreCB(void * ud, SoCallbackAction * action, const SoNode * node)
{
    NrtProxyData * data = static_cast<NrtProxyData *>(ud);
    const SoIndexedLineSet * ils =
        static_cast<const SoIndexedLineSet *>(node);

    const SoCoordinateElement * coords =
        SoCoordinateElement::getInstance(action->getState());
    if (!coords || coords->getNum() == 0)
        return SoCallbackAction::CONTINUE;

    float lineW = SoLineWidthElement::get(action->getState());
    if (lineW <= 0.0f) lineW = 1.0f;
    const float vpH    = static_cast<float>(
        data->vp.getViewportSizePixels()[1]);
    const float radius = nrtLineWorldRadius(action, lineW, vpH);

    SoSeparator * proxy = ils->createCylinderProxy(coords, radius);
    proxy->ref();
    nrtCollectProxy(action, data, proxy);
    proxy->unref();

    return SoCallbackAction::PRUNE;
}

static SoCallbackAction::Response
nrtPointSetPreCB(void * ud, SoCallbackAction * action, const SoNode * node)
{
    NrtProxyData * data = static_cast<NrtProxyData *>(ud);
    const SoPointSet * ps = static_cast<const SoPointSet *>(node);

    const SoCoordinateElement * coords =
        SoCoordinateElement::getInstance(action->getState());
    if (!coords || coords->getNum() == 0)
        return SoCallbackAction::CONTINUE;

    // SoPointSizeElement default is 0 (meaning "use GL default of 1px")
    float ptSz = SoPointSizeElement::get(action->getState());
    if (ptSz <= 0.0f) ptSz = 1.0f;
    const float vpH    = static_cast<float>(
        data->vp.getViewportSizePixels()[1]);
    const float radius = nrtLineWorldRadius(action, ptSz, vpH);

    SoSeparator * proxy = ps->createSphereProxy(coords, radius);
    proxy->ref();
    nrtCollectProxy(action, data, proxy);
    proxy->unref();

    return SoCallbackAction::PRUNE;
}

// ==========================================================================
// Flat nanort scene (built from NrtSceneCollector)
// ==========================================================================

struct NrtScene {
    std::vector<float>        vertices; // 9 floats/triangle (3 verts × xyz)
    std::vector<unsigned int> faces;    // 3 indices/triangle (sequential)
    std::vector<float>        normals;  // 9 floats/triangle
    std::vector<NrtTriangle>  tris;     // source data (for shading)
    nanort::BVHAccel<float>   accel;

    bool build()
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

// Phong shading contribution from one directional light.
// N, V, L are unit vectors.  lightIntensity scales the light colour.
// out[3] is accumulated (caller adds contribution).
static void nrt_phong(const float * N, const float * V, const float * L,
                      const NrtMaterial & mat,
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

// ==========================================================================
// SoNanoRTContextManager
// ==========================================================================

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

        // --- 1. Extract triangles via SoCallbackAction ----------------------
        NrtSceneCollector collector;
        {
            NrtProxyData proxyData;
            proxyData.collector = &collector;
            proxyData.vp        = vp;

            SoCallbackAction cba(vp);
            cba.addTriangleCallback(SoShape::getClassTypeId(),
                                    nrtTriangleCB, &collector);
            cba.addPreCallback(SoLineSet::getClassTypeId(),
                               nrtLineSetPreCB, &proxyData);
            cba.addPreCallback(SoIndexedLineSet::getClassTypeId(),
                               nrtIndexedLineSetPreCB, &proxyData);
            cba.addPreCallback(SoPointSet::getClassTypeId(),
                               nrtPointSetPreCB, &proxyData);
            cba.apply(scene);
        }
        if (collector.tris.empty()) {
            // No triangles: return TRUE so the caller uses the
            // background-filled buffer rather than falling through to GL.
            return TRUE;
        }

        // --- 2. Build nanort BVH --------------------------------------------
        NrtScene nrtScene;
        nrtScene.tris = collector.tris;
        if (!nrtScene.build()) return FALSE;

        // --- 3. Extract directional lights from scene -----------------------
        struct LightInfo { float dir[3]; float rgb[3]; float intensity; };
        std::vector<LightInfo> lights;
        {
            SoSearchAction sa;
            sa.setType(SoDirectionalLight::getClassTypeId());
            sa.setInterest(SoSearchAction::ALL);
            sa.apply(scene);
            const SoPathList & paths = sa.getPaths();
            for (int i = 0; i < paths.getLength(); ++i) {
                const SoDirectionalLight * dl =
                    static_cast<const SoDirectionalLight *>(
                        paths[i]->getTail());
                if (!dl->on.getValue()) continue;
                LightInfo li;
                const SbVec3f & d = dl->direction.getValue();
                li.dir[0] = d[0]; li.dir[1] = d[1]; li.dir[2] = d[2];
                const SbColor & c = dl->color.getValue();
                li.rgb[0] = c[0]; li.rgb[1] = c[1]; li.rgb[2] = c[2];
                li.intensity = dl->intensity.getValue();
                lights.push_back(li);
            }
        }

        // --- 4. Extract view volume from camera in scene --------------------
        // Prefer an SoPerspectiveCamera; fall back to any SoCamera.
        SoCamera * cam = nullptr;
        {
            SoSearchAction sa;
            sa.setType(SoCamera::getClassTypeId());
            sa.setInterest(SoSearchAction::FIRST);
            sa.apply(scene);
            if (sa.getPath())
                cam = static_cast<SoCamera *>(sa.getPath()->getTail());
        }
        if (!cam) return FALSE;  // no camera: fall through to GL

        const SbViewVolume vv = cam->getViewVolume(
            static_cast<float>(width) / static_cast<float>(height));

        // --- 5. Raytrace ----------------------------------------------------
        nanort::TriangleIntersector<float> intersector(
            nrtScene.vertices.data(), nrtScene.faces.data(),
            sizeof(float) * 3);

        const float kAmbientFill = 0.12f;

        for (unsigned int y = 0; y < height; ++y) {
            for (unsigned int x = 0; x < width; ++x) {
                // Map pixel (x, y) to normalised view coordinates.
                // GL convention: row 0 of the buffer is the BOTTOM of the
                // screen (matching glReadPixels), so ny increases upward.
                // SbViewVolume::projectPointToLine() uses (0,0)=bottom-left,
                // (1,1)=top-right, which matches this convention directly.
                const float nx = (x + 0.5f) / static_cast<float>(width);
                const float ny = (y + 0.5f) / static_cast<float>(height);

                // Camera ray via Obol's view-volume math
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
                const bool hit =
                    nrtScene.accel.Traverse(ray, intersector, &isect);

                const size_t idx = (y * width + x) * nrcomponents;
                if (!hit) {
                    // Already filled with background colour before this call
                    continue;
                }

                // Barycentric interpolation: (1-u-v)*v0 + u*v1 + v*v2
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
                {
                    const float nlen = std::sqrt(N[0]*N[0]+N[1]*N[1]+N[2]*N[2]);
                    if (nlen > 1e-6f) {
                        N[0] /= nlen; N[1] /= nlen; N[2] /= nlen;
                    }
                }
                // Face towards the viewer
                const float V[3] = { -d[0], -d[1], -d[2] };
                if (nrt_dot3(N, V) < 0.0f) {
                    N[0] = -N[0]; N[1] = -N[1]; N[2] = -N[2];
                }

                const NrtMaterial & mat = nrtScene.tris[fid].mat;

                // Emission + ambient fill
                float px[3] = {
                    mat.emission[0] + kAmbientFill * mat.ambient[0],
                    mat.emission[1] + kAmbientFill * mat.ambient[1],
                    mat.emission[2] + kAmbientFill * mat.ambient[2]
                };

                // Accumulate directional lights
                for (const LightInfo & li : lights) {
                    float L[3] = { -li.dir[0], -li.dir[1], -li.dir[2] };
                    const float llen = std::sqrt(L[0]*L[0]+L[1]*L[1]+L[2]*L[2]);
                    if (llen > 1e-6f) {
                        L[0] /= llen; L[1] /= llen; L[2] /= llen;
                    }
                    nrt_phong(N, V, L, mat, li.rgb, li.intensity, px);
                }

                // Fallback: if no lights, simple diffuse-from-eye
                if (lights.empty()) {
                    const float NdotV = nrt_clamp01(nrt_dot3(N, V));
                    px[0] = mat.diffuse[0] * (kAmbientFill + (1.0f - kAmbientFill) * NdotV);
                    px[1] = mat.diffuse[1] * (kAmbientFill + (1.0f - kAmbientFill) * NdotV);
                    px[2] = mat.diffuse[2] * (kAmbientFill + (1.0f - kAmbientFill) * NdotV);
                }

                pixels[idx + 0] = static_cast<unsigned char>(nrt_clamp01(px[0]) * 255.0f);
                pixels[idx + 1] = static_cast<unsigned char>(nrt_clamp01(px[1]) * 255.0f);
                pixels[idx + 2] = static_cast<unsigned char>(nrt_clamp01(px[2]) * 255.0f);
                if (nrcomponents == 4) pixels[idx + 3] = 255;
            }
        }

        return TRUE;
    }
};

#endif // COIN_NANORT_CONTEXT_MANAGER_H
