#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"
#include <gtest/gtest.h>

static const int W = 256;
static const int H = 256;

struct PointSetMetrics {
    int red = 0;
    int green = 0;
    int blue = 0;
    int bright = 0;
};

static PointSetMetrics measurePointSet(const ObolTestSupport::RenderFixture & fixture)
{
    PointSetMetrics metrics;
    const auto & pixels = fixture.pixels();
    for (int i = 0; i < W * H; ++i) {
        const unsigned char *p = pixels.data() + i * 3;
        if (p[0] > 150 && p[1] < 80 && p[2] < 80) ++metrics.red;
        if (p[1] > 150 && p[0] < 80 && p[2] < 80) ++metrics.green;
        if (p[2] > 150 && p[0] < 80 && p[1] < 80) ++metrics.blue;
        if (p[0] > 200 && p[1] > 200 && p[2] > 200) ++metrics.bright;
    }
    return metrics;
}

TEST(RenderSceneFactories, PointSetPreservesColourFamilies)
{
    ObolTestSupport::RenderFixture fixture(W, H);
    ASSERT_TRUE(fixture.available());
    auto scene = ObolTestSupport::makeScene(ObolTest::Scenes::createPointSet,
                                            fixture);
    ASSERT_TRUE(fixture.render(scene.root()));

    const PointSetMetrics metrics = measurePointSet(fixture);
    const int families = (metrics.red > 0) + (metrics.green > 0) +
                         (metrics.blue > 0) + (metrics.bright > 0);
    EXPECT_GE(families, 3);
}
