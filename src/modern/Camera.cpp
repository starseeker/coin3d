#include <Obol/scene/Camera.h>

#include <Inventor/SbViewportRegion.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoSeparator.h>

#include <cmath>

namespace obol {
namespace {

SbVec3f toSbVec3f(const Vec3 & value)
{
    return SbVec3f(value.x, value.y, value.z);
}

Vec3 toVec3(const SbVec3f & value)
{
    return {value[0], value[1], value[2]};
}

float vectorLength(const Vec3 & value)
{
    return std::sqrt(value.x * value.x +
                     value.y * value.y +
                     value.z * value.z);
}

Vec3 normalized(const Vec3 & value)
{
    const float length = vectorLength(value);
    if (length <= 0.0f) {
        return {0.0f, 0.0f, 0.0f};
    }
    return {value.x / length, value.y / length, value.z / length};
}

Vec3 added(const Vec3 & lhs, const Vec3 & rhs)
{
    return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

Vec3 subtracted(const Vec3 & lhs, const Vec3 & rhs)
{
    return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

Vec3 scaled(const Vec3 & value, float scale)
{
    return {value.x * scale, value.y * scale, value.z * scale};
}

float dot(const Vec3 & lhs, const Vec3 & rhs)
{
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

Vec3 cross(const Vec3 & lhs, const Vec3 & rhs)
{
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x
    };
}

Vec3 rotateAroundAxis(const Vec3 & value, const Vec3 & axis, float radians)
{
    const Vec3 normal = normalized(axis);
    if (vectorLength(normal) <= 0.0f) {
        return value;
    }

    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return added(
        added(scaled(value, c),
              scaled(cross(normal, value), s)),
        scaled(normal, dot(normal, value) * (1.0f - c)));
}

PerspectiveCamera toPerspectiveCamera(const SoPerspectiveCamera & cameraNode)
{
    SbVec3f forward;
    SbVec3f up;
    cameraNode.orientation.getValue().multVec(SbVec3f(0.0f, 0.0f, -1.0f),
                                              forward);
    cameraNode.orientation.getValue().multVec(SbVec3f(0.0f, 1.0f, 0.0f),
                                              up);

    const SbVec3f position = cameraNode.position.getValue();
    const SbVec3f target =
        position + forward * cameraNode.focalDistance.getValue();

    PerspectiveCamera camera;
    camera.position = toVec3(position);
    camera.target = toVec3(target);
    camera.up = toVec3(up);
    camera.verticalFieldOfViewRadians = cameraNode.heightAngle.getValue();
    camera.nearDistance = cameraNode.nearDistance.getValue();
    camera.farDistance = cameraNode.farDistance.getValue();
    return camera;
}

} // namespace

PerspectiveCamera
CameraFraming::viewAllPerspective(const Scene & scene,
                                  const ViewAllRequest & request)
{
    Scene viewScene = scene;
    viewScene.clearCamera();

    SoSeparator * sceneRoot =
        static_cast<SoSeparator *>(viewScene.createLegacySceneGraph());
    SoSeparator * viewRoot = new SoSeparator;
    viewRoot->ref();

    SoPerspectiveCamera * cameraNode = new SoPerspectiveCamera;
    cameraNode->position.setValue(toSbVec3f(request.position));
    cameraNode->pointAt(toSbVec3f(request.target), toSbVec3f(request.up));
    viewRoot->addChild(cameraNode);
    viewRoot->addChild(sceneRoot);
    sceneRoot->unref();

    cameraNode->viewAll(viewRoot,
                        SbViewportRegion(request.viewportWidth,
                                         request.viewportHeight),
                        request.slack);
    const PerspectiveCamera camera = toPerspectiveCamera(*cameraNode);

    viewRoot->unref();
    return camera;
}

PerspectiveCamera
CameraFraming::orbit(const CameraOrbitRequest & request)
{
    PerspectiveCamera camera = request.camera;
    Vec3 offset = subtracted(request.camera.position, request.center);
    if (vectorLength(offset) <= 0.0f) {
        return camera;
    }

    const Vec3 worldUp = normalized(request.worldUp);
    if (vectorLength(worldUp) <= 0.0f) {
        return camera;
    }

    offset = rotateAroundAxis(offset, worldUp, request.azimuthRadians);

    Vec3 viewDirection = normalized(scaled(offset, -1.0f));
    Vec3 right = cross(worldUp, viewDirection);
    if (vectorLength(right) <= 1.0e-4f) {
        right = {1.0f, 0.0f, 0.0f};
    } else {
        right = normalized(right);
    }

    offset = rotateAroundAxis(offset, right, request.elevationRadians);
    camera.position = added(request.center, offset);
    camera.target = request.center;
    camera.up = worldUp;
    return camera;
}

} // namespace obol
