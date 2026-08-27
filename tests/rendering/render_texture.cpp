#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"
#include <gtest/gtest.h>

TEST(RenderSceneFactories, TextureSceneRendersVisibleGeometry)
{
    ObolTestSupport::RenderFixture fixture(800, 600);
    ASSERT_TRUE(fixture.available());
    auto scene = ObolTestSupport::makeScene(ObolTest::Scenes::createTexture,
                                            fixture);
    ASSERT_TRUE(fixture.render(scene.root()));
    EXPECT_GT(fixture.nonBackgroundPixels(), 20u);
}
