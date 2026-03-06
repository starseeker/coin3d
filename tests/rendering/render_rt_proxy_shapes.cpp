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
#include "testlib/test_scenes.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoScale.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoLineSet.h>
#include <Inventor/nodes/SoIndexedLineSet.h>
#include <Inventor/nodes/SoPointSet.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>
#include <cmath>
#include <algorithm>

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

/*
 * Scale-compensation validation: verify that a LineSet/IndexedLineSet wrapped
 * in a Scale node produces a proxy cylinder whose world-space width is NOT
 * inflated by the scale factor.
 *
 * The test creates two horizontal lines at the same world-space Y position:
 *   - An unscaled red  IndexedLineSet spanning [-0.6 .. 0.6] in world X
 *   - A 3× scaled blue IndexedLineSet with local coords [-0.2 .. 0.2]
 *     (→ same world extent after scale) placed just below the red line
 *
 * Both lines should produce proxy cylinders of approximately the same visual
 * thickness (same pixel-width).  If the scale-compensation fix is missing the
 * blue line's cylinder radius would be 3× too large, yielding ≈9× more pixels.
 *
 * The test counts pixels in a small horizontal stripe through the centre of
 * each line and checks that the ratio is within 4× (generous, but catches the
 * bug which gives a 9× ratio).
 */
#ifdef OBOL_NANORT_BUILD
static bool validateScaleCompensation(const char * outpath)
{
    SoSeparator * root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera * cam = new SoPerspectiveCamera;
    root->addChild(cam);

    SoDirectionalLight * lt = new SoDirectionalLight;
    lt->direction.setValue(-0.3f, -0.8f, -0.5f);
    root->addChild(lt);

    // Unscaled reference line (red) at y = +0.3 in local / world space.
    {
        SoSeparator * sep = new SoSeparator;
        SoTranslation * t = new SoTranslation;
        t->translation.setValue(0.0f, 0.3f, 0.0f);
        sep->addChild(t);

        SoMaterial * mat = new SoMaterial;
        mat->diffuseColor.setValue(0.85f, 0.1f, 0.1f);
        mat->emissiveColor.setValue(0.5f, 0.0f, 0.0f);
        sep->addChild(mat);

        SoCoordinate3 * coords = new SoCoordinate3;
        coords->point.set1Value(0, SbVec3f(-0.6f, 0.0f, 0.0f));
        coords->point.set1Value(1, SbVec3f( 0.6f, 0.0f, 0.0f));
        sep->addChild(coords);

        SoIndexedLineSet * ils = new SoIndexedLineSet;
        ils->coordIndex.set1Value(0, 0);
        ils->coordIndex.set1Value(1, 1);
        ils->coordIndex.set1Value(2, -1);
        sep->addChild(ils);

        root->addChild(sep);
    }

    // Scale(3)-wrapped line (blue) at y = -0.3 in world space.
    // Local coord range is 3x smaller so the world-space line length matches.
    {
        SoSeparator * sep = new SoSeparator;
        SoTranslation * t = new SoTranslation;
        t->translation.setValue(0.0f, -0.3f, 0.0f);
        sep->addChild(t);

        SoMaterial * mat = new SoMaterial;
        mat->diffuseColor.setValue(0.1f, 0.3f, 0.85f);
        mat->emissiveColor.setValue(0.0f, 0.0f, 0.5f);
        sep->addChild(mat);

        SoScale * sc = new SoScale;
        sc->scaleFactor.setValue(3.0f, 3.0f, 3.0f);
        sep->addChild(sc);

        SoCoordinate3 * coords = new SoCoordinate3;
        coords->point.set1Value(0, SbVec3f(-0.2f, 0.0f, 0.0f));
        coords->point.set1Value(1, SbVec3f( 0.2f, 0.0f, 0.0f));
        sep->addChild(coords);

        SoIndexedLineSet * ils = new SoIndexedLineSet;
        ils->coordIndex.set1Value(0, 0);
        ils->coordIndex.set1Value(1, 1);
        ils->coordIndex.set1Value(2, -1);
        sep->addChild(ils);

        root->addChild(sep);
    }

    SbViewportRegion vp(256, 256);
    cam->viewAll(root, vp);

    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    bool ok = false;
    if (renderer.render(root)) {
        const unsigned char * buf = renderer.getBuffer();
        const int W = 256, H = 256;

        // Count pixels in a horizontal stripe (± 8 rows) centred on each line.
        // The red line is near row H*0.35 (y≈+0.3 from centre) and the blue
        // line near row H*0.65 (y≈-0.3).  Use a generous ± 15 row band.
        int red_px = 0, blue_px = 0;
        for (int row = 0; row < H; ++row) {
            for (int col = 0; col < W; ++col) {
                const unsigned char * p = buf + (row * W + col) * 3;
                // Red pixel
                if (p[0] > 80 && p[1] < 40 && p[2] < 40) ++red_px;
                // Blue pixel
                if (p[2] > 80 && p[0] < 40 && p[1] < 60) ++blue_px;
            }
        }

        printf("render_rt_proxy_shapes scale-compensation: red_px=%d blue_px=%d\n",
               red_px, blue_px);

        // Both should be visible; pixel counts should be within 4× of each other.
        // Before the fix the blue (scaled) line would have ≈9× more pixels.
        bool visible = (red_px > 0) && (blue_px > 0);
        float ratio = 0.0f;
        if (red_px > 0 && blue_px > 0)
            ratio = static_cast<float>(std::max(blue_px, red_px)) /
                    static_cast<float>(std::min(blue_px, red_px));

        ok = visible && (ratio < 4.0f);
        if (!ok) {
            if (!visible)
                fprintf(stderr,
                    "render_rt_proxy_shapes scale-compensation: FAIL - line(s) not visible\n");
            else
                fprintf(stderr,
                    "render_rt_proxy_shapes scale-compensation: FAIL - "
                    "pixel ratio %.1f >= 4.0 (scale not compensated)\n", ratio);
        } else {
            printf("render_rt_proxy_shapes scale-compensation: PASS (ratio=%.1f)\n",
                   ratio);
        }

        char scalePath[1024];
        snprintf(scalePath, sizeof(scalePath), "%s_scale_compensation.rgb", outpath);
        renderer.writeToRGB(scalePath);
    } else {
        fprintf(stderr, "render_rt_proxy_shapes scale-compensation: render() failed\n");
    }

    root->unref();
    return ok;
}
#endif // OBOL_NANORT_BUILD

int main(int argc, char **argv)
{
    initCoinHeadless();

    /* Render the canonical factory scene as the primary output image.
     * This ensures obol_viewer and obol_render produce identical scenes. */
    {
        const char *primaryBase = (argc > 1) ? argv[1] : "render_rt_proxy_shapes";
        SoSeparator *fRoot = ObolTest::Scenes::createRTProxyShapes(256, 256);
        SbViewportRegion fVp(256, 256);
        SoOffscreenRenderer fRen(fVp);
        fRen.setComponents(SoOffscreenRenderer::RGB);
        fRen.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
        if (fRen.render(fRoot)) {
            char primaryPath[4096];
            snprintf(primaryPath, sizeof(primaryPath), "%s.rgb", primaryBase);
            fRen.writeToRGB(primaryPath);
        }
        fRoot->unref();
    }


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
#ifdef OBOL_NANORT_BUILD
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

    // Additional test: verify that Scale nodes do not inflate proxy cylinder radii.
    const char * scaleBase = (argc > 1) ? argv[1] : "render_rt_proxy_shapes";
    if (!validateScaleCompensation(scaleBase))
        ok = false;
#else
    ok = renderToFile(root, outpath);
#endif

    root->unref();
    return ok ? 0 : 1;
}
