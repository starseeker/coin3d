#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"
#include <gtest/gtest.h>

TEST(RenderSceneFactories, BackgroundGradientChangesRenderedRows)
{
    constexpr int W = 256;
    constexpr int H = 256;
    ObolTestSupport::RenderFixture fixture(W, H);
    ASSERT_TRUE(fixture.available());
    auto scene = ObolTestSupport::makeScene(
        ObolTest::Scenes::createBackgroundGradient, fixture);

    // Retain the original solid-render sanity pass before exercising the
    // renderer state under test.
    fixture.clearBackgroundGradient();
    ASSERT_TRUE(fixture.render(scene.root()));

    const SbColor bottom(0.05f, 0.05f, 0.25f);
    const SbColor top(0.40f, 0.60f, 0.85f);
    fixture.setBackgroundGradient(bottom, top);
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
