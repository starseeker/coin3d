#include "framework/image_assertions.h"
#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"
#include <gtest/gtest.h>

#include <Inventor/annex/FXViz/nodes/SoShadowGroup.h>

#include <cstring>

TEST(RenderSceneFactories, ShadowSceneRendersVisibleGeometry)
{
    ObolTestSupport::RenderFixture fixture(256, 256);
    ASSERT_TRUE(fixture.available());
    auto scene = ObolTestSupport::makeScene(ObolTest::Scenes::createShadow,
                                            fixture);
    ASSERT_TRUE(fixture.render(scene.root()));
    EXPECT_GT(fixture.nonBackgroundPixels(5), 100u);
}

TEST(RenderFeatureContracts, ShadowSceneChangesWhenShadowsAreDisabled)
{
    ObolTestSupport::RenderFixture fixture(256, 256);
    ASSERT_TRUE(fixture.available());
    if (std::strcmp(fixture.backendName(), "swrast") == 0) {
        GTEST_SKIP() << "bundled renderer uses the unshadowed fallback because "
                        "it cannot compile the legacy variance-shadow-map shader";
    }

    auto scene = ObolTestSupport::makeScene(ObolTest::Scenes::createShadow,
                                            fixture);
    ASSERT_TRUE(fixture.render(scene.root()));
    const ObolTestSupport::RgbImage shadowed{
        static_cast<unsigned int>(fixture.width()),
        static_cast<unsigned int>(fixture.height()), fixture.pixels()
    };
    SoShadowGroup * group =
        ObolTestSupport::findFirstNode<SoShadowGroup>(scene.root());
    ASSERT_NE(group, nullptr);
    ASSERT_TRUE(group->isActive.getValue());

    group->isActive.setValue(FALSE);
    ASSERT_TRUE(fixture.render(scene.root()));
    const ObolTestSupport::RgbImage unshadowed{
        static_cast<unsigned int>(fixture.width()),
        static_cast<unsigned int>(fixture.height()), fixture.pixels()
    };
    const auto comparison =
        ObolTestSupport::compareRgb(shadowed, unshadowed);
    EXPECT_GT(comparison.differing_pixels, 500u)
        << ObolTestSupport::describeComparison(comparison);
    EXPECT_GT(comparison.rms_error, 2.0)
        << ObolTestSupport::describeComparison(comparison);
}
