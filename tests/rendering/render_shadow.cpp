/*
 * render_shadow.cpp - Integration test: SoShadowGroup + SoShadowStyle
 *
 * Builds a scene inside an SoShadowGroup containing:
 *   - A shadow directional light positioned above the scene.
 *   - A white ground plane (SoFaceSet) marked SHADOWED.
 *   - A red sphere marked CASTS_SHADOW_AND_SHADOWED, suspended above the plane.
 *
 * The test first calls SoShadowGroup::isSupported() to probe the driver.
 *
 * Active shadow map rendering inside SoShadowGroup exercises GLSL shader
 * compilation that may crash on headless GLX pixmap contexts (multiple
 * GL contexts in the same process).  OSMesa handles multiple in-memory
 * contexts without these limitations.
 *
 * Pixel validation:
 *   - Primary check: lit geometry is visible (nonbg >= 100 pixels).
 *   - Shadow contrast check (informational): when SoShadowGroup::isSupported()
 *     returns TRUE the rendered image should contain both bright neutral
 *     ("lit ground") pixels and darker neutral ("shadow region") pixels.
 *     The shadow-contrast count is printed for diagnostic purposes; it does
 *     not gate the pass/fail result because GLSL variance shadow maps may
 *     produce subtly different results across headless GL drivers.
 *
 * For a GL-independent shadow test that performs strict shadow-region
 * validation, see render_nanort_shadow.cpp (NanoRT backend).
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
#include <Inventor/nodes/SoFaceSet.h>
#include <Inventor/nodes/SoNormal.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/annex/FXViz/nodes/SoShadowGroup.h>
#include <Inventor/annex/FXViz/nodes/SoShadowStyle.h>
#include <Inventor/annex/FXViz/nodes/SoShadowDirectionalLight.h>
#include <Inventor/SbViewportRegion.h>
#include <algorithm>
#include <cstdio>

static const int W = 256;
static const int H = 256;

// Primary validation: scene must have at least some non-background pixels.
static bool validateScene(const unsigned char *buf)
{
    int nonbg = 0;
    for (int i = 0; i < W * H; ++i) {
        const unsigned char *p = buf + i * 3;
        // Accept any pixel slightly above pure black; ambient-only lighting
        // is roughly 10/255, so use threshold of 5 to catch it.
        if (p[0] > 5 || p[1] > 5 || p[2] > 5) ++nonbg;
    }
    printf("render_shadow: nonbg=%d\n", nonbg);

    if (nonbg < 100) {
        fprintf(stderr, "render_shadow: FAIL – scene appears blank\n");
        return false;
    }
    printf("render_shadow: PASS\n");
    return true;
}

// Shadow-contrast check (informational): counts lit-ground and shadow-region
// pixels.  A neutral pixel (R ≈ G ≈ B) that is bright belongs to the lit
// ground; one that is dark but above background belongs to the shadow region.
// This does not gate pass/fail because GLSL shadow map rendering quality
// varies across headless GL drivers.  Use render_nanort_shadow for strict
// shadow-region validation.
static void reportShadowContrast(const unsigned char *buf)
{
    int litGround    = 0;
    int shadowGround = 0;
    for (int i = 0; i < W * H; ++i) {
        const unsigned char *p = buf + i * 3;
        int r = p[0], g = p[1], b = p[2];
        int hi = std::max({r, g, b});
        int lo = std::min({r, g, b});
        if (hi - lo > 40) continue;          // colored object – skip
        int luma = (r + g + b) / 3;
        if      (luma > 140) ++litGround;
        else if (luma >  10) ++shadowGround;
    }
    printf("render_shadow: shadow contrast – litGround=%d shadowGround=%d\n",
           litGround, shadowGround);
    if (litGround > 50 && shadowGround > 10)
        printf("render_shadow: shadow regions detected (shadows appear active)\n");
    else
        printf("render_shadow: shadow regions not detected "
               "(GLSL shadow maps may not be active on this driver)\n");
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    // Probe shadow support (exercises SoShadowGroup::isSupported)
    bool shadSupported = SoShadowGroup::isSupported();
    printf("render_shadow: SoShadowGroup::isSupported() = %d\n", (int)shadSupported);

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_shadow.rgb");

    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 3.0f, 5.0f);
    cam->orientation.setValue(SbVec3f(1.0f, 0.0f, 0.0f), -0.5f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 50.0f;
    root->addChild(cam);

    // SoShadowGroup wraps the geometry – exercises the shadow group
    // infrastructure (traversal, style elements, isSupported check).
    SoShadowGroup *sg = new SoShadowGroup;
    sg->isActive.setValue(TRUE);
    sg->intensity.setValue(0.7f);
    sg->precision.setValue(0.5f);
    sg->quality.setValue(1.0f);
    sg->smoothBorder.setValue(0.1f);
    root->addChild(sg);

    // SoShadowDirectionalLight is required for directional shadow casting
    // within SoShadowGroup (SoDirectionalLight does not cast shadows).
    SoShadowDirectionalLight *light = new SoShadowDirectionalLight;
    light->direction.setValue(-0.3f, -1.0f, -0.4f);
    light->intensity.setValue(1.0f);
    sg->addChild(light);

    // Ground plane – SHADOWED style
    {
        SoSeparator *planeSep = new SoSeparator;

        SoShadowStyle *ss = new SoShadowStyle;
        ss->style.setValue(SoShadowStyle::SHADOWED);
        planeSep->addChild(ss);

        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.8f, 0.8f, 0.8f);
        planeSep->addChild(mat);

        SoNormal *n = new SoNormal;
        n->vector.set1Value(0, SbVec3f(0.0f, 1.0f, 0.0f));
        planeSep->addChild(n);
        SoNormalBinding *nb = new SoNormalBinding;
        nb->value.setValue(SoNormalBinding::OVERALL);
        planeSep->addChild(nb);

        SoCoordinate3 *coords = new SoCoordinate3;
        coords->point.set1Value(0, SbVec3f(-3.0f, 0.0f, -3.0f));
        coords->point.set1Value(1, SbVec3f( 3.0f, 0.0f, -3.0f));
        coords->point.set1Value(2, SbVec3f( 3.0f, 0.0f,  3.0f));
        coords->point.set1Value(3, SbVec3f(-3.0f, 0.0f,  3.0f));
        planeSep->addChild(coords);

        SoFaceSet *fs = new SoFaceSet;
        fs->numVertices.setValue(4);
        planeSep->addChild(fs);

        sg->addChild(planeSep);
    }

    // Sphere – CASTS_SHADOW_AND_SHADOWED style
    {
        SoSeparator *sphSep = new SoSeparator;

        SoShadowStyle *ss = new SoShadowStyle;
        ss->style.setValue(SoShadowStyle::CASTS_SHADOW_AND_SHADOWED);
        sphSep->addChild(ss);

        SoTranslation *t = new SoTranslation;
        t->translation.setValue(0.0f, 1.2f, 0.0f);
        sphSep->addChild(t);

        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.8f, 0.2f, 0.2f);  // red
        mat->specularColor.setValue(0.7f, 0.7f, 0.7f);
        mat->shininess.setValue(0.5f);
        sphSep->addChild(mat);

        SoSphere *sph = new SoSphere;
        sph->radius.setValue(0.6f);
        sphSep->addChild(sph);

        sg->addChild(sphSep);
    }

    SbViewportRegion vp(W, H);
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    bool ok = false;
    if (renderer.render(root)) {
        const unsigned char *buf = renderer.getBuffer();
        bool pixOk = (buf != nullptr) && validateScene(buf);
        // Report shadow contrast (informational; does not gate pass/fail).
        if (buf && shadSupported) reportShadowContrast(buf);
        ok = pixOk && renderer.writeToRGB(outpath);
    } else {
        fprintf(stderr, "render_shadow: render() failed\n");
    }

    root->unref();
    return ok ? 0 : 1;
}
