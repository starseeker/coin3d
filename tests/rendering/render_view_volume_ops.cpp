/*
 * render_view_volume_ops.cpp — SbViewVolume / SbDPViewVolume coverage test
 *
 * Exercises SbViewVolume and SbDPViewVolume methods that are called far too
 * rarely by existing tests:
 *   1. ortho() / perspective() / frustum() constructors
 *   2. getMatrices() / getMatrix() / getCameraSpaceMatrix()
 *   3. projectPointToLine() (both overloads)
 *   4. projectToScreen() / getPlanePoint()
 *   5. getPlane() / getSightPoint()
 *   6. getAlignRotation() / getWorldToScreenScale()
 *   7. projectBox()
 *   8. rotateCamera() / translateCamera()
 *   9. zVector() / scale() / scaleWidth() / scaleHeight()
 *  10. getProjectionType() / getProjectionPoint() / getProjectionDirection()
 *  11. getNearDist() / getWidth() / getHeight() / getDepth()
 *  12. getViewVolumePlanes() (6 planes)
 *  13. transform(SbMatrix)
 *  14. getViewUp()
 *  15. intersect(SbVec3f) / intersect(SbBox3f)
 *  16. outsideTest()
 *  17. SbDPViewVolume equivalents of the above
 *  18. Render a scene and call camera->getViewVolume() to exercise
 *      the camera→view-volume code path
 *
 * Returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/SbViewVolume.h>
#include <Inventor/SbDPViewVolume.h>
#include <Inventor/SbPlane.h>
#include <Inventor/SbDPPlane.h>
#include <Inventor/SbLine.h>
#include <Inventor/SbDPLine.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbDPMatrix.h>
#include <Inventor/SbRotation.h>
#include <Inventor/SbDPRotation.h>
#include <Inventor/SbVec2d.h>
#include <Inventor/SbBox3f.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <cstdio>
#include <cmath>

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
// Test 1: SbViewVolume — perspective path
// ---------------------------------------------------------------------------
static bool test1_perspectiveVV()
{
    SbViewVolume vv;
    vv.perspective(M_PI / 3.0f, 1.0f, 0.5f, 100.0f);

    // Basic accessors
    printf("  projType=%d (1=perspective)\n",
           (int)vv.getProjectionType());
    printf("  near=%.2f width=%.2f height=%.2f depth=%.2f\n",
           vv.getNearDist(), vv.getWidth(), vv.getHeight(), vv.getDepth());

    // Matrices
    SbMatrix affine, proj;
    vv.getMatrices(affine, proj);
    SbMatrix mat = vv.getMatrix();
    SbMatrix camMat = vv.getCameraSpaceMatrix();

    // Project a point on the near plane center
    SbVec3f ndc;
    vv.projectToScreen(SbVec3f(0,0,-0.5f), ndc);
    printf("  projectToScreen near center: (%.3f,%.3f,%.3f)\n",
           ndc[0], ndc[1], ndc[2]);

    // Project point to line
    SbLine line;
    vv.projectPointToLine(SbVec2f(0.5f, 0.5f), line);
    SbVec3f l0, l1;
    vv.projectPointToLine(SbVec2f(0.5f, 0.5f), l0, l1);
    printf("  lineDir: (%.3f,%.3f,%.3f)\n",
           line.getDirection()[0], line.getDirection()[1],
           line.getDirection()[2]);

    // getPlane / getSightPoint / getPlanePoint
    vv.getPlane(10.0f);
    SbVec3f sp    = vv.getSightPoint(10.0f);
    vv.getPlanePoint(10.0f, SbVec2f(0.5f, 0.5f));
    printf("  sightPt@10: (%.2f,%.2f,%.2f)\n", sp[0], sp[1], sp[2]);

    // getAlignRotation
    vv.getAlignRotation(FALSE);
    vv.getAlignRotation(TRUE);

    // getWorldToScreenScale
    float scale = vv.getWorldToScreenScale(SbVec3f(0,0,-10.0f), 0.5f);
    printf("  worldToScreenScale@10: %.4f\n", scale);

    // projectBox
    SbBox3f box(SbVec3f(-1,-1,-12), SbVec3f(1,1,-8));
    SbVec2f bboxProj = vv.projectBox(box);
    printf("  projectBox: (%.3f,%.3f)\n", bboxProj[0], bboxProj[1]);

    // getViewVolumePlanes
    SbPlane planes[6];
    vv.getViewVolumePlanes(planes);
    printf("  planes[0] normal: (%.2f,%.2f,%.2f)\n",
           planes[0].getNormal()[0],
           planes[0].getNormal()[1],
           planes[0].getNormal()[2]);

    // zVector / getViewUp
    SbVec3f z = vv.zVector();
    SbVec3f up = vv.getViewUp();
    printf("  zVector: (%.2f,%.2f,%.2f)\n", z[0], z[1], z[2]);
    printf("  viewUp:  (%.2f,%.2f,%.2f)\n", up[0], up[1], up[2]);

    // getProjectionPoint / getProjectionDirection
    SbVec3f pt  = vv.getProjectionPoint();
    SbVec3f dir = vv.getProjectionDirection();
    printf("  projPt: (%.2f,%.2f,%.2f)\n", pt[0], pt[1], pt[2]);
    printf("  projDir: (%.2f,%.2f,%.2f)\n", dir[0], dir[1], dir[2]);

    // scale / scaleWidth / scaleHeight
    SbViewVolume vv2 = vv;
    vv2.scale(1.5f);
    vv2.scaleWidth(0.8f);
    vv2.scaleHeight(0.8f);

    // rotateCamera / translateCamera
    SbViewVolume vv3 = vv;
    vv3.rotateCamera(SbRotation(SbVec3f(0,1,0), 0.2f));
    vv3.translateCamera(SbVec3f(1.0f, 0.0f, 0.0f));

    // transform
    SbMatrix xf; xf.makeIdentity();
    vv3.transform(xf);

    // intersect
    bool hit1 = vv.intersect(SbVec3f(0,0,-10.0f));
    bool hit2 = vv.intersect(box);
    SbVec3f closest;
    bool hit3 = vv.intersect(SbVec3f(0,0,-5.0f), SbVec3f(0,0,-15.0f), closest);
    printf("  intersect point=%d box=%d seg=%d\n",
           (int)hit1, (int)hit2, (int)hit3);

    // outsideTest
    SbPlane testPlane(SbVec3f(0,0,1), 200.0f);
    bool out = vv.outsideTest(testPlane,
                               SbVec3f(-1,-1,-10), SbVec3f(1,1,-5));
    printf("  outsideTest=%d\n", (int)out);

    printf("  PASS test1_perspectiveVV\n");
    return true;
}

// ---------------------------------------------------------------------------
// Test 2: SbViewVolume — ortho path
// ---------------------------------------------------------------------------
static bool test2_orthoVV()
{
    SbViewVolume vv;
    vv.ortho(-5.0f, 5.0f, -5.0f, 5.0f, 0.5f, 100.0f);

    printf("  projType=%d (0=ortho)\n", (int)vv.getProjectionType());
    printf("  near=%.2f width=%.2f\n", vv.getNearDist(), vv.getWidth());

    SbMatrix affine, proj;
    vv.getMatrices(affine, proj);

    SbVec3f ndc;
    vv.projectToScreen(SbVec3f(0,0,-50.0f), ndc);
    printf("  projectToScreen midZ: (%.3f,%.3f,%.3f)\n", ndc[0], ndc[1], ndc[2]);

    SbPlane planes[6];
    vv.getViewVolumePlanes(planes);

    printf("  PASS test2_orthoVV\n");
    return true;
}

// ---------------------------------------------------------------------------
// Test 3: SbViewVolume — frustum path
// ---------------------------------------------------------------------------
static bool test3_frustumVV()
{
    SbViewVolume vv;
    vv.frustum(-1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 100.0f);

    printf("  frustum projType=%d\n", (int)vv.getProjectionType());
    SbMatrix affine, proj;
    vv.getMatrices(affine, proj);

    // getDPViewVolume exercises the DP path
    const SbDPViewVolume &dpvv = vv.getDPViewVolume();
    (void)dpvv; // silence unused warning

    printf("  PASS test3_frustumVV\n");
    return true;
}

// ---------------------------------------------------------------------------
// Test 4: SbDPViewVolume equivalents
// ---------------------------------------------------------------------------
static bool test4_dpViewVolume()
{
    SbDPViewVolume dpvv;
    dpvv.perspective(M_PI / 4.0, 1.0, 0.5, 200.0);

    printf("  dpvv projType=%d\n", (int)dpvv.getProjectionType());
    printf("  dpvv near=%.2f width=%.2f\n",
           dpvv.getNearDist(), dpvv.getWidth());

    SbDPMatrix affine, proj;
    dpvv.getMatrices(affine, proj);
    SbDPMatrix mat = dpvv.getMatrix();

    SbVec3d ndc;
    dpvv.projectToScreen(SbVec3d(0,0,-100.0), ndc);
    printf("  dpvv projectToScreen: (%.3f,%.3f,%.3f)\n",
           ndc[0], ndc[1], ndc[2]);

    SbVec3d sp = dpvv.getSightPoint(10.0);
    printf("  dpvv sightPt@10: (%.2f,%.2f,%.2f)\n", sp[0], sp[1], sp[2]);

    SbVec3d pp = dpvv.getPlanePoint(10.0, SbVec2d(0.5, 0.5));
    printf("  dpvv planePoint: (%.2f,%.2f,%.2f)\n", pp[0], pp[1], pp[2]);

    SbVec3d z  = dpvv.zVector();
    SbVec3d up = dpvv.getViewUp();
    printf("  dpvv zVector: (%.2f,%.2f,%.2f)\n", z[0], z[1], z[2]);
    printf("  dpvv viewUp:  (%.2f,%.2f,%.2f)\n", up[0], up[1], up[2]);

    // DP scale / rotate / translate
    SbDPViewVolume dpvv2 = dpvv;
    dpvv2.scale(1.2);
    dpvv2.scaleWidth(0.9);
    dpvv2.scaleHeight(0.9);
    dpvv2.rotateCamera(SbDPRotation(SbVec3d(0,1,0), 0.1));
    dpvv2.translateCamera(SbVec3d(0.5, 0, 0));

    // DP getViewVolumePlanes (returns SbPlane, not SbDPPlane)
    SbPlane dpplanes[6];
    dpvv.getViewVolumePlanes(dpplanes);

    // DP intersect - SbDPViewVolume doesn't have intersect() per API
    // Use projectToScreen to cover the dp render paths instead
    SbVec3d ndc2;
    dpvv.projectToScreen(SbVec3d(0, 0, -50.0), ndc2);
    printf("  dpvv projectToScreen: (%.3f,%.3f,%.3f)\n",
           ndc2[0], ndc2[1], ndc2[2]);

    // ortho path
    SbDPViewVolume dportho;
    dportho.ortho(-5.0, 5.0, -5.0, 5.0, 0.5, 100.0);
    printf("  dpvv ortho projType=%d\n", (int)dportho.getProjectionType());

    // frustum path
    SbDPViewVolume dpfrustum;
    dpfrustum.frustum(-1.0, 1.0, -1.0, 1.0, 1.0, 100.0);

    printf("  PASS test4_dpViewVolume\n");
    return true;
}

// ---------------------------------------------------------------------------
// Test 5: Camera view volume used during rendering (exercises GL paths)
// ---------------------------------------------------------------------------
static bool test5_cameraVVRendering(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->nearDistance = 0.5f;
    cam->farDistance  = 50.0f;
    cam->heightAngle  = M_PI / 3.0f;
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.6f, 0.3f, 0.8f);
    root->addChild(mat);
    root->addChild(new SoSphere);

    SbViewportRegion vp(W, H);
    cam->viewAll(root, vp);

    // After viewAll, get the view volume via the camera
    SbViewVolume vv = cam->getViewVolume(vp.getViewportAspectRatio());
    printf("  test5 vv near=%.2f far=%.2f\n",
           vv.getNearDist(), vv.getNearDist() + vv.getDepth());

    // Test viewAll with an aspect ratio ≠ 1
    SbViewportRegion vpWide(512, 256);
    cam->viewAll(root, vpWide);

    char outpath[1024];
    snprintf(outpath, sizeof(outpath), "%s_cam.rgb", basepath);
    bool ok = renderToFile(root, outpath, W, H);

    if (ok) {
        SoOffscreenRenderer *ren = getSharedRenderer();
        ren->setViewportRegion(vp);
        ok = ren->render(root) &&
             validateNonBlack(ren->getBuffer(), W*H, "test5_cameraVVRendering");
    }

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 6: Orthographic camera view volume
// ---------------------------------------------------------------------------
static bool test6_orthoCamera(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->height.setValue(4.0f);
    cam->nearDistance = 0.5f;
    cam->farDistance  = 20.0f;
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.3f, 0.7f, 0.4f);
    root->addChild(mat);
    root->addChild(new SoCube);

    SbViewportRegion vp(W, H);
    cam->viewAll(root, vp);

    SbViewVolume vv = cam->getViewVolume(vp.getViewportAspectRatio());
    printf("  test6 ortho vv type=%d near=%.2f\n",
           (int)vv.getProjectionType(), vv.getNearDist());

    // Exercise viewAll with a non-default aspect ratio
    SbViewportRegion vpTall(128, 256);
    cam->viewAll(root, vpTall);

    char outpath[1024];
    snprintf(outpath, sizeof(outpath), "%s_ortho.rgb", basepath);
    bool ok = renderToFile(root, outpath, W, H);

    if (ok) {
        SoOffscreenRenderer *ren = getSharedRenderer();
        ren->setViewportRegion(vp);
        ok = ren->render(root) &&
             validateNonBlack(ren->getBuffer(), W*H, "test6_orthoCamera");
    }

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char **argv)
{
    initCoinHeadless();

    const char *basepath = (argc > 1) ? argv[1] : "render_view_volume_ops";

    int failures = 0;

    printf("\n=== SbViewVolume / SbDPViewVolume operation tests ===\n");

    if (!test1_perspectiveVV())              ++failures;
    if (!test2_orthoVV())                    ++failures;
    if (!test3_frustumVV())                  ++failures;
    if (!test4_dpViewVolume())               ++failures;
    if (!test5_cameraVVRendering(basepath))  ++failures;
    if (!test6_orthoCamera(basepath))        ++failures;

    printf("\n=== Summary: %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
