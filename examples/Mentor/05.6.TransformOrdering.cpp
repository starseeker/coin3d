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
 * Headless version of Inventor Mentor example 5.6
 * 
 * Original: TransformOrdering - shows transform order effects
 * Headless: Renders objects with different transform orderings
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cmath>
#include <cstdio>

namespace {

obol::Transform translation(float x, float y, float z)
{
    obol::Transform transform;
    transform.translation = {x, y, z};
    return transform;
}

obol::Transform xRotation(float radians)
{
    obol::Transform transform;
    transform.rotationAxis = {1.0f, 0.0f, 0.0f};
    transform.rotationRadians = radians;
    return transform;
}

obol::Transform scale(float x, float y, float z)
{
    obol::Transform transform;
    transform.scale = {x, y, z};
    return transform;
}

obol::Material material(float r, float g, float b)
{
    obol::Material result;
    result.baseColor = {r, g, b, 1.0f};
    return result;
}

obol::PerspectiveCamera orbitCamera(const obol::PerspectiveCamera & camera,
                                    float azimuth,
                                    float elevation)
{
    obol::CameraOrbitRequest request;
    request.camera = camera;
    request.azimuthRadians = azimuth;
    request.elevationRadians = elevation;
    return obol::CameraFraming::orbit(request);
}

obol::PerspectiveCamera makeCamera(const obol::Scene & scene, bool angled)
{
    obol::ViewAllRequest request;
    request.viewportWidth = DEFAULT_WIDTH;
    request.viewportHeight = DEFAULT_HEIGHT;
    const obol::PerspectiveCamera camera =
        obol::CameraFraming::viewAllPerspective(scene, request);
    if (angled) {
        return orbitCamera(camera,
                           static_cast<float>(M_PI / 4.0),
                           static_cast<float>(M_PI / 6.0));
    }
    return camera;
}

bool renderView(obol::OffscreenRenderer & renderer,
                obol::Scene & scene,
                const char * filename,
                bool angled)
{
    scene.setCamera(makeCamera(scene, angled));
    const obol::FrameResult result = renderer.render(scene);
    return result.success && renderer.writeRGB(filename);
}

} // namespace

int main(int argc, char **argv)
{
    initCoinHeadless();

    constexpr float halfPi = 1.57079632679f;

    obol::Scene scene;
    obol::DirectionalLight light;
    scene.addDirectionalLight(light);

    const obol::Transform rotate = xRotation(halfPi);
    const obol::Transform stretch = scale(2.0f, 1.0f, 3.0f);

    const obol::SceneGroupId leftTranslate =
        scene.addGroup(translation(-1.5f, 0.0f, 0.0f));
    const obol::SceneGroupId leftRotate =
        scene.addGroup(rotate, leftTranslate);
    const obol::SceneGroupId leftScale =
        scene.addGroup(stretch, leftRotate);
    scene.addPrimitive(obol::Primitive::Cube,
                       material(1.0f, 0.5f, 0.0f),
                       obol::Transform{},
                       obol::PrimitiveOptions{},
                       leftScale);

    const obol::SceneGroupId rightTranslate =
        scene.addGroup(translation(1.5f, 0.0f, 0.0f));
    const obol::SceneGroupId rightScale =
        scene.addGroup(stretch, rightTranslate);
    const obol::SceneGroupId rightRotate =
        scene.addGroup(rotate, rightScale);
    scene.addPrimitive(obol::Primitive::Cube,
                       material(0.0f, 0.5f, 1.0f),
                       obol::Transform{},
                       obol::PrimitiveOptions{},
                       rightRotate);

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::OffscreenRenderer renderer(backend, target);
    renderer.setBackgroundColor({0.0f, 0.0f, 0.0f, 1.0f});

    const char *baseFilename = (argc > 1) ? argv[1] : "05.6.TransformOrdering";
    char filename[256];

    snprintf(filename, sizeof(filename), "%s_front.rgb", baseFilename);
    if (!renderView(renderer, scene, filename, false)) {
        fprintf(stderr, "Error: Failed to render front TransformOrdering view with Obol v2 API\n");
        return 1;
    }
    
    printf("Rendered transform ordering example [Obol v2]\n");
    printf("Left: translate->rotate->scale, Right: translate->scale->rotate [Obol v2]\n");

    snprintf(filename, sizeof(filename), "%s_angle.rgb", baseFilename);
    if (!renderView(renderer, scene, filename, true)) {
        fprintf(stderr, "Error: Failed to render angled TransformOrdering view with Obol v2 API\n");
        return 1;
    }

    return 0;
}
