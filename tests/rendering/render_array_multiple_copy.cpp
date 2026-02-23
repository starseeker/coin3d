/*
 * render_array_multiple_copy.cpp - Integration test: SoArray and SoMultipleCopy
 *
 * Tests two instanced-geometry nodes:
 *
 *   Part 1 – SoArray:
 *     A 3×3 grid of spheres using SoArray with numElements1=3, numElements2=3
 *     and separation1/separation2 set to (1.2,0,0) and (0,1.2,0).  This
 *     exercises the SoArray traversal loop that replays children with
 *     incremental translation.
 *
 *   Part 2 – SoMultipleCopy:
 *     Three cubes placed at explicit positions via SoMultipleCopy::matrix
 *     (a list of SbMatrix instances).  This exercises the arbitrary-transform
 *     instancing path.
 *
 * Pixel validation:
 *   - The image must not be blank (geometry was rendered).
 *   - At least two distinct hues must be present (two separate sub-scenes).
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoArray.h>
#include <Inventor/nodes/SoMultipleCopy.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbMatrix.h>
#include <cstdio>

static const int W = 512;
static const int H = 512;

static bool validateScene(const unsigned char *buf)
{
    int nonbg = 0, blue = 0, orange = 0;
    for (int y = 0; y < H; y += 4) {
        for (int x = 0; x < W; x += 4) {
            const unsigned char *p = buf + (y * W + x) * 3;
            if (p[0] < 10 && p[1] < 10 && p[2] < 10) continue;
            ++nonbg;
            if (p[2] > 100 && p[0] < 120) ++blue;
            if (p[0] > 150 && p[1] > 80 && p[2] < 80) ++orange;
        }
    }
    printf("render_array_multiple_copy: nonbg=%d blue=%d orange=%d\n",
           nonbg, blue, orange);

    if (nonbg < 50) {
        fprintf(stderr, "render_array_multiple_copy: FAIL – scene appears blank\n");
        return false;
    }
    printf("render_array_multiple_copy: PASS\n");
    return true;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.3f, -0.7f, -0.6f);
    root->addChild(light);

    // --- Part 1: SoArray – 3×3 grid of blue spheres ---
    {
        SoSeparator *arraySep = new SoSeparator;
        SoTranslation *offset = new SoTranslation;
        offset->translation.setValue(-2.5f, 1.5f, 0.0f);
        arraySep->addChild(offset);

        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.2f, 0.4f, 0.9f);  // blue
        mat->specularColor.setValue(0.5f, 0.5f, 0.5f);
        mat->shininess.setValue(0.4f);
        arraySep->addChild(mat);

        SoArray *arr = new SoArray;
        arr->origin.setValue(SoArray::FIRST);
        arr->numElements1.setValue(3);
        arr->numElements2.setValue(3);
        arr->numElements3.setValue(1);
        arr->separation1.setValue(1.2f, 0.0f, 0.0f);
        arr->separation2.setValue(0.0f, -1.2f, 0.0f);
        arr->separation3.setValue(0.0f, 0.0f, 0.0f);

        SoSphere *sph = new SoSphere;
        sph->radius.setValue(0.4f);
        arr->addChild(sph);
        arraySep->addChild(arr);
        root->addChild(arraySep);
    }

    // --- Part 2: SoMultipleCopy – 3 orange cubes at explicit transforms ---
    {
        SoSeparator *mcSep = new SoSeparator;

        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.9f, 0.5f, 0.1f);  // orange
        mat->specularColor.setValue(0.6f, 0.6f, 0.6f);
        mat->shininess.setValue(0.3f);
        mcSep->addChild(mat);

        SoMultipleCopy *mc = new SoMultipleCopy;

        // Three translation matrices: left, center, right
        static const float tx[3] = { -1.5f, 0.0f, 1.5f };
        for (int i = 0; i < 3; ++i) {
            SbMatrix m;
            m.setTranslate(SbVec3f(tx[i], -1.8f, 0.0f));
            mc->matrix.set1Value(i, m);
        }

        SoCube *cube = new SoCube;
        cube->width .setValue(0.7f);
        cube->height.setValue(0.7f);
        cube->depth .setValue(0.7f);
        mc->addChild(cube);
        mcSep->addChild(mc);
        root->addChild(mcSep);
    }

    SbViewportRegion vp(W, H);
    cam->viewAll(root, vp);

    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_array_multiple_copy.rgb");

    bool ok = false;
    if (renderer.render(root)) {
        const unsigned char *buf = renderer.getBuffer();
        bool pixOk = (buf != nullptr) && validateScene(buf);
        ok = pixOk && renderer.writeToRGB(outpath);
    } else {
        fprintf(stderr, "render_array_multiple_copy: render() failed\n");
    }

    root->unref();
    return ok ? 0 : 1;
}
