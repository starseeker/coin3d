#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"
#include <gtest/gtest.h>

namespace {

int countVisibleCenter(const ObolTestSupport::RenderFixture & fixture)
{
    constexpr int W = 256;
    constexpr int H = 256;
    constexpr int radius = W / 4;
    int visible = 0;
    const auto & pixels = fixture.pixels();
    for (int y = H / 2 - radius; y <= H / 2 + radius; y += 2) {
        for (int x = W / 2 - radius; x <= W / 2 + radius; x += 2) {
            if (x < 0 || x >= W || y < 0 || y >= H) continue;
            const unsigned char * p = pixels.data() + (y * W + x) * 3;
            if (p[0] > 20 || p[1] > 20 || p[2] > 20) ++visible;
        }
    }
    return visible;
}

} // namespace

TEST(RenderSceneFactories, TransparencyScenePreservesVisiblePixels)
{
    ObolTestSupport::RenderFixture fixture(256, 256);
    ASSERT_TRUE(fixture.available());
    auto scene = ObolTestSupport::makeScene(
        ObolTest::Scenes::createTransparency, fixture);
    ASSERT_TRUE(fixture.render(scene.root()));
    EXPECT_GE(countVisibleCenter(fixture), 5);
}
