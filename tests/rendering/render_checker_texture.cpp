#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"
#include <gtest/gtest.h>

TEST(RenderSceneFactories, CheckerTexturePreservesBlackAndWhiteRegions)
{
    constexpr int W = 512;
    constexpr int H = 512;
    ObolTestSupport::RenderFixture fixture(W, H, SbColor(0.2f, 0.3f, 0.4f));
    ASSERT_TRUE(fixture.available());
    auto scene = ObolTestSupport::makeScene(
        ObolTest::Scenes::createCheckerTexture, fixture);
    ASSERT_TRUE(fixture.render(scene.root()));

    int black = 0;
    int white = 0;
    int other = 0;
    const auto & pixels = fixture.pixels();
    for (int y = H / 4; y < 3 * H / 4; y += 8) {
        for (int x = W / 4; x < 3 * W / 4; x += 8) {
            const unsigned char * p = pixels.data() + (y * W + x) * 3;
            if (p[0] < 32 && p[1] < 32 && p[2] < 32) ++black;
            else if (p[0] > 220 && p[1] > 220 && p[2] > 220) ++white;
            else ++other;
        }
    }

    const int total = black + white + other;
    EXPECT_GT(total, 0);
    EXPECT_GT(black, total / 10);
    EXPECT_GT(white, total / 10);
    EXPECT_LT(other, total / 2);
}
