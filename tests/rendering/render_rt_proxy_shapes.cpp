/*
 * render_rt_proxy_shapes.cpp - NanoRT rendering of line sets and point sets
 *
 * Demonstrates and validates the ray-tracing proxy-geometry path for shape
 * types that have no triangle callbacks in the standard Obol primitives:
 *
 *   - SoLineSet    → thin cylinders via SoLineSet::createCylinderProxy()
 *   - SoIndexedLineSet → thin cylinders via SoIndexedLineSet::createCylinderProxy()
 *   - SoPointSet   → small spheres  via SoPointSet::createSphereProxy()
 *
 * The scene contains:
 *   Top-left:     Red   SoLineSet  (horizontal bar)
 *   Top-right:    Green SoIndexedLineSet (diagonal line)
 *   Bottom-left:  Blue  SoPointSet (4-point cross)
 *   Bottom-right: Gold  SoCylinder (reference solid geometry)
 *
 * When rendered via the NanoRT context manager the line and point nodes are
 * intercepted by pre-callbacks in nanort_context_manager.h, which call the
 * new createCylinderProxy() / createSphereProxy() methods and collect the
 * resulting triangles.  The test passes if the rendered image contains pixels
 * from each of the four colour families (red, green, blue, gold).
 *
 * Writes argv[1]+".rgb" (SGI RGB format) and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoLineSet.h>
#include <Inventor/nodes/SoIndexedLineSet.h>
#include <Inventor/nodes/SoPointSet.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>

static bool validateProxyShapes(const unsigned char *buf, int w, int h)
{
    int red = 0, green = 0, blue = 0, gold = 0;
    for (int i = 0; i < w * h; ++i) {
        const unsigned char *p = buf + i * 3;
        // Red pixel: high R, low G, low B
        if (p[0] > 100 && p[1] < 80 && p[2] < 80) ++red;
        // Green pixel: low R, high G, low B
        if (p[1] > 100 && p[0] < 80 && p[2] < 80) ++green;
        // Blue pixel: low R, low G, high B
        if (p[2] > 100 && p[0] < 80 && p[1] < 80) ++blue;
        // Gold pixel: high R, high G (roughly), low B
        if (p[0] > 100 && p[1] > 80 && p[2] < 60) ++gold;
    }
    printf("render_rt_proxy_shapes: red=%d green=%d blue=%d gold=%d\n",
           red, green, blue, gold);

    bool ok = (red > 0) && (green > 0) && (blue > 0) && (gold > 0);
    if (!ok)
        fprintf(stderr,
                "render_rt_proxy_shapes: FAIL - one or more colour families missing\n");
    else
        printf("render_rt_proxy_shapes: PASS\n");
    return ok;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.5f, -0.8f, -0.6f);
    root->addChild(light);

    const float s = 2.5f;   // grid spacing

    // ---- Top-left: red SoLineSet (horizontal bar at Y=0) -------------------
    {
        SoSeparator *sep = new SoSeparator;
        SoTranslation *t = new SoTranslation;
        t->translation.setValue(-s * 0.5f, s * 0.5f, 0.0f);
        sep->addChild(t);

        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.85f, 0.15f, 0.15f);
        sep->addChild(mat);

        SoCoordinate3 *coords = new SoCoordinate3;
        coords->point.set1Value(0, SbVec3f(-0.7f, 0.0f, 0.0f));
        coords->point.set1Value(1, SbVec3f( 0.7f, 0.0f, 0.0f));
        sep->addChild(coords);

        SoLineSet *ls = new SoLineSet;
        ls->numVertices.set1Value(0, 2);
        sep->addChild(ls);

        root->addChild(sep);
    }

    // ---- Top-right: green SoIndexedLineSet (diagonal) ----------------------
    {
        SoSeparator *sep = new SoSeparator;
        SoTranslation *t = new SoTranslation;
        t->translation.setValue(s * 0.5f, s * 0.5f, 0.0f);
        sep->addChild(t);

        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.15f, 0.75f, 0.15f);
        sep->addChild(mat);

        SoCoordinate3 *coords = new SoCoordinate3;
        coords->point.set1Value(0, SbVec3f(-0.6f, -0.6f, 0.0f));
        coords->point.set1Value(1, SbVec3f( 0.6f,  0.6f, 0.0f));
        sep->addChild(coords);

        SoIndexedLineSet *ils = new SoIndexedLineSet;
        ils->coordIndex.set1Value(0, 0);
        ils->coordIndex.set1Value(1, 1);
        ils->coordIndex.set1Value(2, -1);
        sep->addChild(ils);

        root->addChild(sep);
    }

    // ---- Bottom-left: blue SoPointSet (4-point cross) ----------------------
    {
        SoSeparator *sep = new SoSeparator;
        SoTranslation *t = new SoTranslation;
        t->translation.setValue(-s * 0.5f, -s * 0.5f, 0.0f);
        sep->addChild(t);

        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.15f, 0.35f, 0.90f);
        sep->addChild(mat);

        SoCoordinate3 *coords = new SoCoordinate3;
        coords->point.set1Value(0, SbVec3f( 0.0f,  0.5f, 0.0f));
        coords->point.set1Value(1, SbVec3f( 0.0f, -0.5f, 0.0f));
        coords->point.set1Value(2, SbVec3f(-0.5f,  0.0f, 0.0f));
        coords->point.set1Value(3, SbVec3f( 0.5f,  0.0f, 0.0f));
        sep->addChild(coords);

        SoPointSet *ps = new SoPointSet;
        sep->addChild(ps);

        root->addChild(sep);
    }

    // ---- Bottom-right: gold SoCylinder (reference solid geometry) ----------
    {
        SoSeparator *sep = new SoSeparator;
        SoTranslation *t = new SoTranslation;
        t->translation.setValue(s * 0.5f, -s * 0.5f, 0.0f);
        sep->addChild(t);

        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor .setValue(0.90f, 0.75f, 0.15f);
        mat->specularColor.setValue(0.6f, 0.6f, 0.6f);
        mat->shininess    .setValue(0.5f);
        sep->addChild(mat);
        sep->addChild(new SoCylinder);

        root->addChild(sep);
    }

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    cam->viewAll(root, vp);
    cam->position.setValue(cam->position.getValue() * 1.1f);

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_rt_proxy_shapes.rgb");

    bool ok = false;
#ifdef COIN3D_NANORT_BUILD
    // For the NanoRT path we can validate pixels directly.
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
    if (renderer.render(root)) {
        const unsigned char *buf = renderer.getBuffer();
        bool pixOk = (buf != nullptr) &&
                     validateProxyShapes(buf, DEFAULT_WIDTH, DEFAULT_HEIGHT);
        ok = pixOk && renderer.writeToRGB(outpath);
    } else {
        fprintf(stderr, "render_rt_proxy_shapes: render() failed\n");
    }
#else
    ok = renderToFile(root, outpath);
#endif

    root->unref();
    return ok ? 0 : 1;
}
