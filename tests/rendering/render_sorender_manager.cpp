#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"

#include <gtest/gtest.h>

#include <Inventor/SbColor4f.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoRenderManager.h>

#include <cmath>

TEST(RenderSceneFactories, SoRenderManagerPreservesConfigurationAndRenders)
{
    constexpr int W = 256;
    constexpr int H = 256;
    ObolTestSupport::RenderFixture fixture(
        W, H, SbColor(0.1f, 0.2f, 0.3f));
    ASSERT_TRUE(fixture.available());
    auto scene = ObolTestSupport::makeScene(
        ObolTest::Scenes::createSoRenderManager, fixture);

    SoRenderManager manager;
    manager.setViewportRegion(SbViewportRegion(W, H));
    manager.setSceneGraph(scene.root());
    manager.setAutoClipping(SoRenderManager::VARIABLE_NEAR_PLANE);
    EXPECT_EQ(manager.getAutoClipping(), SoRenderManager::VARIABLE_NEAR_PLANE);

    const SbColor4f expected_background(0.1f, 0.2f, 0.3f, 1.0f);
    manager.setBackgroundColor(expected_background);
    const SbColor4f & actual_background = manager.getBackgroundColor();
    EXPECT_NEAR(actual_background[0], expected_background[0], 1e-5f);
    EXPECT_NEAR(actual_background[1], expected_background[1], 1e-5f);
    EXPECT_NEAR(actual_background[2], expected_background[2], 1e-5f);
    EXPECT_NE(manager.getGLRenderAction(), nullptr);

    ASSERT_TRUE(fixture.render(scene.root()));
    EXPECT_GT(fixture.nonBackgroundPixels(), 20u);
}
