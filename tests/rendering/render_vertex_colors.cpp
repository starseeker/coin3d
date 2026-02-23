/*
 * render_vertex_colors.cpp - Integration test: per-vertex colours via SoPackedColor
 *
 * Creates a quad with four distinctly coloured corners using SoPackedColor
 * with PER_VERTEX_INDEXED material binding:
 *   Bottom-left:  Red
 *   Bottom-right: Green
 *   Top-right:    Blue
 *   Top-left:     Yellow (red+green)
 *
 * SoPackedColor provides diffuse RGBA per-material-index; combined with
 * SoMaterialBinding::PER_VERTEX_INDEXED and SoIndexedFaceSet::materialIndex
 * this causes Coin to pass per-vertex colour values to OpenGL for Gouraud
 * interpolation.  A frontal directional light is included so the Phong
 * lighting pipeline is exercised with the per-vertex colours.
 *
 * Pixel validation confirms that each corner of the rendered image contains
 * approximately the expected colour, verifying that per-vertex colour
 * interpolation and SoPackedColor are working correctly.
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoPackedColor.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoNormal.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/nodes/SoMaterialBinding.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>
#include <cmath>

static const int W = 256;
static const int H = 256;
static const int MARGIN = 8;   // pixels from corners to sample

// Check that a pixel near (px,py) matches the expected RGB within tolerance.
static bool checkCorner(const unsigned char *buf, int px, int py,
                        unsigned char er, unsigned char eg, unsigned char eb,
                        int tol = 70, const char *label = "")
{
    if (px < 0 || px >= W || py < 0 || py >= H) return false;
    const unsigned char *p = buf + (py * W + px) * 3;
    bool ok = (std::abs((int)p[0] - (int)er) <= tol &&
               std::abs((int)p[1] - (int)eg) <= tol &&
               std::abs((int)p[2] - (int)eb) <= tol);
    printf("  Corner %-12s px=(%d,%d) got=(%3d,%3d,%3d) exp=(%3d,%3d,%3d) %s\n",
           label, px, py,
           (int)p[0], (int)p[1], (int)p[2],
           (int)er, (int)eg, (int)eb,
           ok ? "OK" : "FAIL");
    return ok;
}

static bool validateVertexColors(const unsigned char *buf)
{
    // OpenGL row order: y=0=bottom.
    // Bottom-left:  Red
    bool bl = checkCorner(buf, MARGIN,     MARGIN,     200,   0,   0, 80, "BottomLeft");
    // Bottom-right: Green
    bool br = checkCorner(buf, W-MARGIN-1, MARGIN,       0, 200,   0, 80, "BottomRight");
    // Top-right:    Blue
    bool tr = checkCorner(buf, W-MARGIN-1, H-MARGIN-1,   0,   0, 200, 80, "TopRight");
    // Top-left:     Yellow
    bool tl = checkCorner(buf, MARGIN,     H-MARGIN-1, 200, 200,   0, 80, "TopLeft");

    if (bl && br && tr && tl) {
        printf("render_vertex_colors: PASS\n");
        return true;
    }
    fprintf(stderr, "render_vertex_colors: FAIL - corner colour mismatch\n");
    return false;
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

    // Frontal directional light so Phong shading is exercised with vertex colours
    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction .setValue(0.0f, 0.0f, -1.0f);
    light->intensity .setValue(1.0f);
    root->addChild(light);

    // Per-vertex colour binding
    SoMaterialBinding *mb = new SoMaterialBinding;
    mb->value.setValue(SoMaterialBinding::PER_VERTEX_INDEXED);
    root->addChild(mb);

    // SoPackedColor provides per-material-index diffuse RGBA colours.
    // Format: 0xRRGGBBAA (MSB = R, LSB = A).
    SoPackedColor *pc = new SoPackedColor;
    pc->orderedRGBA.set1Value(0, 0xFF0000FFu);   // 0: Red
    pc->orderedRGBA.set1Value(1, 0x00FF00FFu);   // 1: Green
    pc->orderedRGBA.set1Value(2, 0x0000FFFFu);   // 2: Blue
    pc->orderedRGBA.set1Value(3, 0xFFFF00FFu);   // 3: Yellow
    root->addChild(pc);

    // Vertex positions: a quad filling world -1..+1
    SoCoordinate3 *coords = new SoCoordinate3;
    coords->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));  // 0: BL
    coords->point.set1Value(1, SbVec3f( 1.0f, -1.0f, 0.0f));  // 1: BR
    coords->point.set1Value(2, SbVec3f( 1.0f,  1.0f, 0.0f));  // 2: TR
    coords->point.set1Value(3, SbVec3f(-1.0f,  1.0f, 0.0f));  // 3: TL
    root->addChild(coords);

    // Single overall normal pointing toward camera (required for Phong lighting)
    SoNormal *normals = new SoNormal;
    normals->vector.set1Value(0, SbVec3f(0.0f, 0.0f, 1.0f));
    root->addChild(normals);
    SoNormalBinding *nb = new SoNormalBinding;
    nb->value.setValue(SoNormalBinding::OVERALL);
    root->addChild(nb);

    // SoIndexedFaceSet with explicit per-vertex material indices
    SoIndexedFaceSet *ifs = new SoIndexedFaceSet;
    ifs->coordIndex.set1Value(0, 0);
    ifs->coordIndex.set1Value(1, 1);
    ifs->coordIndex.set1Value(2, 2);
    ifs->coordIndex.set1Value(3, 3);
    ifs->coordIndex.set1Value(4, -1);
    ifs->materialIndex.set1Value(0, 0);  // BL → Red
    ifs->materialIndex.set1Value(1, 1);  // BR → Green
    ifs->materialIndex.set1Value(2, 2);  // TR → Blue
    ifs->materialIndex.set1Value(3, 3);  // TL → Yellow
    ifs->materialIndex.set1Value(4, -1);
    root->addChild(ifs);

    SbViewportRegion vpr(W, H);
    SoOffscreenRenderer renderer(vpr);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.5f, 0.5f, 0.5f));

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_vertex_colors.rgb");

    bool ok = false;
    if (renderer.render(root)) {
        const unsigned char *buf = renderer.getBuffer();
        bool pixOk = (buf != nullptr) && validateVertexColors(buf);
        ok = pixOk && renderer.writeToRGB(outpath);
    } else {
        fprintf(stderr, "render_vertex_colors: render() failed\n");
    }

    root->unref();
    return ok ? 0 : 1;
}
