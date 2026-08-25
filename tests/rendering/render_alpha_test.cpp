#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"

#include <gtest/gtest.h>

#include <Inventor/nodes/SoAlphaTest.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoFaceSet.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoNormal.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/nodes/SoTextureCoordinate2.h>

TEST(RenderSceneFactories, AlphaTestPreservesThreeFrameVisibilityContract)
{
    constexpr int W = 256;
    constexpr int H = 256;
    ObolTestSupport::RenderFixture fixture(W, H);
    ASSERT_TRUE(fixture.available());

    auto factory_scene = ObolTestSupport::makeScene(
        ObolTest::Scenes::createAlphaTest, fixture);
    ASSERT_TRUE(fixture.render(factory_scene.root()));
    EXPECT_GT(fixture.nonBackgroundPixels(15), 20u);

    auto * root = new SoSeparator;
    root->ref();

    auto * camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 5.0f);
    camera->height.setValue(2.2f);
    camera->nearDistance = 0.1f;
    camera->farDistance = 20.0f;
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);

    auto * alpha_test = new SoAlphaTest;
    root->addChild(alpha_test);

    auto * texture = new SoTexture2;
    constexpr int texture_size = 16;
    unsigned char texture_data[texture_size * texture_size * 4];
    for (int y = 0; y < texture_size; ++y) {
        for (int x = 0; x < texture_size; ++x) {
            const int index = (y * texture_size + x) * 4;
            const bool opaque = ((x + y) % 2) == 0;
            texture_data[index] = opaque ? 200 : 255;
            texture_data[index + 1] = opaque ? 50 : 255;
            texture_data[index + 2] = opaque ? 50 : 255;
            texture_data[index + 3] = opaque ? 255 : 0;
        }
    }
    texture->image.setValue(SbVec2s(texture_size, texture_size), 4,
                            texture_data);
    texture->wrapS.setValue(SoTexture2::REPEAT);
    texture->wrapT.setValue(SoTexture2::REPEAT);
    texture->model.setValue(SoTexture2::REPLACE);
    root->addChild(texture);

    auto * material = new SoMaterial;
    material->diffuseColor.setValue(1.0f, 1.0f, 1.0f);
    root->addChild(material);

    auto * texture_coordinates = new SoTextureCoordinate2;
    texture_coordinates->point.set1Value(0, SbVec2f(0.0f, 0.0f));
    texture_coordinates->point.set1Value(1, SbVec2f(4.0f, 0.0f));
    texture_coordinates->point.set1Value(2, SbVec2f(4.0f, 4.0f));
    texture_coordinates->point.set1Value(3, SbVec2f(0.0f, 4.0f));
    root->addChild(texture_coordinates);

    auto * normals = new SoNormal;
    normals->vector.set1Value(0, SbVec3f(0.0f, 0.0f, 1.0f));
    root->addChild(normals);
    auto * normal_binding = new SoNormalBinding;
    normal_binding->value.setValue(SoNormalBinding::OVERALL);
    root->addChild(normal_binding);

    auto * coordinates = new SoCoordinate3;
    coordinates->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
    coordinates->point.set1Value(1, SbVec3f(1.0f, -1.0f, 0.0f));
    coordinates->point.set1Value(2, SbVec3f(1.0f, 1.0f, 0.0f));
    coordinates->point.set1Value(3, SbVec3f(-1.0f, 1.0f, 0.0f));
    root->addChild(coordinates);

    auto * faces = new SoFaceSet;
    faces->numVertices.set1Value(0, 4);
    root->addChild(faces);

    // NONE: all fragments are present.
    alpha_test->function.setValue(SoAlphaTest::NONE);
    alpha_test->value.setValue(0.5f);
    ASSERT_TRUE(fixture.render(root));
    EXPECT_GE(fixture.nonBackgroundPixels(15), 100u);

    // GREATER 0.5: transparent texels are rejected, opaque texels remain.
    alpha_test->function.setValue(SoAlphaTest::GREATER);
    ASSERT_TRUE(fixture.render(root));
    EXPECT_GE(fixture.nonBackgroundPixels(15), 50u);

    // ALWAYS: all fragments pass again.
    alpha_test->function.setValue(SoAlphaTest::ALWAYS);
    ASSERT_TRUE(fixture.render(root));
    EXPECT_GE(fixture.nonBackgroundPixels(15), 100u);

    root->unref();
}
