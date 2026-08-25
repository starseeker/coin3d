#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"
#include <gtest/gtest.h>

namespace {

constexpr int W = 256;
constexpr int H = 256;

struct FaceSetMetrics {
    int green = 0;
    int black = 0;
};

FaceSetMetrics measureFaceSet(const ObolTestSupport::RenderFixture & fixture)
{
    FaceSetMetrics metrics;
    const auto & pixels = fixture.pixels();

    for (int y = 0; y < H / 2; y += 4) {
        for (int x = 0; x < W / 2; x += 4) {
            const unsigned char * p = pixels.data() + (y * W + x) * 3;
            if (p[1] > 180 && p[0] < 50 && p[2] < 50) ++metrics.green;
        }
    }
    for (int y = H / 2; y < H; y += 4) {
        for (int x = W / 2; x < W; x += 4) {
            const unsigned char * p = pixels.data() + (y * W + x) * 3;
            if (p[0] < 20 && p[1] < 20 && p[2] < 20) ++metrics.black;
        }
    }
    return metrics;
}

} // namespace

TEST(RenderSceneFactories, FaceSetPreservesVisibleRegions)
{
    ObolTestSupport::RenderFixture fixture(W, H);
    ASSERT_TRUE(fixture.available());
    auto scene = ObolTestSupport::makeScene(ObolTest::Scenes::createFaceSet,
                                            fixture);
    ASSERT_TRUE(fixture.render(scene.root()));

    const FaceSetMetrics metrics = measureFaceSet(fixture);
    EXPECT_GE(metrics.green, 10);
    EXPECT_GE(metrics.black, 10);
}
