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
 * Headless version of Inventor Mentor example 12.3
 *
 * Original: AlarmSensor - raises a flag after 12 seconds
 * Headless: app-owned alarm callback updates a v2 object transform
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cstdio>

namespace {

constexpr float kHalfPi = 1.57079632679f;
bool flagRaised = false;

bool renderScene(obol::OffscreenRenderer & renderer,
                 obol::Scene & scene,
                 const char * filename)
{
    const obol::FrameResult result = renderer.render(scene);
    return result.success && renderer.writeRGB(filename);
}

void raiseFlagCallback(obol::Scene & scene,
                       obol::SceneObjectId flag,
                       obol::Transform & transform)
{
    transform.rotationAxis = {0.0f, 0.0f, 1.0f};
    transform.rotationRadians = kHalfPi;
    scene.setObjectTransform(flag, transform);
    flagRaised = true;
    printf("Alarm triggered! Flag raised.\n");
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

    obol::Material flagMaterial;
    flagMaterial.baseColor = {1.0f, 0.2f, 0.0f, 1.0f};
    obol::PrimitiveOptions flagOptions;
    flagOptions.radius = 0.8f;
    flagOptions.height = 2.0f;
    obol::Transform flagTransform;
    const obol::SceneObjectId flag =
        scene.addPrimitive(obol::Primitive::Cone,
                           flagMaterial,
                           flagTransform,
                           flagOptions);

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::OffscreenRenderer renderer(backend, target);
    renderer.setBackgroundColor({0.0f, 0.0f, 0.0f, 1.0f});

    const char *baseFilename = (argc > 1) ? argv[1] : "12.3.AlarmSensor";
    char filename[256];

    printf("Before alarm triggers...\n");
    snprintf(filename, sizeof(filename), "%s_before.rgb", baseFilename);
    if (!renderScene(renderer, scene, filename)) return 1;

    printf("\nProcessing app alarm queue...\n");
    raiseFlagCallback(scene, flag, flagTransform);

    printf("\nAfter alarm triggers...\n");
    snprintf(filename, sizeof(filename), "%s_after.rgb", baseFilename);
    if (!renderScene(renderer, scene, filename)) return 1;

    printf("\nFlag raised: %s\n", flagRaised ? "Yes" : "No");
    return 0;
}
