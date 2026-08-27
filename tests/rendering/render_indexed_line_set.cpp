#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"
#include <gtest/gtest.h>

static const int W = 256;
static const int H = 256;

struct IndexedLineMetrics {
    int green = 0;
    int red = 0;
};

static IndexedLineMetrics measureIndexedLineSet(
    const ObolTestSupport::RenderFixture & fixture)
{
    IndexedLineMetrics metrics;
    const auto & pixels = fixture.pixels();
    // Green horizontal line is at world Y ≈ +0.5.  The factory's
    // orthographic camera places it near the 75% image row.
    const int greenRow = static_cast<int>(H * (0.5f + 0.5f / 2.0f));

    for (int y = greenRow - 6; y <= greenRow + 6; ++y) {
        if (y < 0 || y >= H) continue;
        for (int x = W / 8; x < 7 * W / 8; x += 4) {
            const unsigned char *p = pixels.data() + (y * W + x) * 3;
            if (p[1] > 150 && p[0] < 80 && p[2] < 80) ++metrics.green;
        }
    }

    for (int y = H / 4; y < 3 * H / 4; y += 4) {
        for (int x = W / 4; x < 3 * W / 4; x += 4) {
            const unsigned char *p = pixels.data() + (y * W + x) * 3;
            if (p[0] > 150 && p[1] < 80 && p[2] < 80) ++metrics.red;
        }
    }
    return metrics;
}

TEST(RenderSceneFactories, IndexedLineSetPreservesPolylineColours)
{
    ObolTestSupport::RenderFixture fixture(W, H);
    ASSERT_TRUE(fixture.available());
    auto scene = ObolTestSupport::makeScene(
        ObolTest::Scenes::createIndexedLineSet, fixture);
    ASSERT_TRUE(fixture.render(scene.root()));

    const IndexedLineMetrics metrics = measureIndexedLineSet(fixture);
    EXPECT_GE(metrics.green, 3);
    EXPECT_GE(metrics.red, 3);
}
