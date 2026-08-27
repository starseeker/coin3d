#include <gtest/gtest.h>

#include <Inventor/SbBox3f.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/nodes/SoCamera.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/nodes/SoLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoTransform.h>

#include <map>
#include <memory>
#include <string>

namespace {

struct SceneDeleter {
    void operator()(SoSeparator * root) const
    {
        if (root) root->unref();
    }
};

using ScenePtr = std::unique_ptr<SoSeparator, SceneDeleter>;

ScenePtr minimalScene()
{
    SoSeparator * root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera * camera = new SoPerspectiveCamera;
    camera->position.setValue(0.0f, 0.0f, 5.0f);
    camera->nearDistance.setValue(1.0f);
    camera->farDistance.setValue(10.0f);
    root->addChild(camera);

    SoDirectionalLight * light = new SoDirectionalLight;
    light->direction.setValue(0.0f, 0.0f, -1.0f);
    root->addChild(light);
    return ScenePtr(root);
}

ScenePtr basicGeometryScene()
{
    ScenePtr root = minimalScene();
    SoCube * cube = new SoCube;
    cube->width.setValue(2.0f);
    cube->height.setValue(2.0f);
    cube->depth.setValue(2.0f);
    root->addChild(cube);
    return root;
}

ScenePtr materialScene()
{
    ScenePtr root = basicGeometryScene();
    SoMaterial * material = new SoMaterial;
    material->diffuseColor.setValue(0.8f, 0.2f, 0.2f);
    root->insertChild(material, root->getNumChildren() - 1);
    return root;
}

ScenePtr transformScene()
{
    ScenePtr root = minimalScene();
    SoTransform * transform = new SoTransform;
    transform->translation.setValue(1.0f, 1.0f, 0.0f);
    transform->rotation.setValue(SbVec3f(0.0f, 1.0f, 0.0f), 0.785f);
    root->addChild(transform);

    SoSphere * sphere = new SoSphere;
    sphere->radius.setValue(1.0f);
    root->addChild(sphere);
    return root;
}

void countNodeTypes(SoNode * node, std::map<std::string, int> & counts)
{
    if (!node) return;
    ++counts[node->getTypeId().getName().getString()];
    if (!node->isOfType(SoGroup::getClassTypeId())) return;

    SoGroup * group = static_cast<SoGroup *>(node);
    for (int i = 0; i < group->getNumChildren(); ++i) {
        countNodeTypes(group->getChild(i), counts);
    }
}

bool containsType(SoNode * root, SoType type)
{
    if (!root) return false;
    SoSearchAction search;
    search.setType(type);
    search.setInterest(SoSearchAction::FIRST);
    search.apply(root);
    return search.getPath() != nullptr;
}

SbBox3f boundingBox(SoNode * root)
{
    SoGetBoundingBoxAction action{SbViewportRegion()};
    action.apply(root);
    return action.getBoundingBox();
}

TEST(SceneGraphStructure, RejectsNullRoot)
{
    EXPECT_FALSE(containsType(nullptr, SoCamera::getClassTypeId()));
}

TEST(SceneGraphStructure, MinimalSceneContainsRequiredComponents)
{
    ScenePtr root = minimalScene();
    ASSERT_NE(root, nullptr);
    EXPECT_TRUE(root->getTypeId().isDerivedFrom(SoNode::getClassTypeId()));
    EXPECT_TRUE(containsType(root.get(), SoCamera::getClassTypeId()));
    EXPECT_TRUE(containsType(root.get(), SoLight::getClassTypeId()));

    std::map<std::string, int> counts;
    countNodeTypes(root.get(), counts);
    EXPECT_EQ(counts["Separator"], 1);
    EXPECT_EQ(counts["PerspectiveCamera"], 1);
    EXPECT_EQ(counts["DirectionalLight"], 1);
}

TEST(SceneGraphStructure, BasicGeometryHasExpectedBounds)
{
    ScenePtr root = basicGeometryScene();
    ASSERT_TRUE(containsType(root.get(), SoCube::getClassTypeId()));

    const SbBox3f box = boundingBox(root.get());
    EXPECT_FALSE(box.isEmpty());
    EXPECT_EQ(box.getMin(), SbVec3f(-1.0f, -1.0f, -1.0f));
    EXPECT_EQ(box.getMax(), SbVec3f(1.0f, 1.0f, 1.0f));
}

TEST(SceneGraphStructure, MaterialPrecedesTheGeometryItAffects)
{
    ScenePtr root = materialScene();
    ASSERT_EQ(root->getNumChildren(), 4);
    ASSERT_TRUE(root->getChild(2)->isOfType(SoMaterial::getClassTypeId()));
    ASSERT_TRUE(root->getChild(3)->isOfType(SoCube::getClassTypeId()));

    const SoMaterial * material =
        static_cast<const SoMaterial *>(root->getChild(2));
    EXPECT_EQ(material->diffuseColor[0], SbColor(0.8f, 0.2f, 0.2f));
}

TEST(SceneGraphStructure, TransformAffectsFollowingGeometry)
{
    ScenePtr root = transformScene();
    ASSERT_EQ(root->getNumChildren(), 4);
    ASSERT_TRUE(root->getChild(2)->isOfType(SoTransform::getClassTypeId()));
    ASSERT_TRUE(root->getChild(3)->isOfType(SoSphere::getClassTypeId()));

    const SbBox3f box = boundingBox(root.get());
    EXPECT_FALSE(box.isEmpty());
    EXPECT_NEAR(box.getCenter()[0], 1.0f, 1.0e-5f);
    EXPECT_NEAR(box.getCenter()[1], 1.0f, 1.0e-5f);
    EXPECT_NEAR(box.getCenter()[2], 0.0f, 1.0e-5f);
}

} // namespace
