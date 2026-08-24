#include "framework/render_fixture.h"

#include <gtest/gtest.h>

#include <Inventor/SbColor.h>
#include <Inventor/SbPlane.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoClipPlane.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoFaceSet.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoMaterialBinding.h>
#include <Inventor/nodes/SoPointLight.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSpotLight.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoShapeHints.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoVertexProperty.h>

#include <array>
#include <cstddef>

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

std::array<unsigned char, 3> pixelAt(const ObolTestSupport::RenderFixture & fixture,
                                     const int x, const int y)
{
    EXPECT_GE(x, 0);
    EXPECT_GE(y, 0);
    EXPECT_LT(x, fixture.width());
    EXPECT_LT(y, fixture.height());
    const std::size_t offset =
        (static_cast<std::size_t>(y) * fixture.width() + x) * 3;
    const auto & pixels = fixture.pixels();
    return {pixels[offset], pixels[offset + 1], pixels[offset + 2]};
}

OwnedScene makeLitSphere(const SbColor & color, const float transparency = 0.0f)
{
    auto * root = new SoSeparator;
    auto * camera = new SoPerspectiveCamera;
    camera->position.setValue(0.0f, 0.0f, 4.0f);
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);

    auto * material = new SoMaterial;
    material->diffuseColor.setValue(color);
    material->transparency.setValue(transparency);
    root->addChild(material);
    root->addChild(new SoSphere);
    return OwnedScene(root);
}

OwnedScene makeLitSphereWithLight(SoNode * light)
{
    auto * root = new SoSeparator;
    auto * camera = new SoPerspectiveCamera;
    camera->position.setValue(0.0f, 0.0f, 4.0f);
    root->addChild(camera);
    root->addChild(light);
    auto * material = new SoMaterial;
    material->diffuseColor.setValue(SbColor(0.8f, 0.3f, 0.1f));
    root->addChild(material);
    root->addChild(new SoSphere);
    return OwnedScene(root);
}

std::size_t countDominant(const ObolTestSupport::RenderFixture & fixture,
                          const int begin_x, const int end_x,
                          const int dominant_channel)
{
    std::size_t count = 0;
    const auto & pixels = fixture.pixels();
    for (int y = 0; y < fixture.height(); ++y) {
        for (int x = begin_x; x < end_x; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * fixture.width() + x) * 3;
            const int primary = pixels[offset + dominant_channel];
            const int secondary_a = pixels[offset + (dominant_channel + 1) % 3];
            const int secondary_b = pixels[offset + (dominant_channel + 2) % 3];
            if (primary > secondary_a + 30 && primary > secondary_b + 30) ++count;
        }
    }
    return count;
}

TEST(RenderFeatureBasics, RendersEachCorePrimitiveInItsExpectedRegion)
{
    auto * root = new SoSeparator;
    OwnedScene scene(root);
    auto * camera = new SoPerspectiveCamera;
    camera->position.setValue(0.0f, 0.0f, 8.0f);
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);

    struct Primitive {
        float x;
        float y;
        SbColor color;
        SoNode * shape;
    };
    const Primitive primitives[] = {
        {-1.7f,  1.3f, SbColor(0.9f, 0.1f, 0.1f), new SoSphere},
        { 1.7f,  1.3f, SbColor(0.1f, 0.8f, 0.1f), new SoCube},
        {-1.7f, -1.3f, SbColor(0.1f, 0.2f, 0.9f), new SoCone},
        { 1.7f, -1.3f, SbColor(0.9f, 0.7f, 0.1f), new SoCylinder},
    };
    for (const Primitive & primitive : primitives) {
        auto * group = new SoSeparator;
        auto * translation = new SoTranslation;
        translation->translation.setValue(primitive.x, primitive.y, 0.0f);
        group->addChild(translation);
        auto * material = new SoMaterial;
        material->diffuseColor.setValue(primitive.color);
        group->addChild(material);
        group->addChild(primitive.shape);
        root->addChild(group);
    }

    ObolTestSupport::RenderFixture fixture(192, 144);
    ASSERT_TRUE(fixture.available());
    ASSERT_TRUE(fixture.render(scene.root()));

    EXPECT_GT(countDominant(fixture, 0, 96, 0), 180u);
    EXPECT_GT(countDominant(fixture, 96, 192, 1), 180u);
    EXPECT_GT(countDominant(fixture, 0, 96, 2), 180u);
    EXPECT_GT(countDominant(fixture, 96, 192, 0), 180u);
}

TEST(OSMesaRenderContracts, ClearsBackgroundAndRendersLitMaterialAtTheCameraTarget)
{
    const SbColor background(0.05f, 0.10f, 0.20f);
    OwnedScene scene = makeLitSphere(SbColor(0.9f, 0.1f, 0.1f));
    ObolTestSupport::RenderFixture fixture(128, 96, background);

    ASSERT_TRUE(fixture.available());
    ASSERT_TRUE(fixture.render(scene.root()));

    const auto corner = pixelAt(fixture, 0, 0);
    EXPECT_NEAR(corner[0], 13, 2);
    EXPECT_NEAR(corner[1], 26, 2);
    EXPECT_NEAR(corner[2], 51, 2);

    const auto center = pixelAt(fixture, 64, 48);
    EXPECT_GT(center[0], center[1] + 40);
    EXPECT_GT(center[0], center[2] + 40);
}

TEST(RenderFeatureBasics, AppliesTransformsAndMaterialsToSeparateScreenRegions)
{
    auto * root = new SoSeparator;
    OwnedScene scene(root);
    auto * camera = new SoPerspectiveCamera;
    camera->position.setValue(0.0f, 0.0f, 5.0f);
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);

    auto * left = new SoSeparator;
    auto * left_translation = new SoTranslation;
    left_translation->translation.setValue(-1.0f, 0.0f, 0.0f);
    left->addChild(left_translation);
    auto * red = new SoMaterial;
    red->diffuseColor.setValue(0.9f, 0.05f, 0.05f);
    left->addChild(red);
    left->addChild(new SoSphere);
    root->addChild(left);

    auto * right = new SoSeparator;
    auto * right_translation = new SoTranslation;
    right_translation->translation.setValue(1.0f, 0.0f, 0.0f);
    right->addChild(right_translation);
    auto * green = new SoMaterial;
    green->diffuseColor.setValue(0.05f, 0.9f, 0.05f);
    right->addChild(green);
    right->addChild(new SoCube);
    root->addChild(right);

    ObolTestSupport::RenderFixture fixture(160, 120);
    ASSERT_TRUE(fixture.available());
    ASSERT_TRUE(fixture.render(scene.root()));

    const std::size_t left_red = countDominant(fixture, 0, 80, 0);
    const std::size_t left_green = countDominant(fixture, 0, 80, 1);
    const std::size_t right_red = countDominant(fixture, 80, 160, 0);
    const std::size_t right_green = countDominant(fixture, 80, 160, 1);
    EXPECT_GT(left_red, 250u);
    EXPECT_GT(right_green, 250u);
    EXPECT_GT(left_red, left_green * 2);
    EXPECT_GT(right_green, right_red * 2);
}

TEST(RenderFeatureBasics, VertexPropertiesAndShapeHintsRenderVisibleGeometry)
{
    auto * root = new SoSeparator;
    OwnedScene scene(root);
    auto * camera = new SoPerspectiveCamera;
    camera->position.setValue(0.0f, 0.0f, 5.0f);
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);

    auto * hints = new SoShapeHints;
    hints->vertexOrdering.setValue(SoShapeHints::COUNTERCLOCKWISE);
    hints->shapeType.setValue(SoShapeHints::SOLID);
    hints->creaseAngle.setValue(0.7f);
    root->addChild(hints);

    auto * vertices = new SoVertexProperty;
    vertices->vertex.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
    vertices->vertex.set1Value(1, SbVec3f(1.0f, -1.0f, 0.0f));
    vertices->vertex.set1Value(2, SbVec3f(0.0f, 1.0f, 0.0f));
    vertices->orderedRGBA.set1Value(0, 0xFF0000FF);
    vertices->orderedRGBA.set1Value(1, 0x00FF00FF);
    vertices->orderedRGBA.set1Value(2, 0x0000FFFF);
    vertices->materialBinding.setValue(SoVertexProperty::PER_VERTEX);
    vertices->normalBinding.setValue(SoVertexProperty::PER_VERTEX);
    vertices->normal.set1Value(0, SbVec3f(0.0f, 0.0f, 1.0f));
    vertices->normal.set1Value(1, SbVec3f(0.0f, 0.0f, 1.0f));
    vertices->normal.set1Value(2, SbVec3f(0.0f, 0.0f, 1.0f));
    auto * triangle = new SoFaceSet;
    triangle->numVertices.set1Value(0, 3);
    triangle->vertexProperty.setValue(vertices);
    root->addChild(triangle);

    ObolTestSupport::RenderFixture fixture(128, 96);
    ASSERT_TRUE(fixture.available());
    ASSERT_TRUE(fixture.render(scene.root()));
    EXPECT_GT(fixture.nonBackgroundPixels(), 100u);
}

TEST(OSMesaRenderContracts, TransparencyBlendsInsteadOfDiscardingVisibleGeometry)
{
    OwnedScene opaque = makeLitSphere(SbColor(0.0f, 0.9f, 0.0f));
    OwnedScene transparent = makeLitSphere(SbColor(0.0f, 0.9f, 0.0f), 0.5f);
    ObolTestSupport::RenderFixture opaque_fixture(128, 96);
    ObolTestSupport::RenderFixture transparent_fixture(128, 96);

    ASSERT_TRUE(opaque_fixture.available());
    ASSERT_TRUE(transparent_fixture.available());
    ASSERT_TRUE(opaque_fixture.render(opaque.root()));
    ASSERT_TRUE(transparent_fixture.render(transparent.root()));

    const auto opaque_center = pixelAt(opaque_fixture, 64, 48);
    const auto transparent_center = pixelAt(transparent_fixture, 64, 48);
    EXPECT_GT(opaque_center[1], opaque_center[0] + 40);
    EXPECT_GT(transparent_center[1], transparent_center[0] + 20);
    EXPECT_LT(transparent_center[1], opaque_center[1] - 20);
}

TEST(OSMesaRenderContracts, DirectionalPointAndSpotLightsIlluminateGeometry)
{
    ObolTestSupport::RenderFixture fixture(128, 96);
    ASSERT_TRUE(fixture.available());

    const auto render_light = [&fixture](SoNode * light) {
        OwnedScene scene = makeLitSphereWithLight(light);
        EXPECT_TRUE(fixture.render(scene.root()));
        EXPECT_GT(fixture.nonBackgroundPixels(), 100u);
    };

    render_light(new SoDirectionalLight);

    auto * point = new SoPointLight;
    point->location.setValue(0.0f, 0.0f, 4.0f);
    render_light(point);

    auto * spot = new SoSpotLight;
    spot->location.setValue(0.0f, 0.0f, 4.0f);
    spot->direction.setValue(0.0f, 0.0f, -1.0f);
    render_light(spot);
}

TEST(OSMesaRenderContracts, ClipPlaneReducesVisibleGeometryWithoutDisablingScene)
{
    auto make_scene = [](const bool clipped) {
        auto * root = new SoSeparator;
        auto * camera = new SoPerspectiveCamera;
        camera->position.setValue(0.0f, 0.0f, 4.0f);
        root->addChild(camera);
        root->addChild(new SoDirectionalLight);
        if (clipped) {
            auto * clip = new SoClipPlane;
            clip->plane.setValue(SbPlane(SbVec3f(0.0f, 1.0f, 0.0f), 0.0f));
            root->addChild(clip);
        }
        auto * material = new SoMaterial;
        material->diffuseColor.setValue(SbColor(0.1f, 0.8f, 0.8f));
        root->addChild(material);
        root->addChild(new SoSphere);
        return OwnedScene(root);
    };

    ObolTestSupport::RenderFixture fixture(128, 96);
    ASSERT_TRUE(fixture.available());
    OwnedScene whole = make_scene(false);
    ASSERT_TRUE(fixture.render(whole.root()));
    const std::size_t whole_pixels = fixture.nonBackgroundPixels();
    OwnedScene clipped = make_scene(true);
    ASSERT_TRUE(fixture.render(clipped.root()));
    const std::size_t clipped_pixels = fixture.nonBackgroundPixels();
    EXPECT_GT(clipped_pixels, 100u);
    EXPECT_LT(clipped_pixels, whole_pixels);
}

TEST(OSMesaRenderContracts, RepeatedRendersReuseCacheAndMaterialChangesInvalidateIt)
{
    auto * root = new SoSeparator;
    OwnedScene scene(root);
    auto * camera = new SoPerspectiveCamera;
    camera->position.setValue(0.0f, 0.0f, 4.0f);
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);
    auto * material = new SoMaterial;
    material->diffuseColor.setValue(SbColor(0.9f, 0.1f, 0.1f));
    root->addChild(material);
    root->addChild(new SoSphere);

    ObolTestSupport::RenderFixture fixture(128, 96);
    ASSERT_TRUE(fixture.available());
    ASSERT_TRUE(fixture.render(scene.root()));
    const auto first_frame = fixture.pixels();
    ASSERT_TRUE(fixture.render(scene.root()));
    EXPECT_EQ(fixture.pixels(), first_frame);

    material->diffuseColor.setValue(SbColor(0.1f, 0.1f, 0.9f));
    ASSERT_TRUE(fixture.render(scene.root()));
    EXPECT_NE(fixture.pixels(), first_frame);
    EXPECT_GT(fixture.nonBackgroundPixels(), 100u);
}

} // namespace
