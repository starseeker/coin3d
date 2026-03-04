/*
 * render_embree.cpp
 *
 * Intel Embree 4 raytracing backend demonstration for Obol/Coin scene graphs.
 *
 * This test demonstrates the SoRaytracerSceneCollector generic API together
 * with the SoEmbreeContextManager backend.  Scene collection (geometry,
 * lights, proxy shapes, text overlays) is handled entirely by the Obol
 * library; only the BVH build and ray intersection use Embree.
 *
 * Obol APIs exercised:
 *   - SoRaytracerSceneCollector   (generic scene collection + compositing)
 *   - SoRaytracingParams          (shadows, reflections, AA, ambient fill)
 *   - SoOffscreenRenderer         (dispatches to SoEmbreeContextManager)
 *   - SbViewportRegion / SbViewVolume (camera math)
 *
 * The test passes if:
 *   - Triangle extraction yields > 0 triangles
 *   - At least one light is extracted
 *   - The rendered image has >= 1% non-background pixels
 *   - The RGB file is written without error
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoPointLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoRaytracingParams.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoRaytracerSceneCollector.h>

#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// Validate that the rendered image contains coloured geometry pixels.
// Background is black; we check for non-trivially bright pixels.
// ---------------------------------------------------------------------------
static bool validatePixels(const unsigned char * buf, int w, int h)
{
    int coloured = 0;
    for (int i = 0; i < w * h; ++i) {
        const unsigned char * p = buf + i * 3;
        if (p[0] > 20 || p[1] > 20 || p[2] > 20)
            ++coloured;
    }
    const double pct = 100.0 * coloured / (w * h);
    printf("render_embree: coloured pixels = %d (%.1f%%)\n", coloured, pct);
    return pct >= 1.0;
}

int main(int argc, char ** argv)
{
    initCoinHeadless();

    // ---- Build scene -------------------------------------------------------
    SoSeparator * root = new SoSeparator;
    root->ref();

    // Raytracing hints: enable shadows for a more interesting image
    SoRaytracingParams * rtParams = new SoRaytracingParams;
    rtParams->shadowsEnabled.setValue(TRUE);
    rtParams->ambientIntensity.setValue(0.15f);
    root->addChild(rtParams);

    SoPerspectiveCamera * cam = new SoPerspectiveCamera;
    root->addChild(cam);

    SoDirectionalLight * dirLight = new SoDirectionalLight;
    dirLight->direction.setValue(-1.0f, -1.5f, -1.0f);
    dirLight->intensity.setValue(0.9f);
    root->addChild(dirLight);

    SoPointLight * ptLight = new SoPointLight;
    ptLight->location.setValue(3.0f, 3.0f, 3.0f);
    ptLight->intensity.setValue(0.5f);
    root->addChild(ptLight);

    // Red sphere
    {
        SoSeparator * sep = new SoSeparator;
        SoTranslation * t = new SoTranslation;
        t->translation.setValue(-2.0f, 0.0f, 0.0f);
        sep->addChild(t);
        SoMaterial * mat = new SoMaterial;
        mat->diffuseColor .setValue(0.9f, 0.1f, 0.1f);
        mat->specularColor.setValue(0.6f, 0.6f, 0.6f);
        mat->shininess    .setValue(0.5f);
        sep->addChild(mat);
        sep->addChild(new SoSphere);
        root->addChild(sep);
    }

    // Green cube
    {
        SoSeparator * sep = new SoSeparator;
        SoMaterial * mat = new SoMaterial;
        mat->diffuseColor.setValue(0.1f, 0.8f, 0.1f);
        sep->addChild(mat);
        sep->addChild(new SoCube);
        root->addChild(sep);
    }

    // Blue cylinder
    {
        SoSeparator * sep = new SoSeparator;
        SoTranslation * t = new SoTranslation;
        t->translation.setValue(2.0f, 0.0f, 0.0f);
        sep->addChild(t);
        SoMaterial * mat = new SoMaterial;
        mat->diffuseColor .setValue(0.1f, 0.3f, 0.9f);
        mat->specularColor.setValue(0.4f, 0.4f, 0.4f);
        mat->shininess    .setValue(0.3f);
        sep->addChild(mat);
        sep->addChild(new SoCylinder);
        root->addChild(sep);
    }

    // Gold cone
    {
        SoSeparator * sep = new SoSeparator;
        SoTranslation * t = new SoTranslation;
        t->translation.setValue(0.0f, 2.0f, 0.0f);
        sep->addChild(t);
        SoMaterial * mat = new SoMaterial;
        mat->diffuseColor .setValue(0.85f, 0.70f, 0.10f);
        mat->specularColor.setValue(0.7f, 0.7f, 0.3f);
        mat->shininess    .setValue(0.7f);
        sep->addChild(mat);
        sep->addChild(new SoCone);
        root->addChild(sep);
    }

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    cam->viewAll(root, vp);
    cam->position.setValue(cam->position.getValue() * 1.2f);

    // ---- Validate scene collection via the generic API ---------------------
    // This exercises SoRaytracerSceneCollector directly so we can print
    // statistics about what was collected — independent of which intersection
    // backend (Embree, nanort, …) actually renders the scene.
    {
        SoRaytracerSceneCollector collector;
        collector.collect(root, vp);

        printf("render_embree: collected %zu triangles, %zu lights, %zu overlays\n",
               collector.getTriangles().size(),
               collector.getLights().size(),
               collector.getOverlays().size());

        if (collector.getTriangles().empty()) {
            fprintf(stderr, "render_embree: FAIL — no triangles collected\n");
            root->unref();
            return 1;
        }
        if (collector.getLights().empty()) {
            fprintf(stderr, "render_embree: FAIL — no lights collected\n");
            root->unref();
            return 1;
        }
    }

    // ---- Render via Embree context manager ---------------------------------
    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_embree.rgb");

#ifdef OBOL_EMBREE_BUILD
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    if (!renderer.render(root)) {
        fprintf(stderr, "render_embree: render() failed\n");
        root->unref();
        return 1;
    }

    const unsigned char * buf = renderer.getBuffer();
    if (!buf || !validatePixels(buf, DEFAULT_WIDTH, DEFAULT_HEIGHT)) {
        fprintf(stderr, "render_embree: FAIL — image is empty or blank\n");
        root->unref();
        return 1;
    }

    if (!renderer.writeToRGB(outpath)) {
        fprintf(stderr, "render_embree: FAIL — could not write %s\n", outpath);
        root->unref();
        return 1;
    }

    printf("render_embree: PASS — wrote %s\n", outpath);
#else
    // GL/OSMesa path: basic non-blank check only (no pixel validation)
    if (!renderToFile(root, outpath)) {
        root->unref();
        return 1;
    }
#endif

    root->unref();
    return 0;
}
