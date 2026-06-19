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
 * Headless version of Inventor Mentor example 9.1
 * 
 * Original: Print - renders scene to PostScript
 * Headless: Demonstrates offscreen rendering (already headless by nature)
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cstdio>

namespace {

obol::Transform translation(float x, float y, float z)
{
    obol::Transform transform;
    transform.translation = {x, y, z};
    return transform;
}

} // namespace

int main(int argc, char **argv)
{
    initCoinHeadless();

    obol::Scene scene;

    obol::PerspectiveCamera camera;
    camera.position = {0.0f, 0.0f, 11.0f};
    camera.target = {0.0f, 0.0f, 0.0f};
    camera.verticalFieldOfViewRadians = 0.62f;
    scene.setCamera(camera);
    scene.addDirectionalLight(obol::DirectionalLight{});

    obol::Material redMaterial;
    redMaterial.baseColor = {1.0f, 0.0f, 0.0f, 1.0f};
    scene.addPrimitive(obol::Primitive::Cube,
                       redMaterial,
                       translation(-2.0f, 0.0f, 0.0f));

    obol::Material blueMaterial;
    blueMaterial.baseColor = {0.0f, 0.5f, 1.0f, 1.0f};
    scene.addPrimitive(obol::Primitive::Sphere,
                       blueMaterial,
                       translation(2.0f, 0.0f, 0.0f));

    const char *baseFilename = (argc > 1) ? argv[1] : "09.1.Print";
    char filename[256];
    snprintf(filename, sizeof(filename), "%s.rgb", baseFilename);

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::Renderer renderer(backend);

    obol::FrameRequest request;
    request.scene = &scene;
    request.target = target;
    request.background = {0.0f, 0.0f, 0.0f, 1.0f};
    const obol::FrameResult result = renderer.render(request);
    if (!result.success || !renderer.writeRGB(filename)) {
        fprintf(stderr, "Error: Failed to render print example with Obol v2 API\n");
        return 1;
    }
    
    printf("Rendered scene using Obol v2 offscreen renderer\n");
    printf("Note: Original example printed to PostScript\n");
    return 0;
}
