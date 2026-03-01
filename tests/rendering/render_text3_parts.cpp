/*
 * render_text3_parts.cpp — SoText3 comprehensive coverage test
 *
 * Exercises all SoText3 rendering paths:
 *   1. FRONT part (default) with all three justifications in one scene
 *   2. SIDES part (no geometry without profile)
 *   3. BACK part
 *   4. ALL parts
 *   5. spacing field (multi-line text)
 *   6. SoText3 bounding box computation
 *
 * Returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoFont.h>
#include <Inventor/nodes/SoText3.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>
#include <cmath>

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
    return nonbg >= 10;
}

// ---------------------------------------------------------------------------
// Test 1: Single scene with FRONT texts using all three justifications
// ---------------------------------------------------------------------------
static bool test1_allJustifications(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 15.0f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 80.0f;
    root->addChild(cam);

    SoDirectionalLight *dl = new SoDirectionalLight;
    dl->direction.setValue(-0.3f, -0.5f, -0.8f);
    root->addChild(dl);

    // Use a small font so all three rows fit without overlapping.
    // Default font size is 10 units but the y-translations are only 3 units
    // apart, so a font size of 2 keeps each row within ~1.5 units tall.
    SoFont *fnt = new SoFont;
    fnt->size.setValue(2.0f);
    root->addChild(fnt);

    float ys[3] = { 3.0f, 0.0f, -3.0f };
    SoText3::Justification justs[3] = {
        SoText3::LEFT, SoText3::CENTER, SoText3::RIGHT
    };
    const char *strs[3] = { "Left", "Center", "Right" };
    float colors[3][3] = {
        {0.9f, 0.3f, 0.3f},
        {0.3f, 0.9f, 0.3f},
        {0.3f, 0.3f, 0.9f}
    };

    for (int i = 0; i < 3; ++i) {
        SoSeparator *sep = new SoSeparator;
        SoTranslation *tr = new SoTranslation;
        tr->translation.setValue(0.0f, ys[i], 0.0f);
        sep->addChild(tr);
        SoMaterial *m = new SoMaterial;
        m->diffuseColor.setValue(colors[i][0], colors[i][1], colors[i][2]);
        sep->addChild(m);
        SoText3 *t = new SoText3;
        t->string.setValue(strs[i]);
        t->justification.setValue(justs[i]);
        t->parts.setValue(SoText3::FRONT);
        sep->addChild(t);
        root->addChild(sep);
    }

    SbViewportRegion vp(W, H);
    cam->viewAll(root, vp);

    char outpath[1024];
    snprintf(outpath, sizeof(outpath), "%s_justifications.rgb", basepath);
    bool ok = renderToFile(root, outpath, W, H);

    if (ok) {
        SoOffscreenRenderer *ren = getSharedRenderer();
        ren->setViewportRegion(vp);
        ok = ren->render(root) &&
             validateNonBlack(ren->getBuffer(), W*H, "test1_allJustifications");
    }

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 2: SIDES + BACK + ALL parts - all in one scene
// ---------------------------------------------------------------------------
static bool test2_allParts(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(2.0f, 2.0f, 15.0f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 80.0f;
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    // Use a small font so the three rows don't overlap.
    // Default font size (~10 units tall) exceeds the 3-unit y-spacing, causing
    // the red/green/dark AB rows to blend into each other.
    SoFont *fnt = new SoFont;
    fnt->size.setValue(2.0f);
    root->addChild(fnt);

    // Three "AB" text nodes demonstrating SIDES, BACK and ALL parts.
    // Red  = SIDES only  (extruded side faces; edge-on to viewer so NdotL≈0,
    //                     appears as dark outline regardless of material colour).
    // Green = BACK only  (back face normal points away from camera; NdotL=0
    //                     from the front, so it appears as a dark ambient row).
    // Blue  = ALL parts  (front + sides + back; front face is fully lit and
    //                     clearly visible as a bright blue AB).
    const int partsValues[3] = {
        SoText3::SIDES,
        SoText3::BACK,
        SoText3::ALL
    };
    float ys[3] = { 3.0f, 0.0f, -3.0f };
    float colors[3][3] = {
        {0.9f, 0.3f, 0.3f},   // red   – SIDES (dark from front, edge-on normals)
        {0.3f, 0.9f, 0.3f},   // green – BACK  (dark from front, back-facing normal)
        {0.3f, 0.3f, 0.9f}    // blue  – ALL   (front face bright; sides+back dark)
    };

    for (int i = 0; i < 3; ++i) {
        SoSeparator *sep = new SoSeparator;
        SoTranslation *tr = new SoTranslation;
        tr->translation.setValue(0.0f, ys[i], 0.0f);
        sep->addChild(tr);
        SoMaterial *m = new SoMaterial;
        m->diffuseColor.setValue(colors[i][0], colors[i][1], colors[i][2]);
        sep->addChild(m);
        SoText3 *t = new SoText3;
        t->string.setValue("AB");
        t->justification.setValue(SoText3::CENTER);
        t->parts.setValue(partsValues[i]);
        sep->addChild(t);
        root->addChild(sep);
    }

    SbViewportRegion vp(W, H);
    cam->viewAll(root, vp);

    char outpath[1024];
    snprintf(outpath, sizeof(outpath), "%s_parts.rgb", basepath);
    bool ok = renderToFile(root, outpath, W, H);

    if (ok) {
        SoOffscreenRenderer *ren = getSharedRenderer();
        ren->setViewportRegion(vp);
        ok = ren->render(root) &&
             validateNonBlack(ren->getBuffer(), W*H, "test2_allParts");
    }

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 3: Multi-line text with spacing field
// ---------------------------------------------------------------------------
static bool test3_multilineSpacing(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 12.0f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 60.0f;
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.8f, 0.6f, 0.2f);
    root->addChild(mat);

    SoText3 *t = new SoText3;
    t->string.set1Value(0, "Line 1");
    t->string.set1Value(1, "Line 2");
    t->spacing.setValue(1.5f);
    t->parts.setValue(SoText3::FRONT);
    t->justification.setValue(SoText3::LEFT);
    root->addChild(t);

    SbViewportRegion vp(W, H);
    cam->viewAll(root, vp);

    char outpath[1024];
    snprintf(outpath, sizeof(outpath), "%s_spacing.rgb", basepath);
    bool ok = renderToFile(root, outpath, W, H);

    if (ok) {
        SoOffscreenRenderer *ren = getSharedRenderer();
        ren->setViewportRegion(vp);
        ok = ren->render(root) &&
             validateNonBlack(ren->getBuffer(), W*H, "test3_multilineSpacing");
    }

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 4: SoText3 bounding box computation
// ---------------------------------------------------------------------------
static bool test4_boundingBox()
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    root->addChild(new SoDirectionalLight);

    SoText3 *text = new SoText3;
    text->string.setValue("Hello");
    text->parts.setValue(SoText3::FRONT);
    text->justification.setValue(SoText3::LEFT);
    root->addChild(text);

    SbViewportRegion vp(W, H);
    SoGetBoundingBoxAction bba(vp);
    bba.apply(root);

    SbBox3f box = bba.getBoundingBox();
    SbVec3f min, max;
    box.getBounds(min, max);
    printf("  test4_boundingBox: min=(%.2f,%.2f,%.2f) max=(%.2f,%.2f,%.2f)\n",
           min[0], min[1], min[2], max[0], max[1], max[2]);

    root->unref();

    bool ok = !box.isEmpty() && (max[0] > min[0]);
    if (!ok) {
        fprintf(stderr, "  FAIL test4_boundingBox: empty box\n");
        return false;
    }
    printf("  PASS test4_boundingBox\n");
    return true;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char **argv)
{
    initCoinHeadless();

    const char *basepath = (argc > 1) ? argv[1] : "render_text3_parts";

    int failures = 0;

    printf("\n=== SoText3 parts and justification tests ===\n");

    if (!test1_allJustifications(basepath))  ++failures;
    if (!test2_allParts(basepath))           ++failures;
    if (!test3_multilineSpacing(basepath))   ++failures;
    if (!test4_boundingBox())                ++failures;

    printf("\n=== Summary: %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
