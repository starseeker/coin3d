#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"
#include <gtest/gtest.h>

#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/nodes/SoIndexedPointSet.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoSeparator.h>

#include <array>

static const int W = 256;
static const int H = 256;

struct PointSetMetrics {
    int red = 0;
    int green = 0;
    int blue = 0;
    int bright = 0;
};

static PointSetMetrics measurePointSet(const ObolTestSupport::RenderFixture & fixture)
{
    PointSetMetrics metrics;
    const auto & pixels = fixture.pixels();
    for (int i = 0; i < W * H; ++i) {
        const unsigned char *p = pixels.data() + i * 3;
        if (p[0] > 150 && p[1] < 80 && p[2] < 80) ++metrics.red;
        if (p[1] > 150 && p[0] < 80 && p[2] < 80) ++metrics.green;
        if (p[2] > 150 && p[0] < 80 && p[1] < 80) ++metrics.blue;
        if (p[0] > 200 && p[1] > 200 && p[2] > 200) ++metrics.bright;
    }
    return metrics;
}

TEST(RenderSceneFactories, PointSetPreservesColourFamilies)
{
    ObolTestSupport::RenderFixture fixture(W, H);
    ASSERT_TRUE(fixture.available());
    auto scene = ObolTestSupport::makeScene(ObolTest::Scenes::createPointSet,
                                            fixture);
    ASSERT_TRUE(fixture.render(scene.root()));

    const PointSetMetrics metrics = measurePointSet(fixture);
    const int families = (metrics.red > 0) + (metrics.green > 0) +
                         (metrics.blue > 0) + (metrics.bright > 0);
    EXPECT_GE(families, 3);
}

TEST(RenderPointSet, VertexArraysIgnoreNegativeIndices)
{
    ObolTestSupport::RenderFixture fixture(W, H);
    ASSERT_TRUE(fixture.available());

    auto * root = new SoSeparator;
    root->ref();
    auto * camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 5.0f);
    camera->height = 2.0f;
    root->addChild(camera);

    auto * style = new SoDrawStyle;
    style->pointSize = 7.0f;
    root->addChild(style);
    auto * color = new SoBaseColor;
    color->rgb.setValue(1.0f, 0.2f, 0.1f);
    root->addChild(color);

    std::array<SbVec3f, 20> points;
    for (size_t i = 0; i < points.size(); ++i) {
        const float x = -0.8f + 0.4f * static_cast<float>(i % 5);
        const float y = -0.6f + 0.4f * static_cast<float>(i / 5);
        points[i].setValue(x, y, 0.0f);
    }
    auto * coordinates = new SoCoordinate3;
    coordinates->point.setValues(0, static_cast<int>(points.size()),
                                 points.data());
    root->addChild(coordinates);

    const std::array<int32_t, 23> indices = {
        0, 1, 2, 3, 4, -1,
        5, 6, 7, 8, 9, -1,
        10, 11, 12, 13, 14, -1,
        15, 16, 17, 18, 19
    };
    auto * point_set = new SoIndexedPointSet;
    point_set->coordIndex.setValues(0, static_cast<int>(indices.size()),
                                    indices.data());
    root->addChild(point_set);

    ASSERT_TRUE(fixture.render(root));
    EXPECT_GT(fixture.nonBackgroundPixels(), 200u);
    root->unref();
}
