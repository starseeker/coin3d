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
 * Headless version of Inventor Mentor example 8.2
 * 
 * Original: UniCurve - displays uniform B-spline curve
 * Headless: Renders uniform B-spline curve from multiple angles
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cmath>
#include <cstdio>

// The control points for this curve
const float pts[13][3] = {
   { 6.0,  0.0,  6.0},
   {-5.5,  0.5,  5.5},
   {-5.0,  1.0, -5.0},
   { 4.5,  1.5, -4.5},
   { 4.0,  2.0,  4.0},
   {-3.5,  2.5,  3.5},
   {-3.0,  3.0, -3.0},
   { 2.5,  3.5, -2.5},
   { 2.0,  4.0,  2.0},
   {-1.5,  4.5,  1.5},
   {-1.0,  5.0, -1.0},
   { 0.5,  5.5, -0.5},
   { 0.0,  6.0,  0.0}};

// The knot vector
const float knots[17] = {
   0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10, 10, 10};

namespace {

float basis(int i, int degree, float u)
{
    if (degree == 0) {
        if ((knots[i] <= u && u < knots[i + 1]) ||
            (u == knots[13] && i == 12)) {
            return 1.0f;
        }
        return 0.0f;
    }

    float value = 0.0f;
    const float leftDenom = knots[i + degree] - knots[i];
    if (leftDenom != 0.0f) {
        value += ((u - knots[i]) / leftDenom) * basis(i, degree - 1, u);
    }
    const float rightDenom = knots[i + degree + 1] - knots[i + 1];
    if (rightDenom != 0.0f) {
        value += ((knots[i + degree + 1] - u) / rightDenom) *
                 basis(i + 1, degree - 1, u);
    }
    return value;
}

obol::Vec3 evaluateCurve(float u)
{
    obol::Vec3 point;
    for (int i = 0; i < 13; i++) {
        const float b = basis(i, 3, u);
        point.x += b * pts[i][0];
        point.y += b * pts[i][1];
        point.z += b * pts[i][2];
    }
    return point;
}

obol::Polyline sampledCurve()
{
    obol::Polyline curve;
    curve.lineWidth = 4.0f;
    constexpr int samples = 192;
    for (int i = 0; i <= samples; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(samples);
        const float u = t * 10.0f;
        curve.points.push_back(evaluateCurve(u));
    }
    return curve;
}

obol::Material curveMaterial()
{
    obol::Material material;
    material.baseColor = {1.0f, 0.0f, 0.1f, 1.0f};
    material.emissive = {1.0f, 0.0f, 0.1f, 1.0f};
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
    camera.target = {0.0f, 3.0f, 0.0f};
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
    obol::DirectionalLight light;
    light.direction = {-0.5f, -0.7f, -1.0f};
    scene.addDirectionalLight(light);
    scene.addPolyline(sampledCurve(), curveMaterial());

    obol::PrimitiveOptions markerOptions;
    markerOptions.radius = 0.3f;
    const obol::Material marker = markerMaterial();
    for (int i = 0; i < 13; ++i) {
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

    const char *baseFilename = (argc > 1) ? argv[1] : "08.2.UniCurve";
    char filename[256];

    snprintf(filename, sizeof(filename), "%s_view1.rgb", baseFilename);
    if (!renderView(renderer, scene, target, filename, {0.0f, 3.0f, 20.0f})) {
        fprintf(stderr, "Error: Failed to render UniCurve front view with Obol v2 API\n");
        return 1;
    }

    snprintf(filename, sizeof(filename), "%s_side.rgb", baseFilename);
    if (!renderView(renderer, scene, target, filename, {20.0f, 3.0f, 0.0f})) {
        fprintf(stderr, "Error: Failed to render UniCurve side view with Obol v2 API\n");
        return 1;
    }

    snprintf(filename, sizeof(filename), "%s_top.rgb", baseFilename);
    if (!renderView(renderer, scene, target, filename, {0.0f, 22.0f, 0.0f},
                    {0.0f, 0.0f, -1.0f})) {
        fprintf(stderr, "Error: Failed to render UniCurve top view with Obol v2 API\n");
        return 1;
    }

    printf("Rendered sampled uniform B-spline curve [Obol v2]\n");
    return 0;
}
