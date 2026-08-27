#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"
#include <gtest/gtest.h>

TEST(RenderSceneFactories, BumpMapSceneRendersVisibleGeometry)
{
    ObolTestSupport::RenderFixture fixture(256, 256);
    ASSERT_TRUE(fixture.available());
    auto scene = ObolTestSupport::makeScene(ObolTest::Scenes::createBumpMap,
                                            fixture);
    ASSERT_TRUE(fixture.render(scene.root()));
    EXPECT_GT(fixture.nonBackgroundPixels(15), 100u);
}
