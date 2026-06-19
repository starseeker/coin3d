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
 * Headless version of Inventor Mentor example 11.1
 *
 * Original: ReadFile - reads .iv file and displays
 * Headless: reads .iv file through Obol v2 SceneIO and renders to image
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cstdio>
#include <cstdlib>

namespace {

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

void addViewState(obol::Scene & scene)
{
    applyViewAllCamera(scene);
    scene.addDirectionalLight(obol::DirectionalLight{});
}

} // namespace

int main(int argc, char **argv)
{
    initCoinHeadless();

    obol::Scene scene;
    bool readScene = false;
    char filepath[512] = {0};
    const char *envDataDir = getenv("OBOL_DATA_DIR");
    if (!envDataDir) envDataDir = getenv("IVEXAMPLES_DATA_DIR");
    const char *candidateDataDirs[] = {
        envDataDir,
        "examples/Mentor/data",
        "../../data"
    };
    for (const char *dataDir : candidateDataDirs) {
        if (!dataDir) continue;
        snprintf(filepath, sizeof(filepath), "%s/star.iv", dataDir);
        if (obol::SceneIO::readInventorFile(filepath,
                                            scene,
                                            getCoinHeadlessContextManager())) {
            readScene = true;
            break;
        }
    }

    if (readScene) {
        printf("Successfully read scene from %s through Obol v2 SceneIO\n", filepath);
    } else {
        printf("Could not read file, creating simple fallback scene\n");
        obol::Material material;
        material.baseColor = {1.0f, 0.8f, 0.2f, 1.0f};
        scene.addPrimitive(obol::Primitive::Cone, material);
    }
    addViewState(scene);

    const char *baseFilename = (argc > 1) ? argv[1] : "11.1.ReadFile";
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
        fprintf(stderr, "Error: Failed to render file scene with Obol v2 API\n");
        return 1;
    }

    return 0;
}
