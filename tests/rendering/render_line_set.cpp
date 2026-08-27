#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"
#include <gtest/gtest.h>

namespace {

constexpr int W = 256;
constexpr int H = 256;

struct LineSetMetrics {
    int red = 0;
    int black_above = 0;
    int black_below = 0;
};

LineSetMetrics measureLineSet(const ObolTestSupport::RenderFixture & fixture)
{
    LineSetMetrics metrics;
    const auto & pixels = fixture.pixels();
    for (int x = W / 8; x < 7 * W / 8; x += 4) {
        for (int y = H / 2 - 4; y <= H / 2 + 4; ++y) {
            const unsigned char * p = pixels.data() + (y * W + x) * 3;
            if (p[0] > 180 && p[1] < 50 && p[2] < 50) ++metrics.red;
        }

        const unsigned char * above =
            pixels.data() + ((3 * H / 4) * W + x) * 3;
        if (above[0] < 20 && above[1] < 20 && above[2] < 20) {
            ++metrics.black_above;
        }

        const unsigned char * below =
            pixels.data() + ((H / 4) * W + x) * 3;
        if (below[0] < 20 && below[1] < 20 && below[2] < 20) {
            ++metrics.black_below;
        }
    }
    return metrics;
}

} // namespace

TEST(RenderSceneFactories, LineSetPreservesLineAndBackground)
{
    ObolTestSupport::RenderFixture fixture(W, H);
    ASSERT_TRUE(fixture.available());
    auto scene = ObolTestSupport::makeScene(ObolTest::Scenes::createLineSet,
                                            fixture);
    ASSERT_TRUE(fixture.render(scene.root()));

    const LineSetMetrics metrics = measureLineSet(fixture);
    EXPECT_GE(metrics.red, 5);
    EXPECT_GE(metrics.black_above, 5);
    EXPECT_GE(metrics.black_below, 5);
}
