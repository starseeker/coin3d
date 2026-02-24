/*
 * render_hud_no3d.cpp - Visual regression test: HUD overlay without 3D geometry.
 *
 * Renders the HUD system in complete isolation: no 3D geometry is present,
 * only the HUD elements.  This tests the degenerate case relevant to:
 *   - Splash / loading screens
 *   - Pure 2-D menu overlays
 *   - Debug information panels shown before any scene is loaded
 *
 * HUD layout (800x600 viewport):
 *   - Title label (centered, top): "HUD Display Test"
 *   - Subtitle (centered): "No 3D geometry present"
 *   - Left-side "Main Menu" with 5 buttons (New Scene, Open, Save, Settings, Exit)
 *   - Right-side info panel with multi-line label (Scene, Objects, Status)
 *   - Bottom status bar: "Ready  |  No scene loaded  |  HUD Only Mode"
 *
 * A perspective camera is still added to ensure proper OpenGL state
 * initialisation (the HUD uses its own orthographic camera internally).
 *
 * Viewport: 800 x 600
 * Background: very dark blue (0.05, 0.05, 0.10)
 * Output: argv[1]+".rgb"
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/annex/HUD/nodekits/SoHUDKit.h>
#include <Inventor/annex/HUD/nodes/SoHUDLabel.h>
#include <Inventor/annex/HUD/nodes/SoHUDButton.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>

static const int W = 800;
static const int H = 600;

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = new SoSeparator;
    root->ref();

    // Camera: needed for proper GL state even with no geometry.
    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->nearDistance.setValue(0.1f);
    cam->farDistance.setValue(100.0f);
    root->addChild(cam);

    // Minimal light (no geometry to light, but avoids empty light stack warnings).
    root->addChild(new SoDirectionalLight);

    // ---- HUD Overlay ----
    SoHUDKit *hud = new SoHUDKit;

    // Title: centered at the top.
    SoHUDLabel *titleLabel = new SoHUDLabel;
    titleLabel->position.setValue(400.0f, 560.0f);
    titleLabel->string.set1Value(0, "HUD Display Test");
    titleLabel->color.setValue(SbColor(1.0f, 0.85f, 0.2f));
    titleLabel->fontSize.setValue(16.0f);
    titleLabel->justification.setValue(SoHUDLabel::CENTER);
    hud->addWidget(titleLabel);

    // Subtitle.
    SoHUDLabel *subtitleLabel = new SoHUDLabel;
    subtitleLabel->position.setValue(400.0f, 532.0f);
    subtitleLabel->string.set1Value(0, "No 3D geometry present");
    subtitleLabel->color.setValue(SbColor(0.7f, 0.7f, 0.7f));
    subtitleLabel->fontSize.setValue(12.0f);
    subtitleLabel->justification.setValue(SoHUDLabel::CENTER);
    hud->addWidget(subtitleLabel);

    // Left-panel header.
    SoHUDLabel *leftHeader = new SoHUDLabel;
    leftHeader->position.setValue(10.0f, 490.0f);
    leftHeader->string.set1Value(0, "Main Menu");
    leftHeader->color.setValue(SbColor(0.9f, 0.9f, 0.9f));
    leftHeader->fontSize.setValue(13.0f);
    leftHeader->justification.setValue(SoHUDLabel::LEFT);
    hud->addWidget(leftHeader);

    // Menu buttons.
    struct MenuEntry { const char *label; float r, g, b; };
    static const MenuEntry entries[] = {
        { "New Scene", 0.50f, 0.80f, 1.00f },
        { "Open...",   0.60f, 0.90f, 0.60f },
        { "Save",      0.90f, 0.85f, 0.40f },
        { "Settings",  0.80f, 0.60f, 0.90f },
        { "Exit",      1.00f, 0.40f, 0.40f },
    };
    const int nEntries = (int)(sizeof(entries) / sizeof(entries[0]));
    const float btnX = 10.0f, btnW = 120.0f, btnH = 30.0f, btnStep = 40.0f;
    float btnY = 450.0f;
    for (int i = 0; i < nEntries; ++i) {
        SoHUDButton *btn = new SoHUDButton;
        btn->position.setValue(btnX, btnY);
        btn->size.setValue(btnW, btnH);
        btn->string.setValue(entries[i].label);
        btn->color.setValue(SbColor(entries[i].r, entries[i].g, entries[i].b));
        btn->borderColor.setValue(SbColor(
            entries[i].r * 0.65f,
            entries[i].g * 0.65f,
            entries[i].b * 0.65f));
        btn->fontSize.setValue(11.0f);
        hud->addWidget(btn);
        btnY -= btnStep;
    }

    // Right-panel info header.
    SoHUDLabel *infoHeader = new SoHUDLabel;
    infoHeader->position.setValue(650.0f, 490.0f);
    infoHeader->string.set1Value(0, "Info Panel");
    infoHeader->color.setValue(SbColor(0.9f, 0.9f, 0.9f));
    infoHeader->fontSize.setValue(12.0f);
    infoHeader->justification.setValue(SoHUDLabel::LEFT);
    hud->addWidget(infoHeader);

    // Right-panel multi-line content.
    SoHUDLabel *infoContent = new SoHUDLabel;
    infoContent->position.setValue(650.0f, 460.0f);
    infoContent->string.set1Value(0, "Scene:   Empty");
    infoContent->string.set1Value(1, "Objects: 0");
    infoContent->string.set1Value(2, "Status:  Idle");
    infoContent->color.setValue(SbColor(0.7f, 0.7f, 0.8f));
    infoContent->fontSize.setValue(11.0f);
    infoContent->justification.setValue(SoHUDLabel::LEFT);
    hud->addWidget(infoContent);

    // Bottom status bar.
    SoHUDLabel *statusBar = new SoHUDLabel;
    statusBar->position.setValue(5.0f, 8.0f);
    statusBar->string.set1Value(0, "Ready  |  No scene loaded  |  HUD Only Mode");
    statusBar->color.setValue(SbColor(0.85f, 0.8f, 0.2f));
    statusBar->fontSize.setValue(12.0f);
    statusBar->justification.setValue(SoHUDLabel::LEFT);
    hud->addWidget(statusBar);

    root->addChild(hud);

    // ---- Render ----
    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_hud_no3d.rgb");

    bool ok = renderToFile(root, outpath, W, H, SbColor(0.05f, 0.05f, 0.10f));
    root->unref();
    return ok ? 0 : 1;
}
