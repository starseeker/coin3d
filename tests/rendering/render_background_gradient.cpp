/*
 * render_background_gradient.cpp
 *
 * Tests the gradient background feature of SoOffscreenRenderer on both the
 * GL and NanoRT rendering backends.
 *
 * A simple scene (four coloured primitives in a 2×2 grid) is rendered twice:
 *   1. With a solid background (the existing default behaviour).
 *   2. With a vertical gradient background (dark navy → steel blue).
 *
 * Both renders write SGI RGB files.  The test passes when:
 *   - Both renders succeed.
 *   - The gradient image's bottom row is significantly darker/bluer than its
 *     top row, confirming that the gradient was actually applied.
 *
 * This test is backend-agnostic: it uses headless_utils.h so it works under
 * GLX/OSMesa (system GL) or, when compiled with COIN3D_NANORT_BUILD, the
 * SoNanoRTContextManager raytracing path.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <cstdio>
#include <cstring>
#include <cmath>

// ---------------------------------------------------------------------------
// Scene construction (same 2×2 primitive grid used by render_primitives)
// ---------------------------------------------------------------------------

static void addPrimitive(SoSeparator *root,
                         float r, float g, float b,
                         float tx, float ty,
                         SoNode *shape)
{
    SoSeparator *sep = new SoSeparator;

    SoTranslation *t = new SoTranslation;
    t->translation.setValue(tx, ty, 0.0f);
    sep->addChild(t);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(r, g, b);
    mat->specularColor.setValue(0.6f, 0.6f, 0.6f);
    mat->shininess.setValue(0.5f);
    sep->addChild(mat);

    sep->addChild(shape);
    root->addChild(sep);
}

static SoSeparator *buildScene()
{
    SoSeparator *root = new SoSeparator;

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.5f, -0.8f, -0.6f);
    root->addChild(light);

    const float s = 2.5f;
    addPrimitive(root, 0.85f, 0.15f, 0.15f, -s*0.5f,  s*0.5f, new SoSphere);
    addPrimitive(root, 0.15f, 0.75f, 0.15f,  s*0.5f,  s*0.5f, new SoCube);
    addPrimitive(root, 0.15f, 0.35f, 0.90f, -s*0.5f, -s*0.5f, new SoCone);
    addPrimitive(root, 0.90f, 0.75f, 0.15f,  s*0.5f, -s*0.5f, new SoCylinder);

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    cam->viewAll(root, vp);
    cam->position.setValue(cam->position.getValue() * 1.1f);

    return root;
}

// ---------------------------------------------------------------------------
// Validate gradient: the average blue channel of the top quarter of the
// image should be noticeably higher than the bottom quarter.
// ---------------------------------------------------------------------------

static bool validateGradient(const unsigned char *buf, int w, int h, int ncomp)
{
    // Accumulate blue channel for top 25% and bottom 25% rows.
    // getBuffer() row 0 = screen-bottom, row h-1 = screen-top.
    const int quarterH = h / 4;
    if (quarterH == 0) return true;  // can't validate a tiny image

    double sumTop = 0.0, sumBot = 0.0;
    int countTop = 0, countBot = 0;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const unsigned char blue = buf[(y * w + x) * ncomp + 2];
            if (y < quarterH) {
                sumBot += blue; ++countBot;      // low rows = screen-bottom = dark navy
            } else if (y >= h - quarterH) {
                sumTop += blue; ++countTop;      // high rows = screen-top = steel blue
            }
        }
    }

    if (countTop == 0 || countBot == 0) return true;

    const double avgTop = sumTop / countTop;
    const double avgBot = sumBot / countBot;

    printf("  Gradient check: avg-blue top=%.1f bottom=%.1f\n", avgTop, avgBot);

    // The top of the gradient (steel blue) should have a noticeably higher
    // blue component than the bottom (dark navy).  Allow a margin of 20 units
    // to accommodate slight aliasing from geometry near the border.
    if (avgTop <= avgBot + 20.0) {
        fprintf(stderr,
            "ERROR: gradient check failed – top blue (%.1f) not significantly "
            "greater than bottom blue (%.1f)\n", avgTop, avgBot);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = buildScene();
    root->ref();

    // --- Base output path (no extension) from argv[1] ----------------------
    char solidPath[1024], gradPath[1024];
    if (argc > 1) {
        snprintf(solidPath, sizeof(solidPath), "%s.rgb",          argv[1]);
        snprintf(gradPath,  sizeof(gradPath),  "%s_gradient.rgb", argv[1]);
    } else {
        snprintf(solidPath, sizeof(solidPath), "render_background_gradient.rgb");
        snprintf(gradPath,  sizeof(gradPath),  "render_background_gradient_gradient.rgb");
    }

    // --- 1. Solid background render (sanity baseline) ----------------------
    printf("--- Solid background render ---\n");
    bool ok = renderToFile(root, solidPath,
                           DEFAULT_WIDTH, DEFAULT_HEIGHT,
                           SbColor(0.1f, 0.1f, 0.1f));
    if (!ok) {
        fprintf(stderr, "ERROR: solid background render failed\n");
        root->unref();
        return 1;
    }
    printf("Solid render OK → %s\n", solidPath);

    // --- 2. Gradient background render -------------------------------------
    // dark navy (screen-bottom) → steel blue (screen-top)
    const SbColor gradBottom(0.05f, 0.05f, 0.25f);
    const SbColor gradTop   (0.40f, 0.60f, 0.85f);

    printf("--- Gradient background render ---\n");
    SoOffscreenRenderer *renderer = getSharedRenderer();
    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    renderer->setViewportRegion(vp);
    renderer->setComponents(SoOffscreenRenderer::RGB);
    renderer->setBackgroundColor(SbColor(0.05f, 0.05f, 0.25f));  // solid fallback
    renderer->setBackgroundGradient(gradBottom, gradTop);

    if (!renderer->render(root)) {
        fprintf(stderr, "ERROR: gradient render() call failed\n");
        root->unref();
        return 1;
    }

    if (!renderer->writeToRGB(gradPath)) {
        fprintf(stderr, "ERROR: writeToRGB failed for gradient output\n");
        root->unref();
        return 1;
    }
    printf("Gradient render OK → %s\n", gradPath);

    // Validate that the gradient was actually applied to the image buffer.
    const unsigned char *buf   = renderer->getBuffer();
    const SbVec2s        vpSz  = vp.getViewportSizePixels();
    const int            ncomp = (int)renderer->getComponents();

    if (!validateGradient(buf, vpSz[0], vpSz[1], ncomp)) {
        root->unref();
        return 1;
    }

    // Clear gradient so subsequent renders in other tests are unaffected.
    renderer->clearBackgroundGradient();

    root->unref();
    printf("render_background_gradient: PASSED\n");
    return 0;
}
