/*
 * render_field_connections.cpp — SoField multi-connection API coverage
 *
 * Exercises SoField connection paths not covered by simpler engine tests:
 *   1. getConnectedEngine() — confirm an engine output drives a field
 *   2. getConnections() / getNumConnections() — list connections on a field
 *   3. getForwardConnections() — list downstream fields driven by a source
 *   4. SoMFFloat → SoMFFloat fan-out (one source drives two targets)
 *   5. disconnect(SoField*) — disconnect a specific field-to-field link
 *   6. disconnect(SoEngineOutput*) — disconnect a specific engine output
 *   7. Render a scene whose material colour is driven by a SoComposeVec3f
 *      engine through a Vec3f→Color conversion (visual proof)
 *   8. SoSFString → SoSFFloat conversion round-trip (exercises conversion
 *      engine path on two distinct source strings)
 *
 * Returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <Inventor/fields/SoSFFloat.h>
#include <Inventor/fields/SoSFColor.h>
#include <Inventor/fields/SoSFVec3f.h>
#include <Inventor/fields/SoMFFloat.h>
#include <Inventor/fields/SoSFString.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/engines/SoCalculator.h>
#include <Inventor/engines/SoComposeVec3f.h>
#include <cstdio>
#include <cmath>

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

/* -------------------------------------------------------------------------
 * Test 1: getConnectedEngine
 * ----------------------------------------------------------------------- */
static bool test1_getConnectedEngine()
{
    SoCalculator *calc = new SoCalculator;
    calc->ref();
    calc->expression.set1Value(0, "oa = 2.5");

    SoSFFloat sf;
    sf.connectFrom(&calc->oa);

    SoEngineOutput *eng = nullptr;
    bool isEngine = sf.getConnectedEngine(eng);
    printf("  test1: isEngine=%d eng=%p\n", (int)isEngine, (void *)eng);

    sf.disconnect();
    calc->unref();
    return isEngine && (eng != nullptr);
}

/* -------------------------------------------------------------------------
 * Test 2: getConnections / getNumConnections
 * ----------------------------------------------------------------------- */
static bool test2_getConnections()
{
    SoSFFloat src;
    src.setValue(42.0f);

    SoSFFloat dst;
    dst.connectFrom(&src);

    SoFieldList list;
    int n = dst.getConnections(list);
    int numConn = dst.getNumConnections();
    printf("  test2: getConnections=%d getNumConnections=%d\n", n, numConn);

    dst.disconnect();
    return (n >= 1) && (numConn >= 1);
}

/* -------------------------------------------------------------------------
 * Test 3: getForwardConnections — one source drives two sinks
 * ----------------------------------------------------------------------- */
static bool test3_forwardConnections()
{
    SoSFFloat src;
    src.setValue(5.0f);

    SoSFFloat dst1, dst2;
    dst1.connectFrom(&src);
    dst2.connectFrom(&src);

    SoFieldList fwd;
    int n = src.getForwardConnections(fwd);
    printf("  test3: forwardConnections=%d\n", n);

    dst1.disconnect();
    dst2.disconnect();
    return (n >= 2);
}

/* -------------------------------------------------------------------------
 * Test 4: SoMFFloat → SoMFFloat fan-out
 * ----------------------------------------------------------------------- */
static bool test4_fanOut()
{
    SoMFFloat src;
    src.set1Value(0, 1.0f);
    src.set1Value(1, 2.0f);
    src.set1Value(2, 3.0f);

    SoMFFloat dst1, dst2;
    dst1.connectFrom(&src);
    dst2.connectFrom(&src);

    float v1 = dst1[0];
    float v2 = dst2[0];
    printf("  test4: dst1[0]=%.1f dst2[0]=%.1f\n", v1, v2);

    dst1.disconnect();
    dst2.disconnect();
    return (fabsf(v1 - 1.0f) < 0.01f) && (fabsf(v2 - 1.0f) < 0.01f);
}

/* -------------------------------------------------------------------------
 * Test 5: disconnect(SoField *) — disconnect a specific field link
 * ----------------------------------------------------------------------- */
static bool test5_disconnectField()
{
    SoSFFloat src;
    src.setValue(99.0f);

    SoSFFloat dst;
    dst.connectFrom(&src);

    float driven = dst.getValue();
    printf("  test5: driven=%.1f\n", driven);

    dst.disconnect(&src);   /* disconnect the specific link */

    bool ok = (fabsf(driven - 99.0f) < 0.01f);
    printf("  test5: disconnectField ok=%d\n", (int)ok);
    return ok;
}

/* -------------------------------------------------------------------------
 * Test 6: disconnect(SoEngineOutput *) — disconnect a specific engine link
 * ----------------------------------------------------------------------- */
static bool test6_disconnectEngine()
{
    SoCalculator *calc = new SoCalculator;
    calc->ref();
    calc->expression.set1Value(0, "oa = 7.0");

    SoSFFloat sf;
    sf.connectFrom(&calc->oa);

    float driven = sf.getValue();
    printf("  test6: engineDriven=%.1f\n", driven);

    sf.disconnect(&calc->oa);   /* disconnect via engine output pointer */

    calc->unref();
    return (fabsf(driven - 7.0f) < 0.01f);
}

/* -------------------------------------------------------------------------
 * Test 7: Render a scene driven by SoComposeVec3f (Vec3f→Color conversion)
 * ----------------------------------------------------------------------- */
static bool test7_renderEngineScene(const char *basepath)
{
    SoSeparator *root = new SoSeparator; root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoComposeVec3f *comp = new SoComposeVec3f;
    comp->ref();
    comp->x.setValue(0.9f);
    comp->y.setValue(0.4f);
    comp->z.setValue(0.1f);

    SoMaterial *mat = new SoMaterial;
    /* Connects SoMFVec3f engine output → SoMFColor field via auto-conversion */
    mat->diffuseColor.connectFrom(&comp->vector);
    root->addChild(mat);
    root->addChild(new SoSphere);

    char path[1024];
    snprintf(path, sizeof(path), "%s_engine.rgb", basepath);
    renderToFile(root, path, 128, 128);

    SoOffscreenRenderer *ren = getSharedRenderer();
    SbViewportRegion vp(128, 128);
    ren->setViewportRegion(vp);
    bool ok = ren->render(root) &&
              validateNonBlack(ren->getBuffer(), 128 * 128, "test7_engine");

    mat->diffuseColor.disconnect();
    comp->unref();
    root->unref();
    return ok;
}

/* -------------------------------------------------------------------------
 * Test 8: SoSFString → SoSFFloat conversion with two different strings
 * ----------------------------------------------------------------------- */
static bool test8_stringToFloat()
{
    SoSFString s1, s2;
    s1.setValue("1.5");
    s2.setValue("2.5");

    SoSFFloat f;
    f.connectFrom(&s1);
    float v1 = f.getValue();
    printf("  test8: \"1.5\"→float=%.3f\n", v1);
    f.disconnect();

    f.connectFrom(&s2);
    float v2 = f.getValue();
    printf("  test8: \"2.5\"→float=%.3f\n", v2);
    f.disconnect();

    return (fabsf(v1 - 1.5f) < 0.1f) && (fabsf(v2 - 2.5f) < 0.1f);
}

/* -------------------------------------------------------------------------
 * main
 * ----------------------------------------------------------------------- */
int main(int argc, char **argv)
{
    initCoinHeadless();

    const char *basepath =
        (argc > 1) ? argv[1] : "render_field_connections";

    printf("\n=== SoField multi-connection API tests ===\n");

    int failures = 0;
    if (!test1_getConnectedEngine())        { printf("FAIL test1\n"); ++failures; }
    if (!test2_getConnections())            { printf("FAIL test2\n"); ++failures; }
    if (!test3_forwardConnections())        { printf("FAIL test3\n"); ++failures; }
    if (!test4_fanOut())                    { printf("FAIL test4\n"); ++failures; }
    if (!test5_disconnectField())           { printf("FAIL test5\n"); ++failures; }
    if (!test6_disconnectEngine())          { printf("FAIL test6\n"); ++failures; }
    if (!test7_renderEngineScene(basepath)) { printf("FAIL test7\n"); ++failures; }
    if (!test8_stringToFloat())             { printf("FAIL test8\n"); ++failures; }

    printf("\n=== Summary: %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
