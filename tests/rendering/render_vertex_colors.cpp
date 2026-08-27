#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"
#include <gtest/gtest.h>

#include <cstdlib>

namespace {

constexpr int W = 256;
constexpr int H = 256;
constexpr int MARGIN = 8;

bool checkCorner(const ObolTestSupport::RenderFixture & fixture,
                 int px, int py,
                 unsigned char expected_r,
                 unsigned char expected_g,
                 unsigned char expected_b,
                 int tolerance)
{
    if (px < 0 || px >= W || py < 0 || py >= H) return false;
    const auto & pixels = fixture.pixels();
    const unsigned char * p = pixels.data() + (py * W + px) * 3;
    return std::abs(static_cast<int>(p[0]) - static_cast<int>(expected_r)) <= tolerance &&
           std::abs(static_cast<int>(p[1]) - static_cast<int>(expected_g)) <= tolerance &&
           std::abs(static_cast<int>(p[2]) - static_cast<int>(expected_b)) <= tolerance;
}

} // namespace

TEST(RenderSceneFactories, VertexColorsPreserveCornerColours)
{
    ObolTestSupport::RenderFixture fixture(W, H, SbColor(0.5f, 0.5f, 0.5f));
    ASSERT_TRUE(fixture.available());
    auto scene = ObolTestSupport::makeScene(
        ObolTest::Scenes::createVertexColors, fixture);
    ASSERT_TRUE(fixture.render(scene.root()));

    constexpr int tolerance = 80;
    EXPECT_TRUE(checkCorner(fixture, MARGIN, MARGIN, 200, 0, 0, tolerance));
    EXPECT_TRUE(checkCorner(fixture, W - MARGIN - 1, MARGIN,
                           0, 200, 0, tolerance));
    EXPECT_TRUE(checkCorner(fixture, W - MARGIN - 1, H - MARGIN - 1,
                           0, 0, 200, tolerance));
    EXPECT_TRUE(checkCorner(fixture, MARGIN, H - MARGIN - 1,
                           200, 200, 0, tolerance));
}
