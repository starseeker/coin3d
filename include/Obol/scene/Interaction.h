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
    TranslationBounds bounds;
};

struct PlaneDragRequest {
    SceneObjectId target = InvalidSceneObjectId;
    Transform startTransform;
    Vec3 delta;
    Vec3 planeNormal = {0.0f, 0.0f, 1.0f};
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

struct InteractionOverlay {
    SceneObjectId target = InvalidSceneObjectId;
    SceneGroupId group = InvalidSceneGroupId;
    std::vector<SceneObjectId> objects;
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
};

} // namespace obol

#endif // OBOL_SCENE_INTERACTION_H
