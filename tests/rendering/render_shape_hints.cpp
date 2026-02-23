/*
 * render_shape_hints.cpp - Integration test: SoShapeHints backface culling
 *
 * Tests three SoShapeHints configurations and their effect on rendered output:
 *
 *   Frame 1 (SOLID + COUNTERCLOCKWISE): sphere with backface culling enabled.
 *     Geometry should appear solid; front-facing polygons are visible.
 *
 *   Frame 2 (UNKNOWN_SHAPE_TYPE + COUNTERCLOCKWISE): backface culling off.
 *     Same sphere without culling — should render at least as many non-bg pixels
 *     as with culling.
 *
 *   Frame 3 (SOLID + COUNTERCLOCKWISE + creaseAngle=0): sharp edges with no
 *     smooth normals, applied to a cube.
 *
 * Pixel validation: each frame must be non-blank.
 *
 * Writes argv[1]+".rgb" (frame 1 image) and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoShapeHints.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>

static const int W = 256;
static const int H = 256;

static int countNonBackground(const unsigned char *buf)
{
    int count = 0;
    for (int i = 0; i < W * H; ++i) {
        const unsigned char *p = buf + i * 3;
        if (p[0] > 15 || p[1] > 15 || p[2] > 15) ++count;
    }
    return count;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_shape_hints.rgb");

    SbViewportRegion vp(W, H);
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    bool allOk = true;

    // -----------------------------------------------------------------------
    // Build a reusable scene with a mutable SoShapeHints node
    // -----------------------------------------------------------------------
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 4.0f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 50.0f;
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.3f, -0.7f, -0.6f);
    root->addChild(light);

    SoShapeHints *hints = new SoShapeHints;
    root->addChild(hints);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.6f, 0.3f, 0.9f);
    mat->specularColor.setValue(0.5f, 0.5f, 0.5f);
    mat->shininess.setValue(0.4f);
    root->addChild(mat);

    SoSphere *sph = new SoSphere;
    root->addChild(sph);

    // -----------------------------------------------------------------------
    // Frame 1: SOLID + COUNTERCLOCKWISE (backface culling enabled)
    // -----------------------------------------------------------------------
    hints->vertexOrdering.setValue(SoShapeHints::COUNTERCLOCKWISE);
    hints->shapeType.setValue(SoShapeHints::SOLID);
    hints->faceType.setValue(SoShapeHints::CONVEX);
    hints->creaseAngle.setValue(0.5f);

    bool ok1 = renderer.render(root);
    int nb1  = ok1 ? countNonBackground(renderer.getBuffer()) : 0;
    printf("render_shape_hints frame1 (SOLID+CCW): ok=%d nonbg=%d\n", ok1, nb1);
    renderer.writeToRGB(outpath);
    allOk = allOk && ok1 && (nb1 >= 100);

    // -----------------------------------------------------------------------
    // Frame 2: UNKNOWN_SHAPE_TYPE + COUNTERCLOCKWISE (no backface culling)
    // -----------------------------------------------------------------------
    hints->vertexOrdering.setValue(SoShapeHints::COUNTERCLOCKWISE);
    hints->shapeType.setValue(SoShapeHints::UNKNOWN_SHAPE_TYPE);
    hints->creaseAngle.setValue(0.5f);

    bool ok2 = renderer.render(root);
    int nb2  = ok2 ? countNonBackground(renderer.getBuffer()) : 0;
    printf("render_shape_hints frame2 (UNKNOWN): ok=%d nonbg=%d\n", ok2, nb2);
    allOk = allOk && ok2 && (nb2 >= 100);

    // -----------------------------------------------------------------------
    // Frame 3: SOLID + COUNTERCLOCKWISE + creaseAngle=0 on a cube
    // -----------------------------------------------------------------------
    // Replace sphere with cube
    root->removeChild(sph);
    root->addChild(new SoCube);

    mat->diffuseColor.setValue(0.9f, 0.5f, 0.2f);
    hints->vertexOrdering.setValue(SoShapeHints::COUNTERCLOCKWISE);
    hints->shapeType.setValue(SoShapeHints::SOLID);
    hints->faceType.setValue(SoShapeHints::CONVEX);
    hints->creaseAngle.setValue(0.0f);   // sharp edges, no normal smoothing

    bool ok3 = renderer.render(root);
    int nb3  = ok3 ? countNonBackground(renderer.getBuffer()) : 0;
    printf("render_shape_hints frame3 (SOLID+CCW+crease0 cube): ok=%d nonbg=%d\n", ok3, nb3);
    allOk = allOk && ok3 && (nb3 >= 100);

    root->unref();

    // -----------------------------------------------------------------------
    // Validation
    // -----------------------------------------------------------------------
    if (!allOk) {
        fprintf(stderr, "render_shape_hints: FAIL\n");
        return 1;
    }
    printf("render_shape_hints: PASS\n");
    return 0;
}
