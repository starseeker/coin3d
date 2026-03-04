/*
 * render_scene_texture_multi_mgr.cpp
 *
 * Regression test for the crash that occurred when the same SoSceneTexture2
 * node was rendered by two different SoDB::ContextManager instances in
 * sequence (e.g. system-GL then OSMesa, or two separate OSMesa managers).
 *
 * Before the fix, SoSceneTexture2P::updateBuffer() would update
 * this->contextManager to the new manager without destroying the inner
 * context that was created by the OLD manager.  The new manager then called
 * destroyContext() on a pointer it did not own, which caused type confusion
 * and a crash inside OSMesaDestroyContext().
 *
 * This test exercises the fix by:
 *   1. Building a scene that contains a SoSceneTexture2 node.
 *   2. Rendering the scene with SoOffscreenRenderer A (manager A).
 *   3. Rendering the SAME scene root with SoOffscreenRenderer B (manager B),
 *      where B is a freshly created OSMesa context manager and therefore a
 *      different pointer than A.
 *
 * If SoDB::createOSMesaContextManager() is unavailable (non-OSMesa build)
 * the test passes trivially so it is safe to run in all build configurations.
 *
 * Returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <Inventor/SoDB.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoFaceSet.h>
#include <Inventor/nodes/SoSceneTexture2.h>
#include <Inventor/nodes/SoTextureCoordinate2.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>

static const int W = 128;
static const int H = 128;

static SoSeparator *buildTextureScene()
{
    SoSeparator *scene = new SoSeparator;
    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    scene->addChild(cam);
    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.3f, -0.7f, -0.6f);
    scene->addChild(light);
    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.9f, 0.5f, 0.1f);
    scene->addChild(mat);
    SoCone *cone = new SoCone;
    scene->addChild(cone);
    SbViewportRegion vp(64, 64);
    cam->viewAll(scene, vp);
    return scene;
}

static SoSeparator *buildMainScene()
{
    SoSeparator *root = new SoSeparator;

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 2.0f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 10.0f;
    cam->height       = 2.2f;
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(0.0f, 0.0f, -1.0f);
    root->addChild(light);

    SoSceneTexture2 *stex = new SoSceneTexture2;
    stex->size.setValue(SbVec2s(64, 64));
    stex->backgroundColor.setValue(0.0f, 0.0f, 0.2f, 1.0f);
    stex->type.setValue(SoSceneTexture2::RGBA8);
    stex->wrapS.setValue(SoSceneTexture2::CLAMP);
    stex->wrapT.setValue(SoSceneTexture2::CLAMP);
    stex->scene.setValue(buildTextureScene());
    root->addChild(stex);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(1.0f, 1.0f, 1.0f);
    root->addChild(mat);

    SoTextureCoordinate2 *tc = new SoTextureCoordinate2;
    tc->point.set1Value(0, SbVec2f(0.0f, 0.0f));
    tc->point.set1Value(1, SbVec2f(1.0f, 0.0f));
    tc->point.set1Value(2, SbVec2f(1.0f, 1.0f));
    tc->point.set1Value(3, SbVec2f(0.0f, 1.0f));
    root->addChild(tc);

    SoCoordinate3 *coords = new SoCoordinate3;
    coords->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
    coords->point.set1Value(1, SbVec3f( 1.0f, -1.0f, 0.0f));
    coords->point.set1Value(2, SbVec3f( 1.0f,  1.0f, 0.0f));
    coords->point.set1Value(3, SbVec3f(-1.0f,  1.0f, 0.0f));
    root->addChild(coords);

    SoFaceSet *fs = new SoFaceSet;
    fs->numVertices.setValue(4);
    root->addChild(fs);

    return root;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    const char *basepath = (argc > 1) ? argv[1] : "render_scene_texture_multi_mgr";

    /* Render the canonical factory scene as the primary output image.
     * This ensures obol_viewer and obol_render produce identical scenes. */
    {
        SoSeparator *fRoot = ObolTest::Scenes::createSceneTextureMultiMgr(256, 256);
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

    // Create two independent OSMesa context managers.  If OSMesa is not
    // available in this build both will be nullptr and we skip the test.
    SoDB::ContextManager *mgr_a = SoDB::createOSMesaContextManager();
    SoDB::ContextManager *mgr_b = SoDB::createOSMesaContextManager();

    if (!mgr_a || !mgr_b || mgr_a == mgr_b) {
        // Not an OSMesa build, both are null, or unexpectedly the same instance – skip.
        delete mgr_a;
        delete mgr_b;
        printf("render_scene_texture_multi_mgr: SKIP (OSMesa not available)\n");
        return 0;
    }

    SoSeparator *root = new SoSeparator;
    root->ref();
    root->addChild(buildMainScene());

    SbViewportRegion vp(W, H);

    // ---- First render with manager A ----------------------------------------
    bool ok_a = false;
    {
        SoOffscreenRenderer renderer(mgr_a, vp);
        renderer.setComponents(SoOffscreenRenderer::RGB);
        renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
        ok_a = renderer.render(root);
        if (!ok_a)
            fprintf(stderr, "render_scene_texture_multi_mgr: render A failed\n");
    }

    // ---- Second render with manager B (same scene root, different manager) ---
    // Without the fix this crashes in OSMesaDestroyContext because the inner
    // context created by manager A is wrongly destroyed through manager B.
    bool ok_b = false;
    {
        SoOffscreenRenderer renderer(mgr_b, vp);
        renderer.setComponents(SoOffscreenRenderer::RGB);
        renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
        ok_b = renderer.render(root);
        if (!ok_b)
            fprintf(stderr, "render_scene_texture_multi_mgr: render B failed\n");
    }

    root->unref();
    delete mgr_a;
    delete mgr_b;

    bool passed = ok_a && ok_b;
    printf("render_scene_texture_multi_mgr: %s\n", passed ? "PASS" : "FAIL");
    return passed ? 0 : 1;
}
