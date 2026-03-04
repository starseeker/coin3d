/*
 * render_bump_map.cpp - Integration test: SoBumpMap + SoBumpMapCoordinate
 *
 * Applies a procedural normal-map texture to a sphere using SoBumpMap and
 * SoBumpMapCoordinate.  The normal map is a simple 32×32 RGBA image where
 * the normal direction is derived from a bump pattern (horizontal stripes),
 * packed as (R,G,B) = (nx*0.5+0.5, ny*0.5+0.5, nz*0.5+0.5).
 *
 * Pixel validation: the rendered sphere must be non-blank.  Correct bump
 * shading would show subtle lighting variations, but we only require that
 * the geometry is visible.
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoBumpMap.h>
#include <Inventor/nodes/SoBumpMapCoordinate.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbVec2f.h>
#include <cstdio>
#include <cmath>

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

// Build a simple 32×32 RGBA normal map (horizontal sinusoidal bumps)
static void buildNormalMap(SoBumpMap *bump)
{
    const int S  = 32;
    const int NC = 4;
    unsigned char buf[S * S * NC];
    for (int y = 0; y < S; ++y) {
        for (int x = 0; x < S; ++x) {
            // Sinusoidal bump in the Y direction → normals tilt in Y
            float phase = (float)y / (float)S * 2.0f * 3.14159f * 4.0f;
            float ny = sinf(phase) * 0.5f;
            float nz = sqrtf(1.0f - ny * ny);   // keep unit length
            int idx  = (y * S + x) * NC;
            buf[idx]   = 128;                                 // nx → 0
            buf[idx+1] = (unsigned char)((ny * 0.5f + 0.5f) * 255.0f);
            buf[idx+2] = (unsigned char)((nz * 0.5f + 0.5f) * 255.0f);
            buf[idx+3] = 255;
        }
    }
    bump->image.setValue(SbVec2s(S, S), NC, buf);
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_bump_map.rgb");


    /* Render the canonical factory scene as the primary output image.
     * This ensures obol_viewer and obol_render produce identical scenes. */
    {
        SoSeparator *fRoot = ObolTest::Scenes::createBumpMap(256, 256);
        SbViewportRegion fVp(256, 256);
        SoOffscreenRenderer fRen(fVp);
        fRen.setComponents(SoOffscreenRenderer::RGB);
        fRen.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
        if (fRen.render(fRoot)) {
            fRen.writeToRGB(outpath);
        }
        fRoot->unref();
    }
    SbViewportRegion vp(W, H);
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 4.0f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 50.0f;
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.5f, -0.7f, -0.5f);
    light->intensity.setValue(1.0f);
    root->addChild(light);

    // Bump map node (normal map)
    SoBumpMap *bump = new SoBumpMap;
    buildNormalMap(bump);
    root->addChild(bump);

    // Bump map coordinate node (uses default auto-generation; just insert it)
    SoBumpMapCoordinate *bmc = new SoBumpMapCoordinate;
    root->addChild(bmc);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.5f, 0.7f, 1.0f);
    mat->specularColor.setValue(0.8f, 0.8f, 0.8f);
    mat->shininess.setValue(0.6f);
    root->addChild(mat);

    SoSphere *sph = new SoSphere;
    sph->radius.setValue(1.2f);
    root->addChild(sph);

    bool ok = renderer.render(root);
    int nb  = ok ? countNonBackground(renderer.getBuffer()) : 0;
    printf("render_bump_map: ok=%d nonbg=%d\n", ok, nb);
    renderer.writeToRGB(outpath);

    root->unref();

    if (!ok || nb < 100) {
        fprintf(stderr, "render_bump_map: FAIL – scene blank (nb=%d)\n", nb);
        return 1;
    }
    printf("render_bump_map: PASS\n");
    return 0;
}
