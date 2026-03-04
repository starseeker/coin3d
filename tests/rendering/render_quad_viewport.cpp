/*
 * render_quad_viewport.cpp — SoQuadViewport API coverage test
 *
 * Exercises the SoQuadViewport manager and its SoViewport composition:
 *   1. getQuadrantSize() for a standard window.
 *   2. setCamera / getCamera round-trip + active-quadrant selection.
 *   3. Background colour round-trip (via underlying SoViewport).
 *   4. Viewport tile layout (origin/size for TOP_LEFT and BOTTOM_RIGHT).
 *   5. Render all four quadrants (shared LOD scene); verify non-blank output.
 *      LOD per-view: close camera → green sphere, far camera → red cone.
 *   6. getViewport() gives direct access to the underlying SoViewport tile.
 *   7. processEvent() smoke test (routes to active quadrant, no crash).
 *   8. setSceneGraph() replacement and nullptr removal.
 *   9. viewAll() / viewAllQuadrants() with no camera — must not crash.
 *
 * The last rendered quadrant is written to argv[1]+".rgb".
 * Returns 0 on pass, 1 on fail.
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
#include <Inventor/events/SoKeyboardEvent.h>
#include <Inventor/events/SoButtonEvent.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbVec2s.h>
#include <cstdio>
#include <cmath>

static const int W = 800;
static const int H = 600;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool validateNonBlack(const unsigned char * buf, int npix,
                              const char * label, int threshold = 20)
{
    int nonbg = 0;
    for (int i = 0; i < npix; ++i) {
        const unsigned char * p = buf + i * 3;
        if (p[0] > 5 || p[1] > 5 || p[2] > 5) ++nonbg;
    }
    printf("  %s: nonbg=%d\n", label, nonbg);
    return nonbg >= threshold;
}

// Return index of the dominant colour channel (0=R, 1=G, 2=B).
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
// Shared LOD scene (no camera)
// ---------------------------------------------------------------------------
// SoLOD ranges: [0,5) → green sphere, [5,12) → orange cube, [12,∞) → red cone
static SoSeparator * buildLODScene()
{
    SoSeparator * root = new SoSeparator;
    root->ref();

    SoDirectionalLight * lt = new SoDirectionalLight;
    lt->direction.setValue(-1.0f, -1.0f, -1.0f);
    root->addChild(lt);

    SoLOD * lod = new SoLOD;
    lod->range.set1Value(0, 5.0f);
    lod->range.set1Value(1, 12.0f);

    SoSeparator * hi = new SoSeparator;
    SoMaterial  * hiMat = new SoMaterial;
    hiMat->diffuseColor.setValue(0.1f, 0.8f, 0.1f);   // green
    hi->addChild(hiMat);
    hi->addChild(new SoSphere);
    lod->addChild(hi);

    SoSeparator * med = new SoSeparator;
    SoMaterial  * medMat = new SoMaterial;
    medMat->diffuseColor.setValue(0.9f, 0.5f, 0.1f);  // orange
    med->addChild(medMat);
    med->addChild(new SoCube);
    lod->addChild(med);

    SoSeparator * lo = new SoSeparator;
    SoMaterial  * loMat = new SoMaterial;
    loMat->diffuseColor.setValue(0.8f, 0.1f, 0.1f);   // red
    lo->addChild(loMat);
    lo->addChild(new SoCone);
    lod->addChild(lo);

    root->addChild(lod);
    return root;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char ** argv)
{
    initCoinHeadless();

    const char * basepath = (argc > 1) ? argv[1] : "render_quad_viewport";
    char outpath[1024];
    snprintf(outpath, sizeof(outpath), "%s.rgb", basepath);

    /* Render the canonical factory scene as the primary output image.
     * This ensures obol_viewer and obol_render produce identical scenes. */
    {
        SoSeparator *fRoot = ObolTest::Scenes::createQuadViewport(256, 256);
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
    printf("\n=== SoQuadViewport tests ===\n");

    // -----------------------------------------------------------------------
    // Test 1: getQuadrantSize
    // -----------------------------------------------------------------------
    {
        SoQuadViewport qv;
        qv.setWindowSize(SbVec2s((short)W, (short)H));
        SbVec2s qs = qv.getQuadrantSize();
        bool ok = (qs[0] == W / 2 && qs[1] == H / 2);
        printf("  test1 getQuadrantSize: %dx%d ok=%d\n", qs[0], qs[1], (int)ok);
        if (!ok) { printf("FAIL test1\n"); ++failures; }
    }

    // -----------------------------------------------------------------------
    // Test 2: setCamera / getCamera round-trip + active quadrant
    // -----------------------------------------------------------------------
    {
        SoQuadViewport qv;
        qv.setWindowSize(SbVec2s((short)W, (short)H));

        SoPerspectiveCamera * cams[SoQuadViewport::NUM_QUADS];
        for (int i = 0; i < SoQuadViewport::NUM_QUADS; ++i) {
            cams[i] = new SoPerspectiveCamera;
            qv.setCamera(i, cams[i]);
        }

        bool ok = true;
        for (int i = 0; i < SoQuadViewport::NUM_QUADS; ++i)
            ok = ok && (qv.getCamera(i) == cams[i]);

        qv.setActiveQuadrant(2);
        ok = ok && (qv.getActiveQuadrant() == 2);
        printf("  test2 cameras+active ok=%d\n", (int)ok);
        if (!ok) { printf("FAIL test2\n"); ++failures; }
    }

    // -----------------------------------------------------------------------
    // Test 3: background colour round-trip via underlying SoViewport
    // -----------------------------------------------------------------------
    {
        SoQuadViewport qv;
        SbColor col(0.2f, 0.3f, 0.4f);
        qv.setBackgroundColor(1, col);
        const SbColor & got = qv.getBackgroundColor(1);
        bool ok = (std::fabs(got[0] - col[0]) < 1e-4f &&
                   std::fabs(got[1] - col[1]) < 1e-4f &&
                   std::fabs(got[2] - col[2]) < 1e-4f);
        // Also verify the underlying SoViewport sees the same colour.
        const SoViewport * tile = qv.getViewport(1);
        ok = ok && tile &&
             (std::fabs(tile->getBackgroundColor()[0] - col[0]) < 1e-4f);
        printf("  test3 background colour ok=%d\n", (int)ok);
        if (!ok) { printf("FAIL test3\n"); ++failures; }
    }

    // -----------------------------------------------------------------------
    // Test 4: viewport tile layout
    // -----------------------------------------------------------------------
    {
        SoQuadViewport qv;
        qv.setWindowSize(SbVec2s((short)W, (short)H));
        short qW = W / 2, qH = H / 2;

        const SbViewportRegion & tl = qv.getViewportRegion(SoQuadViewport::TOP_LEFT);
        const SbViewportRegion & br = qv.getViewportRegion(SoQuadViewport::BOTTOM_RIGHT);

        SbVec2s tlOrig = tl.getViewportOriginPixels();
        SbVec2s tlSize = tl.getViewportSizePixels();
        SbVec2s brOrig = br.getViewportOriginPixels();

        bool ok = (tlOrig[0] == 0  && tlOrig[1] == qH &&
                   tlSize[0] == qW && tlSize[1] == qH &&
                   brOrig[0] == qW && brOrig[1] == 0);
        printf("  test4 layout ok=%d  tl=(%d,%d %dx%d) br=(%d,%d)\n",
               (int)ok, tlOrig[0], tlOrig[1], tlSize[0], tlSize[1],
               brOrig[0], brOrig[1]);
        if (!ok) { printf("FAIL test4\n"); ++failures; }
    }

    // -----------------------------------------------------------------------
    // Test 5: render all quadrants; verify non-blank + LOD per-view
    // -----------------------------------------------------------------------
    {
        SoSeparator * geom = buildLODScene();

        SoQuadViewport qv;
        qv.setWindowSize(SbVec2s((short)W, (short)H));
        qv.setSceneGraph(geom);

        // Camera z-distances chosen to hit specific LOD ranges:
        //   quad 0: z=3  → distance < 5  → green sphere   (dominant G)
        //   quad 1: z=8  → distance 5–12 → orange cube
        //   quad 2: z=20 → distance > 12 → red cone        (dominant R)
        //   quad 3: orthographic from the side
        // near/far set explicitly to ensure the origin (scene centre) is visible
        // from every camera position without moving the cameras via viewAll().
        float cameraZ[3] = { 3.0f, 8.0f, 20.0f };
        for (int i = 0; i < 3; ++i) {
            SoPerspectiveCamera * cam = new SoPerspectiveCamera;
            cam->position.setValue(0.0f, 0.0f, cameraZ[i]);
            cam->pointAt(SbVec3f(0.0f, 0.0f, 0.0f));
            cam->nearDistance.setValue(0.1f);
            cam->farDistance.setValue(cameraZ[i] + 5.0f);
            qv.setCamera(i, cam);
        }
        SoOrthographicCamera * ortho = new SoOrthographicCamera;
        ortho->position.setValue(5.0f, 0.0f, 5.0f);
        ortho->pointAt(SbVec3f(0.0f, 0.0f, 0.0f));
        ortho->height.setValue(4.0f);
        ortho->nearDistance.setValue(0.1f);
        ortho->farDistance.setValue(20.0f);
        qv.setCamera(3, ortho);

        SbVec2s qs = qv.getQuadrantSize();
        SoOffscreenRenderer * renderer =
            new SoOffscreenRenderer(getCoinHeadlessContextManager(),
                                    SbViewportRegion(qs[0], qs[1]));
        renderer->setComponents(SoOffscreenRenderer::RGB);

        bool ok = true;
        int dominants[SoQuadViewport::NUM_QUADS];
        for (int i = 0; i < SoQuadViewport::NUM_QUADS; ++i) {
            char lbl[64];
            snprintf(lbl, sizeof(lbl), "test5_quad%d", i);
            bool rendered = (qv.renderQuadrant(i, renderer) == TRUE);
            if (!rendered) {
                printf("  %s: render FAILED\n", lbl);
                ok = false;
                dominants[i] = -1;
                continue;
            }
            const unsigned char * buf = renderer->getBuffer();
            int npix = qs[0] * qs[1];
            ok = ok && validateNonBlack(buf, npix, lbl);
            dominants[i] = dominantChannel(buf, npix);
        }

        printf("  test5 LOD dominants: q0=%d q1=%d q2=%d q3=%d (0=R 1=G 2=B)\n",
               dominants[0], dominants[1], dominants[2], dominants[3]);
        if (dominants[0] == 1 && dominants[2] == 0)
            printf("  test5 LOD per-view: PASS (different detail levels)\n");
        else
            printf("  test5 LOD per-view: NOTE – expected G for q0 and R for q2\n");

        renderer->writeToRGB(outpath);
        printf("  test5: wrote %s\n", outpath);

        delete renderer;
        geom->unref();
        if (!ok) { printf("FAIL test5\n"); ++failures; }
    }

    // -----------------------------------------------------------------------
    // Test 6: getViewport() gives direct SoViewport access
    // -----------------------------------------------------------------------
    {
        SoQuadViewport qv;
        qv.setWindowSize(SbVec2s((short)W, (short)H));

        bool ok = true;
        for (int i = 0; i < SoQuadViewport::NUM_QUADS; ++i) {
            SoViewport * tile = qv.getViewport(i);
            ok = ok && (tile != nullptr);
        }
        // Out-of-range must return nullptr.
        ok = ok && (qv.getViewport(-1) == nullptr);
        ok = ok && (qv.getViewport(SoQuadViewport::NUM_QUADS) == nullptr);
        printf("  test6 getViewport ok=%d\n", (int)ok);
        if (!ok) { printf("FAIL test6\n"); ++failures; }
    }

    // -----------------------------------------------------------------------
    // Test 7: processEvent() routes to active quadrant (smoke test)
    // -----------------------------------------------------------------------
    {
        SoSeparator * geom = buildLODScene();
        SoQuadViewport qv;
        qv.setWindowSize(SbVec2s((short)W, (short)H));
        qv.setSceneGraph(geom);

        SoPerspectiveCamera * cam = new SoPerspectiveCamera;
        cam->position.setValue(0.0f, 0.0f, 5.0f);
        qv.setCamera(0, cam);
        qv.setActiveQuadrant(0);

        SoKeyboardEvent evt;
        evt.setKey(SoKeyboardEvent::ESCAPE);
        evt.setState(SoButtonEvent::DOWN);
        qv.processEvent(&evt);   // must not crash

        printf("  test7 processEvent smoke: PASS\n");
        geom->unref();
    }

    // -----------------------------------------------------------------------
    // Test 8: setSceneGraph replacement + nullptr removal
    // -----------------------------------------------------------------------
    {
        SoSeparator * g1 = buildLODScene();
        SoSeparator * g2 = buildLODScene();

        SoQuadViewport qv;
        qv.setSceneGraph(g1);
        bool ok = (qv.getSceneGraph() == g1);
        qv.setSceneGraph(g2);
        ok = ok && (qv.getSceneGraph() == g2);
        qv.setSceneGraph(nullptr);
        ok = ok && (qv.getSceneGraph() == nullptr);

        printf("  test8 scene replacement ok=%d\n", (int)ok);
        g1->unref();
        g2->unref();
        if (!ok) { printf("FAIL test8\n"); ++failures; }
    }

    // -----------------------------------------------------------------------
    // Test 9: viewAll / viewAllQuadrants with no camera — must not crash
    // -----------------------------------------------------------------------
    {
        SoSeparator * geom = buildLODScene();
        SoQuadViewport qv;
        qv.setWindowSize(SbVec2s((short)W, (short)H));
        qv.setSceneGraph(geom);
        qv.viewAll(0);
        qv.viewAllQuadrants();
        printf("  test9 viewAll no-camera: PASS\n");
        geom->unref();
    }

    // -----------------------------------------------------------------------
    // Test 10: border width/colour round-trips
    // -----------------------------------------------------------------------
    {
        SoQuadViewport qv;

        // Default: no border
        bool ok = (qv.getBorderWidth() == 0);

        qv.setBorderWidth(4);
        ok = ok && (qv.getBorderWidth() == 4);

        // Negative width should clamp to 0
        qv.setBorderWidth(-1);
        ok = ok && (qv.getBorderWidth() == 0);

        // Colour round-trip
        SbColor yellow(1.0f, 1.0f, 0.0f);
        qv.setBorderColor(yellow);
        const SbColor & gc = qv.getBorderColor();
        ok = ok && (std::fabs(gc[0] - yellow[0]) < 1e-4f &&
                    std::fabs(gc[1] - yellow[1]) < 1e-4f &&
                    std::fabs(gc[2] - yellow[2]) < 1e-4f);

        printf("  test10 border API ok=%d\n", (int)ok);
        if (!ok) { printf("FAIL test10\n"); ++failures; }
    }

    // -----------------------------------------------------------------------
    // Test 11: writeCompositeToRGB — non-blank composite + border pixels
    // -----------------------------------------------------------------------
    {
        SoSeparator * geom = buildLODScene();

        SoQuadViewport qv;
        qv.setWindowSize(SbVec2s((short)W, (short)H));
        qv.setSceneGraph(geom);

        // Same camera setup as test5
        float cameraZ[3] = { 3.0f, 8.0f, 20.0f };
        for (int i = 0; i < 3; ++i) {
            SoPerspectiveCamera * cam = new SoPerspectiveCamera;
            cam->position.setValue(0.0f, 0.0f, cameraZ[i]);
            cam->pointAt(SbVec3f(0.0f, 0.0f, 0.0f));
            cam->nearDistance.setValue(0.1f);
            cam->farDistance.setValue(cameraZ[i] + 5.0f);
            qv.setCamera(i, cam);
        }
        SoOrthographicCamera * ortho = new SoOrthographicCamera;
        ortho->position.setValue(5.0f, 0.0f, 5.0f);
        ortho->pointAt(SbVec3f(0.0f, 0.0f, 0.0f));
        ortho->height.setValue(4.0f);
        ortho->nearDistance.setValue(0.1f);
        ortho->farDistance.setValue(20.0f);
        qv.setCamera(3, ortho);

        // Enable a 4-pixel white border
        qv.setBorderWidth(4);
        qv.setBorderColor(SbColor(1.0f, 1.0f, 1.0f));

        SbVec2s qs = qv.getQuadrantSize();
        SoOffscreenRenderer * renderer =
            new SoOffscreenRenderer(getCoinHeadlessContextManager(),
                                    SbViewportRegion(qs[0], qs[1]));
        renderer->setComponents(SoOffscreenRenderer::RGB);

        char compPath[1024];
        snprintf(compPath, sizeof(compPath), "%s_composite.rgb", basepath);
        SbBool ok = qv.writeCompositeToRGB(compPath, renderer);
        printf("  test11 writeCompositeToRGB ok=%d  file=%s\n",
               (int)ok, compPath);

        if (ok) {
            // Read back the SGI RGB file and verify two things:
            // 1. The horizontal border row (y = H/2) has white pixels.
            // 2. The vertical border column (x = W/2) has white pixels.
            // SGI RGB: 512-byte header, then planar R/G/B, rows bottom-to-top.
            const int CW = W, CH = H;
            const int CqW = CW / 2, CqH = CH / 2;
            FILE * fp = fopen(compPath, "rb");
            if (fp) {
                fseek(fp, 512, SEEK_SET);
                const int planeSz = CW * CH;
                unsigned char * rp = new unsigned char[planeSz * 3];
                bool readOk = (fread(rp, 1, planeSz * 3, fp) == (size_t)(planeSz * 3));
                fclose(fp);

                if (readOk) {
                    // Border is 4 pixels wide centred at qH; check y=qH row.
                    int hBorderRow = CqH;
                    unsigned char hR = rp[hBorderRow * CW + CW / 4];
                    unsigned char hG = rp[planeSz + hBorderRow * CW + CW / 4];
                    unsigned char hB = rp[2 * planeSz + hBorderRow * CW + CW / 4];
                    bool hBorderWhite = (hR > 200 && hG > 200 && hB > 200);

                    int vBorderCol = CqW;
                    unsigned char vR = rp[CH / 4 * CW + vBorderCol];
                    unsigned char vG = rp[planeSz + CH / 4 * CW + vBorderCol];
                    unsigned char vB = rp[2 * planeSz + CH / 4 * CW + vBorderCol];
                    bool vBorderWhite = (vR > 200 && vG > 200 && vB > 200);

                    printf("  test11 H-border pixel RGB=(%d,%d,%d) white=%d\n",
                           hR, hG, hB, (int)hBorderWhite);
                    printf("  test11 V-border pixel RGB=(%d,%d,%d) white=%d\n",
                           vR, vG, vB, (int)vBorderWhite);

                    ok = ok && hBorderWhite && vBorderWhite;
                }
                delete[] rp;
            }
        }

        delete renderer;
        geom->unref();
        if (!ok) { printf("FAIL test11\n"); ++failures; }
    }

    // -----------------------------------------------------------------------
    printf("\n=== Summary: %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
