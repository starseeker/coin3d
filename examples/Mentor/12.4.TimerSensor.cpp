/*
 *
 *  Copyright (C) 2000 Silicon Graphics, Inc.  All Rights Reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 */

/*
 * Headless version of Inventor Mentor example 12.4
 *
 * Original: TimerSensor - rotating object with timer-based scheduling
 * Headless: app-owned timer callbacks update v2 object transforms
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cstdio>

namespace {

constexpr float kPi = 3.14159265358979323846f;
int rotationCount = 0;

bool renderScene(obol::OffscreenRenderer & renderer,
                 obol::Scene & scene,
                 const char * filename)
{
    const obol::FrameResult result = renderer.render(scene);
    return result.success && renderer.writeRGB(filename);
}

void rotatingSensorCallback(obol::Scene & scene,
                            obol::SceneObjectId object,
                            obol::Transform & transform)
{
    transform.rotationAxis = {0.0f, 0.0f, 1.0f};
    transform.rotationRadians += kPi / 90.0f;
    scene.setObjectTransform(object, transform);
    ++rotationCount;
    printf("Rotation %d: angle = %.2f degrees\n",
           rotationCount,
           transform.rotationRadians * 180.0f / kPi);
}

void schedulingSensorCallback(float & intervalSeconds)
{
    if (intervalSeconds == 1.0f) {
        intervalSeconds = 0.1f;
        printf("\n*** Changed rotation interval to 0.1 seconds (10x per second) ***\n\n");
    } else {
        intervalSeconds = 1.0f;
        printf("\n*** Changed rotation interval to 1.0 second ***\n\n");
    }
}

} // namespace

int main(int argc, char **argv)
{
    initCoinHeadless();

    obol::Scene scene;
    obol::PerspectiveCamera camera;
    camera.position = {0.0f, 0.0f, 5.0f};
    camera.target = {0.0f, 0.0f, 0.0f};
    scene.setCamera(camera);
    scene.addDirectionalLight(obol::DirectionalLight{});

    obol::Transform coneTransform;
    const obol::SceneObjectId cone =
        scene.addPrimitive(obol::Primitive::Cone,
                           obol::Material{},
                           coneTransform);

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::OffscreenRenderer renderer(backend, target);
    renderer.setBackgroundColor({0.0f, 0.0f, 0.0f, 1.0f});

    float intervalSeconds = 1.0f;

    const char *baseFilename = (argc > 1) ? argv[1] : "12.4.TimerSensor";
    char filename[256];

    printf("Initial state\n");
    snprintf(filename, sizeof(filename), "%s_initial.rgb", baseFilename);
    if (!renderScene(renderer, scene, filename)) return 1;

    int frameCount = 0;
    for (int i = 0; i < 8; i++) {
        rotatingSensorCallback(scene, cone, coneTransform);

        snprintf(filename, sizeof(filename), "%s_frame%02d.rgb", baseFilename, ++frameCount);
        if (!renderScene(renderer, scene, filename)) return 1;

        if (i == 4) {
            printf("\n5 seconds elapsed, triggering scheduling sensor...\n");
            schedulingSensorCallback(intervalSeconds);
        }
    }

    printf("\nTotal rotations: %d\n", rotationCount);
    return 0;
}
