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
 * Headless version of Inventor Mentor example 4.2
 * 
 * Original: Lights - demonstrates directional and point lights with shuttle
 * Headless: Renders scene with different light positions (simulating animation)
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cstdio>

namespace {

obol::Transform translation(const obol::Vec3 & position)
{
    obol::Transform transform;
    transform.translation = position;
    return transform;
}

obol::Vec3 lerp(const obol::Vec3 & a, const obol::Vec3 & b, float t)
{
    return obol::Vec3{
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t
    };
}

bool renderFrame(obol::OffscreenRenderer & renderer,
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

    obol::DirectionalLight directional;
    directional.direction = {0.0f, -1.0f, -1.0f};
    directional.color = {1.0f, 0.0f, 0.0f, 1.0f};
    scene.addDirectionalLight(directional);

    const obol::SceneGroupId movingLight = scene.addGroup();
    obol::PointLight point;
    point.color = {0.0f, 1.0f, 0.0f, 1.0f};
    point.location = {0.0f, 0.0f, 0.0f};
    scene.addPointLight(point, movingLight);

    scene.addPrimitive(obol::Primitive::Cone, obol::Material{});

    obol::ViewAllRequest cameraRequest;
    cameraRequest.viewportWidth = DEFAULT_WIDTH;
    cameraRequest.viewportHeight = DEFAULT_HEIGHT;
    scene.setCamera(obol::CameraFraming::viewAllPerspective(scene,
                                                            cameraRequest));

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::OffscreenRenderer renderer(backend, target);
    renderer.setBackgroundColor({0.0f, 0.0f, 0.0f, 1.0f});

    const char *baseFilename = (argc > 1) ? argv[1] : "04.2.Lights";
    char filename[256];

    const obol::Vec3 pos1{-2.0f, -1.0f, 3.0f};
    const obol::Vec3 pos2{1.0f, 2.0f, -3.0f};

    const int numFrames = 5;
    for (int i = 0; i < numFrames; i++) {
        const float t = static_cast<float>(i) / static_cast<float>(numFrames - 1);
        scene.setGroupTransform(movingLight, translation(lerp(pos1, pos2, t)));
        
        snprintf(filename, sizeof(filename), "%s_frame%02d.rgb", baseFilename, i);
        if (!renderFrame(renderer, scene, filename)) {
            fprintf(stderr, "Error: Failed to render light animation frame %d with Obol v2 API\n", i);
            return 1;
        }
    }

    printf("Rendered %d frames showing lighting variation [Obol v2]\n", numFrames);
    return 0;
}
