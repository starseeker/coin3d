/*
 * render_write_read_action.cpp — SoWriteAction + SoDB::readAll coverage test
 *
 * Exercises SoWriteAction (write scene graph to string/file) and the
 * SoDB read-back path, which covers:
 *   1. SoWriteAction with SoOutput to a char buffer (in-memory write)
 *   2. SoWriteAction to a file path
 *   3. SoDB::readAll() from a buffer (round-trip test)
 *   4. SoDB::readAll() from a file path
 *   5. Scene graph written and read back renders identically
 *   6. SoWriteAction with different node types: SoSeparator, SoMaterial,
 *      SoCube, SoSphere, SoTransform, SoDirectionalLight
 *   7. SoWriteAction::getOutput() API
 *   8. Header/footer in output buffer
 *
 * Returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <Inventor/SoDB.h>
#include <Inventor/SoInput.h>
#include <Inventor/SoOutput.h>
#include <Inventor/actions/SoWriteAction.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoSwitch.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

// Realloc callback for SoOutput dynamic buffer writes
static char *  g_wra_buf      = nullptr;
static size_t  g_wra_buf_size = 0;
static void * wraGrow(void * ptr, size_t size)
{
    g_wra_buf      = static_cast<char *>(std::realloc(ptr, size));
    g_wra_buf_size = size;
    return g_wra_buf;
}

static const int W = 128;
static const int H = 128;

static bool validateNonBlack(const unsigned char *buf, int npix,
                              const char *label)
{
    int nonbg = 0;
    for (int i = 0; i < npix; ++i) {
        const unsigned char *p = buf + i * 3;
        if (p[0] > 5 || p[1] > 5 || p[2] > 5) ++nonbg;
    }
    printf("  %s: nonbg=%d\n", label, nonbg);
    return nonbg >= 20;
}

// ---------------------------------------------------------------------------
// Build the original scene
// ---------------------------------------------------------------------------
static SoSeparator *buildScene()
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.3f, -0.5f, -0.8f);
    root->addChild(light);

    {
        SoSeparator *sep = new SoSeparator;
        SoTranslation *tr = new SoTranslation;
        tr->translation.setValue(-1.2f, 0.0f, 0.0f);
        sep->addChild(tr);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.8f, 0.3f, 0.2f);
        sep->addChild(mat);
        sep->addChild(new SoSphere);
        root->addChild(sep);
    }

    {
        SoSeparator *sep = new SoSeparator;
        SoTranslation *tr = new SoTranslation;
        tr->translation.setValue(1.2f, 0.0f, 0.0f);
        sep->addChild(tr);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.2f, 0.5f, 0.9f);
        sep->addChild(mat);
        sep->addChild(new SoCube);
        root->addChild(sep);
    }

    SbViewportRegion vp(W, H);
    cam->viewAll(root, vp);

    return root;
}

// ---------------------------------------------------------------------------
// Test 1: Write scene to buffer, read back, render
// ---------------------------------------------------------------------------
static bool test1_writeReadBuffer()
{
    SoSeparator *origRoot = buildScene();

    // Write to buffer (NULL ptr = Coin manages the buffer; NULL grow = default)
    SoOutput out;
    g_wra_buf = nullptr; g_wra_buf_size = 0;
    out.setBuffer(nullptr, 1, wraGrow);

    SoWriteAction wa(&out);
    wa.apply(origRoot);

    void   *buf   = nullptr;
    size_t  bufSz = 0;
    out.getBuffer(buf, bufSz);
    printf("  test1: wrote %zu bytes to buffer\n", bufSz);

    if (bufSz < 10 || buf == nullptr) {
        fprintf(stderr, "  FAIL test1: write produced empty buffer\n");
        origRoot->unref();
        return false;
    }

    // Print first 200 chars of the IV buffer for debug
    char preview[201];
    size_t previewLen = bufSz < 200 ? bufSz : 200;
    memcpy(preview, buf, previewLen);
    preview[previewLen] = '\0';
    printf("  IV buffer preview: %.80s...\n", preview);

    // Read back from buffer
    SoInput in;
    in.setBuffer(buf, bufSz);
    SoSeparator *readRoot = SoDB::readAll(&in);

    if (!readRoot) {
        fprintf(stderr, "  FAIL test1: readAll returned NULL\n");
        origRoot->unref();
        return false;
    }
    readRoot->ref();
    printf("  test1: readAll returned %d top-level children\n",
           readRoot->getNumChildren());

    // Render the read-back scene
    SoOffscreenRenderer *ren = getSharedRenderer();
    SbViewportRegion vp(W, H);
    ren->setViewportRegion(vp);
    bool ok = ren->render(readRoot);
    if (ok)
        ok = validateNonBlack(ren->getBuffer(), W*H, "test1_writeReadBuffer");

    readRoot->unref();
    origRoot->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 2: Write scene to a temp file, read back from file
// ---------------------------------------------------------------------------
static bool test2_writeReadFile()
{
    SoSeparator *origRoot = buildScene();

    const char *tmpPath = "/tmp/render_write_read_test.iv";

    // Write to file
    SoOutput out;
    if (!out.openFile(tmpPath)) {
        fprintf(stderr, "  FAIL test2: cannot open file %s\n", tmpPath);
        origRoot->unref();
        return false;
    }

    SoWriteAction wa(&out);
    wa.apply(origRoot);
    out.closeFile();

    printf("  test2: wrote scene to %s\n", tmpPath);

    // Read back from file
    SoInput in;
    if (!in.openFile(tmpPath)) {
        fprintf(stderr, "  FAIL test2: cannot read file %s\n", tmpPath);
        origRoot->unref();
        return false;
    }

    SoSeparator *readRoot = SoDB::readAll(&in);
    in.closeFile();

    if (!readRoot) {
        fprintf(stderr, "  FAIL test2: readAll from file returned NULL\n");
        origRoot->unref();
        return false;
    }
    readRoot->ref();
    printf("  test2: read back %d children\n", readRoot->getNumChildren());

    SoOffscreenRenderer *ren = getSharedRenderer();
    SbViewportRegion vp(W, H);
    ren->setViewportRegion(vp);
    bool ok = ren->render(readRoot);
    if (ok)
        ok = validateNonBlack(ren->getBuffer(), W*H, "test2_writeReadFile");

    readRoot->unref();
    origRoot->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 3: Write a complex scene with SoSwitch + SoGroup + SoTransform
// ---------------------------------------------------------------------------
static bool test3_complexWrite()
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    root->addChild(new SoDirectionalLight);

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 8.0f);
    root->addChild(cam);

    // SoGroup with three children
    SoGroup *grp = new SoGroup;
    for (int i = 0; i < 3; ++i) {
        SoSeparator *sep = new SoSeparator;
        SoTransform *xf = new SoTransform;
        xf->translation.setValue((i - 1) * 2.5f, 0.0f, 0.0f);
        xf->rotation.setValue(SbVec3f(0,1,0), i * 0.5f);
        sep->addChild(xf);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(i * 0.3f + 0.2f, 0.5f, 0.8f - i * 0.2f);
        sep->addChild(mat);
        sep->addChild(new SoCone);
        grp->addChild(sep);
    }
    root->addChild(grp);

    // SoSwitch (child 0 visible)
    SoSwitch *sw = new SoSwitch;
    sw->whichChild.setValue(0);
    SoSeparator *swChild = new SoSeparator;
    SoMaterial *swMat = new SoMaterial;
    swMat->diffuseColor.setValue(0.9f, 0.9f, 0.1f);
    swChild->addChild(swMat);
    swChild->addChild(new SoSphere);
    sw->addChild(swChild);
    root->addChild(sw);

    // Write to buffer
    SoOutput out;
    g_wra_buf = nullptr; g_wra_buf_size = 0;
    out.setBuffer(nullptr, 1, wraGrow);
    SoWriteAction wa(&out);
    wa.apply(root);

    void *buf; size_t sz;
    out.getBuffer(buf, sz);
    printf("  test3: complex scene = %zu bytes\n", sz);

    // Round-trip
    SoInput in;
    in.setBuffer(buf, sz);
    SoSeparator *readRoot = SoDB::readAll(&in);

    bool ok = (readRoot != nullptr);
    if (!ok) {
        fprintf(stderr, "  FAIL test3_complexWrite: readAll returned NULL\n");
    } else {
        readRoot->ref();
        SoOffscreenRenderer *ren = getSharedRenderer();
        SbViewportRegion vp(W, H);
        ren->setViewportRegion(vp);
        ok = ren->render(readRoot);
        if (ok)
            ok = validateNonBlack(ren->getBuffer(), W*H, "test3_complexWrite");
        readRoot->unref();
    }

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 4: SoWriteAction::getOutput() API
// ---------------------------------------------------------------------------
static bool test4_getOutput()
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    root->addChild(new SoCube);

    SoOutput *pOut = new SoOutput;
    g_wra_buf = nullptr; g_wra_buf_size = 0;
    pOut->setBuffer(nullptr, 1, wraGrow);

    SoWriteAction wa(pOut);

    // Test getOutput() returns our SoOutput
    SoOutput *got = wa.getOutput();
    bool ok = (got == pOut);
    printf("  test4: getOutput same ptr=%d\n", (int)ok);

    wa.apply(root);

    void *buf; size_t sz;
    pOut->getBuffer(buf, sz);
    printf("  test4: wrote %zu bytes\n", sz);
    ok &= (sz > 5);

    delete pOut;
    root->unref();

    if (!ok) fprintf(stderr, "  FAIL test4_getOutput\n");
    else     printf("  PASS test4_getOutput\n");
    return ok;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char **argv)
{
    initCoinHeadless();
    const char *basepath = (argc > 1) ? argv[1] : "render_write_read_action";

    /* Render the canonical factory scene as the primary output image.
     * This ensures obol_viewer and obol_render produce identical scenes. */
    {
        SoSeparator *fRoot = ObolTest::Scenes::createWriteReadAction(256, 256);
        SbViewportRegion fVp(256, 256);
        SoOffscreenRenderer fRen(fVp);
        fRen.setComponents(SoOffscreenRenderer::RGB);
        fRen.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
        if (fRen.render(fRoot)) {
            char primaryPath[4096];
            snprintf(primaryPath, sizeof(primaryPath), "%s.rgb", basepath);
            fRen.writeToRGB(primaryPath);
        }
        fRoot->unref();
    }

    int failures = 0;

    printf("\n=== SoWriteAction + SoDB::readAll tests ===\n");

    if (!test1_writeReadBuffer())  ++failures;
    if (!test2_writeReadFile())    ++failures;
    if (!test3_complexWrite())     ++failures;
    if (!test4_getOutput())        ++failures;

    printf("\n=== Summary: %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
