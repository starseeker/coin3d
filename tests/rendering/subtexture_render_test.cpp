#include "framework/render_fixture.h"

#include <gtest/gtest.h>

#include <Inventor/nodes/SoComplexity.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoFaceSet.h>
#include <Inventor/nodes/SoLightModel.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoShapeHints.h>
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/nodes/SoTextureCoordinate2.h>

#include <array>
#include <cstddef>

namespace {

std::array<unsigned char, 3>
pixelAt(const ObolTestSupport::RenderFixture & fixture, const int x, const int y)
{
    const std::size_t offset =
        (static_cast<std::size_t>(y) * fixture.width() + x) * 3;
    const auto & pixels = fixture.pixels();
    return {pixels[offset], pixels[offset + 1], pixels[offset + 2]};
}

} // namespace

TEST(SubtextureRendering, UpdatesOnlyTheRequestedTextureRegion)
{
    SoSeparator * root = new SoSeparator;
    root->ref();

    auto * camera = new SoOrthographicCamera;
    camera->position.setValue(0.0f, 0.0f, 2.0f);
    camera->height.setValue(2.4f);
    root->addChild(camera);

    auto * light_model = new SoLightModel;
    light_model->model.setValue(SoLightModel::BASE_COLOR);
    root->addChild(light_model);

    auto * complexity = new SoComplexity;
    complexity->textureQuality.setValue(0.1f); // no mipmap: exercise TexSubImage2D
    root->addChild(complexity);

    auto * texture = new SoTexture2;
    texture->model.setValue(SoTexture2::REPLACE);
    unsigned char red_image[4 * 4 * 3];
    for (std::size_t i = 0; i < sizeof(red_image); i += 3) {
        red_image[i] = 255;
        red_image[i + 1] = 0;
        red_image[i + 2] = 0;
    }
    texture->image.setValue(SbVec2s(4, 4), 3, red_image);
    root->addChild(texture);

    auto * coordinates = new SoCoordinate3;
    coordinates->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
    coordinates->point.set1Value(1, SbVec3f( 1.0f, -1.0f, 0.0f));
    coordinates->point.set1Value(2, SbVec3f( 1.0f,  1.0f, 0.0f));
    coordinates->point.set1Value(3, SbVec3f(-1.0f,  1.0f, 0.0f));
    root->addChild(coordinates);

    auto * texture_coordinates = new SoTextureCoordinate2;
    texture_coordinates->point.set1Value(0, SbVec2f(0.0f, 0.0f));
    texture_coordinates->point.set1Value(1, SbVec2f(1.0f, 0.0f));
    texture_coordinates->point.set1Value(2, SbVec2f(1.0f, 1.0f));
    texture_coordinates->point.set1Value(3, SbVec2f(0.0f, 1.0f));
    root->addChild(texture_coordinates);

    auto * hints = new SoShapeHints;
    hints->vertexOrdering.setValue(SoShapeHints::COUNTERCLOCKWISE);
    root->addChild(hints);
    auto * face = new SoFaceSet;
    face->numVertices.set1Value(0, 4);
    root->addChild(face);

    ObolTestSupport::RenderFixture fixture(96, 96);
    ASSERT_TRUE(fixture.available());
    ASSERT_TRUE(fixture.render(root));
    const auto initial_center = pixelAt(fixture, 48, 48);
    EXPECT_GT(initial_center[0], 220);
    EXPECT_LT(initial_center[1], 30);

    unsigned char green_subimage[2 * 2 * 3];
    for (std::size_t i = 0; i < sizeof(green_subimage); i += 3) {
        green_subimage[i] = 0;
        green_subimage[i + 1] = 255;
        green_subimage[i + 2] = 0;
    }
    texture->image.setSubValue(SbVec2s(2, 2), SbVec2s(1, 1),
                               green_subimage);

    ASSERT_TRUE(fixture.render(root));
    const auto updated_center = pixelAt(fixture, 48, 48);
    EXPECT_LT(updated_center[0], 30);
    EXPECT_GT(updated_center[1], 220);

    const auto unchanged_corner = pixelAt(fixture, 20, 20);
    EXPECT_GT(unchanged_corner[0], 220);
    EXPECT_LT(unchanged_corner[1], 30);

    root->unref();
}
