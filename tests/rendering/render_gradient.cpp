#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"
#include <gtest/gtest.h>

TEST(RenderSceneFactories, GradientBackgroundPreservesVerticalColourChange)
{
    constexpr int W = 800;
    constexpr int H = 600;
    ObolTestSupport::RenderFixture fixture(W, H);
    ASSERT_TRUE(fixture.available());
    fixture.setBackgroundGradient(SbColor(0.05f, 0.05f, 0.20f),
                                  SbColor(0.20f, 0.35f, 0.60f));
    auto scene = ObolTestSupport::makeScene(ObolTest::Scenes::createGradient,
                                            fixture);
    ASSERT_TRUE(fixture.render(scene.root()));

    double top_blue = 0.0;
    double bottom_blue = 0.0;
    int top_count = 0;
    int bottom_count = 0;
    const auto & pixels = fixture.pixels();
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const unsigned char blue = pixels[(y * W + x) * 3 + 2];
            if (y < H / 4) {
                bottom_blue += blue;
                ++bottom_count;
            } else if (y >= H - H / 4) {
                top_blue += blue;
                ++top_count;
            }
        }
    }
    ASSERT_GT(top_count, 0);
    ASSERT_GT(bottom_count, 0);
    EXPECT_GT(top_blue / top_count, bottom_blue / bottom_count + 20.0);
}
