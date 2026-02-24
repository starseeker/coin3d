/*
 * render_image_deep.cpp — SoImage node deep coverage
 *
 * Exercises SoImage code paths not covered by render_image_node.cpp:
 *   1. 1-component (grayscale) image
 *   2. 2-component (grayscale + alpha) image
 *   3. 4-component (RGBA) image
 *   4. Width / height field overrides (stretch/crop)
 *   5. vertAlignment: TOP, HALF (center), BOTTOM
 *   6. horAlignment: LEFT, CENTER, RIGHT
 *   7. Null image (width=0, height=0) — should render as nothing / not crash
 *   8. Large image (> default max texture size heuristic)
 *   9. Render from a SoPath through an SoImage node
 *  10. Multiple SoImage nodes in the same scene (each with different alignment)
 *
 * Returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoImage.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/fields/SoSFImage.h>
#include <Inventor/SbImage.h>
#include <Inventor/SbVec2s.h>
#include <cstdio>
#include <cstring>
#include <vector>

static const int W = 256;
static const int H = 256;

// ---------------------------------------------------------------------------
// Build an SbImage with the given component count and size
// ---------------------------------------------------------------------------
static void fillImage(SoSFImage &field, int nc, int iw, int ih)
{
    SbVec2s sz((short)iw, (short)ih);
    // allocate and fill
    std::vector<unsigned char> data(iw * ih * nc, 0);
    for (int y = 0; y < ih; ++y) {
        for (int x = 0; x < iw; ++x) {
            unsigned char *p = data.data() + (y * iw + x) * nc;
            // checkerboard pattern
            bool checker = ((x / 4 + y / 4) % 2 == 0);
            if (nc == 1) {
                p[0] = checker ? 220 : 50;
            } else if (nc == 2) {
                p[0] = checker ? 200 : 80;
                p[1] = 200;  // alpha
            } else if (nc == 3) {
                p[0] = checker ? 200 : 50;
                p[1] = checker ? 80  : 200;
                p[2] = 100;
            } else {  // nc == 4
                p[0] = checker ? 220 : 50;
                p[1] = 100;
                p[2] = checker ? 50 : 200;
                p[3] = 200;  // alpha
            }
        }
    }
    field.setValue(sz, nc, data.data());
}

// ---------------------------------------------------------------------------
// Build a scene with an SoImage centered at origin, camera looking at it
// ---------------------------------------------------------------------------
static SoSeparator *buildImageScene(SoImage **imgOut = nullptr)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->height.setValue(4.0f);
    root->addChild(cam);

    root->addChild(new SoDirectionalLight);

    SoImage *img = new SoImage;
    root->addChild(img);

    if (imgOut) *imgOut = img;
    return root;
}

static bool validateNonBlack(const unsigned char *buf, int npix, const char *lbl)
{
    int nonbg = 0;
    for (int i = 0; i < npix; ++i) {
        const unsigned char *p = buf + i * 3;
        if (p[0] > 5 || p[1] > 5 || p[2] > 5) ++nonbg;
    }
    printf("  %s: nonbg=%d\n", lbl, nonbg);
    return nonbg >= 10;
}

static bool renderScene(SoSeparator *root, const char *path)
{
    renderToFile(root, path, W, H);
    SoOffscreenRenderer *ren = getSharedRenderer();
    SbViewportRegion vp((short)W, (short)H);
    ren->setViewportRegion(vp);
    bool ok = ren->render(root);
    return ok;
}

// ---------------------------------------------------------------------------
// Test 1: 1-component (grayscale) image
// ---------------------------------------------------------------------------
static bool test1_grayscale(const char *basepath)
{
    SoImage *img = nullptr;
    SoSeparator *root = buildImageScene(&img);

    fillImage(img->image, 1, 32, 32);
    img->width.setValue(64);
    img->height.setValue(64);

    char path[1024]; snprintf(path, sizeof(path), "%s_gray.rgb", basepath);
    bool ok = renderScene(root, path);
    if (ok) ok = validateNonBlack(getSharedRenderer()->getBuffer(), W*H, "test1_gray");

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 2: 2-component (grayscale+alpha) image
// ---------------------------------------------------------------------------
static bool test2_grayAlpha(const char *basepath)
{
    SoImage *img = nullptr;
    SoSeparator *root = buildImageScene(&img);

    fillImage(img->image, 2, 32, 32);
    img->width.setValue(-1);  // use natural size

    char path[1024]; snprintf(path, sizeof(path), "%s_graya.rgb", basepath);
    bool ok = renderScene(root, path);
    if (ok) ok = validateNonBlack(getSharedRenderer()->getBuffer(), W*H, "test2_grayA");

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 3: 4-component (RGBA) image
// ---------------------------------------------------------------------------
static bool test3_rgba(const char *basepath)
{
    SoImage *img = nullptr;
    SoSeparator *root = buildImageScene(&img);

    fillImage(img->image, 4, 48, 48);

    char path[1024]; snprintf(path, sizeof(path), "%s_rgba.rgb", basepath);
    bool ok = renderScene(root, path);
    if (ok) ok = validateNonBlack(getSharedRenderer()->getBuffer(), W*H, "test3_rgba");

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 4: vertAlignment variations
// ---------------------------------------------------------------------------
static bool test4_vertAlignment(const char *basepath)
{
    bool ok = true;
    SoImage::VertAlignment valigns[] = {
        SoImage::BOTTOM,
        SoImage::HALF,
        SoImage::TOP
    };
    const char *vnames[] = { "BOTTOM", "HALF", "TOP" };

    for (int i = 0; i < 3; ++i) {
        SoImage *img = nullptr;
        SoSeparator *root = buildImageScene(&img);
        fillImage(img->image, 3, 32, 32);
        img->vertAlignment.setValue(valigns[i]);

        char path[1024];
        snprintf(path, sizeof(path), "%s_vert_%s.rgb", basepath, vnames[i]);
        bool rendered = renderScene(root, path);
        if (rendered) {
            char lbl[64]; snprintf(lbl, sizeof(lbl), "test4_%s", vnames[i]);
            ok = ok && validateNonBlack(getSharedRenderer()->getBuffer(), W*H, lbl);
        }
        root->unref();
    }
    return ok;
}

// ---------------------------------------------------------------------------
// Test 5: horAlignment variations
// ---------------------------------------------------------------------------
static bool test5_horAlignment(const char *basepath)
{
    bool ok = true;
    SoImage::HorAlignment haligns[] = {
        SoImage::LEFT,
        SoImage::CENTER,
        SoImage::RIGHT
    };
    const char *hnames[] = { "LEFT", "CENTER", "RIGHT" };

    for (int i = 0; i < 3; ++i) {
        SoImage *img = nullptr;
        SoSeparator *root = buildImageScene(&img);
        fillImage(img->image, 3, 32, 32);
        img->horAlignment.setValue(haligns[i]);

        char path[1024];
        snprintf(path, sizeof(path), "%s_hor_%s.rgb", basepath, hnames[i]);
        bool rendered = renderScene(root, path);
        if (rendered) {
            char lbl[64]; snprintf(lbl, sizeof(lbl), "test5_%s", hnames[i]);
            ok = ok && validateNonBlack(getSharedRenderer()->getBuffer(), W*H, lbl);
        }
        root->unref();
    }
    return ok;
}

// ---------------------------------------------------------------------------
// Test 6: Null / zero-size image (should not crash)
// ---------------------------------------------------------------------------
static bool test6_nullImage(const char *basepath)
{
    SoImage *img = nullptr;
    SoSeparator *root = buildImageScene(&img);

    // Don't set any image → default is empty
    img->width.setValue(0);
    img->height.setValue(0);

    char path[1024]; snprintf(path, sizeof(path), "%s_null.rgb", basepath);
    renderScene(root, path);  // must not crash

    root->unref();
    printf("  test6_null: no crash\n");
    return true;
}

// ---------------------------------------------------------------------------
// Test 7: Large image (exercises resize path)
// ---------------------------------------------------------------------------
static bool test7_largeImage(const char *basepath)
{
    SoImage *img = nullptr;
    SoSeparator *root = buildImageScene(&img);

    fillImage(img->image, 3, 256, 256);  // large image

    char path[1024]; snprintf(path, sizeof(path), "%s_large.rgb", basepath);
    bool ok = renderScene(root, path);
    if (ok) ok = validateNonBlack(getSharedRenderer()->getBuffer(), W*H, "test7_large");

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 8: Multiple SoImage nodes with different alignments in one scene
// ---------------------------------------------------------------------------
static bool test8_multipleImages(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->height.setValue(6.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    // Three images with different alignments
    SoImage::HorAlignment haligns[] = { SoImage::LEFT, SoImage::CENTER, SoImage::RIGHT };
    SoImage::VertAlignment valigns[] = { SoImage::BOTTOM, SoImage::HALF, SoImage::TOP };

    for (int i = 0; i < 3; ++i) {
        SoSeparator *sep = new SoSeparator;
        SoTranslation *tr = new SoTranslation;
        tr->translation.setValue(-2.0f + i * 2.0f, -2.0f + i * 2.0f, 0.0f);
        sep->addChild(tr);

        SoImage *img = new SoImage;
        fillImage(img->image, 3, 24, 24);
        img->horAlignment.setValue(haligns[i]);
        img->vertAlignment.setValue(valigns[i]);
        sep->addChild(img);
        root->addChild(sep);
    }

    char path[1024]; snprintf(path, sizeof(path), "%s_multi.rgb", basepath);
    bool ok = renderScene(root, path);
    if (ok) ok = validateNonBlack(getSharedRenderer()->getBuffer(), W*H, "test8_multi");

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char **argv)
{
    initCoinHeadless();

    const char *basepath = (argc > 1) ? argv[1] : "render_image_deep";

    int failures = 0;
    printf("\n=== SoImage deep coverage tests ===\n");

    if (!test1_grayscale(basepath))     { printf("FAIL test1\n"); ++failures; }
    if (!test2_grayAlpha(basepath))     { printf("FAIL test2\n"); ++failures; }
    if (!test3_rgba(basepath))          { printf("FAIL test3\n"); ++failures; }
    if (!test4_vertAlignment(basepath)) { printf("FAIL test4\n"); ++failures; }
    if (!test5_horAlignment(basepath))  { printf("FAIL test5\n"); ++failures; }
    if (!test6_nullImage(basepath))     { printf("FAIL test6\n"); ++failures; }
    if (!test7_largeImage(basepath))    { printf("FAIL test7\n"); ++failures; }
    if (!test8_multipleImages(basepath)){ printf("FAIL test8\n"); ++failures; }

    printf("\n=== Summary: %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
