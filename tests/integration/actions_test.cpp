#include <gtest/gtest.h>

#include <Inventor/SbBox3f.h>
#include <Inventor/SbColor.h>
#include <Inventor/SbLine.h>
#include <Inventor/SbName.h>
#include <Inventor/SbVec2f.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbXfBox3f.h>
#include <Inventor/SoType.h>
#include <Inventor/SoSceneManager.h>
#include <Inventor/SoPath.h>
#include <Inventor/SoPickedPoint.h>
#include <Inventor/SoPrimitiveVertex.h>
#include <Inventor/actions/SoAction.h>
#include <Inventor/actions/SoCallbackAction.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/actions/SoGetMatrixAction.h>
#include <Inventor/actions/SoGetPrimitiveCountAction.h>
#include <Inventor/actions/SoHandleEventAction.h>
#include <Inventor/actions/SoRayPickAction.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/actions/SoSceneRenderAction.h>
#include <Inventor/elements/SoDecimationTypeElement.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoFaceSet.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <Inventor/nodes/SoIndexedLineSet.h>
#include <Inventor/nodes/SoLineSet.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoMaterialBinding.h>
#include <Inventor/nodes/SoNormal.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoPointSet.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoShape.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoSwitch.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/lists/SoNodeList.h>
#include <Inventor/nodes/SoVertexProperty.h>
#include <Inventor/events/SoMouseButtonEvent.h>
#include <Inventor/events/SoKeyboardEvent.h>

#include <cmath>

namespace {

struct CallbackCounts {
    int pre = 0;
    int post = 0;
    int triangles = 0;
    int lines = 0;
    int points = 0;
    bool saw_translation = false;
};

SoCallbackAction::Response countPre(void * user_data, SoCallbackAction *, const SoNode *)
{
    ++static_cast<CallbackCounts *>(user_data)->pre;
    return SoCallbackAction::CONTINUE;
}

SoCallbackAction::Response countPost(void * user_data, SoCallbackAction *, const SoNode *)
{
    ++static_cast<CallbackCounts *>(user_data)->post;
    return SoCallbackAction::CONTINUE;
}

void countTriangle(void * user_data, SoCallbackAction *, const SoPrimitiveVertex *,
                   const SoPrimitiveVertex *, const SoPrimitiveVertex *)
{
    ++static_cast<CallbackCounts *>(user_data)->triangles;
}

void countLine(void * user_data, SoCallbackAction *, const SoPrimitiveVertex *,
               const SoPrimitiveVertex *)
{
    ++static_cast<CallbackCounts *>(user_data)->lines;
}

void countPoint(void * user_data, SoCallbackAction *, const SoPrimitiveVertex *)
{
    ++static_cast<CallbackCounts *>(user_data)->points;
}

void countTranslatedTriangle(void * user_data, SoCallbackAction * action,
                             const SoPrimitiveVertex *, const SoPrimitiveVertex *,
                             const SoPrimitiveVertex *)
{
    auto & counts = *static_cast<CallbackCounts *>(user_data);
    ++counts.triangles;
    SbVec3f translated_origin;
    action->getModelMatrix().multVecMatrix(SbVec3f(0.0f, 0.0f, 0.0f), translated_origin);
    counts.saw_translation = std::fabs(translated_origin[0] - 1.0f) < 1.0e-6f;
}

} // namespace

TEST(Actions, RegisteredTypesTraverseAndFindNamedNodes)
{
    SoSearchAction search;
    SoGetBoundingBoxAction bounds(SbViewportRegion(100, 100));
    SoCallbackAction callback;
    EXPECT_NE(search.getTypeId(), SoType::badType());
    EXPECT_TRUE(search.isOfType(SoAction::getClassTypeId()));
    EXPECT_NE(bounds.getTypeId(), SoType::badType());
    EXPECT_NE(callback.getTypeId(), SoType::badType());

    auto * root = new SoSeparator;
    root->ref();
    auto * cube = new SoCube;
    cube->setName("modern-action-target");
    root->addChild(cube);
    search.setName(SbName("modern-action-target"));
    search.setFind(SoSearchAction::NAME);
    search.apply(root);
    ASSERT_NE(search.getPath(), nullptr);
    EXPECT_EQ(search.getPath()->getTail(), cube);
    root->unref();
}

TEST(Actions, SearchFindsTypesNamesDerivedTypesAndAllMatches)
{
    auto * root = new SoSeparator;
    root->ref();
    auto * first_cube = new SoCube;
    first_cube->setName("modern-search-cube");
    root->addChild(first_cube);
    root->addChild(new SoCube);
    root->addChild(new SoCone);
    root->addChild(new SoSphere);
    auto * last_sphere = new SoSphere;
    root->addChild(last_sphere);
    root->addChild(new SoMaterial);
    root->addChild(new SoMaterial);

    SoSearchAction first;
    first.setType(SoCube::getClassTypeId());
    first.setFind(SoSearchAction::TYPE);
    first.setInterest(SoSearchAction::FIRST);
    first.apply(root);
    ASSERT_NE(first.getPath(), nullptr);
    EXPECT_EQ(first.getPath()->getTail(), first_cube);

    SoSearchAction by_name;
    by_name.setName("modern-search-cube");
    by_name.setFind(SoSearchAction::NAME);
    by_name.apply(root);
    ASSERT_NE(by_name.getPath(), nullptr);
    EXPECT_EQ(by_name.getPath()->getTail(), first_cube);

    SoSearchAction all_cubes;
    all_cubes.setType(SoCube::getClassTypeId());
    all_cubes.setFind(SoSearchAction::TYPE);
    all_cubes.setInterest(SoSearchAction::ALL);
    EXPECT_EQ(all_cubes.getInterest(), SoSearchAction::ALL);
    all_cubes.apply(root);
    EXPECT_EQ(all_cubes.getPaths().getLength(), 2);

    SoSearchAction all_shapes;
    all_shapes.setType(SoShape::getClassTypeId(), TRUE);
    all_shapes.setFind(SoSearchAction::TYPE);
    all_shapes.setInterest(SoSearchAction::ALL);
    all_shapes.apply(root);
    EXPECT_EQ(all_shapes.getPaths().getLength(), 5);

    SoSearchAction last;
    last.setType(SoSphere::getClassTypeId());
    last.setFind(SoSearchAction::TYPE);
    last.setInterest(SoSearchAction::LAST);
    last.apply(root);
    ASSERT_NE(last.getPath(), nullptr);
    EXPECT_EQ(last.getPath()->getTail(), last_sphere);

    SoSearchAction all_materials;
    all_materials.setType(SoMaterial::getClassTypeId());
    all_materials.setFind(SoSearchAction::TYPE);
    all_materials.setInterest(SoSearchAction::ALL);
    all_materials.apply(root);
    EXPECT_EQ(all_materials.getPaths().getLength(), 2);

    SoSearchAction by_node;
    by_node.setNode(first_cube);
    by_node.apply(root);
    ASSERT_NE(by_node.getPath(), nullptr);
    EXPECT_EQ(by_node.getPath()->getTail(), first_cube);

    auto * inactive_switch = new SoSwitch;
    inactive_switch->whichChild.setValue(SO_SWITCH_NONE);
    auto * hidden_cube = new SoCube;
    hidden_cube->setName("modern-hidden-search-cube");
    inactive_switch->addChild(hidden_cube);
    root->addChild(inactive_switch);
    SoSearchAction include_inactive;
    include_inactive.setName("modern-hidden-search-cube");
    include_inactive.setFind(SoSearchAction::NAME);
    include_inactive.setSearchingAll(TRUE);
    EXPECT_TRUE(include_inactive.isSearchingAll());
    include_inactive.apply(root);
    ASSERT_NE(include_inactive.getPath(), nullptr);
    EXPECT_EQ(include_inactive.getPath()->getTail(), hidden_cube);
    root->unref();
}

TEST(Actions, BoundingBoxesTrackGeometryTransformsAndCenterState)
{
    auto * root = new SoSeparator;
    root->ref();
    auto * translation = new SoTranslation;
    translation->translation.setValue(10.0f, 0.0f, 0.0f);
    root->addChild(translation);
    auto * cube = new SoCube;
    cube->width = 2.0f;
    cube->height = 2.0f;
    cube->depth = 2.0f;
    root->addChild(cube);

    SoGetBoundingBoxAction bounds(SbViewportRegion(512, 512));
    bounds.apply(root);
    ASSERT_FALSE(bounds.getBoundingBox().isEmpty());
    EXPECT_NEAR(bounds.getBoundingBox().getCenter()[0], 10.0f, 0.1f);
    EXPECT_TRUE(bounds.isCenterSet());

    SoGetBoundingBoxAction camera_space(SbViewportRegion(512, 512));
    camera_space.setInCameraSpace(TRUE);
    EXPECT_TRUE(camera_space.isInCameraSpace());
    root->unref();

    auto * reset_root = new SoSeparator;
    reset_root->ref();
    auto * camera = new SoPerspectiveCamera;
    camera->position.setValue(0.0f, 0.0f, 10.0f);
    reset_root->addChild(camera);
    reset_root->addChild(new SoCube);
    SoGetBoundingBoxAction reset_bounds(SbViewportRegion(512, 512));
    reset_bounds.setResetPath(nullptr, TRUE);
    reset_bounds.apply(reset_root);
    EXPECT_FALSE(reset_bounds.getBoundingBox().isEmpty());
    reset_root->unref();

    auto * multi_shape_root = new SoSeparator;
    multi_shape_root->ref();
    auto * origin_cube = new SoCube;
    origin_cube->width.setValue(2.0f);
    multi_shape_root->addChild(origin_cube);
    auto * distant_translation = new SoTransform;
    distant_translation->translation.setValue(10.0f, 0.0f, 0.0f);
    multi_shape_root->addChild(distant_translation);
    multi_shape_root->addChild(new SoSphere);
    SoGetBoundingBoxAction multi_shape_bounds(SbViewportRegion(512, 512));
    multi_shape_bounds.apply(multi_shape_root);
    EXPECT_GE(multi_shape_bounds.getBoundingBox().getMax()[0], 9.0f);
    const SbBox3f projected_bounds = multi_shape_bounds.getXfBoundingBox().project();
    EXPECT_FALSE(projected_bounds.isEmpty());
    multi_shape_root->unref();
}

TEST(Actions, CallbackTraversalReportsNodesAndShapePrimitives)
{
    auto * root = new SoSeparator;
    root->ref();
    root->addChild(new SoCube);
    root->addChild(new SoSphere);

    CallbackCounts counts;
    SoCallbackAction callback;
    callback.addPreCallback(SoNode::getClassTypeId(), countPre, &counts);
    callback.addPostCallback(SoNode::getClassTypeId(), countPost, &counts);
    callback.addTriangleCallback(SoShape::getClassTypeId(), countTriangle, &counts);
    callback.apply(root);

    EXPECT_GT(counts.pre, 0);
    EXPECT_GT(counts.post, 0);
    EXPECT_GT(counts.triangles, 0);

    int sphere_pre_count = 0;
    SoCallbackAction sphere_callback;
    sphere_callback.addPreCallback(
        SoSphere::getClassTypeId(),
        [](void * user_data, SoCallbackAction *, const SoNode *) {
            ++*static_cast<int *>(user_data);
            return SoCallbackAction::CONTINUE;
        },
        &sphere_pre_count);
    sphere_callback.apply(root);
    EXPECT_EQ(sphere_pre_count, 1);
    root->unref();
}

TEST(SceneRenderAction, CollectsPrimitivesLightsAndModelTransforms)
{
    auto * root = new SoSeparator;
    root->ref();
    root->addChild(new SoDirectionalLight);
    auto * translation = new SoTranslation;
    translation->translation.setValue(5.0f, 0.0f, 0.0f);
    root->addChild(translation);
    root->addChild(new SoCube);

    struct Collection {
        int triangles = 0;
        bool saw_translated_vertex = false;
    } collection;

    SoSceneRenderAction action(SbViewportRegion(320, 240));
    action.addTriangleCallback(
        SoShape::getClassTypeId(),
        [](void * data, SoCallbackAction * callback,
           const SoPrimitiveVertex * first, const SoPrimitiveVertex *,
           const SoPrimitiveVertex *) {
            auto & result = *static_cast<Collection *>(data);
            ++result.triangles;
            SbVec3f transformed;
            callback->getModelMatrix().multVecMatrix(first->getPoint(), transformed);
            if (transformed[0] > 3.0f) result.saw_translated_vertex = true;
        },
        &collection);
    action.apply(root);

    EXPECT_EQ(action.getViewportRegion().getWindowSize(), SbVec2s(320, 240));
    EXPECT_EQ(action.getLights().getLength(), 1);
    EXPECT_EQ(collection.triangles, 12);
    EXPECT_TRUE(collection.saw_translated_vertex);
    root->unref();
}

TEST(Actions, TraversalReportsAppliedTargetsStateAndCompletion)
{
    struct AppliedTarget {
        SoAction::AppliedCode code = SoAction::PATH;
        SoNode * node = nullptr;
        SoPath * path = nullptr;
        bool saw_state = false;
    } node_target;

    auto * root = new SoSeparator;
    root->ref();
    auto * cube = new SoCube;
    root->addChild(cube);

    SoCallbackAction node_action;
    node_action.addPreCallback(
        SoCube::getClassTypeId(),
        [](void * user_data, SoCallbackAction * action, const SoNode *) {
            auto & target = *static_cast<AppliedTarget *>(user_data);
            target.code = action->getWhatAppliedTo();
            target.node = action->getNodeAppliedTo();
            target.path = action->getPathAppliedTo();
            target.saw_state = action->getState() != nullptr;
            return SoCallbackAction::CONTINUE;
        },
        &node_target);
    node_action.apply(root);
    EXPECT_EQ(node_target.code, SoAction::NODE);
    EXPECT_EQ(node_target.node, root);
    EXPECT_EQ(node_target.path, nullptr);
    EXPECT_TRUE(node_target.saw_state);

    SoGetBoundingBoxAction bounds(SbViewportRegion(512, 512));
    bounds.apply(root);
    EXPECT_FALSE(bounds.hasTerminated());

    SoSearchAction search;
    search.setNode(cube);
    search.apply(root);
    ASSERT_NE(search.getPath(), nullptr);
    SoPath * cube_path = search.getPath();
    cube_path->ref();

    AppliedTarget path_target;
    SoCallbackAction path_action;
    path_action.addPreCallback(
        SoCube::getClassTypeId(),
        [](void * user_data, SoCallbackAction * action, const SoNode *) {
            auto & target = *static_cast<AppliedTarget *>(user_data);
            target.code = action->getWhatAppliedTo();
            target.path = action->getPathAppliedTo();
            return SoCallbackAction::CONTINUE;
        },
        &path_target);
    path_action.apply(cube_path);
    EXPECT_EQ(path_target.code, SoAction::PATH);
    EXPECT_EQ(path_target.path, cube_path);
    cube_path->unref();
    root->unref();
}

TEST(Actions, CallbackTraversalCoversIndexedLinesPointsAndModelState)
{
    auto * root = new SoSeparator;
    root->ref();

    auto * translation = new SoTranslation;
    translation->translation.setValue(1.0f, 0.0f, 0.0f);
    root->addChild(translation);
    root->addChild(new SoCone);
    root->addChild(new SoCylinder);
    root->addChild(new SoCube);

    auto * coordinates = new SoCoordinate3;
    coordinates->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
    coordinates->point.set1Value(1, SbVec3f(1.0f, -1.0f, 0.0f));
    coordinates->point.set1Value(2, SbVec3f(1.0f, 1.0f, 0.0f));
    coordinates->point.set1Value(3, SbVec3f(-1.0f, 1.0f, 0.0f));
    root->addChild(coordinates);
    auto * faces = new SoIndexedFaceSet;
    const int32_t face_indices[] = {0, 1, 2, 3, -1};
    faces->coordIndex.setValues(0, 5, face_indices);
    root->addChild(faces);
    auto * lines = new SoIndexedLineSet;
    const int32_t line_indices[] = {0, 1, 2, 3, -1};
    lines->coordIndex.setValues(0, 5, line_indices);
    root->addChild(lines);
    auto * points = new SoPointSet;
    points->numPoints.setValue(4);
    root->addChild(points);

    CallbackCounts counts;
    SoCallbackAction callback;
    callback.addTriangleCallback(SoShape::getClassTypeId(), countTranslatedTriangle, &counts);
    callback.addLineSegmentCallback(SoShape::getClassTypeId(), countLine, &counts);
    callback.addPointCallback(SoShape::getClassTypeId(), countPoint, &counts);
    callback.apply(root);

    EXPECT_GE(counts.triangles, 2);
    EXPECT_GE(counts.lines, 3);
    EXPECT_EQ(counts.points, 4);
    EXPECT_TRUE(counts.saw_translation);
    root->unref();
}

TEST(Actions, CallbackTraversalPreservesShapeAndVertexBindingContracts)
{
    const auto count_triangles = [](SoNode * shape) {
        auto * root = new SoSeparator;
        root->ref();
        root->addChild(shape);
        CallbackCounts counts;
        SoCallbackAction callback;
        callback.addTriangleCallback(SoShape::getClassTypeId(), countTriangle, &counts);
        callback.apply(root);
        root->unref();
        return counts.triangles;
    };
    EXPECT_GE(count_triangles(new SoSphere), 10);
    EXPECT_GE(count_triangles(new SoCone), 10);
    EXPECT_GE(count_triangles(new SoCylinder), 10);
    EXPECT_EQ(count_triangles(new SoCube), 12);

    auto * indexed_root = new SoSeparator;
    indexed_root->ref();
    auto * indexed_coordinates = new SoCoordinate3;
    indexed_coordinates->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
    indexed_coordinates->point.set1Value(1, SbVec3f(1.0f, -1.0f, 0.0f));
    indexed_coordinates->point.set1Value(2, SbVec3f(1.0f, 1.0f, 0.0f));
    indexed_coordinates->point.set1Value(3, SbVec3f(-1.0f, 1.0f, 0.0f));
    indexed_root->addChild(indexed_coordinates);
    auto * normals = new SoNormal;
    for (int index = 0; index < 4; ++index) {
        normals->vector.set1Value(index, SbVec3f(0.0f, 0.0f, 1.0f));
    }
    indexed_root->addChild(normals);
    auto * normal_binding = new SoNormalBinding;
    normal_binding->value.setValue(SoNormalBinding::PER_VERTEX_INDEXED);
    indexed_root->addChild(normal_binding);
    auto * indexed_materials = new SoMaterial;
    indexed_materials->diffuseColor.set1Value(0, SbColor(1.0f, 0.0f, 0.0f));
    indexed_materials->diffuseColor.set1Value(1, SbColor(0.0f, 1.0f, 0.0f));
    indexed_root->addChild(indexed_materials);
    auto * indexed_material_binding = new SoMaterialBinding;
    indexed_material_binding->value.setValue(SoMaterialBinding::PER_VERTEX_INDEXED);
    indexed_root->addChild(indexed_material_binding);
    auto * indexed_faces = new SoIndexedFaceSet;
    const int32_t triangle_indices[] = {0, 1, 2, -1, 0, 2, 3, -1};
    indexed_faces->coordIndex.setValues(0, 8, triangle_indices);
    indexed_faces->normalIndex.setValues(0, 8, triangle_indices);
    indexed_faces->materialIndex.setValues(0, 8, triangle_indices);
    indexed_root->addChild(indexed_faces);
    CallbackCounts indexed_counts;
    SoCallbackAction indexed_callback;
    indexed_callback.addTriangleCallback(SoShape::getClassTypeId(), countTriangle, &indexed_counts);
    indexed_callback.apply(indexed_root);
    EXPECT_EQ(indexed_counts.triangles, 2);
    normal_binding->value.setValue(SoNormalBinding::PER_FACE_INDEXED);
    const int32_t face_normal_indices[] = {0, -1, 1, -1};
    indexed_faces->normalIndex.setValues(0, 4, face_normal_indices);
    indexed_callback.apply(indexed_root);
    EXPECT_EQ(indexed_counts.triangles, 4);
    indexed_root->unref();

    auto * face_root = new SoSeparator;
    face_root->ref();
    auto * face_coordinates = new SoCoordinate3;
    face_coordinates->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
    face_coordinates->point.set1Value(1, SbVec3f(1.0f, -1.0f, 0.0f));
    face_coordinates->point.set1Value(2, SbVec3f(0.0f, 1.0f, 0.0f));
    face_coordinates->point.set1Value(3, SbVec3f(-2.0f, -1.0f, 0.0f));
    face_coordinates->point.set1Value(4, SbVec3f(0.0f, -1.0f, 0.0f));
    face_coordinates->point.set1Value(5, SbVec3f(-1.0f, 1.0f, 0.0f));
    face_root->addChild(face_coordinates);
    auto * materials = new SoMaterial;
    materials->diffuseColor.set1Value(0, SbColor(1.0f, 0.0f, 0.0f));
    materials->diffuseColor.set1Value(1, SbColor(0.0f, 1.0f, 0.0f));
    face_root->addChild(materials);
    auto * material_binding = new SoMaterialBinding;
    material_binding->value.setValue(SoMaterialBinding::PER_FACE);
    face_root->addChild(material_binding);
    auto * face_set = new SoFaceSet;
    face_set->numVertices.set1Value(0, 3);
    face_set->numVertices.set1Value(1, 3);
    face_root->addChild(face_set);
    SoGetBoundingBoxAction face_bounds(SbViewportRegion(512, 512));
    face_bounds.apply(face_root);
    EXPECT_FALSE(face_bounds.getBoundingBox().isEmpty());
    material_binding->value.setValue(SoMaterialBinding::PER_VERTEX);
    face_bounds.apply(face_root);
    EXPECT_FALSE(face_bounds.getBoundingBox().isEmpty());
    face_root->unref();

    auto * property_root = new SoSeparator;
    property_root->ref();
    auto * vertex_property = new SoVertexProperty;
    vertex_property->vertex.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
    vertex_property->vertex.set1Value(1, SbVec3f(1.0f, -1.0f, 0.0f));
    vertex_property->vertex.set1Value(2, SbVec3f(0.0f, 1.0f, 0.0f));
    for (int index = 0; index < 3; ++index) {
        vertex_property->normal.set1Value(index, SbVec3f(0.0f, 0.0f, 1.0f));
    }
    vertex_property->normalBinding.setValue(SoVertexProperty::PER_VERTEX);
    auto * property_faces = new SoIndexedFaceSet;
    const int32_t property_indices[] = {0, 1, 2, -1};
    property_faces->coordIndex.setValues(0, 4, property_indices);
    property_faces->vertexProperty.setValue(vertex_property);
    property_root->addChild(property_faces);
    CallbackCounts property_counts;
    SoCallbackAction property_callback;
    property_callback.addTriangleCallback(SoShape::getClassTypeId(), countTriangle, &property_counts);
    property_callback.apply(property_root);
    EXPECT_EQ(property_counts.triangles, 1);
    property_root->unref();
}

TEST(Actions, PrimitiveCountReportsTriangleAndLineGeometry)
{
    auto * solids = new SoSeparator;
    solids->ref();
    solids->addChild(new SoCube);
    solids->addChild(new SoSphere);
    solids->addChild(new SoCone);
    solids->addChild(new SoCylinder);
    SoGetPrimitiveCountAction triangle_count;
    triangle_count.apply(solids);
    EXPECT_GT(triangle_count.getTriangleCount(), 0);
    triangle_count.setDecimationValue(SoDecimationTypeElement::AUTOMATIC, 0.5f);
    triangle_count.apply(solids);
    EXPECT_GT(triangle_count.getTriangleCount(), 0);
    solids->unref();

    auto * line_scene = new SoSeparator;
    line_scene->ref();
    auto * coordinates = new SoCoordinate3;
    coordinates->point.set1Value(0, SbVec3f(0, 0, 0));
    coordinates->point.set1Value(1, SbVec3f(1, 0, 0));
    line_scene->addChild(coordinates);
    auto * lines = new SoLineSet;
    lines->numVertices.set1Value(0, 2);
    line_scene->addChild(lines);
    SoGetPrimitiveCountAction line_count;
    line_count.apply(line_scene);
    EXPECT_GT(line_count.getLineCount(), 0);
    line_scene->unref();

    auto * point_scene = new SoSeparator;
    point_scene->ref();
    auto * point_coordinates = new SoCoordinate3;
    for (int index = 0; index < 5; ++index) {
        point_coordinates->point.set1Value(index, SbVec3f(static_cast<float>(index), 0.0f, 0.0f));
    }
    point_scene->addChild(point_coordinates);
    point_scene->addChild(new SoPointSet);
    SoGetPrimitiveCountAction point_count;
    point_count.apply(point_scene);
    EXPECT_GT(point_count.getPointCount(), 0);
    point_scene->unref();

    auto * grid_scene = new SoSeparator;
    grid_scene->ref();
    auto * grid_coordinates = new SoCoordinate3;
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 5; ++x) {
            grid_coordinates->point.set1Value(y * 5 + x, SbVec3f(float(x), float(y), 0.0f));
        }
    }
    grid_scene->addChild(grid_coordinates);
    auto * grid_faces = new SoIndexedFaceSet;
    int index = 0;
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            grid_faces->coordIndex.set1Value(index++, y * 5 + x);
            grid_faces->coordIndex.set1Value(index++, y * 5 + x + 1);
            grid_faces->coordIndex.set1Value(index++, (y + 1) * 5 + x + 1);
            grid_faces->coordIndex.set1Value(index++, (y + 1) * 5 + x);
            grid_faces->coordIndex.set1Value(index++, -1);
        }
    }
    grid_scene->addChild(grid_faces);
    SoGetPrimitiveCountAction grid_count;
    grid_count.apply(grid_scene);
    EXPECT_GE(grid_count.getTriangleCount(), 32);
    grid_scene->unref();
}

TEST(SceneManager, RetainsSceneViewportAndRenderConfiguration)
{
    SoSceneManager manager;
    auto * root = new SoSeparator;
    root->ref();
    manager.setSceneGraph(root);
    EXPECT_EQ(manager.getSceneGraph(), root);

    manager.setViewportRegion(SbViewportRegion(800, 600));
    EXPECT_EQ(manager.getViewportRegion().getWindowSize()[0], 800);
    EXPECT_EQ(manager.getViewportRegion().getWindowSize()[1], 600);

    manager.setWindowSize(SbVec2s(800, 600));
    EXPECT_EQ(manager.getWindowSize(), SbVec2s(800, 600));
    manager.setSize(SbVec2s(640, 480));
    EXPECT_EQ(manager.getSize(), SbVec2s(640, 480));
    manager.setOrigin(SbVec2s(10, 20));
    EXPECT_EQ(manager.getOrigin(), SbVec2s(10, 20));

    manager.setBackgroundColor(SbColor(0.5f, 0.25f, 0.75f));
    const SbColor background = manager.getBackgroundColor();
    EXPECT_FLOAT_EQ(background[0], 0.5f);
    EXPECT_FLOAT_EQ(background[1], 0.25f);
    EXPECT_FLOAT_EQ(background[2], 0.75f);
    EXPECT_TRUE(manager.isRGBMode());

    auto * camera = new SoPerspectiveCamera;
    camera->ref();
    manager.setCamera(camera);
    EXPECT_EQ(manager.getCamera(), camera);
    camera->unref();

    manager.setSceneGraph(nullptr);
    root->unref();
}

TEST(RayPickAction, StoresConfigurationAndFindsGeometryOrMissesEmptyScenes)
{
    const SbViewportRegion viewport(512, 512);
    SoRayPickAction picker(viewport);
    EXPECT_EQ(picker.getViewportRegion().getWindowSize()[0], 512);
    picker.setRadius(5.0f);
    EXPECT_FLOAT_EQ(picker.getRadius(), 5.0f);

    auto * sphere_scene = new SoSeparator;
    sphere_scene->ref();
    sphere_scene->addChild(new SoSphere);
    picker.setRay(SbVec3f(0, 0, 10), SbVec3f(0, 0, -1));
    picker.apply(sphere_scene);
    ASSERT_GT(picker.getPickedPointList().getLength(), 0);
    sphere_scene->unref();

    auto * empty_scene = new SoSeparator;
    empty_scene->ref();
    picker.apply(empty_scene);
    EXPECT_EQ(picker.getPickedPointList().getLength(), 0);
    empty_scene->unref();
}

TEST(RayPickAction, ScreenCoordinatesAndNormalizedCoordinatesPickCameraScenes)
{
    auto * root = new SoSeparator;
    root->ref();
    auto * camera = new SoPerspectiveCamera;
    camera->position.setValue(0.0f, 0.0f, 10.0f);
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);
    auto * sphere = new SoSphere;
    sphere->radius.setValue(2.0f);
    root->addChild(sphere);

    const SbViewportRegion viewport(512, 512);
    SoRayPickAction screen_picker(viewport);
    screen_picker.setPoint(SbVec2s(256, 256));
    screen_picker.apply(root);
    ASSERT_NE(screen_picker.getPickedPoint(), nullptr);

    SoRayPickAction normalized_picker(viewport);
    normalized_picker.setNormalizedPoint(SbVec2f(0.5f, 0.5f));
    normalized_picker.setPickAll(TRUE);
    normalized_picker.apply(root);
    EXPECT_GT(normalized_picker.getPickedPointList().getLength(), 0);
    root->unref();
}

TEST(RayPickAction, IntersectsIndexedFaceGeometry)
{
    auto * root = new SoSeparator;
    root->ref();
    auto * coordinates = new SoCoordinate3;
    coordinates->point.set1Value(0, SbVec3f(-1, -1, 0));
    coordinates->point.set1Value(1, SbVec3f(1, -1, 0));
    coordinates->point.set1Value(2, SbVec3f(0, 1, 0));
    root->addChild(coordinates);
    auto * faces = new SoIndexedFaceSet;
    const int32_t indices[] = {0, 1, 2, -1};
    faces->coordIndex.setValues(0, 4, indices);
    root->addChild(faces);

    SoRayPickAction picker(SbViewportRegion(512, 512));
    picker.setRay(SbVec3f(0, 0, 10), SbVec3f(0, 0, -1));
    picker.apply(root);
    EXPECT_GT(picker.getPickedPointList().getLength(), 0);
    root->unref();
}

TEST(RayPickAction, GeometricIntersectionHelpersReportExpectedHits)
{
    auto * root = new SoSeparator;
    root->ref();
    root->addChild(new SoSphere);

    SoRayPickAction picker(SbViewportRegion(256, 256));
    picker.setRay(SbVec3f(0.0f, 0.0f, 5.0f), SbVec3f(0.0f, 0.0f, -1.0f));
    picker.apply(root);

    const SbVec3f & direction = picker.getLine().getDirection();
    EXPECT_NEAR(direction[2], -1.0f, 1.0e-6f);
    EXPECT_TRUE(picker.hasWorldSpaceRay());
    EXPECT_TRUE(picker.isBetweenPlanes(SbVec3f(0.0f, 0.0f, 0.0f)));

    SbVec3f intersection;
    SbVec3f barycentric;
    SbBool front_face = FALSE;
    EXPECT_TRUE(picker.intersect(SbVec3f(-1.0f, -1.0f, 0.0f),
                                 SbVec3f(1.0f, -1.0f, 0.0f),
                                 SbVec3f(0.0f, 1.0f, 0.0f),
                                 intersection, barycentric, front_face));
    EXPECT_NEAR(intersection[2], 0.0f, 1.0e-6f);

    EXPECT_TRUE(picker.intersect(SbVec3f(-1.0f, 0.0f, 0.0f),
                                 SbVec3f(1.0f, 0.0f, 0.0f), intersection));
    EXPECT_TRUE(picker.intersect(SbVec3f(0.0f, 0.0f, 0.0f)));

    SbBool on_surface = FALSE;
    EXPECT_TRUE(picker.intersect(SbBox3f(SbVec3f(-1.0f, -1.0f, -1.0f),
                                         SbVec3f(1.0f, 1.0f, 1.0f)),
                                 intersection, on_surface));
    root->unref();
}

TEST(RayPickAction, IntersectsPrimitiveShapesAndReportsTranslatedPickDetails)
{
    const auto hits_shape = [](SoShape * shape, const SbVec3f & origin) {
        auto * root = new SoSeparator;
        root->ref();
        root->addChild(shape);
        SoRayPickAction picker(SbViewportRegion(512, 512));
        picker.setRay(origin, SbVec3f(0.0f, 0.0f, -1.0f));
        picker.apply(root);
        const bool hit = picker.getPickedPoint() != nullptr;
        root->unref();
        return hit;
    };
    EXPECT_TRUE(hits_shape(new SoSphere, SbVec3f(0.0f, 0.0f, 5.0f)));
    EXPECT_FALSE(hits_shape(new SoSphere, SbVec3f(5.0f, 5.0f, 5.0f)));
    EXPECT_TRUE(hits_shape(new SoCube, SbVec3f(0.0f, 0.0f, 5.0f)));
    EXPECT_TRUE(hits_shape(new SoCylinder, SbVec3f(0.0f, 0.0f, 5.0f)));
    EXPECT_TRUE(hits_shape(new SoCone, SbVec3f(0.0f, 0.0f, 5.0f)));

    auto * root = new SoSeparator;
    root->ref();
    auto * translation = new SoTransform;
    translation->translation.setValue(2.0f, 0.0f, 0.0f);
    auto * cube = new SoCube;
    root->addChild(translation);
    root->addChild(cube);
    SoRayPickAction picker(SbViewportRegion(512, 512));
    picker.setRay(SbVec3f(2.0f, 0.0f, 5.0f), SbVec3f(0.0f, 0.0f, -1.0f));
    picker.apply(root);
    auto * picked = picker.getPickedPoint();
    ASSERT_NE(picked, nullptr);
    EXPECT_NEAR(std::fabs(picked->getNormal()[2]), 1.0f, 0.1f);
    ASSERT_NE(picked->getPath(), nullptr);
    EXPECT_GE(picked->getPath()->getLength(), 2);
    EXPECT_EQ(picked->getPath()->getTail(), cube);
    root->unref();
}

TEST(HandleEventAction, RetainsEventStateAndSafelyTraversesScenes)
{
    SoHandleEventAction action(SbViewportRegion(800, 600));
    EXPECT_EQ(action.getViewportRegion().getWindowSize()[0], 800);
    EXPECT_EQ(action.getViewportRegion().getWindowSize()[1], 600);
    EXPECT_FALSE(action.isHandled());

    SoMouseButtonEvent event;
    event.setButton(SoMouseButtonEvent::BUTTON1);
    event.setState(SoButtonEvent::DOWN);
    event.setPosition(SbVec2s(256, 256));
    action.setEvent(&event);
    EXPECT_EQ(action.getEvent(), &event);
    action.setHandled();
    EXPECT_TRUE(action.isHandled());

    action.setPickRadius(5.0f);
    EXPECT_FLOAT_EQ(action.getPickRadius(), 5.0f);

    auto * root = new SoSeparator;
    root->ref();
    root->addChild(new SoSphere);
    action.apply(root);
    root->unref();
}

TEST(HandleEventAction, TraversesKeyboardAndSecondaryMouseEvents)
{
    auto * root = new SoSeparator;
    root->ref();
    root->addChild(new SoPerspectiveCamera);
    root->addChild(new SoSphere);

    SoHandleEventAction action(SbViewportRegion(512, 512));
    SoKeyboardEvent keyboard;
    keyboard.setKey(SoKeyboardEvent::SPACE);
    keyboard.setState(SoButtonEvent::DOWN);
    action.setEvent(&keyboard);
    action.apply(root);
    EXPECT_EQ(action.getEvent(), &keyboard);
    EXPECT_TRUE(SoKeyboardEvent::isKeyPressEvent(&keyboard, SoKeyboardEvent::SPACE));

    keyboard.setKey(SoKeyboardEvent::RETURN);
    keyboard.setState(SoButtonEvent::UP);
    action.apply(root);
    EXPECT_TRUE(SoKeyboardEvent::isKeyReleaseEvent(&keyboard, SoKeyboardEvent::RETURN));

    SoMouseButtonEvent mouse;
    mouse.setPosition(SbVec2s(256, 256));
    mouse.setButton(SoMouseButtonEvent::BUTTON2);
    mouse.setState(SoButtonEvent::DOWN);
    action.setEvent(&mouse);
    action.apply(root);
    EXPECT_TRUE(SoMouseButtonEvent::isButtonPressEvent(&mouse, SoMouseButtonEvent::BUTTON2));

    mouse.setButton(SoMouseButtonEvent::BUTTON3);
    action.apply(root);
    EXPECT_TRUE(SoMouseButtonEvent::isButtonPressEvent(&mouse, SoMouseButtonEvent::BUTTON3));
    root->unref();
}

TEST(Actions, PathsProvideAccumulatedMatricesAndConfigurableSearchDepth)
{
    auto * root = new SoSeparator;
    root->ref();
    auto * first_translation = new SoTranslation;
    first_translation->translation.setValue(2.0f, 0.0f, 0.0f);
    root->addChild(first_translation);
    auto * second_translation = new SoTranslation;
    second_translation->translation.setValue(0.0f, 3.0f, 0.0f);
    root->addChild(second_translation);
    auto * cube = new SoCube;
    cube->setName("modern-deep-action-cube");
    root->addChild(cube);

    SoSearchAction search;
    search.setName("modern-deep-action-cube");
    search.setFind(SoSearchAction::NAME);
    search.setSearchingAll(TRUE);
    EXPECT_TRUE(search.isSearchingAll());
    search.apply(root);
    ASSERT_NE(search.getPath(), nullptr);

    SoGetMatrixAction matrix_action(SbViewportRegion(512, 512));
    matrix_action.apply(search.getPath());
    SbVec3f transformed_origin;
    matrix_action.getMatrix().multVecMatrix(SbVec3f(0, 0, 0), transformed_origin);
    EXPECT_NEAR(transformed_origin[0], 2.0f, 0.1f);
    EXPECT_NEAR(transformed_origin[1], 3.0f, 0.1f);

    SoGetBoundingBoxAction bounds_action(SbViewportRegion(512, 512));
    bounds_action.apply(search.getPath());
    EXPECT_FALSE(bounds_action.getBoundingBox().isEmpty());
    root->unref();
}
