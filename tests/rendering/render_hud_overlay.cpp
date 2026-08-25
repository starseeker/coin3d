#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"
#include <gtest/gtest.h>

TEST(RenderSceneFactories, HudOverlayRendersVisibleGeometry)
{
    ObolTestSupport::RenderFixture fixture(
        800, 600, SbColor(0.08f, 0.08f, 0.12f));
    ASSERT_TRUE(fixture.available());
    auto scene = ObolTestSupport::makeScene(
        ObolTest::Scenes::createHUDOverlay, fixture);
    ASSERT_TRUE(fixture.render(scene.root()));
    EXPECT_GT(fixture.nonBackgroundPixels(), 20u);
}
