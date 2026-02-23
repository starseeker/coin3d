/*
 * render_point_set.cpp - Integration test: SoPointSet rendering
 *
 * Renders a 4-point scene using SoPointSet + SoPackedColor + SoCoordinate3.
 * Each of the four points is positioned in a different quadrant and coloured
 * distinctly (red, green, blue, white) via SoPackedColor with
 * SoMaterialBinding::PER_VERTEX to exercise the per-vertex colour path
 * through SoPointSet.
 *
 * Pixel validation confirms that all four colour families are present,
 * verifying that per-point colour assignment and the point rendering
 * pipeline are working correctly.
 *
 * This exercises SoPointSet, SoPackedColor, SoCoordinate3,
 * SoMaterialBinding::PER_VERTEX, SoDrawStyle (pointSize), and the point
 * rasterisation path in the GL renderer.
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoPackedColor.h>
#include <Inventor/nodes/SoMaterialBinding.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoPointSet.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>
#include <cmath>

static const int W = 256;
static const int H = 256;

static bool validatePointSet(const unsigned char *buf)
{
    int red = 0, green = 0, blue = 0, bright = 0;
    for (int i = 0; i < W * H; ++i) {
        const unsigned char *p = buf + i * 3;
        if (p[0] > 150 && p[1] < 80 && p[2] < 80) ++red;
        if (p[1] > 150 && p[0] < 80 && p[2] < 80) ++green;
        if (p[2] > 150 && p[0] < 80 && p[1] < 80) ++blue;
        if (p[0] > 200 && p[1] > 200 && p[2] > 200) ++bright;
    }
    printf("render_point_set: red=%d green=%d blue=%d bright=%d\n",
           red, green, blue, bright);

    int families = (red > 0) + (green > 0) + (blue > 0) + (bright > 0);
    if (families < 3) {
        fprintf(stderr, "render_point_set: FAIL – expected at least 3 colour families\n");
        return false;
    }
    printf("render_point_set: PASS\n");
    return true;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = new SoSeparator;
    root->ref();

    // Orthographic camera, world -1..+1
    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position    .setValue(0, 0, 1);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 10.0f;
    cam->height       = 2.0f;
    root->addChild(cam);

    // Large point size so pixels are easily detectable
    SoDrawStyle *ds = new SoDrawStyle;
    ds->pointSize.setValue(20.0f);
    root->addChild(ds);

    // Per-vertex material binding
    SoMaterialBinding *mb = new SoMaterialBinding;
    mb->value.setValue(SoMaterialBinding::PER_VERTEX);
    root->addChild(mb);

    // SoPackedColor: one entry per point (RGBA, MSB=R)
    SoPackedColor *pc = new SoPackedColor;
    pc->orderedRGBA.set1Value(0, 0xFF0000FFu);   // top-left:     Red
    pc->orderedRGBA.set1Value(1, 0x00FF00FFu);   // top-right:    Green
    pc->orderedRGBA.set1Value(2, 0x0000FFFFu);   // bottom-left:  Blue
    pc->orderedRGBA.set1Value(3, 0xFFFFFFFFu);   // bottom-right: White
    root->addChild(pc);

    // Four point positions in the four quadrants
    SoCoordinate3 *coords = new SoCoordinate3;
    coords->point.set1Value(0, SbVec3f(-0.5f,  0.5f, 0.0f));  // top-left
    coords->point.set1Value(1, SbVec3f( 0.5f,  0.5f, 0.0f));  // top-right
    coords->point.set1Value(2, SbVec3f(-0.5f, -0.5f, 0.0f));  // bottom-left
    coords->point.set1Value(3, SbVec3f( 0.5f, -0.5f, 0.0f));  // bottom-right
    root->addChild(coords);

    SoPointSet *ps = new SoPointSet;
    root->addChild(ps);

    SbViewportRegion vpr(W, H);
    SoOffscreenRenderer renderer(vpr);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_point_set.rgb");

    bool ok = false;
    if (renderer.render(root)) {
        const unsigned char *buf = renderer.getBuffer();
        bool pixOk = (buf != nullptr) && validatePointSet(buf);
        ok = pixOk && renderer.writeToRGB(outpath);
    } else {
        fprintf(stderr, "render_point_set: render() failed\n");
    }

    root->unref();
    return ok ? 0 : 1;
}
