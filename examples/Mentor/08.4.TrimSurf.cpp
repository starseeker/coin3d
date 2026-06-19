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
 * Headless version of Inventor Mentor example 8.4
 * 
 * Original: TrimSurf - displays trimmed NURBS surface
 * Headless: Renders trimmed NURBS surface from multiple angles
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cmath>
#include <cstdio>

// The array of trim coordinates (only 10 defined, matches original)
const float tpts[10][2] = {
   {0.0, 0.0},
   {1.0, 0.0},
   {1.0, 1.0},
   {0.0, 1.0},
   {0.2, 0.2},
   {0.2, 0.7},
   {0.9, 0.7},
   {0.9, 0.2},
   {0.7, 0.0},
   {0.4, 0.8}};

// The 16 coordinates defining the Bezier surface
const float pts[16][3] = {
   {-4.5, -2.0,  8.0},
   {-2.0,  1.0,  8.0},
   { 2.0, -3.0,  6.0},
   { 5.0, -1.0,  8.0},
   {-3.0,  3.0,  4.0},
   { 0.0, -1.0,  4.0},
   { 1.0, -1.0,  4.0},
   { 3.0,  2.0,  4.0},
   {-5.0, -2.0, -2.0},
   {-2.0, -4.0, -2.0},
   { 2.0, -1.0, -2.0},
   { 5.0,  0.0, -2.0},
   {-4.5,  2.0, -6.0},
   {-2.0, -4.0, -5.0},
   { 2.0,  3.0, -5.0},
   { 4.5, -2.0, -6.0}};

// The 3 knot vectors for the 3 trim curves
const float tknots1[7] = {0, 0, 1, 2, 3, 4, 4};
const float tknots2[6] = {0, 0, 1, 2, 3, 3};
const float tknots3[8] = {0, 0, 0, 0, 1, 1, 1, 1};

// The Bezier knot vector for the surface
const float knots[8] = {0, 0, 0, 0, 1, 1, 1, 1};

namespace {

float bernstein3(int i, float t)
{
    const float inv = 1.0f - t;
    switch (i) {
    case 0: return inv * inv * inv;
    case 1: return 3.0f * t * inv * inv;
    case 2: return 3.0f * t * t * inv;
    case 3: return t * t * t;
    }
    return 0.0f;
}

obol::Vec3 evaluateSurface(float u, float v)
{
    obol::Vec3 point;
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            const float weight = bernstein3(column, u) * bernstein3(row, v);
            const int index = row * 4 + column;
            point.x += weight * pts[index][0];
            point.y += weight * pts[index][1];
            point.z += weight * pts[index][2];
        }
    }
    return point;
}

obol::Vec3 subtract(const obol::Vec3 & lhs, const obol::Vec3 & rhs)
{
    return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

obol::Vec3 cross(const obol::Vec3 & lhs, const obol::Vec3 & rhs)
{
    return {lhs.y * rhs.z - lhs.z * rhs.y,
            lhs.z * rhs.x - lhs.x * rhs.z,
            lhs.x * rhs.y - lhs.y * rhs.x};
}

obol::Vec3 normalize(const obol::Vec3 & value)
{
    const float length = std::sqrt(value.x * value.x +
                                   value.y * value.y +
                                   value.z * value.z);
    if (length <= 0.0f) return {0.0f, 0.0f, 1.0f};
    return {value.x / length, value.y / length, value.z / length};
}

float clamp01(float value)
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

obol::Vec3 surfaceNormal(float u, float v)
{
    constexpr float delta = 0.01f;
    const obol::Vec3 u0 = evaluateSurface(clamp01(u - delta), v);
    const obol::Vec3 u1 = evaluateSurface(clamp01(u + delta), v);
    const obol::Vec3 v0 = evaluateSurface(u, clamp01(v - delta));
    const obol::Vec3 v1 = evaluateSurface(u, clamp01(v + delta));
    return normalize(cross(subtract(u1, u0), subtract(v1, v0)));
}

bool insideTrimHole(float u, float v)
{
    return u >= tpts[4][0] &&
           u <= tpts[6][0] &&
           v >= tpts[4][1] &&
           v <= tpts[6][1];
}

obol::Mesh tessellatedTrimmedSurface()
{
    constexpr uint32_t resolution = 32;
    obol::Mesh mesh;
    mesh.topology = obol::MeshTopology::Polygons;

    for (uint32_t row = 0; row <= resolution; ++row) {
        const float v = static_cast<float>(row) / static_cast<float>(resolution);
        for (uint32_t column = 0; column <= resolution; ++column) {
            const float u = static_cast<float>(column) / static_cast<float>(resolution);
            mesh.positions.push_back(evaluateSurface(u, v));
            mesh.normals.push_back(surfaceNormal(u, v));
        }
    }

    const uint32_t rowStride = resolution + 1;
    for (uint32_t row = 0; row < resolution; ++row) {
        const float centerV = (static_cast<float>(row) + 0.5f) /
                              static_cast<float>(resolution);
        for (uint32_t column = 0; column < resolution; ++column) {
            const float centerU = (static_cast<float>(column) + 0.5f) /
                                  static_cast<float>(resolution);
            if (insideTrimHole(centerU, centerV)) {
                continue;
            }

            const uint32_t upperLeft = row * rowStride + column;
            const uint32_t lowerLeft = (row + 1) * rowStride + column;
            const uint32_t lowerRight = lowerLeft + 1;
            const uint32_t upperRight = upperLeft + 1;
            mesh.indices.push_back(upperLeft);
            mesh.indices.push_back(upperRight);
            mesh.indices.push_back(lowerRight);
            mesh.indices.push_back(lowerLeft);
            mesh.faceVertexCounts.push_back(4);
        }
    }
    return mesh;
}

obol::Material surfaceMaterial()
{
    obol::Material material;
    material.baseColor = {0.8f, 0.3f, 0.1f, 1.0f};
    return material;
}

obol::Material markerMaterial()
{
    obol::Material material;
    material.baseColor = {0.2f, 0.6f, 1.0f, 1.0f};
    return material;
}

obol::Transform translation(float x, float y, float z)
{
    obol::Transform transform;
    transform.translation = {x, y, z};
    return transform;
}

bool renderView(obol::Renderer & renderer,
                obol::Scene & scene,
                const obol::RenderTarget & target,
                const char * filename,
                const obol::Vec3 & position,
                const obol::Vec3 & up = {0.0f, 1.0f, 0.0f})
{
    obol::PerspectiveCamera camera;
    camera.position = position;
    camera.target = {0.0f, -0.5f, 1.0f};
    camera.up = up;
    camera.verticalFieldOfViewRadians = 0.72f;
    scene.setCamera(camera);
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
    scene.addDirectionalLight(obol::DirectionalLight{});
    scene.addMesh(tessellatedTrimmedSurface(), surfaceMaterial());

    obol::PrimitiveOptions markerOptions;
    markerOptions.radius = 0.4f;
    const obol::Material marker = markerMaterial();
    for (int i = 0; i < 16; ++i) {
        scene.addPrimitive(obol::Primitive::Sphere,
                           marker,
                           translation(pts[i][0], pts[i][1], pts[i][2]),
                           markerOptions);
    }

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::Renderer renderer(backend);

    const char *baseFilename = (argc > 1) ? argv[1] : "08.4.TrimSurf";
    char filename[256];

    snprintf(filename, sizeof(filename), "%s_view1.rgb", baseFilename);
    if (!renderView(renderer, scene, target, filename, {0.0f, -0.5f, 22.0f})) {
        fprintf(stderr, "Error: Failed to render TrimSurf front view with Obol v2 API\n");
        return 1;
    }

    snprintf(filename, sizeof(filename), "%s_side.rgb", baseFilename);
    if (!renderView(renderer, scene, target, filename, {22.0f, -0.5f, 1.0f})) {
        fprintf(stderr, "Error: Failed to render TrimSurf side view with Obol v2 API\n");
        return 1;
    }

    snprintf(filename, sizeof(filename), "%s_top.rgb", baseFilename);
    if (!renderView(renderer, scene, target, filename, {0.0f, 22.0f, 1.0f},
                    {0.0f, 0.0f, -1.0f})) {
        fprintf(stderr, "Error: Failed to render TrimSurf top view with Obol v2 API\n");
        return 1;
    }

    printf("Rendered tessellated trimmed Bezier surface [Obol v2]\n");
    return 0;
}
