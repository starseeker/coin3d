#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"
#include <gtest/gtest.h>

TEST(RenderSceneFactories, ClipPlanePreservesClippedGeometryBoundary)
{
    constexpr int W = 256;
    constexpr int H = 256;
    ObolTestSupport::RenderFixture fixture(
        W, H, SbColor(0.02f, 0.02f, 0.02f));
    ASSERT_TRUE(fixture.available());
    auto scene = ObolTestSupport::makeScene(
        ObolTest::Scenes::createClipPlane, fixture);
    ASSERT_TRUE(fixture.render(scene.root()));

    int red_upper = 0;
    int dark_lower = 0;
    int total_upper = 0;
    int total_lower = 0;
    const auto & pixels = fixture.pixels();
    for (int y = H / 2 + 4; y < H - 4; y += 4) {
        for (int x = W / 4; x < 3 * W / 4; x += 4) {
            const unsigned char * p = pixels.data() + (y * W + x) * 3;
            if (p[0] > 150 && p[1] < 80 && p[2] < 80) ++red_upper;
            ++total_upper;
        }
    }
    for (int y = 4; y < H / 2 - 4; y += 4) {
        for (int x = W / 4; x < 3 * W / 4; x += 4) {
            const unsigned char * p = pixels.data() + (y * W + x) * 3;
            if (p[0] < 60 && p[1] < 60 && p[2] < 60) ++dark_lower;
            ++total_lower;
        }
    }
    ASSERT_GT(total_upper, 0);
    ASSERT_GT(total_lower, 0);
    EXPECT_GT(red_upper * 10, total_upper);
    EXPECT_GT(dark_lower * 2, total_lower);
}
