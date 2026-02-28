/*
 * render_sphere_position.cpp - Validates pixel-accurate sphere positioning
 *
 * Renders an emissive red sphere offset from centre and verifies that the
 * sphere centre in the rendered image is within tolerance of the analytically
 * predicted pixel coordinate.
 *
 * The scene is built by the shared testlib factory (createSpherePosition).
 *
 * Writes argv[1]+".rgb" (SGI RGB format) and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/SbViewportRegion.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

static const int   IMG_W  = 256;
static const int   IMG_H  = 256;
static const float SPH_R  = 0.2f;    // world-space sphere radius
static const float SPH_X  = 0.3f;    // world-space X offset (positive = right)
static const float SPH_Y  = 0.2f;    // world-space Y offset (positive = up)
static const float CAM_H  = 2.0f;    // orthographic camera height (world units)

// Predicted pixel coordinates (y=0 is BOTTOM of buffer, OpenGL convention).
// For an orthographic camera of height CAM_H centred at the origin:
//   pixel_x = IMG_W * (0.5 + world_x / CAM_H)
//   pixel_y = IMG_H * (0.5 + world_y / CAM_H)   ← positive Y is UP in world space
//                                                   and also UP in the OpenGL buffer
static const int EXP_PX_X = (int)(IMG_W * (0.5f + SPH_X / CAM_H));
static const int EXP_PX_Y = (int)(IMG_H * (0.5f + SPH_Y / CAM_H));
static const int EXP_PX_R = (int)(SPH_R / CAM_H * IMG_H);

// Sphere emissive colour (reddish; must differ enough from the grey background)
static const unsigned char SPH_CH_R = 255, SPH_CH_G = 100, SPH_CH_B = 100;
static const unsigned char BG_CH    = 50;   // grey background channel value

static bool validateSpherePosition(const unsigned char *buf)
{
    int minX = IMG_W, maxX = -1, minY = IMG_H, maxY = -1;

    for (int y = 0; y < IMG_H; ++y) {
        for (int x = 0; x < IMG_W; ++x) {
            const unsigned char *p = buf + (y * IMG_W + x) * 3;
            if (std::abs((int)p[0] - (int)SPH_CH_R) < 30 &&
                std::abs((int)p[1] - (int)SPH_CH_G) < 30 &&
                std::abs((int)p[2] - (int)SPH_CH_B) < 30) {
                minX = std::min(minX, x); maxX = std::max(maxX, x);
                minY = std::min(minY, y); maxY = std::max(maxY, y);
            }
        }
    }

    if (maxX < 0) {
        fprintf(stderr, "render_sphere_position: no sphere pixels found in buffer\n");
        return false;
    }

    int cx = (minX + maxX) / 2;
    int cy = (minY + maxY) / 2;
    int dx = std::abs(cx - EXP_PX_X);
    int dy = std::abs(cy - EXP_PX_Y);
    int tol = EXP_PX_R + 4;   // a few pixels of tolerance

    printf("Sphere centre: actual=(%d,%d) expected=(%d,%d) [y=0=bottom]\n",
           cx, cy, EXP_PX_X, EXP_PX_Y);
    printf("Offset: (%d,%d) tolerance: %d\n", dx, dy, tol);

    if (dx > tol || dy > tol) {
        fprintf(stderr, "render_sphere_position: FAIL - sphere too far from expected position\n");
        return false;
    }
    printf("render_sphere_position: PASS\n");
    return true;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = ObolTest::Scenes::createSpherePosition(IMG_W, IMG_H);

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_sphere_position.rgb");

    SbViewportRegion vp(IMG_W, IMG_H);
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(BG_CH / 255.0f,
                                        BG_CH / 255.0f,
                                        BG_CH / 255.0f));

    bool ok = false;
    if (renderer.render(root)) {
        const unsigned char *buf = renderer.getBuffer();
        bool posOk = (buf != nullptr) && validateSpherePosition(buf);
        ok = posOk && renderer.writeToRGB(outpath);
    } else {
        fprintf(stderr, "render_sphere_position: render() failed\n");
    }

    root->unref();
    return ok ? 0 : 1;
}
