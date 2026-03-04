/*
 * render_scene_texture.cpp - Integration test: SoSceneTexture2 render-to-texture
 *
 * Exercises SoSceneTexture2 by rendering a small sub-scene (a colored cone)
 * into a 128×128 RGBA texture and then applying that texture to a large flat
 * quad (SoFaceSet) in the main scene.  The test validates that the
 * render-to-texture pipeline is functional by checking:
 *
 *   - The output image is not blank (the textured quad was rendered).
 *   - Non-background pixels are present in the image.
 *
 * The inner scene renders an orange cone on a dark background; the outer
 * scene applies that texture to a facing quad so the cone image should be
 * visible in the final render.
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
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

static const int W = 256;
static const int H = 256;

// Build the sub-scene that will be rendered into the texture:
// an orange cone on a dark blue background.
static SoSeparator *buildTextureScene()
{
    SoSeparator *scene = new SoSeparator;

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    scene->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.3f, -0.7f, -0.6f);
    scene->addChild(light);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.9f, 0.5f, 0.1f);  // orange cone
    mat->specularColor.setValue(0.8f, 0.8f, 0.8f);
    mat->shininess.setValue(0.5f);
    scene->addChild(mat);

    SoCone *cone = new SoCone;
    cone->bottomRadius.setValue(0.6f);
    cone->height.setValue(1.2f);
    scene->addChild(cone);

    SbViewportRegion vp(128, 128);
    cam->viewAll(scene, vp);

    return scene;
}

static bool validateScene(const unsigned char *buf)
{
    int nonbg = 0;
    for (int i = 0; i < W * H; ++i) {
        const unsigned char *p = buf + i * 3;
        // Any pixel with meaningful color is considered non-background
        if (p[0] > 15 || p[1] > 15 || p[2] > 15)
            ++nonbg;
    }
    printf("render_scene_texture: nonbg=%d\n", nonbg);

    if (nonbg < 100) {
        fprintf(stderr, "render_scene_texture: FAIL – scene appears blank\n");
        return false;
    }
    printf("render_scene_texture: PASS\n");
    return true;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    /* Render the canonical factory scene as the primary output image.
     * This ensures obol_viewer and obol_render produce identical scenes. */
    {
        const char *primaryBase = (argc > 1) ? argv[1] : "render_scene_texture";
        SoSeparator *fRoot = ObolTest::Scenes::createSceneTexture(256, 256);
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

    // Main scene camera – orthographic, looking at a unit quad in XY plane
    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 2.0f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 10.0f;
    cam->height       = 2.2f;
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(0.0f, 0.0f, -1.0f);
    root->addChild(light);

    // SoSceneTexture2: render the sub-scene into a 128×128 texture
    SoSceneTexture2 *stex = new SoSceneTexture2;
    stex->size.setValue(SbVec2s(128, 128));
    stex->backgroundColor.setValue(0.0f, 0.0f, 0.2f, 1.0f);  // dark blue bg
    stex->type.setValue(SoSceneTexture2::RGBA8);
    stex->wrapS.setValue(SoSceneTexture2::CLAMP);
    stex->wrapT.setValue(SoSceneTexture2::CLAMP);
    stex->scene.setValue(buildTextureScene());
    root->addChild(stex);

    // White material so the texture colors are not modulated away
    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(1.0f, 1.0f, 1.0f);
    root->addChild(mat);

    // Texture coordinates for the quad
    SoTextureCoordinate2 *tc = new SoTextureCoordinate2;
    tc->point.set1Value(0, SbVec2f(0.0f, 0.0f));
    tc->point.set1Value(1, SbVec2f(1.0f, 0.0f));
    tc->point.set1Value(2, SbVec2f(1.0f, 1.0f));
    tc->point.set1Value(3, SbVec2f(0.0f, 1.0f));
    root->addChild(tc);

    // A large quad facing the camera
    SoCoordinate3 *coords = new SoCoordinate3;
    coords->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
    coords->point.set1Value(1, SbVec3f( 1.0f, -1.0f, 0.0f));
    coords->point.set1Value(2, SbVec3f( 1.0f,  1.0f, 0.0f));
    coords->point.set1Value(3, SbVec3f(-1.0f,  1.0f, 0.0f));
    root->addChild(coords);

    SoFaceSet *fs = new SoFaceSet;
    fs->numVertices.setValue(4);
    root->addChild(fs);

    SbViewportRegion vp(W, H);
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_scene_texture.rgb");

    bool ok = false;
    if (renderer.render(root)) {
        const unsigned char *buf = renderer.getBuffer();
        bool pixOk = (buf != nullptr) && validateScene(buf);
        ok = pixOk && renderer.writeToRGB(outpath);
    } else {
        fprintf(stderr, "render_scene_texture: render() failed\n");
    }

    root->unref();
    return ok ? 0 : 1;
}
