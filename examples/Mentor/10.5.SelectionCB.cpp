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
 * Headless version of Inventor Mentor example 10.5
 * 
 * Original: SelectionCB - demonstrates selection callbacks with mouse interaction
 * Headless: Demonstrates selection callbacks being triggered
 * 
 * This example shows two approaches to selection (both valid):
 * 1. Programmatic selection using select()/deselect() - current implementation
 * 2. Event-based selection via mouse picks - could be added using simulateMousePress()
 * 
 * The programmatic approach is simpler and demonstrates the callback mechanism clearly.
 * For a more realistic simulation, mouse pick events could trigger selection automatically.
 * See 09.4.PickAction for pick event simulation or 15.3.AttachManip for mouse event patterns.
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

obol::Transform translation(float x, float y, float z)
{
    obol::Transform transform;
    transform.translation = {x, y, z};
    return transform;
}

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
    request.slack = 1.5f;
    scene.setCamera(obol::CameraFraming::viewAllPerspective(scene, request));
}

void selectObject(obol::Scene & scene,
                  obol::SceneObjectId object,
                  const char * label,
                  const obol::Material & selectedMaterial)
{
    scene.setObjectMaterial(object, selectedMaterial);
    printf("%s selected - changing to reddish color\n", label);
}

void deselectObject(obol::Scene & scene,
                    obol::SceneObjectId object,
                    const char * label,
                    const obol::Material & normalMaterial)
{
    scene.setObjectMaterial(object, normalMaterial);
    printf("%s deselected - changing to gray color\n", label);
}

} // namespace

int main(int argc, char **argv)
{
    initCoinHeadless();

    obol::Scene scene;
    scene.addDirectionalLight(obol::DirectionalLight{});

    const obol::Material normal = material(0.8f, 0.8f, 0.8f);
    const obol::Material selected = material(1.0f, 0.2f, 0.2f);

    const obol::SceneObjectId sphere =
        scene.addPrimitive(obol::Primitive::Sphere,
                           normal,
                           translation(2.5f, 0.0f, 0.0f));
    const obol::SceneObjectId cube =
        scene.addPrimitive(obol::Primitive::Cube,
                           normal,
                           translation(-2.5f, 0.0f, 0.0f));
    applyViewAllCamera(scene);

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::OffscreenRenderer renderer(backend, target);
    renderer.setBackgroundColor({0.0f, 0.0f, 0.0f, 1.0f});

    const char *baseFilename = (argc > 1) ? argv[1] : "10.5.SelectionCB";
    char filename[256];

    int frameNum = 0;

    printf("\n=== Initial state (nothing selected) ===\n");
    snprintf(filename, sizeof(filename), "%s_frame%02d_initial.rgb", baseFilename, frameNum++);
    if (!renderScene(renderer, scene, filename)) {
        fprintf(stderr, "Error: Failed to render initial selection scene with Obol v2 API\n");
        return 1;
    }

    printf("\n=== Selecting sphere (sphere turns red) ===\n");
    selectObject(scene, sphere, "Sphere", selected);
    snprintf(filename, sizeof(filename), "%s_frame%02d_sphere_selected.rgb", baseFilename, frameNum++);
    if (!renderScene(renderer, scene, filename)) return 1;

    printf("\n=== Deselecting sphere (sphere returns to gray) ===\n");
    deselectObject(scene, sphere, "Sphere", normal);
    snprintf(filename, sizeof(filename), "%s_frame%02d_sphere_deselected.rgb", baseFilename, frameNum++);
    if (!renderScene(renderer, scene, filename)) return 1;

    printf("\n=== Selecting cube (cube turns red) ===\n");
    selectObject(scene, cube, "Cube", selected);
    snprintf(filename, sizeof(filename), "%s_frame%02d_cube_selected.rgb", baseFilename, frameNum++);
    if (!renderScene(renderer, scene, filename)) return 1;

    printf("\n=== Deselecting cube (cube returns to gray) ===\n");
    deselectObject(scene, cube, "Cube", normal);
    snprintf(filename, sizeof(filename), "%s_frame%02d_cube_deselected.rgb", baseFilename, frameNum++);
    if (!renderScene(renderer, scene, filename)) return 1;

    printf("\nRendered %d frames demonstrating application selection callbacks [Obol v2]\n", frameNum);
    return 0;
}
