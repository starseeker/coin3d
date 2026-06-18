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
 * Headless version of Inventor Mentor example 12.1
 *
 * Original: FieldSensor - monitors camera position changes
 * Headless: app-owned camera-change callback over Obol v2 camera state
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cstdio>

namespace {

int callbackCount = 0;

bool renderScene(obol::OffscreenRenderer & renderer,
                 obol::Scene & scene,
                 const char * filename)
{
    const obol::FrameResult result = renderer.render(scene);
    return result.success && renderer.writeRGB(filename);
}

void cameraChangedCB(const obol::PerspectiveCamera & camera)
{
    ++callbackCount;
    printf("Callback %d: Camera position: (%g, %g, %g)\n",
           callbackCount,
           camera.position.x,
           camera.position.y,
           camera.position.z);
}

void setCameraPosition(obol::Scene & scene,
                       obol::PerspectiveCamera & camera,
                       const obol::Vec3 & position)
{
    camera.position = position;
    camera.target = {0.0f, 0.0f, 0.0f};
    scene.setCamera(camera);
    cameraChangedCB(camera);
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
    scene.addPrimitive(obol::Primitive::Cube);

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::OffscreenRenderer renderer(backend, target);
    renderer.setBackgroundColor({0.0f, 0.0f, 0.0f, 1.0f});

    const char *baseFilename = (argc > 1) ? argv[1] : "12.1.FieldSensor";
    char filename[256];

    snprintf(filename, sizeof(filename), "%s_initial.rgb", baseFilename);
    printf("\nRendering initial state...\n");
    if (!renderScene(renderer, scene, filename)) return 1;

    printf("\nChanging camera position 1...\n");
    setCameraPosition(scene, camera, {2.0f, 3.0f, 10.0f});
    snprintf(filename, sizeof(filename), "%s_pos1.rgb", baseFilename);
    if (!renderScene(renderer, scene, filename)) return 1;

    printf("\nChanging camera position 2...\n");
    setCameraPosition(scene, camera, {-3.0f, 2.0f, 8.0f});
    snprintf(filename, sizeof(filename), "%s_pos2.rgb", baseFilename);
    if (!renderScene(renderer, scene, filename)) return 1;

    printf("\nChanging camera position 3...\n");
    setCameraPosition(scene, camera, {0.0f, -4.0f, 6.0f});
    snprintf(filename, sizeof(filename), "%s_pos3.rgb", baseFilename);
    if (!renderScene(renderer, scene, filename)) return 1;

    printf("\nTotal callbacks received: %d\n", callbackCount);
    return 0;
}
