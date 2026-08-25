#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"
#include <gtest/gtest.h>

static const int W = 400;
static const int H = 400;

struct QuadMeshMetrics {
    int non_background = 0;
    int left_red = 0;
    int right_blue = 0;
};

static QuadMeshMetrics measureQuadMesh(const ObolTestSupport::RenderFixture & fixture)
{
    QuadMeshMetrics metrics;
    const auto & pixels = fixture.pixels();
    for (int y = H / 4; y < 3 * H / 4; y += 4) {
        for (int x = W / 16; x < W / 5; x += 4) {
            const unsigned char *p = pixels.data() + (y * W + x) * 3;
            if (p[0] < 10 && p[1] < 10 && p[2] < 10) continue;
            ++metrics.non_background;
            if (p[0] > p[2] + 30) ++metrics.left_red;
        }
        for (int x = 4 * W / 5; x < 15 * W / 16; x += 4) {
            const unsigned char *p = pixels.data() + (y * W + x) * 3;
            if (p[0] < 10 && p[1] < 10 && p[2] < 10) continue;
            ++metrics.non_background;
            if (p[2] > p[0] + 30) ++metrics.right_blue;
        }
    }
    return metrics;
}

TEST(RenderSceneFactories, QuadMeshPreservesColourGradient)
{
    ObolTestSupport::RenderFixture fixture(W, H);
    ASSERT_TRUE(fixture.available());
    auto scene = ObolTestSupport::makeScene(ObolTest::Scenes::createQuadMesh,
                                            fixture);
    ASSERT_TRUE(fixture.render(scene.root()));

    const QuadMeshMetrics metrics = measureQuadMesh(fixture);
    EXPECT_GE(metrics.non_background, 20);
    EXPECT_GE(metrics.left_red, 3);
    EXPECT_GE(metrics.right_blue, 3);
}
