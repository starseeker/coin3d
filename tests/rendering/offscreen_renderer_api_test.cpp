#include "framework/render_fixture.h"

#include <gtest/gtest.h>

#include <Inventor/SbColor.h>
#include <Inventor/SbColor4f.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/actions/SoAction.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/SoRenderManager.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSphere.h>

namespace {

void countPreRender(void * user_data, SoGLRenderAction *)
{
    ++*static_cast<int *>(user_data);
}

SoGLRenderAction::AbortCode continueRendering(void * user_data)
{
    ++*static_cast<int *>(user_data);
    return SoGLRenderAction::CONTINUE;
}

} // namespace

TEST(OffscreenRenderer, ConfigurationApiRetainsViewportComponentsAndBackground)
{
    SoOffscreenRenderer renderer(SbViewportRegion(64, 64));

    EXPECT_EQ(renderer.getViewportRegion().getWindowSize()[0], 64);
    EXPECT_EQ(renderer.getViewportRegion().getWindowSize()[1], 64);

    renderer.setComponents(SoOffscreenRenderer::RGB);
    EXPECT_EQ(renderer.getComponents(), SoOffscreenRenderer::RGB);

    renderer.setComponents(SoOffscreenRenderer::RGB_TRANSPARENCY);
    EXPECT_EQ(renderer.getComponents(), SoOffscreenRenderer::RGB_TRANSPARENCY);

    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 1.0f));
    const SbColor background = renderer.getBackgroundColor();
    EXPECT_FLOAT_EQ(background[0], 0.0f);
    EXPECT_FLOAT_EQ(background[1], 0.0f);
    EXPECT_FLOAT_EQ(background[2], 1.0f);

    const SbVec2s maximum = SoOffscreenRenderer::getMaximumResolution();
    EXPECT_GT(maximum[0], 0);
    EXPECT_GT(maximum[1], 0);

    renderer.setViewportRegion(SbViewportRegion(400, 300));
    EXPECT_EQ(renderer.getViewportRegion().getWindowSize()[0], 400);
    EXPECT_EQ(renderer.getViewportRegion().getWindowSize()[1], 300);
}

TEST(RenderManager, ConfigurationApiRetainsSceneViewportAndBackground)
{
    SoRenderManager manager;
    auto * root = new SoSeparator;
    root->ref();
    root->addChild(new SoCube);

    manager.setSceneGraph(root);
    EXPECT_EQ(manager.getSceneGraph(), root);

    manager.setViewportRegion(SbViewportRegion(640, 480));
    EXPECT_EQ(manager.getViewportRegion().getWindowSize()[0], 640);
    EXPECT_EQ(manager.getViewportRegion().getWindowSize()[1], 480);

    manager.setBackgroundColor(SbColor4f(0.5f, 0.5f, 0.5f, 1.0f));
    const SbColor4f background = manager.getBackgroundColor();
    EXPECT_FLOAT_EQ(background[0], 0.5f);
    EXPECT_FLOAT_EQ(background[3], 1.0f);

    manager.setSceneGraph(nullptr);
    root->unref();
}

TEST(SoGLRenderAction, ConfigurationApiRetainsTraversalSettings)
{
    SoGLRenderAction action(SbViewportRegion(512, 512));
    action.setViewportRegion(SbViewportRegion(800, 600));
    EXPECT_EQ(action.getViewportRegion().getWindowSize()[0], 800);
    EXPECT_EQ(action.getViewportRegion().getWindowSize()[1], 600);
    EXPECT_TRUE(action.isOfType(SoAction::getClassTypeId()));

    action.setTransparencyType(SoGLRenderAction::BLEND);
    EXPECT_EQ(action.getTransparencyType(), SoGLRenderAction::BLEND);
    action.setNumPasses(2);
    EXPECT_EQ(action.getNumPasses(), 2);
    action.setPassUpdate(TRUE);
    EXPECT_TRUE(action.isPassUpdate());
    action.setSmoothing(TRUE);
    EXPECT_TRUE(action.isSmoothing());
    action.setCacheContext(1);
    EXPECT_EQ(action.getCacheContext(), 1);
}

TEST(OSMesaRenderContracts, RenderActionSettingsAndCallbacksSurviveContext)
{
    auto * root = new SoSeparator;
    root->ref();
    auto * camera = new SoPerspectiveCamera;
    camera->position.setValue(0.0f, 0.0f, 5.0f);
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);
    root->addChild(new SoSphere);

    ObolTestSupport::RenderFixture fixture(96, 96);
    ASSERT_TRUE(fixture.available());
    SoGLRenderAction * action = fixture.renderAction();
    ASSERT_NE(action, nullptr);

    int pre_render_count = 0;
    int abort_count = 0;
    action->setNumPasses(2);
    action->setPassUpdate(TRUE);
    action->setSmoothing(TRUE);
    EXPECT_TRUE(action->isSmoothing());
    action->addPreRenderCallback(countPreRender, &pre_render_count);
    action->setAbortCallback(continueRendering, &abort_count);
    action->setUpdateArea(SbVec2f(0.0f, 0.0f), SbVec2f(1.0f, 1.0f));
    action->setTransparencyType(SoGLRenderAction::SORTED_OBJECT_BLEND);
    action->setTransparentDelayedObjectRenderType(SoGLRenderAction::ONE_PASS);
    action->setDelayedObjDepthWrite(TRUE);
    action->setRenderingIsRemote(TRUE);

    SbVec2f update_origin;
    SbVec2f update_size;
    action->getUpdateArea(update_origin, update_size);
    EXPECT_FLOAT_EQ(update_size[0], 1.0f);
    EXPECT_FLOAT_EQ(update_size[1], 1.0f);
    EXPECT_EQ(action->getTransparentDelayedObjectRenderType(),
              SoGLRenderAction::ONE_PASS);
    EXPECT_TRUE(action->getDelayedObjDepthWrite());
    EXPECT_TRUE(action->getRenderingIsRemote());

    ASSERT_TRUE(fixture.render(root));
    EXPECT_GT(pre_render_count, 0);
    EXPECT_GT(abort_count, 0);

    action->removePreRenderCallback(countPreRender, &pre_render_count);
    action->setAbortCallback(nullptr, nullptr);
    root->unref();
}
