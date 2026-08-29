#include "framework/image_assertions.h"
#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"
#include <gtest/gtest.h>

#include <Inventor/nodes/SoTexture3.h>

#include <array>
#include <cstring>

TEST(RenderSceneFactories, ThreeDimensionalTextureSceneRendersVisibleGeometry)
{
    ObolTestSupport::RenderFixture fixture(256, 256);
    ASSERT_TRUE(fixture.available());
    auto scene = ObolTestSupport::makeScene(
        ObolTest::Scenes::createTexture3, fixture);
    ASSERT_TRUE(fixture.render(scene.root()));
    EXPECT_GT(fixture.nonBackgroundPixels(15), 100u);
}

TEST(RenderFeatureContracts, ThreeDimensionalTextureRespondsToImageChanges)
{
    ObolTestSupport::RenderFixture fixture(256, 256);
    ASSERT_TRUE(fixture.available());
    if (std::strcmp(fixture.backendName(), "swrast") == 0) {
        GTEST_SKIP() << "bundled renderer does not support updating 3-D "
                        "texture objects";
    }

    auto scene = ObolTestSupport::makeScene(
        ObolTest::Scenes::createTexture3, fixture);
    ASSERT_TRUE(fixture.render(scene.root()));
    const ObolTestSupport::RgbImage checker{
        static_cast<unsigned int>(fixture.width()),
        static_cast<unsigned int>(fixture.height()), fixture.pixels()
    };
    SoTexture3 * texture =
        ObolTestSupport::findFirstNode<SoTexture3>(scene.root());
    ASSERT_NE(texture, nullptr);

    constexpr int size = 8;
    constexpr int components = 4;
    std::array<unsigned char,
               size * size * size * components> solid_red{};
    for (std::size_t i = 0; i < solid_red.size(); i += components) {
        solid_red[i] = 240;
        solid_red[i + 1] = 20;
        solid_red[i + 2] = 20;
        solid_red[i + 3] = 255;
    }
    texture->setImageData(size, size, size, components, solid_red.data());
    ASSERT_TRUE(fixture.render(scene.root()));
    const ObolTestSupport::RgbImage red{
        static_cast<unsigned int>(fixture.width()),
        static_cast<unsigned int>(fixture.height()), fixture.pixels()
    };
    const auto comparison = ObolTestSupport::compareRgb(checker, red);
    EXPECT_GT(comparison.differing_pixels, 1000u)
        << ObolTestSupport::describeComparison(comparison);
    EXPECT_GT(comparison.rms_error, 10.0)
        << ObolTestSupport::describeComparison(comparison);

    const std::size_t center =
        (static_cast<std::size_t>(fixture.height() / 2) * fixture.width() +
         static_cast<std::size_t>(fixture.width() / 2)) * 3u;
    ASSERT_LT(center + 2u, red.pixels.size());
    EXPECT_GT(static_cast<int>(red.pixels[center]),
              static_cast<int>(red.pixels[center + 2u]) + 30);
}
