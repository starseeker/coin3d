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
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/nodes/SoFont.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <Inventor/nodes/SoLinearProfile.h>
#include <Inventor/nodes/SoLineSet.h>
#include <Inventor/nodes/SoLightModel.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoMaterialBinding.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/nodes/SoPointLight.h>
#include <Inventor/nodes/SoPointSet.h>
#include <Inventor/nodes/SoProfileCoordinate2.h>
#include <Inventor/nodes/SoQuadMesh.h>
#include <Inventor/nodes/SoSpotLight.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoText2.h>
#include <Inventor/nodes/SoText3.h>
#include <Inventor/nodes/SoTexture2.h>
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
        caps.corePortable = true;
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

SoMaterial * findMaterialWithDiffuseColorCount(SoNode * root, int count)
{
    if (!root) return nullptr;
    if (root->isOfType(SoMaterial::getClassTypeId())) {
        SoMaterial * material = static_cast<SoMaterial *>(root);
        if (material->diffuseColor.getNum() == count) {
            return material;
        }
    }
    SoGroup * group = dynamic_cast<SoGroup *>(root);
    if (!group) return nullptr;
    for (int i = 0; i < group->getNumChildren(); ++i) {
        SoMaterial * found =
            findMaterialWithDiffuseColorCount(group->getChild(i), count);
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

    runner.startTest("v2 TransformDragger snaps translation edits");
    {
        obol::AxisDragRequest axisRequest;
        axisRequest.axis = {0.0f, 0.0f, 2.0f};
        axisRequest.distance = 2.74f;
        axisRequest.snap = true;
        axisRequest.snapStep = 0.5f;
        const obol::AxisDragResult axisResult =
            obol::TransformDragger::translateOnAxis(axisRequest);

        obol::FreeDragRequest freeRequest;
        freeRequest.startTransform.translation = {1.0f, 1.0f, 1.0f};
        freeRequest.delta = {1.24f, -0.76f, 0.49f};
        freeRequest.snap = true;
        freeRequest.snapStep = {0.5f, 0.25f, 1.0f};
        const obol::TranslationResult freeResult =
            obol::TransformDragger::translateFreely(freeRequest);

        obol::PlaneDragRequest planeRequest;
        planeRequest.delta = {1.24f, 1.26f, 5.0f};
        planeRequest.planeNormal = {0.0f, 0.0f, 1.0f};
        planeRequest.snap = true;
        planeRequest.snapStep = {0.5f, 0.5f, 0.5f};
        const obol::TranslationResult planeResult =
            obol::TransformDragger::translateOnPlane(planeRequest);

        const bool pass =
            axisResult.valid &&
            std::fabs(axisResult.distance - 2.5f) < 1.0e-5f &&
            std::fabs(axisResult.delta.z - 2.5f) < 1.0e-5f &&
            freeResult.valid &&
            std::fabs(freeResult.delta.x - 1.0f) < 1.0e-5f &&
            std::fabs(freeResult.delta.y + 0.75f) < 1.0e-5f &&
            std::fabs(freeResult.delta.z - 0.0f) < 1.0e-5f &&
            std::fabs(freeResult.transform.translation.x - 2.0f) < 1.0e-5f &&
            std::fabs(freeResult.transform.translation.y - 0.25f) < 1.0e-5f &&
            std::fabs(freeResult.transform.translation.z - 1.0f) < 1.0e-5f &&
            planeResult.valid &&
            std::fabs(planeResult.delta.x - 1.0f) < 1.0e-5f &&
            std::fabs(planeResult.delta.y - 1.5f) < 1.0e-5f &&
            std::fabs(planeResult.delta.z - 0.0f) < 1.0e-5f;
        runner.endTest(pass, pass ? "" : "Snapped translation edits were incorrect");
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

    runner.startTest("v2 TransformDragger snaps axis rotation");
    {
        obol::AxisRotationRequest request;
        request.axis = {0.0f, 5.0f, 0.0f};
        request.angleRadians = 1.31f;
        request.snap = true;
        request.snapStepRadians = 0.25f;

        const obol::AxisRotationResult result =
            obol::TransformDragger::rotateOnAxis(request);

        const bool pass =
            result.valid &&
            std::fabs(result.axis.y - 1.0f) < 1.0e-5f &&
            std::fabs(result.angleRadians - 1.25f) < 1.0e-5f &&
            std::fabs(result.transform.rotationAxis.y - 1.0f) < 1.0e-5f &&
            std::fabs(result.transform.rotationRadians - 1.25f) < 1.0e-5f;
        runner.endTest(pass, pass ? "" : "Snapped axis rotation was incorrect");
    }

    runner.startTest("v2 TransformDragger computes trackball rotation");
    {
        obol::TrackballRotationRequest request;
        request.from = {1.0f, 0.0f, 0.0f};
        request.to = {0.0f, 1.0f, 0.0f};
        request.snap = true;
        request.snapStepRadians = 0.25f;

        const obol::AxisRotationResult result =
            obol::TransformDragger::rotateTrackball(request);

        const bool pass =
            result.valid &&
            result.target == obol::InvalidSceneObjectId &&
            std::fabs(result.axis.x - 0.0f) < 1.0e-5f &&
            std::fabs(result.axis.y - 0.0f) < 1.0e-5f &&
            std::fabs(result.axis.z - 1.0f) < 1.0e-5f &&
            std::fabs(result.angleRadians - 1.5f) < 1.0e-5f &&
            std::fabs(result.transform.rotationAxis.z - 1.0f) < 1.0e-5f &&
            std::fabs(result.transform.rotationRadians - 1.5f) < 1.0e-5f;
        runner.endTest(pass, pass ? "" : "Trackball rotation math was incorrect");
    }

    runner.startTest("v2 TransformDragger applies trackball rotation");
    {
        obol::Scene scene;
        obol::Transform start;
        start.translation = {1.0f, 2.0f, 3.0f};
        const obol::SceneObjectId cube =
            scene.addPrimitive(obol::Primitive::Cube, obol::Material{}, start);

        obol::TrackballRotationRequest request;
        request.target = cube;
        request.startTransform = start;
        request.from = {1.0f, 0.0f, 0.0f};
        request.to = {-1.0f, 0.0f, 0.0f};
        request.fallbackAxis = {0.0f, 1.0f, 0.0f};
        request.clamp = true;
        request.minimumAngleRadians = 0.0f;
        request.maximumAngleRadians = 2.0f;

        obol::AxisRotationResult result;
        const bool applied =
            obol::TransformDragger::applyTrackballRotation(scene, request, &result);

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
            std::fabs(result.axis.y - 1.0f) < 1.0e-5f &&
            std::fabs(result.angleRadians - 2.0f) < 1.0e-5f &&
            transform &&
            std::fabs(axis[1] - 1.0f) < 1.0e-5f &&
            std::fabs(angle - 2.0f) < 1.0e-5f;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Trackball rotation did not update target transform");
    }

    runner.startTest("v2 TransformDragger applies bounded component scaling");
    {
        obol::Scene scene;
        obol::Transform start;
        start.scale = {1.0f, 2.0f, 3.0f};
        const obol::SceneObjectId cube =
            scene.addPrimitive(obol::Primitive::Cube, obol::Material{}, start);

        obol::ScaleRequest request;
        request.target = cube;
        request.startTransform = start;
        request.factors = {1.24f, 2.6f, 0.2f};
        request.snap = true;
        request.snapStep = {0.5f, 0.5f, 0.25f};
        request.bounds.enabled = true;
        request.bounds.minimum = {0.75f, 0.75f, 0.75f};
        request.bounds.maximum = {2.0f, 4.0f, 4.0f};

        obol::ScaleResult result;
        const bool applied =
            obol::TransformDragger::applyScale(scene, request, &result);

        SoSeparator * root = legacySceneGraph(scene);
        SoSeparator * object = findNamedSeparator(root, "ObolSceneObject_1");
        SoTransform * transform = findFirstNodeOfType<SoTransform>(object);
        const SbVec3f scale = transform
            ? transform->scaleFactor.getValue()
            : SbVec3f(0.0f, 0.0f, 0.0f);

        const bool pass =
            applied &&
            result.valid &&
            result.target == cube &&
            std::fabs(result.scale.x - 1.0f) < 1.0e-5f &&
            std::fabs(result.scale.y - 4.0f) < 1.0e-5f &&
            std::fabs(result.scale.z - 0.75f) < 1.0e-5f &&
            transform &&
            std::fabs(scale[0] - 1.0f) < 1.0e-5f &&
            std::fabs(scale[1] - 4.0f) < 1.0e-5f &&
            std::fabs(scale[2] - 0.75f) < 1.0e-5f;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Component scale edit did not update target transform");
    }

    runner.startTest("v2 TransformDragger computes math-only scaling");
    {
        obol::ScaleRequest request;
        request.startTransform.scale = {2.0f, 3.0f, 4.0f};
        request.factors = {0.5f, 2.0f, 1.5f};

        const obol::ScaleResult result =
            obol::TransformDragger::scaleByFactors(request);

        const bool pass =
            result.valid &&
            result.target == obol::InvalidSceneObjectId &&
            std::fabs(result.scale.x - 1.0f) < 1.0e-5f &&
            std::fabs(result.scale.y - 6.0f) < 1.0e-5f &&
            std::fabs(result.scale.z - 6.0f) < 1.0e-5f &&
            std::fabs(result.transform.scale.x - 1.0f) < 1.0e-5f &&
            std::fabs(result.transform.scale.y - 6.0f) < 1.0e-5f &&
            std::fabs(result.transform.scale.z - 6.0f) < 1.0e-5f;
        runner.endTest(pass, pass ? "" : "Math-only scale edit was incorrect");
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
            created.handles.size() == 3 &&
            created.handles[0].kind == obol::InteractionHandleKind::TranslateAxis &&
            std::fabs(created.handles[0].axis.x - 1.0f) < 1.0e-5f &&
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
            created.handles.size() == 3 &&
            created.handles[0].kind == obol::InteractionHandleKind::RotateAxis &&
            std::fabs(created.handles[0].axis.x - 1.0f) < 1.0e-5f &&
            std::fabs(created.handles[1].axis.y - 1.0f) < 1.0e-5f &&
            std::fabs(created.handles[2].axis.z - 1.0f) < 1.0e-5f &&
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
            created.handles.size() == 1 &&
            created.handles[0].kind == obol::InteractionHandleKind::BoundsBox &&
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
            created.handles.size() == 3 &&
            created.handles[0].kind == obol::InteractionHandleKind::RotateAxis &&
            scene.findObjects(lineQuery).size() == 3 &&
            lineSetCount == 3;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Manipulator overlay was not attached portable geometry");
    }

    runner.startTest("v2 TransformDragger resolves picked overlay handles");
    {
        obol::Scene scene;

        obol::TranslateAxisOverlay translateOverlay;
        translateOverlay.axis = {0.0f, 4.0f, 0.0f};
        const obol::InteractionOverlay translate =
            obol::TransformDragger::addTranslateAxisOverlay(scene,
                                                            translateOverlay);
        const obol::InteractionHandlePick translateRail =
            obol::TransformDragger::resolveOverlayHandle(
                translate,
                translate.objects.empty()
                    ? obol::InvalidSceneObjectId
                    : translate.objects[0]);
        const obol::InteractionHandlePick translateHandle =
            obol::TransformDragger::resolveOverlayHandle(
                translate,
                translate.objects.size() > 1
                    ? translate.objects[1]
                    : obol::InvalidSceneObjectId);
        const obol::InteractionHandlePick translateMiss =
            obol::TransformDragger::resolveOverlayHandle(translate, 999);

        obol::TrackballOverlay trackballOverlay;
        trackballOverlay.segments = 12;
        const obol::InteractionOverlay trackball =
            obol::TransformDragger::addTrackballOverlay(scene,
                                                        trackballOverlay);
        const obol::InteractionHandlePick xRing =
            obol::TransformDragger::resolveOverlayHandle(
                trackball,
                trackball.objects.empty()
                    ? obol::InvalidSceneObjectId
                    : trackball.objects[0]);
        const obol::InteractionHandlePick yRing =
            obol::TransformDragger::resolveOverlayHandle(
                trackball,
                trackball.objects.size() > 1
                    ? trackball.objects[1]
                    : obol::InvalidSceneObjectId);
        const obol::InteractionHandlePick zRing =
            obol::TransformDragger::resolveOverlayHandle(
                trackball,
                trackball.objects.size() > 2
                    ? trackball.objects[2]
                    : obol::InvalidSceneObjectId);

        const obol::SceneObjectId target =
            scene.addPrimitive(obol::Primitive::Cube);
        obol::ManipulatorOverlay transformOverlay;
        transformOverlay.target = target;
        transformOverlay.kind = obol::ManipulatorOverlayKind::TransformBox;
        obol::ManipulatorAttachment transformAttachment =
            obol::TransformDragger::attachManipulator(scene, transformOverlay);
        const obol::InteractionHandlePick scaleBox =
            obol::TransformDragger::resolveManipulatorHandle(
                transformAttachment,
                transformAttachment.overlay.objects.empty()
                    ? obol::InvalidSceneObjectId
                    : transformAttachment.overlay.objects[0]);

        const bool detached =
            obol::TransformDragger::detachManipulator(scene,
                                                      transformAttachment);
        const obol::InteractionHandlePick detachedPick =
            obol::TransformDragger::resolveManipulatorHandle(
                transformAttachment,
                scaleBox.object);

        const bool pass =
            translate.objects.size() == 3 &&
            translate.handles.size() == 3 &&
            translateRail.valid &&
            translateRail.kind == obol::InteractionHandleKind::TranslateAxis &&
            std::fabs(translateRail.axis.y - 1.0f) < 1.0e-5f &&
            translateHandle.valid &&
            translateHandle.kind == obol::InteractionHandleKind::TranslateAxis &&
            std::fabs(translateHandle.axis.y - 1.0f) < 1.0e-5f &&
            !translateMiss.valid &&
            trackball.objects.size() == 3 &&
            xRing.valid &&
            yRing.valid &&
            zRing.valid &&
            xRing.kind == obol::InteractionHandleKind::RotateAxis &&
            yRing.kind == obol::InteractionHandleKind::RotateAxis &&
            zRing.kind == obol::InteractionHandleKind::RotateAxis &&
            std::fabs(xRing.axis.x - 1.0f) < 1.0e-5f &&
            std::fabs(yRing.axis.y - 1.0f) < 1.0e-5f &&
            std::fabs(zRing.axis.z - 1.0f) < 1.0e-5f &&
            scaleBox.valid &&
            scaleBox.kind == obol::InteractionHandleKind::ScaleUniform &&
            detached &&
            !detachedPick.valid &&
            transformAttachment.overlay.handles.empty();
        runner.endTest(pass, pass ? "" : "Overlay handle routing did not resolve picked object IDs");
    }

    runner.startTest("v2 TransformDragger dispatches picked handles to edits");
    {
        obol::Scene scene;
        obol::Transform start;
        start.translation = {1.0f, 2.0f, 3.0f};
        start.scale = {1.0f, 1.0f, 1.0f};
        const obol::SceneObjectId target =
            scene.addPrimitive(obol::Primitive::Cube,
                               obol::Material{},
                               start);

        obol::TranslateAxisOverlay translateOverlay;
        translateOverlay.axis = {0.0f, 3.0f, 0.0f};
        const obol::InteractionOverlay translate =
            obol::TransformDragger::addTranslateAxisOverlay(scene,
                                                            translateOverlay);
        const obol::InteractionHandlePick translatePick =
            obol::TransformDragger::resolveOverlayHandle(
                translate,
                translate.objects.empty()
                    ? obol::InvalidSceneObjectId
                    : translate.objects[0]);
        obol::TransformEditState translateEdit =
            obol::TransformDragger::beginTransformEdit(scene, target);
        obol::InteractionHandleEditRequest translateRequest;
        translateRequest.handle = translatePick;
        translateRequest.translation.distance = 2.26f;
        translateRequest.translation.snap = true;
        translateRequest.translation.snapStep = 0.5f;
        obol::InteractionHandleEditResult translateResult;
        const bool translated =
            obol::TransformDragger::updateEditFromHandle(scene,
                                                         translateEdit,
                                                         translateRequest,
                                                         nullptr,
                                                         &translateResult);
        const bool committedTranslation =
            obol::TransformDragger::commitTransformEdit(translateEdit);
        obol::Transform afterTranslation;
        const bool gotTranslation =
            scene.getObjectTransform(target, afterTranslation);

        obol::ManipulatorOverlay trackballOverlay;
        trackballOverlay.target = target;
        trackballOverlay.kind = obol::ManipulatorOverlayKind::Trackball;
        trackballOverlay.segments = 12;
        obol::ManipulatorAttachment trackballAttachment =
            obol::TransformDragger::attachManipulator(scene, trackballOverlay);
        const obol::InteractionHandlePick rotationPick =
            obol::TransformDragger::resolveManipulatorHandle(
                trackballAttachment,
                trackballAttachment.overlay.objects.size() > 2
                    ? trackballAttachment.overlay.objects[2]
                    : obol::InvalidSceneObjectId);
        obol::TransformEditState rotationEdit =
            obol::TransformDragger::beginTransformEdit(scene, target);
        obol::InteractionHandleEditRequest rotationRequest;
        rotationRequest.handle = rotationPick;
        rotationRequest.rotation.angleRadians = 1.12f;
        rotationRequest.rotation.snap = true;
        rotationRequest.rotation.snapStepRadians = 0.25f;
        obol::InteractionHandleEditResult rotationResult;
        const bool rotated =
            obol::TransformDragger::updateEditFromHandle(scene,
                                                         rotationEdit,
                                                         rotationRequest,
                                                         &trackballAttachment,
                                                         &rotationResult);
        const bool committedRotation =
            obol::TransformDragger::commitTransformEdit(rotationEdit);
        obol::Transform afterRotation;
        obol::Transform rotationOverlay;
        const bool gotRotation =
            scene.getObjectTransform(target, afterRotation);
        const bool gotRotationOverlay =
            scene.getGroupTransform(trackballAttachment.overlay.group,
                                    rotationOverlay);

        obol::ManipulatorOverlay transformOverlay;
        transformOverlay.target = target;
        transformOverlay.kind = obol::ManipulatorOverlayKind::TransformBox;
        obol::ManipulatorAttachment transformAttachment =
            obol::TransformDragger::attachManipulator(scene, transformOverlay);
        const obol::InteractionHandlePick scalePick =
            obol::TransformDragger::resolveManipulatorHandle(
                transformAttachment,
                transformAttachment.overlay.objects.empty()
                    ? obol::InvalidSceneObjectId
                    : transformAttachment.overlay.objects[0]);
        obol::TransformEditState scaleEdit =
            obol::TransformDragger::beginTransformEdit(scene, target);
        obol::InteractionHandleEditRequest scaleRequest;
        scaleRequest.handle = scalePick;
        scaleRequest.scale.factors = {1.5f, 2.0f, 2.5f};
        scaleRequest.scale.bounds.enabled = true;
        scaleRequest.scale.bounds.minimum = {1.0f, 1.0f, 1.0f};
        scaleRequest.scale.bounds.maximum = {2.0f, 2.0f, 2.0f};
        obol::InteractionHandleEditResult scaleResult;
        const bool scaled =
            obol::TransformDragger::updateEditFromHandle(scene,
                                                         scaleEdit,
                                                         scaleRequest,
                                                         &transformAttachment,
                                                         &scaleResult);
        const bool committedScale =
            obol::TransformDragger::commitTransformEdit(scaleEdit);
        obol::Transform afterScale;
        obol::Transform scaleOverlay;
        const bool gotScale = scene.getObjectTransform(target, afterScale);
        const bool gotScaleOverlay =
            scene.getGroupTransform(transformAttachment.overlay.group,
                                    scaleOverlay);

        obol::ManipulatorOverlay handleBoxOverlay;
        handleBoxOverlay.target = target;
        handleBoxOverlay.kind = obol::ManipulatorOverlayKind::HandleBox;
        obol::ManipulatorAttachment handleBoxAttachment =
            obol::TransformDragger::attachManipulator(scene, handleBoxOverlay);
        const obol::InteractionHandlePick boundsPick =
            obol::TransformDragger::resolveManipulatorHandle(
                handleBoxAttachment,
                handleBoxAttachment.overlay.objects.empty()
                    ? obol::InvalidSceneObjectId
                    : handleBoxAttachment.overlay.objects[0]);
        obol::TransformEditState rejectedEdit =
            obol::TransformDragger::beginTransformEdit(scene, target);
        obol::InteractionHandleEditRequest rejectedRequest;
        rejectedRequest.handle = boundsPick;
        obol::InteractionHandleEditResult rejectedResult;
        const bool rejected =
            !obol::TransformDragger::updateEditFromHandle(scene,
                                                          rejectedEdit,
                                                          rejectedRequest,
                                                          &handleBoxAttachment,
                                                          &rejectedResult);
        const bool canceledRejected =
            obol::TransformDragger::cancelTransformEdit(scene,
                                                        rejectedEdit,
                                                        &handleBoxAttachment);

        const bool pass =
            translated &&
            translateResult.applied &&
            translateResult.kind == obol::InteractionHandleKind::TranslateAxis &&
            translateResult.translation.valid &&
            committedTranslation &&
            gotTranslation &&
            std::fabs(afterTranslation.translation.x - 1.0f) < 1.0e-5f &&
            std::fabs(afterTranslation.translation.y - 4.5f) < 1.0e-5f &&
            std::fabs(afterTranslation.translation.z - 3.0f) < 1.0e-5f &&
            rotated &&
            rotationResult.applied &&
            rotationResult.kind == obol::InteractionHandleKind::RotateAxis &&
            rotationResult.rotation.valid &&
            committedRotation &&
            gotRotation &&
            gotRotationOverlay &&
            std::fabs(afterRotation.rotationAxis.z - 1.0f) < 1.0e-5f &&
            std::fabs(afterRotation.rotationRadians - 1.0f) < 1.0e-5f &&
            std::fabs(rotationOverlay.rotationRadians - 1.0f) < 1.0e-5f &&
            scaled &&
            scaleResult.applied &&
            scaleResult.kind == obol::InteractionHandleKind::ScaleUniform &&
            scaleResult.scale.valid &&
            committedScale &&
            gotScale &&
            gotScaleOverlay &&
            std::fabs(afterScale.scale.x - 1.5f) < 1.0e-5f &&
            std::fabs(afterScale.scale.y - 2.0f) < 1.0e-5f &&
            std::fabs(afterScale.scale.z - 2.0f) < 1.0e-5f &&
            std::fabs(scaleOverlay.scale.z - 2.0f) < 1.0e-5f &&
            rejected &&
            !rejectedResult.applied &&
            rejectedResult.kind == obol::InteractionHandleKind::BoundsBox &&
            canceledRejected;
        runner.endTest(pass, pass ? "" : "Picked handle dispatch did not apply expected edit operations");
    }

    runner.startTest("v2 TransformDragger manages manipulator edit sessions");
    {
        obol::Scene scene;
        obol::Transform start;
        start.translation = {2.0f, 3.0f, 4.0f};
        const obol::SceneObjectId target =
            scene.addPrimitive(obol::Primitive::Cube,
                               obol::Material{},
                               start);

        obol::ManipulatorOverlay trackballOverlay;
        trackballOverlay.target = target;
        trackballOverlay.kind = obol::ManipulatorOverlayKind::Trackball;
        trackballOverlay.segments = 12;
        obol::ManipulatorAttachment trackballAttachment =
            obol::TransformDragger::attachManipulator(scene, trackballOverlay);
        obol::ManipulatorEditSession rotationSession =
            obol::TransformDragger::beginManipulatorEdit(
                scene,
                trackballAttachment,
                trackballAttachment.overlay.objects.size() > 2
                    ? trackballAttachment.overlay.objects[2]
                    : obol::InvalidSceneObjectId);

        obol::ManipulatorEditRequest rotationRequest;
        rotationRequest.rotation.angleRadians = 1.12f;
        rotationRequest.rotation.snap = true;
        rotationRequest.rotation.snapStepRadians = 0.25f;
        obol::InteractionHandleEditResult rotationResult;
        const bool rotated =
            obol::TransformDragger::updateManipulatorEdit(
                scene,
                rotationSession,
                rotationRequest,
                trackballAttachment,
                &rotationResult);
        const bool committedRotation =
            obol::TransformDragger::commitManipulatorEdit(rotationSession);

        obol::Transform afterRotation;
        obol::Transform rotationOverlayTransform;
        const bool gotRotation =
            scene.getObjectTransform(target, afterRotation);
        const bool gotRotationOverlay =
            scene.getGroupTransform(trackballAttachment.overlay.group,
                                    rotationOverlayTransform);

        obol::ManipulatorOverlay transformOverlay;
        transformOverlay.target = target;
        transformOverlay.kind = obol::ManipulatorOverlayKind::TransformBox;
        obol::ManipulatorAttachment transformAttachment =
            obol::TransformDragger::attachManipulator(scene, transformOverlay);
        obol::ManipulatorEditSession scaleSession =
            obol::TransformDragger::beginManipulatorEdit(
                scene,
                transformAttachment,
                transformAttachment.overlay.objects.empty()
                    ? obol::InvalidSceneObjectId
                    : transformAttachment.overlay.objects[0]);

        obol::ManipulatorEditRequest scaleRequest;
        scaleRequest.scale.factors = {2.0f, 3.0f, 4.0f};
        scaleRequest.scale.bounds.enabled = true;
        scaleRequest.scale.bounds.minimum = {1.0f, 1.0f, 1.0f};
        scaleRequest.scale.bounds.maximum = {2.0f, 2.0f, 2.0f};
        obol::InteractionHandleEditResult scaleResult;
        const bool scaled =
            obol::TransformDragger::updateManipulatorEdit(scene,
                                                          scaleSession,
                                                          scaleRequest,
                                                          transformAttachment,
                                                          &scaleResult);

        obol::Transform duringScale;
        obol::Transform duringScaleOverlay;
        const bool gotDuringScale =
            scene.getObjectTransform(target, duringScale);
        const bool gotDuringScaleOverlay =
            scene.getGroupTransform(transformAttachment.overlay.group,
                                    duringScaleOverlay);
        const bool canceledScale =
            obol::TransformDragger::cancelManipulatorEdit(scene,
                                                          scaleSession,
                                                          &transformAttachment);

        obol::Transform afterScaleCancel;
        obol::Transform afterScaleCancelOverlay;
        const bool gotAfterScaleCancel =
            scene.getObjectTransform(target, afterScaleCancel);
        const bool gotAfterScaleCancelOverlay =
            scene.getGroupTransform(transformAttachment.overlay.group,
                                    afterScaleCancelOverlay);

        obol::ManipulatorOverlay handleBoxOverlay;
        handleBoxOverlay.target = target;
        handleBoxOverlay.kind = obol::ManipulatorOverlayKind::HandleBox;
        obol::ManipulatorAttachment handleBoxAttachment =
            obol::TransformDragger::attachManipulator(scene, handleBoxOverlay);
        obol::ManipulatorEditSession boundsSession =
            obol::TransformDragger::beginManipulatorEdit(
                scene,
                handleBoxAttachment,
                handleBoxAttachment.overlay.objects.empty()
                    ? obol::InvalidSceneObjectId
                    : handleBoxAttachment.overlay.objects[0]);
        obol::ManipulatorEditRequest invalidRequest;
        obol::InteractionHandleEditResult invalidResult;
        const bool rejectedBoundsUpdate =
            !obol::TransformDragger::updateManipulatorEdit(scene,
                                                           boundsSession,
                                                           invalidRequest,
                                                           handleBoxAttachment,
                                                           &invalidResult);

        obol::ManipulatorEditSession missingSession =
            obol::TransformDragger::beginManipulatorEdit(scene,
                                                         trackballAttachment,
                                                         999);

        obol::ManipulatorAttachment detachedAttachment = transformAttachment;
        const bool detached =
            obol::TransformDragger::detachManipulator(scene,
                                                      detachedAttachment);
        obol::ManipulatorEditSession detachedSession =
            obol::TransformDragger::beginManipulatorEdit(
                scene,
                detachedAttachment,
                scaleResult.scale.target);

        const bool pass =
            rotationSession.active == false &&
            rotationResult.applied &&
            rotationResult.kind == obol::InteractionHandleKind::RotateAxis &&
            rotated &&
            committedRotation &&
            gotRotation &&
            gotRotationOverlay &&
            std::fabs(afterRotation.translation.x - 2.0f) < 1.0e-5f &&
            std::fabs(afterRotation.rotationAxis.z - 1.0f) < 1.0e-5f &&
            std::fabs(afterRotation.rotationRadians - 1.0f) < 1.0e-5f &&
            std::fabs(rotationOverlayTransform.rotationRadians - 1.0f) < 1.0e-5f &&
            scaleSession.active == false &&
            scaleResult.applied &&
            scaleResult.kind == obol::InteractionHandleKind::ScaleUniform &&
            scaled &&
            gotDuringScale &&
            gotDuringScaleOverlay &&
            std::fabs(duringScale.scale.x - 2.0f) < 1.0e-5f &&
            std::fabs(duringScale.scale.y - 2.0f) < 1.0e-5f &&
            std::fabs(duringScale.scale.z - 2.0f) < 1.0e-5f &&
            std::fabs(duringScaleOverlay.scale.z - 2.0f) < 1.0e-5f &&
            canceledScale &&
            gotAfterScaleCancel &&
            gotAfterScaleCancelOverlay &&
            std::fabs(afterScaleCancel.scale.x - 1.0f) < 1.0e-5f &&
            std::fabs(afterScaleCancel.scale.y - 1.0f) < 1.0e-5f &&
            std::fabs(afterScaleCancel.scale.z - 1.0f) < 1.0e-5f &&
            std::fabs(afterScaleCancel.rotationRadians - 1.0f) < 1.0e-5f &&
            std::fabs(afterScaleCancelOverlay.scale.x - 1.0f) < 1.0e-5f &&
            !boundsSession.active &&
            rejectedBoundsUpdate &&
            !invalidResult.applied &&
            !missingSession.active &&
            detached &&
            !detachedSession.active;
        runner.endTest(pass, pass ? "" : "Manipulator edit session did not manage handle edits correctly");
    }

    runner.startTest("v2 TransformDragger maps pointer drags to manipulator edits");
    {
        obol::Scene scene;
        obol::Transform start;
        start.translation = {1.0f, 2.0f, 3.0f};
        const obol::SceneObjectId target =
            scene.addPrimitive(obol::Primitive::Cube,
                               obol::Material{},
                               start);

        obol::TransformEditState translationEdit =
            obol::TransformDragger::beginTransformEdit(scene, target);
        obol::ManipulatorEditSession translationSession;
        translationSession.active = true;
        translationSession.handle.valid = true;
        translationSession.handle.kind = obol::InteractionHandleKind::TranslateAxis;
        translationSession.handle.axis = {0.0f, 1.0f, 0.0f};
        translationSession.transformEdit = translationEdit;

        obol::PointerDragGesture translationGesture;
        translationGesture.delta = {3.0f, 4.0f};
        translationGesture.handleScreenDirection = {0.0f, 2.0f};
        translationGesture.translationUnitsPerPixel = 0.5f;
        translationGesture.translation.snap = true;
        translationGesture.translation.snapStep = 0.25f;
        const obol::PointerDragGestureResult translationGestureResult =
            obol::TransformDragger::mapPointerDragToManipulatorEdit(
                translationSession,
                translationGesture);

        obol::InteractionHandleEditRequest translationRequest;
        translationRequest.handle = translationSession.handle;
        translationRequest.translation =
            translationGestureResult.edit.translation;
        obol::InteractionHandleEditResult translationResult;
        const bool translated =
            obol::TransformDragger::updateEditFromHandle(scene,
                                                         translationEdit,
                                                         translationRequest,
                                                         nullptr,
                                                         &translationResult);
        const bool committedTranslation =
            obol::TransformDragger::commitTransformEdit(translationEdit);
        obol::Transform afterTranslation;
        const bool gotTranslation =
            scene.getObjectTransform(target, afterTranslation);

        obol::ManipulatorOverlay trackballOverlay;
        trackballOverlay.target = target;
        trackballOverlay.kind = obol::ManipulatorOverlayKind::Trackball;
        trackballOverlay.segments = 12;
        obol::ManipulatorAttachment trackballAttachment =
            obol::TransformDragger::attachManipulator(scene, trackballOverlay);
        obol::ManipulatorEditSession rotationSession =
            obol::TransformDragger::beginManipulatorEdit(
                scene,
                trackballAttachment,
                trackballAttachment.overlay.objects.size() > 2
                    ? trackballAttachment.overlay.objects[2]
                    : obol::InvalidSceneObjectId);

        obol::PointerDragGesture rotationGesture;
        rotationGesture.delta = {10.0f, 0.0f};
        rotationGesture.handleScreenDirection = {2.0f, 0.0f};
        rotationGesture.rotationRadiansPerPixel = 0.05f;
        rotationGesture.rotation.snap = true;
        rotationGesture.rotation.snapStepRadians = 0.25f;
        const obol::PointerDragGestureResult rotationGestureResult =
            obol::TransformDragger::mapPointerDragToManipulatorEdit(
                rotationSession,
                rotationGesture);
        obol::InteractionHandleEditResult rotationResult;
        const bool rotated =
            obol::TransformDragger::updateManipulatorEdit(
                scene,
                rotationSession,
                rotationGestureResult.edit,
                trackballAttachment,
                &rotationResult);
        const bool committedRotation =
            obol::TransformDragger::commitManipulatorEdit(rotationSession);
        obol::Transform afterRotation;
        obol::Transform rotationOverlayTransform;
        const bool gotRotation =
            scene.getObjectTransform(target, afterRotation);
        const bool gotRotationOverlay =
            scene.getGroupTransform(trackballAttachment.overlay.group,
                                    rotationOverlayTransform);

        obol::ManipulatorOverlay transformOverlay;
        transformOverlay.target = target;
        transformOverlay.kind = obol::ManipulatorOverlayKind::TransformBox;
        obol::ManipulatorAttachment transformAttachment =
            obol::TransformDragger::attachManipulator(scene, transformOverlay);
        obol::ManipulatorEditSession scaleSession =
            obol::TransformDragger::beginManipulatorEdit(
                scene,
                transformAttachment,
                transformAttachment.overlay.objects.empty()
                    ? obol::InvalidSceneObjectId
                    : transformAttachment.overlay.objects[0]);

        obol::PointerDragGesture scaleGesture;
        scaleGesture.delta = {0.0f, -5.0f};
        scaleGesture.handleScreenDirection = {0.0f, -1.0f};
        scaleGesture.scaleFactorPerPixel = 0.1f;
        scaleGesture.scale.bounds.enabled = true;
        scaleGesture.scale.bounds.minimum = {0.5f, 0.5f, 0.5f};
        scaleGesture.scale.bounds.maximum = {2.0f, 2.0f, 2.0f};
        const obol::PointerDragGestureResult scaleGestureResult =
            obol::TransformDragger::mapPointerDragToManipulatorEdit(
                scaleSession,
                scaleGesture);
        obol::InteractionHandleEditResult scaleResult;
        const bool scaled =
            obol::TransformDragger::updateManipulatorEdit(scene,
                                                          scaleSession,
                                                          scaleGestureResult.edit,
                                                          transformAttachment,
                                                          &scaleResult);
        const bool committedScale =
            obol::TransformDragger::commitManipulatorEdit(scaleSession);
        obol::Transform afterScale;
        obol::Transform scaleOverlayTransform;
        const bool gotScale = scene.getObjectTransform(target, afterScale);
        const bool gotScaleOverlay =
            scene.getGroupTransform(transformAttachment.overlay.group,
                                    scaleOverlayTransform);

        obol::ManipulatorEditSession invalidSession;
        invalidSession.active = true;
        invalidSession.handle.valid = true;
        invalidSession.handle.kind = obol::InteractionHandleKind::BoundsBox;
        obol::PointerDragGesture invalidGesture;
        invalidGesture.delta = {10.0f, 10.0f};
        const obol::PointerDragGestureResult invalidGestureResult =
            obol::TransformDragger::mapPointerDragToManipulatorEdit(
                invalidSession,
                invalidGesture);

        obol::ManipulatorEditSession inactiveSession;
        inactiveSession.handle.valid = true;
        inactiveSession.handle.kind = obol::InteractionHandleKind::RotateAxis;
        const obol::PointerDragGestureResult inactiveGestureResult =
            obol::TransformDragger::mapPointerDragToManipulatorEdit(
                inactiveSession,
                rotationGesture);

        const bool pass =
            translationGestureResult.valid &&
            translationGestureResult.kind == obol::InteractionHandleKind::TranslateAxis &&
            std::fabs(translationGestureResult.projectedDelta - 4.0f) < 1.0e-5f &&
            std::fabs(translationGestureResult.edit.translation.distance - 2.0f) < 1.0e-5f &&
            translated &&
            translationResult.applied &&
            committedTranslation &&
            gotTranslation &&
            std::fabs(afterTranslation.translation.y - 4.0f) < 1.0e-5f &&
            rotationGestureResult.valid &&
            rotationGestureResult.kind == obol::InteractionHandleKind::RotateAxis &&
            std::fabs(rotationGestureResult.projectedDelta - 10.0f) < 1.0e-5f &&
            std::fabs(rotationGestureResult.edit.rotation.angleRadians - 0.5f) < 1.0e-5f &&
            rotated &&
            rotationResult.applied &&
            committedRotation &&
            gotRotation &&
            gotRotationOverlay &&
            std::fabs(afterRotation.rotationRadians - 0.5f) < 1.0e-5f &&
            std::fabs(rotationOverlayTransform.rotationRadians - 0.5f) < 1.0e-5f &&
            scaleGestureResult.valid &&
            scaleGestureResult.kind == obol::InteractionHandleKind::ScaleUniform &&
            std::fabs(scaleGestureResult.projectedDelta - 5.0f) < 1.0e-5f &&
            std::fabs(scaleGestureResult.edit.scale.factors.x - 1.5f) < 1.0e-5f &&
            scaled &&
            scaleResult.applied &&
            committedScale &&
            gotScale &&
            gotScaleOverlay &&
            std::fabs(afterScale.scale.x - 1.5f) < 1.0e-5f &&
            std::fabs(afterScale.scale.y - 1.5f) < 1.0e-5f &&
            std::fabs(afterScale.scale.z - 1.5f) < 1.0e-5f &&
            std::fabs(scaleOverlayTransform.scale.x - 1.5f) < 1.0e-5f &&
            !invalidGestureResult.valid &&
            invalidGestureResult.kind == obol::InteractionHandleKind::BoundsBox &&
            !inactiveGestureResult.valid &&
            inactiveGestureResult.kind == obol::InteractionHandleKind::RotateAxis;
        runner.endTest(pass, pass ? "" : "Pointer drag gesture mapping did not produce expected manipulator edits");
    }

    runner.startTest("v2 TransformDragger syncs manipulator overlays to targets");
    {
        obol::Scene scene;
        obol::Transform start;
        start.translation = {1.0f, 0.0f, 0.0f};
        const obol::SceneObjectId target =
            scene.addPrimitive(obol::Primitive::Cube, obol::Material{}, start);

        obol::ManipulatorOverlay overlay;
        overlay.target = target;
        overlay.kind = obol::ManipulatorOverlayKind::HandleBox;
        overlay.boxHalfSize = {1.0f, 1.0f, 1.0f};

        const obol::InteractionOverlay created =
            obol::TransformDragger::addManipulatorOverlay(scene, overlay);

        obol::Transform moved;
        moved.translation = {3.0f, 4.0f, 5.0f};
        moved.rotationAxis = {0.0f, 1.0f, 0.0f};
        moved.rotationRadians = 0.5f;
        moved.scale = {2.0f, 2.0f, 2.0f};

        const bool movedTarget = scene.setObjectTransform(target, moved);
        const bool synced =
            obol::TransformDragger::syncOverlayToTarget(scene, created);
        obol::Transform targetTransform;
        obol::Transform groupTransform;
        const bool gotTarget = scene.getObjectTransform(target, targetTransform);
        const bool gotGroup = scene.getGroupTransform(created.group, groupTransform);

        SoSeparator * root = legacySceneGraph(scene);
        SoSeparator * group = findNamedSeparator(root, "ObolSceneGroup_1");
        SoTransform * transform = findFirstNodeOfType<SoTransform>(group);
        const SbVec3f translation = transform
            ? transform->translation.getValue()
            : SbVec3f(0.0f, 0.0f, 0.0f);
        const SbVec3f scale = transform
            ? transform->scaleFactor.getValue()
            : SbVec3f(0.0f, 0.0f, 0.0f);

        const bool pass =
            movedTarget &&
            synced &&
            gotTarget &&
            gotGroup &&
            std::fabs(targetTransform.translation.x - 3.0f) < 1.0e-5f &&
            std::fabs(groupTransform.translation.x - 3.0f) < 1.0e-5f &&
            std::fabs(groupTransform.translation.y - 4.0f) < 1.0e-5f &&
            std::fabs(groupTransform.translation.z - 5.0f) < 1.0e-5f &&
            std::fabs(groupTransform.scale.x - 2.0f) < 1.0e-5f &&
            group &&
            transform &&
            std::fabs(translation[0] - 3.0f) < 1.0e-5f &&
            std::fabs(translation[1] - 4.0f) < 1.0e-5f &&
            std::fabs(translation[2] - 5.0f) < 1.0e-5f &&
            std::fabs(scale[0] - 2.0f) < 1.0e-5f;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Manipulator overlay did not sync to target transform");
    }

    runner.startTest("v2 TransformDragger removes manipulator overlay objects");
    {
        obol::Scene scene;
        const obol::SceneObjectId target =
            scene.addPrimitive(obol::Primitive::Cube);

        obol::ManipulatorOverlay overlay;
        overlay.target = target;
        overlay.kind = obol::ManipulatorOverlayKind::TransformBox;
        overlay.boxHalfSize = {1.0f, 1.0f, 1.0f};

        obol::InteractionOverlay created =
            obol::TransformDragger::addManipulatorOverlay(scene, overlay);
        const bool removed =
            obol::TransformDragger::removeOverlay(scene, created);
        const bool removedAgain =
            obol::TransformDragger::removeOverlay(scene, created);

        obol::SceneQuery primitiveQuery;
        primitiveQuery.type = obol::SceneObjectType::Primitive;
        obol::SceneQuery lineQuery;
        lineQuery.type = obol::SceneObjectType::Polyline;

        const std::vector<obol::SceneObjectInfo> primitives =
            scene.findObjects(primitiveQuery);
        const std::vector<obol::SceneObjectInfo> lines =
            scene.findObjects(lineQuery);

        SoSeparator * root = legacySceneGraph(scene);
        SoSeparator * targetSep = findNamedSeparator(root, "ObolSceneObject_1");
        SoSeparator * overlaySep = findNamedSeparator(root, "ObolSceneObject_2");
        SoSeparator * overlayGroup = findNamedSeparator(root, "ObolSceneGroup_1");

        const bool pass =
            removed &&
            !removedAgain &&
            created.target == obol::InvalidSceneObjectId &&
            created.group == obol::InvalidSceneGroupId &&
            created.objects.empty() &&
            created.handles.empty() &&
            scene.objectCount() == 1 &&
            scene.groupCount() == 0 &&
            primitives.size() == 1 &&
            primitives[0].id == target &&
            lines.empty() &&
            targetSep != nullptr &&
            overlaySep == nullptr &&
            overlayGroup == nullptr;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Manipulator overlay lifecycle did not remove only overlay objects");
    }

    runner.startTest("v2 TransformDragger hides manipulator overlays");
    {
        obol::Scene scene;
        const obol::SceneObjectId target =
            scene.addPrimitive(obol::Primitive::Cube);

        obol::ManipulatorOverlay overlay;
        overlay.target = target;
        overlay.kind = obol::ManipulatorOverlayKind::Trackball;
        overlay.trackballRadius = 1.0f;
        overlay.segments = 12;

        const obol::InteractionOverlay created =
            obol::TransformDragger::addManipulatorOverlay(scene, overlay);
        const obol::ScenePacket visiblePacket = scene.capturePacket();
        const bool hid =
            obol::TransformDragger::setOverlayVisible(scene, created, false);
        const obol::ScenePacket hiddenPacket = scene.capturePacket();
        const bool showed =
            obol::TransformDragger::setOverlayVisible(scene, created, true);
        const obol::ScenePacket shownPacket = scene.capturePacket();

        obol::SceneQuery lineQuery;
        lineQuery.type = obol::SceneObjectType::Polyline;
        const std::vector<obol::SceneObjectInfo> lineInfos =
            scene.findObjects(lineQuery);

        SoSeparator * root = legacySceneGraph(scene);
        SoSeparator * targetSep = findNamedSeparator(root, "ObolSceneObject_1");
        SoSeparator * overlayGroup = findNamedSeparator(root, "ObolSceneGroup_1");

        const bool pass =
            hid &&
            showed &&
            scene.objectCount() == 4 &&
            scene.groupCount() == 1 &&
            lineInfos.size() == 3 &&
            visiblePacket.groups.size() == 1 &&
            visiblePacket.objects.size() == 4 &&
            hiddenPacket.groups.empty() &&
            hiddenPacket.objects.size() == 1 &&
            hiddenPacket.objects[0].id == target &&
            shownPacket.groups.size() == 1 &&
            shownPacket.objects.size() == 4 &&
            targetSep != nullptr &&
            overlayGroup != nullptr;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Manipulator overlay visibility did not hide only overlay geometry");
    }

    runner.startTest("v2 TransformDragger manages persistent manipulator attachments");
    {
        obol::Scene scene;
        obol::Transform targetStart;
        targetStart.translation = {1.0f, 2.0f, 3.0f};
        targetStart.scale = {1.5f, 1.5f, 1.5f};
        const obol::SceneObjectId target =
            scene.addPrimitive(obol::Primitive::Cube,
                               obol::Material{},
                               targetStart);

        obol::ManipulatorOverlay overlay;
        overlay.target = target;
        overlay.kind = obol::ManipulatorOverlayKind::Trackball;
        overlay.trackballRadius = 1.25f;
        overlay.segments = 12;

        obol::ManipulatorAttachment attachment =
            obol::TransformDragger::attachManipulator(scene, overlay);
        const bool attachedState =
            attachment.attached &&
            attachment.visible &&
            attachment.target == target &&
            attachment.kind == obol::ManipulatorOverlayKind::Trackball &&
            attachment.overlay.target == target &&
            attachment.overlay.group != obol::InvalidSceneGroupId &&
            attachment.overlay.objects.size() == 3;

        obol::Transform initialGroupTransform;
        const bool gotInitialGroup =
            scene.getGroupTransform(attachment.overlay.group,
                                    initialGroupTransform);

        obol::Transform targetMoved = targetStart;
        targetMoved.translation = {4.0f, 5.0f, 6.0f};
        targetMoved.scale = {2.0f, 2.0f, 2.0f};
        const bool movedTarget = scene.setObjectTransform(target, targetMoved);
        const bool synced =
            obol::TransformDragger::syncManipulator(scene, attachment);

        obol::Transform syncedGroupTransform;
        const bool gotSyncedGroup =
            scene.getGroupTransform(attachment.overlay.group,
                                    syncedGroupTransform);

        const obol::ScenePacket visiblePacket = scene.capturePacket();
        const bool hid =
            obol::TransformDragger::setManipulatorVisible(scene,
                                                          attachment,
                                                          false);
        const bool hiddenState = !attachment.visible;
        const obol::ScenePacket hiddenPacket = scene.capturePacket();
        const bool showed =
            obol::TransformDragger::setManipulatorVisible(scene,
                                                          attachment,
                                                          true);
        const bool shownState = attachment.visible;
        const obol::ScenePacket shownPacket = scene.capturePacket();

        const obol::SceneGroupId overlayGroup = attachment.overlay.group;
        const std::vector<obol::SceneObjectId> overlayObjects =
            attachment.overlay.objects;
        const bool detached =
            obol::TransformDragger::detachManipulator(scene, attachment);
        const bool detachedAgain =
            obol::TransformDragger::detachManipulator(scene, attachment);

        obol::SceneQuery primitiveQuery;
        primitiveQuery.type = obol::SceneObjectType::Primitive;
        obol::SceneQuery lineQuery;
        lineQuery.type = obol::SceneObjectType::Polyline;
        const std::vector<obol::SceneObjectInfo> primitives =
            scene.findObjects(primitiveQuery);
        const std::vector<obol::SceneObjectInfo> lines =
            scene.findObjects(lineQuery);

        SoSeparator * root = legacySceneGraph(scene);
        SoSeparator * targetSep = findNamedSeparator(root, "ObolSceneObject_1");
        SoSeparator * overlaySep = findNamedSeparator(root, "ObolSceneObject_2");
        SoSeparator * overlayGroupSep =
            findNamedSeparator(root, "ObolSceneGroup_1");

        const bool pass =
            attachedState &&
            gotInitialGroup &&
            std::fabs(initialGroupTransform.translation.x - 1.0f) < 1.0e-5f &&
            std::fabs(initialGroupTransform.translation.y - 2.0f) < 1.0e-5f &&
            std::fabs(initialGroupTransform.translation.z - 3.0f) < 1.0e-5f &&
            movedTarget &&
            synced &&
            gotSyncedGroup &&
            std::fabs(syncedGroupTransform.translation.x - 4.0f) < 1.0e-5f &&
            std::fabs(syncedGroupTransform.translation.y - 5.0f) < 1.0e-5f &&
            std::fabs(syncedGroupTransform.translation.z - 6.0f) < 1.0e-5f &&
            std::fabs(syncedGroupTransform.scale.x - 2.0f) < 1.0e-5f &&
            visiblePacket.groups.size() == 1 &&
            visiblePacket.objects.size() == 4 &&
            hid &&
            hiddenState &&
            hiddenPacket.groups.empty() &&
            hiddenPacket.objects.size() == 1 &&
            hiddenPacket.objects[0].id == target &&
            showed &&
            shownState &&
            shownPacket.groups.size() == 1 &&
            shownPacket.objects.size() == 4 &&
            overlayGroup != obol::InvalidSceneGroupId &&
            overlayObjects.size() == 3 &&
            detached &&
            !detachedAgain &&
            !attachment.attached &&
            !attachment.visible &&
            attachment.target == obol::InvalidSceneObjectId &&
            attachment.overlay.group == obol::InvalidSceneGroupId &&
            attachment.overlay.objects.empty() &&
            attachment.overlay.handles.empty() &&
            scene.objectCount() == 1 &&
            scene.groupCount() == 0 &&
            primitives.size() == 1 &&
            primitives[0].id == target &&
            lines.empty() &&
            targetSep != nullptr &&
            overlaySep == nullptr &&
            overlayGroupSep == nullptr;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Manipulator attachment lifecycle state diverged from scene state");
    }

    runner.startTest("v2 TransformDragger rejects invalid manipulator attachments");
    {
        obol::Scene scene;

        obol::ManipulatorOverlay missingTarget;
        missingTarget.target = 99;
        missingTarget.kind = obol::ManipulatorOverlayKind::HandleBox;

        obol::ManipulatorAttachment attachment =
            obol::TransformDragger::attachManipulator(scene, missingTarget);
        const bool synced =
            obol::TransformDragger::syncManipulator(scene, attachment);
        const bool hid =
            obol::TransformDragger::setManipulatorVisible(scene,
                                                          attachment,
                                                          false);
        const bool detached =
            obol::TransformDragger::detachManipulator(scene, attachment);

        const bool pass =
            !attachment.attached &&
            !attachment.visible &&
            attachment.target == obol::InvalidSceneObjectId &&
            attachment.overlay.group == obol::InvalidSceneGroupId &&
            attachment.overlay.objects.empty() &&
            !synced &&
            !hid &&
            !detached &&
            scene.objectCount() == 0 &&
            scene.groupCount() == 0;
        runner.endTest(pass, pass ? "" : "Invalid manipulator attachment created scene state");
    }

    runner.startTest("v2 TransformDragger controller applies edits and syncs attachments");
    {
        obol::Scene scene;
        obol::Transform start;
        start.translation = {1.0f, 2.0f, 3.0f};
        start.scale = {1.0f, 1.0f, 1.0f};
        const obol::SceneObjectId target =
            scene.addPrimitive(obol::Primitive::Cube,
                               obol::Material{},
                               start);

        obol::ManipulatorOverlay overlay;
        overlay.target = target;
        overlay.kind = obol::ManipulatorOverlayKind::TransformBox;
        overlay.boxHalfSize = {1.0f, 1.0f, 1.0f};
        obol::ManipulatorAttachment attachment =
            obol::TransformDragger::attachManipulator(scene, overlay);

        obol::TransformEditState translationEdit =
            obol::TransformDragger::beginTransformEdit(scene, target);
        obol::AxisDragRequest axisRequest;
        axisRequest.axis = {1.0f, 0.0f, 0.0f};
        axisRequest.distance = 2.24f;
        axisRequest.snap = true;
        axisRequest.snapStep = 0.5f;
        obol::AxisDragResult axisResult;
        const bool translated =
            obol::TransformDragger::updateEditAxisTranslation(
                scene,
                translationEdit,
                axisRequest,
                &attachment,
                &axisResult);
        const bool committedTranslation =
            obol::TransformDragger::commitTransformEdit(translationEdit);

        obol::Transform afterTranslation;
        obol::Transform afterTranslationOverlay;
        const bool gotTranslation =
            scene.getObjectTransform(target, afterTranslation);
        const bool gotTranslationOverlay =
            scene.getGroupTransform(attachment.overlay.group,
                                    afterTranslationOverlay);

        obol::TransformEditState scaleEdit =
            obol::TransformDragger::beginTransformEdit(scene, target);
        obol::ScaleRequest scaleRequest;
        scaleRequest.factors = {2.0f, 3.0f, 4.0f};
        scaleRequest.bounds.enabled = true;
        scaleRequest.bounds.minimum = {0.5f, 0.5f, 0.5f};
        scaleRequest.bounds.maximum = {3.0f, 3.0f, 3.0f};
        obol::ScaleResult scaleResult;
        const bool scaled =
            obol::TransformDragger::updateEditScale(scene,
                                                    scaleEdit,
                                                    scaleRequest,
                                                    &attachment,
                                                    &scaleResult);
        const bool committedScale =
            obol::TransformDragger::commitTransformEdit(scaleEdit);

        obol::Transform afterScale;
        obol::Transform afterScaleOverlay;
        const bool gotScale = scene.getObjectTransform(target, afterScale);
        const bool gotScaleOverlay =
            scene.getGroupTransform(attachment.overlay.group,
                                    afterScaleOverlay);

        obol::TransformEditState rotationEdit =
            obol::TransformDragger::beginTransformEdit(scene, target);
        obol::AxisRotationRequest rotationRequest;
        rotationRequest.axis = {0.0f, 0.0f, 2.0f};
        rotationRequest.angleRadians = 1.12f;
        rotationRequest.snap = true;
        rotationRequest.snapStepRadians = 0.25f;
        obol::AxisRotationResult rotationResult;
        const bool rotated =
            obol::TransformDragger::updateEditAxisRotation(
                scene,
                rotationEdit,
                rotationRequest,
                &attachment,
                &rotationResult);
        const bool committedRotation =
            obol::TransformDragger::commitTransformEdit(rotationEdit);

        obol::Transform finalTransform;
        obol::Transform finalOverlayTransform;
        const bool gotFinal = scene.getObjectTransform(target, finalTransform);
        const bool gotFinalOverlay =
            scene.getGroupTransform(attachment.overlay.group,
                                    finalOverlayTransform);

        const bool pass =
            attachment.attached &&
            translationEdit.active == false &&
            scaleEdit.active == false &&
            rotationEdit.active == false &&
            translated &&
            axisResult.valid &&
            committedTranslation &&
            gotTranslation &&
            gotTranslationOverlay &&
            std::fabs(afterTranslation.translation.x - 3.0f) < 1.0e-5f &&
            std::fabs(afterTranslation.translation.y - 2.0f) < 1.0e-5f &&
            std::fabs(afterTranslationOverlay.translation.x - 3.0f) < 1.0e-5f &&
            scaled &&
            scaleResult.valid &&
            committedScale &&
            gotScale &&
            gotScaleOverlay &&
            std::fabs(afterScale.translation.x - 3.0f) < 1.0e-5f &&
            std::fabs(afterScale.scale.x - 2.0f) < 1.0e-5f &&
            std::fabs(afterScale.scale.y - 3.0f) < 1.0e-5f &&
            std::fabs(afterScale.scale.z - 3.0f) < 1.0e-5f &&
            std::fabs(afterScaleOverlay.scale.z - 3.0f) < 1.0e-5f &&
            rotated &&
            rotationResult.valid &&
            committedRotation &&
            gotFinal &&
            gotFinalOverlay &&
            std::fabs(finalTransform.translation.x - 3.0f) < 1.0e-5f &&
            std::fabs(finalTransform.scale.z - 3.0f) < 1.0e-5f &&
            std::fabs(finalTransform.rotationAxis.z - 1.0f) < 1.0e-5f &&
            std::fabs(finalTransform.rotationRadians - 1.0f) < 1.0e-5f &&
            std::fabs(finalOverlayTransform.rotationRadians - 1.0f) < 1.0e-5f;
        runner.endTest(pass, pass ? "" : "Transform edit controller failed to apply committed edits");
    }

    runner.startTest("v2 TransformDragger controller cancels edits and rejects invalid state");
    {
        obol::Scene scene;
        obol::Transform start;
        start.translation = {0.0f, 1.0f, 2.0f};
        const obol::SceneObjectId target =
            scene.addPrimitive(obol::Primitive::Sphere,
                               obol::Material{},
                               start);

        obol::ManipulatorOverlay overlay;
        overlay.target = target;
        overlay.kind = obol::ManipulatorOverlayKind::Trackball;
        overlay.trackballRadius = 1.0f;
        overlay.segments = 12;
        obol::ManipulatorAttachment attachment =
            obol::TransformDragger::attachManipulator(scene, overlay);

        obol::TransformEditState invalidEdit =
            obol::TransformDragger::beginTransformEdit(scene, 99);
        obol::FreeDragRequest invalidRequest;
        invalidRequest.delta = {5.0f, 0.0f, 0.0f};
        obol::TranslationResult invalidResult;
        const bool rejectedInvalid =
            !obol::TransformDragger::updateEditFreeTranslation(
                scene,
                invalidEdit,
                invalidRequest,
                &attachment,
                &invalidResult);
        const bool rejectedCommit =
            !obol::TransformDragger::commitTransformEdit(invalidEdit);

        obol::TransformEditState planeEdit =
            obol::TransformDragger::beginTransformEdit(scene, target);
        obol::PlaneDragRequest planeRequest;
        planeRequest.delta = {2.0f, 3.0f, 9.0f};
        planeRequest.planeNormal = {0.0f, 0.0f, 1.0f};
        obol::TranslationResult planeResult;
        const bool planeMoved =
            obol::TransformDragger::updateEditPlaneTranslation(
                scene,
                planeEdit,
                planeRequest,
                &attachment,
                &planeResult);

        obol::Transform duringPlane;
        obol::Transform duringPlaneOverlay;
        const bool gotDuringPlane =
            scene.getObjectTransform(target, duringPlane);
        const bool gotDuringPlaneOverlay =
            scene.getGroupTransform(attachment.overlay.group,
                                    duringPlaneOverlay);
        const bool canceledPlane =
            obol::TransformDragger::cancelTransformEdit(scene,
                                                        planeEdit,
                                                        &attachment);

        obol::Transform afterCancel;
        obol::Transform afterCancelOverlay;
        const bool gotAfterCancel =
            scene.getObjectTransform(target, afterCancel);
        const bool gotAfterCancelOverlay =
            scene.getGroupTransform(attachment.overlay.group,
                                    afterCancelOverlay);

        obol::TransformEditState freeEdit =
            obol::TransformDragger::beginTransformEdit(scene, target);
        obol::FreeDragRequest freeRequest;
        freeRequest.delta = {3.0f, -4.0f, 2.0f};
        freeRequest.bounds.enabled = true;
        freeRequest.bounds.minimum = {-1.0f, -1.0f, -1.0f};
        freeRequest.bounds.maximum = {2.0f, 2.0f, 3.0f};
        obol::TranslationResult freeResult;
        const bool freeMoved =
            obol::TransformDragger::updateEditFreeTranslation(
                scene,
                freeEdit,
                freeRequest,
                &attachment,
                &freeResult);

        obol::Transform duringFree;
        obol::Transform duringFreeOverlay;
        const bool gotDuringFree =
            scene.getObjectTransform(target, duringFree);
        const bool gotDuringFreeOverlay =
            scene.getGroupTransform(attachment.overlay.group,
                                    duringFreeOverlay);
        const bool canceledFree =
            obol::TransformDragger::cancelTransformEdit(scene,
                                                        freeEdit,
                                                        &attachment);

        obol::Transform afterFreeCancel;
        obol::Transform afterFreeCancelOverlay;
        const bool gotAfterFreeCancel =
            scene.getObjectTransform(target, afterFreeCancel);
        const bool gotAfterFreeCancelOverlay =
            scene.getGroupTransform(attachment.overlay.group,
                                    afterFreeCancelOverlay);

        obol::TransformEditState trackballEdit =
            obol::TransformDragger::beginTransformEdit(scene, target);
        obol::TrackballRotationRequest trackballRequest;
        trackballRequest.from = {1.0f, 0.0f, 0.0f};
        trackballRequest.to = {0.0f, 1.0f, 0.0f};
        trackballRequest.snap = true;
        trackballRequest.snapStepRadians = 0.25f;
        obol::AxisRotationResult trackballResult;
        const bool trackballMoved =
            obol::TransformDragger::updateEditTrackballRotation(
                scene,
                trackballEdit,
                trackballRequest,
                &attachment,
                &trackballResult);
        const bool canceledTrackball =
            obol::TransformDragger::cancelTransformEdit(scene,
                                                        trackballEdit,
                                                        &attachment);

        obol::Transform finalTransform;
        obol::Transform finalOverlayTransform;
        const bool gotFinal = scene.getObjectTransform(target, finalTransform);
        const bool gotFinalOverlay =
            scene.getGroupTransform(attachment.overlay.group,
                                    finalOverlayTransform);

        const bool pass =
            attachment.attached &&
            !invalidEdit.active &&
            rejectedInvalid &&
            rejectedCommit &&
            planeMoved &&
            planeResult.valid &&
            gotDuringPlane &&
            gotDuringPlaneOverlay &&
            std::fabs(duringPlane.translation.x - 2.0f) < 1.0e-5f &&
            std::fabs(duringPlane.translation.y - 4.0f) < 1.0e-5f &&
            std::fabs(duringPlane.translation.z - 2.0f) < 1.0e-5f &&
            std::fabs(duringPlaneOverlay.translation.x - 2.0f) < 1.0e-5f &&
            canceledPlane &&
            !planeEdit.active &&
            gotAfterCancel &&
            gotAfterCancelOverlay &&
            std::fabs(afterCancel.translation.x - 0.0f) < 1.0e-5f &&
            std::fabs(afterCancel.translation.y - 1.0f) < 1.0e-5f &&
            std::fabs(afterCancel.translation.z - 2.0f) < 1.0e-5f &&
            std::fabs(afterCancelOverlay.translation.x - 0.0f) < 1.0e-5f &&
            freeMoved &&
            freeResult.valid &&
            gotDuringFree &&
            gotDuringFreeOverlay &&
            std::fabs(duringFree.translation.x - 2.0f) < 1.0e-5f &&
            std::fabs(duringFree.translation.y + 1.0f) < 1.0e-5f &&
            std::fabs(duringFree.translation.z - 3.0f) < 1.0e-5f &&
            std::fabs(duringFreeOverlay.translation.x - 2.0f) < 1.0e-5f &&
            canceledFree &&
            !freeEdit.active &&
            gotAfterFreeCancel &&
            gotAfterFreeCancelOverlay &&
            std::fabs(afterFreeCancel.translation.x - 0.0f) < 1.0e-5f &&
            std::fabs(afterFreeCancel.translation.y - 1.0f) < 1.0e-5f &&
            std::fabs(afterFreeCancel.translation.z - 2.0f) < 1.0e-5f &&
            std::fabs(afterFreeCancelOverlay.translation.x - 0.0f) < 1.0e-5f &&
            trackballMoved &&
            trackballResult.valid &&
            canceledTrackball &&
            !trackballEdit.active &&
            gotFinal &&
            gotFinalOverlay &&
            std::fabs(finalTransform.translation.x - 0.0f) < 1.0e-5f &&
            std::fabs(finalTransform.translation.y - 1.0f) < 1.0e-5f &&
            std::fabs(finalTransform.rotationRadians - 0.0f) < 1.0e-5f &&
            std::fabs(finalOverlayTransform.rotationRadians - 0.0f) < 1.0e-5f;
        runner.endTest(pass, pass ? "" : "Transform edit controller did not restore canceled edits");
    }

    runner.startTest("v2 Scene removes only empty groups");
    {
        obol::Scene scene;
        const obol::SceneGroupId base = scene.addGroup();
        const obol::SceneGroupId child = scene.addGroup(obol::Transform{}, base);
        const obol::SceneObjectId cube =
            scene.addPrimitive(obol::Primitive::Cube,
                               obol::Material{},
                               obol::Transform{},
                               obol::PrimitiveOptions{},
                               child);

        const bool rejectedWithChildObject = !scene.removeGroup(child);
        const bool rejectedWithChildGroup = !scene.removeGroup(base);
        const bool removedObject = scene.removeObject(cube);
        const bool removedChild = scene.removeGroup(child);
        const bool rejectedChildTransform =
            !scene.setGroupTransform(child, obol::Transform{});
        obol::Transform removedGroupTransform;
        const bool rejectedChildQuery =
            !scene.getGroupTransform(child, removedGroupTransform);
        const bool removedBase = scene.removeGroup(base);
        const bool rejectedRepeat = !scene.removeGroup(base);
        const obol::SceneObjectId fallback =
            scene.addPrimitive(obol::Primitive::Sphere,
                               obol::Material{},
                               obol::Transform{},
                               obol::PrimitiveOptions{},
                               child);

        obol::SceneQuery primitiveQuery;
        primitiveQuery.type = obol::SceneObjectType::Primitive;
        const std::vector<obol::SceneObjectInfo> primitives =
            scene.findObjects(primitiveQuery);
        const obol::ScenePacket packet = scene.capturePacket();

        SoSeparator * root = legacySceneGraph(scene);
        SoSeparator * baseSep = findNamedSeparator(root, "ObolSceneGroup_1");
        SoSeparator * childSep = findNamedSeparator(root, "ObolSceneGroup_2");
        SoSeparator * fallbackSep = findNamedSeparator(root, "ObolSceneObject_2");

        const bool pass =
            rejectedWithChildObject &&
            rejectedWithChildGroup &&
            removedObject &&
            removedChild &&
            rejectedChildTransform &&
            rejectedChildQuery &&
            removedBase &&
            rejectedRepeat &&
            scene.groupCount() == 0 &&
            packet.groups.empty() &&
            primitives.size() == 1 &&
            primitives[0].id == fallback &&
            primitives[0].parent == obol::RootSceneGroupId &&
            baseSep == nullptr &&
            childSep == nullptr &&
            fallbackSep != nullptr;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Scene group removal did not preserve stable lifecycle rules");
    }

    runner.startTest("v2 Scene toggles object and group visibility");
    {
        obol::Scene scene;
        const obol::SceneGroupId group = scene.addGroup();
        const obol::SceneObjectId cube =
            scene.addPrimitive(obol::Primitive::Cube);
        const obol::SceneObjectId sphere =
            scene.addPrimitive(obol::Primitive::Sphere,
                               obol::Material{},
                               obol::Transform{},
                               obol::PrimitiveOptions{},
                               group);

        const bool hidObject = scene.setObjectVisible(cube, false);
        const obol::ScenePacket objectHidden = scene.capturePacket();
        const bool showedObject = scene.setObjectVisible(cube, true);
        const bool hidGroup = scene.setGroupVisible(group, false);
        const obol::ScenePacket groupHidden = scene.capturePacket();
        const bool showedGroup = scene.setGroupVisible(group, true);
        const obol::ScenePacket allVisible = scene.capturePacket();

        const bool rejectedObject =
            !scene.setObjectVisible(obol::InvalidSceneObjectId, true);
        const bool rejectedGroup =
            !scene.setGroupVisible(obol::InvalidSceneGroupId, true);

        obol::SceneQuery primitiveQuery;
        primitiveQuery.type = obol::SceneObjectType::Primitive;
        const std::vector<obol::SceneObjectInfo> primitives =
            scene.findObjects(primitiveQuery);

        SoSeparator * root = legacySceneGraph(scene);
        SoSeparator * cubeSep = findNamedSeparator(root, "ObolSceneObject_1");
        SoSeparator * sphereSep = findNamedSeparator(root, "ObolSceneObject_2");
        SoSeparator * groupSep = findNamedSeparator(root, "ObolSceneGroup_1");

        const bool pass =
            hidObject &&
            showedObject &&
            hidGroup &&
            showedGroup &&
            rejectedObject &&
            rejectedGroup &&
            scene.isObjectVisible(cube) &&
            scene.isObjectVisible(sphere) &&
            scene.isGroupVisible(group) &&
            scene.objectCount() == 2 &&
            scene.groupCount() == 1 &&
            primitives.size() == 2 &&
            objectHidden.objects.size() == 1 &&
            objectHidden.objects[0].id == sphere &&
            groupHidden.groups.empty() &&
            groupHidden.objects.size() == 1 &&
            groupHidden.objects[0].id == cube &&
            allVisible.groups.size() == 1 &&
            allVisible.objects.size() == 2 &&
            cubeSep != nullptr &&
            sphereSep != nullptr &&
            groupSep != nullptr;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Scene visibility did not preserve queryable stable ids");
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
        obol::Transform removedTransform;
        const bool rejectedQuery = !scene.getObjectTransform(cube, removedTransform);

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
                          rejectedQuery &&
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
        const obol::RenderCapabilities queriedCapabilities =
            renderer.capabilities();
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
                          result.capabilities.known == false &&
                          queriedCapabilities.backendKind == obol::RenderBackendKind::CPU &&
                          queriedCapabilities.backendName == "solid-color-test" &&
                          obol::supportsRenderFeatureProfile(
                              result.capabilities,
                              obol::RenderFeatureProfile::CorePortable) &&
                          obol::supportsRenderFeatureProfile(
                              queriedCapabilities,
                              obol::RenderFeatureProfile::CorePortable) &&
                          !obol::supportsRenderFeatureProfile(
                              result.capabilities,
                              obol::RenderFeatureProfile::RasterExtended) &&
                          !obol::supportsRenderFeatureProfile(
                              result.capabilities,
                              obol::RenderFeatureProfile::BackendNative);
        runner.endTest(pass, pass ? "" : "Alternative backend render did not fill expected pixels");
    }

    runner.startTest("v2 RenderCapabilities reports feature profiles");
    {
        obol::RenderCapabilities portable;
        portable.corePortable = true;

        obol::RenderCapabilities extended = portable;
        extended.rasterExtended = true;

        obol::RenderCapabilities native = portable;
        native.backendNative = true;

        const bool pass =
            obol::supportsRenderFeatureProfile(
                portable,
                obol::RenderFeatureProfile::CorePortable) &&
            !obol::supportsRenderFeatureProfile(
                portable,
                obol::RenderFeatureProfile::RasterExtended) &&
            !obol::supportsRenderFeatureProfile(
                portable,
                obol::RenderFeatureProfile::BackendNative) &&
            obol::supportsRenderFeatureProfile(
                extended,
                obol::RenderFeatureProfile::RasterExtended) &&
            obol::supportsRenderFeatureProfile(
                native,
                obol::RenderFeatureProfile::BackendNative);
        runner.endTest(pass, pass ? "" : "Render feature profile helper was incorrect");
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
        options.advancedTransparency = true;

        obol::RenderTarget target;
        target.width = 2;
        target.height = 2;
        obol::OffscreenRenderer renderer(backend, target);
        obol::FrameResult result = renderer.render(scene, options);
        bool foundBackendNative = false;
        bool foundRasterExtended = false;
        bool foundFramebuffer = false;
        for (const obol::RenderDiagnostic & diagnostic : result.diagnostics) {
            foundBackendNative = foundBackendNative ||
                diagnostic.message.find("BackendNative") != std::string::npos;
            foundRasterExtended = foundRasterExtended ||
                diagnostic.message.find("RasterExtended") != std::string::npos;
            foundFramebuffer = foundFramebuffer ||
                diagnostic.message.find("framebuffer") != std::string::npos;
        }
        const bool pass = result.success &&
                          foundBackendNative &&
                          foundRasterExtended &&
                          foundFramebuffer;
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
            obol::supportsRenderFeatureProfile(
                result.capabilities,
                obol::RenderFeatureProfile::CorePortable) &&
            !obol::supportsRenderFeatureProfile(
                result.capabilities,
                obol::RenderFeatureProfile::BackendNative) &&
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

    runner.startTest("v2 OffscreenRenderer ignores hidden backend-native callbacks");
    {
        PacketOnlyBackend backend;
        obol::Scene scene;
        scene.addPrimitive(obol::Primitive::Cube);

        obol::OpenGLCallback callback;
        callback.draw = testOpenGLDrawCallback;
        const obol::SceneObjectId callbackObject =
            scene.addOpenGLCallback(callback);
        const bool hidden = scene.setObjectVisible(callbackObject, false);

        obol::OffscreenRenderer renderer(backend, 2, 2);
        const obol::FrameResult result = renderer.render(scene);

        bool foundOpenGLError = false;
        for (const obol::RenderDiagnostic & diagnostic : result.diagnostics) {
            foundOpenGLError = foundOpenGLError ||
                (diagnostic.severity == obol::DiagnosticSeverity::Error &&
                 diagnostic.message.find("OpenGL") != std::string::npos);
        }

        const bool pass =
            callbackObject != obol::InvalidSceneObjectId &&
            hidden &&
            result.success &&
            !foundOpenGLError &&
            backend.lastObjectCount == 1 &&
            renderer.pixels() != nullptr;
        runner.endTest(pass, pass ? "" : "Hidden OpenGL callback poisoned packet-only rendering");
    }

    runner.startTest("v2 OffscreenRenderer ignores hidden SceneIO legacy objects");
    {
        const std::string iv =
            "#Inventor V2.0 ascii\n"
            "Separator {\n"
            "  Material { diffuseColor [ 1 0 0, 0 1 0 ] }\n"
            "  MaterialBinding { value PER_PART }\n"
            "  Cube { width 2 height 2 depth 2 }\n"
            "}\n";

        obol::Scene scene;
        const bool read = obol::SceneIO::readInventorString(iv, scene, &manager);

        obol::SceneQuery legacyQuery;
        legacyQuery.type = obol::SceneObjectType::LegacySceneGraph;
        const obol::SceneObjectId legacyObject = scene.findFirstObject(legacyQuery);
        const bool hidden = scene.setObjectVisible(legacyObject, false);

        PacketOnlyBackend backend;
        obol::OffscreenRenderer renderer(backend, 2, 2);
        const obol::FrameResult result = renderer.render(scene);

        bool foundLegacyWarning = false;
        for (const obol::RenderDiagnostic & diagnostic : result.diagnostics) {
            foundLegacyWarning = foundLegacyWarning ||
                diagnostic.message.find("legacy scene graph") != std::string::npos;
        }

        const bool pass =
            read &&
            legacyObject != obol::InvalidSceneObjectId &&
            hidden &&
            result.success &&
            !foundLegacyWarning &&
            backend.lastObjectCount == 0 &&
            renderer.pixels() != nullptr;
        runner.endTest(pass, pass ? "" : "Hidden SceneIO legacy object poisoned packet-only rendering");
    }

    runner.startTest("v2 Renderer renders explicit frame requests");
    {
        PacketOnlyBackend backend;
        obol::Scene scene;
        scene.addPrimitive(obol::Primitive::Sphere);

        obol::FrameRequest request;
        request.scene = &scene;
        request.target.width = 2;
        request.target.height = 2;
        request.target.pixelFormat = obol::PixelFormat::RGBA;
        request.background = {0.1f, 0.2f, 0.3f, 1.0f};

        obol::Renderer renderer(backend);
        const obol::RenderCapabilities preRenderCapabilities =
            renderer.capabilities();
        const obol::FrameResult result = renderer.render(request);
        const unsigned char * pixels = renderer.pixels();
        const obol::RenderTarget actualTarget = renderer.renderTarget();
        const obol::RenderCapabilities activeCapabilities =
            renderer.capabilities();
        const bool initialPixels =
            pixels &&
            pixels[0] == 25 &&
            pixels[1] == 50 &&
            pixels[2] == 75 &&
            pixels[3] == 255;
        const bool validState =
            renderer.width() == 2 &&
            renderer.height() == 2 &&
            renderer.pixelFormat() == obol::PixelFormat::RGBA &&
            actualTarget.width == 2 &&
            actualTarget.height == 2 &&
            actualTarget.pixelFormat == obol::PixelFormat::RGBA;
        obol::FrameRequest invalidRequest;
        const obol::FrameResult invalidResult = renderer.render(invalidRequest);
        const obol::RenderCapabilities resetCapabilities =
            renderer.capabilities();

        const bool pass =
            result.success &&
            result.target.width == 2 &&
            result.target.height == 2 &&
            result.target.pixelFormat == obol::PixelFormat::RGBA &&
            backend.lastObjectCount == 1 &&
            validState &&
            initialPixels &&
            preRenderCapabilities.backendName == "packet-only-test" &&
            obol::supportsRenderFeatureProfile(
                preRenderCapabilities,
                obol::RenderFeatureProfile::CorePortable) &&
            activeCapabilities.backendName == "packet-only-test" &&
            obol::supportsRenderFeatureProfile(
                activeCapabilities,
                obol::RenderFeatureProfile::CorePortable) &&
            !invalidResult.success &&
            resetCapabilities.backendName == "packet-only-test" &&
            renderer.width() == 0 &&
            renderer.height() == 0 &&
            renderer.renderTarget().width == 0 &&
            renderer.renderTarget().height == 0 &&
            renderer.pixels() == nullptr;
        runner.endTest(pass, pass ? "" : "Expected Renderer FrameRequest packet render and target state");
    }

    runner.startTest("v2 Renderer rejects empty frame requests");
    {
        PacketOnlyBackend backend;
        obol::Renderer renderer(backend);
        obol::FrameRequest request;
        request.target.width = 4;
        request.target.height = 3;

        const obol::FrameResult result = renderer.render(request);

        bool foundError = false;
        for (const obol::RenderDiagnostic & diagnostic : result.diagnostics) {
            foundError = foundError ||
                (diagnostic.severity == obol::DiagnosticSeverity::Error &&
                 diagnostic.message.find("FrameRequest") != std::string::npos);
        }

        const bool pass = !result.success &&
                          result.target.width == 4 &&
                          result.target.height == 3 &&
                          foundError &&
                          renderer.pixels() == nullptr &&
                          !renderer.writeRGB("/tmp/obol_empty_frame_request.rgb");
        runner.endTest(pass, pass ? "" : "Expected FrameRequest validation diagnostic");
    }

    runner.startTest("v2 Renderer reports incomplete packet frame requests");
    {
        const std::string iv =
            "#Inventor V2.0 ascii\n"
            "Separator {\n"
            "  Material { diffuseColor [ 1 0 0, 0 1 0 ] }\n"
            "  MaterialBinding { value PER_PART }\n"
            "  Cube { width 2 height 2 depth 2 }\n"
            "}\n";

        obol::Scene scene;
        const bool read = obol::SceneIO::readInventorString(iv, scene, &manager);

        obol::FrameRequest request;
        request.scene = &scene;
        request.target.width = 2;
        request.target.height = 2;

        PacketOnlyBackend backend;
        obol::Renderer renderer(backend);
        const obol::FrameResult result = renderer.render(request);

        obol::SceneQuery legacyQuery;
        legacyQuery.type = obol::SceneObjectType::LegacySceneGraph;
        const obol::SceneObjectId legacyObject = scene.findFirstObject(legacyQuery);

        bool foundError = false;
        bool foundLegacyWarning = false;
        for (const obol::RenderDiagnostic & diagnostic : result.diagnostics) {
            foundError = foundError ||
                (diagnostic.severity == obol::DiagnosticSeverity::Error &&
                 diagnostic.message.find("fully lowered") != std::string::npos);
            foundLegacyWarning = foundLegacyWarning ||
                (diagnostic.severity == obol::DiagnosticSeverity::Warning &&
                 diagnostic.objectId == legacyObject &&
                 diagnostic.message.find("legacy scene graph") != std::string::npos);
        }

        const bool pass =
            read &&
            legacyObject != obol::InvalidSceneObjectId &&
            !result.success &&
            result.target.width == 2 &&
            result.target.height == 2 &&
            foundError &&
            foundLegacyWarning &&
            renderer.pixels() == nullptr &&
            backend.lastObjectCount == 0;
        runner.endTest(pass, pass ? "" : "Expected FrameRequest to fail before packet backend drops legacy content");
    }

    runner.startTest("v2 OffscreenRenderer reports unsupported packet backends");
    {
        CapabilityOnlyBackend backend;
        obol::Scene scene;
        scene.addPrimitive(obol::Primitive::Cube);
        obol::OffscreenRenderer renderer(backend, 2, 2);
        const obol::FrameResult result = renderer.render(scene);

        bool foundError = false;
        for (const obol::RenderDiagnostic & diagnostic : result.diagnostics) {
            foundError = foundError ||
                (diagnostic.severity == obol::DiagnosticSeverity::Error &&
                 diagnostic.message.find("packet rendering") != std::string::npos);
        }

        const bool pass = !result.success &&
                          result.capabilities.known &&
                          result.capabilities.backendKind == obol::RenderBackendKind::Custom &&
                          foundError &&
                          renderer.pixels() == nullptr;
        runner.endTest(pass, pass ? "" : "Expected explicit diagnostic for backend without packet rendering");
    }

    runner.startTest("v2 OffscreenRenderer reports SceneIO legacy packet dependencies");
    {
        const std::string iv =
            "#Inventor V2.0 ascii\n"
            "Separator {\n"
            "  Material { diffuseColor [ 1 0 0, 0 1 0 ] }\n"
            "  MaterialBinding { value PER_PART }\n"
            "  Cube { width 2 height 2 depth 2 }\n"
            "}\n";

        obol::Scene scene;
        const bool read = obol::SceneIO::readInventorString(iv, scene, &manager);
        CapabilityOnlyBackend backend;
        obol::OffscreenRenderer renderer(backend, 2, 2);
        const obol::FrameResult result = renderer.render(scene);

        obol::SceneQuery legacyQuery;
        legacyQuery.type = obol::SceneObjectType::LegacySceneGraph;
        const obol::SceneObjectId legacyObject = scene.findFirstObject(legacyQuery);

        bool foundError = false;
        bool foundLegacyWarning = false;
        for (const obol::RenderDiagnostic & diagnostic : result.diagnostics) {
            foundError = foundError ||
                (diagnostic.severity == obol::DiagnosticSeverity::Error &&
                 diagnostic.message.find("fully lowered") != std::string::npos);
            foundLegacyWarning = foundLegacyWarning ||
                (diagnostic.severity == obol::DiagnosticSeverity::Warning &&
                 diagnostic.objectId == legacyObject &&
                 diagnostic.message.find("legacy scene graph") != std::string::npos);
        }

        const bool pass =
            read &&
            scene.objectCount() == 1 &&
            legacyObject != obol::InvalidSceneObjectId &&
            !result.success &&
            foundError &&
            foundLegacyWarning &&
            renderer.pixels() == nullptr;
        runner.endTest(pass, pass ? "" : "Expected SceneIO legacy dependency diagnostic for packet-only rendering");
    }

    runner.startTest("v2 OffscreenRenderer surfaces packet extraction diagnostics");
    {
        CapabilityOnlyBackend backend;
        obol::Scene scene;
        const obol::SceneObjectId pointCloud =
            scene.addPointCloud(obol::PointCloud{});

        obol::OffscreenRenderer renderer(backend, 2, 2);
        const obol::FrameResult result = renderer.render(scene);

        bool foundError = false;
        bool foundPointWarning = false;
        for (const obol::RenderDiagnostic & diagnostic : result.diagnostics) {
            foundError = foundError ||
                (diagnostic.severity == obol::DiagnosticSeverity::Error &&
                 diagnostic.message.find("fully lowered") != std::string::npos);
            foundPointWarning = foundPointWarning ||
                (diagnostic.severity == obol::DiagnosticSeverity::Warning &&
                 diagnostic.objectId == pointCloud &&
                 diagnostic.message.find("point cloud has no points") !=
                     std::string::npos);
        }

        const bool pass = !result.success &&
                          foundError &&
                          foundPointWarning &&
                          renderer.pixels() == nullptr;
        runner.endTest(pass, pass ? "" : "Expected aggregate packet extraction diagnostics");
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
        const obol::SceneObjectId callbackObject =
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
                 diagnostic.objectId == callbackObject &&
                 diagnostic.message.find("OpenGL") != std::string::npos);
        }

        const bool pass = callbackObject != obol::InvalidSceneObjectId &&
                          !result.success &&
                          foundError;
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

    runner.startTest("v2 SceneIO round-trips native perspective cameras");
    {
        obol::PerspectiveCamera camera;
        camera.position = {1.0f, 2.0f, 8.0f};
        camera.target = {1.0f, 2.0f, 3.0f};
        camera.up = {0.0f, 1.0f, 0.0f};
        camera.verticalFieldOfViewRadians = 0.55f;
        camera.nearDistance = 0.25f;
        camera.farDistance = 250.0f;

        obol::Scene scene;
        scene.setCamera(camera);

        std::string iv;
        const bool wrote = obol::SceneIO::writeInventorString(scene, iv);

        obol::Scene loaded;
        const bool read = obol::SceneIO::readInventorString(iv, loaded, &manager);
        const obol::ScenePacket packet = loaded.capturePacket();

        SoSeparator * root = legacySceneGraph(loaded);
        SoPerspectiveCamera * cameraNode = root && root->getNumChildren() >= 1
            ? dynamic_cast<SoPerspectiveCamera *>(root->getChild(0))
            : nullptr;
        const bool pass =
            wrote &&
            read &&
            !iv.empty() &&
            loaded.hasCamera() &&
            packet.camera.kind == obol::SceneCameraKind::Perspective &&
            packet.camera.perspective.position.x == 1.0f &&
            packet.camera.perspective.position.y == 2.0f &&
            packet.camera.perspective.position.z == 8.0f &&
            packet.camera.perspective.target.x == 1.0f &&
            packet.camera.perspective.target.y == 2.0f &&
            packet.camera.perspective.target.z == 3.0f &&
            packet.camera.perspective.verticalFieldOfViewRadians == 0.55f &&
            packet.camera.perspective.nearDistance == 0.25f &&
            packet.camera.perspective.farDistance == 250.0f &&
            cameraNode &&
            cameraNode->heightAngle.getValue() == 0.55f &&
            cameraNode->nearDistance.getValue() == 0.25f &&
            cameraNode->farDistance.getValue() == 250.0f;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Native perspective camera did not survive SceneIO round-trip");
    }

    runner.startTest("v2 SceneIO round-trips native orthographic cameras");
    {
        obol::OrthographicCamera camera;
        camera.position = {-1.0f, 2.0f, 10.0f};
        camera.target = {-1.0f, 2.0f, 5.0f};
        camera.up = {0.0f, 1.0f, 0.0f};
        camera.height = 7.5f;
        camera.nearDistance = 0.5f;
        camera.farDistance = 125.0f;

        obol::Scene scene;
        scene.setCamera(camera);

        std::string iv;
        const bool wrote = obol::SceneIO::writeInventorString(scene, iv);

        obol::Scene loaded;
        const bool read = obol::SceneIO::readInventorString(iv, loaded, &manager);
        const obol::ScenePacket packet = loaded.capturePacket();

        SoSeparator * root = legacySceneGraph(loaded);
        SoOrthographicCamera * cameraNode = root && root->getNumChildren() >= 1
            ? dynamic_cast<SoOrthographicCamera *>(root->getChild(0))
            : nullptr;
        const bool pass =
            wrote &&
            read &&
            !iv.empty() &&
            loaded.hasCamera() &&
            packet.camera.kind == obol::SceneCameraKind::Orthographic &&
            packet.camera.orthographic.position.x == -1.0f &&
            packet.camera.orthographic.position.y == 2.0f &&
            packet.camera.orthographic.position.z == 10.0f &&
            packet.camera.orthographic.target.x == -1.0f &&
            packet.camera.orthographic.target.y == 2.0f &&
            packet.camera.orthographic.target.z == 5.0f &&
            packet.camera.orthographic.height == 7.5f &&
            packet.camera.orthographic.nearDistance == 0.5f &&
            packet.camera.orthographic.farDistance == 125.0f &&
            cameraNode &&
            cameraNode->height.getValue() == 7.5f &&
            cameraNode->nearDistance.getValue() == 0.5f &&
            cameraNode->farDistance.getValue() == 125.0f;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Native orthographic camera did not survive SceneIO round-trip");
    }

    runner.startTest("v2 SceneIO round-trips native lights");
    {
        obol::DirectionalLight directional;
        directional.direction = {0.0f, -1.0f, 0.0f};
        directional.color = {1.0f, 0.25f, 0.0f, 1.0f};
        directional.intensity = 0.5f;

        obol::PointLight point;
        point.location = {1.0f, 2.0f, 3.0f};
        point.color = {0.0f, 1.0f, 0.25f, 1.0f};
        point.intensity = 0.75f;

        obol::SpotLight spot;
        spot.location = {-1.0f, 0.0f, 2.0f};
        spot.direction = {0.0f, 0.0f, -1.0f};
        spot.color = {0.25f, 0.5f, 1.0f, 1.0f};
        spot.intensity = 0.9f;
        spot.cutOffAngleRadians = 0.35f;
        spot.dropOffRate = 0.6f;

        obol::Scene scene;
        scene.addDirectionalLight(directional);
        scene.addPointLight(point);
        scene.addSpotLight(spot);

        std::string iv;
        const bool wrote = obol::SceneIO::writeInventorString(scene, iv);

        obol::Scene loaded;
        const bool read = obol::SceneIO::readInventorString(iv, loaded, &manager);

        obol::SceneQuery directionalQuery;
        directionalQuery.type = obol::SceneObjectType::DirectionalLight;
        obol::SceneQuery pointQuery;
        pointQuery.type = obol::SceneObjectType::PointLight;
        obol::SceneQuery spotQuery;
        spotQuery.type = obol::SceneObjectType::SpotLight;

        SoSeparator * root = legacySceneGraph(loaded);
        SoDirectionalLight * directionalNode =
            findFirstNodeOfType<SoDirectionalLight>(root);
        SoPointLight * pointNode = findFirstNodeOfType<SoPointLight>(root);
        SoSpotLight * spotNode = findFirstNodeOfType<SoSpotLight>(root);
        const SbColor directionalColor = directionalNode
            ? directionalNode->color.getValue()
            : SbColor(0.0f, 0.0f, 0.0f);
        const SbColor pointColor = pointNode
            ? pointNode->color.getValue()
            : SbColor(0.0f, 0.0f, 0.0f);
        const SbColor spotColor = spotNode
            ? spotNode->color.getValue()
            : SbColor(0.0f, 0.0f, 0.0f);
        const SbVec3f pointLocation = pointNode
            ? pointNode->location.getValue()
            : SbVec3f(0.0f, 0.0f, 0.0f);
        const SbVec3f spotLocation = spotNode
            ? spotNode->location.getValue()
            : SbVec3f(0.0f, 0.0f, 0.0f);
        const SbVec3f spotDirection = spotNode
            ? spotNode->direction.getValue()
            : SbVec3f(0.0f, 0.0f, 0.0f);

        const bool pass =
            wrote &&
            read &&
            !iv.empty() &&
            loaded.objectCount() == 3 &&
            loaded.hasObjects(directionalQuery) &&
            loaded.hasObjects(pointQuery) &&
            loaded.hasObjects(spotQuery) &&
            directionalNode &&
            directionalNode->intensity.getValue() == 0.5f &&
            directionalColor[0] == 1.0f &&
            directionalColor[1] == 0.25f &&
            directionalColor[2] == 0.0f &&
            pointNode &&
            pointNode->intensity.getValue() == 0.75f &&
            pointLocation == SbVec3f(1.0f, 2.0f, 3.0f) &&
            pointColor[0] == 0.0f &&
            pointColor[1] == 1.0f &&
            pointColor[2] == 0.25f &&
            spotNode &&
            spotNode->intensity.getValue() == 0.9f &&
            spotLocation == SbVec3f(-1.0f, 0.0f, 2.0f) &&
            spotDirection == SbVec3f(0.0f, 0.0f, -1.0f) &&
            spotColor[0] == 0.25f &&
            spotColor[1] == 0.5f &&
            spotColor[2] == 1.0f &&
            spotNode->cutOffAngle.getValue() == 0.35f &&
            spotNode->dropOffRate.getValue() == 0.6f;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Native lights did not survive SceneIO round-trip");
    }

    runner.startTest("v2 SceneIO round-trips native primitives and materials");
    {
        obol::Material material;
        material.baseColor = {0.2f, 0.4f, 0.6f, 0.75f};
        material.specular = {0.1f, 0.2f, 0.3f, 1.0f};
        material.emissive = {0.05f, 0.06f, 0.07f, 1.0f};
        material.shininess = 0.45f;

        obol::PrimitiveOptions cubeOptions;
        cubeOptions.width = 3.0f;
        cubeOptions.height = 2.0f;
        cubeOptions.depth = 1.0f;

        obol::PrimitiveOptions sphereOptions;
        sphereOptions.radius = 1.25f;

        obol::PrimitiveOptions coneOptions;
        coneOptions.radius = 1.5f;
        coneOptions.height = 4.0f;

        obol::PrimitiveOptions cylinderOptions;
        cylinderOptions.radius = 0.75f;
        cylinderOptions.height = 2.5f;

        obol::Scene scene;
        scene.addPrimitive(obol::Primitive::Cube,
                           material,
                           obol::Transform{},
                           cubeOptions);
        scene.addPrimitive(obol::Primitive::Sphere,
                           obol::Material{},
                           obol::Transform{},
                           sphereOptions);
        scene.addPrimitive(obol::Primitive::Cone,
                           obol::Material{},
                           obol::Transform{},
                           coneOptions);
        scene.addPrimitive(obol::Primitive::Cylinder,
                           obol::Material{},
                           obol::Transform{},
                           cylinderOptions);

        std::string iv;
        const bool wrote = obol::SceneIO::writeInventorString(scene, iv);

        obol::Scene loaded;
        const bool read = obol::SceneIO::readInventorString(iv, loaded, &manager);

        obol::SceneQuery primitiveQuery;
        primitiveQuery.type = obol::SceneObjectType::Primitive;

        SoSeparator * root = legacySceneGraph(loaded);
        SoSeparator * cubeObject = findNamedSeparator(root, "ObolSceneObject_1");
        SoSeparator * sphereObject = findNamedSeparator(root, "ObolSceneObject_2");
        SoSeparator * coneObject = findNamedSeparator(root, "ObolSceneObject_3");
        SoSeparator * cylinderObject = findNamedSeparator(root, "ObolSceneObject_4");
        SoMaterial * cubeMaterial = findFirstNodeOfType<SoMaterial>(cubeObject);
        SoCube * cube = findFirstNodeOfType<SoCube>(cubeObject);
        SoSphere * sphere = findFirstNodeOfType<SoSphere>(sphereObject);
        SoCone * cone = findFirstNodeOfType<SoCone>(coneObject);
        SoCylinder * cylinder = findFirstNodeOfType<SoCylinder>(cylinderObject);
        const SbColor diffuse = cubeMaterial && cubeMaterial->diffuseColor.getNum() > 0
            ? cubeMaterial->diffuseColor[0]
            : SbColor(0.0f, 0.0f, 0.0f);
        const SbColor specular = cubeMaterial && cubeMaterial->specularColor.getNum() > 0
            ? cubeMaterial->specularColor[0]
            : SbColor(0.0f, 0.0f, 0.0f);
        const SbColor emissive = cubeMaterial && cubeMaterial->emissiveColor.getNum() > 0
            ? cubeMaterial->emissiveColor[0]
            : SbColor(0.0f, 0.0f, 0.0f);

        const bool pass =
            wrote &&
            read &&
            !iv.empty() &&
            loaded.objectCount() == 4 &&
            loaded.hasObjects(primitiveQuery) &&
            cube &&
            cube->width.getValue() == 3.0f &&
            cube->height.getValue() == 2.0f &&
            cube->depth.getValue() == 1.0f &&
            cubeMaterial &&
            diffuse[0] == 0.2f &&
            diffuse[1] == 0.4f &&
            diffuse[2] == 0.6f &&
            specular[0] == 0.1f &&
            specular[1] == 0.2f &&
            specular[2] == 0.3f &&
            emissive[0] == 0.05f &&
            emissive[1] == 0.06f &&
            emissive[2] == 0.07f &&
            cubeMaterial->shininess.getNum() == 1 &&
            cubeMaterial->shininess[0] == 0.45f &&
            cubeMaterial->transparency.getNum() == 1 &&
            cubeMaterial->transparency[0] == 0.25f &&
            sphere &&
            sphere->radius.getValue() == 1.25f &&
            cone &&
            cone->bottomRadius.getValue() == 1.5f &&
            cone->height.getValue() == 4.0f &&
            cylinder &&
            cylinder->radius.getValue() == 0.75f &&
            cylinder->height.getValue() == 2.5f;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Native primitives/materials did not survive SceneIO round-trip");
    }

    runner.startTest("v2 SceneIO round-trips native group hierarchy");
    {
        obol::Transform baseTransform;
        baseTransform.translation = {2.0f, 0.0f, 0.0f};
        obol::Transform childTransform;
        childTransform.translation = {0.0f, 3.0f, 0.0f};
        obol::Transform objectTransform;
        objectTransform.translation = {0.0f, 0.0f, 4.0f};

        obol::Scene scene;
        const obol::SceneGroupId base = scene.addGroup(baseTransform);
        const obol::SceneGroupId child = scene.addGroup(childTransform, base);
        scene.addPrimitive(obol::Primitive::Cube,
                           obol::Material{},
                           objectTransform,
                           obol::PrimitiveOptions{},
                           child);

        std::string iv;
        const bool wrote = obol::SceneIO::writeInventorString(scene, iv);

        obol::Scene loaded;
        const bool read = obol::SceneIO::readInventorString(iv, loaded, &manager);

        obol::SceneQuery primitiveQuery;
        primitiveQuery.type = obol::SceneObjectType::Primitive;
        const std::vector<obol::SceneObjectInfo> primitives =
            loaded.findObjects(primitiveQuery);
        const obol::ScenePacket packet = loaded.capturePacket();

        obol::Transform loadedBase;
        obol::Transform loadedChild;
        obol::Transform loadedObject;
        const bool hasBase =
            loaded.getGroupTransform(static_cast<obol::SceneGroupId>(1),
                                     loadedBase);
        const bool hasChild =
            loaded.getGroupTransform(static_cast<obol::SceneGroupId>(2),
                                     loadedChild);
        const bool hasObject =
            !primitives.empty() &&
            loaded.getObjectTransform(primitives[0].id, loadedObject);

        SoSeparator * root = legacySceneGraph(loaded);
        SoSeparator * baseSep = findNamedSeparator(root, "ObolSceneGroup_1");
        SoSeparator * childSep = findNamedSeparator(root, "ObolSceneGroup_2");
        SoSeparator * objectSep = findNamedSeparator(root, "ObolSceneObject_1");

        const bool pass =
            wrote &&
            read &&
            !iv.empty() &&
            packet.groups.size() == 2 &&
            packet.groups[0].parent == obol::RootSceneGroupId &&
            packet.groups[1].parent == 1 &&
            primitives.size() == 1 &&
            primitives[0].parent == 2 &&
            hasBase &&
            loadedBase.translation.x == 2.0f &&
            loadedBase.translation.y == 0.0f &&
            loadedBase.translation.z == 0.0f &&
            hasChild &&
            loadedChild.translation.x == 0.0f &&
            loadedChild.translation.y == 3.0f &&
            loadedChild.translation.z == 0.0f &&
            hasObject &&
            loadedObject.translation.x == 0.0f &&
            loadedObject.translation.y == 0.0f &&
            loadedObject.translation.z == 4.0f &&
            baseSep &&
            childSep &&
            objectSep;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Native group hierarchy did not survive SceneIO round-trip");
    }

    runner.startTest("v2 SceneIO round-trips native polygon mesh attributes");
    {
        obol::Mesh mesh;
        mesh.topology = obol::MeshTopology::Polygons;
        mesh.positions = {
            {0.0f, 0.0f, 0.0f},
            {1.0f, 0.0f, 0.0f},
            {1.0f, 1.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            {1.0f, 0.0f, 1.0f}
        };
        mesh.indices = {0, 1, 2, 3, 0, 4, 5, 1};
        mesh.faceVertexCounts = {4, 4};
        mesh.faceNormals = {{0.0f, 0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}};
        mesh.texCoords = {
            {0.0f, 0.0f},
            {1.0f, 0.0f},
            {1.0f, 1.0f},
            {0.0f, 1.0f}
        };
        mesh.texCoordIndices = {0, 1, 2, 3, 0, 1, 2, 3};
        mesh.faceColors = {
            {1.0f, 0.0f, 0.0f, 1.0f},
            {0.0f, 1.0f, 0.0f, 1.0f}
        };

        obol::Scene scene;
        scene.addMesh(mesh);

        std::string iv;
        const bool wrote = obol::SceneIO::writeInventorString(scene, iv);

        obol::Scene loaded;
        const bool read = obol::SceneIO::readInventorString(iv, loaded, &manager);

        obol::SceneQuery meshQuery;
        meshQuery.type = obol::SceneObjectType::Mesh;

        SoSeparator * root = legacySceneGraph(loaded);
        SoSeparator * object = findNamedSeparator(root, "ObolSceneObject_1");
        SoMaterial * material = findFirstNodeOfType<SoMaterial>(object);
        SoMaterialBinding * materialBinding =
            findFirstNodeOfType<SoMaterialBinding>(object);
        SoNormalBinding * normalBinding =
            findFirstNodeOfType<SoNormalBinding>(object);
        SoTextureCoordinate2 * texCoords =
            findFirstNodeOfType<SoTextureCoordinate2>(object);
        SoIndexedFaceSet * faces = findFirstNodeOfType<SoIndexedFaceSet>(object);

        const bool pass =
            wrote &&
            read &&
            !iv.empty() &&
            loaded.objectCount() == 1 &&
            loaded.hasObjects(meshQuery) &&
            object &&
            material &&
            material->diffuseColor.getNum() == 2 &&
            materialBinding &&
            materialBinding->value.getValue() == SoMaterialBinding::PER_FACE &&
            normalBinding &&
            normalBinding->value.getValue() == SoNormalBinding::PER_FACE &&
            texCoords &&
            texCoords->point.getNum() == 4 &&
            faces &&
            faces->coordIndex.getNum() == 10 &&
            faces->textureCoordIndex.getNum() == 10;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Native mesh attributes did not survive SceneIO round-trip");
    }

    runner.startTest("v2 SceneIO round-trips native strip and quad-grid meshes");
    {
        obol::Mesh stripMesh;
        stripMesh.topology = obol::MeshTopology::TriangleStrips;
        stripMesh.positions = {
            {-1.0f, -1.0f, 0.0f},
            {-1.0f,  1.0f, 0.0f},
            { 0.0f, -1.0f, 0.0f},
            { 0.0f,  1.0f, 0.0f},
            { 1.0f, -1.0f, 0.0f},
            { 1.0f,  1.0f, 0.0f},
            { 2.0f, -1.0f, 0.0f}
        };
        stripMesh.stripVertexCounts = {4, 3};
        stripMesh.faceColors = {
            {1.0f, 0.0f, 0.0f, 1.0f},
            {0.0f, 1.0f, 0.0f, 1.0f}
        };

        obol::Mesh quadMesh;
        quadMesh.topology = obol::MeshTopology::QuadGrid;
        quadMesh.gridVertexRows = 2;
        quadMesh.gridVertexColumns = 3;
        quadMesh.positions = {
            {-1.0f,  1.0f, 0.0f},
            { 0.0f,  1.0f, 0.0f},
            { 1.0f,  1.0f, 0.0f},
            {-1.0f, -1.0f, 0.0f},
            { 0.0f, -1.0f, 0.0f},
            { 1.0f, -1.0f, 0.0f}
        };
        quadMesh.faceColors = {
            {0.0f, 0.0f, 1.0f, 1.0f},
            {1.0f, 1.0f, 0.0f, 1.0f}
        };

        obol::Scene scene;
        scene.addMesh(stripMesh);
        scene.addMesh(quadMesh);

        std::string iv;
        const bool wrote = obol::SceneIO::writeInventorString(scene, iv);

        obol::Scene loaded;
        const bool read = obol::SceneIO::readInventorString(iv, loaded, &manager);

        obol::SceneQuery meshQuery;
        meshQuery.type = obol::SceneObjectType::Mesh;

        SoSeparator * root = legacySceneGraph(loaded);
        SoSeparator * stripObject = findNamedSeparator(root, "ObolSceneObject_1");
        SoSeparator * quadObject = findNamedSeparator(root, "ObolSceneObject_2");
        SoTriangleStripSet * stripSet =
            findFirstNodeOfType<SoTriangleStripSet>(stripObject);
        SoMaterial * stripMaterial = findFirstNodeOfType<SoMaterial>(stripObject);
        SoMaterialBinding * stripBinding =
            findFirstNodeOfType<SoMaterialBinding>(stripObject);
        SoQuadMesh * quadSet = findFirstNodeOfType<SoQuadMesh>(quadObject);
        SoMaterial * quadMaterial = findFirstNodeOfType<SoMaterial>(quadObject);
        SoMaterialBinding * quadBinding =
            findFirstNodeOfType<SoMaterialBinding>(quadObject);

        const bool pass =
            wrote &&
            read &&
            !iv.empty() &&
            loaded.objectCount() == 2 &&
            loaded.hasObjects(meshQuery) &&
            stripObject &&
            stripSet &&
            stripSet->numVertices.getNum() == 2 &&
            stripSet->numVertices[0] == 4 &&
            stripSet->numVertices[1] == 3 &&
            stripMaterial &&
            stripMaterial->diffuseColor.getNum() == 2 &&
            stripBinding &&
            stripBinding->value.getValue() == SoMaterialBinding::PER_PART &&
            quadObject &&
            quadSet &&
            quadSet->verticesPerRow.getValue() == 3 &&
            quadSet->verticesPerColumn.getValue() == 2 &&
            quadMaterial &&
            quadMaterial->diffuseColor.getNum() == 2 &&
            quadBinding &&
            quadBinding->value.getValue() == SoMaterialBinding::PER_FACE;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Native strip/quad-grid meshes did not survive SceneIO round-trip");
    }

    runner.startTest("v2 SceneIO round-trips native line and point geometry");
    {
        obol::Polyline polyline;
        polyline.lineWidth = 4.0f;
        polyline.points = {
            {-1.0f, 0.0f, 0.0f},
            { 0.0f, 1.0f, 0.0f},
            { 1.0f, 0.0f, 0.0f}
        };

        obol::PointCloud pointCloud;
        pointCloud.pointSize = 6.0f;
        pointCloud.points = {
            {-1.0f, -1.0f, 0.0f},
            { 0.0f, -0.5f, 0.0f},
            { 1.0f, -1.0f, 0.0f}
        };

        obol::Material lineMaterial;
        lineMaterial.baseColor = {1.0f, 0.0f, 0.0f, 1.0f};
        obol::Material pointMaterial;
        pointMaterial.baseColor = {0.0f, 0.0f, 1.0f, 1.0f};

        obol::Scene scene;
        scene.addPolyline(polyline, lineMaterial);
        scene.addPointCloud(pointCloud, pointMaterial);

        std::string iv;
        const bool wrote = obol::SceneIO::writeInventorString(scene, iv);

        obol::Scene loaded;
        const bool read = obol::SceneIO::readInventorString(iv, loaded, &manager);

        obol::SceneQuery lineQuery;
        lineQuery.type = obol::SceneObjectType::Polyline;
        obol::SceneQuery pointQuery;
        pointQuery.type = obol::SceneObjectType::PointCloud;

        SoSeparator * root = legacySceneGraph(loaded);
        SoSeparator * lineObject = findNamedSeparator(root, "ObolSceneObject_1");
        SoSeparator * pointObject = findNamedSeparator(root, "ObolSceneObject_2");
        SoDrawStyle * lineStyle = findFirstNodeOfType<SoDrawStyle>(lineObject);
        SoDrawStyle * pointStyle = findFirstNodeOfType<SoDrawStyle>(pointObject);
        SoLineSet * lineSet = findFirstNodeOfType<SoLineSet>(lineObject);
        SoPointSet * pointSet = findFirstNodeOfType<SoPointSet>(pointObject);
        SoMaterial * lineRoundTripMaterial =
            findFirstNodeOfType<SoMaterial>(lineObject);
        SoMaterial * pointRoundTripMaterial =
            findFirstNodeOfType<SoMaterial>(pointObject);
        const SbColor lineColor =
            lineRoundTripMaterial && lineRoundTripMaterial->diffuseColor.getNum() > 0
                ? lineRoundTripMaterial->diffuseColor[0]
                : SbColor(0.0f, 0.0f, 0.0f);
        const SbColor pointColor =
            pointRoundTripMaterial && pointRoundTripMaterial->diffuseColor.getNum() > 0
                ? pointRoundTripMaterial->diffuseColor[0]
                : SbColor(0.0f, 0.0f, 0.0f);

        const bool pass =
            wrote &&
            read &&
            !iv.empty() &&
            loaded.objectCount() == 2 &&
            loaded.hasObjects(lineQuery) &&
            loaded.hasObjects(pointQuery) &&
            lineObject &&
            pointObject &&
            lineStyle &&
            lineStyle->lineWidth.getValue() == 4.0f &&
            lineSet &&
            lineSet->numVertices.getNum() == 1 &&
            lineSet->numVertices[0] == 3 &&
            lineColor[0] == 1.0f &&
            lineColor[1] == 0.0f &&
            lineColor[2] == 0.0f &&
            pointStyle &&
            pointStyle->pointSize.getValue() == 6.0f &&
            pointSet &&
            pointSet->numPoints.getValue() == 3 &&
            pointColor[0] == 0.0f &&
            pointColor[1] == 0.0f &&
            pointColor[2] == 1.0f;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Native line/point geometry did not survive SceneIO round-trip");
    }

    runner.startTest("v2 SceneIO round-trips native texture and light model state");
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
        texture.wrapS = obol::TextureWrap::Clamp;
        texture.wrapT = obol::TextureWrap::Repeat;
        texture.model = obol::TextureModel::Replace;

        obol::Material material;
        material.baseColorTexture.reset(new obol::Texture2D(texture));
        material.unlit = true;

        obol::Scene scene;
        scene.addPrimitive(obol::Primitive::Cube, material);

        std::string iv;
        const bool wrote = obol::SceneIO::writeInventorString(scene, iv);

        obol::Scene loaded;
        const bool read = obol::SceneIO::readInventorString(iv, loaded, &manager);

        obol::SceneQuery primitiveQuery;
        primitiveQuery.type = obol::SceneObjectType::Primitive;

        SoSeparator * root = legacySceneGraph(loaded);
        SoSeparator * object = findNamedSeparator(root, "ObolSceneObject_1");
        SoTexture2 * textureNode = findFirstNodeOfType<SoTexture2>(object);
        SoLightModel * lightModel = findFirstNodeOfType<SoLightModel>(object);
        SbVec2s size;
        int components = 0;
        const unsigned char * pixels =
            textureNode ? textureNode->image.getValue(size, components) : nullptr;

        const bool pass =
            wrote &&
            read &&
            !iv.empty() &&
            loaded.objectCount() == 1 &&
            loaded.hasObjects(primitiveQuery) &&
            object &&
            textureNode &&
            size[0] == 2 &&
            size[1] == 2 &&
            components == 4 &&
            pixels &&
            pixels[0] == 255 &&
            pixels[1] == 0 &&
            pixels[2] == 0 &&
            pixels[3] == 255 &&
            textureNode->wrapS.getValue() == SoTexture2::CLAMP &&
            textureNode->wrapT.getValue() == SoTexture2::REPEAT &&
            textureNode->model.getValue() == SoTexture2::REPLACE &&
            lightModel &&
            lightModel->model.getValue() == SoLightModel::BASE_COLOR;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Native texture/light-model state did not survive SceneIO round-trip");
    }

    runner.startTest("v2 SceneIO round-trips native text nodes");
    {
        obol::Text2D text2;
        text2.text = "Screen label";
        text2.fontName = "Sans";
        text2.fontSize = 18.0f;
        text2.spacing = 1.25f;
        text2.justification = obol::TextJustification::Center;
        text2.depthTest = false;

        obol::Text3D text3;
        text3.text = "Solid";
        text3.fontName = "Serif";
        text3.fontSize = 2.0f;
        text3.spacing = 0.85f;
        text3.justification = obol::TextJustification::Right;
        text3.parts = static_cast<uint32_t>(obol::Text3DParts::Front) |
                      static_cast<uint32_t>(obol::Text3DParts::Sides);
        text3.partColors = {
            {1.0f, 0.0f, 0.0f, 1.0f},
            {0.0f, 1.0f, 0.0f, 1.0f}
        };
        text3.profile = {
            {0.0f, 0.0f},
            {0.1f, 0.0f},
            {0.1f, 0.2f}
        };

        obol::Scene scene;
        scene.addText2D(text2);
        scene.addText3D(text3);

        std::string iv;
        const bool wrote = obol::SceneIO::writeInventorString(scene, iv);

        obol::Scene loaded;
        const bool read = obol::SceneIO::readInventorString(iv, loaded, &manager);

        obol::SceneQuery text2Query;
        text2Query.type = obol::SceneObjectType::Text2D;
        obol::SceneQuery text3Query;
        text3Query.type = obol::SceneObjectType::Text3D;

        SoSeparator * root = legacySceneGraph(loaded);
        SoSeparator * text2Object = findNamedSeparator(root, "ObolSceneObject_1");
        SoSeparator * text3Object = findNamedSeparator(root, "ObolSceneObject_2");
        SoFont * text2Font = findFirstNodeOfType<SoFont>(text2Object);
        SoText2 * text2Node = findFirstNodeOfType<SoText2>(text2Object);
        SoFont * text3Font = findFirstNodeOfType<SoFont>(text3Object);
        SoText3 * text3Node = findFirstNodeOfType<SoText3>(text3Object);
        SoMaterial * text3Material =
            findMaterialWithDiffuseColorCount(text3Object, 2);
        SoMaterialBinding * text3Binding =
            findFirstNodeOfType<SoMaterialBinding>(text3Object);
        SoProfileCoordinate2 * text3ProfileCoords =
            findFirstNodeOfType<SoProfileCoordinate2>(text3Object);
        SoLinearProfile * text3Profile =
            findFirstNodeOfType<SoLinearProfile>(text3Object);

        const bool pass =
            wrote &&
            read &&
            !iv.empty() &&
            loaded.objectCount() == 2 &&
            loaded.hasObjects(text2Query) &&
            loaded.hasObjects(text3Query) &&
            text2Font &&
            strcmp(text2Font->name.getValue().getString(), "Sans") == 0 &&
            text2Font->size.getValue() == 18.0f &&
            text2Node &&
            text2Node->string.getNum() == 1 &&
            strcmp(text2Node->string[0].getString(), "Screen label") == 0 &&
            text2Node->spacing.getValue() == 1.25f &&
            text2Node->justification.getValue() == SoText2::CENTER &&
            text2Node->depthTest.getValue() == FALSE &&
            text3Font &&
            strcmp(text3Font->name.getValue().getString(), "Serif") == 0 &&
            text3Font->size.getValue() == 2.0f &&
            text3Node &&
            text3Node->string.getNum() == 1 &&
            strcmp(text3Node->string[0].getString(), "Solid") == 0 &&
            text3Node->spacing.getValue() == 0.85f &&
            text3Node->justification.getValue() == SoText3::RIGHT &&
            text3Node->parts.getValue() ==
                (SoText3::FRONT | SoText3::SIDES) &&
            text3Material &&
            text3Material->diffuseColor.getNum() == 2 &&
            text3Binding &&
            text3Binding->value.getValue() == SoMaterialBinding::PER_PART &&
            text3ProfileCoords &&
            text3ProfileCoords->point.getNum() == 3 &&
            text3ProfileCoords->point[0] == SbVec2f(0.0f, 0.0f) &&
            text3ProfileCoords->point[2] == SbVec2f(0.1f, 0.2f) &&
            text3Profile &&
            text3Profile->index.getNum() == 3 &&
            text3Profile->index[0] == 0 &&
            text3Profile->index[2] == 2;
        const std::string detail =
            std::string("wrote=") + (wrote ? "true" : "false") +
            " read=" + (read ? "true" : "false") +
            " objects=" + std::to_string(loaded.objectCount()) +
            " hasText2=" + (loaded.hasObjects(text2Query) ? "true" : "false") +
            " hasText3=" + (loaded.hasObjects(text3Query) ? "true" : "false") +
            " text2Font=" + (text2Font ? "true" : "false") +
            " text2Node=" + (text2Node ? "true" : "false") +
            " text3Font=" + (text3Font ? "true" : "false") +
            " text3Node=" + (text3Node ? "true" : "false") +
            " text3MaterialColors=" +
                std::to_string(text3Material ? text3Material->diffuseColor.getNum() : -1) +
            " text3Binding=" +
                std::to_string(text3Binding ? text3Binding->value.getValue() : -1) +
            " text3ProfileCoords=" +
                std::to_string(text3ProfileCoords ? text3ProfileCoords->point.getNum() : -1) +
            " text3Profile=" +
                std::to_string(text3Profile ? text3Profile->index.getNum() : -1);
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : detail.c_str());
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

    runner.startTest("v2 Picker reports object ids for native SceneIO imports");
    {
        const std::string iv =
            "#Inventor V2.0 ascii\n"
            "Separator {\n"
            "  Material { diffuseColor 1 0 0 }\n"
            "  Cube { width 2 height 2 depth 2 }\n"
            "}\n";

        obol::Scene loaded;
        const bool read = obol::SceneIO::readInventorString(iv, loaded, &manager);

        obol::SceneQuery primitiveQuery;
        primitiveQuery.type = obol::SceneObjectType::Primitive;
        const obol::SceneObjectId imported = loaded.findFirstObject(primitiveQuery);

        obol::PickRequest request;
        request.viewportWidth = 100;
        request.viewportHeight = 100;
        request.useWorldRay = true;
        request.rayOrigin = {0.0f, 0.0f, 5.0f};
        request.rayDirection = {0.0f, 0.0f, -1.0f};

        const obol::PickResult pick = obol::Picker::pick(loaded, request);

        const bool pass =
            read &&
            loaded.objectCount() == 1 &&
            imported == 1 &&
            pick.hit &&
            !pick.hits.empty() &&
            pick.hits[0].onGeometry &&
            pick.hits[0].objectId == imported;
        runner.endTest(pass, pass ? "" : "Expected imported native .iv object to be pickable by v2 id");
    }

    runner.startTest("v2 Picker reports object ids for fallback SceneIO imports");
    {
        const std::string iv =
            "#Inventor V2.0 ascii\n"
            "Separator {\n"
            "  Material { diffuseColor [ 1 0 0, 0 1 0 ] }\n"
            "  MaterialBinding { value PER_PART }\n"
            "  Cube { width 2 height 2 depth 2 }\n"
            "}\n";

        obol::Scene loaded;
        const bool read = obol::SceneIO::readInventorString(iv, loaded, &manager);

        obol::SceneQuery legacyQuery;
        legacyQuery.type = obol::SceneObjectType::LegacySceneGraph;
        const obol::SceneObjectId imported = loaded.findFirstObject(legacyQuery);

        obol::PickRequest request;
        request.viewportWidth = 100;
        request.viewportHeight = 100;
        request.useWorldRay = true;
        request.rayOrigin = {0.0f, 0.0f, 5.0f};
        request.rayDirection = {0.0f, 0.0f, -1.0f};

        const obol::PickResult pick = obol::Picker::pick(loaded, request);

        const bool pass =
            read &&
            loaded.objectCount() == 1 &&
            imported == 1 &&
            pick.hit &&
            !pick.hits.empty() &&
            pick.hits[0].onGeometry &&
            pick.hits[0].objectId == imported;
        runner.endTest(pass, pass ? "" : "Expected fallback .iv import to be pickable by v2 id");
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

    runner.startTest("v2 SceneIO wraps unsupported Inventor bindings as legacy objects");
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
        obol::SceneQuery legacyQuery;
        legacyQuery.type = obol::SceneObjectType::LegacySceneGraph;

        const bool pass =
            read &&
            loaded.objectCount() == 1 &&
            loaded.hasObjects(legacyQuery) &&
            root &&
            root->getNumChildren() == 1 &&
            nativeObject != nullptr;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Unsupported Inventor binding did not become a v2 legacy object");
    }

    runner.startTest("v2 SceneIO legacy fallback objects report packet diagnostics");
    {
        const std::string iv =
            "#Inventor V2.0 ascii\n"
            "Separator {\n"
            "  Material { diffuseColor [ 1 0 0, 0 1 0 ] }\n"
            "  MaterialBinding { value PER_PART }\n"
            "  Cube { width 2 height 2 depth 2 }\n"
            "}\n";

        obol::Scene loaded;
        const bool read = obol::SceneIO::readInventorString(iv, loaded, &manager);
        const obol::ScenePacket packet = loaded.capturePacket();

        obol::ExtractedPacketScene extracted;
        const bool complete = obol::extractPacketScene(packet, extracted);

        const bool pass =
            read &&
            loaded.objectCount() == 1 &&
            packet.objects.size() == 1 &&
            !packet.hasLegacyFallbackRoot &&
            packet.objects[0].type == obol::SceneObjectType::LegacySceneGraph &&
            packet.objects[0].hasLegacySceneGraph &&
            !complete &&
            !extracted.complete &&
            extracted.support.legacyObjects == 1 &&
            extracted.support.hasLegacyFallbackRoot == false &&
            extracted.diagnostics.size() == 1 &&
            extracted.diagnostics[0].objectId == packet.objects[0].id &&
            extracted.diagnostics[0].message.find("legacy scene graph") !=
                std::string::npos &&
            extracted.triangles.empty() &&
            extracted.lineSegments.empty() &&
            extracted.points.empty();
        runner.endTest(pass, pass ? "" : "SceneIO legacy fallback did not report packet-only backend diagnostics");
    }

    runner.startTest("v2 SceneIO legacy fallback objects are lit by v2 lights");
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
            root->getChild(1)->isOfType(SoSeparator::getClassTypeId()) &&
            findNamedSeparator(root, "ObolSceneObject_1") != nullptr;
        if (root) root->unref();
        runner.endTest(pass, pass ? "" : "Legacy fallback object was not placed after v2 lights");
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

    runner.startTest("v2 Picker reports object ids for legacy SceneIO imports");
    {
        const std::string iv =
            "#Inventor V2.0 ascii\n"
            "Separator {\n"
            "  Material { diffuseColor [ 1 0 0, 0 1 0 ] }\n"
            "  MaterialBinding { value PER_PART }\n"
            "  Cube { width 2 height 2 depth 2 }\n"
            "}\n";

        obol::Scene scene;
        obol::Transform transform;
        transform.translation = {1.0f, 2.0f, 0.0f};
        const obol::SceneObjectId object =
            obol::SceneIO::addInventorString(iv, scene, transform,
                                             obol::RootSceneGroupId, &manager);

        obol::SceneQuery backendQuery;
        backendQuery.type = obol::SceneObjectType::LegacySceneGraph;

        obol::PickRequest request;
        request.viewportWidth = 100;
        request.viewportHeight = 100;
        request.useWorldRay = true;
        request.rayOrigin = {1.0f, 2.0f, 5.0f};
        request.rayDirection = {0.0f, 0.0f, -1.0f};

        const obol::PickResult pick = obol::Picker::pick(scene, request);

        const bool pass =
            object == 1 &&
            scene.hasObjects(backendQuery) &&
            pick.hit &&
            !pick.hits.empty() &&
            pick.hits[0].onGeometry &&
            pick.hits[0].objectId == object;
        runner.endTest(pass, pass ? "" : "Expected transformable legacy import to be pickable by v2 id");
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
