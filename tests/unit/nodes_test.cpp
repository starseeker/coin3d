#include <gtest/gtest.h>

#include <Inventor/SbBox3f.h>
#include <Inventor/SbColor.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbName.h>
#include <Inventor/SbRotation.h>
#include <Inventor/SbString.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoType.h>
#include <Inventor/misc/SoBase.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/actions/SoGetMatrixAction.h>
#include <Inventor/actions/SoGetPrimitiveCountAction.h>
#include <Inventor/actions/SoRayPickAction.h>
#include <Inventor/actions/SoCallbackAction.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/SoPath.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoComplexity.h>
#include <Inventor/nodes/SoAnnotation.h>
#include <Inventor/nodes/SoAsciiText.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/nodes/SoExtSelection.h>
#include <Inventor/nodes/SoFaceSet.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoFont.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/nodes/SoInfo.h>
#include <Inventor/nodes/SoIndexedLineSet.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <Inventor/nodes/SoLOD.h>
#include <Inventor/nodes/SoLightModel.h>
#include <Inventor/nodes/SoLineSet.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoPointLight.h>
#include <Inventor/nodes/SoPointSet.h>
#include <Inventor/nodes/SoRotation.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoShapeHints.h>
#include <Inventor/nodes/SoScale.h>
#include <Inventor/nodes/SoSelection.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoSpotLight.h>
#include <Inventor/nodes/SoSwitch.h>
#include <Inventor/nodes/SoText2.h>
#include <Inventor/nodes/SoText3.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/lists/SoNodeList.h>
#include <Inventor/lists/SoBaseList.h>

#include <cmath>

namespace {

constexpr float kPi = 3.14159265358979323846f;

} // namespace

TEST(Nodes, RuntimeTypesNamesAndBasicHierarchyAreAvailable)
{
    auto * root = new SoSeparator;
    root->ref();
    auto * cube = new SoCube;
    cube->setName("modern-node-lookup");
    auto * sphere = new SoSphere;
    root->addChild(cube);
    root->addChild(sphere);

    EXPECT_NE(cube->getTypeId(), SoType::badType());
    EXPECT_TRUE(cube->isOfType(SoCube::getClassTypeId()));
    EXPECT_TRUE(cube->isOfType(SoNode::getClassTypeId()));
    EXPECT_TRUE(cube->isOfType(SoShape::getClassTypeId()));
    EXPECT_FALSE(cube->isOfType(SoSphere::getClassTypeId()));
    EXPECT_EQ(sphere->getTypeId(), SoSphere::getClassTypeId());
    EXPECT_EQ(sphere->getTypeId().getName(), SbName("Sphere"));
    EXPECT_EQ(SoType::fromName("Cube"), SoCube::getClassTypeId());
    EXPECT_EQ(SoType::fromName("NonExistentType12345"), SoType::badType());
    EXPECT_TRUE(SoSphere::getClassTypeId().isDerivedFrom(SoShape::getClassTypeId()));
    EXPECT_FALSE(SoShape::getClassTypeId().isDerivedFrom(SoSphere::getClassTypeId()));
    EXPECT_EQ(root->getNumChildren(), 2);
    EXPECT_EQ(SoNode::getByName(SbName("modern-node-lookup")), cube);

    cube->width = 5.0f;
    auto * copy = static_cast<SoCube *>(cube->copy());
    copy->ref();
    EXPECT_FLOAT_EQ(copy->width.getValue(), cube->width.getValue());
    copy->unref();

    root->removeChild(cube);
    EXPECT_EQ(root->getNumChildren(), 1);
    root->unref();
}

TEST(Nodes, BaseReferenceCountingNamesAndTypeNamesAreStable)
{
    auto * cube = new SoCube;
    cube->ref();
    EXPECT_EQ(cube->getRefCount(), 1);
    cube->ref();
    EXPECT_EQ(cube->getRefCount(), 2);
    cube->unref();
    EXPECT_EQ(cube->getRefCount(), 1);
    cube->unref();

    auto * sphere = new SoSphere;
    sphere->ref();
    sphere->setName("modern-base-name");
    EXPECT_EQ(sphere->getName(), SbName("modern-base-name"));
    sphere->unref();

    EXPECT_EQ(SoCylinder::getClassTypeId().getName(), SbName("Cylinder"));

    auto * cylinder = new SoCylinder;
    cylinder->ref();
    cylinder->setName("modern-named-cylinder");
    EXPECT_EQ(SoBase::getNamedBase("modern-named-cylinder", SoNode::getClassTypeId()), cylinder);
    cylinder->unref();

    auto * cone_one = new SoCone;
    auto * cone_two = new SoCone;
    cone_one->ref();
    cone_two->ref();
    cone_one->setName("modern-shared-name");
    cone_two->setName("modern-shared-name");
    SoBaseList named_bases;
    EXPECT_GE(SoBase::getNamedBases("modern-shared-name", named_bases,
                                    SoNode::getClassTypeId()), 2);
    cone_one->unref();
    cone_two->unref();
}

TEST(Nodes, CopiesHierarchiesResetsDefaultsAndReportsStateEffects)
{
    auto * material = new SoMaterial;
    material->ref();
    material->diffuseColor.setValue(0.5f, 0.5f, 0.5f);
    material->setToDefaults();
    auto * default_material = new SoMaterial;
    default_material->ref();
    const SbColor reset_color = material->diffuseColor[0];
    const SbColor default_color = default_material->diffuseColor[0];
    EXPECT_FLOAT_EQ(reset_color[0], default_color[0]);
    EXPECT_FLOAT_EQ(reset_color[1], default_color[1]);
    EXPECT_FLOAT_EQ(reset_color[2], default_color[2]);
    EXPECT_TRUE(material->affectsState());
    default_material->unref();
    material->unref();

    auto * original = new SoSeparator;
    original->ref();
    original->addChild(new SoMaterial);
    original->addChild(new SoSphere);
    for (const SbBool copy_connections : {FALSE, TRUE}) {
        auto * copy = static_cast<SoSeparator *>(original->copy(copy_connections));
        ASSERT_NE(copy, nullptr);
        copy->ref();
        EXPECT_NE(copy, original);
        EXPECT_EQ(copy->getNumChildren(), original->getNumChildren());
        EXPECT_NE(copy->getChild(0), original->getChild(0));
        EXPECT_NE(copy->getChild(1), original->getChild(1));
        copy->unref();
    }
    original->unref();

    auto * override_material = new SoMaterial;
    override_material->ref();
    override_material->setOverride(TRUE);
    EXPECT_TRUE(override_material->isOverride());
    override_material->setOverride(FALSE);
    EXPECT_FALSE(override_material->isOverride());
    override_material->unref();

    auto * first_named = new SoSphere;
    first_named->ref();
    first_named->setName("modern-multi-name");
    auto * second_named = new SoSphere;
    second_named->ref();
    second_named->setName("modern-multi-name");
    SoNodeList named_nodes;
    EXPECT_GE(SoNode::getByName(SbName("modern-multi-name"), named_nodes), 2);
    first_named->unref();
    second_named->unref();

    const SbUniqueId first_next_id = SoNode::getNextNodeId();
    auto * id_node = new SoCube;
    id_node->ref();
    const SbUniqueId second_next_id = SoNode::getNextNodeId();
    EXPECT_GT(second_next_id, first_next_id);
    id_node->unref();

    auto * identified_sphere = new SoSphere;
    identified_sphere->ref();
    const SbUniqueId before_change = identified_sphere->getNodeId();
    identified_sphere->radius.setValue(5.0f);
    EXPECT_NE(identified_sphere->getNodeId(), before_change);
    identified_sphere->unref();
}

TEST(Nodes, GroupsSupportInsertionReplacementRemovalAndBounds)
{
    auto * group = new SoGroup;
    group->ref();
    auto * first = new SoCube;
    auto * second = new SoCube;
    auto * sphere = new SoSphere;
    group->addChild(first);
    group->addChild(second);
    group->addChild(sphere);
    ASSERT_EQ(group->getNumChildren(), 3);
    EXPECT_EQ(group->getChild(0), first);
    EXPECT_EQ(group->findChild(sphere), 2);
    auto * absent = new SoCone;
    absent->ref();
    EXPECT_EQ(group->findChild(absent), -1);
    absent->unref();

    auto * cone = new SoCone;
    group->insertChild(cone, 1);
    ASSERT_EQ(group->getNumChildren(), 4);
    EXPECT_EQ(group->getChild(1), cone);

    auto * cylinder = new SoCylinder;
    group->replaceChild(cone, cylinder);
    EXPECT_EQ(group->getChild(1), cylinder);
    group->removeChild(0);
    EXPECT_EQ(group->getNumChildren(), 3);
    group->removeChild(cylinder);
    EXPECT_EQ(group->getNumChildren(), 2);
    EXPECT_EQ(group->findChild(cylinder), -1);
    group->removeAllChildren();
    EXPECT_EQ(group->getNumChildren(), 0);
    group->unref();

    auto * root = new SoGroup;
    root->ref();
    auto * cube = new SoCube;
    cube->width = 2.0f;
    cube->height = 2.0f;
    cube->depth = 2.0f;
    root->addChild(cube);
    auto * transform = new SoTransform;
    transform->translation.setValue(5.0f, 0.0f, 0.0f);
    root->addChild(transform);
    root->addChild(new SoSphere);
    SoGetBoundingBoxAction bounds(SbViewportRegion(512, 512));
    bounds.apply(root);
    ASSERT_FALSE(bounds.getBoundingBox().isEmpty());
    EXPECT_GE(bounds.getBoundingBox().getMax()[0], 5.0f);
    root->unref();
}

TEST(Nodes, PerspectiveCameraProvidesStableViewOperations)
{
    auto * camera = new SoPerspectiveCamera;
    camera->ref();
    const SbVec3f default_position = camera->position.getValue();
    EXPECT_FLOAT_EQ(default_position[0], 0.0f);
    EXPECT_FLOAT_EQ(default_position[1], 0.0f);
    EXPECT_FLOAT_EQ(default_position[2], 1.0f);
    EXPECT_GT(camera->nearDistance.getValue(), 0.0f);

    camera->position.setValue(0.0f, 0.0f, 5.0f);
    camera->heightAngle.setValue(kPi / 2.0f);
    camera->nearDistance.setValue(0.1f);
    camera->farDistance.setValue(100.0f);
    camera->focalDistance.setValue(50.0f);
    EXPECT_FLOAT_EQ(camera->focalDistance.getValue(), 50.0f);
    const SbVec3f sight = camera->getViewVolume(1.0f).getSightPoint(5.0f);
    EXPECT_NEAR(sight[0], 0.0f, 0.1f);
    EXPECT_NEAR(sight[1], 0.0f, 0.1f);

    camera->pointAt(SbVec3f(0.0f, 0.0f, 0.0f));
    const SbRotation orientation = camera->orientation.getValue();
    EXPECT_TRUE(std::isfinite(orientation.getValue()[0]));
    EXPECT_GT(camera->getViewVolume(2.0f).getWidth(),
              camera->getViewVolume(2.0f).getHeight());
    camera->aspectRatio.setValue(4.0f / 3.0f);
    const SbViewportRegion viewport_bounds = camera->getViewportBounds(SbViewportRegion(800, 600));
    EXPECT_GT(viewport_bounds.getWindowSize()[0], 0);
    EXPECT_GT(viewport_bounds.getWindowSize()[1], 0);
    camera->setStereoMode(SoCamera::MONOSCOPIC);
    EXPECT_EQ(camera->getStereoMode(), SoCamera::MONOSCOPIC);
    camera->setStereoAdjustment(0.1f);
    EXPECT_FLOAT_EQ(camera->getStereoAdjustment(), 0.1f);
    camera->unref();

    auto * root = new SoSeparator;
    root->ref();
    root->addChild(new SoCube);
    auto * view_all_camera = new SoPerspectiveCamera;
    view_all_camera->ref();
    view_all_camera->viewAll(root, SbViewportRegion(512, 512));
    EXPECT_GT(view_all_camera->position.getValue().length(), 0.1f);
    view_all_camera->unref();
    root->unref();
}

TEST(Nodes, OrthographicCameraProvidesProjectionAndPathViewAll)
{
    auto * camera = new SoOrthographicCamera;
    camera->ref();
    camera->position.setValue(0.0f, 0.0f, 10.0f);
    camera->height.setValue(5.0f);
    camera->nearDistance.setValue(0.1f);
    camera->farDistance.setValue(100.0f);
    EXPECT_EQ(camera->getViewVolume(1.0f).getProjectionType(),
              SbViewVolume::ORTHOGRAPHIC);

    auto * root = new SoSeparator;
    root->ref();
    auto * cube = new SoCube;
    cube->setName("orthographic-view-all-cube");
    cube->width.setValue(4.0f);
    root->addChild(camera);
    root->addChild(cube);

    SoSearchAction search;
    search.setNode(cube);
    search.apply(root);
    ASSERT_NE(search.getPath(), nullptr);
    SoPath * cube_path = search.getPath();
    cube_path->ref();
    camera->viewAll(cube_path, SbViewportRegion(512, 512));
    EXPECT_GT(camera->position.getValue()[2], 1.0f);

    cube_path->unref();
    root->removeChild(camera);
    camera->unref();
    root->unref();
}

TEST(Nodes, CameraOrbitPreservesRadiusFocusAndLocalYawUpDirection)
{
    constexpr float radius = 5.0f;
    constexpr float sensitivity = 0.25f;
    const SbVec3f center(0, 0, 0);
    auto * camera = new SoPerspectiveCamera;
    camera->ref();
    camera->position.setValue(0, 0, radius);
    camera->orientation.setValue(SbRotation::identity());

    camera->orbitCamera(center, 80.0f, 0.0f, sensitivity);
    EXPECT_NEAR((camera->position.getValue() - center).length(), radius, 1e-3f);
    EXPECT_LT(camera->position.getValue()[0], 0.0f);
    SbVec3f view_direction;
    camera->orientation.getValue().multVec(SbVec3f(0, 0, -1), view_direction);
    SbVec3f toward_center = center - camera->position.getValue();
    toward_center.normalize();
    EXPECT_NEAR(toward_center.dot(view_direction), 1.0f, 1e-3f);

    camera->position.setValue(0, 0, radius);
    camera->orientation.setValue(SbRotation::identity());
    camera->orbitCamera(center, 0.0f, 80.0f, sensitivity);
    EXPECT_NEAR((camera->position.getValue() - center).length(), radius, 1e-3f);
    EXPECT_GT(camera->position.getValue()[1], 0.0f);

    camera->position.setValue(0, 0, radius);
    camera->orientation.setValue(SbRotation::identity());
    camera->orbitCamera(center, 0.0f, 180.0f, sensitivity);
    SbVec3f up_after_pitch;
    camera->orientation.getValue().multVec(SbVec3f(0, 1, 0), up_after_pitch);
    for (int iteration = 0; iteration < 720; ++iteration) {
        camera->orbitCamera(center, 1.0f, 0.0f, sensitivity);
    }
    SbVec3f up_after_yaw;
    camera->orientation.getValue().multVec(SbVec3f(0, 1, 0), up_after_yaw);
    EXPECT_NEAR((up_after_yaw - up_after_pitch).length(), 0.0f, 1e-3f);
    camera->unref();
}

TEST(Nodes, MaterialFieldsRetainConfiguredAppearance)
{
    auto * material = new SoMaterial;
    material->ref();
    ASSERT_GE(material->diffuseColor.getNum(), 1);
    material->ambientColor.setValue(0.2f, 0.2f, 0.2f);
    material->diffuseColor.set1Value(0, SbColor(1.0f, 0.0f, 0.0f));
    material->specularColor.set1Value(0, SbColor(0.5f, 0.5f, 0.5f));
    material->emissiveColor.setValue(0.1f, 0.0f, 0.0f);
    material->shininess.set1Value(0, 0.8f);
    material->transparency.set1Value(0, 0.5f);

    EXPECT_FLOAT_EQ(material->diffuseColor[0][0], 1.0f);
    EXPECT_FLOAT_EQ(material->diffuseColor[0][1], 0.0f);
    EXPECT_FLOAT_EQ(material->ambientColor[0][0], 0.2f);
    EXPECT_FLOAT_EQ(material->emissiveColor[0][0], 0.1f);
    EXPECT_FLOAT_EQ(material->shininess[0], 0.8f);
    EXPECT_FLOAT_EQ(material->transparency[0], 0.5f);
    material->diffuseColor.set1Value(1, SbColor(0.0f, 1.0f, 0.0f));
    material->diffuseColor.set1Value(2, SbColor(0.0f, 0.0f, 1.0f));
    material->transparency.set1Value(1, 0.25f);
    material->transparency.set1Value(2, 1.0f);
    EXPECT_EQ(material->diffuseColor.getNum(), 3);
    EXPECT_FLOAT_EQ(material->transparency[1], 0.25f);
    material->unref();

    auto * root = new SoSeparator;
    root->ref();
    root->addChild(new SoMaterial);
    root->addChild(new SoSphere);
    SoGetBoundingBoxAction bounds(SbViewportRegion(512, 512));
    bounds.apply(root);
    EXPECT_FALSE(bounds.getBoundingBox().isEmpty());
    root->unref();
}

TEST(Nodes, AppearancePropertyNodesRetainConfiguredState)
{
    auto * base_color = new SoBaseColor;
    base_color->ref();
    base_color->rgb.set1Value(0, SbColor(0.0f, 1.0f, 0.0f));
    EXPECT_EQ(base_color->rgb[0], SbColor(0.0f, 1.0f, 0.0f));
    base_color->unref();

    auto * draw_style = new SoDrawStyle;
    draw_style->ref();
    draw_style->style.setValue(SoDrawStyle::LINES);
    draw_style->lineWidth.setValue(2.0f);
    draw_style->pointSize.setValue(5.0f);
    EXPECT_EQ(draw_style->style.getValue(), SoDrawStyle::LINES);
    EXPECT_FLOAT_EQ(draw_style->lineWidth.getValue(), 2.0f);
    EXPECT_FLOAT_EQ(draw_style->pointSize.getValue(), 5.0f);
    draw_style->unref();

    auto * light_model = new SoLightModel;
    light_model->ref();
    light_model->model.setValue(SoLightModel::BASE_COLOR);
    EXPECT_EQ(light_model->model.getValue(), SoLightModel::BASE_COLOR);
    light_model->unref();
}

TEST(Nodes, TransformFieldsProduceTheExpectedMatrix)
{
    auto * transform = new SoTransform;
    transform->ref();
    transform->translation.setValue(SbVec3f(1.0f, 2.0f, 3.0f));
    transform->rotation.setValue(SbRotation(SbVec3f(0, 1, 0), kPi / 4.0f));
    transform->scaleFactor.setValue(SbVec3f(2.0f, 2.0f, 2.0f));

    EXPECT_EQ(transform->translation.getValue(), SbVec3f(1.0f, 2.0f, 3.0f));
    EXPECT_EQ(transform->scaleFactor.getValue(), SbVec3f(2.0f, 2.0f, 2.0f));

    SoGetMatrixAction action(SbViewportRegion(512, 512));
    action.apply(transform);
    SbVec3f transformed_origin;
    action.getMatrix().multVecMatrix(SbVec3f(0.0f, 0.0f, 0.0f), transformed_origin);
    EXPECT_NEAR(transformed_origin[0], 1.0f, 0.1f);
    EXPECT_NEAR(transformed_origin[1], 2.0f, 0.1f);
    EXPECT_NEAR(transformed_origin[2], 3.0f, 0.1f);
    transform->unref();
}

TEST(Nodes, TransformVariantsSetMatricesRecenterAndPreserveBounds)
{
    auto * matrix_transform = new SoTransform;
    matrix_transform->ref();
    SbMatrix translation_matrix = SbMatrix::identity();
    translation_matrix.setTranslate(SbVec3f(5.0f, 0.0f, 0.0f));
    matrix_transform->setMatrix(translation_matrix);
    EXPECT_NEAR(matrix_transform->translation.getValue()[0], 5.0f, 0.1f);
    matrix_transform->recenter(SbVec3f(0.5f, 0.0f, 0.0f));
    EXPECT_EQ(matrix_transform->center.getValue(), SbVec3f(0.5f, 0.0f, 0.0f));
    matrix_transform->unref();

    auto * translated_root = new SoSeparator;
    translated_root->ref();
    auto * translation = new SoTranslation;
    translation->translation.setValue(3.0f, 4.0f, 5.0f);
    translated_root->addChild(translation);
    translated_root->addChild(new SoSphere);
    SoGetBoundingBoxAction translated_bounds(SbViewportRegion(512, 512));
    translated_bounds.apply(translated_root);
    EXPECT_NEAR(translated_bounds.getBoundingBox().getCenter()[0], 3.0f, 0.2f);
    translated_root->unref();

    auto * rotated_root = new SoSeparator;
    rotated_root->ref();
    auto * rotation = new SoRotation;
    rotation->rotation.setValue(SbRotation(SbVec3f(0, 0, 1), kPi / 2.0f));
    rotated_root->addChild(rotation);
    rotated_root->addChild(new SoSphere);
    SoGetBoundingBoxAction rotated_bounds(SbViewportRegion(512, 512));
    rotated_bounds.apply(rotated_root);
    EXPECT_FALSE(rotated_bounds.getBoundingBox().isEmpty());
    rotated_root->unref();

    auto * scaled_root = new SoSeparator;
    scaled_root->ref();
    auto * scale = new SoScale;
    scale->scaleFactor.setValue(3.0f, 3.0f, 3.0f);
    scaled_root->addChild(scale);
    scaled_root->addChild(new SoCube);
    SoGetBoundingBoxAction scaled_bounds(SbViewportRegion(512, 512));
    scaled_bounds.apply(scaled_root);
    EXPECT_NEAR(scaled_bounds.getBoundingBox().getSize()[0], 6.0f, 0.5f);
    scaled_root->unref();
}

TEST(Nodes, TransformHierarchiesAccumulateAndRetainBounds)
{
    auto * translated_root = new SoSeparator;
    translated_root->ref();
    auto * first = new SoTransform;
    first->translation.setValue(1.0f, 0.0f, 0.0f);
    auto * second = new SoTransform;
    second->translation.setValue(0.0f, 2.0f, 0.0f);
    translated_root->addChild(first);
    translated_root->addChild(second);
    translated_root->addChild(new SoCube);
    SoGetBoundingBoxAction translated_bounds(SbViewportRegion(512, 512));
    translated_bounds.apply(translated_root);
    ASSERT_FALSE(translated_bounds.getBoundingBox().isEmpty());
    EXPECT_NEAR(translated_bounds.getBoundingBox().getCenter()[0], 1.0f, 0.1f);
    EXPECT_NEAR(translated_bounds.getBoundingBox().getCenter()[1], 2.0f, 0.1f);
    translated_root->unref();

    auto * scaled_root = new SoSeparator;
    scaled_root->ref();
    auto * scaled_rotation = new SoTransform;
    scaled_rotation->rotation.setValue(SbRotation(SbVec3f(0.0f, 0.0f, 1.0f), kPi / 4.0f));
    scaled_rotation->scaleFactor.setValue(2.0f, 2.0f, 2.0f);
    scaled_root->addChild(scaled_rotation);
    scaled_root->addChild(new SoCube);
    SoGetBoundingBoxAction scaled_bounds(SbViewportRegion(512, 512));
    scaled_bounds.apply(scaled_root);
    ASSERT_FALSE(scaled_bounds.getBoundingBox().isEmpty());
    const SbVec3f scaled_size = scaled_bounds.getBoundingBox().getMax() -
                                scaled_bounds.getBoundingBox().getMin();
    EXPECT_GE(scaled_size[0], 3.0f);
    EXPECT_GE(scaled_size[1], 3.0f);
    scaled_root->unref();

    auto * rotation_root = new SoSeparator;
    rotation_root->ref();
    auto * rotation = new SoRotation;
    rotation->rotation.setValue(SbRotation(SbVec3f(0.0f, 1.0f, 0.0f), kPi / 2.0f));
    rotation_root->addChild(rotation);
    rotation_root->addChild(new SoSphere);
    SoGetBoundingBoxAction rotation_bounds(SbViewportRegion(512, 512));
    rotation_bounds.apply(rotation_root);
    EXPECT_FALSE(rotation_bounds.getBoundingBox().isEmpty());
    rotation_root->unref();
}

TEST(Nodes, SwitchAndLodSelectGeometryWithoutEmptyingVisibleBounds)
{
    auto * visible_root = new SoSeparator;
    visible_root->ref();
    auto * visible_switch = new SoSwitch;
    visible_switch->addChild(new SoCube);
    visible_switch->addChild(new SoCone);
    visible_switch->whichChild.setValue(0);
    visible_root->addChild(visible_switch);
    SoGetBoundingBoxAction visible_bounds(SbViewportRegion(512, 512));
    visible_bounds.apply(visible_root);
    EXPECT_FALSE(visible_bounds.getBoundingBox().isEmpty());
    SoGetPrimitiveCountAction visible_primitives(SbViewportRegion(512, 512));
    visible_primitives.apply(visible_root);
    EXPECT_EQ(visible_primitives.getTriangleCount(), 12);
    visible_root->unref();

    auto * all_root = new SoSeparator;
    all_root->ref();
    auto * all_switch = new SoSwitch;
    all_switch->whichChild.setValue(SO_SWITCH_ALL);
    all_switch->addChild(new SoCube);
    all_switch->addChild(new SoSphere);
    all_root->addChild(all_switch);
    SoGetBoundingBoxAction all_bounds(SbViewportRegion(512, 512));
    all_bounds.apply(all_root);
    EXPECT_FALSE(all_bounds.getBoundingBox().isEmpty());
    SoGetPrimitiveCountAction all_primitives(SbViewportRegion(512, 512));
    all_primitives.apply(all_root);
    EXPECT_GT(all_primitives.getTriangleCount(), visible_primitives.getTriangleCount());
    all_root->unref();

    auto * inherited_switch = new SoSwitch;
    inherited_switch->ref();
    inherited_switch->whichChild.setValue(SO_SWITCH_INHERIT);
    inherited_switch->addChild(new SoCube);
    EXPECT_EQ(inherited_switch->whichChild.getValue(), SO_SWITCH_INHERIT);
    inherited_switch->unref();

    auto * hidden_root = new SoSeparator;
    hidden_root->ref();
    auto * hidden_switch = new SoSwitch;
    hidden_switch->addChild(new SoCube);
    hidden_switch->whichChild.setValue(SO_SWITCH_NONE);
    hidden_root->addChild(hidden_switch);
    SoGetBoundingBoxAction hidden_bounds(SbViewportRegion(512, 512));
    hidden_bounds.apply(hidden_root);
    EXPECT_TRUE(hidden_bounds.getBoundingBox().isEmpty());
    hidden_root->unref();

    auto * lod_root = new SoSeparator;
    lod_root->ref();
    auto * camera = new SoPerspectiveCamera;
    camera->position.setValue(0.0f, 0.0f, 10.0f);
    lod_root->addChild(camera);
    auto * lod = new SoLOD;
    lod->range.set1Value(0, 5.0f);
    lod->addChild(new SoCube);
    lod->addChild(new SoCone);
    lod_root->addChild(lod);
    SoGetBoundingBoxAction lod_bounds(SbViewportRegion(512, 512));
    lod_bounds.apply(lod_root);
    EXPECT_FALSE(lod_bounds.getBoundingBox().isEmpty());
    lod_root->unref();
}

TEST(Nodes, InformationalAnnotationAndFontNodesRetainTheirContracts)
{
    auto * info = new SoInfo;
    info->ref();
    info->string.setValue("modern node information");
    EXPECT_EQ(info->string.getValue(), SbString("modern node information"));
    auto * info_root = new SoSeparator;
    info_root->ref();
    info_root->addChild(info);
    SoGetBoundingBoxAction info_bounds(SbViewportRegion(512, 512));
    info_bounds.apply(info_root);
    info_root->unref();
    info->unref();

    auto * annotation = new SoAnnotation;
    annotation->ref();
    annotation->addChild(new SoCube);
    SoGetBoundingBoxAction annotation_bounds(SbViewportRegion(512, 512));
    annotation_bounds.apply(annotation);
    EXPECT_FALSE(annotation_bounds.getBoundingBox().isEmpty());
    annotation->unref();

    auto * font = new SoFont;
    font->ref();
    font->name.setValue("Helvetica");
    font->size.setValue(12.0f);
    EXPECT_EQ(font->name.getValue(), SbName("Helvetica"));
    EXPECT_FLOAT_EQ(font->size.getValue(), 12.0f);
    font->unref();
}

TEST(Nodes, LightNodesRetainTheirFieldsAndDoNotAffectSceneBounds)
{
    auto * directional = new SoDirectionalLight;
    directional->ref();
    directional->direction.setValue(-1.0f, -1.0f, -1.0f);
    directional->color.setValue(0.8f, 0.8f, 0.8f);
    directional->intensity.setValue(0.7f);
    directional->on.setValue(TRUE);
    EXPECT_EQ(directional->direction.getValue(), SbVec3f(-1.0f, -1.0f, -1.0f));
    EXPECT_NEAR(directional->intensity.getValue(), 0.7f, 1.0e-6f);
    EXPECT_TRUE(directional->on.getValue());
    directional->unref();

    auto * point = new SoPointLight;
    point->ref();
    point->location.setValue(0.0f, 5.0f, 0.0f);
    point->color.setValue(1.0f, 1.0f, 0.0f);
    point->intensity.setValue(0.9f);
    EXPECT_EQ(point->location.getValue(), SbVec3f(0.0f, 5.0f, 0.0f));
    EXPECT_NEAR(point->intensity.getValue(), 0.9f, 1.0e-6f);
    point->unref();

    auto * spot = new SoSpotLight;
    spot->ref();
    spot->location.setValue(0.0f, 10.0f, 0.0f);
    spot->direction.setValue(0.0f, -1.0f, 0.0f);
    spot->cutOffAngle.setValue(kPi / 6.0f);
    spot->dropOffRate.setValue(0.5f);
    EXPECT_NEAR(spot->cutOffAngle.getValue(), kPi / 6.0f, 1.0e-6f);
    EXPECT_NEAR(spot->dropOffRate.getValue(), 0.5f, 1.0e-6f);
    spot->unref();

    auto * root = new SoSeparator;
    root->ref();
    auto * searched_directional = new SoDirectionalLight;
    root->addChild(searched_directional);
    root->addChild(new SoPointLight);
    root->addChild(new SoSphere);
    SoSearchAction search;
    search.setType(SoDirectionalLight::getClassTypeId());
    search.apply(root);
    ASSERT_NE(search.getPath(), nullptr);
    EXPECT_EQ(search.getPath()->getTail(), searched_directional);
    SoGetBoundingBoxAction bounds(SbViewportRegion(512, 512));
    bounds.apply(root);
    EXPECT_FALSE(bounds.getBoundingBox().isEmpty());
    root->unref();
}

TEST(Nodes, ShapeHintsConfigureGeometryTraversalWithoutChangingBounds)
{
    auto * hints = new SoShapeHints;
    hints->ref();
    hints->vertexOrdering.setValue(SoShapeHints::COUNTERCLOCKWISE);
    hints->shapeType.setValue(SoShapeHints::SOLID);
    hints->faceType.setValue(SoShapeHints::CONVEX);
    hints->creaseAngle.setValue(0.6f);
    EXPECT_EQ(hints->vertexOrdering.getValue(), SoShapeHints::COUNTERCLOCKWISE);
    EXPECT_EQ(hints->shapeType.getValue(), SoShapeHints::SOLID);
    EXPECT_EQ(hints->faceType.getValue(), SoShapeHints::CONVEX);
    EXPECT_NEAR(hints->creaseAngle.getValue(), 0.6f, 1.0e-6f);
    hints->unref();

    auto * root = new SoSeparator;
    root->ref();
    auto * scene_hints = new SoShapeHints;
    scene_hints->shapeType.setValue(SoShapeHints::UNKNOWN_SHAPE_TYPE);
    scene_hints->vertexOrdering.setValue(SoShapeHints::COUNTERCLOCKWISE);
    root->addChild(scene_hints);

    auto * coordinates = new SoCoordinate3;
    coordinates->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
    coordinates->point.set1Value(1, SbVec3f(1.0f, -1.0f, 0.0f));
    coordinates->point.set1Value(2, SbVec3f(1.0f, 1.0f, 0.0f));
    coordinates->point.set1Value(3, SbVec3f(-1.0f, 1.0f, 0.0f));
    root->addChild(coordinates);

    auto * faces = new SoIndexedFaceSet;
    const int32_t indices[] = {0, 1, 2, 3, -1};
    faces->coordIndex.setValues(0, 5, indices);
    root->addChild(faces);

    SoGetBoundingBoxAction bounds(SbViewportRegion(512, 512));
    bounds.apply(root);
    EXPECT_FALSE(bounds.getBoundingBox().isEmpty());

    int triangle_count = 0;
    SoCallbackAction callback;
    callback.addTriangleCallback(
        SoShape::getClassTypeId(),
        [](void * user_data, SoCallbackAction *, const SoPrimitiveVertex *,
           const SoPrimitiveVertex *, const SoPrimitiveVertex *) {
            ++*static_cast<int *>(user_data);
        },
        &triangle_count);
    callback.apply(root);
    EXPECT_GE(triangle_count, 2);
    root->unref();
}

TEST(Nodes, PrimitiveAndTextShapesProduceUsableBoundingBoxes)
{
    const auto bounds_for = [](SoNode * node) {
        auto * root = new SoSeparator;
        root->ref();
        root->addChild(node);
        SoGetBoundingBoxAction action(SbViewportRegion(512, 512));
        action.apply(root);
        const SbBox3f bounds = action.getBoundingBox();
        root->unref();
        return bounds;
    };
    const auto expect_minimum_size = [&bounds_for](SoNode * node, float minimum_size) {
        const SbBox3f bounds = bounds_for(node);
        EXPECT_FALSE(bounds.isEmpty());
        const SbVec3f size = bounds.getMax() - bounds.getMin();
        EXPECT_GE(size.length(), minimum_size);
    };

    auto * sphere = new SoSphere;
    sphere->radius.setValue(2.0f);
    expect_minimum_size(sphere, 4.0f);
    auto * cone = new SoCone;
    cone->bottomRadius.setValue(2.0f);
    cone->height.setValue(4.0f);
    const SbBox3f cone_bounds = bounds_for(cone);
    ASSERT_FALSE(cone_bounds.isEmpty());
    EXPECT_NEAR(cone_bounds.getSize()[1], 4.0f, 0.1f);
    expect_minimum_size(new SoCylinder, 1.0f);
    auto * cube = new SoCube;
    cube->width.setValue(3.0f);
    expect_minimum_size(cube, 3.0f);

    auto * ascii_root = new SoSeparator;
    ascii_root->ref();
    auto * ascii_font = new SoFont;
    ascii_font->size.setValue(12.0f);
    ascii_root->addChild(ascii_font);
    auto * ascii_text = new SoAsciiText;
    ascii_text->string.set1Value(0, "Obol");
    ascii_root->addChild(ascii_text);
    SoGetBoundingBoxAction ascii_bounds(SbViewportRegion(512, 512));
    ascii_bounds.apply(ascii_root);
    EXPECT_FALSE(ascii_bounds.getBoundingBox().isEmpty());
    ascii_root->unref();

    auto * text_root = new SoSeparator;
    text_root->ref();
    auto * text_font = new SoFont;
    text_font->size.setValue(12.0f);
    text_root->addChild(text_font);
    auto * text = new SoText3;
    text->string.set1Value(0, "Obol");
    text_root->addChild(text);
    SoGetBoundingBoxAction text_bounds(SbViewportRegion(512, 512));
    text_bounds.apply(text_root);
    EXPECT_FALSE(text_bounds.getBoundingBox().isEmpty());
    text_root->unref();

    auto * text2_root = new SoSeparator;
    text2_root->ref();
    auto * text2_font = new SoFont;
    text2_font->size.setValue(12.0f);
    text2_root->addChild(text2_font);
    auto * text2 = new SoText2;
    text2->string.set1Value(0, "Obol 2D");
    text2->spacing.setValue(1.2f);
    text2->justification.setValue(SoText2::CENTER);
    text2_root->addChild(text2);
    SoGetBoundingBoxAction text2_bounds(SbViewportRegion(512, 512));
    text2_bounds.apply(text2_root);
    EXPECT_FALSE(text2_bounds.getBoundingBox().isEmpty());
    EXPECT_FLOAT_EQ(text2->spacing.getValue(), 1.2f);
    EXPECT_EQ(text2->justification.getValue(), SoText2::CENTER);
    text2_root->unref();
}

TEST(Nodes, SelectionRetainsSelectedNodesAndToggleState)
{
    auto * selection = new SoSelection;
    selection->ref();
    auto * cube = new SoCube;
    auto * sphere = new SoSphere;
    selection->addChild(cube);
    selection->addChild(sphere);

    struct SelectionEvents {
        int selected = 0;
        int deselected = 0;
        int changed = 0;
    } events;
    selection->addSelectionCallback(
        [](void * data, SoPath *) { ++static_cast<SelectionEvents *>(data)->selected; },
        &events);
    selection->addDeselectionCallback(
        [](void * data, SoPath *) { ++static_cast<SelectionEvents *>(data)->deselected; },
        &events);
    selection->addChangeCallback(
        [](void * data, SoSelection *) { ++static_cast<SelectionEvents *>(data)->changed; },
        &events);

    selection->select(cube);
    EXPECT_TRUE(selection->isSelected(cube));
    EXPECT_EQ(selection->getNumSelected(), 1);
    EXPECT_EQ(events.selected, 1);

    selection->toggle(sphere);
    EXPECT_TRUE(selection->isSelected(sphere));
    EXPECT_EQ(selection->getNumSelected(), 2);

    selection->deselect(cube);
    EXPECT_FALSE(selection->isSelected(cube));
    EXPECT_EQ(selection->getNumSelected(), 1);
    EXPECT_EQ(events.deselected, 1);

    selection->deselectAll();
    EXPECT_EQ(selection->getNumSelected(), 0);
    EXPECT_FALSE(selection->isSelected(sphere));
    EXPECT_GE(events.changed, 3);

    selection->policy.setValue(SoSelection::TOGGLE);
    EXPECT_EQ(selection->policy.getValue(), SoSelection::TOGGLE);
    selection->setPickMatching(TRUE);
    EXPECT_TRUE(selection->isPickMatching());
    selection->unref();
}

TEST(Nodes, ExtendedSelectionRetainsPathSelectionAndBounds)
{
    auto * selection = new SoExtSelection;
    selection->ref();
    EXPECT_TRUE(selection->isOfType(SoSelection::getClassTypeId()));
    selection->addChild(new SoCube);
    selection->addChild(new SoSphere);
    ASSERT_EQ(selection->getNumChildren(), 2);

    auto * path = new SoPath(selection);
    path->ref();
    path->append(0);
    static_cast<SoSelection *>(selection)->select(path);
    EXPECT_TRUE(selection->isSelected(path));
    selection->deselectAll();
    EXPECT_EQ(selection->getNumSelected(), 0);

    SoGetBoundingBoxAction bounds(SbViewportRegion(512, 512));
    bounds.apply(selection);
    EXPECT_FALSE(bounds.getBoundingBox().isEmpty());
    path->unref();
    selection->unref();
}

TEST(Nodes, PointAndLineShapesProduceBoundsAndPrimitiveCounts)
{
    const auto expect_bounds = [](SoSeparator * root) {
        SoGetBoundingBoxAction bounds(SbViewportRegion(512, 512));
        bounds.apply(root);
        EXPECT_FALSE(bounds.getBoundingBox().isEmpty());
    };

    auto * point_root = new SoSeparator;
    point_root->ref();
    auto * point_coordinates = new SoCoordinate3;
    point_coordinates->point.set1Value(0, SbVec3f(0.0f, 0.0f, 0.0f));
    point_coordinates->point.set1Value(1, SbVec3f(1.0f, 0.0f, 0.0f));
    point_coordinates->point.set1Value(2, SbVec3f(0.0f, 1.0f, 0.0f));
    point_root->addChild(point_coordinates);
    auto * points = new SoPointSet;
    point_root->addChild(points);
    expect_bounds(point_root);
    SoGetPrimitiveCountAction point_count;
    point_count.apply(point_root);
    EXPECT_EQ(point_count.getPointCount(), 3);
    point_root->unref();

    auto * line_root = new SoSeparator;
    line_root->ref();
    auto * line_coordinates = new SoCoordinate3;
    line_coordinates->point.set1Value(0, SbVec3f(-1.0f, 0.0f, 0.0f));
    line_coordinates->point.set1Value(1, SbVec3f(1.0f, 0.0f, 0.0f));
    line_root->addChild(line_coordinates);
    auto * lines = new SoLineSet;
    lines->numVertices.set1Value(0, 2);
    line_root->addChild(lines);
    expect_bounds(line_root);
    SoGetPrimitiveCountAction line_count;
    line_count.apply(line_root);
    EXPECT_EQ(line_count.getLineCount(), 1);
    line_root->unref();

    auto * indexed_root = new SoSeparator;
    indexed_root->ref();
    auto * indexed_coordinates = new SoCoordinate3;
    indexed_coordinates->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
    indexed_coordinates->point.set1Value(1, SbVec3f(1.0f, -1.0f, 0.0f));
    indexed_coordinates->point.set1Value(2, SbVec3f(0.0f, 1.0f, 0.0f));
    indexed_root->addChild(indexed_coordinates);
    auto * indexed_lines = new SoIndexedLineSet;
    const int32_t indices[] = {0, 1, 2, -1};
    indexed_lines->coordIndex.setValues(0, 4, indices);
    indexed_root->addChild(indexed_lines);
    expect_bounds(indexed_root);
    indexed_root->unref();
}

TEST(Nodes, FaceSetShapesProduceBoundsAndTriangleCounts)
{
    auto * root = new SoSeparator;
    root->ref();
    auto * coordinates = new SoCoordinate3;
    coordinates->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
    coordinates->point.set1Value(1, SbVec3f(1.0f, -1.0f, 0.0f));
    coordinates->point.set1Value(2, SbVec3f(1.0f, 1.0f, 0.0f));
    coordinates->point.set1Value(3, SbVec3f(-1.0f, 1.0f, 0.0f));
    root->addChild(coordinates);

    auto * triangle = new SoFaceSet;
    triangle->numVertices.set1Value(0, 3);
    root->addChild(triangle);
    auto * indexed = new SoIndexedFaceSet;
    const int32_t indices[] = {0, 2, 3, -1};
    indexed->coordIndex.setValues(0, 4, indices);
    root->addChild(indexed);

    SoGetBoundingBoxAction bounds(SbViewportRegion(512, 512));
    bounds.apply(root);
    EXPECT_FALSE(bounds.getBoundingBox().isEmpty());
    SoGetPrimitiveCountAction primitive_count;
    primitive_count.apply(root);
    EXPECT_GE(primitive_count.getTriangleCount(), 2);
    root->unref();
}

TEST(Nodes, SeparatorsRetainCacheAndCullingConfigurationWhileScopingTransforms)
{
    auto * separator = new SoSeparator;
    separator->ref();
    separator->renderCaching.setValue(SoSeparator::OFF);
    EXPECT_EQ(separator->renderCaching.getValue(), SoSeparator::OFF);
    separator->renderCaching.setValue(SoSeparator::ON);
    EXPECT_EQ(separator->renderCaching.getValue(), SoSeparator::ON);
    separator->renderCaching.setValue(SoSeparator::AUTO);
    EXPECT_EQ(separator->renderCaching.getValue(), SoSeparator::AUTO);
    separator->boundingBoxCaching.setValue(SoSeparator::OFF);
    separator->pickCulling.setValue(SoSeparator::OFF);
    separator->renderCulling.setValue(SoSeparator::OFF);
    EXPECT_EQ(separator->boundingBoxCaching.getValue(), SoSeparator::OFF);
    EXPECT_EQ(separator->pickCulling.getValue(), SoSeparator::OFF);
    EXPECT_EQ(separator->renderCulling.getValue(), SoSeparator::OFF);
    auto * cube = new SoCube;
    cube->width.setValue(2.0f);
    separator->addChild(cube);
    separator->addChild(new SoSphere);
    SoGetBoundingBoxAction initial_bounds(SbViewportRegion(512, 512));
    initial_bounds.apply(separator);
    ASSERT_FALSE(initial_bounds.getBoundingBox().isEmpty());
    cube->width.setValue(5.0f);
    SoGetBoundingBoxAction updated_bounds(SbViewportRegion(512, 512));
    updated_bounds.apply(separator);
    EXPECT_GT(updated_bounds.getBoundingBox().getMax()[0],
              initial_bounds.getBoundingBox().getMax()[0]);

    SoRayPickAction pick(SbViewportRegion(512, 512));
    pick.setRay(SbVec3f(0.0f, 0.0f, 5.0f), SbVec3f(0.0f, 0.0f, -1.0f));
    pick.apply(separator);
    EXPECT_NE(pick.getPickedPoint(), nullptr);
    auto * replacement = new SoCylinder;
    separator->replaceChild(cube, replacement);
    EXPECT_EQ(separator->getChild(0), replacement);
    separator->unref();

    auto * outer = new SoSeparator;
    outer->ref();
    outer->renderCulling.setValue(SoSeparator::OFF);
    auto * inner = new SoSeparator;
    inner->renderCulling.setValue(SoSeparator::ON);
    auto * translation = new SoTransform;
    translation->translation.setValue(5.0f, 0.0f, 0.0f);
    inner->addChild(translation);
    inner->addChild(new SoCube);
    outer->addChild(inner);
    outer->addChild(new SoSphere);
    SoGetBoundingBoxAction nested_bounds(SbViewportRegion(512, 512));
    nested_bounds.apply(outer);
    ASSERT_FALSE(nested_bounds.getBoundingBox().isEmpty());
    EXPECT_GE(nested_bounds.getBoundingBox().getMax()[0], 4.0f);
    outer->unref();
}

TEST(Nodes, ComplexityModesRetainBoundsAndPrimitiveTraversal)
{
    auto * bounding_root = new SoSeparator;
    bounding_root->ref();
    auto * bounding_complexity = new SoComplexity;
    bounding_complexity->type.setValue(SoComplexity::BOUNDING_BOX);
    bounding_root->addChild(bounding_complexity);
    bounding_root->addChild(new SoSphere);
    SoGetBoundingBoxAction bounding_bounds(SbViewportRegion(512, 512));
    bounding_bounds.apply(bounding_root);
    EXPECT_FALSE(bounding_bounds.getBoundingBox().isEmpty());
    bounding_root->unref();

    auto * screen_root = new SoSeparator;
    screen_root->ref();
    auto * screen_complexity = new SoComplexity;
    screen_complexity->type.setValue(SoComplexity::SCREEN_SPACE);
    screen_complexity->value.setValue(0.5f);
    screen_root->addChild(screen_complexity);
    screen_root->addChild(new SoCylinder);
    int triangle_count = 0;
    SoCallbackAction callback;
    callback.addTriangleCallback(
        SoShape::getClassTypeId(),
        [](void * user_data, SoCallbackAction *, const SoPrimitiveVertex *,
           const SoPrimitiveVertex *, const SoPrimitiveVertex *) {
            ++*static_cast<int *>(user_data);
        },
        &triangle_count);
    callback.apply(screen_root);
    EXPECT_GE(triangle_count, 4);
    screen_root->unref();
}
