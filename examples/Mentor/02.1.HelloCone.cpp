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
 * Headless version of Inventor Mentor example 2.1
 * 
 * Original: Hello Cone - draws a red cone in a window
 * Headless: Renders a red cone to an image file
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

int main(int argc, char **argv)
{
    initCoinHeadless();

    obol::Scene scene;

    obol::PerspectiveCamera camera;
    camera.position = {0.0f, 0.0f, 5.0f};
    camera.target = {0.0f, 0.0f, 0.0f};
    scene.setCamera(camera);

    scene.addDirectionalLight(obol::DirectionalLight{});

    obol::Material material;
    material.baseColor = {1.0f, 0.0f, 0.0f, 1.0f};
    scene.addPrimitive(obol::Primitive::Cone, material);

    const char *filename = (argc > 1) ? argv[1] : "02.1.HelloCone.rgb";
    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::OffscreenRenderer renderer(backend, target);
    renderer.setBackgroundColor({0.0f, 0.0f, 0.0f, 1.0f});

    const obol::FrameResult result = renderer.render(scene);
    if (!result.success || !renderer.writeRGB(filename)) {
        fprintf(stderr, "Error: Failed to render scene with Obol v2 API\n");
        return 1;
    }

    printf("Successfully rendered to %s (%dx%d) [Obol v2]\n",
           filename, DEFAULT_WIDTH, DEFAULT_HEIGHT);
    return 0;
}
