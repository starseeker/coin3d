/*
 * render_nanort.cpp
 *
 * NanoRT raytracing backend demonstration for Obol/Coin scene graphs.
 *
 * This program demonstrates how to use the Obol public API (scene graph
 * construction, camera, transforms, materials, lights) with a raytracing
 * backend instead of OpenGL. No OpenGL calls are made during rendering.
 *
 * Obol APIs exercised:
 *   - SoDB, SoNodeKit, SoInteraction     (initialization)
 *   - SoSeparator, SoTranslation, SoMaterial (scene graph construction)
 *   - SoPerspectiveCamera                (camera parameters)
 *   - SoDirectionalLight                 (scene lighting)
 *   - SoSphere, SoCube, SoCone, SoCylinder (geometry primitives)
 *   - SoCallbackAction + triangle callback (geometry extraction)
 *   - SoSearchAction                     (scene node queries)
 *   - SbViewportRegion, SbViewVolume     (camera ray generation)
 *   - SbMatrix                           (vertex/normal transforms)
 *
 * Rendering pipeline (replaces OpenGL):
 *   1. SoCallbackAction traverses the scene and calls our triangle
 *      callback for every triangle produced by every shape node.
 *      Each triangle's vertices and normals are transformed to world
 *      space using the current model matrix from the action state.
 *      Material properties (diffuse, specular, shininess) are read
 *      from the callback action's current traversal state.
 *   2. A nanort BVH is built from the collected triangles.
 *   3. For each pixel, a ray is cast using SbViewVolume::projectPointToLine
 *      (camera math handled entirely by Obol).
 *   4. Ray hits are shaded with a Phong model using lights found in
 *      the scene via SoSearchAction.
 *   5. The final image is written as a PNG using stb_image_write.
 *
 * The test passes if:
 *   - Triangle extraction yields > 0 triangles
 *   - BVH builds successfully
 *   - The rendered image has >= 1% non-background pixels
 *   - The PNG is written without error
 */

// ---- Obol/Coin includes -------------------------------------------------------
#include <Inventor/SoDB.h>
#include <Inventor/nodekits/SoNodeKit.h>
#include <Inventor/SoInteraction.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbViewVolume.h>
#include <Inventor/SbVec2f.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/actions/SoCallbackAction.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCamera.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoShape.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/SoPrimitiveVertex.h>

// ---- nanort ------------------------------------------------------------------
// nanort is a single-header BVH/ray-casting library.
// The nanort.cc translation unit provides the non-template implementation.
#include "nanort.h"

// ---- stb_image_write (header-only PNG writer) --------------------------------
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// ---- Standard library --------------------------------------------------------
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#include <string>

// =============================================================================
// No-op context manager
// =============================================================================
// We never call any GL render action, so a no-op context manager suffices.
// SoDB::init() always requires a ContextManager; this satisfies that contract
// while keeping the binary GL-independent at runtime.
class NullContextManager : public SoDB::ContextManager {
public:
    void* createOffscreenContext(unsigned int, unsigned int) override { return nullptr; }
    SbBool makeContextCurrent(void*) override { return FALSE; }
    void restorePreviousContext(void*) override {}
    void destroyContext(void*) override {}
};

// =============================================================================
// Per-triangle scene data
// =============================================================================

struct RtMaterial {
    float diffuse[3];   // RGB [0,1]
    float specular[3];  // RGB [0,1]
    float ambient[3];   // RGB [0,1]
    float emission[3];  // RGB [0,1]
    float shininess;    // [0,1]; multiply by 128 for Phong exponent
};

struct RtTriangle {
    float pos[3][3];    // world-space positions  [vert 0/1/2][x/y/z]
    float norm[3][3];   // world-space normals    [vert 0/1/2][x/y/z]
    RtMaterial mat;
};

struct SceneCollector {
    std::vector<RtTriangle> tris;
};

// =============================================================================
// Triangle callback – called by SoCallbackAction for every triangle
// =============================================================================

static void triangleCB(void* userdata,
                       SoCallbackAction* action,
                       const SoPrimitiveVertex* v0,
                       const SoPrimitiveVertex* v1,
                       const SoPrimitiveVertex* v2)
{
    SceneCollector* col = static_cast<SceneCollector*>(userdata);

    // Model matrix: transforms from object space → world space
    const SbMatrix& mm = action->getModelMatrix();
    // Normal matrix: inverse-transpose of model matrix
    SbMatrix normalMat = mm.inverse().transpose();

    RtTriangle tri;
    const SoPrimitiveVertex* verts[3] = { v0, v1, v2 };
    for (int i = 0; i < 3; ++i) {
        SbVec3f wp, wn;
        mm.multVecMatrix(verts[i]->getPoint(),  wp);
        normalMat.multDirMatrix(verts[i]->getNormal(), wn);
        wn.normalize();
        tri.pos[i][0]  = wp[0];  tri.pos[i][1]  = wp[1];  tri.pos[i][2]  = wp[2];
        tri.norm[i][0] = wn[0];  tri.norm[i][1] = wn[1];  tri.norm[i][2] = wn[2];
    }

    // Material from current traversal state (per-object or per-face binding)
    SbColor ambient, diffuse, specular, emission;
    float shininess, transparency;
    action->getMaterial(ambient, diffuse, specular, emission,
                        shininess, transparency,
                        v0->getMaterialIndex());

    tri.mat.diffuse[0]  = diffuse[0];   tri.mat.diffuse[1]  = diffuse[1];   tri.mat.diffuse[2]  = diffuse[2];
    tri.mat.specular[0] = specular[0];  tri.mat.specular[1] = specular[1];  tri.mat.specular[2] = specular[2];
    tri.mat.ambient[0]  = ambient[0];   tri.mat.ambient[1]  = ambient[1];   tri.mat.ambient[2]  = ambient[2];
    tri.mat.emission[0] = emission[0];  tri.mat.emission[1] = emission[1];  tri.mat.emission[2] = emission[2];
    tri.mat.shininess   = shininess;

    col->tris.push_back(tri);
}

// =============================================================================
// Scene data for nanort
// =============================================================================

struct RtScene {
    // Flat vertex/face/normal arrays required by nanort
    std::vector<float>        vertices;  // 9 floats per triangle (3 verts * xyz)
    std::vector<unsigned int> faces;     // 3 indices per triangle (sequential)
    std::vector<float>        normals;   // 9 floats per triangle (3 verts * xyz)

    // Per-triangle source data (for shading)
    std::vector<RtTriangle> tris;

    nanort::BVHAccel<float> accel;

    bool build()
    {
        size_t n = tris.size();
        if (n == 0) return false;

        vertices.resize(n * 9);
        normals.resize(n * 9);
        faces.resize(n * 3);

        for (size_t i = 0; i < n; ++i) {
            for (int v = 0; v < 3; ++v) {
                size_t base = 9 * i + 3 * v;
                vertices[base + 0] = tris[i].pos[v][0];
                vertices[base + 1] = tris[i].pos[v][1];
                vertices[base + 2] = tris[i].pos[v][2];
                normals[base + 0]  = tris[i].norm[v][0];
                normals[base + 1]  = tris[i].norm[v][1];
                normals[base + 2]  = tris[i].norm[v][2];
                faces[3 * i + v] = static_cast<unsigned int>(3 * i + v);
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

// =============================================================================
// Math helpers
// =============================================================================

static inline float clamp01(float v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

static inline float dot3(const float* a, const float* b) {
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

// =============================================================================
// Phong shading
// =============================================================================
// N      : surface normal (unit, facing viewer)
// V      : unit vector toward eye (= -ray.dir)
// L      : unit vector toward light
// Returns RGB color contribution for one light.
static void phongContrib(const float* N, const float* V, const float* L,
                         const RtMaterial& mat,
                         const float* lightRGB, float lightIntensity,
                         float out[3])
{
    float NdotL = clamp01(dot3(N, L));

    // Diffuse
    out[0] = mat.diffuse[0] * lightRGB[0] * lightIntensity * NdotL;
    out[1] = mat.diffuse[1] * lightRGB[1] * lightIntensity * NdotL;
    out[2] = mat.diffuse[2] * lightRGB[2] * lightIntensity * NdotL;

    // Specular (Phong reflection)
    float specExp = mat.shininess * 128.0f;
    if (NdotL > 0.0f && specExp > 0.0f) {
        float R[3] = {
            2.0f * NdotL * N[0] - L[0],
            2.0f * NdotL * N[1] - L[1],
            2.0f * NdotL * N[2] - L[2]
        };
        float VdotR = clamp01(dot3(V, R));
        float spec  = powf(VdotR, specExp);
        out[0] += mat.specular[0] * lightRGB[0] * lightIntensity * spec;
        out[1] += mat.specular[1] * lightRGB[1] * lightIntensity * spec;
        out[2] += mat.specular[2] * lightRGB[2] * lightIntensity * spec;
    }
}

// =============================================================================
// Main render loop
// =============================================================================

static bool renderToPNG(const RtScene& scene,
                        const SbViewVolume& vv,
                        const std::vector<SbVec3f>& lightDirs,
                        const std::vector<SbColor>& lightColors,
                        const std::vector<float>&   lightIntensities,
                        int W, int H,
                        const char* outpath,
                        const float* bgBottom = nullptr,
                        const float* bgTop    = nullptr)
{
    // Default background: black
    static const float kBlack[3] = { 0.0f, 0.0f, 0.0f };
    const float* bottom = bgBottom ? bgBottom : kBlack;
    const float* top    = bgTop    ? bgTop    : bottom;

    std::vector<unsigned char> image(W * H * 3);

    // Pre-fill the image with the background colour (solid or vertical gradient).
    // Row 0 is the TOP of the PNG (y-down image), so row 0 maps to ny=1 (screen top).
    for (int y = 0; y < H; ++y) {
        // t=0 → top of image (screen top colour), t=1 → bottom of image (screen bottom)
        float t = (H > 1) ? (float)y / (float)(H - 1) : 0.0f;
        float r = top[0] * (1.0f - t) + bottom[0] * t;
        float g = top[1] * (1.0f - t) + bottom[1] * t;
        float b = top[2] * (1.0f - t) + bottom[2] * t;
        auto rB = (unsigned char)(r * 255.0f);
        auto gB = (unsigned char)(g * 255.0f);
        auto bB = (unsigned char)(b * 255.0f);
        for (int x = 0; x < W; ++x) {
            int i = (y * W + x) * 3;
            image[i+0] = rB; image[i+1] = gB; image[i+2] = bB;
        }
    }

    // nanort intersector uses the same flat arrays from RtScene
    nanort::TriangleIntersector<float> intersector(
        scene.vertices.data(), scene.faces.data(), sizeof(float) * 3);

    // Small ambient fill to avoid completely black shadows
    const float kAmbientFill = 0.12f;
    int hitCount = 0;

    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            // Normalized screen coords in [0,1] × [0,1].
            // Y is flipped so that pixel row 0 maps to the TOP of the view
            // (image Y-down; SbViewVolume Y-up).
            float nx = (x + 0.5f) / W;
            float ny = 1.0f - (y + 0.5f) / H;

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
            bool hit = scene.accel.Traverse(ray, intersector, &isect);

            if (!hit) continue;
            ++hitCount;

            unsigned int fid = isect.prim_id;
            // Barycentric: (1-u-v) * v0 + u * v1 + v * v2
            float w0 = 1.0f - isect.u - isect.v;
            float w1 = isect.u;
            float w2 = isect.v;

            // Interpolate normal from per-vertex normals
            const float* n0 = scene.normals.data() + 9 * fid + 0;
            const float* n1 = scene.normals.data() + 9 * fid + 3;
            const float* n2 = scene.normals.data() + 9 * fid + 6;
            float N[3] = {
                w0*n0[0] + w1*n1[0] + w2*n2[0],
                w0*n0[1] + w1*n1[1] + w2*n2[1],
                w0*n0[2] + w1*n1[2] + w2*n2[2]
            };
            float nlen = sqrtf(N[0]*N[0] + N[1]*N[1] + N[2]*N[2]);
            if (nlen > 1e-6f) { N[0] /= nlen; N[1] /= nlen; N[2] /= nlen; }

            // Make sure normal faces the viewer
            float V[3] = { -d[0], -d[1], -d[2] };
            if (dot3(N, V) < 0.0f) { N[0] = -N[0]; N[1] = -N[1]; N[2] = -N[2]; }

            const RtMaterial& mat = scene.tris[fid].mat;

            // Emission + ambient fill
            float px[3] = {
                mat.emission[0] + kAmbientFill * mat.ambient[0],
                mat.emission[1] + kAmbientFill * mat.ambient[1],
                mat.emission[2] + kAmbientFill * mat.ambient[2]
            };

            // Accumulate contribution from each directional light
            for (size_t li = 0; li < lightDirs.size(); ++li) {
                // lightDirs[li] points FROM the light source; negate to get
                // the unit vector pointing TOWARD the light.
                SbVec3f ldir = -lightDirs[li];
                ldir.normalize();
                float L[3] = { ldir[0], ldir[1], ldir[2] };
                float lc[3] = { lightColors[li][0],
                                lightColors[li][1],
                                lightColors[li][2] };
                float contrib[3];
                phongContrib(N, V, L, mat, lc, lightIntensities[li], contrib);
                px[0] += contrib[0];
                px[1] += contrib[1];
                px[2] += contrib[2];
            }

            // Fallback: if no lights, use a simple diffuse-from-eye shading
            if (lightDirs.empty()) {
                float NdotV = clamp01(dot3(N, V));
                px[0] = mat.diffuse[0] * (kAmbientFill + (1.0f - kAmbientFill) * NdotV);
                px[1] = mat.diffuse[1] * (kAmbientFill + (1.0f - kAmbientFill) * NdotV);
                px[2] = mat.diffuse[2] * (kAmbientFill + (1.0f - kAmbientFill) * NdotV);
            }

            int idx = (y * W + x) * 3;
            image[idx + 0] = static_cast<unsigned char>(clamp01(px[0]) * 255.0f);
            image[idx + 1] = static_cast<unsigned char>(clamp01(px[1]) * 255.0f);
            image[idx + 2] = static_cast<unsigned char>(clamp01(px[2]) * 255.0f);
        }
    }

    // Write PNG
    int ok = stbi_write_png(outpath, W, H, 3, image.data(), W * 3);
    if (!ok) {
        fprintf(stderr, "ERROR: stbi_write_png failed for '%s'\n", outpath);
        return false;
    }

    // Sanity-check: at least 1% of pixels should have been hit by rays
    float coverage = static_cast<float>(hitCount) / (W * H);
    printf("Image coverage: %.1f%% hit pixels\n", coverage * 100.0f);
    if (coverage < 0.01f) {
        fprintf(stderr, "ERROR: rendered image has almost no geometry hits\n");
        return false;
    }

    return true;
}

// =============================================================================
// main
// =============================================================================

int main(int argc, char** argv)
{
    const int W = 512, H = 512;
    const char* outpath = (argc > 1) ? argv[1] : "render_nanort_out.png";

    // --- 1. Initialize Coin with a no-op GL context manager ------------------
    // SoDB::init() requires a ContextManager but we never issue any GL calls,
    // so a no-op implementation is sufficient.
    static NullContextManager nullCtx;
    SoDB::init(&nullCtx);
    SoNodeKit::init();
    SoInteraction::init();

    // --- 2. Build scene graph using standard Obol APIs -----------------------
    SoSeparator* root = new SoSeparator;
    root->ref();

    // Camera (perspective)
    SoPerspectiveCamera* cam = new SoPerspectiveCamera;
    root->addChild(cam);

    // Directional light (same direction as render_primitives)
    SoDirectionalLight* light = new SoDirectionalLight;
    light->direction.setValue(-0.5f, -0.8f, -0.6f);
    root->addChild(light);

    // 2×2 grid of coloured primitives at spacing s
    const float s = 2.5f;
    auto addPrimitive = [&](float r, float g, float b,
                            float tx, float ty,
                            SoNode* shape)
    {
        SoSeparator*  sep = new SoSeparator;
        SoTranslation* t  = new SoTranslation;
        t->translation.setValue(tx, ty, 0.0f);
        SoMaterial* mat = new SoMaterial;
        mat->diffuseColor.setValue(r, g, b);
        mat->specularColor.setValue(0.6f, 0.6f, 0.6f);
        mat->shininess.setValue(0.5f);
        sep->addChild(t);
        sep->addChild(mat);
        sep->addChild(shape);
        root->addChild(sep);
    };

    addPrimitive(0.85f, 0.15f, 0.15f, -s * 0.5f,  s * 0.5f, new SoSphere);   // red   sphere TL
    addPrimitive(0.15f, 0.75f, 0.15f,  s * 0.5f,  s * 0.5f, new SoCube);     // green cube   TR
    addPrimitive(0.15f, 0.35f, 0.90f, -s * 0.5f, -s * 0.5f, new SoCone);     // blue  cone   BL
    addPrimitive(0.90f, 0.75f, 0.15f,  s * 0.5f, -s * 0.5f, new SoCylinder); // gold  cyl    BR

    // Position camera to see the whole scene
    SbViewportRegion vp(W, H);
    cam->viewAll(root, vp);
    cam->position.setValue(cam->position.getValue() * 1.1f);

    // --- 3. Extract geometry via SoCallbackAction ----------------------------
    printf("Extracting scene geometry...\n");
    SceneCollector collector;
    {
        SoCallbackAction cba(vp);
        cba.addTriangleCallback(SoShape::getClassTypeId(), triangleCB, &collector);
        cba.apply(root);
    }
    printf("  Collected %zu triangles\n", collector.tris.size());
    if (collector.tris.empty()) {
        fprintf(stderr, "ERROR: no triangles extracted from scene\n");
        root->unref();
        return 1;
    }

    // --- 4. Extract lights from scene ----------------------------------------
    std::vector<SbVec3f> lightDirs;
    std::vector<SbColor> lightColors;
    std::vector<float>   lightIntensities;
    {
        SoSearchAction sa;
        sa.setType(SoDirectionalLight::getClassTypeId());
        sa.setInterest(SoSearchAction::ALL);
        sa.apply(root);
        const SoPathList& paths = sa.getPaths();
        for (int i = 0; i < paths.getLength(); ++i) {
            SoDirectionalLight* dl =
                static_cast<SoDirectionalLight*>(paths[i]->getTail());
            if (dl->on.getValue()) {
                lightDirs.push_back(dl->direction.getValue());
                lightColors.push_back(dl->color.getValue());
                lightIntensities.push_back(dl->intensity.getValue());
            }
        }
    }
    printf("  Found %d directional light(s)\n",
           static_cast<int>(lightDirs.size()));

    // --- 5. Build nanort BVH -------------------------------------------------
    printf("Building BVH...\n");
    RtScene scene;
    scene.tris = collector.tris;
    if (!scene.build()) {
        fprintf(stderr, "ERROR: BVH build failed\n");
        root->unref();
        return 1;
    }
    {
        float bmin[3], bmax[3];
        scene.accel.BoundingBox(bmin, bmax);
        printf("  Triangles  : %zu\n", scene.tris.size());
        printf("  Scene bbox : [%.2f,%.2f,%.2f] – [%.2f,%.2f,%.2f]\n",
               bmin[0], bmin[1], bmin[2], bmax[0], bmax[1], bmax[2]);
    }

    // --- 6. Render via nanort (no OpenGL) ------------------------------------
    // Obtain the view volume from Obol's camera; this encapsulates all
    // camera-space to world-space math and is used for ray generation.
    printf("Rendering %d×%d image with nanort...\n", W, H);
    SbViewVolume vv = cam->getViewVolume(static_cast<float>(W) / H);

    // --- 6a. Solid background render (default black) -----------------------
    if (!renderToPNG(scene, vv, lightDirs, lightColors, lightIntensities,
                     W, H, outpath))
    {
        root->unref();
        return 1;
    }
    printf("  Written to: %s\n", outpath);

    // --- 6b. Gradient background render ------------------------------------
    // Demonstrate that the nanort backend supports gradient backgrounds by
    // rendering the same scene with a dark-blue → steel-blue sky gradient.
    // The gradient output file is written alongside the primary output.
    {
        // Build gradient output path: replace ".png" suffix with "_gradient.png"
        std::string gradpath(outpath);
        const std::string suffix(".png");
        if (gradpath.size() >= suffix.size() &&
            gradpath.compare(gradpath.size() - suffix.size(), suffix.size(), suffix) == 0)
        {
            gradpath.replace(gradpath.size() - suffix.size(), suffix.size(),
                             "_gradient.png");
        } else {
            gradpath += "_gradient.png";
        }

        // Dark navy at the screen-bottom, steel-blue at the screen-top
        const float bgBottom[3] = { 0.05f, 0.05f, 0.25f };  // dark navy
        const float bgTop[3] = { 0.40f, 0.60f, 0.85f };  // steel blue

        printf("Rendering gradient background variant...\n");
        if (!renderToPNG(scene, vv, lightDirs, lightColors, lightIntensities,
                         W, H, gradpath.c_str(), bgBottom, bgTop))
        {
            root->unref();
            return 1;
        }
        printf("  Written to: %s\n", gradpath.c_str());
    }


    root->unref();
    return 0;
}
