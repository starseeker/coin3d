#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"
#include <gtest/gtest.h>

namespace {

constexpr int W = 256;
constexpr int H = 256;

struct TriangleStripMetrics {
    int blue = 0;
    int black = 0;
};

TriangleStripMetrics measureTriangleStrip(
    const ObolTestSupport::RenderFixture & fixture)
{
    TriangleStripMetrics metrics;
    const auto & pixels = fixture.pixels();
    for (int y = 0; y < H / 2; y += 4) {
        for (int x = W / 4; x < 3 * W / 4; x += 4) {
            const unsigned char * p = pixels.data() + (y * W + x) * 3;
            if (p[2] > 150 && p[0] < 60 && p[1] < 60) ++metrics.blue;
        }
    }
    for (int y = H / 2; y < H; y += 4) {
        for (int x = W / 4; x < 3 * W / 4; x += 4) {
            const unsigned char * p = pixels.data() + (y * W + x) * 3;
            if (p[0] < 20 && p[1] < 20 && p[2] < 20) ++metrics.black;
        }
    }
    return metrics;
}

} // namespace

TEST(RenderSceneFactories, TriangleStripSetPreservesVisibleRegions)
{
    ObolTestSupport::RenderFixture fixture(W, H);
    ASSERT_TRUE(fixture.available());
    auto scene = ObolTestSupport::makeScene(
        ObolTest::Scenes::createTriangleStripSet, fixture);
    ASSERT_TRUE(fixture.render(scene.root()));

    const TriangleStripMetrics metrics = measureTriangleStrip(fixture);
    EXPECT_GE(metrics.blue, 5);
    EXPECT_GE(metrics.black, 5);
}
