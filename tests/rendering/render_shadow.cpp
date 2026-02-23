/*
 * render_shadow.cpp - Integration test: SoShadowGroup + SoShadowStyle
 *
 * Builds a scene inside an SoShadowGroup containing:
 *   - A directional light positioned above the scene.
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
 * Pixel validation only requires that lit geometry is visible; it does
 * not check for actual shadow regions.
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
#include <Inventor/SbViewportRegion.h>
#include <cstdio>

static const int W = 256;
static const int H = 256;

static bool validateScene(const unsigned char *buf)
{
    int nonbg = 0;
    for (int i = 0; i < W * H; ++i) {
        const unsigned char *p = buf + i * 3;
        if (p[0] > 20 || p[1] > 20 || p[2] > 20) ++nonbg;
    }
    printf("render_shadow: nonbg=%d\n", nonbg);

    if (nonbg < 100) {
        fprintf(stderr, "render_shadow: FAIL – scene appears blank\n");
        return false;
    }
    printf("render_shadow: PASS\n");
    return true;
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

    // Regular directional light
    SoDirectionalLight *light = new SoDirectionalLight;
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
    renderer.setBackgroundColor(SbColor(0.05f, 0.05f, 0.1f));

    bool ok = false;
    if (renderer.render(root)) {
        const unsigned char *buf = renderer.getBuffer();
        bool pixOk = (buf != nullptr) && validateScene(buf);
        ok = pixOk && renderer.writeToRGB(outpath);
    } else {
        fprintf(stderr, "render_shadow: render() failed\n");
    }

    root->unref();
    return ok ? 0 : 1;
}
