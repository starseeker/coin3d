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

float vectorLength(const Vec2 & value)
{
    return std::sqrt(value.x * value.x + value.y * value.y);
}

Vec3 normalized(const Vec3 & value)
{
    const float length = vectorLength(value);
    if (length <= 0.0f) {
        return {0.0f, 0.0f, 0.0f};
    }
    return {value.x / length, value.y / length, value.z / length};
}

Vec2 normalized(const Vec2 & value)
{
    const float length = vectorLength(value);
    if (length <= 0.0f) {
        return {1.0f, 0.0f};
    }
    return {value.x / length, value.y / length};
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

Vec3 multiplied(const Vec3 & lhs, const Vec3 & rhs)
{
    return {lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z};
}

float clampScalar(float value, float first, float second)
{
    const float minimum = std::min(first, second);
    const float maximum = std::max(first, second);
    return std::max(minimum, std::min(maximum, value));
}

float snapScalar(float value, float step)
{
    const float magnitude = std::fabs(step);
    if (magnitude <= 0.0f) {
        return value;
    }
    return std::round(value / magnitude) * magnitude;
}

Vec3 snapped(const Vec3 & value, const Vec3 & step)
{
    return {
        snapScalar(value.x, step.x),
        snapScalar(value.y, step.y),
        snapScalar(value.z, step.z)
    };
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

Vec3 clamped(const Vec3 & value, const ScaleBounds & bounds)
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

float dot(const Vec2 & lhs, const Vec2 & rhs)
{
    return lhs.x * rhs.x + lhs.y * rhs.y;
}

float clampedDot(const Vec3 & lhs, const Vec3 & rhs)
{
    return clampScalar(dot(lhs, rhs), -1.0f, 1.0f);
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

bool syncMatchingAttachment(Scene & scene,
                            SceneObjectId target,
                            ManipulatorAttachment * attachment)
{
    if (!attachment || !attachment->attached) {
        return true;
    }
    if (attachment->target != target) {
        return false;
    }
    return TransformDragger::syncManipulator(scene, *attachment);
}

void addOverlayHandle(InteractionOverlay & overlay,
                      SceneObjectId object,
                      InteractionHandleKind kind,
                      const Vec3 & axis = Vec3{})
{
    overlay.objects.push_back(object);
    InteractionHandle handle;
    handle.object = object;
    handle.kind = kind;
    handle.axis = normalized(axis);
    overlay.handles.push_back(handle);
}

bool isEditableHandleKind(InteractionHandleKind kind)
{
    return kind == InteractionHandleKind::TranslateAxis ||
           kind == InteractionHandleKind::RotateAxis ||
           kind == InteractionHandleKind::ScaleUniform;
}

float projectedPointerDelta(const PointerDragGesture & gesture)
{
    const Vec2 direction = normalized(gesture.handleScreenDirection);
    return dot(gesture.delta, direction);
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
    if (request.snap) {
        distance = snapScalar(distance, request.snapStep);
    }
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

    const Vec3 delta = request.snap
        ? snapped(request.delta, request.snapStep)
        : request.delta;
    const Vec3 requestedTranslation =
        added(request.startTransform.translation, delta);
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

    Vec3 projectedDelta =
        subtracted(request.delta, scaled(normal, dot(request.delta, normal)));
    if (request.snap) {
        projectedDelta = snapped(projectedDelta, request.snapStep);
    }
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
    if (request.snap) {
        angle = snapScalar(angle, request.snapStepRadians);
    }
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

AxisRotationResult
TransformDragger::rotateTrackball(const TrackballRotationRequest & request)
{
    AxisRotationResult result;
    result.target = request.target;
    result.transform = request.startTransform;

    const Vec3 from = normalized(request.from);
    const Vec3 to = normalized(request.to);
    if (vectorLength(from) <= 0.0f || vectorLength(to) <= 0.0f) {
        return result;
    }

    Vec3 axis = normalized(cross(from, to));
    if (vectorLength(axis) <= 0.0f) {
        axis = normalized(request.fallbackAxis);
    }
    if (vectorLength(axis) <= 0.0f) {
        return result;
    }

    float angle = std::acos(clampedDot(from, to));
    if (request.snap) {
        angle = snapScalar(angle, request.snapStepRadians);
    }
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
TransformDragger::applyTrackballRotation(
    Scene & scene,
    const TrackballRotationRequest & request,
    AxisRotationResult * output)
{
    const AxisRotationResult result = rotateTrackball(request);
    if (output) {
        *output = result;
    }
    if (!result.valid || request.target == InvalidSceneObjectId) {
        return false;
    }
    return scene.setObjectTransform(result.target, result.transform);
}

ScaleResult
TransformDragger::scaleByFactors(const ScaleRequest & request)
{
    ScaleResult result;
    result.valid = true;
    result.target = request.target;
    result.transform = request.startTransform;

    Vec3 nextScale = multiplied(request.startTransform.scale, request.factors);
    if (request.snap) {
        nextScale = snapped(nextScale, request.snapStep);
    }
    nextScale = clamped(nextScale, request.bounds);

    result.scale = nextScale;
    result.transform.scale = nextScale;
    return result;
}

bool
TransformDragger::applyScale(Scene & scene,
                             const ScaleRequest & request,
                             ScaleResult * output)
{
    const ScaleResult result = scaleByFactors(request);
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
    addOverlayHandle(
        result,
        scene.addPolyline(rail, overlay.material, Transform{}, result.group),
        InteractionHandleKind::TranslateAxis,
        axis);

    PrimitiveOptions handle;
    handle.width = overlay.handleSize;
    handle.height = overlay.handleSize;
    handle.depth = overlay.handleSize;

    Transform positive;
    positive.translation = scaled(axis, half);
    addOverlayHandle(
        result,
        scene.addPrimitive(Primitive::Cube,
                           overlay.material,
                           positive,
                           handle,
                           result.group),
        InteractionHandleKind::TranslateAxis,
        axis);

    Transform negative;
    negative.translation = scaled(axis, -half);
    addOverlayHandle(
        result,
        scene.addPrimitive(Primitive::Cube,
                           overlay.material,
                           negative,
                           handle,
                           result.group),
        InteractionHandleKind::TranslateAxis,
        axis);

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
            addOverlayHandle(
                result,
                scene.addPolyline(ring, material, Transform{}, result.group),
                InteractionHandleKind::RotateAxis,
                axis);
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
    addOverlayHandle(
        result,
        scene.addPolyline(box, overlay.material, Transform{}, result.group),
        InteractionHandleKind::BoundsBox);
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
        if (!result.handles.empty()) {
            result.handles[0].kind =
                overlay.kind == ManipulatorOverlayKind::TransformBox
                ? InteractionHandleKind::ScaleUniform
                : InteractionHandleKind::BoundsBox;
        }
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

ManipulatorAttachment
TransformDragger::attachManipulator(Scene & scene,
                                    const ManipulatorOverlay & overlay)
{
    ManipulatorAttachment result;
    result.target = overlay.target;
    result.kind = overlay.kind;

    Transform targetTransform;
    if (overlay.target == InvalidSceneObjectId ||
        !scene.getObjectTransform(overlay.target, targetTransform)) {
        result.target = InvalidSceneObjectId;
        return result;
    }

    result.overlay = addManipulatorOverlay(scene, overlay);
    result.attached =
        result.overlay.group != InvalidSceneGroupId &&
        !result.overlay.objects.empty();
    result.visible = result.attached;

    if (result.attached) {
        syncOverlayToTarget(scene, result.overlay);
    }
    return result;
}

InteractionHandlePick
TransformDragger::resolveOverlayHandle(const InteractionOverlay & overlay,
                                       SceneObjectId object)
{
    InteractionHandlePick result;
    if (object == InvalidSceneObjectId) {
        return result;
    }

    for (const InteractionHandle & handle : overlay.handles) {
        if (handle.object == object) {
            result.valid = true;
            result.object = object;
            result.kind = handle.kind;
            result.axis = handle.axis;
            return result;
        }
    }
    return result;
}

InteractionHandlePick
TransformDragger::resolveManipulatorHandle(
    const ManipulatorAttachment & attachment,
    SceneObjectId object)
{
    if (!attachment.attached) {
        return InteractionHandlePick{};
    }
    return resolveOverlayHandle(attachment.overlay, object);
}

bool
TransformDragger::syncOverlayToTarget(Scene & scene,
                                      const InteractionOverlay & overlay)
{
    if (overlay.target == InvalidSceneObjectId ||
        overlay.group == InvalidSceneGroupId) {
        return false;
    }

    Transform targetTransform;
    if (!scene.getObjectTransform(overlay.target, targetTransform)) {
        return false;
    }
    return scene.setGroupTransform(overlay.group, targetTransform);
}

bool
TransformDragger::syncManipulator(Scene & scene,
                                  const ManipulatorAttachment & attachment)
{
    if (!attachment.attached) {
        return false;
    }
    return syncOverlayToTarget(scene, attachment.overlay);
}

bool
TransformDragger::setOverlayVisible(Scene & scene,
                                    const InteractionOverlay & overlay,
                                    bool visible)
{
    bool updated = false;
    if (overlay.group != InvalidSceneGroupId) {
        updated = scene.setGroupVisible(overlay.group, visible) || updated;
    }
    for (SceneObjectId object : overlay.objects) {
        updated = scene.setObjectVisible(object, visible) || updated;
    }
    return updated;
}

bool
TransformDragger::setManipulatorVisible(Scene & scene,
                                        ManipulatorAttachment & attachment,
                                        bool visible)
{
    if (!attachment.attached) {
        return false;
    }
    const bool updated = setOverlayVisible(scene, attachment.overlay, visible);
    if (updated) {
        attachment.visible = visible;
    }
    return updated;
}

bool
TransformDragger::removeOverlay(Scene & scene,
                                InteractionOverlay & overlay)
{
    bool removedAny = false;
    for (SceneObjectId object : overlay.objects) {
        removedAny = scene.removeObject(object) || removedAny;
    }
    const bool removedGroup =
        overlay.group != InvalidSceneGroupId &&
        scene.removeGroup(overlay.group);
    if (removedAny || removedGroup) {
        overlay.target = InvalidSceneObjectId;
        overlay.group = InvalidSceneGroupId;
        overlay.objects.clear();
        overlay.handles.clear();
    }
    return removedAny || removedGroup;
}

bool
TransformDragger::detachManipulator(Scene & scene,
                                    ManipulatorAttachment & attachment)
{
    if (!attachment.attached) {
        return false;
    }
    const bool detached = removeOverlay(scene, attachment.overlay);
    if (detached) {
        attachment.attached = false;
        attachment.visible = false;
        attachment.target = InvalidSceneObjectId;
    }
    return detached;
}

TransformEditState
TransformDragger::beginTransformEdit(const Scene & scene,
                                     SceneObjectId target)
{
    TransformEditState edit;
    if (target == InvalidSceneObjectId) {
        return edit;
    }

    Transform transform;
    if (!scene.getObjectTransform(target, transform)) {
        return edit;
    }

    edit.active = true;
    edit.target = target;
    edit.startTransform = transform;
    edit.currentTransform = transform;
    return edit;
}

bool
TransformDragger::updateEditAxisTranslation(
    Scene & scene,
    TransformEditState & edit,
    const AxisDragRequest & request,
    ManipulatorAttachment * attachment,
    AxisDragResult * output)
{
    if (!edit.active || edit.target == InvalidSceneObjectId) {
        return false;
    }

    AxisDragRequest effective = request;
    effective.target = edit.target;
    effective.startTransform = edit.startTransform;

    AxisDragResult result;
    const bool applied = applyAxisTranslation(scene, effective, &result);
    if (output) {
        *output = result;
    }
    if (!applied) {
        return false;
    }

    edit.currentTransform = result.transform;
    return syncMatchingAttachment(scene, edit.target, attachment);
}

bool
TransformDragger::updateEditFreeTranslation(
    Scene & scene,
    TransformEditState & edit,
    const FreeDragRequest & request,
    ManipulatorAttachment * attachment,
    TranslationResult * output)
{
    if (!edit.active || edit.target == InvalidSceneObjectId) {
        return false;
    }

    FreeDragRequest effective = request;
    effective.target = edit.target;
    effective.startTransform = edit.startTransform;

    TranslationResult result;
    const bool applied = applyFreeTranslation(scene, effective, &result);
    if (output) {
        *output = result;
    }
    if (!applied) {
        return false;
    }

    edit.currentTransform = result.transform;
    return syncMatchingAttachment(scene, edit.target, attachment);
}

bool
TransformDragger::updateEditPlaneTranslation(
    Scene & scene,
    TransformEditState & edit,
    const PlaneDragRequest & request,
    ManipulatorAttachment * attachment,
    TranslationResult * output)
{
    if (!edit.active || edit.target == InvalidSceneObjectId) {
        return false;
    }

    PlaneDragRequest effective = request;
    effective.target = edit.target;
    effective.startTransform = edit.startTransform;

    TranslationResult result;
    const bool applied = applyPlaneTranslation(scene, effective, &result);
    if (output) {
        *output = result;
    }
    if (!applied) {
        return false;
    }

    edit.currentTransform = result.transform;
    return syncMatchingAttachment(scene, edit.target, attachment);
}

bool
TransformDragger::updateEditAxisRotation(
    Scene & scene,
    TransformEditState & edit,
    const AxisRotationRequest & request,
    ManipulatorAttachment * attachment,
    AxisRotationResult * output)
{
    if (!edit.active || edit.target == InvalidSceneObjectId) {
        return false;
    }

    AxisRotationRequest effective = request;
    effective.target = edit.target;
    effective.startTransform = edit.startTransform;

    AxisRotationResult result;
    const bool applied = applyAxisRotation(scene, effective, &result);
    if (output) {
        *output = result;
    }
    if (!applied) {
        return false;
    }

    edit.currentTransform = result.transform;
    return syncMatchingAttachment(scene, edit.target, attachment);
}

bool
TransformDragger::updateEditTrackballRotation(
    Scene & scene,
    TransformEditState & edit,
    const TrackballRotationRequest & request,
    ManipulatorAttachment * attachment,
    AxisRotationResult * output)
{
    if (!edit.active || edit.target == InvalidSceneObjectId) {
        return false;
    }

    TrackballRotationRequest effective = request;
    effective.target = edit.target;
    effective.startTransform = edit.startTransform;

    AxisRotationResult result;
    const bool applied = applyTrackballRotation(scene, effective, &result);
    if (output) {
        *output = result;
    }
    if (!applied) {
        return false;
    }

    edit.currentTransform = result.transform;
    return syncMatchingAttachment(scene, edit.target, attachment);
}

bool
TransformDragger::updateEditScale(Scene & scene,
                                  TransformEditState & edit,
                                  const ScaleRequest & request,
                                  ManipulatorAttachment * attachment,
                                  ScaleResult * output)
{
    if (!edit.active || edit.target == InvalidSceneObjectId) {
        return false;
    }

    ScaleRequest effective = request;
    effective.target = edit.target;
    effective.startTransform = edit.startTransform;

    ScaleResult result;
    const bool applied = applyScale(scene, effective, &result);
    if (output) {
        *output = result;
    }
    if (!applied) {
        return false;
    }

    edit.currentTransform = result.transform;
    return syncMatchingAttachment(scene, edit.target, attachment);
}

bool
TransformDragger::updateEditFromHandle(
    Scene & scene,
    TransformEditState & edit,
    const InteractionHandleEditRequest & request,
    ManipulatorAttachment * attachment,
    InteractionHandleEditResult * output)
{
    InteractionHandleEditResult result;
    result.kind = request.handle.kind;
    if (!request.handle.valid ||
        !edit.active ||
        edit.target == InvalidSceneObjectId) {
        if (output) {
            *output = result;
        }
        return false;
    }

    bool applied = false;
    switch (request.handle.kind) {
    case InteractionHandleKind::TranslateAxis: {
        AxisDragRequest translation = request.translation;
        translation.axis = request.handle.axis;
        applied = updateEditAxisTranslation(scene,
                                            edit,
                                            translation,
                                            attachment,
                                            &result.translation);
        break;
    }
    case InteractionHandleKind::RotateAxis: {
        AxisRotationRequest rotation = request.rotation;
        rotation.axis = request.handle.axis;
        applied = updateEditAxisRotation(scene,
                                         edit,
                                         rotation,
                                         attachment,
                                         &result.rotation);
        break;
    }
    case InteractionHandleKind::ScaleUniform:
        applied = updateEditScale(scene,
                                  edit,
                                  request.scale,
                                  attachment,
                                  &result.scale);
        break;
    case InteractionHandleKind::BoundsBox:
    case InteractionHandleKind::None:
        applied = false;
        break;
    }

    result.applied = applied;
    if (output) {
        *output = result;
    }
    return applied;
}

ManipulatorEditSession
TransformDragger::beginManipulatorEdit(
    const Scene & scene,
    const ManipulatorAttachment & attachment,
    SceneObjectId pickedObject)
{
    ManipulatorEditSession session;
    if (!attachment.attached || attachment.target == InvalidSceneObjectId) {
        return session;
    }

    session.handle = resolveManipulatorHandle(attachment, pickedObject);
    if (!session.handle.valid ||
        !isEditableHandleKind(session.handle.kind)) {
        return session;
    }

    session.transformEdit = beginTransformEdit(scene, attachment.target);
    session.active = session.transformEdit.active;
    if (!session.active) {
        session.handle = InteractionHandlePick{};
    }
    return session;
}

bool
TransformDragger::updateManipulatorEdit(
    Scene & scene,
    ManipulatorEditSession & session,
    const ManipulatorEditRequest & request,
    ManipulatorAttachment & attachment,
    InteractionHandleEditResult * output)
{
    InteractionHandleEditResult result;
    result.kind = session.handle.kind;
    if (!session.active ||
        !attachment.attached ||
        attachment.target != session.transformEdit.target) {
        if (output) {
            *output = result;
        }
        return false;
    }

    InteractionHandleEditRequest editRequest;
    editRequest.handle = session.handle;
    editRequest.translation = request.translation;
    editRequest.rotation = request.rotation;
    editRequest.scale = request.scale;

    const bool updated = updateEditFromHandle(scene,
                                              session.transformEdit,
                                              editRequest,
                                              &attachment,
                                              &result);
    session.active = session.transformEdit.active && updated;
    if (output) {
        *output = result;
    }
    return updated;
}

bool
TransformDragger::commitManipulatorEdit(ManipulatorEditSession & session)
{
    if (!session.active) {
        return false;
    }
    const bool committed = commitTransformEdit(session.transformEdit);
    if (committed) {
        session.active = false;
    }
    return committed;
}

bool
TransformDragger::cancelManipulatorEdit(
    Scene & scene,
    ManipulatorEditSession & session,
    ManipulatorAttachment * attachment)
{
    if (!session.active) {
        return false;
    }
    const bool canceled =
        cancelTransformEdit(scene, session.transformEdit, attachment);
    if (canceled) {
        session.active = false;
    }
    return canceled;
}

PointerDragGestureResult
TransformDragger::mapPointerDragToManipulatorEdit(
    const ManipulatorEditSession & session,
    const PointerDragGesture & gesture)
{
    PointerDragGestureResult result;
    result.kind = session.handle.kind;
    if (!session.active ||
        !session.handle.valid ||
        !isEditableHandleKind(session.handle.kind)) {
        return result;
    }

    const float projected = projectedPointerDelta(gesture);
    result.valid = true;
    result.projectedDelta = projected;

    switch (session.handle.kind) {
    case InteractionHandleKind::TranslateAxis:
        result.edit.translation = gesture.translation;
        result.edit.translation.distance =
            projected * gesture.translationUnitsPerPixel;
        break;
    case InteractionHandleKind::RotateAxis:
        result.edit.rotation = gesture.rotation;
        result.edit.rotation.angleRadians =
            projected * gesture.rotationRadiansPerPixel;
        break;
    case InteractionHandleKind::ScaleUniform: {
        result.edit.scale = gesture.scale;
        const float factor =
            std::max(0.001f, 1.0f + projected * gesture.scaleFactorPerPixel);
        result.edit.scale.factors = {factor, factor, factor};
        break;
    }
    case InteractionHandleKind::BoundsBox:
    case InteractionHandleKind::None:
        result.valid = false;
        break;
    }

    return result;
}

bool
TransformDragger::commitTransformEdit(TransformEditState & edit)
{
    if (!edit.active) {
        return false;
    }
    edit.startTransform = edit.currentTransform;
    edit.active = false;
    return true;
}

bool
TransformDragger::cancelTransformEdit(Scene & scene,
                                      TransformEditState & edit,
                                      ManipulatorAttachment * attachment)
{
    if (!edit.active || edit.target == InvalidSceneObjectId) {
        return false;
    }

    const bool restored =
        scene.setObjectTransform(edit.target, edit.startTransform);
    if (!restored) {
        return false;
    }

    edit.currentTransform = edit.startTransform;
    const bool synced = syncMatchingAttachment(scene, edit.target, attachment);
    edit.active = false;
    return synced;
}

} // namespace obol
