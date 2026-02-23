/*
 * render_coordinates.cpp - Verifies coordinate-system orientation
 *
 * Fills each quadrant of the viewport with a distinct emissive colour:
 *   Bottom-left  → Red
 *   Bottom-right → Green
 *   Top-right    → Blue
 *   Top-left     → Yellow
 *
 * The SoOffscreenRenderer pixel buffer is in OpenGL order (row 0 = bottom), so
 * the control image produced here can be compared against future runs to catch
 * any change in coordinate orientation or Y-flip behaviour.
 *
 * Writes argv[1]+".rgb" (SGI RGB format).
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoVertexProperty.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <cstdio>

// Add a single coloured triangle filling the given world-space triangle
static SoSeparator *makeTriangle(float r, float g, float b,
                                 float x0, float y0,
                                 float x1, float y1,
                                 float x2, float y2)
{
    SoSeparator *sep = new SoSeparator;

    SoMaterial *mat = new SoMaterial;
    mat->emissiveColor.setValue(r, g, b);
    mat->diffuseColor .setValue(0, 0, 0);
    sep->addChild(mat);

    SoVertexProperty *vp = new SoVertexProperty;
    vp->vertex.set1Value(0, SbVec3f(x0, y0, 0));
    vp->vertex.set1Value(1, SbVec3f(x1, y1, 0));
    vp->vertex.set1Value(2, SbVec3f(x2, y2, 0));

    SoIndexedFaceSet *face = new SoIndexedFaceSet;
    face->vertexProperty = vp;
    face->coordIndex.set1Value(0, 0);
    face->coordIndex.set1Value(1, 1);
    face->coordIndex.set1Value(2, 2);
    face->coordIndex.set1Value(3, -1);
    sep->addChild(face);

    return sep;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position    .setValue(0, 0, 1);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 10.0f;
    cam->height       = 2.0f;   // world Y: -1 to +1
    root->addChild(cam);

    // Bottom-left:  Red    (-1,-1) → (0,-1) → (-1,0)
    root->addChild(makeTriangle(1,0,0,  -1,-1,  0,-1,  -1,0));
    // Bottom-right: Green   (1,-1) → (0,-1) →  (1,0)
    root->addChild(makeTriangle(0,1,0,   1,-1,  0,-1,   1,0));
    // Top-right:    Blue    (1, 1) → (0, 1) →  (1,0)
    root->addChild(makeTriangle(0,0,1,   1, 1,  0, 1,   1,0));
    // Top-left:     Yellow (-1, 1) → (0, 1) → (-1,0)
    root->addChild(makeTriangle(1,1,0,  -1, 1,  0, 1,  -1,0));

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_coordinates.rgb");

    bool ok = renderToFile(root, outpath,
                           DEFAULT_WIDTH, DEFAULT_HEIGHT,
                           SbColor(0.0f, 0.0f, 0.0f));
    root->unref();
    return ok ? 0 : 1;
}
