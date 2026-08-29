#include "framework/image_assertions.h"
#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"
#include <gtest/gtest.h>

#include <Inventor/nodes/SoBumpMap.h>

#include <array>
#include <cstring>

TEST(RenderSceneFactories, BumpMapSceneRendersVisibleGeometry)
{
    ObolTestSupport::RenderFixture fixture(256, 256);
    ASSERT_TRUE(fixture.available());
    auto scene = ObolTestSupport::makeScene(ObolTest::Scenes::createBumpMap,
                                            fixture);
    ASSERT_TRUE(fixture.render(scene.root()));
    EXPECT_GT(fixture.nonBackgroundPixels(15), 100u);
}

TEST(RenderFeatureContracts, BumpMapChangesSurfaceLighting)
{
    ObolTestSupport::RenderFixture fixture(256, 256);
    ASSERT_TRUE(fixture.available());
    if (std::strcmp(fixture.backendName(), "swrast") == 0) {
        GTEST_SKIP() << "bundled renderer does not advertise legacy bump mapping";
    }

    auto scene = ObolTestSupport::makeScene(ObolTest::Scenes::createBumpMap,
                                            fixture);
    ASSERT_TRUE(fixture.render(scene.root()));
    const ObolTestSupport::RgbImage bumped{
        static_cast<unsigned int>(fixture.width()),
        static_cast<unsigned int>(fixture.height()), fixture.pixels()
    };
    SoBumpMap * bump =
        ObolTestSupport::findFirstNode<SoBumpMap>(scene.root());
    ASSERT_NE(bump, nullptr);

    constexpr int size = 32;
    constexpr int components = 4;
    std::array<unsigned char, size * size * components> flat_normal{};
    for (std::size_t i = 0; i < flat_normal.size(); i += components) {
        flat_normal[i] = 128;
        flat_normal[i + 1] = 128;
        flat_normal[i + 2] = 255;
        flat_normal[i + 3] = 255;
    }
    bump->image.setValue(SbVec2s(size, size), components,
                         flat_normal.data());
    ASSERT_TRUE(fixture.render(scene.root()));
    const ObolTestSupport::RgbImage flat{
        static_cast<unsigned int>(fixture.width()),
        static_cast<unsigned int>(fixture.height()), fixture.pixels()
    };
    const auto comparison = ObolTestSupport::compareRgb(bumped, flat);
    EXPECT_GT(comparison.differing_pixels, 1000u)
        << ObolTestSupport::describeComparison(comparison);
    EXPECT_GT(comparison.rms_error, 2.0)
        << ObolTestSupport::describeComparison(comparison);
}
