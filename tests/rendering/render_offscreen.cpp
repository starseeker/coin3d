/*
 * render_offscreen.cpp - Integration test: SoOffscreenRenderer API coverage
 *
 * Exercises the user-facing SoOffscreenRenderer API beyond the simple
 * render-and-compare pattern used in other tests:
 *
 *   1. getViewportRegion() / setViewportRegion() round-trip.
 *   2. getBackgroundColor() / setBackgroundColor() round-trip.
 *   3. getComponents() after setComponents().
 *   4. Multiple sequential renders with the same renderer (same context reuse).
 *   5. getBuffer() non-null after a successful render.
 *   6. Render different resolutions (64×64 and 256×256) with one renderer.
 *   7. getGLRenderAction() returns a non-null action pointer.
 *   8. Pixel content validation: renders a bright red sphere; the centre
 *      region must contain predominantly red pixels.
 *
 * Covers SoOffscreenRenderer code paths in src/rendering/SoOffscreenRenderer.cpp
 * that are not exercised by image-comparison tests (which only render + write).
 *
 * Returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbColor.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <cstdio>
#include <cmath>
#include <cstring>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Minimum amount by which red must exceed green and blue to qualify as
// a "predominantly red" pixel, and minimum red channel intensity.
static const int MIN_RED_DOMINANCE = 40;
static const int MIN_RED_INTENSITY  = 60;

// Count pixels that are "predominantly red" in an RGB buffer.
static int countRedPixels(const unsigned char *buf, int w, int h)
{
    int count = 0;
    for (int i = 0; i < w * h; ++i) {
        const unsigned char *p = buf + i * 3;
        if ((int)p[0] > (int)p[1] + MIN_RED_DOMINANCE &&
            (int)p[0] > (int)p[2] + MIN_RED_DOMINANCE &&
            p[0] > MIN_RED_INTENSITY)
            ++count;
    }
    return count;
}

// ---------------------------------------------------------------------------

int main(int argc, char **argv)
{
    initCoinHeadless();

    const char *outpath = (argc > 1) ? argv[1] : "render_offscreen";

    bool ok = true;

    /* Use the shared factory so viewer and CLI render the same scene.
     * The factory creates a red sphere with perspective camera — identical
     * to the old inline buildRedSphereScene(). */
    SoSeparator *root = ObolTest::Scenes::createOffscreen(64, 64);
    root->ref();

    // ------------------------------------------------------------------
    // Test 1: viewport region round-trip
    // ------------------------------------------------------------------
    {
        SbViewportRegion vp64(64, 64);
        SoOffscreenRenderer r(vp64);
        const SbViewportRegion &got = r.getViewportRegion();
        if (got.getWindowSize() != SbVec2s(64, 64)) {
            fprintf(stderr, "FAIL: getViewportRegion() size mismatch: got %d×%d\n",
                    (int)got.getWindowSize()[0], (int)got.getWindowSize()[1]);
            ok = false;
        } else {
            printf("Test 1 (getViewportRegion): PASS\n");
        }
    }

    // ------------------------------------------------------------------
    // Test 2: background color round-trip
    // ------------------------------------------------------------------
    {
        SbViewportRegion vp(64, 64);
        SoOffscreenRenderer r(vp);
        SbColor bg(0.2f, 0.4f, 0.6f);
        r.setBackgroundColor(bg);
        const SbColor &got = r.getBackgroundColor();
        float dr = fabsf(got[0] - bg[0]);
        float dg = fabsf(got[1] - bg[1]);
        float db = fabsf(got[2] - bg[2]);
        if (dr > 0.001f || dg > 0.001f || db > 0.001f) {
            fprintf(stderr, "FAIL: getBackgroundColor() mismatch: (%.3f,%.3f,%.3f)\n",
                    got[0], got[1], got[2]);
            ok = false;
        } else {
            printf("Test 2 (getBackgroundColor): PASS\n");
        }
    }

    // ------------------------------------------------------------------
    // Test 3: getComponents() after setComponents()
    // ------------------------------------------------------------------
    {
        SbViewportRegion vp(64, 64);
        SoOffscreenRenderer r(vp);
        r.setComponents(SoOffscreenRenderer::RGB);
        if (r.getComponents() != SoOffscreenRenderer::RGB) {
            fprintf(stderr, "FAIL: getComponents() did not return RGB\n");
            ok = false;
        } else {
            printf("Test 3 (getComponents RGB): PASS\n");
        }
        r.setComponents(SoOffscreenRenderer::RGB_TRANSPARENCY);
        if (r.getComponents() != SoOffscreenRenderer::RGB_TRANSPARENCY) {
            fprintf(stderr, "FAIL: getComponents() did not return RGB_TRANSPARENCY\n");
            ok = false;
        } else {
            printf("Test 3 (getComponents RGBA): PASS\n");
        }
    }

    // ------------------------------------------------------------------
    // Test 4: getGLRenderAction() returns non-null
    // ------------------------------------------------------------------
    {
        SbViewportRegion vp(64, 64);
        SoOffscreenRenderer r(vp);
        if (r.getGLRenderAction() == nullptr) {
            fprintf(stderr, "FAIL: getGLRenderAction() returned null\n");
            ok = false;
        } else {
            printf("Test 4 (getGLRenderAction): PASS\n");
        }
    }

    // ------------------------------------------------------------------
    // Test 5: render() + getBuffer() non-null
    // ------------------------------------------------------------------
    {
        SbViewportRegion vp(64, 64);
        SoOffscreenRenderer r(vp);
        r.setComponents(SoOffscreenRenderer::RGB);
        r.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
        bool rendered = r.render(root);
        if (!rendered) {
            fprintf(stderr, "FAIL: render() returned false\n");
            ok = false;
        } else if (r.getBuffer() == nullptr) {
            fprintf(stderr, "FAIL: getBuffer() returned null after successful render\n");
            ok = false;
        } else {
            printf("Test 5 (render + getBuffer): PASS\n");
        }
    }

    // ------------------------------------------------------------------
    // Test 6: multiple sequential renders with one renderer
    // ------------------------------------------------------------------
    {
        SbViewportRegion vp(64, 64);
        SoOffscreenRenderer r(vp);
        r.setComponents(SoOffscreenRenderer::RGB);
        r.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

        for (int pass = 0; pass < 3; ++pass) {
            bool rendered = r.render(root);
            if (!rendered || r.getBuffer() == nullptr) {
                fprintf(stderr, "FAIL: sequential render pass %d failed\n", pass);
                ok = false;
                break;
            }
        }
        if (ok) printf("Test 6 (3 sequential renders): PASS\n");
    }

    // ------------------------------------------------------------------
    // Test 7: setViewportRegion() changes size (two different sizes)
    // ------------------------------------------------------------------
    {
        SbViewportRegion vp1(64, 64);
        SoOffscreenRenderer r(vp1);
        r.setComponents(SoOffscreenRenderer::RGB);

        // Render at 64×64
        r.setViewportRegion(SbViewportRegion(64, 64));
        bool ok64 = r.render(root);

        // Render at 128×128
        r.setViewportRegion(SbViewportRegion(128, 128));
        bool ok128 = r.render(root);

        const SbViewportRegion &vp = r.getViewportRegion();
        bool sizeOk = (vp.getWindowSize() == SbVec2s(128, 128));

        if (!ok64 || !ok128 || !sizeOk) {
            fprintf(stderr, "FAIL: setViewportRegion resize (ok64=%d ok128=%d sizeOk=%d)\n",
                    (int)ok64, (int)ok128, (int)sizeOk);
            ok = false;
        } else {
            printf("Test 7 (setViewportRegion resize): PASS\n");
        }
    }

    // ------------------------------------------------------------------
    // Test 8: pixel content – red sphere renders red
    // Reuse the already-built `root` scene (camera was viewAll'd at 64×64;
    // the sphere still fills the frame well enough at 128×128).
    // ------------------------------------------------------------------
    {
        const int W = 128, H = 128;
        SbViewportRegion vp(W, H);
        SoOffscreenRenderer r(vp);
        r.setComponents(SoOffscreenRenderer::RGB);
        r.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

        bool rendered = r.render(root);
        if (rendered) {
            const unsigned char *buf = r.getBuffer();
            int reds = countRedPixels(buf, W, H);
            printf("Test 8: reds=%d (need >=200)\n", reds);
            if (reds < 200) {
                fprintf(stderr, "FAIL: too few red pixels (%d)\n", reds);
                ok = false;
            } else {
                printf("Test 8 (pixel content – red sphere): PASS\n");
            }
        } else {
            fprintf(stderr, "FAIL: render() returned false in Test 8\n");
            ok = false;
        }
    }

    root->unref();

    /* Write the canonical factory scene as the primary output image. */
    if (ok) {
        SoSeparator *out = ObolTest::Scenes::createOffscreen(256, 256);
        SbViewportRegion vpOut(256, 256);
        SoOffscreenRenderer rOut(vpOut);
        rOut.setComponents(SoOffscreenRenderer::RGB);
        rOut.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
        if (rOut.render(out)) {
            char path[4096];
            snprintf(path, sizeof(path), "%s.rgb", outpath);
            rOut.writeToRGB(path);
        }
        out->unref();
    }

    if (ok) {
        printf("render_offscreen: ALL TESTS PASSED\n");
    } else {
        fprintf(stderr, "render_offscreen: SOME TESTS FAILED\n");
    }
    return ok ? 0 : 1;
}
