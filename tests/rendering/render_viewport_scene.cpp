#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"

#include <gtest/gtest.h>

#include <Inventor/SoViewport.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSphere.h>

TEST(RenderSceneFactories, ViewportRendersGreenSceneThroughOwnedRenderer)
{
    constexpr int W = 256;
    constexpr int H = 256;
    ObolTestSupport::RenderFixture fixture(W, H);
    ASSERT_TRUE(fixture.available());

    auto factory_scene = ObolTestSupport::makeScene(
        ObolTest::Scenes::createViewportScene, fixture);
    ASSERT_TRUE(fixture.render(factory_scene.root()));
    EXPECT_GT(fixture.nonBackgroundPixels(5), 20u);

    auto * geometry = new SoSeparator;
    geometry->ref();
    auto * light = new SoDirectionalLight;
    light->direction.setValue(-1.0f, -1.0f, -1.0f);
    geometry->addChild(light);
    auto * material = new SoMaterial;
    material->diffuseColor.setValue(0.1f, 0.8f, 0.2f);
    geometry->addChild(material);
    geometry->addChild(new SoSphere);

    SoViewport viewport;
    viewport.setWindowSize(SbVec2s(W, H));
    viewport.setSceneGraph(geometry);
    viewport.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
    auto * camera = new SoPerspectiveCamera;
    camera->position.setValue(0.0f, 0.0f, 5.0f);
    camera->pointAt(SbVec3f(0.0f, 0.0f, 0.0f));
    viewport.setCamera(camera);
    viewport.viewAll();

    ASSERT_NE(fixture.renderer(), nullptr);
    ASSERT_TRUE(viewport.render(fixture.renderer()));
    ASSERT_TRUE(fixture.capture());

    const auto & pixels = fixture.pixels();
    int visible = 0;
    long red = 0;
    long green = 0;
    long blue = 0;
    for (int i = 0; i < W * H; ++i) {
        const unsigned char * p = pixels.data() + i * 3;
        if (p[0] > 5 || p[1] > 5 || p[2] > 5) ++visible;
        red += p[0];
        green += p[1];
        blue += p[2];
    }
    EXPECT_GE(visible, 200);
    EXPECT_GT(green, red);
    EXPECT_GT(green, blue);

    geometry->unref();
}
