#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"
#include <gtest/gtest.h>

namespace {

constexpr int W = 512;
constexpr int H = 256;

struct ResetTransformMetrics {
    int red_center = 0;
    int blue_right = 0;
};

ResetTransformMetrics measureResetTransform(
    const ObolTestSupport::RenderFixture & fixture)
{
    ResetTransformMetrics metrics;
    const auto & pixels = fixture.pixels();

    for (int y = H / 4; y < 3 * H / 4; y += 2) {
        for (int x = W / 2 - 60; x < W / 2 + 60; x += 2) {
            if (x < 0 || x >= W) continue;
            const unsigned char * p = pixels.data() + (y * W + x) * 3;
            if (p[0] > 150 && p[1] < 80 && p[2] < 80) ++metrics.red_center;
        }
    }

    for (int y = H / 4; y < 3 * H / 4; y += 2) {
        for (int x = W / 2 + 40; x < W - 40; x += 2) {
            if (x < 0 || x >= W) continue;
            const unsigned char * p = pixels.data() + (y * W + x) * 3;
            if (p[2] > 100 && p[0] < 80) ++metrics.blue_right;
        }
    }
    return metrics;
}

} // namespace

TEST(RenderSceneFactories, ResetTransformPreservesIndependentObjectPositions)
{
    ObolTestSupport::RenderFixture fixture(W, H);
    ASSERT_TRUE(fixture.available());
    auto scene = ObolTestSupport::makeScene(
        ObolTest::Scenes::createResetTransform, fixture);
    ASSERT_TRUE(fixture.render(scene.root()));

    const ResetTransformMetrics metrics = measureResetTransform(fixture);
    EXPECT_GE(metrics.red_center, 3);
    EXPECT_GE(metrics.blue_right, 3);
}
