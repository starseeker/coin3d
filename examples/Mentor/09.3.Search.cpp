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
 * Headless version of Inventor Mentor example 9.3
 * 
 * Original: Search - uses search action to find lights
 * Headless: Demonstrates search action and renders result
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cstdio>

namespace {

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
    camera.position = {0.0f, 0.0f, 5.0f};
    camera.target = {0.0f, 0.0f, 0.0f};
    scene.setCamera(camera);

    obol::Material material;
    material.baseColor = {0.8f, 0.3f, 0.1f, 1.0f};
    scene.addPrimitive(obol::Primitive::Cube, material);

    obol::SceneQuery lightQuery;
    lightQuery.category = obol::SceneObjectCategory::Light;
    
    const char *baseFilename = (argc > 1) ? argv[1] : "09.3.Search";
    char filename[256];

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::Renderer renderer(backend);

    snprintf(filename, sizeof(filename), "%s_no_light.rgb", baseFilename);
    if (!renderScene(renderer, scene, target, filename)) {
        fprintf(stderr, "Error: Failed to render unlit scene with Obol v2 API\n");
        return 1;
    }
    printf("Rendered scene without light\n");

    if (!scene.hasObjects(lightQuery)) {
        printf("Scene query: No lights found - adding default light\n");
        scene.addDirectionalLight(obol::DirectionalLight{});
    } else {
        printf("Scene query: Light already exists\n");
    }

    snprintf(filename, sizeof(filename), "%s_with_light.rgb", baseFilename);
    if (!renderScene(renderer, scene, target, filename)) {
        fprintf(stderr, "Error: Failed to render lit scene with Obol v2 API\n");
        return 1;
    }
    printf("Rendered scene with light\n");

    return 0;
}
