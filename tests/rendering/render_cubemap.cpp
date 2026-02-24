/*
 * render_cubemap.cpp - Integration test: SoTextureCubeMap cube-map rendering
 *
 * Exercises the SoTextureCubeMap + SoGLCubeMapImage path:
 *   1. Constructs 6 procedural face images (each a solid colour).
 *   2. Applies SoTextureCubeMap with REPLACE model to a sphere.
 *   3. Renders with SoOffscreenRenderer.
 *   4. Validates that non-background pixels are present in the output.
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoTextureCubeMap.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoTextureCoordinateEnvironment.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbColor.h>
#include <cstdio>
#include <cstring>
#include <cmath>

static const int W = 256;
static const int H = 256;
static const int FACE_SIZE = 64;

// Fill a square RGBA image with a solid colour.
static void fillFace(unsigned char * data, int w, int h,
                     unsigned char r, unsigned char g,
                     unsigned char b)
{
    for (int i = 0; i < w * h; ++i) {
        data[i * 4 + 0] = r;
        data[i * 4 + 1] = g;
        data[i * 4 + 2] = b;
        data[i * 4 + 3] = 255;
    }
}

int main(int argc, char ** argv)
{
    initCoinHeadless();

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_cubemap.rgb");

    bool ok = true;

    // -----------------------------------------------------------------------
    // Build scene
    // -----------------------------------------------------------------------
    SoSeparator * root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera * cam = new SoPerspectiveCamera;
    root->addChild(cam);

    SoDirectionalLight * light = new SoDirectionalLight;
    light->direction.setValue(-1.0f, -1.0f, -1.0f);
    root->addChild(light);

    // SoTextureCubeMap: fill each face with a distinct colour
    SoTextureCubeMap * cubeMap = new SoTextureCubeMap;
    cubeMap->model = SoTextureCubeMap::REPLACE;

    unsigned char faceData[FACE_SIZE * FACE_SIZE * 4];
    SbVec2s faceSize(FACE_SIZE, FACE_SIZE);

    fillFace(faceData, FACE_SIZE, FACE_SIZE, 255,   0,   0);
    cubeMap->imagePosX.setValue(faceSize, 4, faceData);

    fillFace(faceData, FACE_SIZE, FACE_SIZE,   0, 255,   0);
    cubeMap->imageNegX.setValue(faceSize, 4, faceData);

    fillFace(faceData, FACE_SIZE, FACE_SIZE,   0,   0, 255);
    cubeMap->imagePosY.setValue(faceSize, 4, faceData);

    fillFace(faceData, FACE_SIZE, FACE_SIZE, 255, 255,   0);
    cubeMap->imageNegY.setValue(faceSize, 4, faceData);

    fillFace(faceData, FACE_SIZE, FACE_SIZE,   0, 255, 255);
    cubeMap->imagePosZ.setValue(faceSize, 4, faceData);

    fillFace(faceData, FACE_SIZE, FACE_SIZE, 255,   0, 255);
    cubeMap->imageNegZ.setValue(faceSize, 4, faceData);

    root->addChild(cubeMap);

    // Reflection environment texture coordinates
    root->addChild(new SoTextureCoordinateEnvironment);

    // Sphere with cube-map texture
    SoSphere * sphere = new SoSphere;
    sphere->radius.setValue(1.2f);
    root->addChild(sphere);

    SbViewportRegion vp(W, H);
    cam->viewAll(root, vp);

    // -----------------------------------------------------------------------
    // Render and validate
    // -----------------------------------------------------------------------
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    if (!renderer.render(root)) {
        fprintf(stderr, "render_cubemap: FAIL - render() returned false\n");
        ok = false;
    } else {
        const unsigned char * buf = renderer.getBuffer();
        if (!buf) {
            fprintf(stderr, "render_cubemap: FAIL - getBuffer() returned null\n");
            ok = false;
        } else {
            // Count non-black pixels (the coloured sphere faces should cover
            // a significant portion of the viewport)
            int nonBlack = 0;
            for (int i = 0; i < W * H; ++i) {
                const unsigned char * p = buf + i * 3;
                if (p[0] > 30 || p[1] > 30 || p[2] > 30)
                    ++nonBlack;
            }
            printf("render_cubemap: non-black pixels = %d / %d\n",
                   nonBlack, W * H);
            if (nonBlack < (W * H / 8)) {
                fprintf(stderr, "render_cubemap: FAIL - too few non-black pixels "
                        "(%d < %d)\n", nonBlack, W * H / 8);
                ok = false;
            } else {
                printf("render_cubemap: pixel validation OK\n");
                renderer.writeToRGB(outpath);
            }
        }
    }

    root->unref();
    printf("render_cubemap: %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
