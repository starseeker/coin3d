#include "test_scenes.h"

#include <Inventor/SbVec2f.h>
#include <Inventor/SbVec2s.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoFaceSet.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoSceneTexture2.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTextureCoordinate2.h>

namespace ObolTest {
namespace Scenes {
namespace {

SoSeparator * buildConeSubScene()
{
    SoSeparator *scene = new SoSeparator;
    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    scene->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.3f, -0.7f, -0.6f);
    scene->addChild(light);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.9f, 0.5f, 0.1f);
    mat->specularColor.setValue(0.8f, 0.8f, 0.8f);
    mat->shininess.setValue(0.5f);
    scene->addChild(mat);

    SoCone *cone = new SoCone;
    cone->bottomRadius.setValue(0.6f);
    cone->height.setValue(1.2f);
    scene->addChild(cone);

    cam->viewAll(scene, SbViewportRegion(128, 128));
    return scene;
}

SoSeparator * buildSceneTextureScene(const short textureSize)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 2.0f);
    cam->nearDistance = 0.1f;
    cam->farDistance = 10.0f;
    cam->height = 2.2f;
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(0.0f, 0.0f, -1.0f);
    root->addChild(light);

    SoSceneTexture2 *texture = new SoSceneTexture2;
    texture->size.setValue(SbVec2s(textureSize, textureSize));
    texture->backgroundColor.setValue(0.0f, 0.0f, 0.2f, 1.0f);
    texture->type.setValue(SoSceneTexture2::RGBA8);
    texture->wrapS.setValue(SoSceneTexture2::CLAMP);
    texture->wrapT.setValue(SoSceneTexture2::CLAMP);
    texture->scene.setValue(buildConeSubScene());
    root->addChild(texture);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(1.0f, 1.0f, 1.0f);
    root->addChild(mat);

    SoTextureCoordinate2 *coordinates = new SoTextureCoordinate2;
    coordinates->point.set1Value(0, SbVec2f(0.0f, 0.0f));
    coordinates->point.set1Value(1, SbVec2f(1.0f, 0.0f));
    coordinates->point.set1Value(2, SbVec2f(1.0f, 1.0f));
    coordinates->point.set1Value(3, SbVec2f(0.0f, 1.0f));
    root->addChild(coordinates);

    SoCoordinate3 *vertices = new SoCoordinate3;
    vertices->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
    vertices->point.set1Value(1, SbVec3f(1.0f, -1.0f, 0.0f));
    vertices->point.set1Value(2, SbVec3f(1.0f, 1.0f, 0.0f));
    vertices->point.set1Value(3, SbVec3f(-1.0f, 1.0f, 0.0f));
    root->addChild(vertices);

    SoFaceSet *face = new SoFaceSet;
    face->numVertices.setValue(4);
    root->addChild(face);
    return root;
}

} // namespace

SoSeparator * createSceneTexture(int width, int height)
{
    (void)width;
    (void)height;
    return buildSceneTextureScene(128);
}

SoSeparator * createSceneTextureMultiMgr(int width, int height)
{
    (void)width;
    (void)height;
    return buildSceneTextureScene(64);
}

} // namespace Scenes
} // namespace ObolTest
