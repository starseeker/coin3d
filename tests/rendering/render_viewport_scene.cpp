/*
 * render_viewport_scene.cpp — SoViewport visual regression / control image test
 *
 * Renders a deterministic scene through the SoViewport API and writes the
 * result as an SGI RGB file.  The image is used as a ground-truth control
 * image for ongoing regression testing of the SoViewport class.
 *
 * Scene (256×256):
 *   A directional light illuminating a green sphere placed at the origin.
 *   Camera: perspective, z=5, looking at origin.  viewAll() is called to
 *   frame the scene exactly.
 *
 * Pixel assertions:
 *   - At least 200 non-background pixels (scene is not blank).
 *   - The green channel is dominant (sphere material is green).
 *
 * The rendered image is written to argv[1]+".rgb" (SGI RGB format).
 * Returns 0 on pass, 1 on fail.
 *
 * Generating the control PNG:
 *   cmake --build <build> --target generate_rendering_controls
 * or:
 *   build/tests/bin/rgb_to_png  render_viewport_scene_control.rgb \
 *                               tests/control_images/render_viewport_scene_control.png
 */

#include "headless_utils.h"
#include <Inventor/SoViewport.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbVec2s.h>
#include <cstdio>

static const int W = 256;
static const int H = 256;

// ---------------------------------------------------------------------------
// Scene: white directional light + green sphere (no camera node in scene).
// ---------------------------------------------------------------------------
static SoSeparator * buildScene()
{
    SoSeparator * root = new SoSeparator;
    root->ref();

    SoDirectionalLight * lt = new SoDirectionalLight;
    lt->direction.setValue(-1.0f, -1.0f, -1.0f);
    root->addChild(lt);

    SoMaterial * mat = new SoMaterial;
    mat->diffuseColor.setValue(0.1f, 0.8f, 0.2f);   // green
    root->addChild(mat);

    root->addChild(new SoSphere);
    return root;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool validateNonBlank(const unsigned char * buf, int npix,
                              const char * label, int threshold = 200)
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

    const char * basepath = (argc > 1) ? argv[1] : "render_viewport_scene";
    char outpath[1024];
    snprintf(outpath, sizeof(outpath), "%s.rgb", basepath);

    int failures = 0;
    printf("\n=== SoViewport scene render test ===\n");

    SoSeparator * geom = buildScene();

    // ---- Set up SoViewport ------------------------------------------------
    SoViewport vp;
    vp.setWindowSize(SbVec2s((short)W, (short)H));
    vp.setSceneGraph(geom);
    vp.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));  // black background

    SoPerspectiveCamera * cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->pointAt(SbVec3f(0.0f, 0.0f, 0.0f));
    vp.setCamera(cam);   // SoViewport refs the camera; its destructor unrefs it
    vp.viewAll();   // frame the scene precisely

    // ---- Render via SoViewport::render() -----------------------------------
    SoOffscreenRenderer * renderer =
        new SoOffscreenRenderer(getCoinHeadlessContextManager(),
                                vp.getViewportRegion());
    renderer->setComponents(SoOffscreenRenderer::RGB);

    SbBool rendered = vp.render(renderer);
    if (!rendered) {
        printf("  render() FAILED\n");
        printf("FAIL render\n");
        ++failures;
    } else {
        const unsigned char * buf = renderer->getBuffer();
        const int npix = W * H;

        // Non-blank check
        bool nonblank = validateNonBlank(buf, npix, "viewport_scene");
        if (!nonblank) { printf("FAIL non-blank check\n"); ++failures; }

        // Green should be the dominant channel
        int dom = dominantChannel(buf, npix);
        printf("  dominant channel: %d (0=R 1=G 2=B) — expect 1 (G)\n", dom);
        if (dom != 1) { printf("FAIL dominant channel not green\n"); ++failures; }

        // Write output file (used as control image source)
        renderer->writeToRGB(outpath);
        printf("  wrote %s\n", outpath);
    }

    delete renderer;
    geom->unref();

    printf("\n=== Summary: %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
