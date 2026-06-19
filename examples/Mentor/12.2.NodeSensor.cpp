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
 * Headless version of Inventor Mentor example 12.2
 *
 * Original: NodeSensor - monitors node changes using getTriggerNode/Field
 * Headless: app-owned scene-change callback over v2 object state
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cstdio>

namespace {

struct ObjectState {
    obol::SceneObjectId id = obol::InvalidSceneObjectId;
    obol::Transform transform;
    const char *name = "";
};

bool renderScene(obol::OffscreenRenderer & renderer,
                 obol::Scene & scene,
                 const char * filename)
{
    const obol::FrameResult result = renderer.render(scene);
    return result.success && renderer.writeRGB(filename);
}

void rootChangedCB(const char * nodeName, const char * fieldName)
{
    printf("The node named '%s' changed", nodeName);
    if (fieldName) {
        printf(" (field %s)\n", fieldName);
    } else {
        printf(" (no fields changed)\n");
    }
}

obol::PerspectiveCamera viewAllCamera(const obol::Scene & scene)
{
    obol::ViewAllRequest request;
    request.viewportWidth = DEFAULT_WIDTH;
    request.viewportHeight = DEFAULT_HEIGHT;
    return obol::CameraFraming::viewAllPerspective(scene, request);
}

} // namespace

int main(int argc, char **argv)
{
    initCoinHeadless();

    obol::Scene scene;
    scene.addDirectionalLight(obol::DirectionalLight{});

    ObjectState cube;
    cube.name = "MyCube";
    cube.id = scene.addPrimitive(obol::Primitive::Cube,
                                 obol::Material{},
                                 cube.transform);

    ObjectState sphere;
    sphere.name = "MySphere";
    sphere.id = scene.addPrimitive(obol::Primitive::Sphere,
                                   obol::Material{},
                                   sphere.transform);
    scene.setCamera(viewAllCamera(scene));

    obol::ObservableValue<obol::Transform> cubeTransform(cube.transform);
    cubeTransform.addObserver(
        [&](const obol::ValueChange<obol::Transform> & change) {
            cube.transform = change.value;
            scene.setObjectTransform(cube.id, cube.transform);
            rootChangedCB(cube.name,
                          change.fieldName.empty()
                              ? nullptr
                              : change.fieldName.c_str());
        });

    obol::ObservableValue<obol::Transform> sphereTransform(sphere.transform);
    sphereTransform.addObserver(
        [&](const obol::ValueChange<obol::Transform> & change) {
            sphere.transform = change.value;
            scene.setObjectTransform(sphere.id, sphere.transform);
            rootChangedCB(sphere.name,
                          change.fieldName.empty()
                              ? nullptr
                              : change.fieldName.c_str());
        });

    obol::ObservableValue<bool> spherePresent(true);
    spherePresent.addObserver([&](const obol::ValueChange<bool> & change) {
        if (!change.value && sphere.id != obol::InvalidSceneObjectId) {
            scene.removeObject(sphere.id);
            sphere.id = obol::InvalidSceneObjectId;
        }
        rootChangedCB("Root", nullptr);
    });

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::OffscreenRenderer renderer(backend, target);
    renderer.setBackgroundColor({0.0f, 0.0f, 0.0f, 1.0f});

    const char *baseFilename = (argc > 1) ? argv[1] : "12.2.NodeSensor";
    char filename[256];

    printf("\n=== Initial state ===\n");
    snprintf(filename, sizeof(filename), "%s_initial.rgb", baseFilename);
    if (!renderScene(renderer, scene, filename)) return 1;

    printf("\n=== Changing cube width ===\n");
    cube.transform.scale.x = 1.5f;
    cubeTransform.set(cube.transform, "width");
    snprintf(filename, sizeof(filename), "%s_cube_width.rgb", baseFilename);
    if (!renderScene(renderer, scene, filename)) return 1;

    printf("\n=== Changing cube height ===\n");
    cube.transform.scale.y = 2.0f;
    cubeTransform.set(cube.transform, "height");
    snprintf(filename, sizeof(filename), "%s_cube_height.rgb", baseFilename);
    if (!renderScene(renderer, scene, filename)) return 1;

    printf("\n=== Changing sphere radius ===\n");
    sphere.transform.scale = {2.0f, 2.0f, 2.0f};
    sphereTransform.set(sphere.transform, "radius");
    snprintf(filename, sizeof(filename), "%s_sphere_radius.rgb", baseFilename);
    if (!renderScene(renderer, scene, filename)) return 1;

    printf("\n=== Removing sphere ===\n");
    spherePresent.set(false);
    snprintf(filename, sizeof(filename), "%s_removed_sphere.rgb", baseFilename);
    if (!renderScene(renderer, scene, filename)) return 1;

    return 0;
}
