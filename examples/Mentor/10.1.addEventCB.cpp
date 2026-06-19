/*
 *
 *  Copyright (C) 2000 Silicon Graphics, Inc.  All Rights Reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  Further, this software is distributed without any warranty that it is
 *  free of the rightful claim of any third person regarding infringement
 *  or the like.  Any license provided herein, whether implied or
 *  otherwise, applies only to this software file.  Patent licenses, if
 *  any, provided herein do not apply to combinations of this program with
 *  other software, or any other product whatsoever.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 *  Mountain View, CA  94043, or:
 *
 *  http://www.sgi.com
 *
 *  For further information regarding this notice, see:
 *
 *  http://oss.sgi.com/projects/GenInfo/NoticeExplan/
 *
 */

/*
 * Headless version of Inventor Mentor example 10.1
 * 
 * Original: addEventCB - demonstrates keyboard event callbacks for interactive scaling
 * Headless: Simulates keyboard events to scale objects up and down
 * 
 * This example demonstrates the event simulation pattern developed for manipulators:
 * - Uses simulateKeyPress/Release from headless_utils.h
 * - Proper event callback registration and handling
 * - Events trigger callbacks just like in interactive mode
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cstdio>
#include <vector>

namespace {

enum class Key {
    Up,
    Down
};

struct ObjectState {
    obol::SceneObjectId id = obol::InvalidSceneObjectId;
    obol::Transform transform;
};

obol::Material material(float r, float g, float b)
{
    obol::Material result;
    result.baseColor = {r, g, b, 1.0f};
    return result;
}

obol::Material selectedMaterial(float r, float g, float b)
{
    obol::Material result = material(r, g, b);
    result.emissive = {0.12f, 0.12f, 0.05f, 1.0f};
    return result;
}

obol::Transform transform(float x, float y, float z)
{
    obol::Transform result;
    result.translation = {x, y, z};
    return result;
}

bool renderScene(obol::Renderer & renderer,
                 obol::Scene & scene,
                 const obol::RenderTarget & target,
                 const char * filename)
{
    obol::FrameRequest request;
    request.scene = &scene;
    request.target = target;
    request.background = {0.0f, 0.0f, 0.0f, 1.0f};
    const obol::FrameResult result = renderer.render(request);
    return result.success && renderer.writeRGB(filename);
}

void scaleObject(obol::Scene & scene, ObjectState & object, float factor)
{
    object.transform.scale.x *= factor;
    object.transform.scale.y *= factor;
    object.transform.scale.z *= factor;
    scene.setObjectTransform(object.id, object.transform);
}

void handleKeyPress(obol::Scene & scene,
                    std::vector<ObjectState *> & selected,
                    Key key)
{
    const float factor = key == Key::Up ? 1.1f : (1.0f / 1.1f);
    printf("%s detected - scaling selected objects\n",
           key == Key::Up ? "UP_ARROW" : "DOWN_ARROW");
    for (ObjectState * object : selected) {
        if (object) {
            scaleObject(scene, *object, factor);
        }
    }
}

} // namespace

int main(int argc, char **argv)
{
    initCoinHeadless();

    obol::Scene scene;
    obol::PerspectiveCamera camera;
    camera.position = {0.0f, 0.0f, 10.0f};
    camera.target = {0.0f, 0.0f, 0.0f};
    camera.verticalFieldOfViewRadians = 0.72f;
    scene.setCamera(camera);
    scene.addDirectionalLight(obol::DirectionalLight{});

    ObjectState cube{obol::InvalidSceneObjectId, transform(-2.0f, 2.0f, 0.0f)};
    cube.id = scene.addPrimitive(obol::Primitive::Cube,
                                 material(0.8f, 0.0f, 0.0f),
                                 cube.transform);

    ObjectState sphere{obol::InvalidSceneObjectId, transform(2.0f, 2.0f, 0.0f)};
    sphere.id = scene.addPrimitive(obol::Primitive::Sphere,
                                   material(0.0f, 0.0f, 0.8f),
                                   sphere.transform);

    ObjectState cone{obol::InvalidSceneObjectId, transform(2.0f, -2.0f, 0.0f)};
    cone.id = scene.addPrimitive(obol::Primitive::Cone,
                                 material(0.0f, 0.8f, 0.0f),
                                 cone.transform);

    ObjectState cylinder{obol::InvalidSceneObjectId, transform(-2.0f, -2.0f, 0.0f)};
    cylinder.id = scene.addPrimitive(obol::Primitive::Cylinder,
                                     material(0.8f, 0.0f, 0.8f),
                                     cylinder.transform);

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::Renderer renderer(backend);

    const char *baseFilename = (argc > 1) ? argv[1] : "10.1.addEventCB";
    char filename[256];

    int frameNum = 0;

    printf("\n=== Initial state (nothing selected) ===\n");
    snprintf(filename, sizeof(filename), "%s_frame%02d_initial.rgb", baseFilename, frameNum++);
    if (!renderScene(renderer, scene, target, filename)) {
        fprintf(stderr, "Error: Failed to render initial event callback scene with Obol v2 API\n");
        return 1;
    }

    std::vector<ObjectState *> selected;
    selected.push_back(&cube);
    selected.push_back(&sphere);
    scene.setObjectMaterial(cube.id, selectedMaterial(0.8f, 0.0f, 0.0f));
    scene.setObjectMaterial(sphere.id, selectedMaterial(0.0f, 0.0f, 0.8f));
    printf("Selected cube\n");
    printf("Selected sphere\n");

    printf("\n=== Cube and sphere selected ===\n");
    snprintf(filename, sizeof(filename), "%s_frame%02d_selected.rgb", baseFilename, frameNum++);
    if (!renderScene(renderer, scene, target, filename)) return 1;

    printf("\n=== Simulating UP ARROW key presses (scale up) ===\n");
    printf("This demonstrates application event dispatch triggering callbacks\n");
    for (int i = 0; i < 3; i++) {
        handleKeyPress(scene, selected, Key::Up);
        printf("Scale after UP %d: (%.3f, %.3f, %.3f)\n",
               i + 1, cube.transform.scale.x, cube.transform.scale.y, cube.transform.scale.z);

        snprintf(filename, sizeof(filename), "%s_frame%02d_scaleup_%d.rgb", baseFilename, frameNum++, i+1);
        if (!renderScene(renderer, scene, target, filename)) return 1;
    }

    printf("\n=== Simulating DOWN ARROW key presses (scale down) ===\n");
    for (int i = 0; i < 5; i++) {
        handleKeyPress(scene, selected, Key::Down);
        printf("Scale after DOWN %d: (%.3f, %.3f, %.3f)\n",
               i + 1, cube.transform.scale.x, cube.transform.scale.y, cube.transform.scale.z);

        snprintf(filename, sizeof(filename), "%s_frame%02d_scaledown_%d.rgb", baseFilename, frameNum++, i+1);
        if (!renderScene(renderer, scene, target, filename)) return 1;
    }

    printf("\nRendered %d frames demonstrating application event callbacks [Obol v2]\n", frameNum);
    printf("Events are application-owned and update v2 scene object transforms by stable ID\n");
    return 0;
}
