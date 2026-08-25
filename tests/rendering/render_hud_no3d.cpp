#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"
#include <gtest/gtest.h>

TEST(RenderSceneFactories, HudWithoutThreeDGeometryRendersVisibleOverlay)
{
    ObolTestSupport::RenderFixture fixture(
        800, 600, SbColor(0.05f, 0.05f, 0.10f));
    ASSERT_TRUE(fixture.available());
    auto scene = ObolTestSupport::makeScene(
        ObolTest::Scenes::createHUDNo3D, fixture);
    ASSERT_TRUE(fixture.render(scene.root()));
    EXPECT_GT(fixture.nonBackgroundPixels(), 20u);
}
