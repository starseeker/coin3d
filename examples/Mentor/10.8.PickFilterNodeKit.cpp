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
 */

/*
 * Headless version of Inventor Mentor example 10.8
 *
 * Original: PickFilterNodeKit - Pick filter with material editor
 * Headless: application-owned pick filtering and material editing over v2 IDs
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cmath>
#include <cstdio>
#include <vector>

namespace {

constexpr float kPi = 3.14159265358979323846f;

struct ObjectState {
    obol::SceneObjectId id = obol::InvalidSceneObjectId;
    obol::Material material;
};

obol::Material material(float r, float g, float b)
{
    obol::Material result;
    result.baseColor = {r, g, b, 1.0f};
    result.specular = {0.5f, 0.5f, 0.5f, 1.0f};
    result.shininess = 0.4f;
    return result;
}

obol::Transform ringTransform(int index)
{
    obol::Transform transform;
    const float angle = static_cast<float>(index) * kPi / 6.0f;
    transform.translation = {
        8.0f * std::sin(angle),
        8.0f * std::cos(angle),
        0.0f
    };
    return transform;
}

void selectSingle(std::vector<size_t> & selected, size_t index)
{
    selected.clear();
    selected.push_back(index);
    printf("Selection callback: selected object %zu and synced material editor\n", index);
}

void selectMultiple(std::vector<size_t> & selected, size_t a, size_t b)
{
    selected.clear();
    selected.push_back(a);
    selected.push_back(b);
    printf("Selection callback: selected objects %zu and %zu\n", a, b);
}

void applyMaterial(obol::Scene & scene,
                   std::vector<ObjectState> & objects,
                   const std::vector<size_t> & selected,
                   const obol::Material & mat)
{
    printf("Material change callback: Updating %zu selected objects\n",
           selected.size());
    for (size_t index : selected) {
        if (index >= objects.size()) continue;
        objects[index].material = mat;
        scene.setObjectMaterial(objects[index].id, mat);
    }
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

} // namespace

int main(int argc, char **argv)
{
    printf("=== Mentor Example 10.8: Pick Filter for NodeKits ===\n");
    printf("This demonstrates toolkit-agnostic pick filtering and material editing\n");
    printf("\nOriginal used Xt/Motif for window/viewer/editor widgets\n");
    printf("This version keeps selection and material editing application-owned over Obol v2 IDs\n\n");

    initCoinHeadless();

    obol::Scene scene;
    obol::PerspectiveCamera camera;
    camera.position = {0.0f, 0.0f, 30.0f};
    camera.target = {0.0f, 0.0f, 0.0f};
    camera.verticalFieldOfViewRadians = kPi / 4.0f;
    scene.setCamera(camera);
    scene.addDirectionalLight(obol::DirectionalLight{});

    std::vector<ObjectState> objects;
    for (int i = 0; i < 12; ++i) {
        ObjectState state;
        state.material = material(0.8f, 0.8f, 0.8f);
        state.id = scene.addPrimitive(obol::Primitive::Cube,
                                      state.material,
                                      ringTransform(i));
        objects.push_back(state);
    }

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::Renderer renderer(backend);

    std::vector<size_t> selected;
    const char *baseFilename = (argc > 1) ? argv[1] : "10.8.PickFilterNodeKit";
    char filename[512];

    printf("\n--- State 1: Initial scene (nothing selected) ---\n");
    snprintf(filename, sizeof(filename), "%s_initial.rgb", baseFilename);
    if (!renderScene(renderer, scene, target, filename)) return 1;

    printf("\n--- Simulating pick on object 0 (top) ---\n");
    selectSingle(selected, 0);
    printf("--- State 2: Object 0 selected (default material) ---\n");
    snprintf(filename, sizeof(filename), "%s_selected_default.rgb", baseFilename);
    if (!renderScene(renderer, scene, target, filename)) return 1;

    printf("\n--- User changes material to red in editor ---\n");
    applyMaterial(scene, objects, selected, material(1.0f, 0.0f, 0.0f));
    printf("--- State 3: Selected object now red ---\n");
    snprintf(filename, sizeof(filename), "%s_red.rgb", baseFilename);
    if (!renderScene(renderer, scene, target, filename)) return 1;

    printf("\n--- Simulating pick on object 3 (right side) ---\n");
    selectSingle(selected, 3);
    printf("--- State 4: Different object selected ---\n");
    printf("(Editor should sync to show this object's material)\n");
    snprintf(filename, sizeof(filename), "%s_select_different.rgb", baseFilename);
    if (!renderScene(renderer, scene, target, filename)) return 1;

    printf("\n--- User changes this object's material to blue ---\n");
    applyMaterial(scene, objects, selected, material(0.0f, 0.3f, 1.0f));
    printf("--- State 5: Now have both red and blue objects ---\n");
    snprintf(filename, sizeof(filename), "%s_multiple_colors.rgb", baseFilename);
    if (!renderScene(renderer, scene, target, filename)) return 1;

    printf("\n--- Selecting multiple objects ---\n");
    selectMultiple(selected, 0, 6);
    printf("--- State 6: Multiple objects selected ---\n");
    snprintf(filename, sizeof(filename), "%s_multi_select.rgb", baseFilename);
    if (!renderScene(renderer, scene, target, filename)) return 1;

    printf("\n--- User changes material to green (affects all selected) ---\n");
    applyMaterial(scene, objects, selected, material(0.0f, 0.8f, 0.1f));
    printf("--- State 7: Multiple objects changed to green ---\n");
    snprintf(filename, sizeof(filename), "%s_multi_edit.rgb", baseFilename);
    if (!renderScene(renderer, scene, target, filename)) return 1;

    printf("\n=== Summary ===\n");
    printf("Generated 7 images showing pick filtering and material editing [Obol v2]\n");
    printf("\nKey architectural insights:\n");
    printf("  - Pick filtering is application-owned over stable v2 object IDs\n");
    printf("  - Material editors update selected object IDs through v2 material state\n");
    printf("  - Toolkits only translate input and display controls; render backends stay independent\n");

    return 0;
}
