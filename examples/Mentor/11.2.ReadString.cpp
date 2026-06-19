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
 * Headless version of Inventor Mentor example 11.2
 *
 * Original: ReadString - reads scene from string buffer
 * Headless: parses scene through Obol v2 SceneIO and renders to image
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cstdio>
#include <string>

namespace {

const char *sceneBuffer =
    "#Inventor V2.0 ascii\n"
    "\n"
    "Separator {\n"
    "   Material {\n"
    "      diffuseColor [ 1 0 0, 0 1 0, 0 0 1 ]\n"
    "   }\n"
    "   MaterialBinding { value PER_PART }\n"
    "   Cone {}\n"
    "}\n";

bool renderScene(obol::OffscreenRenderer & renderer,
                 obol::Scene & scene,
                 const char * filename)
{
    const obol::FrameResult result = renderer.render(scene);
    return result.success && renderer.writeRGB(filename);
}

void applyViewAllCamera(obol::Scene & scene)
{
    obol::ViewAllRequest request;
    request.viewportWidth = DEFAULT_WIDTH;
    request.viewportHeight = DEFAULT_HEIGHT;
    scene.setCamera(obol::CameraFraming::viewAllPerspective(scene, request));
}

} // namespace

int main(int argc, char **argv)
{
    initCoinHeadless();

    obol::Scene scene;
    if (!obol::SceneIO::readInventorString(std::string(sceneBuffer),
                                           scene,
                                           getCoinHeadlessContextManager())) {
        fprintf(stderr, "Failed to parse scene through Obol v2 SceneIO\n");
        return 1;
    }

    applyViewAllCamera(scene);
    scene.addDirectionalLight(obol::DirectionalLight{});

    printf("Successfully parsed scene from string buffer through Obol v2 SceneIO\n");

    const char *baseFilename = (argc > 1) ? argv[1] : "11.2.ReadString";
    char filename[256];
    snprintf(filename, sizeof(filename), "%s.rgb", baseFilename);

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::OffscreenRenderer renderer(backend, target);
    renderer.setBackgroundColor({0.0f, 0.0f, 0.0f, 1.0f});

    if (!renderScene(renderer, scene, filename)) {
        fprintf(stderr, "Error: Failed to render string scene with Obol v2 API\n");
        return 1;
    }

    return 0;
}
