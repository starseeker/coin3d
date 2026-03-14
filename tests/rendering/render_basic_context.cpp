/*
 * render_basic_context.cpp
 *
 * Exercises Obol's graceful-degradation behaviour when given a "basic"
 * application-provided GL context — specifically one whose backing surface
 * is too small for direct glReadPixels readback and that may not expose
 * every GL extension Obol would ideally like.
 *
 * The test uses BasicFLTKContextManager, a minimal SoDB::ContextManager
 * that uses only FLTK's native 1×1 window context with no Pbuffers, no
 * FBConfig selection, and no per-context sharing.  This matches what an
 * application might hand to Obol if it builds its own Fl_Gl_Window and
 * registers that window's context as the renderer backend.
 *
 * Three rendering scenarios are exercised:
 *
 *   1. Plain geometry (coloured sphere) — should succeed on any usable GL
 *      context.  An FBO is created by CoinOffscreenGLCanvas for the outer
 *      render; if FBOs are unavailable the renderer falls back gracefully.
 *
 *   2. SoTexture2 (static image texture) — should succeed when texture
 *      objects are supported (OpenGL >= 1.1); degrades silently otherwise.
 *
 *   3. SoSceneTexture2 (render-to-texture) — the original crash scenario.
 *      With BasicFLTKContextManager:
 *        (a) If the inner context supports FBOs, a temporary FBO is created
 *            and the scene texture works correctly.
 *        (b) If FBOs are unavailable, Obol detects the 1×1 surface via
 *            getActualSurfaceSize(), skips the render, and posts a warning.
 *      In either case the test must NOT crash and the outer scene must
 *      still produce a valid (possibly blank-textured) image.
 *
 * The test writes argv[1]+".rgb" (plain geometry result) and returns 0 on
 * pass, 1 on fail.  "Pass" means: no crash, no memory corruption, and the
 * plain geometry scene produces non-blank pixels.
 */

/* Must come before any Inventor headers when using BasicFLTKContextManager */
#ifdef OBOL_VIEWER_FLTK_GL
#  include "utils/fltk_context_manager.h"
#else
#  include "headless_utils.h"
#endif

#include <Inventor/SoDB.h>
#include <Inventor/nodekits/SoNodeKit.h>
#include <Inventor/SoInteraction.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/nodes/SoSceneTexture2.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoFaceSet.h>
#include <Inventor/nodes/SoTextureCoordinate2.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/misc/SoGLDriverDatabase.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>
#include <cstring>

static const int W = 256;
static const int H = 256;

/* ----------------------------------------------------------------------- */
/* Scene builders                                                           */
/* ----------------------------------------------------------------------- */

/* Scenario 1: plain coloured sphere — baseline geometry rendering. */
static SoSeparator* buildPlainGeometry()
{
    SoSeparator* root = new SoSeparator;

    SoPerspectiveCamera* cam = new SoPerspectiveCamera;
    root->addChild(cam);

    SoDirectionalLight* light = new SoDirectionalLight;
    light->direction.setValue(-0.5f, -0.7f, -0.5f);
    root->addChild(light);

    SoMaterial* mat = new SoMaterial;
    mat->diffuseColor.setValue(0.2f, 0.5f, 0.9f);   /* blue sphere */
    mat->specularColor.setValue(0.8f, 0.8f, 0.8f);
    mat->shininess.setValue(0.6f);
    root->addChild(mat);

    SoSphere* sphere = new SoSphere;
    sphere->radius.setValue(0.8f);
    root->addChild(sphere);

    SbViewportRegion vp(W, H);
    cam->viewAll(root, vp);
    return root;
}

/* Scenario 2: static image texture on a sphere. */
static SoSeparator* buildTexturedGeometry()
{
    SoSeparator* root = new SoSeparator;

    SoPerspectiveCamera* cam = new SoPerspectiveCamera;
    root->addChild(cam);

    SoDirectionalLight* light = new SoDirectionalLight;
    root->addChild(light);

    /* Build a simple 8×8 RGB checkerboard in memory. */
    static unsigned char checker[8 * 8 * 3];
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            bool white = ((x + y) & 1) == 0;
            unsigned char* p = checker + (y * 8 + x) * 3;
            p[0] = white ? 220 : 40;
            p[1] = white ? 220 : 40;
            p[2] = white ? 220 : 40;
        }
    }

    SoTexture2* tex = new SoTexture2;
    tex->image.setValue(SbVec2s(8, 8), 3, checker);
    root->addChild(tex);

    SoMaterial* mat = new SoMaterial;
    mat->diffuseColor.setValue(1.0f, 1.0f, 1.0f);
    root->addChild(mat);

    SoSphere* sphere = new SoSphere;
    sphere->radius.setValue(0.8f);
    root->addChild(sphere);

    SbViewportRegion vp(W, H);
    cam->viewAll(root, vp);
    return root;
}

/* Scenario 3: SoSceneTexture2 applied to a flat quad.
 * The inner scene renders an orange cone into a 128×128 texture. */
static SoSeparator* buildSceneTextureScene()
{
    /* Inner scene -------------------------------------------------------- */
    SoSeparator* innerScene = new SoSeparator;
    {
        SoPerspectiveCamera* cam = new SoPerspectiveCamera;
        innerScene->addChild(cam);

        SoDirectionalLight* light = new SoDirectionalLight;
        light->direction.setValue(-0.3f, -0.7f, -0.6f);
        innerScene->addChild(light);

        SoMaterial* mat = new SoMaterial;
        mat->diffuseColor.setValue(0.9f, 0.5f, 0.1f);  /* orange */
        innerScene->addChild(mat);

        SoCone* cone = new SoCone;
        cone->bottomRadius.setValue(0.6f);
        cone->height.setValue(1.2f);
        innerScene->addChild(cone);

        SbViewportRegion vp(128, 128);
        cam->viewAll(innerScene, vp);
    }

    /* Outer scene -------------------------------------------------------- */
    SoSeparator* root = new SoSeparator;

    SoOrthographicCamera* cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 2.0f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 10.0f;
    cam->height       = 2.2f;
    root->addChild(cam);

    SoDirectionalLight* light = new SoDirectionalLight;
    light->direction.setValue(0.0f, 0.0f, -1.0f);
    root->addChild(light);

    SoSceneTexture2* stex = new SoSceneTexture2;
    stex->size.setValue(SbVec2s(128, 128));
    stex->backgroundColor.setValue(0.0f, 0.0f, 0.2f, 1.0f);
    stex->type.setValue(SoSceneTexture2::RGBA8);
    stex->wrapS.setValue(SoSceneTexture2::CLAMP);
    stex->wrapT.setValue(SoSceneTexture2::CLAMP);
    stex->scene.setValue(innerScene);
    root->addChild(stex);

    SoMaterial* mat = new SoMaterial;
    mat->diffuseColor.setValue(1.0f, 1.0f, 1.0f);
    root->addChild(mat);

    SoTextureCoordinate2* tc = new SoTextureCoordinate2;
    tc->point.set1Value(0, SbVec2f(0.0f, 0.0f));
    tc->point.set1Value(1, SbVec2f(1.0f, 0.0f));
    tc->point.set1Value(2, SbVec2f(1.0f, 1.0f));
    tc->point.set1Value(3, SbVec2f(0.0f, 1.0f));
    root->addChild(tc);

    SoCoordinate3* coords = new SoCoordinate3;
    coords->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
    coords->point.set1Value(1, SbVec3f( 1.0f, -1.0f, 0.0f));
    coords->point.set1Value(2, SbVec3f( 1.0f,  1.0f, 0.0f));
    coords->point.set1Value(3, SbVec3f(-1.0f,  1.0f, 0.0f));
    root->addChild(coords);

    SoFaceSet* fs = new SoFaceSet;
    fs->numVertices.setValue(4);
    root->addChild(fs);

    return root;
}

/* ----------------------------------------------------------------------- */
/* Pixel validation helpers                                                 */
/* ----------------------------------------------------------------------- */

static int countNonBlackPixels(const unsigned char* buf, int w, int h, int nc)
{
    int count = 0;
    for (int i = 0; i < w * h; ++i) {
        const unsigned char* p = buf + i * nc;
        if (p[0] > 20 || p[1] > 20 || p[2] > 20)
            ++count;
    }
    return count;
}

/* ----------------------------------------------------------------------- */
/* Test runner                                                              */
/* ----------------------------------------------------------------------- */

int main(int argc, char** argv)
{
    /* ------------------------------------------------------------------ */
    /* Initialise with BasicFLTKContextManager (FLTK-window-only context). */
    /* ------------------------------------------------------------------ */
#ifdef OBOL_VIEWER_FLTK_GL
    /* FLTK_GL build: use BasicFLTKContextManager directly. */
    BasicFLTKContextManager basicMgr;
    SoDB::init(&basicMgr);
    SoNodeKit::init();
    SoInteraction::init();
    printf("render_basic_context: using BasicFLTKContextManager "
           "(FLTK 1×1 window, no Pbuffers)\n");
#else
    /* Headless (OSMesa) build: still exercises the hardened code paths,
     * though OSMesa itself provides a full-featured context.  The test
     * remains valuable for the GL wrapper / SoGLDisplayList hardening. */
    initCoinHeadless();
    printf("render_basic_context: using headless OSMesa context manager\n");
#endif

    SoGLDriverDatabase::init();

    char outbase[4096];
    if (argc > 1)
        snprintf(outbase, sizeof(outbase), "%s", argv[1]);
    else
        snprintf(outbase, sizeof(outbase), "render_basic_context");

    bool all_ok = true;

    /* ------------------------------------------------------------------ */
    /* Scenario 1: plain geometry — must always produce non-blank output.  */
    /* ------------------------------------------------------------------ */
    printf("render_basic_context: [1] plain geometry ... ");
    {
        SoSeparator* root = buildPlainGeometry();
        root->ref();

        SbViewportRegion vp(W, H);
        SoOffscreenRenderer renderer(vp);
        renderer.setComponents(SoOffscreenRenderer::RGB);
        renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

        bool rendered = renderer.render(root) == TRUE;
        int nonBlack = 0;
        if (rendered) {
            const unsigned char* buf = renderer.getBuffer();
            if (buf) nonBlack = countNonBlackPixels(buf, W, H, 3);
        }

        if (!rendered || nonBlack < (W * H / 8)) {
            printf("FAIL (rendered=%d nonBlack=%d)\n", (int)rendered, nonBlack);
            all_ok = false;
        } else {
            /* Write output image. */
            char path[sizeof(outbase) + 4];
            snprintf(path, sizeof(path), "%s.rgb", outbase);
            renderer.writeToRGB(path);
            printf("PASS (nonBlack=%d)\n", nonBlack);
        }
        root->unref();
    }

    /* ------------------------------------------------------------------ */
    /* Scenario 2: static texture — must not crash; pixel check lenient.   */
    /* ------------------------------------------------------------------ */
    printf("render_basic_context: [2] static SoTexture2 ... ");
    {
        SoSeparator* root = buildTexturedGeometry();
        root->ref();

        SbViewportRegion vp(W, H);
        SoOffscreenRenderer renderer(vp);
        renderer.setComponents(SoOffscreenRenderer::RGB);
        renderer.setBackgroundColor(SbColor(0.05f, 0.05f, 0.05f));

        bool rendered = renderer.render(root) == TRUE;
        /* As long as it didn't crash, the test passes — a basic context
         * without texture objects will fall back to flat shading. */
        printf("%s\n", rendered ? "PASS (no crash)" : "PASS (render failed gracefully)");
        root->unref();
    }

    /* ------------------------------------------------------------------ */
    /* Scenario 3: SoSceneTexture2 — the original crash scenario.          */
    /* Must NOT crash regardless of context capability.                    */
    /* ------------------------------------------------------------------ */
    printf("render_basic_context: [3] SoSceneTexture2 ... ");
    {
        SoSeparator* root = buildSceneTextureScene();
        root->ref();

        SbViewportRegion vp(W, H);
        SoOffscreenRenderer renderer(vp);
        renderer.setComponents(SoOffscreenRenderer::RGB);
        renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

        /* render() is allowed to fail (e.g. SoSceneTexture2 skips the
         * inner render because it detected the surface is too small).
         * What is NOT allowed is a crash or memory corruption. */
        bool rendered = renderer.render(root) == TRUE;
        /* If it rendered, check that we got some pixels. */
        int nonBlack = 0;
        if (rendered) {
            const unsigned char* buf = renderer.getBuffer();
            if (buf) nonBlack = countNonBlackPixels(buf, W, H, 3);
        }
        printf("PASS (rendered=%d nonBlack=%d — no crash)\n",
               (int)rendered, nonBlack);
        root->unref();
    }

    printf("render_basic_context: overall %s\n", all_ok ? "PASS" : "FAIL");
    return all_ok ? 0 : 1;
}
