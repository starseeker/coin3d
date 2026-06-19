#ifndef OBOL_SCENE_CAMERA_H
#define OBOL_SCENE_CAMERA_H

/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
\**************************************************************************/

#include <Obol/base/Export.h>
#include <Obol/scene/Scene.h>

namespace obol {

struct ViewAllRequest {
    unsigned int viewportWidth = 640;
    unsigned int viewportHeight = 480;
    Vec3 position = {0.0f, 0.0f, 5.0f};
    Vec3 target = {0.0f, 0.0f, 0.0f};
    Vec3 up = {0.0f, 1.0f, 0.0f};
    float slack = 1.0f;
};

struct CameraOrbitRequest {
    PerspectiveCamera camera;
    Vec3 center = {0.0f, 0.0f, 0.0f};
    Vec3 worldUp = {0.0f, 1.0f, 0.0f};
    float azimuthRadians = 0.0f;
    float elevationRadians = 0.0f;
};

class OBOL_V2_API CameraFraming {
public:
    static PerspectiveCamera viewAllPerspective(const Scene & scene,
                                                const ViewAllRequest & request);
    static PerspectiveCamera orbit(const CameraOrbitRequest & request);
};

} // namespace obol

#endif // OBOL_SCENE_CAMERA_H
