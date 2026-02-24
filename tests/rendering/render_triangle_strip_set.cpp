/*
 * render_triangle_strip_set.cpp - Integration test: SoTriangleStripSet rendering
 *
 * Renders a triangle strip forming a quad (2 triangles) using SoTriangleStripSet
 * with an emissive blue material.  Pixel validation confirms:
 *   - Blue pixels are present in the lower portion of the buffer.
 *   - Black pixels (background) appear in the upper portion.
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoTriangleStripSet.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>
#include <cmath>

static const int W = 256;
static const int H = 256;

static bool validateTriangleStrip(const unsigned char * buf)
{
    int blueFound = 0, blackFound = 0;

    // Lower half (y: 0..H/2 in OpenGL order: y=0 = bottom of viewport)
    // The geometry is in y = [-1, 0], which maps to the lower half
    for (int y = 0; y < H / 2; y += 4) {
        for (int x = W / 4; x < 3 * W / 4; x += 4) {
            const unsigned char * p = buf + (y * W + x) * 3;
            if (p[2] > 150 && p[0] < 60 && p[1] < 60)
                ++blueFound;
        }
    }

    // Upper half (y: H/2..H) should be black background
    for (int y = H / 2; y < H; y += 4) {
        for (int x = W / 4; x < 3 * W / 4; x += 4) {
            const unsigned char * p = buf + (y * W + x) * 3;
            if (p[0] < 20 && p[1] < 20 && p[2] < 20)
                ++blackFound;
        }
    }

    printf("render_triangle_strip_set: blueFound=%d blackFound=%d\n",
           blueFound, blackFound);

    if (blueFound < 5) {
        fprintf(stderr, "render_triangle_strip_set: FAIL - not enough blue pixels\n");
        return false;
    }
    if (blackFound < 5) {
        fprintf(stderr, "render_triangle_strip_set: FAIL - not enough black pixels\n");
        return false;
    }
    printf("render_triangle_strip_set: PASS\n");
    return true;
}

int main(int argc, char ** argv)
{
    initCoinHeadless();

    SoSeparator * root = new SoSeparator;
    root->ref();

    // Orthographic camera; world coords -1..+1 in both axes
    SoOrthographicCamera * cam = new SoOrthographicCamera;
    cam->position    .setValue(0.0f, 0.0f, 1.0f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 10.0f;
    cam->height       = 2.0f;
    root->addChild(cam);

    SoSeparator * grp = new SoSeparator;

    // Emissive blue material (no lighting dependency)
    SoMaterial * mat = new SoMaterial;
    mat->emissiveColor.setValue(0.0f, 0.0f, 1.0f);
    mat->diffuseColor .setValue(0.0f, 0.0f, 0.0f);
    grp->addChild(mat);

    // 4 vertices forming a quad in the lower half: y in [-1, 0]
    SoCoordinate3 * coords = new SoCoordinate3;
    coords->point.set1Value(0, SbVec3f(-0.8f, -1.0f, 0.0f));
    coords->point.set1Value(1, SbVec3f( 0.8f, -1.0f, 0.0f));
    coords->point.set1Value(2, SbVec3f(-0.8f,  0.0f, 0.0f));
    coords->point.set1Value(3, SbVec3f( 0.8f,  0.0f, 0.0f));
    grp->addChild(coords);

    // Triangle strip: one strip of 4 vertices = 2 triangles
    SoTriangleStripSet * strips = new SoTriangleStripSet;
    strips->numVertices.set1Value(0, 4);
    grp->addChild(strips);

    root->addChild(grp);

    SbViewportRegion vp(W, H);
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_triangle_strip_set.rgb");

    bool ok = false;
    if (renderer.render(root)) {
        const unsigned char * buf = renderer.getBuffer();
        bool pixOk = (buf != nullptr) && validateTriangleStrip(buf);
        ok = pixOk && renderer.writeToRGB(outpath);
    } else {
        fprintf(stderr, "render_triangle_strip_set: render() failed\n");
    }

    root->unref();
    return ok ? 0 : 1;
}
