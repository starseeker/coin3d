/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
\**************************************************************************/

#include "../test_utils.h"
#include "../../examples/modern_api_example.h"

#include <Obol/Obol.h>

#include <Obol/compat/cad/SoCADAssembly.h>

#include <Inventor/SoDB.h>
#include <Inventor/SoInteraction.h>
#include <Inventor/nodes/SoCallback.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <Inventor/nodes/SoLinearProfile.h>
#include <Inventor/nodes/SoLineSet.h>
#include <Inventor/nodes/SoLightModel.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoMaterialBinding.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/nodes/SoPointSet.h>
#include <Inventor/nodes/SoQuadMesh.h>
#include <Inventor/nodes/SoText3.h>
#include <Inventor/nodes/SoTextureCoordinate2.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoTriangleStripSet.h>

#include <cmath>
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

class PacketOnlyBackend : public obol::RenderBackend {
public:
    obol::RenderBackendKind kind() const override { return obol::RenderBackendKind::CPU; }
    const char * name() const override { return "packet-only-test"; }

    obol::RenderCapabilities capabilities() const override
    {
        obol::RenderCapabilities caps;
        caps.known = true;
        caps.backendKind = kind();
        caps.backendName = name();
        return caps;
    }

    bool renderPacket(const obol::ScenePacket & packet,
                      const obol::RenderTarget & target,
                      const obol::RenderOptions &,
                      const obol::Color &,
                      std::vector<unsigned char> & pixels,
                      std::vector<obol::RenderDiagnostic> &) override
    {
        lastObjectCount = packet.objects.size();
        lastGroupCount = packet.groups.size();
        lastCameraKind = packet.camera.kind;
        const size_t components =
            target.pixelFormat == obol::PixelFormat::Luminance ? 1 :
            target.pixelFormat == obol::PixelFormat::LuminanceAlpha ? 2 :
            target.pixelFormat == obol::PixelFormat::RGB ? 3 : 4;
        pixels.assign(static_cast<size_t>(target.width) *
                          static_cast<size_t>(target.height) *
                          components,
                      0);
        for (size_t i = 0; i < pixels.size(); i += components) {
            pixels[i] = 25;
            if (components > 1) pixels[i + 1] = 50;
            if (components > 2) pixels[i + 2] = 75;
            if (components > 3) pixels[i + 3] = 255;
        }
        return true;
    }

    size_t lastObjectCount = 0;
    size_t lastGroupCount = 0;
    obol::SceneCameraKind lastCameraKind = obol::SceneCameraKind::None;
};

class CapabilityOnlyBackend : public obol::RenderBackend {
public:
    obol::RenderBackendKind kind() const override { return obol::RenderBackendKind::Custom; }
    const char * name() const override { return "capability-only-test"; }

    obol::RenderCapabilities capabilities() const override
    {
        obol::RenderCapabilities caps;
        caps.known = true;
        caps.backendKind = kind();
        caps.backendName = name();
        return caps;
    }
};

SoSeparator *
legacySceneGraph(const obol::Scene & scene)
{
    return static_cast<SoSeparator *>(scene.createLegacySceneGraph());
}

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

template <typename NodeT>
NodeT * findFirstNodeOfType(SoNode * root)
{
    if (!root) return nullptr;
    if (root->isOfType(NodeT::getClassTypeId())) {
        return static_cast<NodeT *>(root);
    }
    SoGroup * group = dynamic_cast<SoGroup *>(root);
    if (!group) return nullptr;
    for (int i = 0; i < group->getNumChildren(); ++i) {
        NodeT * found = findFirstNodeOfType<NodeT>(group->getChild(i));
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

    runner.startTest("v2 ObservableValue reports field changes");
    {
        obol::ObservableValue<int> value(3);
        int callbackCount = 0;
        int previousValue = 0;
        int currentValue = 0;
        std::string fieldName;

        const obol::ObserverId observer = value.addObserver(
            [&](const obol::ValueChange<int> & change) {
                ++callbackCount;
                previousValue = change.previous;
                currentValue = change.value;
                fieldName = change.fieldName;
            });

        value.set(7, "width");
        const bool removed = value.removeObserver(observer);
        value.set(9, "height");

        const bool pass =
            observer != 0 &&
            callbackCount == 1 &&
            previousValue == 3 &&
            currentValue == 7 &&
            fieldName == "width" &&
            value.get() == 9 &&
            removed &&
            value.observerCount() == 0;
        runner.endTest(pass, pass ? "" : "ObservableValue did not report expected field change");
    }

    runner.startTest("v2 Time formats UTC clock values");
    {
        const obol::Time start = obol::Time::fromUnixSeconds(946684800.0);
        const obol::Time current =
            start + obol::TimeSpan::fromMilliseconds(3661000.0);
        const obol::TimeSpan elapsed = current - start;
        const std::string formatted = current.formatUTC();

        const bool pass =
            formatted == "Saturday, 01/01/00 01:01:01 AM" &&
            std::fabs(elapsed.seconds - 3661.0) < 1.0e-5 &&
            std::fabs((current - obol::TimeSpan::fromSeconds(1.0))
                          .secondsSinceUnixEpoch() -
                      946688460.0) < 1.0e-5;
        runner.endTest(pass, pass ? "" : "Time formatting or duration math failed");
    }

    runner.startTest("v2 Scene creates a legacy graph bridge");
    {
        obol::Scene scene;
        scene.setCamera(obol::PerspectiveCamera{});
        scene.addDirectionalLight(obol::DirectionalLight{});
        obol::Material red;
        red.baseColor = {1.0f, 0.0f, 0.0f, 1.0f};
        scene.addPrimitive(obol::Primitive::Cone, red);

        SoSeparator * root = legacySceneGraph(scene);
        const bool pass = root && root->getNumChildren() == 3 && scene.objectCount() == 2;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Scene did not produce expected bridge graph");
    }

    runner.startTest("v2 CameraFraming frames scenes without public Inventor camera state");
    {
        obol::Scene scene;
        scene.addPrimitive(obol::Primitive::Cube);

        obol::ViewAllRequest request;
        request.viewportWidth = 320;
        request.viewportHeight = 240;
        request.position = {0.0f, 0.0f, 5.0f};
        request.target = {0.0f, 0.0f, 0.0f};
        const obol::PerspectiveCamera camera =
            obol::CameraFraming::viewAllPerspective(scene, request);

        const bool pass =
            camera.nearDistance > 0.0f &&
            camera.farDistance > camera.nearDistance &&
            camera.verticalFieldOfViewRadians > 0.0f &&
            std::fabs(camera.position.z) > 0.0f &&
            std::fabs(camera.target.x) < 1.0e-4f &&
            std::fabs(camera.target.y) < 1.0e-4f;
        runner.endTest(pass, pass ? "" : "CameraFraming did not produce a usable v2 camera");
    }

    runner.startTest("v2 CameraFraming orbits perspective cameras");
    {
        obol::CameraOrbitRequest request;
        request.camera.position = {0.0f, 0.0f, 10.0f};
        request.camera.target = {0.0f, 0.0f, 0.0f};
        request.azimuthRadians = 1.57079632679f;
        request.elevationRadians = 0.0f;

        const obol::PerspectiveCamera camera = obol::CameraFraming::orbit(request);

        const bool pass =
            std::fabs(camera.position.x - 10.0f) < 1.0e-4f &&
            std::fabs(camera.position.y - 0.0f) < 1.0e-4f &&
            std::fabs(camera.position.z - 0.0f) < 1.0e-4f &&
            std::fabs(camera.target.x - 0.0f) < 1.0e-4f &&
            std::fabs(camera.target.y - 0.0f) < 1.0e-4f &&
            std::fabs(camera.target.z - 0.0f) < 1.0e-4f &&
            std::fabs(camera.up.y - 1.0f) < 1.0e-4f;
        runner.endTest(pass, pass ? "" : "CameraFraming orbit did not preserve expected camera state");
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

    runner.startTest("v2 Scene captures backend-neutral scene packets");
    {
        obol::Scene scene;
        obol::OrthographicCamera camera;
        camera.height = 4.0f;
        scene.setCamera(camera);

        obol::Transform groupTransform;
        groupTransform.translation = {1.0f, 2.0f, 3.0f};
        const obol::SceneGroupId group = scene.addGroup(groupTransform);
        obol::Transform childGroupTransform;
        childGroupTransform.translation = {4.0f, 0.0f, 0.0f};
        const obol::SceneGroupId childGroup = scene.addGroup(childGroupTransform,
                                                             group);

        obol::Material material;
        material.baseColor = {0.2f, 0.4f, 0.6f, 1.0f};

        obol::Mesh mesh;
        mesh.positions = {{0.0f, 0.0f, 0.0f},
                          {1.0f, 0.0f, 0.0f},
                          {0.0f, 1.0f, 0.0f}};
        mesh.indices = {0, 1, 2};
        obol::Transform meshTransform;
        meshTransform.translation = {0.0f, 5.0f, 0.0f};
        const obol::SceneObjectId meshId = scene.addMesh(mesh, material,
                                                         meshTransform,
                                                         childGroup);

        const obol::SceneObjectId removed =
            scene.addPointLight(obol::PointLight{}, group);
        scene.removeObject(removed);

        obol::OpenGLCallback callback;
        callback.draw = testOpenGLDrawCallback;
        callback.label = "packet-callback";
        const obol::SceneObjectId callbackId = scene.addOpenGLCallback(callback,
                                                                       group);

        SoSeparator * legacyRoot = new SoSeparator;
        legacyRoot->ref();
        legacyRoot->addChild(new SoCube);
        const obol::SceneObjectId legacyId =
            scene.addLegacySceneGraph(legacyRoot, obol::Transform{}, group);
        legacyRoot->unref();

        const obol::ScenePacket packet = scene.capturePacket();
        const obol::SceneObjectRecord * meshRecord = nullptr;
        const obol::SceneObjectRecord * callbackRecord = nullptr;
        const obol::SceneObjectRecord * legacyRecord = nullptr;
        for (const obol::SceneObjectRecord & object : packet.objects) {
            if (object.id == meshId) meshRecord = &object;
            if (object.id == callbackId) callbackRecord = &object;
            if (object.id == legacyId) legacyRecord = &object;
        }

        const obol::SceneGroupRecord * childGroupRecord = nullptr;
        for (const obol::SceneGroupRecord & groupRecord : packet.groups) {
            if (groupRecord.id == childGroup) childGroupRecord = &groupRecord;
        }
        std::vector<obol::PacketTriangle> packetTriangles;
        std::vector<obol::PacketGeometryDiagnostic> packetDiagnostics;
        std::vector<obol::PacketGeometryDiagnostic> supportDiagnostics;
        const obol::PacketGeometrySupport packetSupport =
            obol::inspectPacketGeometrySupport(packet, &supportDiagnostics);
        const bool trianglesLowered =
            obol::collectPacketTriangles(packet, packetTriangles,
                                         &packetDiagnostics);

        const bool pass =
            packet.camera.kind == obol::SceneCameraKind::Orthographic &&
            std::fabs(packet.camera.orthographic.height - 4.0f) < 1.0e-5f &&
            packet.groups.size() == 2 &&
            packet.groups[0].id == group &&
            packet.groups[0].parent == obol::RootSceneGroupId &&
            std::fabs(packet.groups[0].transform.translation.x - 1.0f) < 1.0e-5f &&
            std::fabs(packet.groups[0].localToWorld.values[12] - 1.0f) < 1.0e-5f &&
            std::fabs(packet.groups[0].localToWorld.values[13] - 2.0f) < 1.0e-5f &&
            std::fabs(packet.groups[0].localToWorld.values[14] - 3.0f) < 1.0e-5f &&
            childGroupRecord &&
            childGroupRecord->parent == group &&
            std::fabs(childGroupRecord->localToWorld.values[12] - 5.0f) < 1.0e-5f &&
            std::fabs(childGroupRecord->localToWorld.values[13] - 2.0f) < 1.0e-5f &&
            std::fabs(childGroupRecord->localToWorld.values[14] - 3.0f) < 1.0e-5f &&
            packet.objects.size() == 3 &&
            !packet.hasLegacyFallbackRoot &&
            meshRecord &&
            meshRecord->type == obol::SceneObjectType::Mesh &&
            meshRecord->category == obol::SceneObjectCategory::Geometry &&
            meshRecord->parent == childGroup &&
            std::fabs(meshRecord->localToWorld.values[12] - 5.0f) < 1.0e-5f &&
            std::fabs(meshRecord->localToWorld.values[13] - 7.0f) < 1.0e-5f &&
            std::fabs(meshRecord->localToWorld.values[14] - 3.0f) < 1.0e-5f &&
            meshRecord->mesh.positions.size() == 3 &&
            std::fabs(meshRecord->material.baseColor.g - 0.4f) < 1.0e-5f &&
            callbackRecord &&
            callbackRecord->type == obol::SceneObjectType::OpenGLCallback &&
            callbackRecord->category == obol::SceneObjectCategory::BackendNative &&
            callbackRecord->openGLCallback.draw == testOpenGLDrawCallback &&
            legacyRecord &&
            legacyRecord->type == obol::SceneObjectType::LegacySceneGraph &&
            legacyRecord->hasLegacySceneGraph &&
            packetSupport.portableGeometryObjects == 1 &&
            packetSupport.backendNativeObjects == 1 &&
            packetSupport.legacyObjects == 1 &&
            packetSupport.unsupportedObjects == 0 &&
            supportDiagnostics.size() == 2 &&
            trianglesLowered &&
            packetDiagnostics.empty() &&
            packetTriangles.size() == 1 &&
            packetTriangles[0].objectId == meshId &&
            std::fabs(packetTriangles[0].vertices[0].position.x - 5.0f) < 1.0e-5f &&
            std::fabs(packetTriangles[0].vertices[0].position.y - 7.0f) < 1.0e-5f &&
            std::fabs(packetTriangles[0].vertices[0].position.z - 3.0f) < 1.0e-5f &&
            std::fabs(packetTriangles[0].vertices[1].position.x - 6.0f) < 1.0e-5f &&
            std::fabs(packetTriangles[0].vertices[2].position.y - 8.0f) < 1.0e-5f;
        runner.endTest(pass, pass ? "" : "ScenePacket did not preserve expected v2 scene records");
    }

    runner.startTest("v2 TransformDragger applies clamped axis translation");
    {
        obol::Scene scene;
        obol::Transform start;
        start.translation = {1.0f, 2.0f, 3.0f};
        const obol::SceneObjectId cube =
            scene.addPrimitive(obol::Primitive::Cube, obol::Material{}, start);

        obol::AxisDragRequest request;
        request.target = cube;
        request.startTransform = start;
        request.axis = {0.0f, 3.0f, 0.0f};
        request.distance = 5.0f;
        request.clamp = true;
        request.minimumDistance = -2.0f;
        request.maximumDistance = 3.0f;

        obol::AxisDragResult result;
        const bool applied =
            obol::TransformDragger::applyAxisTranslation(scene, request, &result);

        SoSeparator * root = legacySceneGraph(scene);
        SoSeparator * object = findNamedSeparator(root, "ObolSceneObject_1");
        SoTransform * transform = findFirstNodeOfType<SoTransform>(object);
        const SbVec3f translation = transform
            ? transform->translation.getValue()
            : SbVec3f(0.0f, 0.0f, 0.0f);

        const bool pass =
            applied &&
            result.valid &&
            result.target == cube &&
            std::fabs(result.distance - 3.0f) < 1.0e-5f &&
            std::fabs(result.delta.y - 3.0f) < 1.0e-5f &&
            transform &&
            std::fabs(translation[0] - 1.0f) < 1.0e-5f &&
            std::fabs(translation[1] - 5.0f) < 1.0e-5f &&
            std::fabs(translation[2] - 3.0f) < 1.0e-5f;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Axis drag did not update target transform");
    }

    runner.startTest("v2 TransformDragger computes math-only axis translation");
    {
        obol::AxisDragRequest request;
        request.startTransform.translation = {1.0f, 0.0f, -1.0f};
        request.axis = {0.0f, 0.0f, 4.0f};
        request.distance = 2.5f;

        const obol::AxisDragResult result =
            obol::TransformDragger::translateOnAxis(request);

        const bool pass =
            result.valid &&
            result.target == obol::InvalidSceneObjectId &&
            std::fabs(result.delta.z - 2.5f) < 1.0e-5f &&
            std::fabs(result.transform.translation.x - 1.0f) < 1.0e-5f &&
            std::fabs(result.transform.translation.y - 0.0f) < 1.0e-5f &&
            std::fabs(result.transform.translation.z - 1.5f) < 1.0e-5f;
        runner.endTest(pass, pass ? "" : "Axis drag math-only result was incorrect");
    }

    runner.startTest("v2 TransformDragger composes sequential axis translations");
    {
        obol::Transform transform;

        obol::AxisDragRequest xRequest;
        xRequest.startTransform = transform;
        xRequest.axis = {1.0f, 0.0f, 0.0f};
        xRequest.distance = 4.0f;
        obol::AxisDragResult xResult =
            obol::TransformDragger::translateOnAxis(xRequest);

        obol::AxisDragRequest yRequest;
        yRequest.startTransform = xResult.transform;
        yRequest.axis = {0.0f, 1.0f, 0.0f};
        yRequest.distance = 2.0f;
        obol::AxisDragResult yResult =
            obol::TransformDragger::translateOnAxis(yRequest);

        obol::AxisDragRequest zRequest;
        zRequest.startTransform = yResult.transform;
        zRequest.axis = {0.0f, 0.0f, 1.0f};
        zRequest.distance = -3.0f;
        obol::AxisDragResult zResult =
            obol::TransformDragger::translateOnAxis(zRequest);

        const bool pass =
            xResult.valid &&
            yResult.valid &&
            zResult.valid &&
            std::fabs(zResult.transform.translation.x - 4.0f) < 1.0e-5f &&
            std::fabs(zResult.transform.translation.y - 2.0f) < 1.0e-5f &&
            std::fabs(zResult.transform.translation.z + 3.0f) < 1.0e-5f;
        runner.endTest(pass, pass ? "" : "Sequential axis drag composition was incorrect");
    }

    runner.startTest("v2 TransformDragger projects translation onto planes");
    {
        obol::PlaneDragRequest request;
        request.startTransform.translation = {1.0f, 1.0f, 1.0f};
        request.delta = {2.0f, 3.0f, 4.0f};
        request.planeNormal = {0.0f, 0.0f, 5.0f};

        const obol::TranslationResult result =
            obol::TransformDragger::translateOnPlane(request);

        const bool pass =
            result.valid &&
            result.target == obol::InvalidSceneObjectId &&
            std::fabs(result.delta.x - 2.0f) < 1.0e-5f &&
            std::fabs(result.delta.y - 3.0f) < 1.0e-5f &&
            std::fabs(result.delta.z - 0.0f) < 1.0e-5f &&
            std::fabs(result.transform.translation.x - 3.0f) < 1.0e-5f &&
            std::fabs(result.transform.translation.y - 4.0f) < 1.0e-5f &&
            std::fabs(result.transform.translation.z - 1.0f) < 1.0e-5f;
        runner.endTest(pass, pass ? "" : "Plane drag projection was incorrect");
    }

    runner.startTest("v2 TransformDragger applies bounded free translation");
    {
        obol::Scene scene;
        obol::Transform start;
        start.translation = {1.0f, 1.0f, 1.0f};
        const obol::SceneObjectId cube =
            scene.addPrimitive(obol::Primitive::Cube, obol::Material{}, start);

        obol::FreeDragRequest request;
        request.target = cube;
        request.startTransform = start;
        request.delta = {5.0f, -5.0f, 2.0f};
        request.bounds.enabled = true;
        request.bounds.minimum = {0.0f, 0.0f, 0.0f};
        request.bounds.maximum = {3.0f, 3.0f, 2.0f};

        obol::TranslationResult result;
        const bool applied =
            obol::TransformDragger::applyFreeTranslation(scene, request, &result);

        SoSeparator * root = legacySceneGraph(scene);
        SoSeparator * object = findNamedSeparator(root, "ObolSceneObject_1");
        SoTransform * transform = findFirstNodeOfType<SoTransform>(object);
        const SbVec3f translation = transform
            ? transform->translation.getValue()
            : SbVec3f(0.0f, 0.0f, 0.0f);

        const bool pass =
            applied &&
            result.valid &&
            result.target == cube &&
            std::fabs(result.delta.x - 2.0f) < 1.0e-5f &&
            std::fabs(result.delta.y + 1.0f) < 1.0e-5f &&
            std::fabs(result.delta.z - 1.0f) < 1.0e-5f &&
            transform &&
            std::fabs(translation[0] - 3.0f) < 1.0e-5f &&
            std::fabs(translation[1] - 0.0f) < 1.0e-5f &&
            std::fabs(translation[2] - 2.0f) < 1.0e-5f;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Bounded free translation did not update target transform");
    }

    runner.startTest("v2 TransformDragger applies clamped axis rotation");
    {
        obol::Scene scene;
        obol::Transform start;
        start.translation = {1.0f, 2.0f, 3.0f};
        const obol::SceneObjectId cube =
            scene.addPrimitive(obol::Primitive::Cube, obol::Material{}, start);

        obol::AxisRotationRequest request;
        request.target = cube;
        request.startTransform = start;
        request.axis = {0.0f, 0.0f, 5.0f};
        request.angleRadians = 2.0f;
        request.clamp = true;
        request.minimumAngleRadians = -1.0f;
        request.maximumAngleRadians = 1.25f;

        obol::AxisRotationResult result;
        const bool applied =
            obol::TransformDragger::applyAxisRotation(scene, request, &result);

        SoSeparator * root = legacySceneGraph(scene);
        SoSeparator * object = findNamedSeparator(root, "ObolSceneObject_1");
        SoTransform * transform = findFirstNodeOfType<SoTransform>(object);

        SbVec3f axis(0.0f, 0.0f, 0.0f);
        float angle = 0.0f;
        if (transform) {
            transform->rotation.getValue().getValue(axis, angle);
        }

        const bool pass =
            applied &&
            result.valid &&
            result.target == cube &&
            std::fabs(result.angleRadians - 1.25f) < 1.0e-5f &&
            std::fabs(result.axis.z - 1.0f) < 1.0e-5f &&
            transform &&
            std::fabs(axis[2] - 1.0f) < 1.0e-5f &&
            std::fabs(angle - 1.25f) < 1.0e-5f;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Axis rotation did not update target transform");
    }

    runner.startTest("v2 TransformDragger creates portable translate-axis overlays");
    {
        obol::Scene scene;
        obol::TranslateAxisOverlay overlay;
        overlay.transform.translation = {2.0f, 0.0f, 0.0f};
        overlay.axis = {1.0f, 0.0f, 0.0f};
        overlay.length = 4.0f;
        overlay.handleSize = 0.5f;
        overlay.lineWidth = 3.0f;

        const obol::InteractionOverlay created =
            obol::TransformDragger::addTranslateAxisOverlay(scene, overlay);

        obol::SceneQuery lineQuery;
        lineQuery.type = obol::SceneObjectType::Polyline;
        obol::SceneQuery primitiveQuery;
        primitiveQuery.type = obol::SceneObjectType::Primitive;

        SoSeparator * root = legacySceneGraph(scene);
        SoSeparator * group = findNamedSeparator(root, "ObolSceneGroup_1");
        SoLineSet * line = findFirstNodeOfType<SoLineSet>(group);
        SoDrawStyle * style = findFirstNodeOfType<SoDrawStyle>(group);

        const bool pass =
            created.group == 1 &&
            created.objects.size() == 3 &&
            scene.groupCount() == 1 &&
            scene.findObjects(lineQuery).size() == 1 &&
            scene.findObjects(primitiveQuery).size() == 2 &&
            group &&
            line &&
            style &&
            style->lineWidth.getValue() == 3.0f;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Translate-axis overlay was not portable scene geometry");
    }

    runner.startTest("v2 TransformDragger creates portable trackball overlays");
    {
        obol::Scene scene;
        obol::TrackballOverlay overlay;
        overlay.transform.translation = {-2.0f, 0.0f, 0.0f};
        overlay.radius = 1.5f;
        overlay.lineWidth = 4.0f;
        overlay.segments = 16;

        const obol::InteractionOverlay created =
            obol::TransformDragger::addTrackballOverlay(scene, overlay);

        obol::SceneQuery lineQuery;
        lineQuery.type = obol::SceneObjectType::Polyline;

        SoSeparator * root = legacySceneGraph(scene);
        SoSeparator * group = findNamedSeparator(root, "ObolSceneGroup_1");
        int lineSetCount = 0;
        int drawStyleCount = 0;
        if (group) {
            for (int i = 0; i < group->getNumChildren(); ++i) {
                SoSeparator * object = dynamic_cast<SoSeparator *>(group->getChild(i));
                if (!object) continue;
                lineSetCount += findFirstNodeOfType<SoLineSet>(object) ? 1 : 0;
                SoDrawStyle * style = findFirstNodeOfType<SoDrawStyle>(object);
                if (style && style->lineWidth.getValue() == 4.0f) {
                    ++drawStyleCount;
                }
            }
        }

        const bool pass =
            created.group == 1 &&
            created.objects.size() == 3 &&
            scene.groupCount() == 1 &&
            scene.findObjects(lineQuery).size() == 3 &&
            lineSetCount == 3 &&
            drawStyleCount == 3;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Trackball overlay was not portable scene geometry");
    }

    runner.startTest("v2 TransformDragger creates portable box overlays");
    {
        obol::Scene scene;
        obol::BoxOverlay overlay;
        overlay.transform.translation = {1.0f, 2.0f, 3.0f};
        overlay.halfSize = {1.0f, 2.0f, 3.0f};
        overlay.lineWidth = 5.0f;

        const obol::InteractionOverlay created =
            obol::TransformDragger::addBoxOverlay(scene, overlay);

        obol::SceneQuery lineQuery;
        lineQuery.type = obol::SceneObjectType::Polyline;

        SoSeparator * root = legacySceneGraph(scene);
        SoSeparator * group = findNamedSeparator(root, "ObolSceneGroup_1");
        SoLineSet * line = findFirstNodeOfType<SoLineSet>(group);
        SoDrawStyle * style = findFirstNodeOfType<SoDrawStyle>(group);
        SoTransform * transform = findFirstNodeOfType<SoTransform>(group);
        const SbVec3f translation = transform
            ? transform->translation.getValue()
            : SbVec3f(0.0f, 0.0f, 0.0f);

        const bool pass =
            created.group == 1 &&
            created.objects.size() == 1 &&
            scene.groupCount() == 1 &&
            scene.findObjects(lineQuery).size() == 1 &&
            group &&
            line &&
            style &&
            style->lineWidth.getValue() == 5.0f &&
            transform &&
            std::fabs(translation[0] - 1.0f) < 1.0e-5f &&
            std::fabs(translation[1] - 2.0f) < 1.0e-5f &&
            std::fabs(translation[2] - 3.0f) < 1.0e-5f;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Box overlay was not portable scene geometry");
    }

    runner.startTest("v2 TransformDragger creates target-attached manipulator overlays");
    {
        obol::Scene scene;
        const obol::SceneObjectId target =
            scene.addPrimitive(obol::Primitive::Cube);

        obol::ManipulatorOverlay overlay;
        overlay.target = target;
        overlay.kind = obol::ManipulatorOverlayKind::Trackball;
        overlay.transform.translation = {-1.0f, 0.0f, 0.0f};
        overlay.trackballRadius = 1.25f;
        overlay.lineWidth = 6.0f;
        overlay.segments = 12;

        const obol::InteractionOverlay created =
            obol::TransformDragger::addManipulatorOverlay(scene, overlay);

        obol::SceneQuery lineQuery;
        lineQuery.type = obol::SceneObjectType::Polyline;

        SoSeparator * root = legacySceneGraph(scene);
        SoSeparator * group = findNamedSeparator(root, "ObolSceneGroup_1");
        int lineSetCount = 0;
        if (group) {
            for (int i = 0; i < group->getNumChildren(); ++i) {
                SoSeparator * object = dynamic_cast<SoSeparator *>(group->getChild(i));
                lineSetCount += findFirstNodeOfType<SoLineSet>(object) ? 1 : 0;
            }
        }

        const bool pass =
            created.target == target &&
            created.group == 1 &&
            created.objects.size() == 3 &&
            scene.findObjects(lineQuery).size() == 3 &&
            lineSetCount == 3;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Manipulator overlay was not attached portable geometry");
    }

    runner.startTest("v2 Scene removes objects while preserving stable ids");
    {
        obol::Scene scene;
        scene.setCamera(obol::PerspectiveCamera{});
        const obol::SceneObjectId cube = scene.addPrimitive(obol::Primitive::Cube);
        const obol::SceneObjectId sphere = scene.addPrimitive(obol::Primitive::Sphere);

        const bool removed = scene.removeObject(cube);
        const bool rejectedRepeat = !scene.removeObject(cube);
        const bool rejectedMutation = !scene.setObjectTransform(cube, obol::Transform{});

        obol::SceneQuery primitiveQuery;
        primitiveQuery.type = obol::SceneObjectType::Primitive;
        const std::vector<obol::SceneObjectInfo> primitives =
            scene.findObjects(primitiveQuery);

        SoSeparator * root = legacySceneGraph(scene);
        SoSeparator * removedSep = findNamedSeparator(root, "ObolSceneObject_1");
        SoSeparator * remainingSep = findNamedSeparator(root, "ObolSceneObject_2");

        const bool pass = removed &&
                          rejectedRepeat &&
                          rejectedMutation &&
                          scene.objectCount() == 1 &&
                          primitives.size() == 1 &&
                          primitives[0].id == sphere &&
                          removedSep == nullptr &&
                          remainingSep != nullptr;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Removed object remained visible or mutable");
    }

    runner.startTest("v2 Scene carries imported legacy graphs as transformable objects");
    {
        SoSeparator * imported = new SoSeparator;
        imported->ref();
        imported->addChild(new SoCube);

        obol::Scene scene;
        const obol::SceneGroupId group = scene.addGroup();
        obol::Transform transform;
        transform.translation = {1.0f, 2.0f, 3.0f};
        const obol::SceneObjectId legacy =
            scene.addLegacySceneGraph(imported, transform, group);
        imported->unref();

        obol::Material material;
        material.baseColor = {1.0f, 0.0f, 0.0f, 1.0f};
        const bool updatedMaterial = scene.setObjectMaterial(legacy, material);

        obol::SceneQuery backendQuery;
        backendQuery.category = obol::SceneObjectCategory::BackendNative;
        const std::vector<obol::SceneObjectInfo> backendObjects =
            scene.findObjects(backendQuery);

        SoSeparator * root = legacySceneGraph(scene);
        SoSeparator * object = findNamedSeparator(root, "ObolSceneObject_1");
        SoTransform * bridgedTransform = object && object->getNumChildren() > 0
            ? dynamic_cast<SoTransform *>(object->getChild(0))
            : nullptr;
        SoMaterial * bridgedMaterial = object && object->getNumChildren() > 1
            ? dynamic_cast<SoMaterial *>(object->getChild(1))
            : nullptr;
        SoSeparator * copiedLegacy = object && object->getNumChildren() > 2
            ? dynamic_cast<SoSeparator *>(object->getChild(2))
            : nullptr;
        SoCube * cube = copiedLegacy && copiedLegacy->getNumChildren() == 1
            ? dynamic_cast<SoCube *>(copiedLegacy->getChild(0))
            : nullptr;

        const SbVec3f translation = bridgedTransform
            ? bridgedTransform->translation.getValue()
            : SbVec3f(0.0f, 0.0f, 0.0f);
        const SbColor color = bridgedMaterial
            ? bridgedMaterial->diffuseColor[0]
            : SbColor(0.0f, 0.0f, 0.0f);
        const bool pass = legacy == 1 &&
                          updatedMaterial &&
                          backendObjects.size() == 1 &&
                          backendObjects[0].id == legacy &&
                          backendObjects[0].type == obol::SceneObjectType::LegacySceneGraph &&
                          backendObjects[0].parent == group &&
                          object &&
                          bridgedTransform &&
                          translation == SbVec3f(1.0f, 2.0f, 3.0f) &&
                          bridgedMaterial &&
                          color == SbColor(1.0f, 0.0f, 0.0f) &&
                          copiedLegacy &&
                          cube;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Legacy graph bridge did not preserve v2 identity and transform");
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

        SoSeparator * root = legacySceneGraph(scene);
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

        SoSeparator * root = legacySceneGraph(scene);
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

    runner.startTest("v2 Scene bridges unlit materials as base-color rendering");
    {
        obol::Scene scene;
        obol::Material material;
        material.baseColor = {0.4f, 0.6f, 0.8f, 1.0f};
        material.unlit = true;
        scene.addPrimitive(obol::Primitive::Sphere, material);

        SoSeparator * root = legacySceneGraph(scene);
        SoSeparator * object = findNamedSeparator(root, "ObolSceneObject_1");
        SoLightModel * lightModel = nullptr;
        if (object) {
            for (int i = 0; i < object->getNumChildren(); ++i) {
                SoNode * child = object->getChild(i);
                if (child->isOfType(SoLightModel::getClassTypeId())) {
                    lightModel = static_cast<SoLightModel *>(child);
                    break;
                }
            }
        }
        const bool pass =
            object &&
            lightModel &&
            lightModel->model.getValue() == SoLightModel::BASE_COLOR;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Unlit material did not bridge to base-color light model");
    }

    runner.startTest("v2 sphere tessellation generates portable mesh data");
    {
        const obol::Mesh mesh = obol::makeSphereMesh(2.0f, 8, 4);
        obol::Scene scene;
        const obol::SceneObjectId sphereMesh = scene.addMesh(mesh);
        SoSeparator * root = legacySceneGraph(scene);
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

    runner.startTest("v2 packet geometry lowers primitives to triangles");
    {
        obol::Scene scene;
        obol::PrimitiveOptions options;
        options.width = 4.0f;
        options.height = 2.0f;
        options.depth = 6.0f;
        obol::Transform transform;
        transform.translation = {1.0f, 2.0f, 3.0f};
        const obol::SceneObjectId cube =
            scene.addPrimitive(obol::Primitive::Cube,
                               obol::Material{},
                               transform,
                               options);

        std::vector<obol::PacketTriangle> triangles;
        std::vector<obol::PacketGeometryDiagnostic> diagnostics;
        const bool lowered =
            obol::collectPacketTriangles(scene.capturePacket(),
                                         triangles,
                                         &diagnostics);
        const bool pass =
            lowered &&
            diagnostics.empty() &&
            triangles.size() == 12 &&
            triangles[0].objectId == cube &&
            std::fabs(triangles[0].vertices[0].position.x + 1.0f) < 1.0e-5f &&
            std::fabs(triangles[0].vertices[0].position.y - 1.0f) < 1.0e-5f &&
            std::fabs(triangles[0].vertices[0].position.z - 6.0f) < 1.0e-5f &&
            std::fabs(triangles[0].vertices[0].normal.z - 1.0f) < 1.0e-5f;
        runner.endTest(pass, pass ? "" : "Packet primitive triangle extraction failed");
    }

    runner.startTest("v2 packet geometry lowers lines and points");
    {
        obol::Scene scene;
        obol::Transform lineTransform;
        lineTransform.translation = {1.0f, 2.0f, 3.0f};
        obol::Material lineMaterial;
        lineMaterial.baseColor = {0.25f, 0.5f, 0.75f, 1.0f};
        obol::Polyline polyline;
        polyline.points = {{0.0f, 0.0f, 0.0f},
                           {2.0f, 0.0f, 0.0f},
                           {2.0f, 3.0f, 0.0f}};
        polyline.lineWidth = 5.0f;
        const obol::SceneObjectId lineId =
            scene.addPolyline(polyline, lineMaterial, lineTransform);

        obol::Transform pointTransform;
        pointTransform.translation = {-1.0f, 4.0f, 0.0f};
        obol::PointCloud pointCloud;
        pointCloud.points = {{0.0f, 0.0f, 1.0f},
                             {1.0f, 0.0f, 1.0f}};
        pointCloud.pointSize = 7.0f;
        const obol::SceneObjectId pointId =
            scene.addPointCloud(pointCloud, obol::Material{}, pointTransform);

        const obol::ScenePacket packet = scene.capturePacket();
        std::vector<obol::PacketGeometryDiagnostic> supportDiagnostics;
        const obol::PacketGeometrySupport support =
            obol::inspectPacketGeometrySupport(packet, &supportDiagnostics);
        std::vector<obol::PacketLineSegment> segments;
        std::vector<obol::PacketPoint> points;
        std::vector<obol::PacketGeometryDiagnostic> lineDiagnostics;
        std::vector<obol::PacketGeometryDiagnostic> pointDiagnostics;
        const bool linesLowered =
            obol::collectPacketLineSegments(packet, segments, &lineDiagnostics);
        const bool pointsLowered =
            obol::collectPacketPoints(packet, points, &pointDiagnostics);

        const bool pass =
            support.portableGeometryObjects == 2 &&
            support.unsupportedObjects == 0 &&
            supportDiagnostics.empty() &&
            linesLowered &&
            pointsLowered &&
            lineDiagnostics.empty() &&
            pointDiagnostics.empty() &&
            segments.size() == 2 &&
            segments[0].objectId == lineId &&
            std::fabs(segments[0].lineWidth - 5.0f) < 1.0e-5f &&
            std::fabs(segments[0].vertices[0].position.x - 1.0f) < 1.0e-5f &&
            std::fabs(segments[0].vertices[0].position.y - 2.0f) < 1.0e-5f &&
            std::fabs(segments[0].vertices[0].position.z - 3.0f) < 1.0e-5f &&
            std::fabs(segments[1].vertices[1].position.x - 3.0f) < 1.0e-5f &&
            std::fabs(segments[1].vertices[1].position.y - 5.0f) < 1.0e-5f &&
            points.size() == 2 &&
            points[0].objectId == pointId &&
            std::fabs(points[0].pointSize - 7.0f) < 1.0e-5f &&
            std::fabs(points[0].position.x + 1.0f) < 1.0e-5f &&
            std::fabs(points[0].position.y - 4.0f) < 1.0e-5f &&
            std::fabs(points[0].position.z - 1.0f) < 1.0e-5f &&
            std::fabs(points[1].position.x - 0.0f) < 1.0e-5f;
        runner.endTest(pass, pass ? "" : "Packet line/point extraction failed");
    }

    runner.startTest("v2 packet lighting lowers lights to world space");
    {
        obol::Scene scene;
        obol::Transform translated;
        translated.translation = {10.0f, 0.0f, 0.0f};
        const obol::SceneGroupId translatedGroup = scene.addGroup(translated);

        obol::Transform rotated;
        rotated.rotationAxis = {0.0f, 0.0f, 1.0f};
        rotated.rotationRadians = 1.57079632679f;
        const obol::SceneGroupId rotatedGroup = scene.addGroup(rotated);

        obol::DirectionalLight directional;
        directional.direction = {1.0f, 0.0f, 0.0f};
        directional.intensity = 0.5f;
        const obol::SceneObjectId directionalId =
            scene.addDirectionalLight(directional, rotatedGroup);

        obol::PointLight point;
        point.location = {1.0f, 2.0f, 3.0f};
        point.intensity = 2.0f;
        const obol::SceneObjectId pointId =
            scene.addPointLight(point, translatedGroup);

        obol::SpotLight spot;
        spot.location = {0.0f, 1.0f, 0.0f};
        spot.direction = {0.0f, -1.0f, 0.0f};
        spot.cutOffAngleRadians = 0.25f;
        spot.dropOffRate = 0.75f;
        const obol::SceneObjectId spotId =
            scene.addSpotLight(spot, translatedGroup);

        std::vector<obol::PacketLight> lights;
        std::vector<obol::PacketGeometryDiagnostic> diagnostics;
        const bool lowered =
            obol::collectPacketLights(scene.capturePacket(), lights,
                                      &diagnostics);

        const obol::PacketLight * directionalPacket = nullptr;
        const obol::PacketLight * pointPacket = nullptr;
        const obol::PacketLight * spotPacket = nullptr;
        for (const obol::PacketLight & light : lights) {
            if (light.objectId == directionalId) directionalPacket = &light;
            if (light.objectId == pointId) pointPacket = &light;
            if (light.objectId == spotId) spotPacket = &light;
        }

        const bool pass =
            lowered &&
            diagnostics.empty() &&
            lights.size() == 3 &&
            directionalPacket &&
            directionalPacket->kind == obol::PacketLightKind::Directional &&
            std::fabs(directionalPacket->intensity - 0.5f) < 1.0e-5f &&
            std::fabs(directionalPacket->direction.x) < 1.0e-5f &&
            std::fabs(directionalPacket->direction.y - 1.0f) < 1.0e-5f &&
            pointPacket &&
            pointPacket->kind == obol::PacketLightKind::Point &&
            std::fabs(pointPacket->position.x - 11.0f) < 1.0e-5f &&
            std::fabs(pointPacket->position.y - 2.0f) < 1.0e-5f &&
            std::fabs(pointPacket->position.z - 3.0f) < 1.0e-5f &&
            spotPacket &&
            spotPacket->kind == obol::PacketLightKind::Spot &&
            std::fabs(spotPacket->position.x - 10.0f) < 1.0e-5f &&
            std::fabs(spotPacket->position.y - 1.0f) < 1.0e-5f &&
            std::fabs(spotPacket->direction.y + 1.0f) < 1.0e-5f &&
            std::fabs(spotPacket->cutOffAngleRadians - 0.25f) < 1.0e-5f &&
            std::fabs(spotPacket->dropOffRate - 0.75f) < 1.0e-5f;
        runner.endTest(pass, pass ? "" : "Packet light extraction failed");
    }

    runner.startTest("v2 packet text preserves text payloads");
    {
        obol::Scene scene;
        obol::Transform text2Transform;
        text2Transform.translation = {2.0f, 3.0f, 4.0f};
        obol::Material text2Material;
        text2Material.baseColor = {0.1f, 0.2f, 0.3f, 1.0f};
        obol::Text2D text2;
        text2.text = "packet label";
        text2.fontName = "iosevka";
        text2.fontSize = 18.0f;
        text2.spacing = 1.25f;
        text2.justification = obol::TextJustification::Center;
        text2.depthTest = false;
        const obol::SceneObjectId text2Id =
            scene.addText2D(text2, text2Material, text2Transform);

        obol::Transform text3Transform;
        text3Transform.translation = {-1.0f, 0.0f, 5.0f};
        obol::Text3D text3;
        text3.text = "solid";
        text3.fontName = "default";
        text3.fontSize = 2.0f;
        text3.spacing = 0.75f;
        text3.justification = obol::TextJustification::Right;
        text3.parts = static_cast<uint32_t>(obol::Text3DParts::Front) |
                      static_cast<uint32_t>(obol::Text3DParts::Sides);
        text3.partColors = {{1.0f, 0.0f, 0.0f, 1.0f},
                            {0.0f, 1.0f, 0.0f, 1.0f}};
        text3.profile = {{0.0f, 0.0f}, {0.1f, 0.0f}};
        const obol::SceneObjectId text3Id =
            scene.addText3D(text3, obol::Material{}, text3Transform);

        const obol::ScenePacket packet = scene.capturePacket();
        std::vector<obol::PacketGeometryDiagnostic> supportDiagnostics;
        const obol::PacketGeometrySupport support =
            obol::inspectPacketGeometrySupport(packet, &supportDiagnostics);
        std::vector<obol::PacketText> packetText;
        std::vector<obol::PacketGeometryDiagnostic> textDiagnostics;
        const bool lowered =
            obol::collectPacketText(packet, packetText, &textDiagnostics);

        const obol::PacketText * text2Packet = nullptr;
        const obol::PacketText * text3Packet = nullptr;
        for (const obol::PacketText & record : packetText) {
            if (record.objectId == text2Id) text2Packet = &record;
            if (record.objectId == text3Id) text3Packet = &record;
        }

        const bool pass =
            support.textObjects == 2 &&
            supportDiagnostics.empty() &&
            lowered &&
            textDiagnostics.empty() &&
            packetText.size() == 2 &&
            text2Packet &&
            text2Packet->kind == obol::PacketTextKind::Text2D &&
            text2Packet->text == "packet label" &&
            text2Packet->fontName == "iosevka" &&
            std::fabs(text2Packet->fontSize - 18.0f) < 1.0e-5f &&
            std::fabs(text2Packet->spacing - 1.25f) < 1.0e-5f &&
            text2Packet->justification == obol::TextJustification::Center &&
            !text2Packet->depthTest &&
            std::fabs(text2Packet->origin.x - 2.0f) < 1.0e-5f &&
            std::fabs(text2Packet->origin.y - 3.0f) < 1.0e-5f &&
            std::fabs(text2Packet->origin.z - 4.0f) < 1.0e-5f &&
            std::fabs(text2Packet->material.baseColor.g - 0.2f) < 1.0e-5f &&
            text3Packet &&
            text3Packet->kind == obol::PacketTextKind::Text3D &&
            text3Packet->text == "solid" &&
            text3Packet->justification == obol::TextJustification::Right &&
            text3Packet->parts == text3.parts &&
            text3Packet->partColors.size() == 2 &&
            text3Packet->profile.size() == 2 &&
            std::fabs(text3Packet->origin.x + 1.0f) < 1.0e-5f &&
            std::fabs(text3Packet->origin.z - 5.0f) < 1.0e-5f;
        runner.endTest(pass, pass ? "" : "Packet text extraction failed");
    }

    runner.startTest("v2 packet extraction aggregates portable scene records");
    {
        obol::Scene scene;

        obol::Material material;
        material.baseColor = {0.25f, 0.5f, 0.75f, 1.0f};
        scene.addPrimitive(obol::Primitive::Cube, material);

        obol::Polyline polyline;
        polyline.points = {{0.0f, 0.0f, 0.0f},
                           {1.0f, 0.0f, 0.0f},
                           {1.0f, 1.0f, 0.0f}};
        polyline.lineWidth = 3.0f;
        scene.addPolyline(polyline, material);

        obol::PointCloud pointCloud;
        pointCloud.points = {{0.0f, 0.0f, 1.0f},
                             {0.0f, 1.0f, 1.0f}};
        pointCloud.pointSize = 4.0f;
        scene.addPointCloud(pointCloud, material);

        obol::DirectionalLight light;
        light.direction = {0.0f, -1.0f, -1.0f};
        scene.addDirectionalLight(light);

        obol::Text2D text;
        text.text = "aggregate";
        scene.addText2D(text, material);

        obol::ExtractedPacketScene extracted;
        const bool complete =
            obol::extractPacketScene(scene.capturePacket(), extracted);

        const bool pass =
            complete &&
            extracted.complete &&
            extracted.support.portableGeometryObjects == 3 &&
            extracted.support.lightObjects == 1 &&
            extracted.support.textObjects == 1 &&
            extracted.triangles.size() == 12 &&
            extracted.lineSegments.size() == 2 &&
            extracted.points.size() == 2 &&
            extracted.lights.size() == 1 &&
            extracted.text.size() == 1 &&
            extracted.cadAssemblies.empty() &&
            extracted.diagnostics.empty();
        runner.endTest(pass, pass ? "" : "Packet scene aggregation failed");
    }

    runner.startTest("v2 packet extraction reports backend-specific content");
    {
        obol::Scene scene;
        scene.addPrimitive(obol::Primitive::Cube);
        obol::OpenGLCallback callback;
        callback.draw = testOpenGLDrawCallback;
        scene.addOpenGLCallback(callback);

        obol::ExtractedPacketScene extracted;
        const bool complete =
            obol::extractPacketScene(scene.capturePacket(), extracted);

        const bool pass =
            !complete &&
            !extracted.complete &&
            extracted.support.portableGeometryObjects == 1 &&
            extracted.support.backendNativeObjects == 1 &&
            extracted.triangles.size() == 12 &&
            extracted.diagnostics.size() == 1 &&
            extracted.diagnostics[0].severity ==
                obol::PacketGeometryDiagnosticSeverity::Warning &&
            extracted.diagnostics[0].message.find("OpenGL callback") !=
                std::string::npos;
        runner.endTest(pass, pass ? "" : "Packet extraction did not report backend-specific content");
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

        SoSeparator * root = legacySceneGraph(scene);
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

        SoSeparator * root = legacySceneGraph(scene);
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

    runner.startTest("v2 OffscreenRenderer uses packet-only render backends");
    {
        PacketOnlyBackend backend;
        obol::Scene scene;
        obol::PerspectiveCamera camera;
        scene.setCamera(camera);
        const obol::SceneGroupId group = scene.addGroup();
        scene.addPrimitive(obol::Primitive::Cube,
                           obol::Material{},
                           obol::Transform{},
                           obol::PrimitiveOptions{},
                           group);

        obol::RenderTarget target;
        target.width = 3;
        target.height = 2;
        target.pixelFormat = obol::PixelFormat::RGBA;
        obol::OffscreenRenderer renderer(backend, target);
        const obol::FrameResult result = renderer.render(scene);
        const unsigned char * pixels = renderer.pixels();
        const char * path = "/tmp/obol_packet_backend.rgb";
        const bool wrote = renderer.writeRGB(path);

        FILE * file = std::fopen(path, "rb");
        int magic0 = EOF;
        int magic1 = EOF;
        if (file) {
            magic0 = std::fgetc(file);
            magic1 = std::fgetc(file);
            std::fclose(file);
            std::remove(path);
        }

        const bool pass =
            backend.legacyContextHandle() == nullptr &&
            result.success &&
            result.capabilities.known &&
            result.capabilities.backendKind == obol::RenderBackendKind::CPU &&
            result.capabilities.backendName == "packet-only-test" &&
            backend.lastObjectCount == 1 &&
            backend.lastGroupCount == 1 &&
            backend.lastCameraKind == obol::SceneCameraKind::Perspective &&
            pixels &&
            pixels[0] == 25 &&
            pixels[1] == 50 &&
            pixels[2] == 75 &&
            pixels[3] == 255 &&
            wrote &&
            magic0 == 0x01 &&
            magic1 == 0xda;
        runner.endTest(pass, pass ? "" : "Expected packet-only backend render and SGI RGB output");
    }

    runner.startTest("v2 OffscreenRenderer reports unsupported packet backends");
    {
        CapabilityOnlyBackend backend;
        obol::Scene scene;
        scene.addPrimitive(obol::Primitive::Cube);
        SoSeparator * legacyRoot = new SoSeparator;
        legacyRoot->ref();
        legacyRoot->addChild(new SoCube);
        scene.addLegacySceneGraph(legacyRoot);
        legacyRoot->unref();
        obol::OffscreenRenderer renderer(backend, 2, 2);
        const obol::FrameResult result = renderer.render(scene);

        bool foundError = false;
        bool foundLegacyWarning = false;
        for (const obol::RenderDiagnostic & diagnostic : result.diagnostics) {
            foundError = foundError ||
                (diagnostic.severity == obol::DiagnosticSeverity::Error &&
                 diagnostic.message.find("packet rendering") != std::string::npos);
            foundLegacyWarning = foundLegacyWarning ||
                (diagnostic.severity == obol::DiagnosticSeverity::Warning &&
                 diagnostic.message.find("legacy scene graph") != std::string::npos);
        }

        const bool pass = !result.success &&
                          result.capabilities.known &&
                          result.capabilities.backendKind == obol::RenderBackendKind::Custom &&
                          foundError &&
                          foundLegacyWarning &&
                          renderer.pixels() == nullptr;
        runner.endTest(pass, pass ? "" : "Expected explicit diagnostic for backend without packet rendering");
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

        SoSeparator * root = legacySceneGraph(scene);
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
        SoSeparator * root = legacySceneGraph(loaded);
        const bool pass = wrote && read && !iv.empty() && root && root->getNumChildren() >= 1;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Inventor string round-trip failed through v2 SceneIO");
    }

    runner.startTest("v2 SceneIO extracts simple Inventor primitives as native objects");
    {
        const std::string iv =
            "#Inventor V2.0 ascii\n"
            "Separator {\n"
            "  Material { diffuseColor 1 0 0 specularColor 0.25 0.25 0.25 }\n"
            "  Transform { translation 1 2 3 scaleFactor 2 1 1 }\n"
            "  Cube { width 3 height 2 depth 1 }\n"
            "}\n";

        obol::Scene loaded;
        const bool read = obol::SceneIO::readInventorString(iv, loaded, &manager);

        obol::SceneQuery primitiveQuery;
        primitiveQuery.type = obol::SceneObjectType::Primitive;

        SoSeparator * root = legacySceneGraph(loaded);
        SoSeparator * object = findNamedSeparator(root, "ObolSceneObject_1");
        SoCube * cube = nullptr;
        SoMaterial * material = nullptr;
        if (object) {
            for (int i = 0; i < object->getNumChildren(); ++i) {
                SoNode * child = object->getChild(i);
                if (child->isOfType(SoMaterial::getClassTypeId())) {
                    material = static_cast<SoMaterial *>(child);
                } else if (child->isOfType(SoCube::getClassTypeId())) {
                    cube = static_cast<SoCube *>(child);
                }
            }
        }
        const SbColor color = material
            ? material->diffuseColor[0]
            : SbColor(0.0f, 0.0f, 0.0f);
        const bool pass =
            read &&
            loaded.objectCount() == 1 &&
            loaded.hasObjects(primitiveQuery) &&
            object &&
            cube &&
            cube->width.getValue() == 3.0f &&
            cube->height.getValue() == 2.0f &&
            cube->depth.getValue() == 1.0f &&
            material &&
            color[0] == 1.0f &&
            color[1] == 0.0f &&
            color[2] == 0.0f;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Simple Inventor scene was not extracted to native v2 objects");
    }

    runner.startTest("v2 SceneIO extracts Inventor IndexedFaceSet material bindings");
    {
        const std::string iv =
            "#Inventor V2.0 ascii\n"
            "Separator {\n"
            "  Coordinate3 { point [ 0 0 0, 1 0 0, 1 1 0, 0 1 0, 0 0 1, 1 0 1 ] }\n"
            "  Normal { vector [ 0 0 1, 0 -1 0 ] }\n"
            "  NormalBinding { value PER_FACE }\n"
            "  TextureCoordinate2 { point [ 0 0, 1 0, 1 1, 0 1 ] }\n"
            "  Material { diffuseColor [ 1 0 0, 0 1 0 ] }\n"
            "  MaterialBinding { value PER_FACE }\n"
            "  IndexedFaceSet {\n"
            "    coordIndex [ 0, 1, 2, 3, -1, 0, 4, 5, 1, -1 ]\n"
            "    textureCoordIndex [ 0, 1, 2, 3, -1, 0, 1, 2, 3, -1 ]\n"
            "  }\n"
            "}\n";

        obol::Scene loaded;
        const bool read = obol::SceneIO::readInventorString(iv, loaded, &manager);

        obol::SceneQuery meshQuery;
        meshQuery.type = obol::SceneObjectType::Mesh;

        SoSeparator * root = legacySceneGraph(loaded);
        SoSeparator * object = findNamedSeparator(root, "ObolSceneObject_1");
        SoMaterial * material = findFirstNodeOfType<SoMaterial>(object);
        SoMaterialBinding * binding = findFirstNodeOfType<SoMaterialBinding>(object);
        SoNormalBinding * normalBinding = findFirstNodeOfType<SoNormalBinding>(object);
        SoTextureCoordinate2 * texCoords = findFirstNodeOfType<SoTextureCoordinate2>(object);
        SoIndexedFaceSet * faces = findFirstNodeOfType<SoIndexedFaceSet>(object);

        const bool pass =
            read &&
            loaded.objectCount() == 1 &&
            loaded.hasObjects(meshQuery) &&
            object &&
            material &&
            material->diffuseColor.getNum() == 2 &&
            binding &&
            binding->value.getValue() == SoMaterialBinding::PER_FACE &&
            normalBinding &&
            normalBinding->value.getValue() == SoNormalBinding::PER_FACE &&
            texCoords &&
            texCoords->point.getNum() == 4 &&
            faces &&
            faces->coordIndex.getNum() == 10 &&
            faces->textureCoordIndex.getNum() == 10;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "IndexedFaceSet attributes did not extract to native v2 mesh");
    }

    runner.startTest("v2 SceneIO extracts Inventor IndexedTriangleStripSet meshes");
    {
        const std::string iv =
            "#Inventor V2.1 ascii\n"
            "Separator {\n"
            "  Coordinate3 { point [ 0 0 0, 1 0 0, 0 1 0, 1 1 0, 2 0 0, 2 1 0 ] }\n"
            "  Normal { vector [ 0 0 1, 0 0 1, 0 0 1, 0 0 1, 0 0 1, 0 0 1 ] }\n"
            "  NormalBinding { value PER_VERTEX_INDEXED }\n"
            "  Material { diffuseColor [ 1 0 0, 0 1 0 ] }\n"
            "  MaterialBinding { value PER_PART }\n"
            "  IndexedTriangleStripSet {\n"
            "    coordIndex [ 0, 1, 2, 3, -1, 2, 3, 4, 5, -1 ]\n"
            "  }\n"
            "}\n";

        obol::Scene loaded;
        const bool read = obol::SceneIO::readInventorString(iv, loaded, &manager);

        obol::SceneQuery meshQuery;
        meshQuery.type = obol::SceneObjectType::Mesh;

        SoSeparator * root = legacySceneGraph(loaded);
        SoSeparator * object = findNamedSeparator(root, "ObolSceneObject_1");
        SoMaterial * material = findFirstNodeOfType<SoMaterial>(object);
        SoMaterialBinding * binding = findFirstNodeOfType<SoMaterialBinding>(object);
        SoTriangleStripSet * strips = findFirstNodeOfType<SoTriangleStripSet>(object);
        const int materialColorCount = material ? material->diffuseColor.getNum() : -1;
        const int bindingValue = binding ? binding->value.getValue() : -1;
        const int stripCount = strips ? strips->numVertices.getNum() : -1;

        const bool pass =
            read &&
            loaded.objectCount() == 1 &&
            loaded.hasObjects(meshQuery) &&
            object &&
            material &&
            materialColorCount == 2 &&
            binding &&
            bindingValue == SoMaterialBinding::PER_PART &&
            strips &&
            stripCount == 2 &&
            strips->numVertices[0] == 4 &&
            strips->numVertices[1] == 4;
        std::string emitted;
        obol::SceneIO::writeInventorString(loaded, emitted);
        if (emitted.size() > 400) {
            emitted.resize(400);
        }
        if (root) root->unref();
        const std::string detail =
            std::string("read=") + (read ? "true" : "false") +
            " objects=" + std::to_string(loaded.objectCount()) +
            " hasMesh=" + (loaded.hasObjects(meshQuery) ? "true" : "false") +
            " object=" + (object ? "true" : "false") +
            " materialColors=" + std::to_string(materialColorCount) +
            " binding=" + std::to_string(bindingValue) +
            " strips=" + (strips ? "true" : "false") +
            " stripCount=" + std::to_string(stripCount) +
            " emitted=" + emitted;
        runner.endTest(pass, pass ? "" : detail.c_str());
    }

    runner.startTest("v2 SceneIO extracts Inventor LineSet geometry");
    {
        const std::string iv =
            "#Inventor V2.0 ascii\n"
            "Separator {\n"
            "  DrawStyle { lineWidth 3 }\n"
            "  Coordinate3 { point [ 0 0 0, 1 0 0, 0 1 0, 1 1 0, 2 1 0 ] }\n"
            "  Material { diffuseColor [ 1 0 0, 0 1 0 ] }\n"
            "  MaterialBinding { value PER_PART }\n"
            "  LineSet { numVertices [ 2, 3 ] }\n"
            "}\n";

        obol::Scene loaded;
        const bool read = obol::SceneIO::readInventorString(iv, loaded, &manager);

        obol::SceneQuery lineQuery;
        lineQuery.type = obol::SceneObjectType::Polyline;

        SoSeparator * root = legacySceneGraph(loaded);
        SoSeparator * first = findNamedSeparator(root, "ObolSceneObject_1");
        SoSeparator * second = findNamedSeparator(root, "ObolSceneObject_2");
        SoDrawStyle * firstStyle = findFirstNodeOfType<SoDrawStyle>(first);
        SoDrawStyle * secondStyle = findFirstNodeOfType<SoDrawStyle>(second);
        SoLineSet * firstLine = findFirstNodeOfType<SoLineSet>(first);
        SoLineSet * secondLine = findFirstNodeOfType<SoLineSet>(second);
        SoMaterial * secondMaterial = findFirstNodeOfType<SoMaterial>(second);
        const SbColor secondColor = secondMaterial && secondMaterial->diffuseColor.getNum() > 0
            ? secondMaterial->diffuseColor[0]
            : SbColor(0.0f, 0.0f, 0.0f);

        const bool pass =
            read &&
            loaded.objectCount() == 2 &&
            loaded.hasObjects(lineQuery) &&
            first &&
            second &&
            firstStyle &&
            firstStyle->lineWidth.getValue() == 3.0f &&
            secondStyle &&
            secondStyle->lineWidth.getValue() == 3.0f &&
            firstLine &&
            firstLine->numVertices.getNum() == 1 &&
            firstLine->numVertices[0] == 2 &&
            secondLine &&
            secondLine->numVertices.getNum() == 1 &&
            secondLine->numVertices[0] == 3 &&
            secondColor[0] == 0.0f &&
            secondColor[1] == 1.0f &&
            secondColor[2] == 0.0f;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "LineSet did not extract to v2 polylines");
    }

    runner.startTest("v2 SceneIO extracts Inventor PointSet geometry");
    {
        const std::string iv =
            "#Inventor V2.0 ascii\n"
            "Separator {\n"
            "  DrawStyle { pointSize 5 }\n"
            "  Coordinate3 { point [ 0 0 0, 1 0 0, 0 1 0, 1 1 0 ] }\n"
            "  Material { diffuseColor 0 0 1 }\n"
            "  PointSet { numPoints 3 }\n"
            "}\n";

        obol::Scene loaded;
        const bool read = obol::SceneIO::readInventorString(iv, loaded, &manager);

        obol::SceneQuery pointQuery;
        pointQuery.type = obol::SceneObjectType::PointCloud;

        SoSeparator * root = legacySceneGraph(loaded);
        SoSeparator * object = findNamedSeparator(root, "ObolSceneObject_1");
        SoDrawStyle * style = findFirstNodeOfType<SoDrawStyle>(object);
        SoPointSet * points = findFirstNodeOfType<SoPointSet>(object);
        SoMaterial * material = findFirstNodeOfType<SoMaterial>(object);
        const SbColor color = material && material->diffuseColor.getNum() > 0
            ? material->diffuseColor[0]
            : SbColor(0.0f, 0.0f, 0.0f);

        const bool pass =
            read &&
            loaded.objectCount() == 1 &&
            loaded.hasObjects(pointQuery) &&
            object &&
            style &&
            style->pointSize.getValue() == 5.0f &&
            points &&
            points->numPoints.getValue() == 3 &&
            color[0] == 0.0f &&
            color[1] == 0.0f &&
            color[2] == 1.0f;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "PointSet did not extract to v2 point cloud");
    }

    runner.startTest("v2 SceneIO falls back for unsupported Inventor bindings");
    {
        const std::string iv =
            "#Inventor V2.0 ascii\n"
            "Separator {\n"
            "  Material { diffuseColor [ 1 0 0, 0 1 0 ] }\n"
            "  MaterialBinding { value PER_PART }\n"
            "  Cone { }\n"
            "}\n";

        obol::Scene loaded;
        const bool read = obol::SceneIO::readInventorString(iv, loaded, &manager);
        SoSeparator * root = legacySceneGraph(loaded);
        SoSeparator * nativeObject = findNamedSeparator(root, "ObolSceneObject_1");
        const bool pass =
            read &&
            loaded.objectCount() == 0 &&
            root &&
            root->getNumChildren() == 1 &&
            nativeObject == nullptr;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Unsupported Inventor binding did not preserve legacy fallback only");
    }

    runner.startTest("v2 SceneIO legacy fallback is lit by v2 lights");
    {
        const std::string iv =
            "#Inventor V2.0 ascii\n"
            "Separator {\n"
            "  Material { diffuseColor [ 1 0 0, 0 1 0 ] }\n"
            "  MaterialBinding { value PER_PART }\n"
            "  Cone { }\n"
            "}\n";

        obol::Scene loaded;
        const bool read = obol::SceneIO::readInventorString(iv, loaded, &manager);
        loaded.addDirectionalLight(obol::DirectionalLight{});

        SoSeparator * root = legacySceneGraph(loaded);
        const bool pass =
            read &&
            root &&
            root->getNumChildren() == 2 &&
            root->getChild(0)->isOfType(SoDirectionalLight::getClassTypeId()) &&
            root->getChild(1)->isOfType(SoSeparator::getClassTypeId());
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Legacy fallback root was not placed after v2 lights");
    }

    runner.startTest("v2 SceneIO imports Inventor content as a transformable object");
    {
        const std::string iv =
            "#Inventor V2.0 ascii\n"
            "Separator {\n"
            "  Material { diffuseColor [ 1 0 0, 0 1 0 ] }\n"
            "  MaterialBinding { value PER_PART }\n"
            "  Cone { }\n"
            "}\n";

        obol::Scene scene;
        const obol::SceneGroupId group = scene.addGroup();
        obol::Transform transform;
        transform.translation = {1.0f, 2.0f, 3.0f};
        const obol::SceneObjectId object =
            obol::SceneIO::addInventorString(iv, scene, transform, group, &manager);

        obol::SceneQuery backendQuery;
        backendQuery.type = obol::SceneObjectType::LegacySceneGraph;
        const std::vector<obol::SceneObjectInfo> objects =
            scene.findObjects(backendQuery);

        SoSeparator * root = legacySceneGraph(scene);
        SoSeparator * objectSep = findNamedSeparator(root, "ObolSceneObject_1");
        SoTransform * objectTransform = objectSep && objectSep->getNumChildren() > 0
            ? dynamic_cast<SoTransform *>(objectSep->getChild(0))
            : nullptr;
        const SbVec3f translation = objectTransform
            ? objectTransform->translation.getValue()
            : SbVec3f(0.0f, 0.0f, 0.0f);

        const bool pass = object == 1 &&
                          scene.objectCount() == 1 &&
                          objects.size() == 1 &&
                          objects[0].id == object &&
                          objects[0].parent == group &&
                          objectSep &&
                          objectTransform &&
                          translation == SbVec3f(1.0f, 2.0f, 3.0f);
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "SceneIO did not import Inventor content as a transformable v2 object");
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

        SoSeparator * root = legacySceneGraph(scene);
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

        SoSeparator * root = legacySceneGraph(scene);
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

        SoSeparator * root = legacySceneGraph(scene);
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

        SoSeparator * root = legacySceneGraph(scene);
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

        SoSeparator * root = legacySceneGraph(scene);
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

        SoSeparator * root = legacySceneGraph(scene);
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

    runner.startTest("v2 Scene bridges triangle strips to legacy strip nodes");
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
        mesh.faceColors = {{0.2f, 0.3f, 0.9f, 1.0f},
                           {0.4f, 0.4f, 0.4f, 1.0f}};

        obol::Scene scene;
        scene.addMesh(mesh);

        SoSeparator * root = legacySceneGraph(scene);
        SoSeparator * object = root && root->getNumChildren() == 1
            ? static_cast<SoSeparator *>(root->getChild(0))
            : nullptr;
        SoTriangleStripSet * stripSet = nullptr;
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
                        if (meshChild->isOfType(SoTriangleStripSet::getClassTypeId())) {
                            stripSet = static_cast<SoTriangleStripSet *>(meshChild);
                        }
                    }
                }
            }
        }

        const bool pass = object &&
                          foundMaterialBinding &&
                          stripSet &&
                          stripSet->numVertices.getNum() == 2 &&
                          stripSet->numVertices[0] == 4 &&
                          stripSet->numVertices[1] == 2;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Expected triangle strip legacy bridge");
    }

    runner.startTest("v2 Scene bridges quad grids to legacy quad mesh nodes");
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

        SoSeparator * root = legacySceneGraph(scene);
        SoSeparator * object = root && root->getNumChildren() == 1
            ? static_cast<SoSeparator *>(root->getChild(0))
            : nullptr;
        SoQuadMesh * quadMesh = nullptr;
        if (object) {
            for (int i = 0; i < object->getNumChildren(); ++i) {
                SoNode * child = object->getChild(i);
                if (child->isOfType(SoSeparator::getClassTypeId())) {
                    SoSeparator * meshSep = static_cast<SoSeparator *>(child);
                    for (int j = 0; j < meshSep->getNumChildren(); ++j) {
                        SoNode * meshChild = meshSep->getChild(j);
                        if (meshChild->isOfType(SoQuadMesh::getClassTypeId())) {
                            quadMesh = static_cast<SoQuadMesh *>(meshChild);
                        }
                    }
                }
            }
        }

        const bool pass = object &&
                          quadMesh &&
                          quadMesh->verticesPerRow.getValue() == 2 &&
                          quadMesh->verticesPerColumn.getValue() == 2;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Expected quad grid legacy bridge");
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
        SoSeparator * faceRoot = legacySceneGraph(faceScene);
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
        SoSeparator * vertexRoot = legacySceneGraph(vertexScene);
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

        SoSeparator * root = legacySceneGraph(scene);
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
        obol::CadPartGeometry geometry;
        geometry.wire = obol::CadWireRep{};
        geometry.wire->bounds.setBounds({-1.0f, -1.0f, -1.0f},
                                        {1.0f, 1.0f, 1.0f});
        obol::CadWirePolyline edge;
        edge.points.push_back({-1.0f, 0.0f, 0.0f});
        edge.points.push_back({1.0f, 0.0f, 0.0f});
        geometry.wire->polylines.push_back(edge);
        geometry.shaded = obol::CadTriMesh{};
        geometry.shaded->positions.push_back({-1.0f, -1.0f, 0.0f});
        geometry.shaded->positions.push_back({1.0f, -1.0f, 0.0f});
        geometry.shaded->positions.push_back({0.0f, 1.0f, 0.0f});
        geometry.shaded->indices = {0, 1, 2};
        geometry.shaded->bounds.setBounds({-1.0f, -1.0f, -0.1f},
                                          {1.0f, 1.0f, 0.1f});
        cad.upsertPart(part, geometry);

        obol::CadInstanceRecord record;
        record.part = part;
        record.parent = obol::CadIdBuilder::Root();
        record.childName = "v2_cad_instance";
        record.localToRoot.makeIdentity();
        record.localToRoot.values[12] = 2.0f;
        const obol::InstanceId instance = cad.upsertInstanceAuto(record);
        cad.setSelectedInstances({instance});
        cad.setHiddenInstances({instance});

        obol::Scene scene;
        obol::PerspectiveCamera camera;
        camera.position = {0.0f, 0.0f, 5.0f};
        camera.target = {0.0f, 0.0f, 0.0f};
        scene.setCamera(camera);
        obol::Transform cadTransform;
        cadTransform.translation = {5.0f, 0.0f, 0.0f};
        const obol::SceneObjectId object = scene.addCadAssembly(cad,
                                                                cadTransform);

        std::vector<obol::PacketCadAssembly> packetCad;
        std::vector<obol::PacketGeometryDiagnostic> packetDiagnostics;
        const bool packetLowered =
            obol::collectPacketCadAssemblies(scene.capturePacket(),
                                             packetCad,
                                             &packetDiagnostics);
        obol::ExtractedPacketScene extracted;
        const bool packetExtracted =
            obol::extractPacketScene(scene.capturePacket(), extracted);

        SoSeparator * root = legacySceneGraph(scene);
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
                          cadNode->isOfType(SoCADAssembly::getClassTypeId()) &&
                          packetLowered &&
                          packetDiagnostics.empty() &&
                          packetExtracted &&
                          extracted.complete &&
                          extracted.cadAssemblies.size() == 1 &&
                          extracted.cadAssemblies[0].instances.size() == 1 &&
                          extracted.cadAssemblies[0].instances[0].selected &&
                          packetCad.size() == 1 &&
                          packetCad[0].objectId == object &&
                          packetCad[0].drawMode == obol::CadDrawMode::Wireframe &&
                          packetCad[0].pickMode == obol::CadPickMode::Hybrid &&
                          packetCad[0].lodEnabled &&
                          packetCad[0].parts.size() == 1 &&
                          packetCad[0].parts[0].partId == part &&
                          packetCad[0].parts[0].geometry.wire &&
                          packetCad[0].parts[0].geometry.shaded &&
                          packetCad[0].instances.size() == 1 &&
                          packetCad[0].instances[0].instanceId == instance &&
                          packetCad[0].instances[0].partId == part &&
                          packetCad[0].instances[0].selected &&
                          packetCad[0].instances[0].hidden &&
                          std::fabs(packetCad[0].instances[0].localToWorld.values[12] - 7.0f) < 1.0e-5f;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Expected v2 CAD assembly to bridge to SoCADAssembly");
    }

    return runner.getSummary();
}
