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
 * Two benches are shown so filtered whole-model selection and default
 * component selection stay visually comparable with the original example.
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

    const float plankX[3] = {-0.6f, 0.0f, 0.6f};
    const obol::Material plankMaterials[3] = {
        material(0.55f, 0.30f, 0.10f),
        material(0.60f, 0.35f, 0.12f),
        material(0.50f, 0.28f, 0.09f)
    };
    for (int i = 0; i < 3; ++i) {
        addBenchCube(scene,
                     bench,
                     plankMaterials[i],
                     partTransform(plankX[i], 0.55f, 0.0f, 0.45f, 0.08f, 1.8f));
    }

    const float legZ[2] = {-0.75f, 0.75f};
    const obol::Material frameMaterial = material(0.25f, 0.25f, 0.30f);
    for (int i = 0; i < 2; ++i) {
        addBenchCube(scene,
                     bench,
                     frameMaterial,
                     partTransform(0.0f, 0.25f, legZ[i], 1.6f, 0.5f, 0.08f));
    }

    return bench;
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
    initCoinHeadless();

    obol::Scene scene;
    obol::PerspectiveCamera camera;
    camera.position = {0.0f, 5.5f, 8.0f};
    camera.target = {0.0f, 0.2f, 0.0f};
    camera.verticalFieldOfViewRadians = 0.62f;
    scene.setCamera(camera);

    obol::DirectionalLight light1;
    light1.direction = {-1.0f, -1.5f, -1.0f};
    scene.addDirectionalLight(light1);
    obol::DirectionalLight light2;
    light2.direction = {1.0f, -0.5f, -0.3f};
    light2.intensity = 0.35f;
    light2.color = {0.8f, 0.9f, 1.0f, 1.0f};
    scene.addDirectionalLight(light2);

    Bench leftBench = makeBench(scene, -3.0f);
    makeBench(scene, 3.0f);

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::Renderer renderer(backend);

    const char *base = (argc > 1) ? argv[1] : "10.6.PickFilterTopLevel";
    char filename[512];
    int frameNum = 0;

    printf("Frame %d: initial scene (both benches unselected)\n", frameNum);
    snprintf(filename, sizeof(filename), "%s_frame%02d_initial.rgb", base, frameNum++);
    if (!renderScene(renderer, scene, target, filename)) return 1;

    printf("Frame %d: filtered selection -> entire left bench highlighted\n", frameNum);
    setBenchMaterial(scene, leftBench, highlightMaterial());
    snprintf(filename, sizeof(filename), "%s_frame%02d_filtered_selected.rgb", base, frameNum++);
    if (!renderScene(renderer, scene, target, filename)) return 1;
    restoreBenchMaterial(scene, leftBench);

    printf("Frame %d: default selection -> only one left-bench component highlighted\n", frameNum);
    if (!leftBench.parts.empty()) {
        scene.setObjectMaterial(leftBench.parts[0].id, highlightMaterial());
    }
    snprintf(filename, sizeof(filename), "%s_frame%02d_default_selected.rgb", base, frameNum++);
    if (!renderScene(renderer, scene, target, filename)) return 1;

    printf("\nRendered %d frames demonstrating top-level pick filtering [Obol v2]\n", frameNum);
    return 0;
}
