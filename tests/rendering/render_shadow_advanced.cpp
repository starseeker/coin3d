/*
 * render_shadow_advanced.cpp — Extended SoShadowGroup coverage test
 *
 * Exercises previously uncovered paths in the shadows subsystem:
 *   1. SoShadowSpotLight as the shadow-casting light source
 *   2. Multiple shadow-casting objects (sphere + cone + cube)
 *   3. SoShadowGroup::visibilityRadius / visibilityNearRadius fields
 *   4. SoShadowGroup::epsilon / threshold fields
 *   5. SoShadowGroup::shadowCachingEnabled field toggle
 *   6. SoShadowGroup::enableSubgraphSearchOnNotify
 *   7. SoShadowStyle::NO_SHADOWING (objects that ignore shadows)
 *   8. SoShadowStyle::CASTS_SHADOW_AND_SHADOWED (full participation)
 *   9. SoShadowCulling node
 *  10. isActive=FALSE path (scene still renders without shadows)
 *
 * Returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoSpotLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoFaceSet.h>
#include <Inventor/nodes/SoNormal.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/annex/FXViz/nodes/SoShadowGroup.h>
#include <Inventor/annex/FXViz/nodes/SoShadowStyle.h>
#include <Inventor/annex/FXViz/nodes/SoShadowDirectionalLight.h>
#include <Inventor/annex/FXViz/nodes/SoShadowSpotLight.h>
#include <Inventor/annex/FXViz/nodes/SoShadowCulling.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>

static const int W = 256;
static const int H = 256;

static bool validateNonBlack(const unsigned char *buf, int npix,
                              const char *label)
{
    int nonbg = 0;
    for (int i = 0; i < npix; ++i) {
        const unsigned char *p = buf + i * 3;
        if (p[0] > 5 || p[1] > 5 || p[2] > 5) ++nonbg;
    }
    printf("  %s: nonbg=%d\n", label, nonbg);
    if (nonbg < 50) {
        fprintf(stderr, "  FAIL %s: scene appears blank\n", label);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Build a simple ground plane
// ---------------------------------------------------------------------------
static SoSeparator *buildGroundPlane()
{
    static const float pts[4][3] = {
        {-4.0f, -1.5f, -4.0f},
        { 4.0f, -1.5f, -4.0f},
        { 4.0f, -1.5f,  4.0f},
        {-4.0f, -1.5f,  4.0f}
    };
    static const int verts[] = { 4 };

    SoSeparator *sep = new SoSeparator;

    SoNormal *n = new SoNormal;
    n->vector.set1Value(0, SbVec3f(0.0f, 1.0f, 0.0f));
    sep->addChild(n);

    SoNormalBinding *nb = new SoNormalBinding;
    nb->value.setValue(SoNormalBinding::OVERALL);
    sep->addChild(nb);

    SoCoordinate3 *c3 = new SoCoordinate3;
    c3->point.setValues(0, 4, pts);
    sep->addChild(c3);

    SoFaceSet *fs = new SoFaceSet;
    fs->numVertices.setValues(0, 1, verts);
    sep->addChild(fs);

    return sep;
}

// ---------------------------------------------------------------------------
// Test 1: SoShadowSpotLight as shadow light source
// ---------------------------------------------------------------------------
static bool test1_shadowSpotLight(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 4.0f, 7.0f);
    cam->orientation.setValue(SbVec3f(1.0f, 0.0f, 0.0f), -0.5f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 50.0f;
    root->addChild(cam);

    SoShadowGroup *sg = new SoShadowGroup(getCoinHeadlessContextManager());
    sg->isActive.setValue(TRUE);
    sg->intensity.setValue(0.8f);
    sg->precision.setValue(0.5f);
    sg->quality.setValue(1.0f);
    sg->smoothBorder.setValue(0.2f);
    sg->visibilityRadius.setValue(15.0f);
    sg->visibilityNearRadius.setValue(0.1f);
    sg->epsilon.setValue(0.001f);
    sg->threshold.setValue(0.1f);
    sg->shadowCachingEnabled.setValue(FALSE);
    root->addChild(sg);

    // Enable subgraph search on notify
    sg->enableSubgraphSearchOnNotify(TRUE);

    // SoShadowSpotLight instead of SoShadowDirectionalLight
    SoShadowSpotLight *spot = new SoShadowSpotLight;
    spot->location.setValue(0.0f, 5.0f, 3.0f);
    spot->direction.setValue(0.0f, -1.0f, -0.5f);
    spot->cutOffAngle.setValue(0.6f);
    spot->dropOffRate.setValue(0.3f);
    spot->intensity.setValue(1.0f);
    spot->nearDistance.setValue(0.5f);
    spot->farDistance.setValue(20.0f);
    sg->addChild(spot);

    // Ground plane (SHADOWED)
    {
        SoSeparator *planeSep = new SoSeparator;
        SoShadowStyle *ss = new SoShadowStyle;
        ss->style.setValue(SoShadowStyle::SHADOWED);
        planeSep->addChild(ss);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.8f, 0.8f, 0.7f);
        planeSep->addChild(mat);
        planeSep->addChild(buildGroundPlane());
        sg->addChild(planeSep);
    }

    // Sphere casting shadow
    {
        SoSeparator *sep = new SoSeparator;
        SoShadowStyle *ss = new SoShadowStyle;
        ss->style.setValue(SoShadowStyle::CASTS_SHADOW_AND_SHADOWED);
        sep->addChild(ss);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.8f, 0.2f, 0.2f);
        sep->addChild(mat);
        SoTranslation *tr = new SoTranslation;
        tr->translation.setValue(-1.5f, 0.5f, 0.0f);
        sep->addChild(tr);
        sep->addChild(new SoSphere);
        sg->addChild(sep);
    }

    // Cone (NO_SHADOWING: it doesn't cast or receive shadows)
    {
        SoSeparator *sep = new SoSeparator;
        SoShadowStyle *ss = new SoShadowStyle;
        ss->style.setValue(SoShadowStyle::NO_SHADOWING);
        sep->addChild(ss);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.2f, 0.8f, 0.2f);
        sep->addChild(mat);
        SoTranslation *tr = new SoTranslation;
        tr->translation.setValue(1.5f, 0.5f, 0.0f);
        sep->addChild(tr);
        sep->addChild(new SoCone);
        sg->addChild(sep);
    }

    char outpath[1024];
    snprintf(outpath, sizeof(outpath), "%s_spot.rgb", basepath);
    bool ok = renderToFile(root, outpath, W, H);

    if (ok) {
        SoOffscreenRenderer *ren = getSharedRenderer();
        ren->setViewportRegion(SbViewportRegion(W, H));
        ok = ren->render(root);
        if (ok)
            ok = validateNonBlack(ren->getBuffer(), W * H, "test1_shadowSpotLight");
    }

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 2: SoShadowDirectionalLight + multiple shadow casters + SoShadowCulling
// ---------------------------------------------------------------------------
static bool test2_multiCasters(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 5.0f, 10.0f);
    cam->orientation.setValue(SbVec3f(1.0f, 0.0f, 0.0f), -0.45f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 60.0f;
    root->addChild(cam);

    SoShadowGroup *sg = new SoShadowGroup(getCoinHeadlessContextManager());
    sg->isActive.setValue(TRUE);
    sg->intensity.setValue(0.7f);
    sg->precision.setValue(0.3f);
    sg->quality.setValue(1.0f);
    sg->smoothBorder.setValue(0.0f);
    sg->shadowCachingEnabled.setValue(TRUE);
    root->addChild(sg);

    // SoShadowCulling to control which objects cast shadows
    SoShadowCulling *sc = new SoShadowCulling;
    sc->mode.setValue(SoShadowCulling::NO_CULLING);
    sg->addChild(sc);

    SoShadowDirectionalLight *dlight = new SoShadowDirectionalLight;
    dlight->direction.setValue(-0.4f, -1.0f, -0.3f);
    dlight->intensity.setValue(0.9f);
    sg->addChild(dlight);

    // Ground
    {
        SoSeparator *planeSep = new SoSeparator;
        SoShadowStyle *ss = new SoShadowStyle;
        ss->style.setValue(SoShadowStyle::SHADOWED);
        planeSep->addChild(ss);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.7f, 0.7f, 0.6f);
        planeSep->addChild(mat);
        planeSep->addChild(buildGroundPlane());
        sg->addChild(planeSep);
    }

    // Three shapes casting shadows
    float xpos[3] = { -2.5f, 0.0f, 2.5f };
    SoNode *shapes[3] = { new SoSphere, new SoCube, new SoCone };
    float colors[3][3] = {
        {0.8f, 0.2f, 0.2f},
        {0.2f, 0.5f, 0.9f},
        {0.9f, 0.6f, 0.1f}
    };

    for (int i = 0; i < 3; ++i) {
        SoSeparator *sep = new SoSeparator;
        SoShadowStyle *ss = new SoShadowStyle;
        ss->style.setValue(SoShadowStyle::CASTS_SHADOW_AND_SHADOWED);
        sep->addChild(ss);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(colors[i][0], colors[i][1], colors[i][2]);
        sep->addChild(mat);
        SoTranslation *tr = new SoTranslation;
        tr->translation.setValue(xpos[i], 0.5f, 0.0f);
        sep->addChild(tr);
        sep->addChild(shapes[i]);
        sg->addChild(sep);
    }

    char outpath[1024];
    snprintf(outpath, sizeof(outpath), "%s_multi.rgb", basepath);
    bool ok = renderToFile(root, outpath, W, H);

    if (ok) {
        SoOffscreenRenderer *ren = getSharedRenderer();
        ren->setViewportRegion(SbViewportRegion(W, H));
        ok = ren->render(root);
        if (ok)
            ok = validateNonBlack(ren->getBuffer(), W * H, "test2_multiCasters");
    }

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 3: isActive=FALSE — scene renders without shadows (no crash)
// ---------------------------------------------------------------------------
static bool test3_shadowDisabled(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 3.0f, 8.0f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 50.0f;
    root->addChild(cam);

    SoShadowGroup *sg = new SoShadowGroup(getCoinHeadlessContextManager());
    sg->isActive.setValue(FALSE); // disabled
    root->addChild(sg);

    SoShadowDirectionalLight *dl = new SoShadowDirectionalLight;
    dl->direction.setValue(-0.3f, -1.0f, -0.4f);
    sg->addChild(dl);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.6f, 0.8f, 0.3f);
    sg->addChild(mat);
    sg->addChild(new SoSphere);

    SbViewportRegion vp(W, H);
    cam->viewAll(root, vp);

    char outpath[1024];
    snprintf(outpath, sizeof(outpath), "%s_disabled.rgb", basepath);
    bool ok = renderToFile(root, outpath, W, H);

    if (ok) {
        SoOffscreenRenderer *ren = getSharedRenderer();
        ren->setViewportRegion(SbViewportRegion(W, H));
        ok = ren->render(root);
        if (ok)
            ok = validateNonBlack(ren->getBuffer(), W * H, "test3_shadowDisabled");
    }

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 4: Shadow caching toggle (covers shadowCachingEnabled notification path)
// ---------------------------------------------------------------------------
static bool test4_cachingToggle(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 4.0f, 8.0f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 50.0f;
    root->addChild(cam);

    SoShadowGroup *sg = new SoShadowGroup(getCoinHeadlessContextManager());
    sg->isActive.setValue(TRUE);
    sg->intensity.setValue(0.6f);
    sg->precision.setValue(0.4f);
    sg->shadowCachingEnabled.setValue(TRUE);
    root->addChild(sg);

    SoShadowDirectionalLight *dl = new SoShadowDirectionalLight;
    dl->direction.setValue(-0.3f, -1.0f, -0.4f);
    dl->intensity.setValue(1.0f);
    sg->addChild(dl);

    SoShadowStyle *ss = new SoShadowStyle;
    ss->style.setValue(SoShadowStyle::CASTS_SHADOW_AND_SHADOWED);
    sg->addChild(ss);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.5f, 0.7f, 0.9f);
    sg->addChild(mat);
    sg->addChild(new SoCube);

    SbViewportRegion vp(W, H);
    cam->viewAll(root, vp);

    // Render with caching ON
    SoOffscreenRenderer *ren = getSharedRenderer();
    ren->setViewportRegion(vp);
    bool ok = ren->render(root);
    if (!ok) { root->unref(); return false; }

    // Toggle caching (exercises notify path)
    sg->shadowCachingEnabled.setValue(FALSE);
    ok = ren->render(root);
    if (!ok) { root->unref(); return false; }

    // And toggle back
    sg->shadowCachingEnabled.setValue(TRUE);
    ok = ren->render(root);

    char outpath[1024];
    snprintf(outpath, sizeof(outpath), "%s_caching.rgb", basepath);
    renderToFile(root, outpath, W, H);

    if (ok)
        ok = validateNonBlack(ren->getBuffer(), W * H, "test4_cachingToggle");

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char **argv)
{
    initCoinHeadless();

    const char *basepath = (argc > 1) ? argv[1] : "render_shadow_advanced";

    SoShadowGroup * sg = new SoShadowGroup(getCoinHeadlessContextManager());
    sg->ref();
    printf("SoShadowGroup::isSupported() = %d\n",
           (int)sg->isSupported());
    sg->unref();

    int failures = 0;

    printf("\n=== Extended SoShadowGroup tests ===\n");

    if (!test1_shadowSpotLight(basepath)) ++failures;
    if (!test2_multiCasters(basepath))    ++failures;
    if (!test3_shadowDisabled(basepath))  ++failures;
    if (!test4_cachingToggle(basepath))   ++failures;

    printf("\n=== Summary: %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
