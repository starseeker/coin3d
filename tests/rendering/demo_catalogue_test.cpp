#include "framework/render_fixture.h"

#include <gtest/gtest.h>

#include "demo_scenes.h"

#include <Inventor/nodes/SoSeparator.h>

TEST(DemoCatalogue, EverySceneBuildsAndRendersVisibleContent)
{
    const auto & scenes = ObolDemo::demoScenes();
    ASSERT_GE(scenes.size(), 1u);

    for (const auto & scene : scenes) {
        ASSERT_TRUE(static_cast<bool>(scene.create)) << scene.id;
        auto * root = scene.create(160, 120);
        ASSERT_NE(root, nullptr) << scene.id;

        ObolTestSupport::RenderFixture fixture(160, 120);
        ASSERT_TRUE(fixture.available()) << scene.id;
        ASSERT_TRUE(fixture.render(root)) << scene.id;
        EXPECT_GT(fixture.nonBackgroundPixels(), 20u) << scene.id;

        root->unref();
    }
}

TEST(DemoCatalogue, SceneIdsAreUniqueAndFindable)
{
    const auto & scenes = ObolDemo::demoScenes();
    for (const auto & scene : scenes) {
        ASSERT_FALSE(scene.id.empty());
        EXPECT_EQ(ObolDemo::findDemoScene(scene.id)->id, scene.id);
    }
}
