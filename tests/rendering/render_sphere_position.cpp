#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

namespace {

constexpr int IMG_W = 256;
constexpr int IMG_H = 256;
constexpr float SPH_R = 0.2f;
constexpr float SPH_X = 0.3f;
constexpr float SPH_Y = 0.2f;
constexpr float CAM_H = 2.0f;
constexpr int EXP_PX_X = static_cast<int>(IMG_W * (0.5f + SPH_X / CAM_H));
constexpr int EXP_PX_Y = static_cast<int>(IMG_H * (0.5f + SPH_Y / CAM_H));
constexpr int EXP_PX_R = static_cast<int>(SPH_R / CAM_H * IMG_H);
constexpr unsigned char SPH_CH_R = 255;
constexpr unsigned char SPH_CH_G = 100;
constexpr unsigned char SPH_CH_B = 100;

struct SphereBounds {
    int min_x = IMG_W;
    int max_x = -1;
    int min_y = IMG_H;
    int max_y = -1;
};

SphereBounds findSphere(const ObolTestSupport::RenderFixture & fixture)
{
    SphereBounds bounds;
    const auto & pixels = fixture.pixels();
    for (int y = 0; y < IMG_H; ++y) {
        for (int x = 0; x < IMG_W; ++x) {
            const unsigned char * p = pixels.data() + (y * IMG_W + x) * 3;
            if (std::abs(static_cast<int>(p[0]) - SPH_CH_R) < 30 &&
                std::abs(static_cast<int>(p[1]) - SPH_CH_G) < 30 &&
                std::abs(static_cast<int>(p[2]) - SPH_CH_B) < 30) {
                bounds.min_x = std::min(bounds.min_x, x);
                bounds.max_x = std::max(bounds.max_x, x);
                bounds.min_y = std::min(bounds.min_y, y);
                bounds.max_y = std::max(bounds.max_y, y);
            }
        }
    }
    return bounds;
}

} // namespace

TEST(RenderSceneFactories, SpherePositionMatchesAnalyticalProjection)
{
    ObolTestSupport::RenderFixture fixture(
        IMG_W, IMG_H, SbColor(50.0f / 255.0f,
                              50.0f / 255.0f,
                              50.0f / 255.0f));
    ASSERT_TRUE(fixture.available());
    auto scene = ObolTestSupport::makeScene(
        ObolTest::Scenes::createSpherePosition, fixture);
    ASSERT_TRUE(fixture.render(scene.root()));

    const SphereBounds bounds = findSphere(fixture);
    ASSERT_GE(bounds.max_x, 0);

    const int center_x = (bounds.min_x + bounds.max_x) / 2;
    const int center_y = (bounds.min_y + bounds.max_y) / 2;
    const int tolerance = EXP_PX_R + 4;
    EXPECT_LE(std::abs(center_x - EXP_PX_X), tolerance);
    EXPECT_LE(std::abs(center_y - EXP_PX_Y), tolerance);
}
