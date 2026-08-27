#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"

#include <gtest/gtest.h>

#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoFaceSet.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoNormal.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/nodes/SoTexture2Transform.h>
#include <Inventor/nodes/SoTextureCoordinate2.h>
#include <Inventor/nodes/SoTranslation.h>

namespace {

SoSeparator * buildTexturedQuad(const bool with_transform)
{
    auto * separator = new SoSeparator;

    auto * texture = new SoTexture2;
    constexpr int size = 32;
    unsigned char data[size * size * 3];
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const int index = (y * size + x) * 3;
            const bool red = (((x / 4) + (y / 4)) % 2) == 0;
            data[index] = red ? 200 : 255;
            data[index + 1] = red ? 40 : 255;
            data[index + 2] = red ? 40 : 255;
        }
    }
    texture->image.setValue(SbVec2s(size, size), 3, data);
    texture->wrapS.setValue(SoTexture2::REPEAT);
    texture->wrapT.setValue(SoTexture2::REPEAT);
    separator->addChild(texture);

    if (with_transform) {
        auto * transform = new SoTexture2Transform;
        transform->scaleFactor.setValue(2.0f, 2.0f);
        transform->rotation.setValue(0.785398f);
        transform->translation.setValue(0.1f, 0.1f);
        separator->addChild(transform);
    }

    auto * material = new SoMaterial;
    material->diffuseColor.setValue(1.0f, 1.0f, 1.0f);
    separator->addChild(material);

    auto * texture_coordinates = new SoTextureCoordinate2;
    texture_coordinates->point.set1Value(0, SbVec2f(0.0f, 0.0f));
    texture_coordinates->point.set1Value(1, SbVec2f(1.0f, 0.0f));
    texture_coordinates->point.set1Value(2, SbVec2f(1.0f, 1.0f));
    texture_coordinates->point.set1Value(3, SbVec2f(0.0f, 1.0f));
    separator->addChild(texture_coordinates);

    auto * normal = new SoNormal;
    normal->vector.set1Value(0, SbVec3f(0.0f, 0.0f, 1.0f));
    separator->addChild(normal);
    auto * normal_binding = new SoNormalBinding;
    normal_binding->value.setValue(SoNormalBinding::OVERALL);
    separator->addChild(normal_binding);

    auto * coordinates = new SoCoordinate3;
    coordinates->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
    coordinates->point.set1Value(1, SbVec3f(1.0f, -1.0f, 0.0f));
    coordinates->point.set1Value(2, SbVec3f(1.0f, 1.0f, 0.0f));
    coordinates->point.set1Value(3, SbVec3f(-1.0f, 1.0f, 0.0f));
    separator->addChild(coordinates);

    auto * faces = new SoFaceSet;
    faces->numVertices.set1Value(0, 4);
    separator->addChild(faces);
    return separator;
}

} // namespace

TEST(RenderSceneFactories, TextureTransformPreservesVisibleTexturedQuads)
{
    constexpr int W = 512;
    constexpr int H = 256;
    ObolTestSupport::RenderFixture fixture(
        W, H, SbColor(0.05f, 0.05f, 0.05f));
    ASSERT_TRUE(fixture.available());

    auto factory_scene = ObolTestSupport::makeScene(
        ObolTest::Scenes::createTextureTransform, fixture);
    ASSERT_TRUE(fixture.render(factory_scene.root()));
    EXPECT_GT(fixture.nonBackgroundPixels(15), 20u);

    auto * root = new SoSeparator;
    root->ref();
    auto * camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 5.0f);
    camera->height.setValue(2.5f);
    camera->nearDistance = 0.1f;
    camera->farDistance = 20.0f;
    root->addChild(camera);
    auto * light = new SoDirectionalLight;
    light->direction.setValue(0.0f, 0.0f, -1.0f);
    root->addChild(light);

    auto * left = new SoSeparator;
    auto * left_translation = new SoTranslation;
    left_translation->translation.setValue(-1.3f, 0.0f, 0.0f);
    left->addChild(left_translation);
    left->addChild(buildTexturedQuad(false));
    root->addChild(left);

    auto * right = new SoSeparator;
    auto * right_translation = new SoTranslation;
    right_translation->translation.setValue(1.3f, 0.0f, 0.0f);
    right->addChild(right_translation);
    right->addChild(buildTexturedQuad(true));
    root->addChild(right);

    ASSERT_TRUE(fixture.render(root));
    EXPECT_GT(fixture.nonBackgroundPixels(15), 200u);
    root->unref();
}
