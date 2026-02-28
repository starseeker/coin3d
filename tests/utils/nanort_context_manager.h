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
 *   - SoSearchAction    – find camera, lights, SoRaytracingParams in scene graph
 *   - SbViewportRegion  – viewport for camera setup
 *   - SoCamera          – get view volume for ray generation
 *   - SbViewVolume      – projectPointToLine() for per-pixel ray directions
 *   - SbMatrix          – transform vertices/normals/positions to world space
 *   - SoRaytracingParams – rendering hints (shadows, reflections, AA, ambient)
 *
 * Rendering features implemented:
 *   - Perspective and orthographic cameras (via SoCamera / SbViewVolume)
 *   - Directional lights (SoDirectionalLight) – Phong diffuse + specular
 *   - Point lights (SoPointLight) – positional with inverse-square attenuation
 *   - Spot lights (SoSpotLight) – cone-limited positional lights
 *   - SoMaterial: diffuse, specular, emissive, ambient, shininess
 *   - All triangle-generating shapes (SoSphere, SoCube, SoCone, SoCylinder,
 *     SoFaceSet, SoIndexedFaceSet, SoVertexProperty, …)
 *   - Any shape-local or per-face material bindings
 *   - All rigid-body transforms (SoTranslation, SoRotation, SoScale, …)
 *   - SoSeparator / SoGroup scene-graph structure
 *   - SoRaytracingParams: hard shadows, specular reflections, AA super-sampling,
 *     configurable ambient fill intensity
 *
 * Rendering features NOT supported (fall back to rendering nothing extra):
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
 *   - SoText2 – screen-aligned text rendered as one screen-aligned quad per
 *     visible glyph via SoText2::buildGlyphQuads(); each quad matches the
 *     pixel footprint of the glyph bitmap at the text anchor depth, with the
 *     per-line justification offsets applied; whitespace chars are skipped
 *
 * Dependencies:
 *   - nanort.h (external/nanort/nanort.h)
 *   - stb_image_write.h is NOT needed here (pixel writing is done by Coin)
 */

#ifndef OBOL_NANORT_CONTEXT_MANAGER_H
#define OBOL_NANORT_CONTEXT_MANAGER_H

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
#include <Inventor/nodes/SoPointLight.h>
#include <Inventor/nodes/SoSpotLight.h>
#include <Inventor/nodes/SoShape.h>
#include <Inventor/nodes/SoLineSet.h>
#include <Inventor/nodes/SoIndexedLineSet.h>
#include <Inventor/nodes/SoPointSet.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoText2.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoMatrixTransform.h>
#include <Inventor/nodes/SoRaytracingParams.h>
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
#include <cstdint>
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

// Per-SoText2-node pixel overlay collected during scene traversal.
// After ray-tracing, these are composited directly onto the framebuffer.
struct NrtTextOverlay {
    std::vector<unsigned char> pixbuf; // RGBA, bottom-to-top rows
    int x, y, w, h;                   // viewport-space bottom-left + size
};

struct NrtProxyData {
    NrtSceneCollector *          collector;
    SbViewportRegion             vp;
    std::vector<NrtTextOverlay>  textOverlays; // collected SoText2 bitmaps
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
// SoText2 pre-callback: per-glyph billboard quads
// ==========================================================================
// SoText2 renders screen-aligned text via GL rasterisation, so its
// generatePrimitives() emits nothing for ray-tracing backends.  This
// pre-callback intercepts each SoText2 node and builds a crisp RGBA pixel
// buffer via SoText2::buildPixelBuffer().  The overlay is stored for
// compositing directly onto the framebuffer after the ray-trace pass,
// which gives letter-accurate glyph shapes (not bounding-box blocks).
static SoCallbackAction::Response
nrtText2PreCB(void * ud, SoCallbackAction * action, const SoNode * node)
{
    NrtProxyData * data = static_cast<NrtProxyData *>(ud);
    const SoText2 * text = static_cast<const SoText2 *>(node);
    SoState * state = action->getState();

    NrtTextOverlay ov;
    if (!text->buildPixelBuffer(state, ov.pixbuf, ov.x, ov.y, ov.w, ov.h))
        return SoCallbackAction::PRUNE;

    data->textOverlays.push_back(std::move(ov));
    return SoCallbackAction::PRUNE;  // generatePrimitives() produces nothing
}

// ==========================================================================
// Light data structures (all light types)
// ==========================================================================

// Discriminator for the supported light types.
enum NrtLightType { NRT_DIRECTIONAL, NRT_POINT, NRT_SPOT };

struct NrtLightInfo {
    NrtLightType type;
    float rgb[3];       // light colour
    float intensity;    // intensity scale
    // Directional lights use dir only (points away from light source).
    // Point lights use pos only.
    // Spot lights use both pos and dir (dir = cone axis direction).
    float dir[3];       // world-space direction (DIRECTIONAL: away from light; SPOT: cone axis)
    float pos[3];       // world-space position (POINT, SPOT)
    float cutOffAngle;  // spot cone half-angle in radians (SPOT only)
    float dropOffRate;  // spot intensity drop-off exponent (SPOT only)
};

// ==========================================================================
// Light collection pre-callbacks (registered on the main SoCallbackAction)
// ==========================================================================

static SoCallbackAction::Response
nrtDirectionalLightCB(void * ud, SoCallbackAction * action, const SoNode * node)
{
    std::vector<NrtLightInfo> * lights =
        static_cast<std::vector<NrtLightInfo> *>(ud);
    const SoDirectionalLight * dl =
        static_cast<const SoDirectionalLight *>(node);
    if (!dl->on.getValue())
        return SoCallbackAction::CONTINUE;

    // Transform direction to world space using the model matrix at
    // the time of traversal.
    const SbMatrix & mm = action->getModelMatrix();
    SbVec3f wDir;
    mm.multDirMatrix(dl->direction.getValue(), wDir);
    // Normalize defensively; the world may have a scaling transform.
    float wlen = wDir.length();
    if (wlen > 1e-6f) wDir /= wlen;

    NrtLightInfo li;
    li.type = NRT_DIRECTIONAL;
    li.dir[0] = wDir[0]; li.dir[1] = wDir[1]; li.dir[2] = wDir[2];
    li.pos[0] = li.pos[1] = li.pos[2] = 0.0f;
    const SbColor & c = dl->color.getValue();
    li.rgb[0] = c[0]; li.rgb[1] = c[1]; li.rgb[2] = c[2];
    li.intensity    = dl->intensity.getValue();
    li.cutOffAngle  = 0.0f;
    li.dropOffRate  = 0.0f;
    lights->push_back(li);
    return SoCallbackAction::CONTINUE;
}

static SoCallbackAction::Response
nrtPointLightCB(void * ud, SoCallbackAction * action, const SoNode * node)
{
    std::vector<NrtLightInfo> * lights =
        static_cast<std::vector<NrtLightInfo> *>(ud);
    const SoPointLight * pl =
        static_cast<const SoPointLight *>(node);
    if (!pl->on.getValue())
        return SoCallbackAction::CONTINUE;

    // Transform position to world space.
    const SbMatrix & mm = action->getModelMatrix();
    SbVec3f wPos;
    mm.multVecMatrix(pl->location.getValue(), wPos);

    NrtLightInfo li;
    li.type = NRT_POINT;
    li.dir[0] = li.dir[1] = li.dir[2] = 0.0f;
    li.pos[0] = wPos[0]; li.pos[1] = wPos[1]; li.pos[2] = wPos[2];
    const SbColor & c = pl->color.getValue();
    li.rgb[0] = c[0]; li.rgb[1] = c[1]; li.rgb[2] = c[2];
    li.intensity   = pl->intensity.getValue();
    li.cutOffAngle = 0.0f;
    li.dropOffRate = 0.0f;
    lights->push_back(li);
    return SoCallbackAction::CONTINUE;
}

static SoCallbackAction::Response
nrtSpotLightCB(void * ud, SoCallbackAction * action, const SoNode * node)
{
    std::vector<NrtLightInfo> * lights =
        static_cast<std::vector<NrtLightInfo> *>(ud);
    const SoSpotLight * sl =
        static_cast<const SoSpotLight *>(node);
    if (!sl->on.getValue())
        return SoCallbackAction::CONTINUE;

    const SbMatrix & mm = action->getModelMatrix();
    SbVec3f wPos, wDir;
    mm.multVecMatrix(sl->location.getValue(),  wPos);
    mm.multDirMatrix(sl->direction.getValue(), wDir);
    float wlen = wDir.length();
    if (wlen > 1e-6f) wDir /= wlen;

    NrtLightInfo li;
    li.type = NRT_SPOT;
    li.dir[0] = wDir[0]; li.dir[1] = wDir[1]; li.dir[2] = wDir[2];
    li.pos[0] = wPos[0]; li.pos[1] = wPos[1]; li.pos[2] = wPos[2];
    const SbColor & c = sl->color.getValue();
    li.rgb[0] = c[0]; li.rgb[1] = c[1]; li.rgb[2] = c[2];
    li.intensity   = sl->intensity.getValue();
    li.cutOffAngle = sl->cutOffAngle.getValue();
    li.dropOffRate = sl->dropOffRate.getValue();
    lights->push_back(li);
    return SoCallbackAction::CONTINUE;
}



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

// Phong shading contribution from one light.
// N, V, L are unit vectors.  lightIntensity scales the light colour.
// out[3] is accumulated in-place (caller initializes and adds contribution).
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

// Test whether a shadow ray from hitPt (offset along N) toward direction L
// is blocked before reaching maxT.
// N      : surface normal at hitPt (used to offset origin away from surface)
// L      : unit vector toward the light
// maxT   : maximum intersection distance (1e30 for directional, finite for positional)
static bool nrt_is_shadowed(const NrtScene & scene,
                             const nanort::TriangleIntersector<float> & isect,
                             const float hitPt[3],
                             const float N[3],
                             const float L[3],
                             float maxT)
{
    nanort::Ray<float> shadowRay;
    // Offset origin along normal to avoid self-intersection.
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
// hitPt  : world-space hit position
// N      : surface normal (unit, facing viewer)
// V      : unit view direction (camera → hit point, negated)
// mat    : surface material
// Returns the RGB contribution for this point (emission + ambient + lights).
static void nrt_shade(const NrtScene & scene,
                       const nanort::TriangleIntersector<float> & isect,
                       const float hitPt[3],
                       const float N[3],
                       const float V[3],
                       const NrtMaterial & mat,
                       const std::vector<NrtLightInfo> & lights,
                       float ambientFill,
                       bool shadowsEnabled,
                       float out[3])
{
    // Emission + ambient fill
    out[0] = mat.emission[0] + ambientFill * mat.ambient[0];
    out[1] = mat.emission[1] + ambientFill * mat.ambient[1];
    out[2] = mat.emission[2] + ambientFill * mat.ambient[2];

    for (const NrtLightInfo & li : lights) {
        float L[3];
        float attenuation = li.intensity;
        float shadowMaxT  = 1.0e30f;

        if (li.type == NRT_DIRECTIONAL) {
            // Direction points away from the light source; negate for 'toward light'.
            // Re-normalize: a scaling transform could have stretched the direction
            // before it was captured in nrtDirectionalLightCB.
            L[0] = -li.dir[0]; L[1] = -li.dir[1]; L[2] = -li.dir[2];
            nrt_normalize3(L);
            // Directional lights are infinitely far; no distance attenuation.

        } else if (li.type == NRT_POINT) {
            // Vector from hit point to light position.
            L[0] = li.pos[0] - hitPt[0];
            L[1] = li.pos[1] - hitPt[1];
            L[2] = li.pos[2] - hitPt[2];
            const float dist = std::sqrt(L[0]*L[0] + L[1]*L[1] + L[2]*L[2]);
            if (dist < 1e-6f) continue;
            L[0] /= dist; L[1] /= dist; L[2] /= dist;
            // Inverse-square attenuation (avoid div-by-zero for very close lights).
            const float d2 = dist * dist;
            attenuation = li.intensity / (1.0f + d2);
            shadowMaxT  = dist;

        } else { // NRT_SPOT
            L[0] = li.pos[0] - hitPt[0];
            L[1] = li.pos[1] - hitPt[1];
            L[2] = li.pos[2] - hitPt[2];
            const float dist = std::sqrt(L[0]*L[0] + L[1]*L[1] + L[2]*L[2]);
            if (dist < 1e-6f) continue;
            L[0] /= dist; L[1] /= dist; L[2] /= dist;

            // Spot cone check: dot product of cone axis (li.dir) with direction from
            // surface toward light (-L).  li.dir points from the light outward.
            const float cosHalfCone = std::cos(li.cutOffAngle);
            const float cosSurface  = -nrt_dot3(li.dir, L); // cos of angle to cone axis
            if (cosSurface < cosHalfCone)
                continue;  // outside the cone: no contribution

            // Smooth edge with drop-off
            const float spotFactor = (li.dropOffRate > 0.0f)
                ? std::pow(cosSurface, li.dropOffRate)
                : 1.0f;
            const float d2  = dist * dist;
            attenuation = li.intensity * spotFactor / (1.0f + d2);
            shadowMaxT  = dist;
        }

        // Shadow test (skip if shadowsEnabled is false)
        if (shadowsEnabled && nrt_is_shadowed(scene, isect, hitPt, N, L, shadowMaxT))
            continue;

        nrt_phong(N, V, L, mat, li.rgb, attenuation, out);
    }

    // Fallback if no lights: simple diffuse-from-eye
    if (lights.empty()) {
        const float NdotV = nrt_clamp01(nrt_dot3(N, V));
        out[0] = mat.diffuse[0] * (ambientFill + (1.0f - ambientFill) * NdotV);
        out[1] = mat.diffuse[1] * (ambientFill + (1.0f - ambientFill) * NdotV);
        out[2] = mat.diffuse[2] * (ambientFill + (1.0f - ambientFill) * NdotV);
    }
}

// Trace a ray through the scene and return the RGB colour (background or shaded hit).
// depth: number of additional reflection bounces allowed (0 = no reflections).
// bgColor: background colour to use for misses.
static void nrt_trace(const NrtScene & scene,
                       const nanort::TriangleIntersector<float> & intersector,
                       const float org[3], const float dir[3],
                       const std::vector<NrtLightInfo> & lights,
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

    // Interpolated normal
    const float * n0 = scene.normals.data() + 9 * fid + 0;
    const float * n1 = scene.normals.data() + 9 * fid + 3;
    const float * n2 = scene.normals.data() + 9 * fid + 6;
    float N[3] = {
        w0*n0[0] + w1*n1[0] + w2*n2[0],
        w0*n0[1] + w1*n1[1] + w2*n2[1],
        w0*n0[2] + w1*n1[2] + w2*n2[2]
    };
    nrt_normalize3(N);

    // View direction (from hit toward camera)
    const float V[3] = { -dir[0], -dir[1], -dir[2] };
    // Flip normal if it faces away from the viewer (back-face)
    if (nrt_dot3(N, V) < 0.0f) { N[0] = -N[0]; N[1] = -N[1]; N[2] = -N[2]; }

    // World-space hit position: p0 + t * dir
    const float hitPt[3] = {
        org[0] + dir[0] * isect.t,
        org[1] + dir[1] * isect.t,
        org[2] + dir[2] * isect.t
    };

    const NrtMaterial & mat = scene.tris[fid].mat;

    // Shade the direct illumination component
    nrt_shade(scene, intersector, hitPt, N, V, mat, lights,
              ambientFill, shadowsEnabled, out);

    // Reflection bounce (only when depth > 0 and surface is specular)
    if (depth > 0) {
        // Specular reflectance proxy: use the luminance of the specular colour
        // scaled by shininess.  Only trace if there is a meaningful contribution.
        const float specLum = (mat.specular[0] * 0.2126f +
                               mat.specular[1] * 0.7152f +
                               mat.specular[2] * 0.0722f) * mat.shininess;
        if (specLum > 0.01f) {
            // Reflection direction R = dir - 2*(N·dir)*N
            const float NdotI = nrt_dot3(N, dir);
            const float Rdir[3] = {
                dir[0] - 2.0f * NdotI * N[0],
                dir[1] - 2.0f * NdotI * N[1],
                dir[2] - 2.0f * NdotI * N[2]
            };
            // Offset origin along normal to avoid self-intersection
            const float kEps = 1e-3f;
            const float Rorg[3] = {
                hitPt[0] + N[0] * kEps,
                hitPt[1] + N[1] * kEps,
                hitPt[2] + N[2] * kEps
            };
            float reflColor[3];
            nrt_trace(scene, intersector, Rorg, Rdir, lights,
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

        // --- 1. Extract triangles and all light types in one traversal ------
        NrtSceneCollector collector;
        std::vector<NrtLightInfo> lights;
        // proxyData is declared outside the inner scope so that textOverlays
        // (filled during SoCallbackAction traversal) remain accessible for
        // the compositing step in --- 6 below.
        NrtProxyData proxyData;
        {
            proxyData.collector = &collector;
            proxyData.vp        = vp;

            SoCallbackAction cba(vp);
            // Geometry
            cba.addTriangleCallback(SoShape::getClassTypeId(),
                                    nrtTriangleCB, &collector);
            // Proxy geometry for lines/points
            cba.addPreCallback(SoLineSet::getClassTypeId(),
                               nrtLineSetPreCB, &proxyData);
            cba.addPreCallback(SoIndexedLineSet::getClassTypeId(),
                               nrtIndexedLineSetPreCB, &proxyData);
            cba.addPreCallback(SoPointSet::getClassTypeId(),
                               nrtPointSetPreCB, &proxyData);
            cba.addPreCallback(SoText2::getClassTypeId(),
                               nrtText2PreCB, &proxyData);
            // All supported light types (with world-space transforms)
            cba.addPreCallback(SoDirectionalLight::getClassTypeId(),
                               nrtDirectionalLightCB, &lights);
            cba.addPreCallback(SoPointLight::getClassTypeId(),
                               nrtPointLightCB, &lights);
            cba.addPreCallback(SoSpotLight::getClassTypeId(),
                               nrtSpotLightCB, &lights);
            cba.apply(scene);
        }
        if (collector.tris.empty()) {
            // No ray-traceable triangles: return TRUE so the caller uses the
            // background-filled buffer rather than falling through to GL.
            // Note: SoText2 overlays need geometry in the scene to get here;
            // a text-only NanoRT scene would also have no triangle hits, so we
            // skip the ray-trace but still composite any collected overlays.
            if (proxyData.textOverlays.empty())
                return TRUE;
        }

        // --- 2-5. BVH build and ray-trace (skipped if scene has no triangles) ---
        if (!collector.tris.empty()) {
        // --- 2. Build nanort BVH --------------------------------------------
        NrtScene nrtScene;
        nrtScene.tris = collector.tris;
        if (!nrtScene.build()) return FALSE;

        // --- 3. Read SoRaytracingParams hints from scene --------------------
        bool  shadowsEnabled     = false;
        int   maxBouncesAllowed  = 0;
        int   samplesPerPixel    = 1;
        float ambientFill        = 0.20f;
        {
            SoSearchAction sa;
            sa.setType(SoRaytracingParams::getClassTypeId());
            sa.setInterest(SoSearchAction::FIRST);
            sa.apply(scene);
            if (sa.getPath()) {
                const SoRaytracingParams * rp =
                    static_cast<const SoRaytracingParams *>(
                        sa.getPath()->getTail());
                shadowsEnabled    = rp->shadowsEnabled.getValue() != FALSE;
                maxBouncesAllowed = rp->maxReflectionBounces.getValue();
                samplesPerPixel   = rp->samplesPerPixel.getValue();
                ambientFill       = rp->ambientIntensity.getValue();
                // Clamp to sane values
                if (samplesPerPixel  < 1)  samplesPerPixel  = 1;
                if (maxBouncesAllowed < 0) maxBouncesAllowed = 0;
                ambientFill = nrt_clamp01(ambientFill);
            }
        }

        // --- 4. Extract view volume from camera in scene --------------------
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

        const float aspect_ratio =
            static_cast<float>(width) / static_cast<float>(height);
        SbViewVolume vv = cam->getViewVolume(aspect_ratio);
        if (aspect_ratio < 1.0f) vv.scale(1.0f / aspect_ratio);

        // --- 5. Raytrace ----------------------------------------------------
        nanort::TriangleIntersector<float> intersector(
            nrtScene.vertices.data(), nrtScene.faces.data(),
            sizeof(float) * 3);

        // Per-pixel state for jitter PRNG seed (deterministic for reproducibility)
        uint32_t rngState = 0xDEADBEEFu;

        for (unsigned int y = 0; y < height; ++y) {
            for (unsigned int x = 0; x < width; ++x) {
                // GL convention: row 0 is the BOTTOM of the screen.
                const float fx = static_cast<float>(x);
                const float fy = static_cast<float>(y);
                const float fw = static_cast<float>(width);
                const float fh = static_cast<float>(height);

                // Accumulate colour across AA samples.
                float accum[3] = { 0.0f, 0.0f, 0.0f };
                int hitSamples = 0;

                for (int s = 0; s < samplesPerPixel; ++s) {
                    // Jitter offset within pixel (stratified sub-pixel sampling).
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

                    // Primary ray
                    nanort::Ray<float> ray;
                    ray.org[0] = p0[0]; ray.org[1] = p0[1]; ray.org[2] = p0[2];
                    ray.dir[0] = d[0];  ray.dir[1] = d[1];  ray.dir[2] = d[2];
                    ray.min_t  = 0.001f;
                    ray.max_t  = 1.0e30f;

                    nanort::TriangleIntersection<float> isect;
                    if (!nrtScene.accel.Traverse(ray, intersector, &isect))
                        continue;  // primary miss: leave pre-filled background pixel
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
                    if (nrt_dot3(N, V) < 0.0f) { N[0] = -N[0]; N[1] = -N[1]; N[2] = -N[2]; }

                    const float hitPt[3] = {
                        p0[0] + dir[0] * isect.t,
                        p0[1] + dir[1] * isect.t,
                        p0[2] + dir[2] * isect.t
                    };

                    const NrtMaterial & mat = nrtScene.tris[fid].mat;

                    // Direct illumination
                    float px[3] = { 0.0f, 0.0f, 0.0f };
                    nrt_shade(nrtScene, intersector, hitPt, N, V, mat,
                              lights, ambientFill, shadowsEnabled, px);

                    // Reflection bounce (uses nrt_trace for secondary rays;
                    // background_rgb is used for missed secondary rays).
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
                            nrt_trace(nrtScene, intersector, Rorg, Rdir,
                                      lights, ambientFill, shadowsEnabled,
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

                // Only write pixel if at least one sample hit geometry.
                // Pixels with no hits keep the pre-filled background colour
                // (which may be a gradient set by the caller).
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
        } // end BVH/raytrace block (skipped when no triangles)

        // --- 6. Composite SoText2 overlays directly onto the framebuffer ----
        // Text overlays were collected during the scene traversal above.
        // They contain crisp (binary-threshold) RGBA pixel buffers stored in
        // GL convention (row 0 = bottom of viewport).  We composite them here
        // so each glyph shows its actual letter shape rather than a solid block.
        for (const NrtTextOverlay & ov : proxyData.textOverlays) {
            // Clamp the overlay rectangle to the framebuffer.
            const int src_x0 = std::max(0, -ov.x);
            const int src_y0 = std::max(0, -ov.y);
            const int dst_x0 = std::max(0,  ov.x);
            const int dst_y0 = std::max(0,  ov.y);
            const int draw_w = std::min(ov.w - src_x0,
                                        static_cast<int>(width)  - dst_x0);
            const int draw_h = std::min(ov.h - src_y0,
                                        static_cast<int>(height) - dst_y0);
            if (draw_w <= 0 || draw_h <= 0) continue;

            for (int row = 0; row < draw_h; ++row) {
                // Source: ov.pixbuf row (src_y0 + row) counted from bottom.
                const int src_row = src_y0 + row;
                const unsigned char * src =
                    ov.pixbuf.data() + (src_row * ov.w + src_x0) * 4;

                // Destination: pixels row (dst_y0 + row) counted from bottom
                // (same GL convention the ray-tracer uses).
                const int dst_row = dst_y0 + row;
                const size_t dst_base =
                    static_cast<size_t>(dst_row) * width + dst_x0;

                for (int col = 0; col < draw_w; ++col) {
                    const unsigned char r_src = src[0];
                    const unsigned char g_src = src[1];
                    const unsigned char b_src = src[2];
                    const unsigned char a_src = src[3];
                    src += 4;

                    if (a_src == 0) continue; // fully transparent – skip

                    // Alpha-composite (src over dst) using the full stb_truetype
                    // grayscale alpha value for smooth anti-aliased text edges.
                    const float fa = a_src * (1.0f / 255.0f);
                    const float fb = 1.0f - fa;
                    const size_t idx = (dst_base + col) * nrcomponents;
                    pixels[idx + 0] = static_cast<unsigned char>(r_src * fa + pixels[idx + 0] * fb);
                    pixels[idx + 1] = static_cast<unsigned char>(g_src * fa + pixels[idx + 1] * fb);
                    pixels[idx + 2] = static_cast<unsigned char>(b_src * fa + pixels[idx + 2] * fb);
                    if (nrcomponents == 4) pixels[idx + 3] = 255;
                }
            }
        }

        return TRUE;
    }
};

#endif // OBOL_NANORT_CONTEXT_MANAGER_H
