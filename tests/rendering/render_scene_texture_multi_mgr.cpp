#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"

#include <gtest/gtest.h>

#include <Inventor/SoDB.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/SbViewportRegion.h>

#include <iostream>
#include <memory>

TEST(RenderSceneFactories, SceneTextureSurvivesDifferentContextManagers)
{
    constexpr int W = 128;
    constexpr int H = 128;

    // Context-backed scene nodes may retain resources owned by the manager
    // that rendered them.  Declare the managers first so they outlive both
    // the scene and every renderer, including on assertion/skip exits.
    std::unique_ptr<SoDB::ContextManager> manager_a(
        SoDB::createOSMesaContextManager());
    std::unique_ptr<SoDB::ContextManager> manager_b(
        SoDB::createOSMesaContextManager());
    if (!manager_a || !manager_b || manager_a.get() == manager_b.get()) {
        // Defensively avoid double deletion if a broken implementation ever
        // returns the same owned instance from two factory calls.
        if (manager_a.get() == manager_b.get()) manager_b.release();
        GTEST_SKIP() << "two independent OSMesa context managers are unavailable";
    }

    ObolTestSupport::RenderFixture fixture(W, H);
    ASSERT_TRUE(fixture.available());
    auto scene = ObolTestSupport::makeScene(
        ObolTest::Scenes::createSceneTextureMultiMgr, fixture);

    {
        const SbViewportRegion viewport(W, H);
        std::cout << "scene-texture multi-manager: rendering with manager A"
                  << std::endl;
        SoOffscreenRenderer renderer_a(manager_a.get(), viewport);
        renderer_a.setComponents(SoOffscreenRenderer::RGB);
        renderer_a.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
        ASSERT_TRUE(renderer_a.render(scene.root()));
        ASSERT_NE(renderer_a.getBuffer(), nullptr);

        // Render the same scene through a different manager.  This is the
        // regression path for stale inner SoSceneTexture2 context ownership.
        std::cout << "scene-texture multi-manager: switching to manager B"
                  << std::endl;
        SoOffscreenRenderer renderer_b(manager_b.get(), viewport);
        renderer_b.setComponents(SoOffscreenRenderer::RGB);
        renderer_b.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
        ASSERT_TRUE(renderer_b.render(scene.root()));
        ASSERT_NE(renderer_b.getBuffer(), nullptr);
    }

    std::cout << "scene-texture multi-manager: renderers destroyed; "
                 "scene teardown is next"
              << std::endl;
}
