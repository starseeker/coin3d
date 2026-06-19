#include <Obol/scene/Interaction.h>

#include <algorithm>
#include <cmath>

namespace obol {
namespace {

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

Vec3 scaled(const Vec3 & value, float scale)
{
    return {value.x * scale, value.y * scale, value.z * scale};
}

Vec3 added(const Vec3 & lhs, const Vec3 & rhs)
{
    return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

Vec3 subtracted(const Vec3 & lhs, const Vec3 & rhs)
{
    return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

float clampScalar(float value, float first, float second)
{
    const float minimum = std::min(first, second);
    const float maximum = std::max(first, second);
    return std::max(minimum, std::min(maximum, value));
}

Vec3 clamped(const Vec3 & value, const TranslationBounds & bounds)
{
    if (!bounds.enabled) {
        return value;
    }
    return {
        clampScalar(value.x, bounds.minimum.x, bounds.maximum.x),
        clampScalar(value.y, bounds.minimum.y, bounds.maximum.y),
        clampScalar(value.z, bounds.minimum.z, bounds.maximum.z)
    };
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

Polyline makeCirclePolyline(const Vec3 & axis,
                            float radius,
                            float lineWidth,
                            uint32_t segments)
{
    Polyline polyline;
    polyline.lineWidth = lineWidth;
    if (radius <= 0.0f) {
        return polyline;
    }

    const Vec3 normal = normalized(axis);
    if (vectorLength(normal) <= 0.0f) {
        return polyline;
    }

    const Vec3 reference = std::fabs(dot(normal, {0.0f, 0.0f, 1.0f})) < 0.9f
        ? Vec3{0.0f, 0.0f, 1.0f}
        : Vec3{0.0f, 1.0f, 0.0f};
    const Vec3 u = normalized(cross(normal, reference));
    const Vec3 v = normalized(cross(normal, u));
    if (vectorLength(u) <= 0.0f || vectorLength(v) <= 0.0f) {
        return polyline;
    }

    const uint32_t count = std::max<uint32_t>(segments, 8);
    polyline.points.reserve(static_cast<size_t>(count) + 1);
    constexpr float pi = 3.14159265358979323846f;
    for (uint32_t i = 0; i <= count; ++i) {
        const float angle = 2.0f * pi *
            static_cast<float>(i) / static_cast<float>(count);
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        polyline.points.push_back({
            radius * (u.x * c + v.x * s),
            radius * (u.y * c + v.y * s),
            radius * (u.z * c + v.z * s)
        });
    }
    return polyline;
}

Polyline makeBoxPolyline(const Vec3 & halfSize, float lineWidth)
{
    Polyline box;
    box.lineWidth = lineWidth;
    const float x = halfSize.x;
    const float y = halfSize.y;
    const float z = halfSize.z;
    if (x <= 0.0f || y <= 0.0f || z <= 0.0f) {
        return box;
    }

    box.points = {
        {-x, -y, -z}, { x, -y, -z}, { x,  y, -z}, {-x,  y, -z},
        {-x, -y, -z}, {-x, -y,  z}, { x, -y,  z}, { x,  y,  z},
        {-x,  y,  z}, {-x, -y,  z}, { x, -y,  z}, { x, -y, -z},
        { x,  y, -z}, { x,  y,  z}, {-x,  y,  z}, {-x,  y, -z}
    };
    return box;
}

} // namespace

AxisDragResult
TransformDragger::translateOnAxis(const AxisDragRequest & request)
{
    AxisDragResult result;
    result.target = request.target;
    result.transform = request.startTransform;

    const Vec3 axis = normalized(request.axis);
    if (vectorLength(axis) <= 0.0f) {
        return result;
    }

    float distance = request.distance;
    if (request.clamp) {
        distance = clampScalar(distance,
                               request.minimumDistance,
                               request.maximumDistance);
    }

    result.valid = true;
    result.distance = distance;
    result.delta = scaled(axis, distance);
    result.transform.translation =
        added(request.startTransform.translation, result.delta);
    return result;
}

bool
TransformDragger::applyAxisTranslation(Scene & scene,
                                       const AxisDragRequest & request,
                                       AxisDragResult * output)
{
    const AxisDragResult result = translateOnAxis(request);
    if (output) {
        *output = result;
    }
    if (!result.valid || request.target == InvalidSceneObjectId) {
        return false;
    }
    return scene.setObjectTransform(result.target, result.transform);
}

TranslationResult
TransformDragger::translateFreely(const FreeDragRequest & request)
{
    TranslationResult result;
    result.valid = true;
    result.target = request.target;
    result.transform = request.startTransform;

    const Vec3 requestedTranslation =
        added(request.startTransform.translation, request.delta);
    result.transform.translation = clamped(requestedTranslation, request.bounds);
    result.delta =
        subtracted(result.transform.translation, request.startTransform.translation);
    return result;
}

bool
TransformDragger::applyFreeTranslation(Scene & scene,
                                       const FreeDragRequest & request,
                                       TranslationResult * output)
{
    const TranslationResult result = translateFreely(request);
    if (output) {
        *output = result;
    }
    if (!result.valid || request.target == InvalidSceneObjectId) {
        return false;
    }
    return scene.setObjectTransform(result.target, result.transform);
}

TranslationResult
TransformDragger::translateOnPlane(const PlaneDragRequest & request)
{
    TranslationResult result;
    result.target = request.target;
    result.transform = request.startTransform;

    const Vec3 normal = normalized(request.planeNormal);
    if (vectorLength(normal) <= 0.0f) {
        return result;
    }

    const Vec3 projectedDelta =
        subtracted(request.delta, scaled(normal, dot(request.delta, normal)));
    const Vec3 requestedTranslation =
        added(request.startTransform.translation, projectedDelta);
    result.transform.translation = clamped(requestedTranslation, request.bounds);
    result.delta =
        subtracted(result.transform.translation, request.startTransform.translation);
    result.valid = true;
    return result;
}

bool
TransformDragger::applyPlaneTranslation(Scene & scene,
                                        const PlaneDragRequest & request,
                                        TranslationResult * output)
{
    const TranslationResult result = translateOnPlane(request);
    if (output) {
        *output = result;
    }
    if (!result.valid || request.target == InvalidSceneObjectId) {
        return false;
    }
    return scene.setObjectTransform(result.target, result.transform);
}

AxisRotationResult
TransformDragger::rotateOnAxis(const AxisRotationRequest & request)
{
    AxisRotationResult result;
    result.target = request.target;
    result.transform = request.startTransform;

    const Vec3 axis = normalized(request.axis);
    if (vectorLength(axis) <= 0.0f) {
        return result;
    }

    float angle = request.angleRadians;
    if (request.clamp) {
        angle = clampScalar(angle,
                            request.minimumAngleRadians,
                            request.maximumAngleRadians);
    }

    result.valid = true;
    result.axis = axis;
    result.angleRadians = angle;
    result.transform.rotationAxis = axis;
    result.transform.rotationRadians = angle;
    return result;
}

bool
TransformDragger::applyAxisRotation(Scene & scene,
                                    const AxisRotationRequest & request,
                                    AxisRotationResult * output)
{
    const AxisRotationResult result = rotateOnAxis(request);
    if (output) {
        *output = result;
    }
    if (!result.valid || request.target == InvalidSceneObjectId) {
        return false;
    }
    return scene.setObjectTransform(result.target, result.transform);
}

InteractionOverlay
TransformDragger::addTranslateAxisOverlay(Scene & scene,
                                          const TranslateAxisOverlay & overlay)
{
    InteractionOverlay result;
    const Vec3 axis = normalized(overlay.axis);
    if (vectorLength(axis) <= 0.0f || overlay.length <= 0.0f) {
        return result;
    }

    result.group = scene.addGroup(overlay.transform, overlay.parent);

    const float half = overlay.length * 0.5f;
    Polyline rail;
    rail.lineWidth = overlay.lineWidth;
    rail.points = {scaled(axis, -half), scaled(axis, half)};
    result.objects.push_back(
        scene.addPolyline(rail, overlay.material, Transform{}, result.group));

    PrimitiveOptions handle;
    handle.width = overlay.handleSize;
    handle.height = overlay.handleSize;
    handle.depth = overlay.handleSize;

    Transform positive;
    positive.translation = scaled(axis, half);
    result.objects.push_back(
        scene.addPrimitive(Primitive::Cube,
                           overlay.material,
                           positive,
                           handle,
                           result.group));

    Transform negative;
    negative.translation = scaled(axis, -half);
    result.objects.push_back(
        scene.addPrimitive(Primitive::Cube,
                           overlay.material,
                           negative,
                           handle,
                           result.group));

    return result;
}

InteractionOverlay
TransformDragger::addTrackballOverlay(Scene & scene,
                                      const TrackballOverlay & overlay)
{
    InteractionOverlay result;
    if (overlay.radius <= 0.0f) {
        return result;
    }

    result.group = scene.addGroup(overlay.transform, overlay.parent);

    const auto addRing = [&](const Vec3 & axis, const Material & material) {
        Polyline ring =
            makeCirclePolyline(axis,
                               overlay.radius,
                               overlay.lineWidth,
                               overlay.segments);
        if (!ring.points.empty()) {
            result.objects.push_back(
                scene.addPolyline(ring, material, Transform{}, result.group));
        }
    };

    addRing({1.0f, 0.0f, 0.0f}, overlay.xMaterial);
    addRing({0.0f, 1.0f, 0.0f}, overlay.yMaterial);
    addRing({0.0f, 0.0f, 1.0f}, overlay.zMaterial);
    return result;
}

InteractionOverlay
TransformDragger::addBoxOverlay(Scene & scene,
                                const BoxOverlay & overlay)
{
    InteractionOverlay result;
    Polyline box = makeBoxPolyline(overlay.halfSize, overlay.lineWidth);
    if (box.points.empty()) {
        return result;
    }

    result.group = scene.addGroup(overlay.transform, overlay.parent);
    result.objects.push_back(
        scene.addPolyline(box, overlay.material, Transform{}, result.group));
    return result;
}

InteractionOverlay
TransformDragger::addManipulatorOverlay(Scene & scene,
                                        const ManipulatorOverlay & overlay)
{
    InteractionOverlay result;
    result.target = overlay.target;

    switch (overlay.kind) {
    case ManipulatorOverlayKind::HandleBox:
    case ManipulatorOverlayKind::TransformBox: {
        BoxOverlay box;
        box.transform = overlay.transform;
        box.halfSize = overlay.boxHalfSize;
        box.lineWidth = overlay.lineWidth;
        box.material = overlay.material;
        box.parent = overlay.parent;
        result = addBoxOverlay(scene, box);
        result.target = overlay.target;
        return result;
    }
    case ManipulatorOverlayKind::Trackball: {
        TrackballOverlay trackball;
        trackball.transform = overlay.transform;
        trackball.radius = overlay.trackballRadius;
        trackball.lineWidth = overlay.lineWidth;
        trackball.segments = overlay.segments;
        trackball.xMaterial = overlay.material;
        trackball.yMaterial = overlay.material;
        trackball.zMaterial = overlay.material;
        trackball.parent = overlay.parent;
        result = addTrackballOverlay(scene, trackball);
        result.target = overlay.target;
        return result;
    }
    }

    return result;
}

} // namespace obol
