/*
 * render_switch_visibility.cpp - Integration test: SoSwitch visibility control
 *
 * Creates a scene with two coloured spheres (red and blue) separated
 * horizontally, each wrapped in its own SoSwitch.  The test renders three
 * frames with different switch settings and validates the pixel content:
 *
 *   Frame 1: both switches ON  → both spheres visible (red left, blue right)
 *   Frame 2: red switch OFF    → only blue sphere visible (left side black)
 *   Frame 3: blue switch OFF   → only red sphere visible (right side black)
 *
 * Verifies that SoSwitch::whichChild correctly controls geometry visibility
 * and that SO_SWITCH_NONE hides a subtree completely.
 *
 * Writes argv[1]+".rgb" (frame 1 image) and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoSwitch.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>
#include <cmath>

static const int W = 256;
static const int H = 256;
static const float SPH_R  = 0.3f;
static const float SPH_X  = 0.5f;   // offset from centre
static const float CAM_H  = 2.0f;

// Predicted left-sphere centre pixel (red sphere, offset to -SPH_X)
static const int LEFT_PX  = (int)(W * (0.5f - SPH_X / CAM_H));
// Predicted right-sphere centre pixel (blue sphere, offset to +SPH_X)
static const int RIGHT_PX = (int)(W * (0.5f + SPH_X / CAM_H));
static const int TOL      = (int)(SPH_R / CAM_H * W) + 8;

// Return true if pixel (x,y) matches colour c within tolerance t (per channel)
static bool isColour(const unsigned char *buf, int x, int y,
                     unsigned char r, unsigned char g, unsigned char b,
                     int t = 60)
{
    if (x < 0 || x >= W || y < 0 || y >= H) return false;
    const unsigned char *p = buf + (y * W + x) * 3;
    return (std::abs((int)p[0] - (int)r) <= t &&
            std::abs((int)p[1] - (int)g) <= t &&
            std::abs((int)p[2] - (int)b) <= t);
}

static bool hasSphereAt(const unsigned char *buf, int cx,
                        unsigned char r, unsigned char g, unsigned char b)
{
    int cy = H / 2;   // spheres are centred vertically
    int hits = 0;
    for (int dx = -TOL; dx <= TOL; dx += 4) {
        int x = cx + dx;
        for (int dy = -TOL; dy <= TOL; dy += 4) {
            int y = cy + dy;
            if (isColour(buf, x, y, r, g, b)) ++hits;
        }
    }
    return hits >= 3;
}

static bool isBlankAt(const unsigned char *buf, int cx)
{
    int cy = H / 2;
    int blank = 0, total = 0;
    for (int dx = -TOL; dx <= TOL; dx += 4) {
        int x = cx + dx;
        for (int dy = -TOL; dy <= TOL; dy += 4) {
            int y = cy + dy;
            if (x < 0 || x >= W || y < 0 || y >= H) continue;
            const unsigned char *p = buf + (y * W + x) * 3;
            if (p[0] < 30 && p[1] < 30 && p[2] < 30) ++blank;
            ++total;
        }
    }
    return total > 0 && blank > total * 3 / 4;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = new SoSeparator;
    root->ref();

    // Camera: orthographic, top view
    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position    .setValue(0, 0, 3);
    cam->nearDistance = 1.0f;
    cam->farDistance  = 10.0f;
    cam->height       = CAM_H;
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(0, 0, -1);
    root->addChild(light);

    // --- Red sphere (left) wrapped in a switch ---
    SoSwitch *redSwitch = new SoSwitch;
    {
        SoSeparator *sep = new SoSeparator;
        SoTranslation *t = new SoTranslation;
        t->translation.setValue(-SPH_X, 0, 0);
        sep->addChild(t);
        SoMaterial *mat = new SoMaterial;
        mat->emissiveColor.setValue(1.0f, 0.0f, 0.0f);
        mat->diffuseColor .setValue(0.0f, 0.0f, 0.0f);
        sep->addChild(mat);
        SoSphere *sph = new SoSphere;
        sph->radius = SPH_R;
        sep->addChild(sph);
        redSwitch->addChild(sep);
    }
    root->addChild(redSwitch);

    // --- Blue sphere (right) wrapped in a switch ---
    SoSwitch *blueSwitch = new SoSwitch;
    {
        SoSeparator *sep = new SoSeparator;
        SoTranslation *t = new SoTranslation;
        t->translation.setValue(SPH_X, 0, 0);
        sep->addChild(t);
        SoMaterial *mat = new SoMaterial;
        mat->emissiveColor.setValue(0.0f, 0.0f, 1.0f);
        mat->diffuseColor .setValue(0.0f, 0.0f, 0.0f);
        sep->addChild(mat);
        SoSphere *sph = new SoSphere;
        sph->radius = SPH_R;
        sep->addChild(sph);
        blueSwitch->addChild(sep);
    }
    root->addChild(blueSwitch);

    SbViewportRegion vp(W, H);
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    bool allOk = true;

    // ---- Frame 1: both switches ON ----
    redSwitch ->whichChild.setValue(SO_SWITCH_ALL);
    blueSwitch->whichChild.setValue(SO_SWITCH_ALL);
    if (!renderer.render(root)) {
        fprintf(stderr, "render_switch_visibility: frame1 render() failed\n");
        allOk = false;
    } else {
        const unsigned char *buf = renderer.getBuffer();
        bool redOk  = buf && hasSphereAt(buf, LEFT_PX,  255, 0, 0);
        bool blueOk = buf && hasSphereAt(buf, RIGHT_PX, 0, 0, 255);
        printf("Frame1 (both ON): red=%s blue=%s\n",
               redOk ? "FOUND" : "MISSING", blueOk ? "FOUND" : "MISSING");
        if (!redOk || !blueOk) {
            fprintf(stderr, "render_switch_visibility: FAIL frame1\n");
            allOk = false;
        }
    }

    // Save frame 1 as the RGB output
    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_switch_visibility.rgb");
    renderer.writeToRGB(outpath);

    // ---- Frame 2: red switch OFF ----
    redSwitch ->whichChild.setValue(SO_SWITCH_NONE);
    blueSwitch->whichChild.setValue(SO_SWITCH_ALL);
    if (!renderer.render(root)) {
        fprintf(stderr, "render_switch_visibility: frame2 render() failed\n");
        allOk = false;
    } else {
        const unsigned char *buf = renderer.getBuffer();
        bool redGone  = buf && isBlankAt(buf, LEFT_PX);
        bool blueOk   = buf && hasSphereAt(buf, RIGHT_PX, 0, 0, 255);
        printf("Frame2 (red OFF): redGone=%s blue=%s\n",
               redGone ? "YES" : "NO", blueOk ? "FOUND" : "MISSING");
        if (!redGone || !blueOk) {
            fprintf(stderr, "render_switch_visibility: FAIL frame2\n");
            allOk = false;
        }
    }

    // ---- Frame 3: blue switch OFF ----
    redSwitch ->whichChild.setValue(SO_SWITCH_ALL);
    blueSwitch->whichChild.setValue(SO_SWITCH_NONE);
    if (!renderer.render(root)) {
        fprintf(stderr, "render_switch_visibility: frame3 render() failed\n");
        allOk = false;
    } else {
        const unsigned char *buf = renderer.getBuffer();
        bool redOk    = buf && hasSphereAt(buf, LEFT_PX,  255, 0, 0);
        bool blueGone = buf && isBlankAt(buf, RIGHT_PX);
        printf("Frame3 (blue OFF): red=%s blueGone=%s\n",
               redOk ? "FOUND" : "MISSING", blueGone ? "YES" : "NO");
        if (!redOk || !blueGone) {
            fprintf(stderr, "render_switch_visibility: FAIL frame3\n");
            allOk = false;
        }
    }

    if (allOk) printf("render_switch_visibility: PASS\n");

    root->unref();
    return allOk ? 0 : 1;
}
