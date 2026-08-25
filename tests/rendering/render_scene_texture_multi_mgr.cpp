#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"

#include <gtest/gtest.h>

#include <Inventor/SoDB.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/SbViewportRegion.h>

TEST(RenderSceneFactories, SceneTextureSurvivesDifferentContextManagers)
{
    constexpr int W = 128;
    constexpr int H = 128;
    ObolTestSupport::RenderFixture fixture(W, H);
    ASSERT_TRUE(fixture.available());
    auto scene = ObolTestSupport::makeScene(
        ObolTest::Scenes::createSceneTextureMultiMgr, fixture);

    auto * manager_a = SoDB::createOSMesaContextManager();
    auto * manager_b = SoDB::createOSMesaContextManager();
    if (!manager_a || !manager_b || manager_a == manager_b) {
        delete manager_a;
        delete manager_b;
        GTEST_SKIP() << "OSMesa context managers are unavailable";
    }

    {
        const SbViewportRegion viewport(W, H);
        SoOffscreenRenderer renderer_a(manager_a, viewport);
        renderer_a.setComponents(SoOffscreenRenderer::RGB);
        renderer_a.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
        ASSERT_TRUE(renderer_a.render(scene.root()));
        ASSERT_NE(renderer_a.getBuffer(), nullptr);

        // Render the same scene through a different manager.  This is the
        // regression path for stale inner SoSceneTexture2 context ownership.
        SoOffscreenRenderer renderer_b(manager_b, viewport);
        renderer_b.setComponents(SoOffscreenRenderer::RGB);
        renderer_b.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
        ASSERT_TRUE(renderer_b.render(scene.root()));
        ASSERT_NE(renderer_b.getBuffer(), nullptr);
    }

    delete manager_a;
    delete manager_b;
}
