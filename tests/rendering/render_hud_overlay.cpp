/*
 * render_hud_overlay.cpp - Visual regression test: HUD overlay over a 3D scene.
 *
 * Renders a lit blue sphere (3D) with a HUD overlay consisting of:
 *   - A status bar at the bottom: "Scene: Sphere  |  Radius: 0.50  |  Color: Blue"
 *   - A "Controls" header label near the top-left
 *   - Five side-menu buttons on the left: Larger, Smaller, Red, Blue, Green
 *
 * This test verifies that:
 *   1. HUD elements composite correctly on top of 3D scene geometry.
 *   2. Screen-space labels are drawn at the right pixel positions.
 *   3. Button borders (2-D line geometry) are visible.
 *   4. Depth buffer is correctly disabled for HUD elements so they always
 *      appear in front of the sphere.
 *
 * Viewport: 800 x 600
 * Background: near-black dark blue (0.08, 0.08, 0.12)
 * Output: argv[1]+".rgb"  (SGI RGB, used by image comparison infrastructure)
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/annex/HUD/nodekits/SoHUDKit.h>
#include <Inventor/annex/HUD/nodes/SoHUDLabel.h>
#include <Inventor/annex/HUD/nodes/SoHUDButton.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>

static const int W = 800;
static const int H = 600;

// Add a static HUD button (no callback needed for a visual regression test).
static void addMenuButton(SoHUDKit *hud,
                          const char *label,
                          float y,
                          float r, float g, float b)
{
    SoHUDButton *btn = new SoHUDButton;
    btn->position.setValue(10.0f, y);
    btn->size.setValue(100.0f, 28.0f);
    btn->string.setValue(label);
    btn->color.setValue(SbColor(r, g, b));
    btn->borderColor.setValue(SbColor(r * 0.65f, g * 0.65f, b * 0.65f));
    btn->fontSize.setValue(11.0f);
    hud->addWidget(btn);
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = new SoSeparator;
    root->ref();

    // ---- 3-D Scene ----

    // Perspective camera: looking down -Z, 45-degree FOV.
    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 3.0f);
    cam->orientation.setValue(SbVec3f(0.0f, 1.0f, 0.0f), 0.0f);
    cam->heightAngle.setValue(0.7854f);   // 45 degrees in radians
    cam->nearDistance.setValue(0.1f);
    cam->farDistance.setValue(100.0f);
    root->addChild(cam);

    // Single directional light from upper-right.
    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.5f, -1.0f, -0.8f);
    light->intensity.setValue(1.0f);
    root->addChild(light);

    // Blue sphere at the origin, radius 0.5.
    {
        SoSeparator *sep = new SoSeparator;
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.15f, 0.35f, 0.85f);
        mat->specularColor.setValue(0.4f, 0.4f, 0.7f);
        mat->shininess.setValue(0.6f);
        sep->addChild(mat);
        SoSphere *sphere = new SoSphere;
        sphere->radius.setValue(0.5f);
        sep->addChild(sphere);
        root->addChild(sep);
    }

    // ---- HUD Overlay ----
    SoHUDKit *hud = new SoHUDKit;

    // Status bar at the bottom of the viewport.
    SoHUDLabel *statusLabel = new SoHUDLabel;
    statusLabel->position.setValue(5.0f, 8.0f);
    statusLabel->string.set1Value(0, "Scene: Sphere  |  Radius: 0.50  |  Color: Blue");
    statusLabel->color.setValue(SbColor(0.9f, 0.85f, 0.3f));
    statusLabel->fontSize.setValue(12.0f);
    statusLabel->justification.setValue(SoHUDLabel::LEFT);
    hud->addWidget(statusLabel);

    // "Controls" header label near the top-left.
    SoHUDLabel *headerLabel = new SoHUDLabel;
    headerLabel->position.setValue(10.0f, 568.0f);
    headerLabel->string.set1Value(0, "Controls");
    headerLabel->color.setValue(SbColor(0.9f, 0.9f, 0.9f));
    headerLabel->fontSize.setValue(12.0f);
    headerLabel->justification.setValue(SoHUDLabel::LEFT);
    hud->addWidget(headerLabel);

    // Left-side menu: five action buttons stacked vertically.
    addMenuButton(hud, "Larger",  530.0f, 0.55f, 0.88f, 0.55f);
    addMenuButton(hud, "Smaller", 495.0f, 0.88f, 0.88f, 0.55f);
    addMenuButton(hud, "Red",     460.0f, 1.00f, 0.35f, 0.35f);
    addMenuButton(hud, "Blue",    425.0f, 0.35f, 0.55f, 1.00f);
    addMenuButton(hud, "Green",   390.0f, 0.35f, 0.88f, 0.35f);

    root->addChild(hud);

    // ---- Render ----
    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_hud_overlay.rgb");

    bool ok = renderToFile(root, outpath, W, H, SbColor(0.08f, 0.08f, 0.12f));
    root->unref();
    return ok ? 0 : 1;
}
