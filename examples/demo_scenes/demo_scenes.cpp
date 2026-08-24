#include "demo_scenes.h"

#include <Inventor/SbColor.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbPlane.h>
#include <Inventor/SbVec2s.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/nodes/SoAlphaTest.h>
#include <Inventor/nodes/SoAnnotation.h>
#include <Inventor/nodes/SoArray.h>
#include <Inventor/nodes/SoClipPlane.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/nodes/SoEnvironment.h>
#include <Inventor/nodes/SoFaceSet.h>
#include <Inventor/nodes/SoImage.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <Inventor/nodes/SoIndexedLineSet.h>
#include <Inventor/nodes/SoLOD.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoMaterialBinding.h>
#include <Inventor/nodes/SoMultipleCopy.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoPointLight.h>
#include <Inventor/nodes/SoPointSet.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoShapeHints.h>
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/nodes/SoTextureCoordinate2.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoSpotLight.h>
#include <Inventor/nodes/SoText2.h>
#include <Inventor/nodes/SoText3.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoVertexProperty.h>
#include <Inventor/nodes/SoSelection.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/engines/SoComposeVec3f.h>
#include <Inventor/engines/SoElapsedTime.h>
#include <Inventor/engines/SoInterpolateFloat.h>
#include <Inventor/draggers/SoHandleBoxDragger.h>
#include <Inventor/draggers/SoRotateSphericalDragger.h>
#include <Inventor/draggers/SoTabPlaneDragger.h>
#include <Inventor/draggers/SoTranslate1Dragger.h>

namespace ObolDemo {
namespace {

SoSeparator * makeRoot(const SbVec3f & camera_position = SbVec3f(0.0f, 0.0f, 11.0f))
{
    auto * root = new SoSeparator;
    root->ref();

    auto * camera = new SoPerspectiveCamera;
    camera->position.setValue(camera_position);
    camera->nearDistance.setValue(0.1f);
    camera->farDistance.setValue(200.0f);
    root->addChild(camera);
    return root;
}

SoSeparator * translated(float x, float y, float z)
{
    auto * group = new SoSeparator;
    auto * translation = new SoTranslation;
    translation->translation.setValue(x, y, z);
    group->addChild(translation);
    return group;
}

SoMaterial * material(float red, float green, float blue,
                      float shininess = 0.2f)
{
    auto * value = new SoMaterial;
    value->diffuseColor.setValue(red, green, blue);
    value->specularColor.setValue(0.8f, 0.8f, 0.8f);
    value->shininess.setValue(shininess);
    return value;
}

void addKeyLight(SoSeparator * root)
{
    auto * light = new SoDirectionalLight;
    light->direction.setValue(-0.4f, -0.7f, -1.0f);
    light->intensity.setValue(0.9f);
    root->addChild(light);
}

SoSeparator * createPrimitives(int, int)
{
    auto * root = makeRoot();
    addKeyLight(root);

    auto * sphere = translated(-3.0f, 1.8f, 0.0f);
    sphere->addChild(material(0.9f, 0.2f, 0.15f));
    sphere->addChild(new SoSphere);
    root->addChild(sphere);

    auto * cube = translated(3.0f, 1.8f, 0.0f);
    cube->addChild(material(0.15f, 0.55f, 0.95f));
    cube->addChild(new SoCube);
    root->addChild(cube);

    auto * cone = translated(-3.0f, -2.0f, 0.0f);
    cone->addChild(material(0.95f, 0.75f, 0.15f));
    cone->addChild(new SoCone);
    root->addChild(cone);

    auto * cylinder = translated(3.0f, -2.0f, 0.0f);
    cylinder->addChild(material(0.25f, 0.85f, 0.45f));
    cylinder->addChild(new SoCylinder);
    root->addChild(cylinder);
    return root;
}

SoSeparator * createMaterials(int, int)
{
    auto * root = makeRoot();
    addKeyLight(root);

    const float x[] = {-4.5f, -1.5f, 1.5f, 4.5f};
    const float shine[] = {0.0f, 0.2f, 0.6f, 1.0f};
    for (int i = 0; i < 4; ++i) {
        auto * group = translated(x[i], 0.0f, 0.0f);
        group->addChild(material(0.3f, 0.55f, 0.95f, shine[i]));
        group->addChild(new SoSphere);
        root->addChild(group);
    }
    return root;
}

SoSeparator * createTransforms(int, int)
{
    auto * root = makeRoot();
    addKeyLight(root);
    const float x[] = {-4.0f, 0.0f, 4.0f};
    const float angle[] = {0.0f, 0.55f, 1.1f};
    const float scale[] = {0.7f, 1.0f, 1.35f};
    for (int i = 0; i < 3; ++i) {
        auto * group = translated(x[i], 0.0f, 0.0f);
        auto * transform = new SoTransform;
        transform->rotation.setValue(SbVec3f(0.0f, 1.0f, 0.0f), angle[i]);
        transform->scaleFactor.setValue(scale[i], scale[i], scale[i]);
        group->addChild(transform);
        group->addChild(material(0.2f + i * 0.25f, 0.75f - i * 0.18f, 0.45f));
        group->addChild(new SoCube);
        root->addChild(group);
    }
    return root;
}

SoSeparator * createLighting(int, int)
{
    auto * root = makeRoot(SbVec3f(0.0f, 0.0f, 12.0f));

    auto * directional = new SoDirectionalLight;
    directional->direction.setValue(0.0f, -0.2f, -1.0f);
    directional->intensity.setValue(0.7f);
    root->addChild(directional);

    auto * point = new SoPointLight;
    point->location.setValue(0.0f, 4.0f, 4.0f);
    point->color.setValue(0.3f, 0.5f, 1.0f);
    point->intensity.setValue(0.8f);
    root->addChild(point);

    auto * spot = new SoSpotLight;
    spot->location.setValue(-5.0f, 4.0f, 5.0f);
    spot->direction.setValue(0.7f, -0.5f, -1.0f);
    spot->color.setValue(1.0f, 0.35f, 0.2f);
    spot->intensity.setValue(0.7f);
    root->addChild(spot);

    const float x[] = {-3.5f, 0.0f, 3.5f};
    for (float position : x) {
        auto * group = translated(position, 0.0f, 0.0f);
        group->addChild(material(0.75f, 0.75f, 0.75f, 0.45f));
        group->addChild(new SoSphere);
        root->addChild(group);
    }
    return root;
}

SoSeparator * createTransparency(int, int)
{
    auto * root = makeRoot(SbVec3f(0.0f, 0.0f, 9.0f));
    addKeyLight(root);
    for (int i = 0; i < 3; ++i) {
        auto * group = translated(-1.5f + i * 1.5f, 0.0f, -i * 0.4f);
        auto * value = material(i == 0 ? 0.95f : 0.15f,
                                i == 1 ? 0.95f : 0.15f,
                                i == 2 ? 0.95f : 0.15f,
                                0.5f);
        value->transparency.setValue(0.35f);
        group->addChild(value);
        group->addChild(new SoSphere);
        root->addChild(group);
    }
    return root;
}

SoSeparator * createSceneGraph(int, int)
{
    auto * root = makeRoot();
    addKeyLight(root);

    auto * parent = new SoSeparator;
    parent->setName("assembly");
    auto * base = translated(0.0f, -2.0f, 0.0f);
    base->addChild(material(0.25f, 0.25f, 0.3f));
    auto * base_scale = new SoTransform;
    base_scale->scaleFactor.setValue(4.0f, 0.35f, 1.5f);
    base->addChild(base_scale);
    base->addChild(new SoCube);
    parent->addChild(base);

    auto * arm = translated(0.0f, 0.5f, 0.0f);
    auto * arm_transform = new SoTransform;
    arm_transform->rotation.setValue(SbVec3f(0.0f, 0.0f, 1.0f), -0.35f);
    arm_transform->scaleFactor.setValue(0.55f, 2.8f, 0.55f);
    arm->addChild(arm_transform);
    arm->addChild(material(0.85f, 0.4f, 0.12f));
    arm->addChild(new SoCube);
    parent->addChild(arm);

    auto * tip = translated(1.0f, 3.0f, 0.0f);
    tip->addChild(material(0.2f, 0.7f, 0.9f));
    tip->addChild(new SoSphere);
    parent->addChild(tip);
    root->addChild(parent);
    return root;
}

SoSeparator * createText(int, int)
{
    auto * root = makeRoot(SbVec3f(0.0f, 0.0f, 12.0f));
    addKeyLight(root);
    auto * title = translated(-4.0f, 0.0f, 0.0f);
    title->addChild(material(0.95f, 0.5f, 0.12f, 0.45f));
    auto * text = new SoText3;
    text->string.setValue("Obol");
    text->justification.setValue(SoText3::LEFT);
    title->addChild(text);
    root->addChild(title);

    auto * caption = translated(-4.0f, -2.3f, 0.0f);
    caption->addChild(material(0.45f, 0.9f, 0.55f));
    auto * text2 = new SoText2;
    text2->string.setValue("Scene graph rendering");
    caption->addChild(text2);
    root->addChild(caption);
    return root;
}

SoSeparator * createDraggers(int, int)
{
    auto * root = makeRoot(SbVec3f(0.0f, 0.0f, 12.0f));
    addKeyLight(root);

    auto * horizontal = translated(0.0f, 1.8f, 0.0f);
    horizontal->addChild(new SoTranslate1Dragger);
    root->addChild(horizontal);

    auto * spherical = translated(0.0f, -1.8f, 0.0f);
    spherical->addChild(new SoRotateSphericalDragger);
    root->addChild(spherical);

    auto * reference = translated(3.0f, 0.0f, 0.0f);
    reference->addChild(material(0.2f, 0.55f, 0.9f));
    reference->addChild(new SoCube);
    root->addChild(reference);
    return root;
}

SoSeparator * createSelection(int, int)
{
    auto * root = makeRoot(SbVec3f(0.0f, 0.0f, 12.0f));
    addKeyLight(root);

    auto * selection = new SoSelection;
    selection->policy.setValue(SoSelection::SHIFT);
    root->addChild(selection);

    const float positions[] = {-3.0f, 0.0f, 3.0f};
    for (int i = 0; i < 3; ++i) {
        auto * item = translated(positions[i], 0.0f, 0.0f);
        item->addChild(material(i == 0 ? 0.9f : 0.2f,
                                i == 1 ? 0.8f : 0.35f,
                                i == 2 ? 0.9f : 0.25f));
        if (i == 0) item->addChild(new SoSphere);
        else if (i == 1) item->addChild(new SoCube);
        else item->addChild(new SoCone);
        selection->addChild(item);
    }
    return root;
}

SoSeparator * createIndexedGeometry(int, int)
{
    auto * root = makeRoot(SbVec3f(0.0f, 0.0f, 14.0f));
    addKeyLight(root);

    auto * faces = translated(-3.5f, 1.0f, 0.0f);
    faces->addChild(material(0.85f, 0.25f, 0.2f));
    auto * face_coordinates = new SoCoordinate3;
    face_coordinates->point.set1Value(0, SbVec3f(-1.2f, -1.0f, 0.0f));
    face_coordinates->point.set1Value(1, SbVec3f(1.2f, -1.0f, 0.0f));
    face_coordinates->point.set1Value(2, SbVec3f(1.2f, 1.0f, 0.0f));
    face_coordinates->point.set1Value(3, SbVec3f(-1.2f, 1.0f, 0.0f));
    faces->addChild(face_coordinates);
    auto * face_set = new SoIndexedFaceSet;
    const int32_t face_indices[] = {0, 1, 2, 3, -1};
    face_set->coordIndex.setValues(0, 5, face_indices);
    faces->addChild(face_set);
    root->addChild(faces);

    auto * lines = translated(0.0f, 1.0f, 0.0f);
    auto * line_style = new SoDrawStyle;
    line_style->style.setValue(SoDrawStyle::LINES);
    line_style->lineWidth.setValue(3.0f);
    lines->addChild(line_style);
    lines->addChild(material(0.2f, 0.8f, 0.95f));
    auto * line_coordinates = new SoCoordinate3;
    line_coordinates->point.set1Value(0, SbVec3f(-1.2f, -1.0f, 0.0f));
    line_coordinates->point.set1Value(1, SbVec3f(0.0f, 1.0f, 0.0f));
    line_coordinates->point.set1Value(2, SbVec3f(1.2f, -1.0f, 0.0f));
    lines->addChild(line_coordinates);
    auto * line_set = new SoIndexedLineSet;
    const int32_t line_indices[] = {0, 1, 2, -1};
    line_set->coordIndex.setValues(0, 4, line_indices);
    lines->addChild(line_set);
    root->addChild(lines);

    auto * points = translated(3.5f, 1.0f, 0.0f);
    auto * point_style = new SoDrawStyle;
    point_style->style.setValue(SoDrawStyle::POINTS);
    point_style->pointSize.setValue(8.0f);
    points->addChild(point_style);
    points->addChild(material(0.95f, 0.75f, 0.15f));
    auto * point_coordinates = new SoCoordinate3;
    for (int index = 0; index < 5; ++index)
        point_coordinates->point.set1Value(index,
            SbVec3f(-1.0f + index * 0.5f, (index % 2) ? 0.5f : -0.5f, 0.0f));
    points->addChild(point_coordinates);
    auto * point_set = new SoPointSet;
    point_set->numPoints.setValue(5);
    points->addChild(point_set);
    root->addChild(points);
    return root;
}

SoSeparator * createTextures(int, int)
{
    auto * root = makeRoot(SbVec3f(0.0f, 0.0f, 8.0f));
    addKeyLight(root);

    auto * texture = new SoTexture2;
    const unsigned char pixels[] = {
        230, 50, 50, 50, 230, 80,
        50, 100, 230, 240, 220, 60
    };
    texture->image.setValue(SbVec2s(2, 2), 3, pixels);
    texture->wrapS.setValue(SoTexture2::REPEAT);
    texture->wrapT.setValue(SoTexture2::REPEAT);
    root->addChild(texture);

    auto * coordinates = new SoCoordinate3;
    coordinates->point.set1Value(0, SbVec3f(-3.0f, -2.0f, 0.0f));
    coordinates->point.set1Value(1, SbVec3f(3.0f, -2.0f, 0.0f));
    coordinates->point.set1Value(2, SbVec3f(3.0f, 2.0f, 0.0f));
    coordinates->point.set1Value(3, SbVec3f(-3.0f, 2.0f, 0.0f));
    root->addChild(coordinates);
    auto * texcoords = new SoTextureCoordinate2;
    texcoords->point.set1Value(0, SbVec2f(0.0f, 0.0f));
    texcoords->point.set1Value(1, SbVec2f(2.0f, 0.0f));
    texcoords->point.set1Value(2, SbVec2f(2.0f, 2.0f));
    texcoords->point.set1Value(3, SbVec2f(0.0f, 2.0f));
    root->addChild(texcoords);
    auto * face_set = new SoFaceSet;
    face_set->numVertices.set1Value(0, 4);
    root->addChild(face_set);
    return root;
}

SoSeparator * createSpecialNodes(int, int)
{
    auto * root = makeRoot(SbVec3f(0.0f, 0.0f, 15.0f));
    addKeyLight(root);

    auto * annotation = translated(-4.0f, 2.0f, 0.0f);
    annotation->addChild(material(0.2f, 0.3f, 0.9f));
    annotation->addChild(new SoCube);
    auto * overlay = new SoAnnotation;
    overlay->addChild(material(0.95f, 0.2f, 0.15f));
    overlay->addChild(new SoSphere);
    annotation->addChild(overlay);
    root->addChild(annotation);

    auto * lod_group = translated(0.0f, 2.0f, 0.0f);
    auto * lod = new SoLOD;
    lod->center.setValue(SbVec3f(0.0f, 0.0f, 0.0f));
    lod->range.set1Value(0, 3.0f);
    lod->range.set1Value(1, 8.0f);
    lod->addChild(new SoSphere);
    lod->addChild(new SoCone);
    lod->addChild(new SoCube);
    lod_group->addChild(material(0.2f, 0.8f, 0.35f));
    lod_group->addChild(lod);
    root->addChild(lod_group);

    auto * array_group = translated(4.0f, 1.0f, 0.0f);
    auto * array = new SoArray;
    array->numElements1.setValue(2);
    array->numElements2.setValue(2);
    array->separation1.setValue(SbVec3f(1.2f, 0.0f, 0.0f));
    array->separation2.setValue(SbVec3f(0.0f, 1.2f, 0.0f));
    array->addChild(material(0.8f, 0.5f, 0.15f));
    array->addChild(new SoSphere);
    array_group->addChild(array);
    root->addChild(array_group);
    return root;
}

SoSeparator * createEnvironment(int, int)
{
    auto * root = makeRoot(SbVec3f(0.0f, 0.0f, 9.0f));
    addKeyLight(root);
    auto * environment = new SoEnvironment;
    environment->fogType.setValue(SoEnvironment::HAZE);
    environment->fogColor.setValue(SbColor(0.35f, 0.45f, 0.65f));
    environment->fogVisibility.setValue(12.0f);
    environment->ambientIntensity.setValue(0.35f);
    root->addChild(environment);
    auto * clip = new SoClipPlane;
    clip->plane.setValue(SbPlane(SbVec3f(0.0f, 1.0f, 0.0f), -0.2f));
    root->addChild(clip);
    auto * group = translated(0.0f, 0.0f, 0.0f);
    group->addChild(material(0.2f, 0.7f, 0.9f));
    group->addChild(new SoSphere);
    root->addChild(group);
    return root;
}

SoSeparator * createAnimated(int, int)
{
    auto * root = makeRoot(SbVec3f(0.0f, 0.0f, 9.0f));
    addKeyLight(root);
    auto * timer = new SoElapsedTime;
    auto * interpolate = new SoInterpolateFloat;
    interpolate->input0.setValue(0.15f);
    interpolate->input1.setValue(0.75f);
    interpolate->alpha.connectFrom(&timer->timeOut);
    auto * group = translated(0.0f, 0.0f, 0.0f);
    auto * appearance = material(0.85f, 0.3f, 0.7f);
    appearance->transparency.connectFrom(&interpolate->output);
    group->addChild(appearance);
    group->addChild(new SoSphere);
    root->addChild(group);
    return root;
}

SoSeparator * createAdvancedDraggers(int, int)
{
    auto * root = makeRoot(SbVec3f(0.0f, 0.0f, 12.0f));
    addKeyLight(root);
    auto * left = translated(-2.5f, 0.0f, 0.0f);
    left->addChild(new SoHandleBoxDragger);
    root->addChild(left);
    auto * right = translated(2.5f, 0.0f, 0.0f);
    right->addChild(new SoTabPlaneDragger);
    root->addChild(right);
    return root;
}

} // namespace

const std::vector<DemoScene> & demoScenes()
{
    static const std::vector<DemoScene> scenes = {
        {"primitives", "Core", "Primitives", "Sphere, cube, cone, and cylinder.", false, {}, createPrimitives, {}},
        {"materials", "Rendering", "Materials", "Specular and shininess variations.", false, {}, createMaterials, {}},
        {"transforms", "Core", "Transforms", "Translation, rotation, and scaling.", false, {}, createTransforms, {}},
        {"lighting", "Rendering", "Lights", "Directional, point, and spot lighting.", false, {}, createLighting, {}},
        {"scene_graph", "Core", "Scene graph", "A small hierarchical mechanical assembly.", false, {}, createSceneGraph, {}},
        {"text", "Rendering", "Text", "3-D title with a 2-D screen-space caption.", false, {}, createText, {}},
        {"transparency", "Rendering", "Transparency", "Overlapping translucent spheres.", false, {false, false, false}, createTransparency, {}},
        {"draggers", "Interaction", "Draggers", "Translate and rotate handles; drag with the mouse.", true, {}, createDraggers, {}},
        {"selection", "Interaction", "Selection", "Shift-click shapes to select multiple scene items.", true, {}, createSelection, {}},
        {"indexed_geometry", "Geometry", "Indexed geometry", "Indexed faces, lines, and point sets with explicit draw styles.", false, {}, createIndexedGeometry, {}},
        {"textures", "Rendering", "Textures", "A repeating 2-D texture mapped onto a quad.", false, {false, false, false}, createTextures, {}},
        {"special_nodes", "Scene graph", "Special nodes", "Annotation, LOD, and array traversal in one scene.", false, {}, createSpecialNodes, {}},
        {"environment", "Rendering", "Environment", "Fog and clip-plane state applied to lit geometry.", false, {}, createEnvironment, {}},
        {"animated", "Engines", "Animated fields", "Elapsed-time and interpolation engine connections.", false, {}, createAnimated, {}},
        {"advanced_draggers", "Interaction", "Advanced draggers", "Handle-box and tab-plane interaction handles.", true, {}, createAdvancedDraggers, {}}
    };
    return scenes;
}

const DemoScene * findDemoScene(const std::string_view id)
{
    const auto & scenes = demoScenes();
    for (const auto & scene : scenes)
        if (scene.id == id) return &scene;
    return nullptr;
}

} // namespace ObolDemo
