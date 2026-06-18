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
 * Headless version of Inventor Mentor example 4.1
 * 
 * Original: Cameras - demonstrates different camera types with blinker
 * Headless: Renders scene from three different camera projections
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cstdio>

namespace {

obol::Material material(float r, float g, float b)
{
    obol::Material m;
    m.baseColor = {r, g, b, 1.0f};
    return m;
}

obol::Transform translation(float x, float y, float z)
{
    obol::Transform t;
    t.translation = {x, y, z};
    return t;
}

obol::Scene makeScene()
{
    obol::Scene scene;
    scene.addDirectionalLight(obol::DirectionalLight{});

    scene.addPrimitive(obol::Primitive::Cone,
                       material(0.85f, 0.15f, 0.10f),
                       translation(-2.5f, 0.0f, 1.0f));
    scene.addPrimitive(obol::Primitive::Sphere,
                       material(0.15f, 0.70f, 0.20f));
    scene.addPrimitive(obol::Primitive::Cube,
                       material(0.15f, 0.30f, 0.85f),
                       translation(2.5f, 0.0f, -1.0f));
    return scene;
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

    obol::Scene scene = makeScene();
    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::OffscreenRenderer renderer(backend, target);
    renderer.setBackgroundColor({0.0f, 0.0f, 0.0f, 1.0f});

    const char *baseFilename = (argc > 1) ? argv[1] : "04.1.Cameras";
    char filename[256];

    obol::OrthographicCamera orthoCamera;
    orthoCamera.position = {0.0f, 0.0f, 10.0f};
    orthoCamera.target = {0.0f, 0.0f, 0.0f};
    orthoCamera.height = 6.5f;
    scene.setCamera(orthoCamera);
    snprintf(filename, sizeof(filename), "%s_orthographic.rgb", baseFilename);
    if (!renderScene(renderer, scene, filename)) {
        fprintf(stderr, "Error: Failed to render orthographic camera view with Obol v2 API\n");
        return 1;
    }

    obol::PerspectiveCamera perspectiveCamera;
    perspectiveCamera.position = {0.0f, 0.0f, 10.0f};
    perspectiveCamera.target = {0.0f, 0.0f, 0.0f};
    perspectiveCamera.verticalFieldOfViewRadians = 0.65f;
    scene.setCamera(perspectiveCamera);
    snprintf(filename, sizeof(filename), "%s_perspective.rgb", baseFilename);
    if (!renderScene(renderer, scene, filename)) {
        fprintf(stderr, "Error: Failed to render perspective camera view with Obol v2 API\n");
        return 1;
    }

    obol::PerspectiveCamera offCenterCamera;
    offCenterCamera.position = {3.0f, 2.5f, 12.0f};
    offCenterCamera.target = {0.0f, 0.0f, 0.0f};
    offCenterCamera.verticalFieldOfViewRadians = 0.65f;
    scene.setCamera(offCenterCamera);
    snprintf(filename, sizeof(filename), "%s_offcenter.rgb", baseFilename);
    if (!renderScene(renderer, scene, filename)) {
        fprintf(stderr, "Error: Failed to render off-center camera view with Obol v2 API\n");
        return 1;
    }

    printf("Rendered scene from 3 different camera projections [Obol v2]\n");
    return 0;
}
