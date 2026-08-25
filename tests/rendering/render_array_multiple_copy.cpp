#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"
#include <gtest/gtest.h>

TEST(RenderSceneFactories, ArrayAndMultipleCopyPreserveVisibleGeometry)
{
    constexpr int W = 512;
    constexpr int H = 512;
    ObolTestSupport::RenderFixture fixture(W, H);
    ASSERT_TRUE(fixture.available());
    auto scene = ObolTestSupport::makeScene(
        ObolTest::Scenes::createArrayMultipleCopy, fixture);
    ASSERT_TRUE(fixture.render(scene.root()));

    int non_background = 0;
    const auto & pixels = fixture.pixels();
    for (int y = 0; y < H; y += 4) {
        for (int x = 0; x < W; x += 4) {
            const unsigned char * p = pixels.data() + (y * W + x) * 3;
            if (p[0] >= 10 || p[1] >= 10 || p[2] >= 10) ++non_background;
        }
    }
    EXPECT_GE(non_background, 50);
}
