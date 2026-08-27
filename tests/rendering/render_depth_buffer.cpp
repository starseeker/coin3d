#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"

#include <gtest/gtest.h>

#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoDepthBuffer.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoTranslation.h>

TEST(RenderSceneFactories, DepthBufferPreservesThreeFrameConfigurationContract)
{
    constexpr int W = 256;
    constexpr int H = 256;
    ObolTestSupport::RenderFixture fixture(W, H);
    ASSERT_TRUE(fixture.available());

    auto factory_scene = ObolTestSupport::makeScene(
        ObolTest::Scenes::createDepthBuffer, fixture);
    ASSERT_TRUE(fixture.render(factory_scene.root()));
    EXPECT_GT(fixture.nonBackgroundPixels(15), 20u);

    auto * root = new SoSeparator;
    root->ref();
    auto * camera = new SoPerspectiveCamera;
    camera->position.setValue(0.0f, 0.0f, 6.0f);
    camera->nearDistance = 0.5f;
    camera->farDistance = 30.0f;
    root->addChild(camera);
    auto * light = new SoDirectionalLight;
    light->direction.setValue(-0.3f, -0.7f, -0.6f);
    root->addChild(light);

    auto * depth = new SoDepthBuffer;
    root->addChild(depth);

    auto * cube_group = new SoSeparator;
    auto * cube_material = new SoMaterial;
    cube_material->diffuseColor.setValue(0.8f, 0.2f, 0.2f);
    cube_group->addChild(cube_material);
    auto * cube = new SoCube;
    cube->width = 1.4f;
    cube->height = 1.4f;
    cube->depth = 1.4f;
    cube_group->addChild(cube);
    root->addChild(cube_group);

    auto * sphere_group = new SoSeparator;
    auto * translation = new SoTranslation;
    translation->translation.setValue(0.0f, 0.0f, -2.0f);
    sphere_group->addChild(translation);
    auto * sphere_material = new SoMaterial;
    sphere_material->diffuseColor.setValue(0.2f, 0.4f, 0.9f);
    sphere_group->addChild(sphere_material);
    auto * sphere = new SoSphere;
    sphere->radius = 0.8f;
    sphere_group->addChild(sphere);
    root->addChild(sphere_group);

    // Normal depth test.
    depth->test = TRUE;
    depth->write = TRUE;
    depth->function = SoDepthBuffer::LEQUAL;
    ASSERT_TRUE(fixture.render(root));
    EXPECT_GE(fixture.nonBackgroundPixels(15), 100u);

    // Depth testing and writes disabled: the later sphere remains visible.
    depth->test = FALSE;
    depth->write = FALSE;
    ASSERT_TRUE(fixture.render(root));
    EXPECT_GE(fixture.nonBackgroundPixels(15), 100u);

    // Alternate comparison mode must still render successfully.
    depth->test = TRUE;
    depth->write = TRUE;
    depth->function = SoDepthBuffer::GEQUAL;
    ASSERT_TRUE(fixture.render(root));

    root->unref();
}
