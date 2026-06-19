#ifndef OBOL_SCENE_INTERACTION_H
#define OBOL_SCENE_INTERACTION_H

/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
\**************************************************************************/

#include <Obol/base/Export.h>
#include <Obol/scene/Scene.h>

#include <cstdint>
#include <vector>

namespace obol {

struct AxisDragRequest {
    SceneObjectId target = InvalidSceneObjectId;
    Transform startTransform;
    Vec3 axis = {1.0f, 0.0f, 0.0f};
    float distance = 0.0f;
    bool snap = false;
    float snapStep = 0.0f;
    bool clamp = false;
    float minimumDistance = 0.0f;
    float maximumDistance = 0.0f;
};

struct AxisDragResult {
    bool valid = false;
    SceneObjectId target = InvalidSceneObjectId;
    Transform transform;
    Vec3 delta;
    float distance = 0.0f;
};

struct TranslationBounds {
    bool enabled = false;
    Vec3 minimum = {-1.0f, -1.0f, -1.0f};
    Vec3 maximum = {1.0f, 1.0f, 1.0f};
};

struct FreeDragRequest {
    SceneObjectId target = InvalidSceneObjectId;
    Transform startTransform;
    Vec3 delta;
    bool snap = false;
    Vec3 snapStep = {0.0f, 0.0f, 0.0f};
    TranslationBounds bounds;
};

struct PlaneDragRequest {
    SceneObjectId target = InvalidSceneObjectId;
    Transform startTransform;
    Vec3 delta;
    Vec3 planeNormal = {0.0f, 0.0f, 1.0f};
    bool snap = false;
    Vec3 snapStep = {0.0f, 0.0f, 0.0f};
    TranslationBounds bounds;
};

struct TranslationResult {
    bool valid = false;
    SceneObjectId target = InvalidSceneObjectId;
    Transform transform;
    Vec3 delta;
};

struct AxisRotationRequest {
    SceneObjectId target = InvalidSceneObjectId;
    Transform startTransform;
    Vec3 axis = {0.0f, 0.0f, 1.0f};
    float angleRadians = 0.0f;
    bool snap = false;
    float snapStepRadians = 0.0f;
    bool clamp = false;
    float minimumAngleRadians = 0.0f;
    float maximumAngleRadians = 0.0f;
};

struct AxisRotationResult {
    bool valid = false;
    SceneObjectId target = InvalidSceneObjectId;
    Transform transform;
    Vec3 axis = {0.0f, 0.0f, 1.0f};
    float angleRadians = 0.0f;
};

struct TrackballRotationRequest {
    SceneObjectId target = InvalidSceneObjectId;
    Transform startTransform;
    Vec3 from = {0.0f, 0.0f, 1.0f};
    Vec3 to = {0.0f, 0.0f, 1.0f};
    Vec3 fallbackAxis = {0.0f, 0.0f, 1.0f};
    bool snap = false;
    float snapStepRadians = 0.0f;
    bool clamp = false;
    float minimumAngleRadians = 0.0f;
    float maximumAngleRadians = 0.0f;
};

struct ScaleBounds {
    bool enabled = false;
    Vec3 minimum = {0.001f, 0.001f, 0.001f};
    Vec3 maximum = {1000.0f, 1000.0f, 1000.0f};
};

struct ScaleRequest {
    SceneObjectId target = InvalidSceneObjectId;
    Transform startTransform;
    Vec3 factors = {1.0f, 1.0f, 1.0f};
    bool snap = false;
    Vec3 snapStep = {0.0f, 0.0f, 0.0f};
    ScaleBounds bounds;
};

struct ScaleResult {
    bool valid = false;
    SceneObjectId target = InvalidSceneObjectId;
    Transform transform;
    Vec3 scale = {1.0f, 1.0f, 1.0f};
};

struct TranslateAxisOverlay {
    Transform transform;
    Vec3 axis = {1.0f, 0.0f, 0.0f};
    float length = 2.0f;
    float handleSize = 0.25f;
    float lineWidth = 2.0f;
    Material material;
    SceneGroupId parent = RootSceneGroupId;
};

struct TrackballOverlay {
    Transform transform;
    float radius = 1.0f;
    float lineWidth = 2.0f;
    uint32_t segments = 48;
    Material xMaterial;
    Material yMaterial;
    Material zMaterial;
    SceneGroupId parent = RootSceneGroupId;
};

struct BoxOverlay {
    Transform transform;
    Vec3 halfSize = {1.0f, 1.0f, 1.0f};
    float lineWidth = 2.0f;
    Material material;
    SceneGroupId parent = RootSceneGroupId;
};

enum class ManipulatorOverlayKind {
    HandleBox,
    Trackball,
    TransformBox
};

enum class InteractionHandleKind {
    None,
    TranslateAxis,
    RotateAxis,
    ScaleUniform,
    BoundsBox
};

struct ManipulatorOverlay {
    SceneObjectId target = InvalidSceneObjectId;
    ManipulatorOverlayKind kind = ManipulatorOverlayKind::HandleBox;
    Transform transform;
    Vec3 boxHalfSize = {1.0f, 1.0f, 1.0f};
    float trackballRadius = 1.0f;
    float lineWidth = 2.0f;
    uint32_t segments = 48;
    Material material;
    SceneGroupId parent = RootSceneGroupId;
};

struct InteractionHandle {
    SceneObjectId object = InvalidSceneObjectId;
    InteractionHandleKind kind = InteractionHandleKind::None;
    Vec3 axis;
};

struct InteractionOverlay {
    SceneObjectId target = InvalidSceneObjectId;
    SceneGroupId group = InvalidSceneGroupId;
    std::vector<SceneObjectId> objects;
    std::vector<InteractionHandle> handles;
};

struct InteractionHandlePick {
    bool valid = false;
    SceneObjectId object = InvalidSceneObjectId;
    InteractionHandleKind kind = InteractionHandleKind::None;
    Vec3 axis;
};

struct InteractionHandleEditRequest {
    InteractionHandlePick handle;
    AxisDragRequest translation;
    AxisRotationRequest rotation;
    ScaleRequest scale;
};

struct InteractionHandleEditResult {
    bool applied = false;
    InteractionHandleKind kind = InteractionHandleKind::None;
    AxisDragResult translation;
    AxisRotationResult rotation;
    ScaleResult scale;
};

struct ManipulatorAttachment {
    bool attached = false;
    bool visible = false;
    SceneObjectId target = InvalidSceneObjectId;
    ManipulatorOverlayKind kind = ManipulatorOverlayKind::HandleBox;
    InteractionOverlay overlay;
};

struct TransformEditState {
    bool active = false;
    SceneObjectId target = InvalidSceneObjectId;
    Transform startTransform;
    Transform currentTransform;
};

struct ManipulatorEditRequest {
    AxisDragRequest translation;
    AxisRotationRequest rotation;
    ScaleRequest scale;
};

struct ManipulatorEditSession {
    bool active = false;
    InteractionHandlePick handle;
    TransformEditState transformEdit;
};

struct PointerDragGesture {
    Vec2 delta;
    Vec2 handleScreenDirection = {1.0f, 0.0f};
    float translationUnitsPerPixel = 0.01f;
    float rotationRadiansPerPixel = 0.01f;
    float scaleFactorPerPixel = 0.01f;
    AxisDragRequest translation;
    AxisRotationRequest rotation;
    ScaleRequest scale;
};

struct PointerDragGestureResult {
    bool valid = false;
    InteractionHandleKind kind = InteractionHandleKind::None;
    float projectedDelta = 0.0f;
    ManipulatorEditRequest edit;
};

class OBOL_V2_API TransformDragger {
public:
    static AxisDragResult translateOnAxis(const AxisDragRequest & request);
    static bool applyAxisTranslation(Scene & scene,
                                     const AxisDragRequest & request,
                                     AxisDragResult * result = nullptr);
    static TranslationResult translateFreely(const FreeDragRequest & request);
    static bool applyFreeTranslation(Scene & scene,
                                     const FreeDragRequest & request,
                                     TranslationResult * result = nullptr);
    static TranslationResult translateOnPlane(const PlaneDragRequest & request);
    static bool applyPlaneTranslation(Scene & scene,
                                      const PlaneDragRequest & request,
                                      TranslationResult * result = nullptr);
    static AxisRotationResult rotateOnAxis(const AxisRotationRequest & request);
    static bool applyAxisRotation(Scene & scene,
                                  const AxisRotationRequest & request,
                                  AxisRotationResult * result = nullptr);
    static AxisRotationResult rotateTrackball(
        const TrackballRotationRequest & request);
    static bool applyTrackballRotation(
        Scene & scene,
        const TrackballRotationRequest & request,
        AxisRotationResult * result = nullptr);
    static ScaleResult scaleByFactors(const ScaleRequest & request);
    static bool applyScale(Scene & scene,
                           const ScaleRequest & request,
                           ScaleResult * result = nullptr);
    static InteractionOverlay addTranslateAxisOverlay(
        Scene & scene,
        const TranslateAxisOverlay & overlay);
    static InteractionOverlay addTrackballOverlay(
        Scene & scene,
        const TrackballOverlay & overlay);
    static InteractionOverlay addBoxOverlay(Scene & scene,
                                            const BoxOverlay & overlay);
    static InteractionOverlay addManipulatorOverlay(
        Scene & scene,
        const ManipulatorOverlay & overlay);
    static ManipulatorAttachment attachManipulator(
        Scene & scene,
        const ManipulatorOverlay & overlay);
    static InteractionHandlePick resolveOverlayHandle(
        const InteractionOverlay & overlay,
        SceneObjectId object);
    static InteractionHandlePick resolveManipulatorHandle(
        const ManipulatorAttachment & attachment,
        SceneObjectId object);
    static bool syncOverlayToTarget(Scene & scene,
                                    const InteractionOverlay & overlay);
    static bool syncManipulator(Scene & scene,
                                const ManipulatorAttachment & attachment);
    static bool setOverlayVisible(Scene & scene,
                                  const InteractionOverlay & overlay,
                                  bool visible);
    static bool setManipulatorVisible(Scene & scene,
                                      ManipulatorAttachment & attachment,
                                      bool visible);
    static bool removeOverlay(Scene & scene,
                              InteractionOverlay & overlay);
    static bool detachManipulator(Scene & scene,
                                  ManipulatorAttachment & attachment);
    static TransformEditState beginTransformEdit(const Scene & scene,
                                                 SceneObjectId target);
    static bool updateEditAxisTranslation(
        Scene & scene,
        TransformEditState & edit,
        const AxisDragRequest & request,
        ManipulatorAttachment * attachment = nullptr,
        AxisDragResult * result = nullptr);
    static bool updateEditFreeTranslation(
        Scene & scene,
        TransformEditState & edit,
        const FreeDragRequest & request,
        ManipulatorAttachment * attachment = nullptr,
        TranslationResult * result = nullptr);
    static bool updateEditPlaneTranslation(
        Scene & scene,
        TransformEditState & edit,
        const PlaneDragRequest & request,
        ManipulatorAttachment * attachment = nullptr,
        TranslationResult * result = nullptr);
    static bool updateEditAxisRotation(
        Scene & scene,
        TransformEditState & edit,
        const AxisRotationRequest & request,
        ManipulatorAttachment * attachment = nullptr,
        AxisRotationResult * result = nullptr);
    static bool updateEditTrackballRotation(
        Scene & scene,
        TransformEditState & edit,
        const TrackballRotationRequest & request,
        ManipulatorAttachment * attachment = nullptr,
        AxisRotationResult * result = nullptr);
    static bool updateEditScale(Scene & scene,
                                TransformEditState & edit,
                                const ScaleRequest & request,
                                ManipulatorAttachment * attachment = nullptr,
                                ScaleResult * result = nullptr);
    static bool updateEditFromHandle(
        Scene & scene,
        TransformEditState & edit,
        const InteractionHandleEditRequest & request,
        ManipulatorAttachment * attachment = nullptr,
        InteractionHandleEditResult * result = nullptr);
    static ManipulatorEditSession beginManipulatorEdit(
        const Scene & scene,
        const ManipulatorAttachment & attachment,
        SceneObjectId pickedObject);
    static bool updateManipulatorEdit(
        Scene & scene,
        ManipulatorEditSession & session,
        const ManipulatorEditRequest & request,
        ManipulatorAttachment & attachment,
        InteractionHandleEditResult * result = nullptr);
    static bool commitManipulatorEdit(ManipulatorEditSession & session);
    static bool cancelManipulatorEdit(
        Scene & scene,
        ManipulatorEditSession & session,
        ManipulatorAttachment * attachment = nullptr);
    static PointerDragGestureResult mapPointerDragToManipulatorEdit(
        const ManipulatorEditSession & session,
        const PointerDragGesture & gesture);
    static bool commitTransformEdit(TransformEditState & edit);
    static bool cancelTransformEdit(Scene & scene,
                                    TransformEditState & edit,
                                    ManipulatorAttachment * attachment = nullptr);
};

} // namespace obol

#endif // OBOL_SCENE_INTERACTION_H
