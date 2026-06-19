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

void cameraChangedCB(const obol::ValueChange<obol::Vec3> & change)
{
    ++callbackCount;
    printf("Callback %d: Camera position: (%g, %g, %g)\n",
           callbackCount,
           change.value.x,
           change.value.y,
           change.value.z);
}

obol::PerspectiveCamera viewAllCamera(const obol::Scene & scene,
                                      const obol::Vec3 & requestedPosition)
{
    obol::ViewAllRequest request;
    request.viewportWidth = DEFAULT_WIDTH;
    request.viewportHeight = DEFAULT_HEIGHT;
    request.position = requestedPosition;
    return obol::CameraFraming::viewAllPerspective(scene, request);
}

void setCameraPosition(obol::Scene & scene,
                       obol::ObservableValue<obol::Vec3> & cameraPosition,
                       const obol::Vec3 & position)
{
    cameraPosition.set(position, "position");
    const obol::PerspectiveCamera camera = viewAllCamera(scene, position);
    scene.setCamera(camera);
}

} // namespace

int main(int argc, char **argv)
{
    initCoinHeadless();

    obol::Scene scene;
    scene.addDirectionalLight(obol::DirectionalLight{});
    scene.addPrimitive(obol::Primitive::Cube);
    obol::ObservableValue<obol::Vec3> cameraPosition({0.0f, 0.0f, 5.0f});
    cameraPosition.addObserver(cameraChangedCB);
    const obol::PerspectiveCamera camera =
        viewAllCamera(scene, cameraPosition.get());
    scene.setCamera(camera);

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
    setCameraPosition(scene, cameraPosition, {2.0f, 3.0f, 10.0f});
    snprintf(filename, sizeof(filename), "%s_pos1.rgb", baseFilename);
    if (!renderScene(renderer, scene, filename)) return 1;

    printf("\nChanging camera position 2...\n");
    setCameraPosition(scene, cameraPosition, {-3.0f, 2.0f, 8.0f});
    snprintf(filename, sizeof(filename), "%s_pos2.rgb", baseFilename);
    if (!renderScene(renderer, scene, filename)) return 1;

    printf("\nChanging camera position 3...\n");
    setCameraPosition(scene, cameraPosition, {0.0f, -4.0f, 6.0f});
    snprintf(filename, sizeof(filename), "%s_pos3.rgb", baseFilename);
    if (!renderScene(renderer, scene, filename)) return 1;

    printf("\nTotal callbacks received: %d\n", callbackCount);
    return 0;
}
