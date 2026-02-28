/*
 * render_shadow.cpp - Integration test: SoShadowGroup + SoShadowStyle
 *
 * Scene built by ObolTest::Scenes::createShadow.
 *
 * Pixel validation:
 *   - Primary check: lit geometry is visible (nonbg >= 100 pixels).
 *   - Shadow contrast check (informational): when SoShadowGroup::isSupported()
 *     returns TRUE the rendered image should contain both bright neutral
 *     ("lit ground") pixels and darker neutral ("shadow region") pixels.
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
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
    printf("render_shadow: nonbg=%d\n", nonbg);

    if (nonbg < 100) {
        fprintf(stderr, "render_shadow: FAIL – scene appears blank\n");
        return false;
    }
    printf("render_shadow: PASS\n");
    return true;
}

static void reportShadowContrast(const unsigned char *buf)
{
    int litGround    = 0;
    int shadowGround = 0;
    for (int i = 0; i < W * H; ++i) {
        const unsigned char *p = buf + i * 3;
        int r = p[0], g = p[1], b = p[2];
        int hi = std::max({r, g, b});
        int lo = std::min({r, g, b});
        if (hi - lo > 40) continue;
        int luma = (r + g + b) / 3;
        if      (luma > 140) ++litGround;
        else if (luma >  10) ++shadowGround;
    }
    printf("render_shadow: shadow contrast – litGround=%d shadowGround=%d\n",
           litGround, shadowGround);
    if (litGround > 50 && shadowGround > 10)
        printf("render_shadow: shadow regions detected (shadows appear active)\n");
    else
        printf("render_shadow: shadow regions not detected "
               "(GLSL shadow maps may not be active on this driver)\n");
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    bool shadSupported = SoShadowGroup::isSupported();
    printf("render_shadow: SoShadowGroup::isSupported() = %d\n", (int)shadSupported);

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_shadow.rgb");

    SoSeparator *root = ObolTest::Scenes::createShadow(W, H);

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
        fprintf(stderr, "render_shadow: render() failed\n");
    }

    root->unref();
    return ok ? 0 : 1;
}
