#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"
#include <gtest/gtest.h>

#include <cstddef>

static const int W = 512;
static const int H = 512;

struct IndexedFaceMetrics {
    int non_background = 0;
};

static IndexedFaceMetrics measureScene(const ObolTestSupport::RenderFixture & fixture)
{
    IndexedFaceMetrics metrics;
    const auto & pixels = fixture.pixels();
    for (int y = 0; y < H; y += 4) {
        for (int x = 0; x < W; x += 4) {
            const unsigned char *p = pixels.data() + (y * W + x) * 3;
            if (p[0] < 10 && p[1] < 10 && p[2] < 10) continue;
            ++metrics.non_background;
        }
    }
    return metrics;
}

TEST(RenderSceneFactories, IndexedFaceSetRendersVisibleGeometry)
{
    ObolTestSupport::RenderFixture fixture(W, H);
    ASSERT_TRUE(fixture.available());
    auto scene = ObolTestSupport::makeScene(
        ObolTest::Scenes::createIndexedFaceSet, fixture);
    ASSERT_TRUE(fixture.render(scene.root()));

    const IndexedFaceMetrics metrics = measureScene(fixture);
    EXPECT_GE(metrics.non_background, 100);
}
