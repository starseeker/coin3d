/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
\**************************************************************************/

#include "../test_utils.h"
#include "../../examples/modern_api_example.h"

#include <Obol/Obol.h>

#include <Inventor/SoDB.h>
#include <Inventor/SoInteraction.h>
#include <Inventor/nodes/SoCallback.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <Inventor/nodes/SoLinearProfile.h>
#include <Inventor/nodes/SoLineSet.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoMaterialBinding.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/nodes/SoPointSet.h>
#include <Inventor/nodes/SoText3.h>
#include <Inventor/nodes/SoTextureCoordinate2.h>

#include <string>

using namespace SimpleTest;

namespace {

class SolidColorContextManager : public SoDB::ContextManager {
public:
    void * createOffscreenContext(unsigned int, unsigned int) override { return nullptr; }
    SbBool makeContextCurrent(void *) override { return FALSE; }
    void restorePreviousContext(void *) override {}
    void destroyContext(void *) override {}

    SbBool renderScene(SoNode *,
                       unsigned int width,
                       unsigned int height,
                       unsigned char * pixels,
                       unsigned int nrcomponents,
                       const float[3]) override
    {
        if (!pixels || nrcomponents < 3) return FALSE;
        for (unsigned int i = 0; i < width * height; ++i) {
            pixels[i * nrcomponents + 0] = 17;
            pixels[i * nrcomponents + 1] = 34;
            pixels[i * nrcomponents + 2] = 51;
            if (nrcomponents == 4) pixels[i * nrcomponents + 3] = 255;
        }
        return TRUE;
    }
};

SoSeparator * findNamedSeparator(SoSeparator * root, const char * name)
{
    if (!root) return nullptr;
    if (root->getName() == SbName(name)) return root;
    for (int i = 0; i < root->getNumChildren(); ++i) {
        SoSeparator * child = dynamic_cast<SoSeparator *>(root->getChild(i));
        if (!child) continue;
        SoSeparator * found = findNamedSeparator(child, name);
        if (found) return found;
    }
    return nullptr;
}

void testOpenGLDrawCallback(void *, const obol::OpenGLCallbackContext &)
{
}

} // namespace

int main()
{
    static SolidColorContextManager manager;
    static obol::ContextManagerBackend backend(&manager,
                                               obol::RenderBackendKind::CPU,
                                               "solid-color-test");
    if (!SoDB::isInitialized()) {
        SoDB::init(&manager);
        SoInteraction::init();
    }

    TestRunner runner;

    runner.startTest("v2 Scene creates a legacy graph bridge");
    {
        obol::Scene scene;
        scene.setCamera(obol::PerspectiveCamera{});
        scene.addDirectionalLight(obol::DirectionalLight{});
        obol::Material red;
        red.baseColor = {1.0f, 0.0f, 0.0f, 1.0f};
        scene.addPrimitive(obol::Primitive::Cone, red);

        SoSeparator * root = scene.createLegacySceneGraph();
        const bool pass = root && root->getNumChildren() == 3 && scene.objectCount() == 2;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Scene did not produce expected bridge graph");
    }

    runner.startTest("v2 Scene queries objects by category and type");
    {
        obol::Scene scene;
        const obol::SceneGroupId group = scene.addGroup();
        const obol::SceneObjectId cube = scene.addPrimitive(obol::Primitive::Cube);
        const obol::SceneObjectId light = scene.addPointLight(obol::PointLight{}, group);
        scene.addText3D(obol::Text3D{});

        obol::SceneQuery lightQuery;
        lightQuery.category = obol::SceneObjectCategory::Light;
        const std::vector<obol::SceneObjectInfo> lights = scene.findObjects(lightQuery);

        obol::SceneQuery pointLightQuery;
        pointLightQuery.type = obol::SceneObjectType::PointLight;

        obol::SceneQuery textQuery;
        textQuery.category = obol::SceneObjectCategory::Text;

        obol::SceneQuery impossibleQuery;
        impossibleQuery.type = obol::SceneObjectType::Primitive;
        impossibleQuery.category = obol::SceneObjectCategory::Light;

        const bool pass = cube == 1 &&
                          light == 2 &&
                          lights.size() == 1 &&
                          lights[0].id == light &&
                          lights[0].type == obol::SceneObjectType::PointLight &&
                          lights[0].parent == group &&
                          scene.findFirstObject(pointLightQuery) == light &&
                          scene.hasObjects(textQuery) &&
                          !scene.hasObjects(impossibleQuery);
        runner.endTest(pass, pass ? "" : "Scene query did not return expected v2 object IDs");
    }

    runner.startTest("v2 Scene updates object materials by stable id");
    {
        obol::Scene scene;
        obol::Material red;
        red.baseColor = {1.0f, 0.0f, 0.0f, 1.0f};
        const obol::SceneObjectId cube = scene.addPrimitive(obol::Primitive::Cube, red);
        const obol::SceneObjectId light = scene.addDirectionalLight(obol::DirectionalLight{});

        obol::Material green;
        green.baseColor = {0.0f, 1.0f, 0.0f, 1.0f};
        const bool updated = scene.setObjectMaterial(cube, green);
        const bool rejectedLight = !scene.setObjectMaterial(light, green);
        const bool rejectedInvalid =
            !scene.setObjectMaterial(obol::InvalidSceneObjectId, green);

        SoSeparator * root = scene.createLegacySceneGraph();
        SoSeparator * cubeSep = findNamedSeparator(root, "ObolSceneObject_1");
        SoMaterial * material = cubeSep && cubeSep->getNumChildren() >= 2
            ? dynamic_cast<SoMaterial *>(cubeSep->getChild(1))
            : nullptr;
        SbColor color = material ? material->diffuseColor[0] : SbColor(0.0f, 0.0f, 0.0f);
        const bool pass = updated &&
                          rejectedLight &&
                          rejectedInvalid &&
                          material &&
                          color[0] == 0.0f &&
                          color[1] == 1.0f &&
                          color[2] == 0.0f;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Material update did not bridge by object id");
    }

    runner.startTest("v2 Scene updates primitive options by stable id");
    {
        obol::Scene scene;
        obol::PrimitiveOptions options;
        options.radius = 0.5f;
        options.height = 2.0f;
        const obol::SceneObjectId cone =
            scene.addPrimitive(obol::Primitive::Cone,
                               obol::Material{},
                               obol::Transform{},
                               options);
        const obol::SceneObjectId light = scene.addDirectionalLight(obol::DirectionalLight{});

        options.radius = 1.75f;
        options.height = 3.0f;
        const bool updated = scene.setObjectPrimitiveOptions(cone, options);
        const bool rejectedLight =
            !scene.setObjectPrimitiveOptions(light, options);
        const bool rejectedInvalid =
            !scene.setObjectPrimitiveOptions(obol::InvalidSceneObjectId, options);

        SoSeparator * root = scene.createLegacySceneGraph();
        SoSeparator * coneSep = findNamedSeparator(root, "ObolSceneObject_1");
        SoCone * coneNode = coneSep && coneSep->getNumChildren() >= 3
            ? dynamic_cast<SoCone *>(coneSep->getChild(2))
            : nullptr;
        const bool pass = updated &&
                          rejectedLight &&
                          rejectedInvalid &&
                          coneNode &&
                          coneNode->bottomRadius.getValue() == 1.75f &&
                          coneNode->height.getValue() == 3.0f;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Primitive option update did not bridge by object id");
    }

    runner.startTest("v2 sphere tessellation generates portable mesh data");
    {
        const obol::Mesh mesh = obol::makeSphereMesh(2.0f, 8, 4);
        obol::Scene scene;
        const obol::SceneObjectId sphereMesh = scene.addMesh(mesh);
        SoSeparator * root = scene.createLegacySceneGraph();
        const bool pass = sphereMesh == 1 &&
                          mesh.topology == obol::MeshTopology::Triangles &&
                          mesh.positions.size() == 45 &&
                          mesh.normals.size() == mesh.positions.size() &&
                          mesh.indices.size() == 144 &&
                          root &&
                          root->getNumChildren() == 1;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Sphere tessellation did not produce expected triangle mesh");
    }

    runner.startTest("v2 Scene updates point cloud geometry by stable id");
    {
        obol::PointCloud points;
        points.points = {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}};
        points.pointSize = 4.0f;
        obol::Scene scene;
        const obol::SceneObjectId pointCloud = scene.addPointCloud(points);

        obol::PointCloud updated = points;
        updated.points.push_back({0.0f, 1.0f, 0.0f});
        updated.pointSize = 7.0f;
        const bool updatedPoints = scene.setObjectPointCloud(pointCloud, updated);
        const bool rejectedInvalid =
            !scene.setObjectPointCloud(obol::InvalidSceneObjectId, updated);

        obol::SceneQuery query;
        query.type = obol::SceneObjectType::PointCloud;

        SoSeparator * root = scene.createLegacySceneGraph();
        SoSeparator * objectSep = root && root->getNumChildren() == 1
            ? dynamic_cast<SoSeparator *>(root->getChild(0))
            : nullptr;
        SoSeparator * pointSep = objectSep && objectSep->getNumChildren() >= 3
            ? dynamic_cast<SoSeparator *>(objectSep->getChild(2))
            : nullptr;
        SoDrawStyle * drawStyle = pointSep && pointSep->getNumChildren() >= 1
            ? dynamic_cast<SoDrawStyle *>(pointSep->getChild(0))
            : nullptr;
        SoPointSet * pointSet = pointSep && pointSep->getNumChildren() >= 3
            ? dynamic_cast<SoPointSet *>(pointSep->getChild(2))
            : nullptr;

        const bool pass = updatedPoints &&
                          rejectedInvalid &&
                          scene.findFirstObject(query) == pointCloud &&
                          drawStyle &&
                          drawStyle->pointSize.getValue() == 7.0f &&
                          pointSet &&
                          pointSet->numPoints.getValue() == 3;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Point cloud update did not bridge expected point data");
    }

    runner.startTest("v2 OffscreenRenderer uses alternative render backend");
    {
        obol::Scene scene;
        scene.addPrimitive(obol::Primitive::Cube);

        obol::RenderTarget target;
        target.width = 4;
        target.height = 4;
        target.pixelFormat = obol::PixelFormat::RGB;
        obol::OffscreenRenderer renderer(backend, target);
        obol::FrameResult result = renderer.render(scene);
        const unsigned char * pixels = renderer.pixels();
        const bool pass = result.success &&
                          result.target.width == 4 &&
                          result.target.height == 4 &&
                          result.target.pixelFormat == obol::PixelFormat::RGB &&
                          pixels &&
                          pixels[0] == 17 &&
                          pixels[1] == 34 &&
                          pixels[2] == 51 &&
                          result.capabilities.backendKind == obol::RenderBackendKind::CPU &&
                          result.capabilities.backendName == "solid-color-test" &&
                          result.capabilities.known == false;
        runner.endTest(pass, pass ? "" : "Alternative backend render did not fill expected pixels");
    }

    runner.startTest("v2 OffscreenRenderer updates explicit render targets");
    {
        obol::RenderTarget initial;
        initial.width = 1;
        initial.height = 1;
        obol::OffscreenRenderer renderer(backend, initial);
        obol::RenderTarget target;
        target.width = 3;
        target.height = 2;
        target.pixelFormat = obol::PixelFormat::RGBA;
        renderer.setRenderTarget(target);
        const obol::RenderTarget actual = renderer.renderTarget();
        const bool pass = renderer.width() == 3 &&
                          renderer.height() == 2 &&
                          renderer.pixelFormat() == obol::PixelFormat::RGBA &&
                          actual.width == 3 &&
                          actual.height == 2 &&
                          actual.pixelFormat == obol::PixelFormat::RGBA;
        runner.endTest(pass, pass ? "" : "Render target state did not update");
    }

    runner.startTest("v2 Scene bridges orthographic cameras");
    {
        obol::Scene scene;
        obol::OrthographicCamera camera;
        camera.position = {0.0f, 0.0f, 10.0f};
        camera.target = {0.0f, 0.0f, 0.0f};
        camera.height = 8.0f;
        scene.setCamera(camera);
        scene.addPrimitive(obol::Primitive::Cube);

        SoSeparator * root = scene.createLegacySceneGraph();
        SoNode * cameraNode = root && root->getNumChildren() >= 1
            ? root->getChild(0)
            : nullptr;
        const bool pass = scene.hasCamera() &&
                          cameraNode &&
                          cameraNode->isOfType(SoOrthographicCamera::getClassTypeId());
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Expected orthographic camera bridge node");
    }

    runner.startTest("v2 renderer records graceful degradation diagnostics");
    {
        obol::Scene scene;
        scene.addPrimitive(obol::Primitive::Sphere);

        obol::RenderOptions options;
        options.shadows = true;
        options.nativeShaders = true;

        obol::RenderTarget target;
        target.width = 2;
        target.height = 2;
        obol::OffscreenRenderer renderer(backend, target);
        obol::FrameResult result = renderer.render(scene, options);
        const bool pass = result.success && result.diagnostics.size() >= 2;
        runner.endTest(pass, pass ? "" : "Expected degradation diagnostics for unsupported options");
    }

    runner.startTest("v2 Scene rejects empty OpenGL callbacks");
    {
        obol::Scene scene;
        const obol::SceneObjectId id = scene.addOpenGLCallback(obol::OpenGLCallback{});
        const bool pass = id == obol::InvalidSceneObjectId &&
                          !scene.hasObjects(obol::SceneQuery{
                              obol::SceneObjectType::OpenGLCallback,
                              obol::SceneObjectCategory::BackendNative});
        runner.endTest(pass, pass ? "" : "Expected empty OpenGL callback to be rejected");
    }

    runner.startTest("v2 Scene bridges OpenGL callbacks as backend-native nodes");
    {
        obol::OpenGLCallback callback;
        callback.draw = testOpenGLDrawCallback;
        callback.label = "test";

        obol::Scene scene;
        const obol::SceneObjectId id = scene.addOpenGLCallback(callback);

        obol::SceneQuery query;
        query.type = obol::SceneObjectType::OpenGLCallback;
        query.category = obol::SceneObjectCategory::BackendNative;

        SoSeparator * root = scene.createLegacySceneGraph();
        SoCallback * callbackNode = root && root->getNumChildren() == 1
            ? dynamic_cast<SoCallback *>(root->getChild(0))
            : nullptr;
        const bool pass = id == 1 &&
                          scene.hasObjects(query) &&
                          callbackNode != nullptr;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Expected OpenGL callback bridge node");
    }

    runner.startTest("v2 renderer rejects OpenGL callbacks without an OpenGL backend");
    {
        obol::OpenGLCallback callback;
        callback.draw = testOpenGLDrawCallback;

        obol::Scene scene;
        scene.addOpenGLCallback(callback);

        obol::RenderTarget target;
        target.width = 2;
        target.height = 2;
        obol::OffscreenRenderer renderer(backend, target);
        obol::FrameResult result = renderer.render(scene);

        bool foundError = false;
        for (const obol::RenderDiagnostic & diagnostic : result.diagnostics) {
            foundError = foundError ||
                (diagnostic.severity == obol::DiagnosticSeverity::Error &&
                 diagnostic.message.find("OpenGL") != std::string::npos);
        }

        const bool pass = !result.success && foundError;
        runner.endTest(pass, pass ? "" : "Expected unsupported OpenGL callback diagnostic");
    }

    runner.startTest("v2 SceneIO writes and reads Inventor strings");
    {
        obol::Scene scene;
        scene.setCamera(obol::PerspectiveCamera{});
        scene.addDirectionalLight(obol::DirectionalLight{});
        scene.addPrimitive(obol::Primitive::Cube);

        std::string iv;
        const bool wrote = obol::SceneIO::writeInventorString(scene, iv);

        obol::Scene loaded;
        const bool read = obol::SceneIO::readInventorString(iv, loaded, &manager);
        SoSeparator * root = loaded.createLegacySceneGraph();
        const bool pass = wrote && read && !iv.empty() && root && root->getNumChildren() >= 1;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Inventor string round-trip failed through v2 SceneIO");
    }

    runner.startTest("v2 Picker returns a center hit on a cube");
    {
        obol::Scene scene;
        obol::PerspectiveCamera camera;
        camera.position = {0.0f, 0.0f, 5.0f};
        camera.target = {0.0f, 0.0f, 0.0f};
        scene.setCamera(camera);
        const obol::SceneObjectId cube = scene.addPrimitive(obol::Primitive::Cube);

        obol::PickRequest request;
        request.viewportWidth = 100;
        request.viewportHeight = 100;
        request.x = 50;
        request.y = 50;

        obol::PickResult result = obol::Picker::pick(scene, request);

        obol::PickRequest rayRequest;
        rayRequest.viewportWidth = 100;
        rayRequest.viewportHeight = 100;
        rayRequest.useWorldRay = true;
        rayRequest.rayOrigin = {0.0f, 0.0f, 5.0f};
        rayRequest.rayDirection = {0.0f, 0.0f, -1.0f};
        obol::PickResult rayResult = obol::Picker::pick(scene, rayRequest);

        const bool pass = result.hit &&
                          !result.hits.empty() &&
                          result.hits[0].onGeometry &&
                          result.hits[0].objectId == cube &&
                          rayResult.hit &&
                          !rayResult.hits.empty() &&
                          rayResult.hits[0].objectId == cube;
        runner.endTest(pass, pass ? "" : "Expected center pick to hit cube geometry with object id");
    }

    runner.startTest("v2 Scene supports point lights, spot lights, and text");
    {
        obol::Scene scene;
        scene.addPointLight(obol::PointLight{});
        scene.addSpotLight(obol::SpotLight{});

        obol::Text2D label;
        label.text = "Obol v2";
        label.fontSize = 18.0f;
        label.justification = obol::TextJustification::Center;
        scene.addText2D(label);

        SoSeparator * root = scene.createLegacySceneGraph();
        const bool pass = root && root->getNumChildren() == 3 && scene.objectCount() == 3;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Expected point light, spot light, and text bridge nodes");
    }

    runner.startTest("v2 Scene bridges 3D text with part colors");
    {
        obol::Text3D text;
        text.text = "Obol";
        text.fontName = "Times";
        text.fontSize = 0.25f;
        text.partColors = {
            {1.0f, 1.0f, 1.0f, 1.0f},
            {0.1f, 0.1f, 0.1f, 1.0f}
        };
        text.profile = {
            {0.0f, 0.0f},
            {0.25f, 0.25f},
            {1.25f, 0.25f},
            {1.5f, 0.0f}
        };

        obol::Scene scene;
        scene.addText3D(text);

        SoSeparator * root = scene.createLegacySceneGraph();
        SoSeparator * object = root && root->getNumChildren() == 1
            ? static_cast<SoSeparator *>(root->getChild(0))
            : nullptr;
        bool foundMaterialBinding = false;
        bool foundProfile = false;
        bool foundText3D = false;
        if (object) {
            for (int i = 0; i < object->getNumChildren(); ++i) {
                SoNode * child = object->getChild(i);
                if (child->isOfType(SoSeparator::getClassTypeId())) {
                    SoSeparator * textSep = static_cast<SoSeparator *>(child);
                    for (int j = 0; j < textSep->getNumChildren(); ++j) {
                        SoNode * textChild = textSep->getChild(j);
                        foundMaterialBinding = foundMaterialBinding ||
                            textChild->isOfType(SoMaterialBinding::getClassTypeId());
                        foundProfile = foundProfile ||
                            textChild->isOfType(SoLinearProfile::getClassTypeId());
                        foundText3D = foundText3D ||
                            textChild->isOfType(SoText3::getClassTypeId());
                    }
                }
            }
        }
        const bool pass = object && foundMaterialBinding && foundProfile && foundText3D;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Expected 3D text bridge nodes");
    }

    runner.startTest("v2 Scene bridges inline 2D textures");
    {
        obol::Texture2D texture;
        texture.image.width = 2;
        texture.image.height = 2;
        texture.image.format = obol::ImageFormat::RGBA;
        texture.image.pixels = {
            255, 0, 0, 255,
            0, 255, 0, 255,
            0, 0, 255, 255,
            255, 255, 255, 255
        };

        obol::Material material;
        material.baseColorTexture.reset(new obol::Texture2D(texture));

        obol::Scene scene;
        scene.addPrimitive(obol::Primitive::Cube, material);

        SoSeparator * root = scene.createLegacySceneGraph();
        SoSeparator * object = root && root->getNumChildren() == 1
            ? static_cast<SoSeparator *>(root->getChild(0))
            : nullptr;
        const bool pass = object && object->getNumChildren() == 4;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Expected transform, texture, material, shape nodes");
    }

    runner.startTest("v2 Scene bridges mesh texture coordinates");
    {
        obol::Mesh mesh;
        mesh.topology = obol::MeshTopology::Polygons;
        mesh.positions = {
            {-1.0f, -1.0f, 0.0f},
            { 1.0f, -1.0f, 0.0f},
            { 1.0f,  1.0f, 0.0f},
            {-1.0f,  1.0f, 0.0f}
        };
        mesh.texCoords = {
            {0.0f, 0.0f},
            {1.0f, 0.0f},
            {1.0f, 1.0f},
            {0.0f, 1.0f}
        };
        mesh.indices = {0, 1, 2, 3};
        mesh.texCoordIndices = {0, 1, 2, 3};
        mesh.faceVertexCounts = {4};

        obol::Scene scene;
        scene.addMesh(mesh);

        SoSeparator * root = scene.createLegacySceneGraph();
        SoSeparator * object = root && root->getNumChildren() == 1
            ? static_cast<SoSeparator *>(root->getChild(0))
            : nullptr;
        bool foundTexCoords = false;
        SoIndexedFaceSet * faceSet = nullptr;
        if (object) {
            for (int i = 0; i < object->getNumChildren(); ++i) {
                SoNode * child = object->getChild(i);
                if (child->isOfType(SoSeparator::getClassTypeId())) {
                    SoSeparator * meshSep = static_cast<SoSeparator *>(child);
                    for (int j = 0; j < meshSep->getNumChildren(); ++j) {
                        SoNode * meshChild = meshSep->getChild(j);
                        foundTexCoords = foundTexCoords ||
                            meshChild->isOfType(SoTextureCoordinate2::getClassTypeId());
                        if (meshChild->isOfType(SoIndexedFaceSet::getClassTypeId())) {
                            faceSet = static_cast<SoIndexedFaceSet *>(meshChild);
                        }
                    }
                }
            }
        }
        const bool pass = object &&
                          foundTexCoords &&
                          faceSet &&
                          faceSet->textureCoordIndex.getNum() == 5 &&
                          faceSet->textureCoordIndex[0] == 0 &&
                          faceSet->textureCoordIndex[1] == 1 &&
                          faceSet->textureCoordIndex[2] == 2 &&
                          faceSet->textureCoordIndex[3] == 3 &&
                          faceSet->textureCoordIndex[4] == SO_END_FACE_INDEX;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Expected mesh texture coordinate bridge");
    }

    runner.startTest("v2 Scene bridges wire polylines");
    {
        obol::Polyline polyline;
        polyline.lineWidth = 4.0f;
        polyline.points = {
            {-1.0f, 0.0f, 0.0f},
            { 0.0f, 1.0f, 0.0f},
            { 1.0f, 0.0f, 0.0f}
        };

        obol::Scene scene;
        scene.addPolyline(polyline);

        SoSeparator * root = scene.createLegacySceneGraph();
        SoSeparator * object = root && root->getNumChildren() == 1
            ? static_cast<SoSeparator *>(root->getChild(0))
            : nullptr;
        bool foundDrawStyle = false;
        bool foundLineSet = false;
        if (object) {
            for (int i = 0; i < object->getNumChildren(); ++i) {
                SoNode * child = object->getChild(i);
                if (child->isOfType(SoSeparator::getClassTypeId())) {
                    SoSeparator * lineSep = static_cast<SoSeparator *>(child);
                    for (int j = 0; j < lineSep->getNumChildren(); ++j) {
                        SoNode * lineChild = lineSep->getChild(j);
                        foundDrawStyle = foundDrawStyle ||
                            lineChild->isOfType(SoDrawStyle::getClassTypeId());
                        foundLineSet = foundLineSet ||
                            lineChild->isOfType(SoLineSet::getClassTypeId());
                    }
                }
            }
        }
        const bool pass = object && foundDrawStyle && foundLineSet;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Expected polyline bridge nodes");
    }

    runner.startTest("v2 Scene bridges polygon meshes with per-face attributes");
    {
        obol::Mesh mesh;
        mesh.topology = obol::MeshTopology::Polygons;
        mesh.positions = {
            {-1.0f, -1.0f, 0.0f},
            { 1.0f, -1.0f, 0.0f},
            { 1.0f,  1.0f, 0.0f},
            {-1.0f,  1.0f, 0.0f}
        };
        mesh.indices = {0, 1, 2, 3};
        mesh.faceVertexCounts = {4};
        mesh.faceNormals = {{0.0f, 0.0f, 1.0f}};
        mesh.faceColors = {{0.8f, 0.2f, 0.1f, 1.0f}};

        obol::Scene scene;
        scene.addMesh(mesh);

        SoSeparator * root = scene.createLegacySceneGraph();
        SoSeparator * object = root && root->getNumChildren() == 1
            ? static_cast<SoSeparator *>(root->getChild(0))
            : nullptr;
        bool foundNormalBinding = false;
        bool foundMaterialBinding = false;
        bool foundFaceSet = false;
        if (object) {
            for (int i = 0; i < object->getNumChildren(); ++i) {
                SoNode * child = object->getChild(i);
                foundNormalBinding = foundNormalBinding ||
                    child->isOfType(SoNormalBinding::getClassTypeId());
                foundMaterialBinding = foundMaterialBinding ||
                    child->isOfType(SoMaterialBinding::getClassTypeId());
                foundFaceSet = foundFaceSet ||
                    child->isOfType(SoIndexedFaceSet::getClassTypeId());
                if (child->isOfType(SoSeparator::getClassTypeId())) {
                    SoSeparator * meshSep = static_cast<SoSeparator *>(child);
                    for (int j = 0; j < meshSep->getNumChildren(); ++j) {
                        SoNode * meshChild = meshSep->getChild(j);
                        foundNormalBinding = foundNormalBinding ||
                            meshChild->isOfType(SoNormalBinding::getClassTypeId());
                        foundFaceSet = foundFaceSet ||
                            meshChild->isOfType(SoIndexedFaceSet::getClassTypeId());
                    }
                }
            }
        }
        const bool pass = object &&
                          foundNormalBinding &&
                          foundMaterialBinding &&
                          foundFaceSet;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Expected polygon mesh face attribute bridge");
    }

    runner.startTest("v2 Scene lowers triangle strips to legacy indexed faces");
    {
        obol::Mesh mesh;
        mesh.topology = obol::MeshTopology::TriangleStrips;
        mesh.positions = {
            {-1.0f, -1.0f, 0.0f},
            {-1.0f,  1.0f, 0.0f},
            { 1.0f, -1.0f, 0.0f},
            { 1.0f,  1.0f, 0.0f},
            { 2.0f, -1.0f, 0.0f},
            { 2.0f,  1.0f, 0.0f}
        };
        mesh.indices = {0, 1, 2, 3, 4, 5};
        mesh.stripVertexCounts = {4, 2};
        mesh.faceColors = {{0.2f, 0.3f, 0.9f, 1.0f}};

        obol::Scene scene;
        scene.addMesh(mesh);

        SoSeparator * root = scene.createLegacySceneGraph();
        SoSeparator * object = root && root->getNumChildren() == 1
            ? static_cast<SoSeparator *>(root->getChild(0))
            : nullptr;
        SoIndexedFaceSet * faceSet = nullptr;
        bool foundMaterialBinding = false;
        if (object) {
            for (int i = 0; i < object->getNumChildren(); ++i) {
                SoNode * child = object->getChild(i);
                foundMaterialBinding = foundMaterialBinding ||
                    child->isOfType(SoMaterialBinding::getClassTypeId());
                if (child->isOfType(SoSeparator::getClassTypeId())) {
                    SoSeparator * meshSep = static_cast<SoSeparator *>(child);
                    for (int j = 0; j < meshSep->getNumChildren(); ++j) {
                        SoNode * meshChild = meshSep->getChild(j);
                        if (meshChild->isOfType(SoIndexedFaceSet::getClassTypeId())) {
                            faceSet = static_cast<SoIndexedFaceSet *>(meshChild);
                        }
                    }
                }
            }
        }

        const bool pass = object &&
                          foundMaterialBinding &&
                          faceSet &&
                          faceSet->coordIndex.getNum() == 8 &&
                          faceSet->coordIndex[0] == 0 &&
                          faceSet->coordIndex[1] == 1 &&
                          faceSet->coordIndex[2] == 2 &&
                          faceSet->coordIndex[3] == SO_END_FACE_INDEX &&
                          faceSet->coordIndex[4] == 2 &&
                          faceSet->coordIndex[5] == 1 &&
                          faceSet->coordIndex[6] == 3 &&
                          faceSet->coordIndex[7] == SO_END_FACE_INDEX;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Expected triangle strip legacy face fallback");
    }

    runner.startTest("v2 Scene lowers quad grids to legacy indexed faces");
    {
        obol::Mesh mesh;
        mesh.topology = obol::MeshTopology::QuadGrid;
        mesh.gridVertexRows = 2;
        mesh.gridVertexColumns = 2;
        mesh.positions = {
            {-1.0f,  1.0f, 0.0f},
            { 1.0f,  1.0f, 0.0f},
            {-1.0f, -1.0f, 0.0f},
            { 1.0f, -1.0f, 0.0f}
        };

        obol::Scene scene;
        scene.addMesh(mesh);

        SoSeparator * root = scene.createLegacySceneGraph();
        SoSeparator * object = root && root->getNumChildren() == 1
            ? static_cast<SoSeparator *>(root->getChild(0))
            : nullptr;
        SoIndexedFaceSet * faceSet = nullptr;
        if (object) {
            for (int i = 0; i < object->getNumChildren(); ++i) {
                SoNode * child = object->getChild(i);
                if (child->isOfType(SoSeparator::getClassTypeId())) {
                    SoSeparator * meshSep = static_cast<SoSeparator *>(child);
                    for (int j = 0; j < meshSep->getNumChildren(); ++j) {
                        SoNode * meshChild = meshSep->getChild(j);
                        if (meshChild->isOfType(SoIndexedFaceSet::getClassTypeId())) {
                            faceSet = static_cast<SoIndexedFaceSet *>(meshChild);
                        }
                    }
                }
            }
        }

        const bool pass = object &&
                          faceSet &&
                          faceSet->coordIndex.getNum() == 5 &&
                          faceSet->coordIndex[0] == 0 &&
                          faceSet->coordIndex[1] == 2 &&
                          faceSet->coordIndex[2] == 3 &&
                          faceSet->coordIndex[3] == 1 &&
                          faceSet->coordIndex[4] == SO_END_FACE_INDEX;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Expected quad grid legacy face fallback");
    }

    runner.startTest("v2 Scene bridges indexed mesh color bindings");
    {
        obol::Mesh faceMesh;
        faceMesh.topology = obol::MeshTopology::Polygons;
        faceMesh.positions = {
            {-1.0f, -1.0f, 0.0f},
            { 0.0f,  1.0f, 0.0f},
            { 1.0f, -1.0f, 0.0f},
            { 2.0f,  1.0f, 0.0f}
        };
        faceMesh.indices = {0, 1, 2, 1, 3, 2};
        faceMesh.faceVertexCounts = {3, 3};
        faceMesh.faceColors = {
            {1.0f, 0.0f, 0.0f, 1.0f},
            {0.0f, 0.0f, 1.0f, 1.0f}
        };
        faceMesh.faceColorIndices = {1, 0};

        obol::Mesh vertexMesh;
        vertexMesh.topology = obol::MeshTopology::Polygons;
        vertexMesh.positions = {
            {-1.0f, -1.0f, 0.0f},
            { 1.0f, -1.0f, 0.0f},
            { 1.0f,  1.0f, 0.0f},
            {-1.0f,  1.0f, 0.0f}
        };
        vertexMesh.indices = {0, 1, 2, 3};
        vertexMesh.faceVertexCounts = {4};
        vertexMesh.vertexColors = {
            {1.0f, 0.0f, 0.0f, 1.0f},
            {0.0f, 1.0f, 0.0f, 1.0f},
            {0.0f, 0.0f, 1.0f, 1.0f},
            {1.0f, 1.0f, 0.0f, 1.0f}
        };

        obol::Scene faceScene;
        faceScene.addMesh(faceMesh);
        SoSeparator * faceRoot = faceScene.createLegacySceneGraph();
        SoSeparator * faceObject = faceRoot && faceRoot->getNumChildren() == 1
            ? static_cast<SoSeparator *>(faceRoot->getChild(0))
            : nullptr;
        SoMaterialBinding * faceBinding = nullptr;
        SoIndexedFaceSet * faceSet = nullptr;
        if (faceObject) {
            for (int i = 0; i < faceObject->getNumChildren(); ++i) {
                SoNode * child = faceObject->getChild(i);
                if (child->isOfType(SoMaterialBinding::getClassTypeId())) {
                    faceBinding = static_cast<SoMaterialBinding *>(child);
                } else if (child->isOfType(SoSeparator::getClassTypeId())) {
                    SoSeparator * meshSep = static_cast<SoSeparator *>(child);
                    for (int j = 0; j < meshSep->getNumChildren(); ++j) {
                        SoNode * meshChild = meshSep->getChild(j);
                        if (meshChild->isOfType(SoIndexedFaceSet::getClassTypeId())) {
                            faceSet = static_cast<SoIndexedFaceSet *>(meshChild);
                        }
                    }
                }
            }
        }

        obol::Scene vertexScene;
        vertexScene.addMesh(vertexMesh);
        SoSeparator * vertexRoot = vertexScene.createLegacySceneGraph();
        SoSeparator * vertexObject = vertexRoot && vertexRoot->getNumChildren() == 1
            ? static_cast<SoSeparator *>(vertexRoot->getChild(0))
            : nullptr;
        SoMaterialBinding * vertexBinding = nullptr;
        SoIndexedFaceSet * vertexSet = nullptr;
        if (vertexObject) {
            for (int i = 0; i < vertexObject->getNumChildren(); ++i) {
                SoNode * child = vertexObject->getChild(i);
                if (child->isOfType(SoMaterialBinding::getClassTypeId())) {
                    vertexBinding = static_cast<SoMaterialBinding *>(child);
                } else if (child->isOfType(SoSeparator::getClassTypeId())) {
                    SoSeparator * meshSep = static_cast<SoSeparator *>(child);
                    for (int j = 0; j < meshSep->getNumChildren(); ++j) {
                        SoNode * meshChild = meshSep->getChild(j);
                        if (meshChild->isOfType(SoIndexedFaceSet::getClassTypeId())) {
                            vertexSet = static_cast<SoIndexedFaceSet *>(meshChild);
                        }
                    }
                }
            }
        }

        const bool pass =
            faceBinding &&
            faceBinding->value.getValue() == SoMaterialBinding::PER_FACE_INDEXED &&
            faceSet &&
            faceSet->materialIndex.getNum() == 2 &&
            faceSet->materialIndex[0] == 1 &&
            faceSet->materialIndex[1] == 0 &&
            vertexBinding &&
            vertexBinding->value.getValue() == SoMaterialBinding::PER_VERTEX_INDEXED &&
            vertexSet &&
            vertexSet->materialIndex.getNum() == 5 &&
            vertexSet->materialIndex[0] == 0 &&
            vertexSet->materialIndex[1] == 1 &&
            vertexSet->materialIndex[2] == 2 &&
            vertexSet->materialIndex[3] == 3 &&
            vertexSet->materialIndex[4] == SO_END_FACE_INDEX;
        if (faceRoot) faceRoot->unref();
        if (vertexRoot) vertexRoot->unref();
        runner.endTest(pass, pass ? "" : "Expected indexed mesh color bindings");
    }

    runner.startTest("v2 Scene supports nested transform groups");
    {
        obol::Scene scene;
        obol::Transform baseTransform;
        baseTransform.translation = {2.0f, 0.0f, 0.0f};
        const obol::SceneGroupId base = scene.addGroup(baseTransform);

        obol::Transform childTransform;
        childTransform.translation = {0.0f, 1.0f, 0.0f};
        const obol::SceneGroupId child = scene.addGroup(childTransform, base);
        childTransform.translation = {0.0f, 2.0f, 0.0f};
        const bool movedChild = scene.setGroupTransform(child, childTransform);
        const bool rejectedInvalidGroup =
            !scene.setGroupTransform(obol::InvalidSceneGroupId, childTransform);

        const obol::SceneObjectId cube =
            scene.addPrimitive(obol::Primitive::Cube,
                               obol::Material{},
                               obol::Transform{},
                               obol::PrimitiveOptions{},
                               child);
        obol::Transform objectTransform;
        objectTransform.translation = {0.0f, 0.0f, 0.0f};
        const bool movedObject = scene.setObjectTransform(cube, objectTransform);
        const bool rejectedInvalidObject =
            !scene.setObjectTransform(obol::InvalidSceneObjectId, objectTransform);

        SoSeparator * root = scene.createLegacySceneGraph();
        SoSeparator * baseSep = root && root->getNumChildren() == 1
            ? static_cast<SoSeparator *>(root->getChild(0))
            : nullptr;
        SoSeparator * childSep = baseSep && baseSep->getNumChildren() == 2
            ? static_cast<SoSeparator *>(baseSep->getChild(1))
            : nullptr;
        SoSeparator * cubeSep = childSep && childSep->getNumChildren() == 2
            ? static_cast<SoSeparator *>(childSep->getChild(1))
            : nullptr;

        obol::PickRequest request;
        request.viewportWidth = 100;
        request.viewportHeight = 100;
        request.useWorldRay = true;
        request.rayOrigin = {2.0f, 2.0f, 5.0f};
        request.rayDirection = {0.0f, 0.0f, -1.0f};
        obol::PickResult pick = obol::Picker::pick(scene, request);

        const bool pass = scene.groupCount() == 2 &&
                          scene.objectCount() == 1 &&
                          cube == 1 &&
                          movedChild &&
                          movedObject &&
                          rejectedInvalidGroup &&
                          rejectedInvalidObject &&
                          baseSep &&
                          childSep &&
                          cubeSep &&
                          cubeSep->getNumChildren() == 3 &&
                          pick.hit &&
                          !pick.hits.empty() &&
                          pick.hits[0].objectId == cube;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Expected nested groups to bridge and pick correctly");
    }

    runner.startTest("v2 Scene bridges modern CAD assemblies");
    {
        obol::CadAssembly cad;
        cad.setDrawMode(obol::CadDrawMode::Wireframe);
        cad.setPickMode(obol::CadPickMode::Hybrid);
        cad.setLodEnabled(true);

        obol::PartId part = obol::CadIdBuilder::hash128(std::string("v2_cad_part"));
        obol::PartGeometry geometry;
        geometry.wire = obol::WireRep{};
        geometry.wire->bounds.setBounds(SbVec3f(-1.0f, -1.0f, -1.0f),
                                        SbVec3f(1.0f, 1.0f, 1.0f));
        obol::WirePolyline edge;
        edge.points.push_back(SbVec3f(-1.0f, 0.0f, 0.0f));
        edge.points.push_back(SbVec3f(1.0f, 0.0f, 0.0f));
        geometry.wire->polylines.push_back(edge);
        geometry.shaded = obol::TriMesh{};
        geometry.shaded->positions.push_back(SbVec3f(-1.0f, -1.0f, 0.0f));
        geometry.shaded->positions.push_back(SbVec3f(1.0f, -1.0f, 0.0f));
        geometry.shaded->positions.push_back(SbVec3f(0.0f, 1.0f, 0.0f));
        geometry.shaded->indices = {0, 1, 2};
        geometry.shaded->bounds.setBounds(SbVec3f(-1.0f, -1.0f, -0.1f),
                                          SbVec3f(1.0f, 1.0f, 0.1f));
        cad.upsertPart(part, geometry);

        obol::InstanceRecord record;
        record.part = part;
        record.parent = obol::CadIdBuilder::Root();
        record.childName = "v2_cad_instance";
        record.localToRoot.makeIdentity();
        const obol::InstanceId instance = cad.upsertInstanceAuto(record);
        cad.setSelectedInstances({instance});

        obol::Scene scene;
        obol::PerspectiveCamera camera;
        camera.position = {0.0f, 0.0f, 5.0f};
        camera.target = {0.0f, 0.0f, 0.0f};
        scene.setCamera(camera);
        const obol::SceneObjectId object = scene.addCadAssembly(cad);

        SoSeparator * root = scene.createLegacySceneGraph();
        SoSeparator * objectSep = root && root->getNumChildren() == 2
            ? static_cast<SoSeparator *>(root->getChild(1))
            : nullptr;
        SoNode * cadNode = objectSep && objectSep->getNumChildren() == 2
            ? objectSep->getChild(1)
            : nullptr;
        const bool pass = object == 1 &&
                          cad.partCount() == 1 &&
                          cad.instanceCount() == 1 &&
                          cadNode &&
                          cadNode->isOfType(SoCADAssembly::getClassTypeId());
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Expected v2 CAD assembly to bridge to SoCADAssembly");
    }

    return runner.getSummary();
}
