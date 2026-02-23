/*
 * render_indexed_line_set.cpp - Integration test: SoIndexedLineSet polylines
 *
 * Renders three polyline paths using SoIndexedLineSet:
 *   - A green horizontal line across the top third of the viewport
 *   - A red diagonal line from bottom-left to top-right
 *   - A blue "V" shape in the lower portion
 *
 * Each polyline uses a different SoBaseColor to exercise per-polyline colour
 * assignment.  Pixel validation confirms that:
 *   - Pixels near the horizontal scan line (top third) contain green.
 *   - Pixels near the diagonal contain reddish values.
 *
 * This exercises SoIndexedLineSet, SoCoordinate3, SoBaseColor, SoDrawStyle
 * and the per-polyline GL line rendering path.
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoIndexedLineSet.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>
#include <cmath>

static const int W = 256;
static const int H = 256;

static bool validateIndexedLineSet(const unsigned char *buf)
{
    // Green horizontal line is at world Y ≈ +0.5.
    // Camera height = 2, so world +0.5 → pixel row H*(0.5 + 0.5/2) = H*0.75 = 192
    // (OpenGL row 0 = bottom)
    int greenFound = 0, redFound = 0;

    int greenRow = (int)(H * (0.5f + 0.5f / 2.0f));

    // Scan a band ±6 rows around the expected green line row
    for (int y = greenRow - 6; y <= greenRow + 6; ++y) {
        if (y < 0 || y >= H) continue;
        for (int x = W / 8; x < 7 * W / 8; x += 4) {
            const unsigned char *p = buf + (y * W + x) * 3;
            if (p[1] > 150 && p[0] < 80 && p[2] < 80) ++greenFound;
        }
    }

    // Red diagonal: somewhere in the middle band
    for (int y = H / 4; y < 3 * H / 4; y += 4) {
        for (int x = W / 4; x < 3 * W / 4; x += 4) {
            const unsigned char *p = buf + (y * W + x) * 3;
            if (p[0] > 150 && p[1] < 80 && p[2] < 80) ++redFound;
        }
    }

    printf("render_indexed_line_set: greenFound=%d redFound=%d\n",
           greenFound, redFound);

    if (greenFound < 3) {
        fprintf(stderr, "render_indexed_line_set: FAIL – green line not found\n");
        return false;
    }
    if (redFound < 3) {
        fprintf(stderr, "render_indexed_line_set: FAIL – red diagonal not found\n");
        return false;
    }
    printf("render_indexed_line_set: PASS\n");
    return true;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = new SoSeparator;
    root->ref();

    // Orthographic camera: world -1..+1
    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position    .setValue(0, 0, 1);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 10.0f;
    cam->height       = 2.0f;
    root->addChild(cam);

    // Thick lines for easy detection
    SoDrawStyle *ds = new SoDrawStyle;
    ds->lineWidth.setValue(3.0f);
    root->addChild(ds);

    // All polylines share one SoCoordinate3 node
    SoCoordinate3 *coords = new SoCoordinate3;
    // 0-1: green horizontal at Y=+0.5
    coords->point.set1Value(0, SbVec3f(-0.9f,  0.5f, 0.0f));
    coords->point.set1Value(1, SbVec3f( 0.9f,  0.5f, 0.0f));
    // 2-3: red diagonal from BL to TR
    coords->point.set1Value(2, SbVec3f(-0.7f, -0.7f, 0.0f));
    coords->point.set1Value(3, SbVec3f( 0.7f,  0.3f, 0.0f));
    // 4-5-6: blue "V" in lower region
    coords->point.set1Value(4, SbVec3f(-0.6f, -0.1f, 0.0f));
    coords->point.set1Value(5, SbVec3f( 0.0f, -0.8f, 0.0f));
    coords->point.set1Value(6, SbVec3f( 0.6f, -0.1f, 0.0f));
    root->addChild(coords);

    // --- Green horizontal ---
    SoSeparator *lineSep1 = new SoSeparator;
    SoBaseColor *bc1 = new SoBaseColor;
    bc1->rgb.setValue(SbColor(0.0f, 1.0f, 0.0f));
    lineSep1->addChild(bc1);
    SoIndexedLineSet *ils1 = new SoIndexedLineSet;
    ils1->coordIndex.set1Value(0, 0);
    ils1->coordIndex.set1Value(1, 1);
    ils1->coordIndex.set1Value(2, -1);
    lineSep1->addChild(ils1);
    root->addChild(lineSep1);

    // --- Red diagonal ---
    SoSeparator *lineSep2 = new SoSeparator;
    SoBaseColor *bc2 = new SoBaseColor;
    bc2->rgb.setValue(SbColor(1.0f, 0.0f, 0.0f));
    lineSep2->addChild(bc2);
    SoIndexedLineSet *ils2 = new SoIndexedLineSet;
    ils2->coordIndex.set1Value(0, 2);
    ils2->coordIndex.set1Value(1, 3);
    ils2->coordIndex.set1Value(2, -1);
    lineSep2->addChild(ils2);
    root->addChild(lineSep2);

    // --- Blue "V" (two segments) ---
    SoSeparator *lineSep3 = new SoSeparator;
    SoBaseColor *bc3 = new SoBaseColor;
    bc3->rgb.setValue(SbColor(0.2f, 0.2f, 1.0f));
    lineSep3->addChild(bc3);
    SoIndexedLineSet *ils3 = new SoIndexedLineSet;
    ils3->coordIndex.set1Value(0, 4);
    ils3->coordIndex.set1Value(1, 5);
    ils3->coordIndex.set1Value(2, -1);
    ils3->coordIndex.set1Value(3, 5);
    ils3->coordIndex.set1Value(4, 6);
    ils3->coordIndex.set1Value(5, -1);
    lineSep3->addChild(ils3);
    root->addChild(lineSep3);

    SbViewportRegion vp(W, H);
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_indexed_line_set.rgb");

    bool ok = false;
    if (renderer.render(root)) {
        const unsigned char *buf = renderer.getBuffer();
        bool pixOk = (buf != nullptr) && validateIndexedLineSet(buf);
        ok = pixOk && renderer.writeToRGB(outpath);
    } else {
        fprintf(stderr, "render_indexed_line_set: render() failed\n");
    }

    root->unref();
    return ok ? 0 : 1;
}
