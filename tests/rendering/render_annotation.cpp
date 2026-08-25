#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"
#include <gtest/gtest.h>

TEST(RenderSceneFactories, AnnotationSceneRendersVisibleGeometry)
{
    constexpr int W = 256;
    constexpr int H = 256;
    ObolTestSupport::RenderFixture fixture(W, H);
    ASSERT_TRUE(fixture.available());
    auto scene = ObolTestSupport::makeScene(
        ObolTest::Scenes::createAnnotation, fixture);
    ASSERT_TRUE(fixture.render(scene.root()));

    int visible = 0;
    const auto & pixels = fixture.pixels();
    constexpr int radius = W / 4;
    for (int y = H / 2 - radius; y <= H / 2 + radius; y += 2) {
        for (int x = W / 2 - radius; x <= W / 2 + radius; x += 2) {
            const unsigned char * p = pixels.data() + (y * W + x) * 3;
            if (p[0] > 20 || p[1] > 20 || p[2] > 20) ++visible;
        }
    }
    EXPECT_GE(visible, 5);
}
