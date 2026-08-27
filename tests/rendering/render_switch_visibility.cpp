#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"

#include <gtest/gtest.h>

#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/nodes/SoSwitch.h>

#include <cmath>

namespace {

constexpr int W = 256;
constexpr int H = 256;
constexpr float SPH_X = 0.5f;
constexpr float SPH_R = 0.3f;
constexpr float CAM_H = 2.0f;
constexpr int LEFT_PX = static_cast<int>(W * (0.5f - SPH_X / CAM_H));
constexpr int RIGHT_PX = static_cast<int>(W * (0.5f + SPH_X / CAM_H));
constexpr int TOL = static_cast<int>(SPH_R / CAM_H * W) + 8;

bool isColour(const ObolTestSupport::RenderFixture & fixture,
              int x, int y,
              unsigned char red,
              unsigned char green,
              unsigned char blue,
              int tolerance = 60)
{
    if (x < 0 || x >= W || y < 0 || y >= H) return false;
    const auto & pixels = fixture.pixels();
    const unsigned char * p = pixels.data() + (y * W + x) * 3;
    return std::abs(static_cast<int>(p[0]) - red) <= tolerance &&
           std::abs(static_cast<int>(p[1]) - green) <= tolerance &&
           std::abs(static_cast<int>(p[2]) - blue) <= tolerance;
}

bool hasSphereAt(const ObolTestSupport::RenderFixture & fixture,
                 int center_x,
                 unsigned char red,
                 unsigned char green,
                 unsigned char blue)
{
    int hits = 0;
    for (int dx = -TOL; dx <= TOL; dx += 4) {
        for (int dy = -TOL; dy <= TOL; dy += 4) {
            if (isColour(fixture, center_x + dx, H / 2 + dy,
                         red, green, blue)) {
                ++hits;
            }
        }
    }
    return hits >= 3;
}

bool isBlankAt(const ObolTestSupport::RenderFixture & fixture, int center_x)
{
    int blank = 0;
    int total = 0;
    const auto & pixels = fixture.pixels();
    for (int dx = -TOL; dx <= TOL; dx += 4) {
        const int x = center_x + dx;
        for (int dy = -TOL; dy <= TOL; dy += 4) {
            const int y = H / 2 + dy;
            if (x < 0 || x >= W || y < 0 || y >= H) continue;
            const unsigned char * p = pixels.data() + (y * W + x) * 3;
            if (p[0] < 30 && p[1] < 30 && p[2] < 30) ++blank;
            ++total;
        }
    }
    return total > 0 && blank > total * 3 / 4;
}

} // namespace

TEST(RenderSceneFactories, SwitchVisibilityMutatesAcrossRepeatedRenders)
{
    ObolTestSupport::RenderFixture fixture(W, H);
    ASSERT_TRUE(fixture.available());
    auto scene = ObolTestSupport::makeScene(
        ObolTest::Scenes::createSwitchVisibility, fixture);

    SoSearchAction search;
    search.setInterest(SoSearchAction::ALL);
    search.setType(SoSwitch::getClassTypeId(), FALSE);
    search.apply(scene.root());
    SoPathList & paths = search.getPaths();
    ASSERT_GE(paths.getLength(), 2);

    auto * red_switch = dynamic_cast<SoSwitch *>(paths[0]->getTail());
    auto * blue_switch = dynamic_cast<SoSwitch *>(paths[1]->getTail());
    ASSERT_NE(red_switch, nullptr);
    ASSERT_NE(blue_switch, nullptr);

    // Frame 1: both switches on.
    red_switch->whichChild.setValue(SO_SWITCH_ALL);
    blue_switch->whichChild.setValue(SO_SWITCH_ALL);
    ASSERT_TRUE(fixture.render(scene.root()));
    EXPECT_TRUE(hasSphereAt(fixture, LEFT_PX, 255, 0, 0));
    EXPECT_TRUE(hasSphereAt(fixture, RIGHT_PX, 0, 0, 255));

    // Frame 2: hide only the red sphere.
    red_switch->whichChild.setValue(SO_SWITCH_NONE);
    ASSERT_TRUE(fixture.render(scene.root()));
    EXPECT_TRUE(isBlankAt(fixture, LEFT_PX));
    EXPECT_TRUE(hasSphereAt(fixture, RIGHT_PX, 0, 0, 255));

    // Frame 3: restore red and hide only the blue sphere.
    red_switch->whichChild.setValue(SO_SWITCH_ALL);
    blue_switch->whichChild.setValue(SO_SWITCH_NONE);
    ASSERT_TRUE(fixture.render(scene.root()));
    EXPECT_TRUE(hasSphereAt(fixture, LEFT_PX, 255, 0, 0));
    EXPECT_TRUE(isBlankAt(fixture, RIGHT_PX));
}
