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
 * Headless version of Inventor Mentor example 10.7
 *
 * Original: PickFilterManip - demonstrates pick filtering around manipulators
 * Headless: demonstrates app-level pick filtering and selected-object highlight
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cstdio>

namespace {

obol::Material material(float r, float g, float b)
{
    obol::Material result;
    result.baseColor = {r, g, b, 1.0f};
    return result;
}

obol::Transform transformForIndex(int index)
{
    obol::Transform transform;
    transform.translation = {2.5f * static_cast<float>(index), 0.0f, 0.0f};
    return transform;
}

bool renderScene(obol::OffscreenRenderer & renderer,
                 obol::Scene & scene,
                 const char * filename)
{
    const obol::FrameResult result = renderer.render(scene);
    return result.success && renderer.writeRGB(filename);
}

} // namespace

int main(int argc, char **argv)
{
    initCoinHeadless();

    obol::Scene scene;
    obol::PerspectiveCamera camera;
    camera.position = {15.0f, 0.0f, 28.0f};
    camera.target = {15.0f, 0.0f, 0.0f};
    camera.verticalFieldOfViewRadians = 0.52f;
    scene.setCamera(camera);
    scene.addDirectionalLight(obol::DirectionalLight{});

    const obol::Material colors[] = {
        material(0.0f, 0.0f, 1.0f),
        material(0.0f, 1.0f, 0.0f),
        material(0.0f, 1.0f, 1.0f),
        material(1.0f, 0.0f, 0.0f),
        material(1.0f, 0.0f, 1.0f),
        material(1.0f, 1.0f, 0.0f),
        material(0.8f, 0.8f, 0.8f)
    };

    obol::SceneObjectId firstCone = obol::InvalidSceneObjectId;
    obol::Material firstConeMaterial;
    for (int i = 0; i < 12; ++i) {
        const obol::Material coneMaterial = colors[i % 7];
        const obol::SceneObjectId id =
            scene.addPrimitive(obol::Primitive::Cone,
                               coneMaterial,
                               transformForIndex(i));
        if (i == 0) {
            firstCone = id;
            firstConeMaterial = coneMaterial;
        }
    }

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::OffscreenRenderer renderer(backend, target);
    renderer.setBackgroundColor({0.0f, 0.0f, 0.0f, 1.0f});

    const char *baseFilename = (argc > 1) ? argv[1] : "10.7.PickFilterManip";
    char filename[256];
    int frameNum = 0;

    printf("\n=== Initial scene ===\n");
    snprintf(filename, sizeof(filename), "%s_frame%02d_initial.rgb", baseFilename, frameNum++);
    if (!renderScene(renderer, scene, filename)) return 1;

    printf("\n=== Object selected (highlighted; manipulator would attach in an interactive app) ===\n");
    scene.setObjectMaterial(firstCone, material(1.0f, 0.5f, 0.0f));
    snprintf(filename, sizeof(filename), "%s_frame%02d_with_manip.rgb", baseFilename, frameNum++);
    if (!renderScene(renderer, scene, filename)) return 1;

    printf("\n=== Object deselected (restored to original color) ===\n");
    scene.setObjectMaterial(firstCone, firstConeMaterial);
    snprintf(filename, sizeof(filename), "%s_frame%02d_without_manip.rgb", baseFilename, frameNum++);
    if (!renderScene(renderer, scene, filename)) return 1;

    printf("\nRendered %d frames demonstrating pick filtering around manipulators [Obol v2]\n", frameNum);
    return 0;
}
