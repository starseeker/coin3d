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
 * Headless version of Inventor Mentor example 10.6
 *
 * Demonstrates top-level versus default pick filtering using v2 object IDs.
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cstdio>
#include <vector>

namespace {

struct BenchPart {
    obol::SceneObjectId id = obol::InvalidSceneObjectId;
    obol::Material material;
};

struct Bench {
    obol::SceneGroupId group = obol::InvalidSceneGroupId;
    std::vector<BenchPart> parts;
};

obol::Material material(float r, float g, float b)
{
    obol::Material result;
    result.baseColor = {r, g, b, 1.0f};
    return result;
}

obol::Material highlightMaterial()
{
    obol::Material result = material(1.0f, 0.95f, 0.15f);
    result.specular = {0.5f, 0.5f, 0.1f, 1.0f};
    result.shininess = 0.5f;
    return result;
}

obol::Transform translation(float x, float y, float z)
{
    obol::Transform result;
    result.translation = {x, y, z};
    return result;
}

obol::Transform partTransform(float x, float y, float z,
                              float sx, float sy, float sz)
{
    obol::Transform result;
    result.translation = {x, y, z};
    result.scale = {sx, sy, sz};
    return result;
}

BenchPart addBenchCube(obol::Scene & scene,
                       Bench & bench,
                       const obol::Material & partMaterial,
                       const obol::Transform & transform)
{
    BenchPart part;
    part.material = partMaterial;
    part.id = scene.addPrimitive(obol::Primitive::Cube,
                                 partMaterial,
                                 transform,
                                 obol::PrimitiveOptions{},
                                 bench.group);
    bench.parts.push_back(part);
    return part;
}

Bench makeBench(obol::Scene & scene, float x)
{
    Bench bench;
    bench.group = scene.addGroup(translation(x, 0.0f, 0.0f));

    const obol::Material gray = material(0.65f, 0.67f, 0.67f);
    const float slatX[11] = {
        -2.5f, -2.0f, -1.5f, -1.0f, -0.5f, 0.0f,
         0.5f,  1.0f,  1.5f,  2.0f,  2.5f
    };
    for (float sx : slatX) {
        addBenchCube(scene,
                     bench,
                     gray,
                     partTransform(sx, 0.75f, 0.0f, 0.055f, 0.85f, 0.05f));
    }

    addBenchCube(scene, bench, gray, partTransform(0.0f, 0.0f, 0.0f, 1.7f, 0.12f, 0.08f));
    addBenchCube(scene, bench, gray, partTransform(0.0f, 1.45f, 0.0f, 1.55f, 0.07f, 0.08f));
    addBenchCube(scene, bench, gray, partTransform(-3.1f, 0.55f, 0.0f, 0.10f, 0.95f, 0.10f));
    addBenchCube(scene, bench, gray, partTransform(3.1f, 0.55f, 0.0f, 0.10f, 0.95f, 0.10f));
    addBenchCube(scene, bench, gray, partTransform(-2.6f, -0.75f, 0.0f, 0.10f, 0.70f, 0.10f));
    addBenchCube(scene, bench, gray, partTransform(2.6f, -0.75f, 0.0f, 0.10f, 0.70f, 0.10f));
    addBenchCube(scene, bench, gray, partTransform(-2.8f, 0.10f, 0.0f, 0.06f, 0.75f, 0.08f));
    addBenchCube(scene, bench, gray, partTransform(2.8f, 0.10f, 0.0f, 0.06f, 0.75f, 0.08f));

    return bench;
}

void applyViewAllCamera(obol::Scene & scene)
{
    obol::ViewAllRequest request;
    request.viewportWidth = DEFAULT_WIDTH;
    request.viewportHeight = DEFAULT_HEIGHT;
    request.slack = 1.25f;
    scene.setCamera(obol::CameraFraming::viewAllPerspective(scene, request));
}

void setBenchMaterial(obol::Scene & scene, const Bench & bench, const obol::Material & mat)
{
    for (const BenchPart & part : bench.parts) {
        scene.setObjectMaterial(part.id, mat);
    }
}

void restoreBenchMaterial(obol::Scene & scene, const Bench & bench)
{
    for (const BenchPart & part : bench.parts) {
        scene.setObjectMaterial(part.id, part.material);
    }
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

    obol::DirectionalLight light1;
    light1.direction = {-1.0f, -1.5f, -1.0f};
    scene.addDirectionalLight(light1);
    obol::DirectionalLight light2;
    light2.direction = {1.0f, -0.5f, -0.3f};
    light2.intensity = 0.35f;
    light2.color = {0.8f, 0.9f, 1.0f, 1.0f};
    scene.addDirectionalLight(light2);

    Bench bench = makeBench(scene, 0.0f);
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

    const char *base = (argc > 1) ? argv[1] : "10.6.PickFilterTopLevel";
    char filename[512];
    int frameNum = 0;

    printf("Frame %d: initial scene (bench unselected)\n", frameNum);
    snprintf(filename, sizeof(filename), "%s_frame%02d_initial.rgb", base, frameNum++);
    if (!renderScene(renderer, scene, filename)) return 1;

    printf("Frame %d: filtered selection -> entire bench highlighted\n", frameNum);
    setBenchMaterial(scene, bench, highlightMaterial());
    snprintf(filename, sizeof(filename), "%s_frame%02d_filtered_selected.rgb", base, frameNum++);
    if (!renderScene(renderer, scene, filename)) return 1;
    restoreBenchMaterial(scene, bench);

    printf("Frame %d: default selection -> only one bench component highlighted\n", frameNum);
    if (!bench.parts.empty()) {
        scene.setObjectMaterial(bench.parts[0].id, highlightMaterial());
    }
    snprintf(filename, sizeof(filename), "%s_frame%02d_default_selected.rgb", base, frameNum++);
    if (!renderScene(renderer, scene, filename)) return 1;

    printf("\nRendered %d frames demonstrating top-level pick filtering [Obol v2]\n", frameNum);
    return 0;
}
