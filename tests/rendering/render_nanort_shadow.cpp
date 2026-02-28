/*
 * render_nanort_shadow.cpp - NanoRT shadow casting demonstration and validation
 *
 * Builds a scene with SoRaytracingParams (shadowsEnabled=TRUE) containing:
 *   - A SoDirectionalLight shining from above
 *   - A white ground plane (SoFaceSet)
 *   - A red sphere suspended above the plane
 *
 * When rendered with the NanoRT context manager the shadow-ray mechanism fires
 * a secondary ray toward the light source for every illuminated surface point.
 * The sphere occludes those rays for the region of the ground directly below
 * it, producing a visible shadow.
 *
 * Pixel validation (OBOL_NANORT_BUILD path):
 *   - "Lit ground" pixels: bright neutral gray (R ≈ G ≈ B > 140) – ground
 *     illuminated by the directional light.
 *   - "Shadow ground" pixels: dark but non-zero neutral gray
 *     (10 < R ≈ G ≈ B < 100) – ground in the sphere's shadow where only the
 *     ambient fill contributes.
 *   - Both populations must be present for the test to PASS.
 *
 * SoRaytracingParams notes:
 *   - shadowsEnabled = TRUE  enables shadow rays in the nanort path.
 *   - ambientIntensity = 0.2 gives shadow regions ~14% of the ground color,
 *     i.e. ≈(36,36,36) for a diffuse=(0.7,0.7,0.7) surface, well above the
 *     black background (0,0,0) and clearly below the lit value of ≈(203,203,203).
 *   - The GL backend ignores SoRaytracingParams; renderToFile() is used there
 *     and the test passes without shadow validation (non-black geometry check).
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <Inventor/nodes/SoNormal.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoRaytracingParams.h>
#include <Inventor/SbViewportRegion.h>
#include <algorithm>
#include <cstdio>

static const int W = 256;
static const int H = 256;

// ---------------------------------------------------------------------------
// validateShadow: checks for both lit-ground and shadow-ground pixels.
//
// Ground plane: diffuse (0.7, 0.7, 0.7) → neutral gray.
//   Lit   : ambient*diff + NdotL*diff ≈ 0.2*0.7 + ~0.96*0.7 ≈ 0.81 → ~207/255
//   Shadow: ambient*diff             ≈ 0.2*0.7              ≈ 0.14 →  ~36/255
//
// A pixel is "neutral" when its three channels are within 30 of each other
// (rules out the colored sphere and the black background).
// ---------------------------------------------------------------------------
static bool validateShadow(const unsigned char *buf)
{
    int litGround    = 0;
    int shadowGround = 0;

    for (int i = 0; i < W * H; ++i) {
        const unsigned char *p = buf + i * 3;
        int r = p[0], g = p[1], b = p[2];

        // Neutral pixel: channels close to each other → ground or background,
        // not the red sphere.
        int hi = std::max({r, g, b});
        int lo = std::min({r, g, b});
        if (hi - lo > 40) continue;          // colored – skip

        int luma = (r + g + b) / 3;
        if      (luma > 140) ++litGround;    // well-lit ground
        else if (luma >  10) ++shadowGround; // shadow region (above background)
    }

    printf("render_nanort_shadow: litGround=%d shadowGround=%d\n",
           litGround, shadowGround);

    if (litGround < 100) {
        fprintf(stderr,
                "render_nanort_shadow: FAIL – too few lit-ground pixels (%d)\n",
                litGround);
        return false;
    }
    if (shadowGround < 20) {
        fprintf(stderr,
                "render_nanort_shadow: FAIL – too few shadow pixels (%d); "
                "shadows may not be rendering\n", shadowGround);
        return false;
    }
    printf("render_nanort_shadow: PASS\n");
    return true;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_nanort_shadow.rgb");

    // -----------------------------------------------------------------------
    // Scene graph
    // -----------------------------------------------------------------------
    SoSeparator *root = new SoSeparator;
    root->ref();

    // --- Raytracing hints: enable shadow rays -------------------------------
    SoRaytracingParams *rtParams = new SoRaytracingParams;
    rtParams->shadowsEnabled.setValue(TRUE);
    rtParams->ambientIntensity.setValue(0.2f);
    root->addChild(rtParams);

    // --- Camera (perspective, slightly above looking down) ------------------
    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 5.0f, 8.0f);
    cam->orientation.setValue(SbVec3f(1.0f, 0.0f, 0.0f), -0.5f);
    cam->nearDistance = 0.5f;
    cam->farDistance  = 50.0f;
    root->addChild(cam);

    // --- Directional light shining steeply from above-left ------------------
    // Direction (-0.3, -1.0, 0.0) → normalised ≈ (-0.287, -0.958, 0)
    // The ground normal is (0,1,0), so NdotL ≈ 0.958 (very bright lit ground).
    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.3f, -1.0f, 0.0f);
    light->intensity.setValue(1.0f);
    root->addChild(light);

    // --- Ground plane: white quad at y = -1.0 --------------------------------
    {
        SoSeparator *plane = new SoSeparator;

        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.7f, 0.7f, 0.7f);
        mat->specularColor.setValue(0.1f, 0.1f, 0.1f);
        mat->shininess.setValue(0.05f);
        plane->addChild(mat);

        SoNormal *n = new SoNormal;
        n->vector.set1Value(0, SbVec3f(0.0f, 1.0f, 0.0f));
        plane->addChild(n);

        SoNormalBinding *nb = new SoNormalBinding;
        nb->value.setValue(SoNormalBinding::OVERALL);
        plane->addChild(nb);

        SoCoordinate3 *coords = new SoCoordinate3;
        const float gY = -1.0f;
        coords->point.set1Value(0, SbVec3f(-5.0f, gY, -5.0f));
        coords->point.set1Value(1, SbVec3f( 5.0f, gY, -5.0f));
        coords->point.set1Value(2, SbVec3f( 5.0f, gY,  5.0f));
        coords->point.set1Value(3, SbVec3f(-5.0f, gY,  5.0f));
        plane->addChild(coords);

        SoIndexedFaceSet *ifs = new SoIndexedFaceSet;
        static const int32_t idx[] = { 0, 1, 2, 3, SO_END_FACE_INDEX };
        ifs->coordIndex.setValues(0, 5, idx);
        plane->addChild(ifs);

        root->addChild(plane);
    }

    // --- Red sphere: suspended 0.5 units above the ground ------------------
    {
        SoSeparator *sph = new SoSeparator;

        SoTranslation *t = new SoTranslation;
        t->translation.setValue(0.0f, 0.3f, 0.0f);  // sphere bottom at y=-0.5, which is 0.5 units above ground plane (y=-1.0)
        sph->addChild(t);

        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.85f, 0.15f, 0.15f);
        mat->specularColor.setValue(0.7f, 0.7f, 0.7f);
        mat->shininess.setValue(0.5f);
        sph->addChild(mat);

        SoSphere *sphere = new SoSphere;
        sphere->radius.setValue(0.8f);
        sph->addChild(sphere);

        root->addChild(sph);
    }

    // -----------------------------------------------------------------------
    // Render and validate
    // -----------------------------------------------------------------------
    bool ok = false;

#ifdef OBOL_NANORT_BUILD
    // NanoRT path: pixel-level shadow validation.
    SoOffscreenRenderer renderer(SbViewportRegion(W, H));
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    if (renderer.render(root)) {
        const unsigned char *buf = renderer.getBuffer();
        bool pixOk = (buf != nullptr) && validateShadow(buf);
        ok = pixOk && renderer.writeToRGB(outpath);
    } else {
        fprintf(stderr, "render_nanort_shadow: render() failed\n");
    }
#else
    // GL path: no shadow validation (SoRaytracingParams is ignored by GL).
    // Just verify the scene renders non-blank geometry.
    SoOffscreenRenderer renderer(SbViewportRegion(W, H));
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    if (renderer.render(root)) {
        const unsigned char *buf = renderer.getBuffer();
        if (buf) {
            int nonbg = 0;
            for (int i = 0; i < W * H; ++i) {
                const unsigned char *p = buf + i * 3;
                if (p[0] > 5 || p[1] > 5 || p[2] > 5) ++nonbg;
            }
            printf("render_nanort_shadow (GL): nonbg=%d\n", nonbg);
            ok = (nonbg >= 100) && renderer.writeToRGB(outpath);
        }
    } else {
        fprintf(stderr, "render_nanort_shadow: render() failed\n");
    }
#endif

    root->unref();
    return ok ? 0 : 1;
}
