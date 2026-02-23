/*
 * render_face_set.cpp - Integration test: SoFaceSet with explicit geometry
 *
 * Renders a coloured quad built from SoFaceSet + SoCoordinate3.  The quad
 * fills the lower-left quadrant and is emissive green; the background is
 * black.  Pixel validation confirms that:
 *   - The lower-left region of the buffer contains green pixels.
 *   - The upper-right region contains only background (black) pixels.
 *
 * This exercises SoFaceSet, SoCoordinate3, SoMaterial, SoOrthographicCamera
 * and the fixed-function per-face rendering pipeline.
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoFaceSet.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>
#include <cmath>

static const int W = 256;
static const int H = 256;

// Sample the rendered RGB buffer and validate face-set rendering:
//  - lower-left quadrant should be green (emissive)
//  - upper-right quadrant should be black (background)
static bool validateFaceSet(const unsigned char *buf)
{
    int greenFound = 0, blackFound = 0;

    // Lower-left quadrant (x: 0..W/2, y: 0..H/2 in OpenGL row order: y=0=bottom)
    for (int y = 0; y < H / 2; y += 4) {
        for (int x = 0; x < W / 2; x += 4) {
            const unsigned char *p = buf + (y * W + x) * 3;
            if (p[1] > 180 && p[0] < 50 && p[2] < 50)
                ++greenFound;
        }
    }

    // Upper-right quadrant (x: W/2..W, y: H/2..H)
    for (int y = H / 2; y < H; y += 4) {
        for (int x = W / 2; x < W; x += 4) {
            const unsigned char *p = buf + (y * W + x) * 3;
            if (p[0] < 20 && p[1] < 20 && p[2] < 20)
                ++blackFound;
        }
    }

    printf("render_face_set: greenFound=%d blackFound=%d\n", greenFound, blackFound);

    if (greenFound < 10) {
        fprintf(stderr, "render_face_set: FAIL - not enough green pixels in lower-left quadrant\n");
        return false;
    }
    if (blackFound < 10) {
        fprintf(stderr, "render_face_set: FAIL - not enough black pixels in upper-right quadrant\n");
        return false;
    }
    printf("render_face_set: PASS\n");
    return true;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = new SoSeparator;
    root->ref();

    // Orthographic camera, world coords: -1..+1 in both axes
    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position    .setValue(0, 0, 1);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 10.0f;
    cam->height       = 2.0f;
    root->addChild(cam);

    SoSeparator *faceGrp = new SoSeparator;

    // Bright emissive green material
    SoMaterial *mat = new SoMaterial;
    mat->emissiveColor.setValue(0.0f, 1.0f, 0.0f);
    mat->diffuseColor .setValue(0.0f, 0.0f, 0.0f);
    faceGrp->addChild(mat);

    // A quad in the lower-left quadrant: x in [-1, 0], y in [-1, 0]
    SoCoordinate3 *coords = new SoCoordinate3;
    coords->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
    coords->point.set1Value(1, SbVec3f( 0.0f, -1.0f, 0.0f));
    coords->point.set1Value(2, SbVec3f( 0.0f,  0.0f, 0.0f));
    coords->point.set1Value(3, SbVec3f(-1.0f,  0.0f, 0.0f));
    faceGrp->addChild(coords);

    // SoFaceSet: one face with 4 vertices
    SoFaceSet *faces = new SoFaceSet;
    faces->numVertices.set1Value(0, 4);
    faceGrp->addChild(faces);

    root->addChild(faceGrp);

    SbViewportRegion vp(W, H);

    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_face_set.rgb");

    bool ok = false;
    if (renderer.render(root)) {
        const unsigned char *buf = renderer.getBuffer();
        bool pixOk = (buf != nullptr) && validateFaceSet(buf);
        ok = pixOk && renderer.writeToRGB(outpath);
    } else {
        fprintf(stderr, "render_face_set: render() failed\n");
    }

    root->unref();
    return ok ? 0 : 1;
}
