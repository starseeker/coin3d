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
 * Headless version of Inventor Mentor example 9.4
 * 
 * Original: PickAction - demonstrates pick actions with mouse interaction
 * Headless: Simulates pick actions at calculated screen positions of objects
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cmath>
#include <cstdio>

namespace {

obol::Material material(float r, float g, float b)
{
    obol::Material result;
    result.baseColor = {r, g, b, 1.0f};
    return result;
}

obol::Material highlight(float r, float g, float b)
{
    obol::Material result = material(r, g, b);
    result.emissive = {r * 0.35f, g * 0.35f, b * 0.35f, 1.0f};
    return result;
}

obol::Transform translation(const obol::Vec3 & offset)
{
    obol::Transform transform;
    transform.translation = offset;
    return transform;
}

obol::Mesh makeStarMesh()
{
    static const float points[][3] = {
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, -0.55000001f},
        {0.323282f, 0.0f, 0.44495901f},
        {-0.523081f, 0.0f, -0.16995899f},
        {0.523081f, 0.0f, -0.16995899f},
        {-0.323282f, 0.0f, 0.44495901f},
        {0.0f, 0.0f, -0.55000001f},
        {0.0f, 0.12f, -0.5f},
        {0.29389301f, 0.12f, 0.40450901f},
        {0.323282f, 0.0f, 0.44495901f},
        {0.323282f, 0.0f, 0.44495901f},
        {0.29389301f, 0.12f, 0.40450901f},
        {-0.475528f, 0.12f, -0.15450899f},
        {-0.523081f, 0.0f, -0.16995899f},
        {-0.523081f, 0.0f, -0.16995899f},
        {-0.475528f, 0.12f, -0.15450899f},
        {0.475528f, 0.12f, -0.15450899f},
        {0.523081f, 0.0f, -0.16995899f},
        {0.523081f, 0.0f, -0.16995899f},
        {0.475528f, 0.12f, -0.15450899f},
        {-0.29389301f, 0.12f, 0.40450901f},
        {-0.323282f, 0.0f, 0.44495901f},
        {-0.323282f, 0.0f, 0.44495901f},
        {-0.29389301f, 0.12f, 0.40450901f},
        {0.0f, 0.12f, -0.5f},
        {0.0f, 0.0f, -0.55000001f},
        {0.0f, 0.17f, -0.44999999f},
        {0.264503f, 0.17f, 0.36405799f},
        {0.264503f, 0.17f, 0.36405799f},
        {-0.427975f, 0.17f, -0.13905799f},
        {-0.427975f, 0.17f, -0.13905799f},
        {0.427975f, 0.17f, -0.13905799f},
        {0.427975f, 0.17f, -0.13905799f},
        {-0.264503f, 0.17f, 0.36405799f},
        {-0.264503f, 0.17f, 0.36405799f},
        {0.0f, 0.17f, -0.44999999f},
        {0.0f, 0.2f, -0.34999999f},
        {0.205725f, 0.2f, 0.28315601f},
        {0.205725f, 0.2f, 0.28315601f},
        {-0.33287001f, 0.2f, -0.108156f},
        {-0.33287001f, 0.2f, -0.108156f},
        {0.33287001f, 0.2f, -0.108156f},
        {0.33287001f, 0.2f, -0.108156f},
        {-0.205725f, 0.2f, 0.28315601f},
        {-0.205725f, 0.2f, 0.28315601f},
        {0.0f, 0.2f, -0.34999999f},
        {0.0f, 0.2f, -0.30000001f},
        {0.17633601f, 0.2f, 0.242705f},
        {-0.285317f, 0.2f, -0.092704996f},
        {0.285317f, 0.2f, -0.092704996f},
        {-0.17633601f, 0.2f, 0.242705f},
        {0.0f, 0.2f, 0.0f}
    };
    static const uint32_t indices[] = {
        9, 6, 8, 7, 27, 26, 37, 36, 47, 46, 51, 50, 49, 42,
        13, 10, 12, 11, 29, 28, 39, 38, 48, 47, 51,
        17, 14, 16, 15, 31, 30, 41, 40, 49, 48, 51,
        21, 18, 20, 19, 33, 32, 43, 42, 50,
        25, 22, 24, 23, 35, 34, 45, 44, 46, 50,
        1, 2, 0, 3, 4,
        4, 5, 0, 1
    };

    obol::Mesh mesh;
    mesh.topology = obol::MeshTopology::TriangleStrips;
    for (const auto & point : points) {
        mesh.positions.push_back({point[0], point[1], point[2]});
    }
    mesh.indices.assign(indices, indices + sizeof(indices) / sizeof(indices[0]));
    mesh.stripVertexCounts = {14, 11, 11, 9, 10, 5, 4};
    return mesh;
}

obol::Vec3 subtract(const obol::Vec3 & a, const obol::Vec3 & b)
{
    return obol::Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

obol::Vec3 normalize(const obol::Vec3 & v)
{
    const float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (length <= 0.0f) {
        return obol::Vec3{0.0f, 0.0f, -1.0f};
    }
    return obol::Vec3{v.x / length, v.y / length, v.z / length};
}

bool renderScene(obol::OffscreenRenderer & renderer,
                 obol::Scene & scene,
                 const char * filename)
{
    const obol::FrameResult result = renderer.render(scene);
    return result.success && renderer.writeRGB(filename);
}

obol::PickResult pickAtWorldPoint(const obol::Scene & scene,
                                  const obol::Vec3 & cameraPosition,
                                  const obol::Vec3 & worldPoint)
{
    obol::PickRequest request;
    request.viewportWidth = DEFAULT_WIDTH;
    request.viewportHeight = DEFAULT_HEIGHT;
    request.radiusPixels = 8.0f;
    request.useWorldRay = true;
    request.rayOrigin = cameraPosition;
    request.rayDirection = normalize(subtract(worldPoint, cameraPosition));
    return obol::Picker::pick(scene, request);
}

void printPickResult(const char * label, const obol::PickResult & result)
{
    if (!result.hit || result.hits.empty()) {
        printf("\n%s was not picked\n", label);
        return;
    }

    const obol::PickHit & hit = result.hits[0];
    printf("\nPicked %s as scene object %u\n", label, hit.objectId);
    printf("  point: (%.2f, %.2f, %.2f)\n",
           hit.point.x, hit.point.y, hit.point.z);
    printf("  normal: (%.2f, %.2f, %.2f)\n",
           hit.normal.x, hit.normal.y, hit.normal.z);
}

} // namespace

int main(int argc, char **argv)
{
    initCoinHeadless();

    obol::Scene scene;
    const obol::Vec3 cameraPosition{0.0f, -5.5f, 0.0f};
    obol::PerspectiveCamera camera;
    camera.position = cameraPosition;
    camera.target = {0.0f, 0.0f, 0.0f};
    camera.up = {0.0f, 0.0f, 1.0f};
    camera.verticalFieldOfViewRadians = 0.62f;
    scene.setCamera(camera);
    obol::DirectionalLight light;
    light.direction = {0.0f, 1.0f, 0.0f};
    scene.addDirectionalLight(light);

    const obol::Mesh starMesh = makeStarMesh();
    const obol::Vec3 star1Center{-1.0f, 0.0f, 0.65f};
    const obol::Vec3 star2Center{1.0f, 0.0f, -0.65f};

    const obol::Material star1Material = material(0.9f, 0.9f, 0.9f);
    const obol::Material star2Material = material(1.0f, 0.0f, 0.0f);
    const obol::SceneObjectId star1 =
        scene.addMesh(starMesh,
                      star1Material,
                      translation(star1Center));
    const obol::SceneObjectId star2 =
        scene.addMesh(starMesh,
                      star2Material,
                      translation(star2Center));

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::OffscreenRenderer renderer(backend, target);
    renderer.setBackgroundColor({0.0f, 0.0f, 0.0f, 1.0f});

    const char *baseFilename = (argc > 1) ? argv[1] : "09.4.PickAction";
    char filename[256];

    int frameNum = 0;

    snprintf(filename, sizeof(filename), "%s_initial.rgb", baseFilename);
    if (!renderScene(renderer, scene, filename)) {
        fprintf(stderr, "Error: Failed to render initial pick scene with Obol v2 API\n");
        return 1;
    }
    frameNum++;

    printf("Star 1 center: world (%g, %g, %g), scene object %u\n",
           star1Center.x, star1Center.y, star1Center.z, star1);
    printf("Star 2 center: world (%g, %g, %g), scene object %u\n",
           star2Center.x, star2Center.y, star2Center.z, star2);

    const obol::PickResult pickedStar1 =
        pickAtWorldPoint(scene, cameraPosition, star1Center);
    printPickResult("first star", pickedStar1);
    if (pickedStar1.hit && !pickedStar1.hits.empty() &&
        pickedStar1.hits[0].objectId == star1) {
        scene.setObjectMaterial(star1, highlight(1.0f, 1.0f, 0.0f));
        snprintf(filename, sizeof(filename), "%s_pick_star1.rgb", baseFilename);
        if (!renderScene(renderer, scene, filename)) {
            fprintf(stderr, "Error: Failed to render first highlighted pick with Obol v2 API\n");
            return 1;
        }
        frameNum++;
        scene.setObjectMaterial(star1, star1Material);
    }

    const obol::PickResult pickedStar2 =
        pickAtWorldPoint(scene, cameraPosition, star2Center);
    printPickResult("second star", pickedStar2);
    if (pickedStar2.hit && !pickedStar2.hits.empty() &&
        pickedStar2.hits[0].objectId == star2) {
        scene.setObjectMaterial(star2, highlight(1.0f, 0.0f, 0.0f));
        snprintf(filename, sizeof(filename), "%s_pick_star2.rgb", baseFilename);
        if (!renderScene(renderer, scene, filename)) {
            fprintf(stderr, "Error: Failed to render second highlighted pick with Obol v2 API\n");
            return 1;
        }
        frameNum++;
        scene.setObjectMaterial(star2, star2Material);
    }

    printf("\nRendered %d frames demonstrating pick action [Obol v2]\n", frameNum);
    return 0;
}
