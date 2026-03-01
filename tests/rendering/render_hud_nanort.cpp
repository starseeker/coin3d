/*
 * render_hud_nanort.cpp – NanoRT HUD overlay validation test.
 *
 * Validates two things:
 *
 *  1. SoHUDLabel text compositing (nrtHUDLabelPreCB):
 *     Uses createHUD(W,H) — a blue cube with a yellow "HUD Overlay" label.
 *     Checks for yellow pixels in the top half of the rendered image.
 *
 *  2. SoHUDButton border + label compositing (nrtHUDButtonPreCB):
 *     Uses createHUDOverlay(W,H) — a blue sphere with labelled side-menu
 *     buttons.  Checks for colored border pixels on the left side of the
 *     image (where the buttons are placed).
 *
 * Viewport : 800 × 600
 * Output   : argv[1]+".rgb" (SGI RGB, used by the image-comparison infra)
 *
 * Building / linking: see CMakeLists.txt add_nanort_testlib_rendering_test()
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <cstdio>

static const int W = 800;
static const int H = 600;

#ifdef OBOL_NANORT_BUILD
// ---- colour-detection thresholds -----------------------------------------
// Matches the createHUD label color: SbColor(1.0, 1.0, 0.2) → yellow.
static const int YELLOW_MIN_RED      = 120;
static const int YELLOW_MIN_GREEN    = 100;
static const int YELLOW_MAX_BLUE     = 80;
static const int YELLOW_RG_TOLERANCE = 60;

// Minimum pixel counts for the two sub-tests to pass.
static const int MIN_NON_BACKGROUND_PIXELS = 500;
static const int MIN_YELLOW_PIXELS         = 10;   // HUD label text
// Button border pixels: createHUDOverlay buttons use green/yellow/red hues.
// We only require a small count because the borders are 1-pixel-wide rectangles.
static const int MIN_BORDER_PIXELS         = 5;

// ---- validateHUDText: checks SoHUDLabel text pixels ----------------------
static bool validateHUDText(const unsigned char * buf, int w, int h)
{
    // "HUD Overlay" label: yellow (R≈G, B low) near the top.
    // createHUD places the label at y = height - 30 (GL row ≈ height - 30).
    int yellowInTopHalf = 0;
    int nonBg = 0;

    for (int row = h / 2; row < h; ++row) {
        for (int col = 0; col < w; ++col) {
            const unsigned char * p = buf + (row * w + col) * 3;
            const int r = p[0], g = p[1], b = p[2];
            if (r > YELLOW_MIN_RED && g > YELLOW_MIN_GREEN &&
                b < YELLOW_MAX_BLUE && std::abs(r - g) < YELLOW_RG_TOLERANCE)
                ++yellowInTopHalf;
        }
    }
    for (int i = 0; i < w * h; ++i) {
        const unsigned char * p = buf + i * 3;
        if (p[0] > 10 || p[1] > 10 || p[2] > 10) ++nonBg;
    }

    printf("render_hud_nanort (label): yellowInTopHalf=%d nonBg=%d\n",
           yellowInTopHalf, nonBg);

    if (nonBg < MIN_NON_BACKGROUND_PIXELS) {
        fprintf(stderr,
                "render_hud_nanort: FAIL – label scene appears blank (%d non-bg)\n",
                nonBg);
        return false;
    }
    if (yellowInTopHalf < MIN_YELLOW_PIXELS) {
        fprintf(stderr,
                "render_hud_nanort: FAIL – no yellow HUD label text in top half "
                "(%d pixels); nrtHUDLabelPreCB may not be working\n",
                yellowInTopHalf);
        return false;
    }
    return true;
}

// ---- validateHUDButtons: checks SoHUDButton border pixels ----------------
// createHUDOverlay places 5 buttons on the left side (x=10..110).
// Button borderColors are muted green/yellow/red hues.
// We scan the left column band (x < 120) for colored, non-dark pixels.
static bool validateHUDButtons(const unsigned char * buf, int w, int h)
{
    int borderPixels = 0;

    for (int row = 0; row < h; ++row) {
        for (int col = 0; col < 120 && col < w; ++col) {
            const unsigned char * p = buf + (row * w + col) * 3;
            // Count any non-trivially-dark pixel as a border/text pixel.
            // Button borderColors are scaled-down label colors so they won't
            // be bright, but should be clearly above the background threshold.
            if (p[0] > 30 || p[1] > 30 || p[2] > 30)
                ++borderPixels;
        }
    }

    printf("render_hud_nanort (buttons): borderPixels in left band=%d\n",
           borderPixels);

    if (borderPixels < MIN_BORDER_PIXELS) {
        fprintf(stderr,
                "render_hud_nanort: FAIL – no button border/text pixels found "
                "in left column band (%d pixels); nrtHUDButtonPreCB may not "
                "be working\n", borderPixels);
        return false;
    }
    return true;
}
#endif // OBOL_NANORT_BUILD

static const int MAX_PATH_LEN = 1024;

int main(int argc, char ** argv)
{
    initCoinHeadless();

    // ---- Sub-test 1: SoHUDLabel text compositing --------------------------
    {
        SoSeparator * root = ObolTest::Scenes::createHUD(W, H);

        char outpath[MAX_PATH_LEN];
        if (argc > 1)
            snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
        else
            snprintf(outpath, sizeof(outpath), "render_hud_nanort.rgb");

        bool ok = false;
#ifdef OBOL_NANORT_BUILD
        SoOffscreenRenderer renderer(SbViewportRegion(W, H));
        renderer.setComponents(SoOffscreenRenderer::RGB);
        renderer.setBackgroundColor(SbColor(0.1f, 0.1f, 0.1f));
        if (renderer.render(root)) {
            const unsigned char * buf = renderer.getBuffer();
            ok = (buf != nullptr) && validateHUDText(buf, W, H) &&
                 renderer.writeToRGB(outpath);
        } else {
            fprintf(stderr, "render_hud_nanort: render() failed (label scene)\n");
        }
#else
        ok = renderToFile(root, outpath, W, H);
#endif
        root->unref();
        if (!ok) return 1;
    }

    // ---- Sub-test 2: SoHUDButton border + label compositing ---------------
    {
        SoSeparator * root2 = ObolTest::Scenes::createHUDOverlay(W, H);

        char outpath2[MAX_PATH_LEN];
        if (argc > 1)
            snprintf(outpath2, sizeof(outpath2), "%s_overlay.rgb", argv[1]);
        else
            snprintf(outpath2, sizeof(outpath2), "render_hud_nanort_overlay.rgb");

        bool ok2 = false;
#ifdef OBOL_NANORT_BUILD
        SoOffscreenRenderer renderer2(SbViewportRegion(W, H));
        renderer2.setComponents(SoOffscreenRenderer::RGB);
        renderer2.setBackgroundColor(SbColor(0.05f, 0.05f, 0.1f));
        if (renderer2.render(root2)) {
            const unsigned char * buf2 = renderer2.getBuffer();
            ok2 = (buf2 != nullptr) && validateHUDButtons(buf2, W, H) &&
                  renderer2.writeToRGB(outpath2);
        } else {
            fprintf(stderr, "render_hud_nanort: render() failed (overlay scene)\n");
        }
#else
        ok2 = renderToFile(root2, outpath2, W, H);
#endif
        root2->unref();
        if (!ok2) return 1;
    }

    printf("render_hud_nanort: PASS\n");
    return 0;
}
