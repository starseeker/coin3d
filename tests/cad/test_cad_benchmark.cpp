/*
 * test_cad_benchmark.cpp
 *
 * Rendering comparison and performance benchmark: standard Open Inventor
 * scene graph vs. SoCADAssembly compiled CAD approach.
 *
 * The test procedurally generates a large hierarchical scene (a NxNxN grid
 * of wireframe box "parts") using both approaches, renders each with
 * SoOffscreenRenderer, then:
 *
 *  1. Verifies that both renderings are substantially similar (non-blank,
 *     similar pixel density).
 *  2. Reports wall-clock timing for scene-build, GL render, and total
 *     phases so the CAD assembly's scalability advantage can be observed.
 *
 * A "wireframe box" part consists of the 12 edges of a unit cube expressed
 * as 12 two-point polylines (for the CAD approach) or a SoLineSet with 12
 * GL_LINES pairs (for the scene-graph approach).
 *
 * Grid size: INSTANCES_PER_AXIS (default 8) → INSTANCES_PER_AXIS^3 instances.
 * With INSTANCES_PER_AXIS=8 that is
 * 512 instances, which stresses a naive scene graph (512 SoSeparator +
 * SoTransform + SoCoordinate3 + SoLineSet node groups) while remaining
 * manageable in headless CI.
 *
 * Usage: test_cad_benchmark [outprefix]
 *   Writes <outprefix>_sg.rgb  (scene-graph render)
 *         <outprefix>_cad.rgb (CAD assembly render)
 * Returns 0 on pass, non-zero on failure.
 */

#include "headless_utils.h"

#include <obol/cad/SoCADAssembly.h>
#include <obol/cad/SoCADDetail.h>
#include <obol/cad/CadIds.h>

#include <Inventor/SoDB.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbBox3f.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SoOffscreenRenderer.h>

#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoLineSet.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoLightModel.h>

#include <Inventor/actions/SoGetBoundingBoxAction.h>

#include <chrono>
#include <cstdio>
#include <cmath>
#include <vector>
#include <string>

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
static const int  INSTANCES_PER_AXIS = 8;    // instances per axis → INSTANCES_PER_AXIS^3 total
static const float SPACING = 2.5f; // world-space spacing between instances
static const int  W        = 512;
static const int  H        = 512;

// ---------------------------------------------------------------------------
// Geometry helpers – unit wireframe box
// ---------------------------------------------------------------------------

// 12 edges of a unit cube [0,1]^3, each as (p0, p1)
static const float BOX_EDGES[12][2][3] = {
    // Bottom face
    {{0,0,0},{1,0,0}}, {{1,0,0},{1,0,1}}, {{1,0,1},{0,0,1}}, {{0,0,1},{0,0,0}},
    // Top face
    {{0,1,0},{1,1,0}}, {{1,1,0},{1,1,1}}, {{1,1,1},{0,1,1}}, {{0,1,1},{0,1,0}},
    // Vertical pillars
    {{0,0,0},{0,1,0}}, {{1,0,0},{1,1,0}}, {{1,0,1},{1,1,1}}, {{0,0,1},{0,1,1}},
};

// ============================================================================
// Standard scene-graph approach
// ============================================================================

/**
 * Build a flat SoCoordinate3 + SoLineSet rendering a unit wireframe box.
 * The box has 12 edges × 2 vertices = 24 vertices; numVertices has 12×2=24
 * with all values == 2 (each linestrip is a single edge segment).
 */
static SoSeparator *buildBoxSG()
{
    SoSeparator *sep = new SoSeparator;

    SoCoordinate3 *coords = new SoCoordinate3;
    SoLineSet *ls = new SoLineSet;

    int idx = 0;
    for (int e = 0; e < 12; ++e) {
        const float *p0 = BOX_EDGES[e][0];
        const float *p1 = BOX_EDGES[e][1];
        coords->point.set1Value(idx,   SbVec3f(p0[0], p0[1], p0[2]));
        coords->point.set1Value(idx+1, SbVec3f(p1[0], p1[1], p1[2]));
        ls->numVertices.set1Value(e, 2);
        idx += 2;
    }

    sep->addChild(coords);
    sep->addChild(ls);
    return sep;
}

/**
 * Build the full NxNxN grid using standard Open Inventor nodes.
 * Returns ref-counted root (caller must unref).
 */
static SoSeparator *buildSceneGraph(int grid)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    // Camera looking at the centre of the grid
    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    root->addChild(cam);

    // BASE_COLOR lighting (no shading) so colors are deterministic
    SoLightModel *lm = new SoLightModel;
    lm->model.setValue(SoLightModel::BASE_COLOR);
    root->addChild(lm);

    SoBaseColor *col = new SoBaseColor;
    col->rgb.setValue(0.8f, 0.8f, 0.8f);
    root->addChild(col);

    // Grid of instances: each is SoSeparator{SoTransform, SoCoordinate3, SoLineSet}
    for (int ix = 0; ix < grid; ++ix) {
        for (int iy = 0; iy < grid; ++iy) {
            for (int iz = 0; iz < grid; ++iz) {
                SoSeparator *inst = new SoSeparator;

                SoTransform *xf = new SoTransform;
                xf->translation.setValue(
                    ix * SPACING, iy * SPACING, iz * SPACING);
                inst->addChild(xf);

                inst->addChild(buildBoxSG());
                root->addChild(inst);
            }
        }
    }

    // viewAll frames the scene; don't scale the position (would shift near/far planes)
    SbViewportRegion vp(W, H);
    cam->viewAll(root, vp);

    return root;
}

// ============================================================================
// CAD assembly approach
// ============================================================================

/**
 * Build the unit wireframe box as a WireRep for SoCADAssembly.
 */
static obol::WireRep buildBoxWire()
{
    obol::WireRep wire;
    for (int e = 0; e < 12; ++e) {
        obol::WirePolyline poly;
        poly.points.push_back(SbVec3f(BOX_EDGES[e][0][0],
                                      BOX_EDGES[e][0][1],
                                      BOX_EDGES[e][0][2]));
        poly.points.push_back(SbVec3f(BOX_EDGES[e][1][0],
                                      BOX_EDGES[e][1][1],
                                      BOX_EDGES[e][1][2]));
        poly.edgeId = static_cast<uint32_t>(e + 1);
        wire.polylines.push_back(std::move(poly));
    }
    wire.bounds = SbBox3f(SbVec3f(0,0,0), SbVec3f(1,1,1));
    return wire;
}

/**
 * Build the full NxNxN grid using SoCADAssembly.
 * Returns ref-counted root (caller must unref).
 */
static SoSeparator *buildCADScene(int grid)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    // Camera (same as SG scene for comparable framing)
    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    root->addChild(cam);

    // BASE_COLOR lighting consistent with SG approach
    SoLightModel *lm = new SoLightModel;
    lm->model.setValue(SoLightModel::BASE_COLOR);
    root->addChild(lm);

    SoCADAssembly *assembly = new SoCADAssembly;
    assembly->drawMode.setValue(SoCADAssembly::WIREFRAME);
    root->addChild(assembly);

    // Register one part: the unit wireframe box
    obol::PartId unitBoxPartId = obol::CadIdBuilder::hash128("unit_box");
    obol::PartGeometry geom;
    geom.wire = buildBoxWire();
    assembly->upsertPart(unitBoxPartId, geom);

    // Create grid^3 instances with per-instance transforms
    obol::InstanceId rootId = obol::CadIdBuilder::Root();
    int instanceIdx = 0;
    for (int ix = 0; ix < grid; ++ix) {
        for (int iy = 0; iy < grid; ++iy) {
            for (int iz = 0; iz < grid; ++iz) {
                char name[64];
                snprintf(name, sizeof(name), "box_%d_%d_%d", ix, iy, iz);

                obol::InstanceRecord rec;
                rec.part   = unitBoxPartId;
                rec.parent = rootId;
                rec.childName = name;
                rec.occurrenceIndex = static_cast<uint32_t>(instanceIdx++);
                rec.boolOp = 0;
                rec.localToRoot.makeIdentity();
                rec.localToRoot.setTranslate(SbVec3f(
                    ix * SPACING, iy * SPACING, iz * SPACING));

                rec.style.hasColorOverride = true;
                rec.style.color = SbColor4f(0.8f, 0.8f, 0.8f, 1.0f);

                assembly->upsertInstanceAuto(rec);
            }
        }
    }

    // Frame camera the same way as the SG scene
    SbViewportRegion vp(W, H);
    cam->viewAll(root, vp);

    return root;
}

// ============================================================================
// Pixel analysis helpers
// ============================================================================

static int countNonBlack(const unsigned char *buf, int w, int h)
{
    int n = 0;
    for (int i = 0; i < w * h; ++i) {
        if (buf[i*3+0] > 20 || buf[i*3+1] > 20 || buf[i*3+2] > 20)
            ++n;
    }
    return n;
}

/** Mean absolute difference per pixel across all channels. */
static double pixelMAD(const unsigned char *a, const unsigned char *b, int w, int h)
{
    double sum = 0.0;
    for (int i = 0; i < w * h * 3; ++i)
        sum += std::abs((int)a[i] - (int)b[i]);
    return sum / (w * h * 3);
}

// ============================================================================
// Orthographic camera helper
// ============================================================================

/**
 * Replace the first SoPerspectiveCamera found in root with a
 * SoOrthographicCamera that covers the same view volume.
 * The orthographic height is derived from the perspective heightAngle and
 * focal distance so that object apparent sizes remain unchanged.
 */
static void swapToOrthoCamera(SoSeparator *root)
{
    SoPerspectiveCamera *pcam = nullptr;
    for (int i = 0; i < root->getNumChildren(); ++i) {
        pcam = dynamic_cast<SoPerspectiveCamera*>(root->getChild(i));
        if (pcam) break;
    }
    if (!pcam) return;

    SoOrthographicCamera *ocam = new SoOrthographicCamera;
    ocam->position.setValue(pcam->position.getValue());
    ocam->orientation.setValue(pcam->orientation.getValue());
    ocam->focalDistance.setValue(pcam->focalDistance.getValue());
    ocam->nearDistance.setValue(pcam->nearDistance.getValue());
    ocam->farDistance.setValue(pcam->farDistance.getValue());

    float ha    = pcam->heightAngle.getValue();
    float focal = pcam->focalDistance.getValue();
    ocam->height.setValue(2.0f * focal * std::tan(ha * 0.5f));

    root->replaceChild(pcam, ocam);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char **argv)
{
    const char *outprefix = (argc > 1) ? argv[1] : "test_cad_benchmark";

    // Initialise Obol with the headless backend
    initCoinHeadless();

    // Register CAD node types (not part of core SoDB)
    SoCADAssembly::initClass();

    printf("test_cad_benchmark: instances_per_axis=%d  total_instances=%d\n",
           INSTANCES_PER_AXIS,
           INSTANCES_PER_AXIS * INSTANCES_PER_AXIS * INSTANCES_PER_AXIS);
    printf("test_cad_benchmark: viewport=%dx%d\n", W, H);
    printf("\n");

    using Clock = std::chrono::high_resolution_clock;
    using Ms    = std::chrono::duration<double, std::milli>;

    // -----------------------------------------------------------------------
    // 1. Standard scene-graph approach
    // -----------------------------------------------------------------------
    printf("--- Standard scene-graph approach ---\n");

    auto t0 = Clock::now();
    SoSeparator *sgRoot = buildSceneGraph(INSTANCES_PER_AXIS);
    auto t1 = Clock::now();
    double sgBuildMs = Ms(t1 - t0).count();
    printf("  build:  %.1f ms\n", sgBuildMs);

    SbViewportRegion vpSG(W, H);
    SoOffscreenRenderer sgRenderer(vpSG);
    sgRenderer.setComponents(SoOffscreenRenderer::RGB);
    sgRenderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    auto t2 = Clock::now();
    bool sgOk = (sgRenderer.render(sgRoot) == TRUE);
    auto t3 = Clock::now();
    double sgRenderMs = Ms(t3 - t2).count();
    printf("  render: %.1f ms  (ok=%d)\n", sgRenderMs, (int)sgOk);

    const unsigned char *sgBuf = sgOk ? sgRenderer.getBuffer() : nullptr;
    int sgNonBlack = sgBuf ? countNonBlack(sgBuf, W, H) : 0;
    printf("  non-black pixels: %d / %d  (%.1f%%)\n",
           sgNonBlack, W*H, 100.0 * sgNonBlack / (W*H));

    if (sgOk && sgBuf) {
        char path[1024];
        snprintf(path, sizeof(path), "%s_sg.rgb", outprefix);
        sgRenderer.writeToRGB(path);
        printf("  wrote: %s\n", path);
    }

    // -----------------------------------------------------------------------
    // 2. CAD assembly approach
    // -----------------------------------------------------------------------
    printf("\n--- CAD assembly approach ---\n");

    auto t4 = Clock::now();
    SoSeparator *cadRoot = buildCADScene(INSTANCES_PER_AXIS);
    auto t5 = Clock::now();
    double cadBuildMs = Ms(t5 - t4).count();
    printf("  build:  %.1f ms\n", cadBuildMs);

    // Find the assembly node (it's the only SoCADAssembly in cadRoot)
    SoCADAssembly *cadAssembly = nullptr;
    for (int i = 0; i < cadRoot->getNumChildren(); ++i) {
        cadAssembly = dynamic_cast<SoCADAssembly*>(cadRoot->getChild(i));
        if (cadAssembly) break;
    }

    SbViewportRegion vpCAD(W, H);
    SoOffscreenRenderer cadRenderer(vpCAD);
    cadRenderer.setComponents(SoOffscreenRenderer::RGB);
    cadRenderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    auto t6 = Clock::now();
    bool cadOk = (cadRenderer.render(cadRoot) == TRUE);
    auto t7 = Clock::now();
    double cadRenderMs = Ms(t7 - t6).count();

    // Report which rendering tier was selected
    static const char *kTierName[] = {
        "Tier 0 (immediate-mode, GL 1.1 fallback)",
        "Tier 1 (VBO-loop, GL 2.0 / GLSL 1.10)",
        "Tier 2 (instanced, GL 3.1+)",
    };
    int tier = cadAssembly ? cadAssembly->lastRenderTier() : -1;
    const char *tierStr = (tier >= 0 && tier <= 2) ? kTierName[tier] : "unknown";
    printf("  render: %.1f ms  (ok=%d)  [%s]\n", cadRenderMs, (int)cadOk, tierStr);

    const unsigned char *cadBuf = cadOk ? cadRenderer.getBuffer() : nullptr;
    int cadNonBlack = cadBuf ? countNonBlack(cadBuf, W, H) : 0;
    printf("  non-black pixels: %d / %d  (%.1f%%)\n",
           cadNonBlack, W*H, 100.0 * cadNonBlack / (W*H));

    if (cadOk && cadBuf) {
        char path[1024];
        snprintf(path, sizeof(path), "%s_cad.rgb", outprefix);
        cadRenderer.writeToRGB(path);
        printf("  wrote: %s\n", path);
    }

    // -----------------------------------------------------------------------
    // 3. Summary / comparison
    // -----------------------------------------------------------------------
    printf("\n--- Summary ---\n");
    printf("  SG   build+render: %.1f ms  (build=%.1f  render=%.1f)\n",
           sgBuildMs + sgRenderMs, sgBuildMs, sgRenderMs);
    printf("  CAD  build+render: %.1f ms  (build=%.1f  render=%.1f)\n",
           cadBuildMs + cadRenderMs, cadBuildMs, cadRenderMs);

    // -----------------------------------------------------------------------
    // 4. Correctness checks
    // -----------------------------------------------------------------------
    bool allOk = true;

    // 4a. Both renders must succeed
    if (!sgOk) {
        fprintf(stderr, "FAIL: scene-graph render() returned false\n");
        allOk = false;
    }
    if (!cadOk) {
        fprintf(stderr, "FAIL: CAD assembly render() returned false\n");
        allOk = false;
    }

    // 4b. Both renders must produce non-trivial non-black pixel coverage
    // (at least 1% of the viewport must be lit)
    const int minNonBlack = (W * H) / 100;
    if (sgNonBlack < minNonBlack) {
        fprintf(stderr, "FAIL: scene-graph produced too few non-black pixels "
                "(%d < %d)\n", sgNonBlack, minNonBlack);
        allOk = false;
    }
    if (cadNonBlack < minNonBlack) {
        fprintf(stderr, "FAIL: CAD assembly produced too few non-black pixels "
                "(%d < %d)\n", cadNonBlack, minNonBlack);
        allOk = false;
    }

    // 4c. Pixel coverage should be substantially similar.
    // Allow ±20 percentage-point difference (transforms may differ by a few
    // pixels in anti-aliasing due to different traversal order and GL state).
    if (sgNonBlack > 0 && cadNonBlack > 0) {
        double sgPct  = 100.0 * sgNonBlack  / (W*H);
        double cadPct = 100.0 * cadNonBlack / (W*H);
        double diff   = std::abs(sgPct - cadPct);
        printf("  pixel coverage diff: %.1f pp  (SG=%.1f%%  CAD=%.1f%%)\n",
               diff, sgPct, cadPct);
        if (diff > 20.0) {
            fprintf(stderr, "FAIL: pixel coverage differs by more than 20pp "
                    "(%.1f)\n", diff);
            allOk = false;
        }
    }

    // 4d. Mean absolute pixel difference (if both rendered).
    // Both scenes use the same geometry + camera, so MAD should be small
    // enough to confirm no gross rendering error.
    if (sgBuf && cadBuf) {
        double mad = pixelMAD(sgBuf, cadBuf, W, H);
        printf("  pixel MAD: %.2f / 255\n", mad);
        // Accept up to MAD=30/255 – different GL states can cause some
        // rasterisation divergence on edges
        if (mad > 30.0) {
            fprintf(stderr, "FAIL: pixel MAD=%.2f exceeds threshold (30.0)\n", mad);
            allOk = false;
        }
    }

    // -----------------------------------------------------------------------
    // 5. Orthographic camera – scene-graph approach
    // -----------------------------------------------------------------------
    printf("\n--- Orthographic: scene-graph approach ---\n");

    SoSeparator *sgOrthoRoot = buildSceneGraph(INSTANCES_PER_AXIS);
    swapToOrthoCamera(sgOrthoRoot);

    SbViewportRegion vpSGO(W, H);
    SoOffscreenRenderer sgOrthoRenderer(vpSGO);
    sgOrthoRenderer.setComponents(SoOffscreenRenderer::RGB);
    sgOrthoRenderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    bool sgOrthoOk = (sgOrthoRenderer.render(sgOrthoRoot) == TRUE);
    printf("  render: ok=%d\n", (int)sgOrthoOk);

    const unsigned char *sgOrthoBuf = sgOrthoOk ? sgOrthoRenderer.getBuffer() : nullptr;
    int sgOrthoNonBlack = sgOrthoBuf ? countNonBlack(sgOrthoBuf, W, H) : 0;
    printf("  non-black pixels: %d / %d  (%.1f%%)\n",
           sgOrthoNonBlack, W*H, 100.0 * sgOrthoNonBlack / (W*H));

    if (sgOrthoOk && sgOrthoBuf) {
        char path[1024];
        snprintf(path, sizeof(path), "%s_sg_ortho.rgb", outprefix);
        sgOrthoRenderer.writeToRGB(path);
        printf("  wrote: %s\n", path);
    }

    // -----------------------------------------------------------------------
    // 6. Orthographic camera – CAD assembly approach
    // -----------------------------------------------------------------------
    printf("\n--- Orthographic: CAD assembly approach ---\n");

    SoSeparator *cadOrthoRoot = buildCADScene(INSTANCES_PER_AXIS);
    swapToOrthoCamera(cadOrthoRoot);

    SbViewportRegion vpCADO(W, H);
    SoOffscreenRenderer cadOrthoRenderer(vpCADO);
    cadOrthoRenderer.setComponents(SoOffscreenRenderer::RGB);
    cadOrthoRenderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    bool cadOrthoOk = (cadOrthoRenderer.render(cadOrthoRoot) == TRUE);
    printf("  render: ok=%d\n", (int)cadOrthoOk);

    const unsigned char *cadOrthoBuf = cadOrthoOk ? cadOrthoRenderer.getBuffer() : nullptr;
    int cadOrthoNonBlack = cadOrthoBuf ? countNonBlack(cadOrthoBuf, W, H) : 0;
    printf("  non-black pixels: %d / %d  (%.1f%%)\n",
           cadOrthoNonBlack, W*H, 100.0 * cadOrthoNonBlack / (W*H));

    if (cadOrthoOk && cadOrthoBuf) {
        char path[1024];
        snprintf(path, sizeof(path), "%s_cad_ortho.rgb", outprefix);
        cadOrthoRenderer.writeToRGB(path);
        printf("  wrote: %s\n", path);
    }

    // -----------------------------------------------------------------------
    // 7. Orthographic correctness checks
    // -----------------------------------------------------------------------
    printf("\n--- Orthographic checks ---\n");

    if (!sgOrthoOk) {
        fprintf(stderr, "FAIL: orthographic SG render() returned false\n");
        allOk = false;
    }
    if (!cadOrthoOk) {
        fprintf(stderr, "FAIL: orthographic CAD render() returned false\n");
        allOk = false;
    }
    if (sgOrthoNonBlack < minNonBlack) {
        fprintf(stderr, "FAIL: orthographic SG too few non-black pixels "
                "(%d < %d)\n", sgOrthoNonBlack, minNonBlack);
        allOk = false;
    }
    if (cadOrthoNonBlack < minNonBlack) {
        fprintf(stderr, "FAIL: orthographic CAD too few non-black pixels "
                "(%d < %d)\n", cadOrthoNonBlack, minNonBlack);
        allOk = false;
    }
    printf("  orthographic checks: %s\n", allOk ? "ok" : "FAILED");

    // -----------------------------------------------------------------------
    // Cleanup
    // -----------------------------------------------------------------------
    sgRoot->unref();
    cadRoot->unref();
    sgOrthoRoot->unref();
    cadOrthoRoot->unref();

    printf("\ntest_cad_benchmark: %s\n", allOk ? "PASS" : "FAIL");
    return allOk ? 0 : 1;
}
