/*
 * nogl_scenegraph_test.cpp
 *
 * No-OpenGL scene graph validation test.
 *
 * This test verifies that the OBOL_NO_OPENGL build can:
 *   - Initialise SoDB with a NULL context manager
 *   - Build and traverse a complete scene graph
 *   - Extract triangles via SoCallbackAction
 *   - Perform scene queries via SoSearchAction
 *   - Execute camera math (SbViewVolume::projectPointToLine)
 *   - Apply SbMatrix transforms correctly
 *
 * No OpenGL headers or functions are used; no display server is required.
 * All assertion failures are fatal (std::abort via assert()).
 *
 * Exit codes: 0 = pass, non-zero = fail.
 */

#include <Inventor/SoDB.h>
#include <Inventor/nodekits/SoNodeKit.h>
#include <Inventor/SoInteraction.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbViewVolume.h>
#include <Inventor/SbLine.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/actions/SoCallbackAction.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoShape.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/SoPrimitiveVertex.h>

#include <cassert>
#include <cstdio>

static int s_triangle_count = 0;

static void
count_triangles(void * /*ud*/,
                SoCallbackAction * /*action*/,
                const SoPrimitiveVertex * /*v0*/,
                const SoPrimitiveVertex * /*v1*/,
                const SoPrimitiveVertex * /*v2*/)
{
    ++s_triangle_count;
}

int main()
{
    /* --- Initialise ---------------------------------------------------- */
    /* NULL context manager = no OpenGL; custom context drivers are set up
       per render call via SoOffscreenRenderer. */
    SoDB::init(nullptr);
    SoNodeKit::init();
    SoInteraction::init();

    /* --- Scene graph --------------------------------------------------- */
    SoSeparator * root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera * cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 6.0f);
    cam->nearDistance.setValue(0.1f);
    cam->farDistance.setValue(100.0f);
    root->addChild(cam);

    root->addChild(new SoDirectionalLight);

    auto add_shape = [&](SoNode * shape, float tx) {
        SoSeparator * s = new SoSeparator;
        SoTranslation * t = new SoTranslation;
        t->translation.setValue(tx, 0.0f, 0.0f);
        s->addChild(t);
        s->addChild(shape);
        root->addChild(s);
    };
    add_shape(new SoSphere,   -3.0f);
    add_shape(new SoCube,     -1.0f);
    add_shape(new SoCone,      1.0f);
    add_shape(new SoCylinder,  3.0f);

    /* --- Triangle extraction ------------------------------------------ */
    SoCallbackAction cba;
    cba.addTriangleCallback(SoShape::getClassTypeId(), count_triangles, nullptr);
    cba.apply(root);

    assert(s_triangle_count > 0 &&
           "FAIL: SoCallbackAction yielded zero triangles");
    printf("Triangle count: %d\n", s_triangle_count);

    /* --- SoSearchAction ----------------------------------------------- */
    SoSearchAction sa;
    sa.setType(SoSphere::getClassTypeId());
    sa.setInterest(SoSearchAction::FIRST);
    sa.apply(root);
    assert(sa.getPath() != nullptr &&
           "FAIL: SoSearchAction did not find SoSphere");
    printf("SoSearchAction: OK\n");

    /* --- Camera math -------------------------------------------------- */
    SbViewportRegion vp(320, 240);
    cam->viewAll(root, vp);
    SbViewVolume vv = cam->getViewVolume(vp.getViewportAspectRatio());
    SbLine ray;
    vv.projectPointToLine(SbVec2f(0.5f, 0.5f), ray);
    SbVec3f dir = ray.getDirection();
    /* Centre pixel should point roughly in -Z direction */
    assert(dir[2] < -0.5f && "FAIL: centre ray does not point toward scene");
    printf("Camera ray direction: (%.3f, %.3f, %.3f): OK\n",
           dir[0], dir[1], dir[2]);

    /* --- SbMatrix transform ------------------------------------------ */
    SbMatrix m = SbMatrix::identity();
    SbVec3f pt(1.0f, 2.0f, 3.0f), out;
    m.multVecMatrix(pt, out);
    assert((out - pt).length() < 1e-5f &&
           "FAIL: SbMatrix identity transform is wrong");
    printf("SbMatrix identity: OK\n");

    /* --- Cleanup ------------------------------------------------------ */
    root->unref();

    printf("PASS: nogl_scenegraph_test (%d triangles)\n", s_triangle_count);
    return 0;
}
