/*
 * render_line_set.cpp - Integration test: SoLineSet rendering
 *
 * Renders a horizontal line across the middle of the viewport using
 * SoLineSet + SoCoordinate3.  The line is bright red (SoBaseColor) against a
 * black background.  Pixel validation confirms that:
 *   - Pixels near the horizontal centre of the buffer contain red pixels.
 *   - Pixels well above and below the centre line are black.
 *
 * SoBaseColor is used instead of SoMaterial::emissiveColor because the
 * emissive path does not colour line/point primitives under the default
 * Phong lighting model; SoBaseColor sets the GL current colour directly.
 *
 * This exercises SoLineSet, SoCoordinate3, SoDrawStyle, SoBaseColor and the
 * line rendering path in the GL renderer.
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoLineSet.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>
#include <cmath>

static const int W = 256;
static const int H = 256;

// Validate that a horizontal line is visible in the centre row band.
static bool validateLineSet(const unsigned char *buf)
{
    // The line sits at world Y=0.  For a 256px viewport with camera height=2
    // (world -1..+1), world Y=0 maps to pixel row H/2 = 128 (OpenGL: y=0=bottom).
    // Sample a band of ±4 rows around row 128.
    int redFound   = 0;
    int blackAbove = 0;   // rows well above the line
    int blackBelow = 0;   // rows well below the line

    for (int x = W / 8; x < 7 * W / 8; x += 4) {
        // Near the centre band: rows 124..132
        for (int y = H / 2 - 4; y <= H / 2 + 4; ++y) {
            const unsigned char *p = buf + (y * W + x) * 3;
            if (p[0] > 180 && p[1] < 50 && p[2] < 50)
                ++redFound;
        }
        // Far above the line
        {
            const unsigned char *p = buf + ((3 * H / 4) * W + x) * 3;
            if (p[0] < 20 && p[1] < 20 && p[2] < 20)
                ++blackAbove;
        }
        // Far below the line
        {
            const unsigned char *p = buf + ((H / 4) * W + x) * 3;
            if (p[0] < 20 && p[1] < 20 && p[2] < 20)
                ++blackBelow;
        }
    }

    printf("render_line_set: redFound=%d blackAbove=%d blackBelow=%d\n",
           redFound, blackAbove, blackBelow);

    if (redFound < 5) {
        fprintf(stderr, "render_line_set: FAIL - not enough red pixels along horizontal line\n");
        return false;
    }
    if (blackAbove < 5 || blackBelow < 5) {
        fprintf(stderr, "render_line_set: FAIL - background above/below the line is not black\n");
        return false;
    }
    printf("render_line_set: PASS\n");
    return true;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = new SoSeparator;
    root->ref();

    // Orthographic camera, world -1..+1 in both axes
    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position    .setValue(0, 0, 1);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 10.0f;
    cam->height       = 2.0f;
    root->addChild(cam);

    SoSeparator *lineGrp = new SoSeparator;

    // Thick line for easy detection
    SoDrawStyle *ds = new SoDrawStyle;
    ds->lineWidth.setValue(3.0f);
    lineGrp->addChild(ds);

    // SoBaseColor sets the current OpenGL colour directly, bypassing the
    // lighting pipeline – the only reliable way to colour primitive-type
    // geometry (lines, points) that ignores material emissive settings.
    SoBaseColor *bc = new SoBaseColor;
    bc->rgb.setValue(SbColor(1.0f, 0.0f, 0.0f));
    lineGrp->addChild(bc);

    // Horizontal line from left to right at Y=0
    SoCoordinate3 *coords = new SoCoordinate3;
    coords->point.set1Value(0, SbVec3f(-0.9f, 0.0f, 0.0f));
    coords->point.set1Value(1, SbVec3f( 0.9f, 0.0f, 0.0f));
    lineGrp->addChild(coords);

    // SoLineSet: one line strip with 2 vertices
    SoLineSet *ls = new SoLineSet;
    ls->numVertices.set1Value(0, 2);
    lineGrp->addChild(ls);

    root->addChild(lineGrp);

    SbViewportRegion vp(W, H);

    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_line_set.rgb");

    bool ok = false;
    if (renderer.render(root)) {
        const unsigned char *buf = renderer.getBuffer();
        bool pixOk = (buf != nullptr) && validateLineSet(buf);
        ok = pixOk && renderer.writeToRGB(outpath);
    } else {
        fprintf(stderr, "render_line_set: render() failed\n");
    }

    root->unref();
    return ok ? 0 : 1;
}
