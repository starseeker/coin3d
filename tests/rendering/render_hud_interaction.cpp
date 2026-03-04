/*
 * render_hud_interaction.cpp - HUD interaction test: buttons modify 3D scene.
 *
 * This validation test verifies the full interaction pipeline:
 *
 *   1. Field-level: after simulated mouse presses on the "Larger"/"Smaller"
 *      buttons, SoSphere::radius must change by exactly the expected amount.
 *
 *   2. Field-level: after pressing a colour button, SoMaterial::diffuseColor
 *      must reflect the new colour.
 *
 *   3. Pixel-level: rendering before vs. after "Larger" clicks must show
 *      more sphere coverage (the sphere appears bigger).
 *
 *   4. Pixel-level: after the "Red" button the dominant colour channel in
 *      the sphere region must be red (channel 0).
 *
 * Scene layout (800x600):
 *   3D: perspective camera, directional light, blue sphere (radius 0.5)
 *   HUD: status bar (bottom) + five left-side buttons:
 *     - Larger  at y=530  → increments radius by 0.20
 *     - Smaller at y=495  → decrements radius by 0.20 (min 0.10)
 *     - Red     at y=460  → diffuse = (0.90, 0.15, 0.15)
 *     - Blue    at y=425  → diffuse = (0.15, 0.35, 0.90)
 *     - Green   at y=390  → diffuse = (0.20, 0.85, 0.20)
 *
 * Button coordinates (lower-left corner in viewport pixels):
 *   x=10, width=100, height=28 → click centre at (60, y+14)
 *
 * The test writes the post-interaction frame to argv[1]+".rgb" so the
 * image comparison infrastructure can optionally compare it.
 *
 * Exit code: 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/annex/HUD/nodekits/SoHUDKit.h>
#include <Inventor/annex/HUD/nodes/SoHUDLabel.h>
#include <Inventor/annex/HUD/nodes/SoHUDButton.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>
#include <cmath>
#include <cstring>

static const int W = 800;
static const int H = 600;

// Button geometry constants.
static const float BTN_X       = 10.0f;
static const float BTN_W       = 100.0f;
static const float BTN_H       = 28.0f;
static const float BTN_LARGER_Y  = 530.0f;
static const float BTN_SMALLER_Y = 495.0f;
static const float BTN_RED_Y     = 460.0f;
static const float BTN_BLUE_Y    = 425.0f;
static const float BTN_GREEN_Y   = 390.0f;

// Centre x/y of a button for click simulation.
static inline int btnClickX()        { return (int)(BTN_X + BTN_W * 0.5f); }
static inline int btnClickY(float y) { return (int)(y + BTN_H * 0.5f); }

// ---- Scene state shared with callbacks ----
struct SceneState {
    SoSphere   *sphere;
    SoMaterial *material;
    int largerClicks;
    int smallerClicks;
    int colorClicks;
};

static void onLarger(void *ud, SoHUDButton * /*b*/)
{
    SceneState *s = static_cast<SceneState *>(ud);
    float r = s->sphere->radius.getValue();
    s->sphere->radius.setValue(r + 0.2f);
    ++s->largerClicks;
    printf("  [Larger]  radius %.2f -> %.2f\n", r, r + 0.2f);
}

static void onSmaller(void *ud, SoHUDButton * /*b*/)
{
    SceneState *s = static_cast<SceneState *>(ud);
    float r = s->sphere->radius.getValue();
    float nr = (r - 0.2f > 0.1f) ? r - 0.2f : 0.1f;
    s->sphere->radius.setValue(nr);
    ++s->smallerClicks;
    printf("  [Smaller] radius %.2f -> %.2f\n", r, nr);
}

static void onRed(void *ud, SoHUDButton * /*b*/)
{
    SceneState *s = static_cast<SceneState *>(ud);
    s->material->diffuseColor.setValue(0.9f, 0.15f, 0.15f);
    ++s->colorClicks;
    printf("  [Red]     color -> red\n");
}

static void onBlue(void *ud, SoHUDButton * /*b*/)
{
    SceneState *s = static_cast<SceneState *>(ud);
    s->material->diffuseColor.setValue(0.15f, 0.35f, 0.9f);
    ++s->colorClicks;
    printf("  [Blue]    color -> blue\n");
}

static void onGreen(void *ud, SoHUDButton * /*b*/)
{
    SceneState *s = static_cast<SceneState *>(ud);
    s->material->diffuseColor.setValue(0.2f, 0.85f, 0.2f);
    ++s->colorClicks;
    printf("  [Green]   color -> green\n");
}

// ---- Pixel analysis helpers ----

// Count pixels brighter than a low threshold in a circular region.
// Buffer is W*H*3 RGB, stored row-major from row 0 (Coin's offscreen buffer).
static int countBrightPixels(const unsigned char *buf, int cx, int cy, int radius)
{
    int count = 0;
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            int px = cx + dx, py = cy + dy;
            if (px < 0 || px >= W || py < 0 || py >= H) continue;
            const unsigned char *p = buf + (py * W + px) * 3;
            if ((int)p[0] + (int)p[1] + (int)p[2] > 40) ++count;
        }
    }
    return count;
}

// Return the index (0=R, 1=G, 2=B) of the dominant colour channel
// in the pixels within a circular region that are above background.
static int dominantChannel(const unsigned char *buf, int cx, int cy, int radius)
{
    long totR = 0, totG = 0, totB = 0;
    int n = 0;
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            int px = cx + dx, py = cy + dy;
            if (px < 0 || px >= W || py < 0 || py >= H) continue;
            const unsigned char *p = buf + (py * W + px) * 3;
            if ((int)p[0] + (int)p[1] + (int)p[2] < 20) continue; // skip background
            totR += p[0]; totG += p[1]; totB += p[2]; ++n;
        }
    }
    if (n == 0) return -1;
    if (totR >= totG && totR >= totB) return 0;
    if (totG >= totR && totG >= totB) return 1;
    return 2;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    /* Render the canonical factory scene as the primary output image.
     * This ensures obol_viewer and obol_render produce identical scenes. */
    {
        const char *primaryBase = (argc > 1) ? argv[1] : "render_hud_interaction";
        SoSeparator *fRoot = ObolTest::Scenes::createHUDInteraction(256, 256);
        SbViewportRegion fVp(256, 256);
        SoOffscreenRenderer fRen(fVp);
        fRen.setComponents(SoOffscreenRenderer::RGB);
        fRen.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
        if (fRen.render(fRoot)) {
            char primaryPath[4096];
            snprintf(primaryPath, sizeof(primaryPath), "%s.rgb", primaryBase);
            fRen.writeToRGB(primaryPath);
        }
        fRoot->unref();
    }


    // ---- Build scene ----
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 3.5f);
    cam->orientation.setValue(SbVec3f(0.0f, 1.0f, 0.0f), 0.0f);
    cam->heightAngle.setValue(0.7854f); // 45 degrees
    cam->nearDistance.setValue(0.1f);
    cam->farDistance.setValue(100.0f);
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.5f, -1.0f, -0.8f);
    root->addChild(light);

    // Blue sphere (initial state).
    SoMaterial *sphereMat = new SoMaterial;
    sphereMat->diffuseColor.setValue(0.15f, 0.35f, 0.85f);
    sphereMat->specularColor.setValue(0.4f, 0.4f, 0.7f);
    sphereMat->shininess.setValue(0.5f);

    SoSphere *sphere = new SoSphere;
    sphere->radius.setValue(0.5f);

    {
        SoSeparator *sep = new SoSeparator;
        sep->addChild(sphereMat);
        sep->addChild(sphere);
        root->addChild(sep);
    }

    // ---- Scene state ----
    SceneState state;
    state.sphere       = sphere;
    state.material     = sphereMat;
    state.largerClicks  = 0;
    state.smallerClicks = 0;
    state.colorClicks   = 0;

    // ---- HUD Overlay ----
    SoHUDKit *hud = new SoHUDKit;

    // Status label at bottom (not updated live in this test, just for visuals).
    SoHUDLabel *statusLabel = new SoHUDLabel;
    statusLabel->position.setValue(5.0f, 8.0f);
    statusLabel->string.set1Value(0, "Scene: Sphere  |  Radius: 0.50  |  Color: Blue");
    statusLabel->color.setValue(SbColor(0.9f, 0.85f, 0.3f));
    statusLabel->fontSize.setValue(12.0f);
    hud->addWidget(statusLabel);

    // Controls header.
    SoHUDLabel *ctrlHeader = new SoHUDLabel;
    ctrlHeader->position.setValue(10.0f, 568.0f);
    ctrlHeader->string.set1Value(0, "Controls");
    ctrlHeader->color.setValue(SbColor(0.85f, 0.85f, 0.85f));
    ctrlHeader->fontSize.setValue(12.0f);
    hud->addWidget(ctrlHeader);

    // Side-menu buttons.
    struct BtnSpec {
        const char *label;
        float       y;
        void (*cb)(void *, SoHUDButton *);
        float r, g, b;
    };
    static const BtnSpec specs[] = {
        { "Larger",  BTN_LARGER_Y,  onLarger,  0.55f, 0.88f, 0.55f },
        { "Smaller", BTN_SMALLER_Y, onSmaller, 0.88f, 0.88f, 0.55f },
        { "Red",     BTN_RED_Y,     onRed,     1.00f, 0.35f, 0.35f },
        { "Blue",    BTN_BLUE_Y,    onBlue,    0.35f, 0.55f, 1.00f },
        { "Green",   BTN_GREEN_Y,   onGreen,   0.35f, 0.88f, 0.35f },
    };
    const int nSpecs = (int)(sizeof(specs) / sizeof(specs[0]));
    for (int i = 0; i < nSpecs; ++i) {
        SoHUDButton *btn = new SoHUDButton;
        btn->position.setValue(BTN_X, specs[i].y);
        btn->size.setValue(BTN_W, BTN_H);
        btn->string.setValue(specs[i].label);
        btn->color.setValue(SbColor(specs[i].r, specs[i].g, specs[i].b));
        btn->borderColor.setValue(SbColor(
            specs[i].r * 0.65f, specs[i].g * 0.65f, specs[i].b * 0.65f));
        btn->fontSize.setValue(11.0f);
        btn->addClickCallback(specs[i].cb, &state);
        hud->addWidget(btn);
    }

    root->addChild(hud);

    // ---- Shared viewport for all renders and event dispatch ----
    SbViewportRegion vp(W, H);

    // ---- Initial render ----
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.08f, 0.08f, 0.12f));

    printf("render_hud_interaction: rendering initial frame...\n");
    if (!renderer.render(root)) {
        fprintf(stderr, "render_hud_interaction: FAIL - initial render failed\n");
        root->unref();
        return 1;
    }

    // Record pixel statistics BEFORE interactions (copy counts from the buffer).
    const unsigned char *buf0 = renderer.getBuffer();
    const int cx = W / 2, cy = H / 2;
    const int initBrightCount  = countBrightPixels(buf0, cx, cy, 150);
    const int initDominantChan = dominantChannel(buf0, cx, cy, 80);
    const float initRadius     = sphere->radius.getValue();

    printf("  Initial: radius=%.2f  spherePixels=%d  dominantChan=%d (0=R,1=G,2=B)\n",
           initRadius, initBrightCount, initDominantChan);

    // ---- Test 1: Larger button (3 clicks) ----
    printf("\nTest 1: clicking 'Larger' 3 times\n");
    for (int i = 0; i < 3; ++i)
        simulateMousePress(root, vp, btnClickX(), btnClickY(BTN_LARGER_Y));

    const float expectedR1 = initRadius + 3 * 0.2f;
    const float actualR1   = sphere->radius.getValue();
    printf("  Expected radius: %.2f  Got: %.2f\n", expectedR1, actualR1);
    if (fabsf(actualR1 - expectedR1) > 0.001f) {
        fprintf(stderr, "render_hud_interaction: FAIL - 'Larger' did not update radius\n");
        root->unref();
        return 1;
    }
    printf("  PASS: 'Larger' updated sphere radius correctly\n");

    // ---- Test 2: Smaller button (2 clicks) ----
    printf("\nTest 2: clicking 'Smaller' 2 times\n");
    for (int i = 0; i < 2; ++i)
        simulateMousePress(root, vp, btnClickX(), btnClickY(BTN_SMALLER_Y));

    const float expectedR2 = actualR1 - 2 * 0.2f;
    const float actualR2   = sphere->radius.getValue();
    printf("  Expected radius: %.2f  Got: %.2f\n", expectedR2, actualR2);
    if (fabsf(actualR2 - expectedR2) > 0.001f) {
        fprintf(stderr, "render_hud_interaction: FAIL - 'Smaller' did not update radius\n");
        root->unref();
        return 1;
    }
    printf("  PASS: 'Smaller' updated sphere radius correctly\n");

    // ---- Test 3: Red colour button ----
    printf("\nTest 3: clicking 'Red' button\n");
    simulateMousePress(root, vp, btnClickX(), btnClickY(BTN_RED_Y));

    const SbColor matColor = sphereMat->diffuseColor[0];
    printf("  Material color: (%.2f, %.2f, %.2f)\n",
           matColor[0], matColor[1], matColor[2]);
    if (matColor[0] < 0.5f) {
        fprintf(stderr, "render_hud_interaction: FAIL - 'Red' did not set red color\n");
        root->unref();
        return 1;
    }
    printf("  PASS: 'Red' updated material color correctly\n");

    // ---- Test 4: Blue colour button ----
    printf("\nTest 4: clicking 'Blue' button\n");
    simulateMousePress(root, vp, btnClickX(), btnClickY(BTN_BLUE_Y));

    const SbColor blueColor = sphereMat->diffuseColor[0];
    printf("  Material color: (%.2f, %.2f, %.2f)\n",
           blueColor[0], blueColor[1], blueColor[2]);
    if (blueColor[2] < 0.5f) {
        fprintf(stderr, "render_hud_interaction: FAIL - 'Blue' did not set blue color\n");
        root->unref();
        return 1;
    }
    printf("  PASS: 'Blue' updated material color correctly\n");

    // ---- Test 5: Green colour button ----
    printf("\nTest 5: clicking 'Green' button\n");
    simulateMousePress(root, vp, btnClickX(), btnClickY(BTN_GREEN_Y));

    const SbColor greenColor = sphereMat->diffuseColor[0];
    printf("  Material color: (%.2f, %.2f, %.2f)\n",
           greenColor[0], greenColor[1], greenColor[2]);
    if (greenColor[1] < 0.5f) {
        fprintf(stderr, "render_hud_interaction: FAIL - 'Green' did not set green color\n");
        root->unref();
        return 1;
    }
    printf("  PASS: 'Green' updated material color correctly\n");

    // ---- Test 6: Go back to Red for pixel analysis ----
    simulateMousePress(root, vp, btnClickX(), btnClickY(BTN_RED_Y));

    // Now make sphere large (click Larger several more times so it clearly grows).
    // Total from start: 3 Larger - 2 Smaller = net +0.2, then from Test 4/5 changes.
    // Current radius = actualR2 = initRadius + 0.2 = 0.7.  Apply 3 more Larger.
    for (int i = 0; i < 3; ++i)
        simulateMousePress(root, vp, btnClickX(), btnClickY(BTN_LARGER_Y));

    printf("\nTest 6: pixel analysis after growing sphere and setting red\n");
    printf("  Current radius: %.2f\n", sphere->radius.getValue());

    if (!renderer.render(root)) {
        fprintf(stderr, "render_hud_interaction: FAIL - post-interaction render failed\n");
        root->unref();
        return 1;
    }

    const unsigned char *buf1 = renderer.getBuffer();
    const int afterBrightCount  = countBrightPixels(buf1, cx, cy, 150);
    const int afterDominantChan = dominantChannel(buf1, cx, cy, 80);

    printf("  After: spherePixels=%d  dominantChan=%d\n",
           afterBrightCount, afterDominantChan);

    // Sphere is now bigger → must cover more pixels in the search region.
    if (afterBrightCount <= initBrightCount) {
        fprintf(stderr,
                "render_hud_interaction: FAIL - sphere did not grow "
                "(pixels %d vs initial %d)\n",
                afterBrightCount, initBrightCount);
        root->unref();
        return 1;
    }
    printf("  PASS: sphere coverage grew (%d -> %d pixels)\n",
           initBrightCount, afterBrightCount);

    // Sphere is red → dominant channel must be R (0).
    if (afterDominantChan != 0) {
        fprintf(stderr,
                "render_hud_interaction: FAIL - expected dominant R channel, "
                "got channel %d\n", afterDominantChan);
        root->unref();
        return 1;
    }
    printf("  PASS: dominant colour channel is Red (channel 0)\n");

    // ---- Write final output ----
    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_hud_interaction.rgb");

    if (!renderer.writeToRGB(outpath)) {
        fprintf(stderr, "render_hud_interaction: WARNING - could not write output (non-fatal)\n");
    }

    printf("\nrender_hud_interaction: ALL TESTS PASSED\n");
    printf("  Larger clicks: %d  Smaller clicks: %d  Color clicks: %d\n",
           state.largerClicks, state.smallerClicks, state.colorClicks);
    printf("  Final sphere radius: %.2f\n", sphere->radius.getValue());

    root->unref();
    return 0;
}
