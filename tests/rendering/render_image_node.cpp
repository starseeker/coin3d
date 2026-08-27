#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"
#include <gtest/gtest.h>

namespace {

constexpr int W = 256;
constexpr int H = 256;

struct ImageMetrics {
    int red = 0;
    int green = 0;
};

ImageMetrics measureImage(const ObolTestSupport::RenderFixture & fixture)
{
    ImageMetrics metrics;
    const auto & pixels = fixture.pixels();
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const unsigned char * p = pixels.data() + (y * W + x) * 3;
            if (p[0] > 150 && p[1] < 50 && p[2] < 50) ++metrics.red;
            if (p[1] > 150 && p[0] < 50 && p[2] < 50) ++metrics.green;
        }
    }
    return metrics;
}

} // namespace

TEST(RenderSceneFactories, ImageNodePreservesImageColours)
{
    ObolTestSupport::RenderFixture fixture(W, H, SbColor(0.2f, 0.2f, 0.2f));
    ASSERT_TRUE(fixture.available());
    auto scene = ObolTestSupport::makeScene(
        ObolTest::Scenes::createImageNode, fixture);
    ASSERT_TRUE(fixture.render(scene.root()));

    const ImageMetrics metrics = measureImage(fixture);
    EXPECT_GE(metrics.red, 4);
    EXPECT_GE(metrics.green, 4);
}
