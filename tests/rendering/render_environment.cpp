#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"

#include <gtest/gtest.h>

#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoEnvironment.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSphere.h>

TEST(RenderSceneFactories, EnvironmentLightingAndFogRemainVisible)
{
    constexpr int W = 256;
    constexpr int H = 256;
    ObolTestSupport::RenderFixture fixture(W, H);
    ASSERT_TRUE(fixture.available());

    // Keep the shared catalogue scene covered as well as the mutable-node
    // contract that the original test exercised directly.
    auto factory_scene = ObolTestSupport::makeScene(
        ObolTest::Scenes::createEnvironment, fixture);
    ASSERT_TRUE(fixture.render(factory_scene.root()));
    EXPECT_GT(fixture.nonBackgroundPixels(15), 100u);

    auto * root = new SoSeparator;
    root->ref();

    auto * camera = new SoPerspectiveCamera;
    camera->position.setValue(0.0f, 0.0f, 4.0f);
    camera->nearDistance = 0.1f;
    camera->farDistance = 50.0f;
    root->addChild(camera);

    auto * environment = new SoEnvironment;
    root->addChild(environment);

    auto * light = new SoDirectionalLight;
    light->direction.setValue(-0.3f, -0.7f, -0.6f);
    root->addChild(light);

    auto * material = new SoMaterial;
    material->diffuseColor.setValue(0.8f, 0.2f, 0.2f);
    material->ambientColor.setValue(0.8f, 0.2f, 0.2f);
    root->addChild(material);
    root->addChild(new SoSphere);

    // Frame 1: bright ambient lighting, no fog.
    environment->ambientIntensity.setValue(0.9f);
    environment->ambientColor.setValue(1.0f, 1.0f, 1.0f);
    environment->fogType.setValue(SoEnvironment::NONE);
    ASSERT_TRUE(fixture.render(root));
    EXPECT_GT(fixture.nonBackgroundPixels(15), 100u);

    // Frame 2: short-visibility fog.  The sphere should blend toward the fog
    // colour, but remain represented by non-background pixels.
    environment->ambientIntensity.setValue(0.3f);
    environment->fogType.setValue(SoEnvironment::FOG);
    environment->fogColor.setValue(0.8f, 0.8f, 0.8f);
    environment->fogVisibility.setValue(6.0f);
    ASSERT_TRUE(fixture.render(root));
    EXPECT_GT(fixture.nonBackgroundPixels(15), 100u);

    root->unref();
}
