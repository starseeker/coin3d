#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"
#include <gtest/gtest.h>

TEST(RenderSceneFactories, AsciiTextSceneRendersVisibleGlyphs)
{
    constexpr int W = 256;
    constexpr int H = 256;
    ObolTestSupport::RenderFixture fixture(W, H);
    ASSERT_TRUE(fixture.available());
    auto scene = ObolTestSupport::makeScene(
        ObolTest::Scenes::createAsciiText, fixture);
    ASSERT_TRUE(fixture.render(scene.root()));

    int lit = 0;
    const auto & pixels = fixture.pixels();
    for (int y = 0; y < H; y += 2) {
        for (int x = 0; x < W; x += 2) {
            const unsigned char * p = pixels.data() + (y * W + x) * 3;
            if (p[0] > 80 || p[1] > 80 || p[2] > 80) ++lit;
        }
    }
    EXPECT_GE(lit, 5);
}
