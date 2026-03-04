/*
 * render_shadow_advanced.cpp — Extended SoShadowGroup coverage test
 *
 * Uses the shared ObolTest::Scenes::createShadowAdvanced scene factory so that
 * the CLI image-generation path (obol_render) and the interactive viewer
 * (obol_viewer) render an identical scene.
 *
 * The scene exercises:
 *   - SoShadowSpotLight as the shadow-casting light source
 *   - SoShadowGroup::visibilityRadius / visibilityNearRadius / epsilon / threshold
 *   - SoShadowGroup::shadowCachingEnabled field
 *   - SoShadowStyle::SHADOWED / CASTS_SHADOW_AND_SHADOWED / NO_SHADOWING
 *
 * Pixel validation:
 *   - Primary check: lit geometry visible (nonbg >= 50 pixels).
 *   - Shadow contrast check (informational): when SoShadowGroup::isSupported()
 *     returns TRUE the image should contain both bright and darker neutral pixels.
 *
 * Writes argv[1]+".rgb" (or "render_shadow_advanced.rgb") and returns 0 on pass.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <Inventor/annex/FXViz/nodes/SoShadowGroup.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <algorithm>
#include <cstdio>

static const int W = 256;
static const int H = 256;

static bool validateScene(const unsigned char *buf)
{
    int nonbg = 0;
    for (int i = 0; i < W * H; ++i) {
        const unsigned char *p = buf + i * 3;
        if (p[0] > 5 || p[1] > 5 || p[2] > 5) ++nonbg;
    }
    printf("render_shadow_advanced: nonbg=%d\n", nonbg);

    if (nonbg < 50) {
        fprintf(stderr, "render_shadow_advanced: FAIL – scene appears blank\n");
        return false;
    }
    printf("render_shadow_advanced: PASS\n");
    return true;
}

static void reportShadowContrast(const unsigned char *buf)
{
    /* Count pixels that appear to belong to the lit ground plane versus the
     * shadow region.  A pixel is considered "ground" when it is nearly achromatic
     * (max - min channel < 40 = low saturation).  Among those, pixels with
     * luma >= 140 are "lit" and those with luma 11-139 are "shadowed".
     * Thresholds: litGround > 30 and shadowGround > 5 indicate working shadows. */
    static const int SATURATION_LIMIT = 40;  /* max-min channel: achromatic test */
    static const int LIT_LUMA_MIN     = 140; /* luma threshold for lit ground   */
    static const int SHADOW_LUMA_MIN  =  10; /* luma threshold for shadow region */
    static const int LIT_MIN_PIXELS   =  30; /* minimum lit pixels for "detected" */
    static const int SHADOW_MIN_PIXELS =  5; /* minimum shadow pixels for "detected" */

    int litGround    = 0;
    int shadowGround = 0;
    for (int i = 0; i < W * H; ++i) {
        const unsigned char *p = buf + i * 3;
        int r = p[0], g = p[1], b = p[2];
        int hi = std::max({r, g, b});
        int lo = std::min({r, g, b});
        if (hi - lo > SATURATION_LIMIT) continue;
        int luma = (r + g + b) / 3;
        if      (luma >= LIT_LUMA_MIN)    ++litGround;
        else if (luma >  SHADOW_LUMA_MIN) ++shadowGround;
    }
    printf("render_shadow_advanced: shadow contrast – litGround=%d shadowGround=%d\n",
           litGround, shadowGround);
    if (litGround > LIT_MIN_PIXELS && shadowGround > SHADOW_MIN_PIXELS)
        printf("render_shadow_advanced: shadow regions detected\n");
    else
        printf("render_shadow_advanced: shadow regions not detected "
               "(GLSL shadow maps may not be active on this driver)\n");
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoShadowGroup *sg = new SoShadowGroup(getCoinHeadlessContextManager());
    sg->ref();
    bool shadSupported = sg->isSupported();
    sg->unref();
    printf("render_shadow_advanced: SoShadowGroup::isSupported() = %d\n",
           (int)shadSupported);

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_shadow_advanced.rgb");

    /* Use the shared testlib scene factory so viewer and CLI render the same scene */
    SoSeparator *root = ObolTest::Scenes::createShadowAdvanced(W, H);

    SbViewportRegion vp(W, H);
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    bool ok = false;
    if (renderer.render(root)) {
        const unsigned char *buf = renderer.getBuffer();
        bool pixOk = (buf != nullptr) && validateScene(buf);
        if (buf && shadSupported) reportShadowContrast(buf);
        ok = pixOk && renderer.writeToRGB(outpath);
    } else {
        fprintf(stderr, "render_shadow_advanced: render() failed\n");
    }

    root->unref();
    return ok ? 0 : 1;
}
