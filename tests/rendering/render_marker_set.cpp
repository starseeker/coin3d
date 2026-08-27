#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"

#include <gtest/gtest.h>

#include <Inventor/SoType.h>
#include <Inventor/nodes/SoMarkerSet.h>

namespace {

constexpr int W = 256;
constexpr int H = 256;

int countVisiblePixels(const ObolTestSupport::RenderFixture & fixture)
{
    int visible = 0;
    const auto & pixels = fixture.pixels();
    for (int i = 0; i < W * H; ++i) {
        const unsigned char * p = pixels.data() + i * 3;
        if (p[0] > 30 || p[1] > 30 || p[2] > 30) ++visible;
    }
    return visible;
}

} // namespace

TEST(RenderSceneFactories, MarkerSetApiAndRenderingRemainAvailable)
{
    EXPECT_NE(SoMarkerSet::getClassTypeId(), SoType::badType());
    EXPECT_GT(SoMarkerSet::getNumDefinedMarkers(), 0);

    ObolTestSupport::RenderFixture fixture(W, H);
    ASSERT_TRUE(fixture.available());
    auto scene = ObolTestSupport::makeScene(
        ObolTest::Scenes::createMarkerSet, fixture);
    ASSERT_TRUE(fixture.render(scene.root()));

    EXPECT_GT(countVisiblePixels(fixture), 0);
}
