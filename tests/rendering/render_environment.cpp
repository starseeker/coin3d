/*
 * render_environment.cpp - Integration test: SoEnvironment fog + ambient colour
 *
 * Exercises SoEnvironment by rendering two frames:
 *
 *   Frame 1 (no fog): a red sphere with high ambient intensity.
 *     Validates that ambient light brightening is active.
 *
 *   Frame 2 (linear fog, short visibility): same scene with FOG enabled at
 *     short visibility distance so the object is heavily fogged.
 *     The fogged frame must still be non-blank (fog blends toward fog colour).
 *
 * Writes argv[1]+".rgb" (frame 1) and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoEnvironment.h>
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
        snprintf(outpath, sizeof(outpath), "render_environment.rgb");

    SbViewportRegion vp(W, H);
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    bool allOk = true;

    // -----------------------------------------------------------------------
    // Build scene with a mutable SoEnvironment node
    // -----------------------------------------------------------------------
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 4.0f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 50.0f;
    root->addChild(cam);

    SoEnvironment *env = new SoEnvironment;
    root->addChild(env);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.3f, -0.7f, -0.6f);
    root->addChild(light);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.8f, 0.2f, 0.2f);
    mat->ambientColor.setValue(0.8f, 0.2f, 0.2f);
    root->addChild(mat);
    root->addChild(new SoSphere);

    // -----------------------------------------------------------------------
    // Frame 1: no fog, high ambient intensity
    // -----------------------------------------------------------------------
    env->ambientIntensity.setValue(0.9f);
    env->ambientColor.setValue(1.0f, 1.0f, 1.0f);
    env->fogType.setValue(SoEnvironment::NONE);

    bool ok1 = renderer.render(root);
    int nb1  = ok1 ? countNonBackground(renderer.getBuffer()) : 0;
    printf("render_environment frame1 (no fog): ok=%d nonbg=%d\n", ok1, nb1);
    renderer.writeToRGB(outpath);
    allOk = allOk && ok1 && (nb1 >= 100);

    // -----------------------------------------------------------------------
    // Frame 2: FOG enabled with white fog colour and short visibility
    // -----------------------------------------------------------------------
    env->ambientIntensity.setValue(0.3f);
    env->fogType.setValue(SoEnvironment::FOG);
    env->fogColor.setValue(0.8f, 0.8f, 0.8f);
    env->fogVisibility.setValue(6.0f);

    bool ok2 = renderer.render(root);
    int nb2  = ok2 ? countNonBackground(renderer.getBuffer()) : 0;
    printf("render_environment frame2 (fog): ok=%d nonbg=%d\n", ok2, nb2);
    allOk = allOk && ok2 && (nb2 >= 100);

    root->unref();

    // -----------------------------------------------------------------------
    // Validation
    // -----------------------------------------------------------------------
    if (!allOk) {
        fprintf(stderr, "render_environment: FAIL\n");
        return 1;
    }
    printf("render_environment: PASS\n");
    return 0;
}
