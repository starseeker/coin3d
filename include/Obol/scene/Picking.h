#ifndef OBOL_SCENE_PICKING_H
#define OBOL_SCENE_PICKING_H

/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
\**************************************************************************/

#include <Inventor/SbBasic.h>

#include <Obol/cad/CadIds.h>
#include <Obol/scene/Scene.h>

#include <optional>
#include <vector>

namespace obol {

struct PickRequest {
    unsigned int viewportWidth = 1;
    unsigned int viewportHeight = 1;
    int x = 0;
    int y = 0;
    float radiusPixels = 1.0f;
    bool allHits = false;
    bool useWorldRay = false;
    Vec3 rayOrigin;
    Vec3 rayDirection = {0.0f, 0.0f, -1.0f};
    float nearDistance = -1.0f;
    float farDistance = -1.0f;
};

enum class CadPickPrimitive {
    Edge,
    Triangle,
    Bounds
};

struct CadPickDetail {
    InstanceId instanceId;
    PartId partId;
    CadPickPrimitive primitive = CadPickPrimitive::Bounds;
    uint32_t primitiveIndex0 = 0;
    uint32_t primitiveIndex1 = 0;
    float u = 0.0f;
};

struct PickHit {
    SceneObjectId objectId = InvalidSceneObjectId;
    Vec3 point;
    Vec3 normal;
    int materialIndex = -1;
    bool onGeometry = false;
    std::optional<CadPickDetail> cad;
};

struct PickResult {
    bool hit = false;
    std::vector<PickHit> hits;
};

class OBOL_DLL_API Picker {
public:
    static PickResult pick(const Scene & scene, const PickRequest & request);
};

} // namespace obol

#endif // OBOL_SCENE_PICKING_H
