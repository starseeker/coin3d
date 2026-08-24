#include "framework/render_fixture.h"

#include <gtest/gtest.h>

#include <Inventor/SbColor.h>
#include <Inventor/SbVec2s.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/actions/SoRayPickAction.h>
#include <Inventor/draggers/SoHandleBoxDragger.h>
#include <Inventor/draggers/SoTabPlaneDragger.h>
#include <Inventor/engines/SoElapsedTime.h>
#include <Inventor/engines/SoInterpolateFloat.h>
#include <Inventor/nodes/SoAlphaTest.h>
#include <Inventor/nodes/SoAnnotation.h>
#include <Inventor/nodes/SoArray.h>
#include <Inventor/nodes/SoComplexity.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoDepthBuffer.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/nodes/SoFaceSet.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <Inventor/nodes/SoIndexedLineSet.h>
#include <Inventor/nodes/SoLOD.h>
#include <Inventor/nodes/SoLevelOfDetail.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoMaterialBinding.h>
#include <Inventor/nodes/SoNormal.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoPickStyle.h>
#include <Inventor/nodes/SoPointSet.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoShapeHints.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/nodes/SoTextureCoordinate2.h>

#include <array>

namespace {

class OwnedScene final {
public:
    explicit OwnedScene(SoSeparator * root) : root_(root) { root_->ref(); }
    ~OwnedScene() { root_->unref(); }
    OwnedScene(const OwnedScene &) = delete;
    OwnedScene & operator=(const OwnedScene &) = delete;
    SoSeparator * root() const { return root_; }

private:
    SoSeparator * root_;
};

std::array<unsigned char, 3> centerPixel(const ObolTestSupport::RenderFixture & fixture)
{
    const auto & pixels = fixture.pixels();
    const std::size_t offset =
        (static_cast<std::size_t>(fixture.height() / 2) * fixture.width() +
         fixture.width() / 2) * 3;
    return {pixels[offset], pixels[offset + 1], pixels[offset + 2]};
}

TEST(OSMesaFeatureContracts, IndexedShapesAndMaterialBindingsRender)
{
    auto * root = new SoSeparator;
    OwnedScene scene(root);
    auto * camera = new SoPerspectiveCamera;
    camera->position.setValue(0.0f, 0.0f, 8.0f);
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);

    auto * material = new SoMaterial;
    material->diffuseColor.set1Value(0, SbColor(0.9f, 0.1f, 0.1f));
    material->diffuseColor.set1Value(1, SbColor(0.1f, 0.8f, 0.2f));
    root->addChild(material);
    auto * binding = new SoMaterialBinding;
    binding->value.setValue(SoMaterialBinding::PER_VERTEX_INDEXED);
    root->addChild(binding);

    auto * coordinates = new SoCoordinate3;
    coordinates->point.set1Value(0, SbVec3f(-2.5f, -1.5f, 0.0f));
    coordinates->point.set1Value(1, SbVec3f(0.0f, 1.5f, 0.0f));
    coordinates->point.set1Value(2, SbVec3f(2.5f, -1.5f, 0.0f));
    coordinates->point.set1Value(3, SbVec3f(-2.5f, 1.5f, 0.0f));
    coordinates->point.set1Value(4, SbVec3f(2.5f, 1.5f, 0.0f));
    root->addChild(coordinates);

    auto * faces = new SoIndexedFaceSet;
    const int face_indices[] = {0, 1, 2, -1, 1, 4, 3, -1};
    faces->coordIndex.setValues(0, 8, face_indices);
    faces->materialIndex.setValues(0, 8, face_indices);
    root->addChild(faces);

    auto * style = new SoDrawStyle;
    style->style.setValue(SoDrawStyle::LINES);
    style->lineWidth.setValue(2.0f);
    root->addChild(style);
    auto * lines = new SoIndexedLineSet;
    const int line_indices[] = {0, 1, 2, -1};
    lines->coordIndex.setValues(0, 4, line_indices);
    root->addChild(lines);

    style->style.setValue(SoDrawStyle::POINTS);
    auto * points = new SoPointSet;
    points->numPoints.setValue(5);
    root->addChild(points);

    ObolTestSupport::RenderFixture fixture(160, 120);
    ASSERT_TRUE(fixture.available());
    ASSERT_TRUE(fixture.render(scene.root()));
    EXPECT_GT(fixture.nonBackgroundPixels(), 200u);
    EXPECT_EQ(binding->value.getValue(), SoMaterialBinding::PER_VERTEX_INDEXED);
}

TEST(OSMesaFeatureContracts, TextureUploadAndUpdateAffectRenderedPixels)
{
    auto * root = new SoSeparator;
    OwnedScene scene(root);
    auto * camera = new SoPerspectiveCamera;
    camera->position.setValue(0.0f, 0.0f, 6.0f);
    root->addChild(camera);
    auto * light = new SoDirectionalLight;
    light->direction.setValue(0.0f, 0.0f, -1.0f);
    root->addChild(light);

    auto * texture = new SoTexture2;
    const unsigned char red[] = {240, 20, 20, 240, 20, 20, 240, 20, 20, 240, 20, 20};
    texture->image.setValue(SbVec2s(2, 2), 3, red);
    root->addChild(texture);
    auto * material = new SoMaterial;
    material->diffuseColor.setValue(1.0f, 1.0f, 1.0f);
    root->addChild(material);
    auto * coordinates = new SoTextureCoordinate2;
    coordinates->point.set1Value(0, SbVec2f(0.0f, 0.0f));
    coordinates->point.set1Value(1, SbVec2f(1.0f, 0.0f));
    coordinates->point.set1Value(2, SbVec2f(1.0f, 1.0f));
    coordinates->point.set1Value(3, SbVec2f(0.0f, 1.0f));
    root->addChild(coordinates);
    auto * vertices = new SoCoordinate3;
    vertices->point.set1Value(0, SbVec3f(-2.0f, -2.0f, 0.0f));
    vertices->point.set1Value(1, SbVec3f(2.0f, -2.0f, 0.0f));
    vertices->point.set1Value(2, SbVec3f(2.0f, 2.0f, 0.0f));
    vertices->point.set1Value(3, SbVec3f(-2.0f, 2.0f, 0.0f));
    root->addChild(vertices);
    auto * normal = new SoNormal;
    normal->vector.set1Value(0, SbVec3f(0.0f, 0.0f, 1.0f));
    root->addChild(normal);
    auto * normal_binding = new SoNormalBinding;
    normal_binding->value.setValue(SoNormalBinding::OVERALL);
    root->addChild(normal_binding);
    auto * quad = new SoFaceSet;
    quad->numVertices.set1Value(0, 4);
    root->addChild(quad);
    camera->viewAll(root, SbViewportRegion(128, 96));

    ObolTestSupport::RenderFixture fixture(128, 96);
    ASSERT_TRUE(fixture.available());
    ASSERT_TRUE(fixture.render(scene.root()));
    const auto first = centerPixel(fixture);
    const unsigned char blue[] = {20, 20, 240, 20, 20, 240, 20, 20, 240, 20, 20, 240};
    texture->image.setValue(SbVec2s(2, 2), 3, blue);
    ASSERT_TRUE(fixture.render(scene.root()));
    const auto second = centerPixel(fixture);
    EXPECT_GT(first[0], first[2] + 60);
    EXPECT_GT(second[2], second[0] + 60);
}

TEST(OSMesaFeatureContracts, RenderStateNodesAndEngineConnectionsRemainUsable)
{
    auto * root = new SoSeparator;
    OwnedScene scene(root);
    auto * camera = new SoPerspectiveCamera;
    camera->position.setValue(0.0f, 0.0f, 10.0f);
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);

    auto * lod = new SoLOD;
    lod->range.set1Value(0, 4.0f);
    lod->range.set1Value(1, 8.0f);
    lod->addChild(new SoSphere);
    lod->addChild(new SoSphere);
    root->addChild(lod);

    auto * screen_lod = new SoLevelOfDetail;
    screen_lod->screenArea.set1Value(0, 10000.0f);
    screen_lod->screenArea.set1Value(1, 1000.0f);
    screen_lod->addChild(new SoSphere);
    screen_lod->addChild(new SoSphere);
    root->addChild(screen_lod);

    auto * annotation = new SoAnnotation;
    annotation->addChild(new SoSphere);
    root->addChild(annotation);
    auto * array = new SoArray;
    array->numElements1.setValue(2);
    array->numElements2.setValue(2);
    array->separation1.setValue(SbVec3f(1.5f, 0.0f, 0.0f));
    array->separation2.setValue(SbVec3f(0.0f, 1.5f, 0.0f));
    array->addChild(new SoSphere);
    root->addChild(array);

    auto * timer = new SoElapsedTime;
    auto * interpolation = new SoInterpolateFloat;
    interpolation->alpha.connectFrom(&timer->timeOut);
    EXPECT_TRUE(interpolation->alpha.isConnected());
    auto * complexity = new SoComplexity;
    complexity->value.setValue(0.7f);
    root->addChild(complexity);
    auto * depth = new SoDepthBuffer;
    depth->test.setValue(TRUE);
    depth->write.setValue(TRUE);
    root->addChild(depth);
    auto * alpha = new SoAlphaTest;
    alpha->value.setValue(0.5f);
    root->addChild(alpha);

    auto * draggers = new SoSeparator;
    draggers->addChild(new SoHandleBoxDragger);
    draggers->addChild(new SoTabPlaneDragger);
    root->addChild(draggers);

    ObolTestSupport::RenderFixture fixture(160, 120);
    ASSERT_TRUE(fixture.available());
    ASSERT_TRUE(fixture.render(scene.root()));
    EXPECT_GT(fixture.nonBackgroundPixels(), 100u);
}

TEST(OSMesaFeatureContracts, RepeatedIndexedRenderSurvivesNormalCacheInvalidation)
{
    auto * root = new SoSeparator;
    OwnedScene scene(root);
    auto * camera = new SoPerspectiveCamera;
    camera->position.setValue(0.0f, 0.0f, 6.0f);
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);
    auto * hints = new SoShapeHints;
    hints->shapeType.setValue(SoShapeHints::SOLID);
    hints->creaseAngle.setValue(0.4f);
    root->addChild(hints);
    root->addChild(new SoSphere);
    auto * pick_style = new SoPickStyle;
    pick_style->style.setValue(SoPickStyle::BOUNDING_BOX);
    root->addChild(pick_style);

    ObolTestSupport::RenderFixture fixture(128, 96);
    ASSERT_TRUE(fixture.available());
    ASSERT_TRUE(fixture.render(scene.root()));
    const auto first = fixture.nonBackgroundPixels();
    ASSERT_TRUE(fixture.render(scene.root()));
    EXPECT_EQ(fixture.nonBackgroundPixels(), first);
    hints->creaseAngle.setValue(1.2f);
    ASSERT_TRUE(fixture.render(scene.root()));
    EXPECT_GT(fixture.nonBackgroundPixels(), 100u);

    SoRayPickAction picker(SbViewportRegion(128, 96));
    picker.setPoint(SbVec2s(64, 48));
    picker.apply(root);
    SUCCEED();
}

} // namespace
