#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"

#include <gtest/gtest.h>

#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoShapeHints.h>
#include <Inventor/nodes/SoSphere.h>

TEST(RenderSceneFactories, ShapeHintsPreservesThreeFrameGeometry)
{
    constexpr int W = 256;
    constexpr int H = 256;
    ObolTestSupport::RenderFixture fixture(W, H);
    ASSERT_TRUE(fixture.available());

    auto factory_scene = ObolTestSupport::makeScene(
        ObolTest::Scenes::createShapeHints, fixture);
    ASSERT_TRUE(fixture.render(factory_scene.root()));
    EXPECT_GT(fixture.nonBackgroundPixels(15), 20u);

    auto * root = new SoSeparator;
    root->ref();
    auto * camera = new SoPerspectiveCamera;
    camera->position.setValue(0.0f, 0.0f, 4.0f);
    camera->nearDistance = 0.1f;
    camera->farDistance = 50.0f;
    root->addChild(camera);
    auto * light = new SoDirectionalLight;
    light->direction.setValue(-0.3f, -0.7f, -0.6f);
    root->addChild(light);

    auto * hints = new SoShapeHints;
    root->addChild(hints);
    auto * material = new SoMaterial;
    material->diffuseColor.setValue(0.6f, 0.3f, 0.9f);
    material->specularColor.setValue(0.5f, 0.5f, 0.5f);
    material->shininess = 0.4f;
    root->addChild(material);
    auto * sphere = new SoSphere;
    root->addChild(sphere);

    // SOLID + counterclockwise: backface culling enabled.
    hints->vertexOrdering = SoShapeHints::COUNTERCLOCKWISE;
    hints->shapeType = SoShapeHints::SOLID;
    hints->faceType = SoShapeHints::CONVEX;
    hints->creaseAngle = 0.5f;
    ASSERT_TRUE(fixture.render(root));
    const std::size_t solid_pixels = fixture.nonBackgroundPixels(15);
    EXPECT_GE(solid_pixels, 100u);

    // UNKNOWN_SHAPE_TYPE: culling disabled, so the sphere remains visible.
    hints->shapeType = SoShapeHints::UNKNOWN_SHAPE_TYPE;
    ASSERT_TRUE(fixture.render(root));
    const std::size_t unknown_pixels = fixture.nonBackgroundPixels(15);
    EXPECT_GE(unknown_pixels, 100u);
    EXPECT_GE(unknown_pixels, solid_pixels);

    // Sharp-edged cube with zero crease angle.
    root->removeChild(sphere);
    root->addChild(new SoCube);
    material->diffuseColor.setValue(0.9f, 0.5f, 0.2f);
    hints->shapeType = SoShapeHints::SOLID;
    hints->faceType = SoShapeHints::CONVEX;
    hints->creaseAngle = 0.0f;
    ASSERT_TRUE(fixture.render(root));
    EXPECT_GE(fixture.nonBackgroundPixels(15), 100u);

    root->unref();
}
