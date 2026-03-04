/*
 * render_quad_viewport_lod.cpp — SoQuadViewport composite LOD regression test
 *
 * Produces a 512×512 composite image showing four views of the same SoLOD
 * scene rendered at different camera distances, so each quadrant displays a
 * distinct level of detail:
 *
 *   TOP_LEFT     (z= 3): distance < 5  → green sphere    (high detail)
 *   TOP_RIGHT    (z= 8): distance 5–12 → orange cube     (medium detail)
 *   BOTTOM_LEFT  (z=20): distance > 12 → red cone        (low detail)
 *   BOTTOM_RIGHT        : orthographic side view → orange cube
 *
 * A 4-pixel white border separates the quadrants for visual clarity.
 *
 * Pixel-level assertions:
 *   - Each quadrant is non-blank (at least 50 lit pixels).
 *   - TOP_LEFT  dominant colour channel is green (G > R, G > B).
 *   - BOTTOM_LEFT dominant colour channel is red   (R > G, R > B).
 *   - The composite file is written as an SGI RGB file at argv[1]+".rgb".
 *
 * Returns 0 on pass, 1 on fail.
 *
 * This test also serves as the source for the control image used in image-
 * comparison regression tests; generate with:
 *   cmake --build build_dir --target generate_rendering_controls
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <Inventor/SoQuadViewport.h>
#include <Inventor/SoViewport.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoLOD.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbVec2s.h>
#include <cstdio>

// ---------------------------------------------------------------------------
// Image size: 512×512 composite (256×256 per quadrant)
// ---------------------------------------------------------------------------
static const int W = 512;
static const int H = 512;

// ---------------------------------------------------------------------------
// Shared LOD scene (no camera)
// SoLOD ranges: [0,5) → green sphere, [5,12) → orange cube, [12,∞) → red cone
// ---------------------------------------------------------------------------
static SoSeparator * buildLODScene()
{
    SoSeparator * root = new SoSeparator;
    root->ref();

    SoDirectionalLight * lt = new SoDirectionalLight;
    lt->direction.setValue(-1.0f, -1.0f, -1.0f);
    root->addChild(lt);

    SoLOD * lod = new SoLOD;
    lod->range.set1Value(0,  5.0f);
    lod->range.set1Value(1, 12.0f);

    // HIGH detail: green sphere
    SoSeparator * hi = new SoSeparator;
    SoMaterial  * hiMat = new SoMaterial;
    hiMat->diffuseColor.setValue(0.1f, 0.8f, 0.1f);
    hi->addChild(hiMat);
    hi->addChild(new SoSphere);
    lod->addChild(hi);

    // MEDIUM detail: orange cube
    SoSeparator * med = new SoSeparator;
    SoMaterial  * medMat = new SoMaterial;
    medMat->diffuseColor.setValue(0.9f, 0.5f, 0.1f);
    med->addChild(medMat);
    med->addChild(new SoCube);
    lod->addChild(med);

    // LOW detail: red cone
    SoSeparator * lo = new SoSeparator;
    SoMaterial  * loMat = new SoMaterial;
    loMat->diffuseColor.setValue(0.8f, 0.1f, 0.1f);
    lo->addChild(loMat);
    lo->addChild(new SoCone);
    lod->addChild(lo);

    root->addChild(lod);
    return root;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool validateNonBlank(const unsigned char * buf, int npix,
                              const char * label, int threshold = 50)
{
    int lit = 0;
    for (int i = 0; i < npix; ++i) {
        const unsigned char * p = buf + i * 3;
        if (p[0] > 5 || p[1] > 5 || p[2] > 5) ++lit;
    }
    printf("  %s: lit=%d\n", label, lit);
    return lit >= threshold;
}

// Return the dominant colour channel (0=R, 1=G, 2=B).
static int dominantChannel(const unsigned char * buf, int npix)
{
    long sum[3] = { 0, 0, 0 };
    for (int i = 0; i < npix; ++i) {
        sum[0] += buf[i * 3 + 0];
        sum[1] += buf[i * 3 + 1];
        sum[2] += buf[i * 3 + 2];
    }
    int dom = 0;
    if (sum[1] > sum[dom]) dom = 1;
    if (sum[2] > sum[dom]) dom = 2;
    return dom;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char ** argv)
{
    initCoinHeadless();

    const char * basepath = (argc > 1) ? argv[1] : "render_quad_viewport_lod";
    char outpath[1024];
    snprintf(outpath, sizeof(outpath), "%s.rgb", basepath);

    /* Render the canonical factory scene as the primary output image.
     * This ensures obol_viewer and obol_render produce identical scenes. */
    {
        SoSeparator *fRoot = ObolTest::Scenes::createQuadViewportLOD(256, 256);
        SbViewportRegion fVp(256, 256);
        SoOffscreenRenderer fRen(fVp);
        fRen.setComponents(SoOffscreenRenderer::RGB);
        fRen.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
        if (fRen.render(fRoot)) {
            fRen.writeToRGB(outpath);
        }
        fRoot->unref();
    }

    int failures = 0;
    printf("\n=== SoQuadViewport LOD composite test ===\n");

    SoSeparator * geom = buildLODScene();

    SoQuadViewport qv;
    qv.setWindowSize(SbVec2s((short)W, (short)H));
    qv.setSceneGraph(geom);

    // TOP_LEFT: z=3 → green sphere (LOD distance < 5)
    {
        SoPerspectiveCamera * cam = new SoPerspectiveCamera;
        cam->position.setValue(0.0f, 0.0f, 3.0f);
        cam->pointAt(SbVec3f(0.0f, 0.0f, 0.0f));
        cam->nearDistance.setValue(0.1f);
        cam->farDistance.setValue(8.0f);
        qv.setCamera(SoQuadViewport::TOP_LEFT, cam);
    }
    // TOP_RIGHT: z=8 → orange cube (LOD distance 5–12)
    {
        SoPerspectiveCamera * cam = new SoPerspectiveCamera;
        cam->position.setValue(0.0f, 0.0f, 8.0f);
        cam->pointAt(SbVec3f(0.0f, 0.0f, 0.0f));
        cam->nearDistance.setValue(0.1f);
        cam->farDistance.setValue(13.0f);
        qv.setCamera(SoQuadViewport::TOP_RIGHT, cam);
    }
    // BOTTOM_LEFT: z=20 → red cone (LOD distance > 12)
    {
        SoPerspectiveCamera * cam = new SoPerspectiveCamera;
        cam->position.setValue(0.0f, 0.0f, 20.0f);
        cam->pointAt(SbVec3f(0.0f, 0.0f, 0.0f));
        cam->nearDistance.setValue(0.1f);
        cam->farDistance.setValue(25.0f);
        qv.setCamera(SoQuadViewport::BOTTOM_LEFT, cam);
    }
    // BOTTOM_RIGHT: orthographic side view (medium distance → orange cube)
    {
        SoOrthographicCamera * cam = new SoOrthographicCamera;
        cam->position.setValue(5.0f, 0.0f, 5.0f);
        cam->pointAt(SbVec3f(0.0f, 0.0f, 0.0f));
        cam->height.setValue(4.0f);
        cam->nearDistance.setValue(0.1f);
        cam->farDistance.setValue(20.0f);
        qv.setCamera(SoQuadViewport::BOTTOM_RIGHT, cam);
    }

    // Black background for all quadrants
    for (int i = 0; i < SoQuadViewport::NUM_QUADS; ++i)
        qv.setBackgroundColor(i, SbColor(0.0f, 0.0f, 0.0f));

    // 4-pixel white border between quadrants
    qv.setBorderWidth(4);
    qv.setBorderColor(SbColor(1.0f, 1.0f, 1.0f));

    // Create a quadrant-sized renderer for per-quad rendering
    const SbVec2s qs = qv.getQuadrantSize();
    SoOffscreenRenderer * renderer =
        new SoOffscreenRenderer(getCoinHeadlessContextManager(),
                                SbViewportRegion(qs[0], qs[1]));
    renderer->setComponents(SoOffscreenRenderer::RGB);

    // ---------------------------------------------------------------------------
    // Validate individual quadrants before writing the composite
    // ---------------------------------------------------------------------------
    bool ok = true;
    int dominants[SoQuadViewport::NUM_QUADS];

    for (int q = 0; q < SoQuadViewport::NUM_QUADS; ++q) {
        char lbl[64];
        snprintf(lbl, sizeof(lbl), "quad%d", q);

        SbViewportRegion qvp(qs[0], qs[1]);
        renderer->setViewportRegion(qvp);
        renderer->setBackgroundColor(qv.getBackgroundColor(q));
        SbBool rendered = renderer->render(qv.getViewport(q)->getRoot());

        if (!rendered) {
            printf("  %s: render FAILED\n", lbl);
            ok = false;
            dominants[q] = -1;
            continue;
        }

        const unsigned char * buf = renderer->getBuffer();
        const int npix = qs[0] * qs[1];
        ok = ok && validateNonBlank(buf, npix, lbl);
        dominants[q] = dominantChannel(buf, npix);
    }

    printf("  LOD dominants: q0=%d q1=%d q2=%d q3=%d (0=R 1=G 2=B)\n",
           dominants[0], dominants[1], dominants[2], dominants[3]);

    // TOP_LEFT (q0) should be green-dominant; BOTTOM_LEFT (q2) red-dominant.
    if (dominants[0] == 1)
        printf("  TOP_LEFT: green sphere (high LOD) — PASS\n");
    else { printf("  TOP_LEFT dominant not green — FAIL\n"); ++failures; }

    if (dominants[2] == 0)
        printf("  BOTTOM_LEFT: red cone (low LOD) — PASS\n");
    else { printf("  BOTTOM_LEFT dominant not red — FAIL\n"); ++failures; }

    // ---------------------------------------------------------------------------
    // Write composite (all 4 quads + white border) to SGI RGB file
    // ---------------------------------------------------------------------------
    SbBool written = qv.writeCompositeToRGB(outpath, renderer);
    printf("  writeCompositeToRGB: %s  path=%s\n",
           written ? "OK" : "FAILED", outpath);
    if (!written) {
        printf("FAIL writeCompositeToRGB\n");
        ++failures;
    } else {
        // Read back the SGI RGB file and verify border pixels are white.
        // SGI RGB: 512-byte header, planar R/G/B planes, rows bottom-to-top.
        const int planeSz = W * H;
        FILE * fp = fopen(outpath, "rb");
        if (fp) {
            fseek(fp, 512, SEEK_SET);
            unsigned char * planes = new unsigned char[planeSz * 3];
            bool readOk = (fread(planes, 1, planeSz * 3, fp) == (size_t)(planeSz * 3));
            fclose(fp);

            if (readOk) {
                // Check horizontal border: row y=H/2, column x=W/4
                int hRow = H / 2, hCol = W / 4;
                unsigned char hR = planes[hRow * W + hCol];
                unsigned char hG = planes[planeSz + hRow * W + hCol];
                unsigned char hB = planes[2 * planeSz + hRow * W + hCol];
                bool hWhite = (hR > 200 && hG > 200 && hB > 200);

                // Check vertical border: row y=H/4, column x=W/2
                int vRow = H / 4, vCol = W / 2;
                unsigned char vR = planes[vRow * W + vCol];
                unsigned char vG = planes[planeSz + vRow * W + vCol];
                unsigned char vB = planes[2 * planeSz + vRow * W + vCol];
                bool vWhite = (vR > 200 && vG > 200 && vB > 200);

                printf("  H-border (x=%d,y=%d) RGB=(%d,%d,%d) white=%d\n",
                       hCol, hRow, hR, hG, hB, (int)hWhite);
                printf("  V-border (x=%d,y=%d) RGB=(%d,%d,%d) white=%d\n",
                       vCol, vRow, vR, vG, vB, (int)vWhite);

                if (!hWhite) { printf("FAIL H-border not white\n"); ++failures; }
                if (!vWhite) { printf("FAIL V-border not white\n"); ++failures; }
            }
            delete[] planes;
        }
    }

    delete renderer;
    geom->unref();

    printf("\n=== Summary: %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
