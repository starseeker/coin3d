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
 * Headless version of Inventor Mentor example 3.2
 * 
 * Original: Robot - creates a robot using node sharing for legs
 * Headless: Renders the robot from multiple viewpoints
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cstdio>

namespace {

obol::Transform translation(float x, float y, float z)
{
    obol::Transform t;
    t.translation = {x, y, z};
    return t;
}

obol::PrimitiveOptions box(float width, float height, float depth)
{
    obol::PrimitiveOptions options;
    options.width = width;
    options.height = height;
    options.depth = depth;
    return options;
}

obol::Material bronzeMaterial()
{
    obol::Material material;
    material.baseColor = {0.78f, 0.57f, 0.11f, 1.0f};
    material.specular = {0.99f, 0.94f, 0.81f, 1.0f};
    material.shininess = 0.28f;
    return material;
}

obol::Material silverMaterial()
{
    obol::Material material;
    material.baseColor = {0.6f, 0.6f, 0.6f, 1.0f};
    material.specular = {0.5f, 0.5f, 0.5f, 1.0f};
    material.shininess = 0.5f;
    return material;
}

void addLeg(obol::Scene & scene,
            obol::SceneGroupId body,
            float x,
            const obol::Material & material)
{
    const obol::SceneGroupId leg =
        scene.addGroup(translation(x, -4.25f, 0.0f), body);
    scene.addPrimitive(obol::Primitive::Cube,
                       material,
                       obol::Transform{},
                       box(1.2f, 2.2f, 1.1f),
                       leg);

    const obol::SceneGroupId calf =
        scene.addGroup(translation(0.0f, -2.25f, 0.0f), leg);
    scene.addPrimitive(obol::Primitive::Cube,
                       material,
                       obol::Transform{},
                       box(1.0f, 2.2f, 1.0f),
                       calf);

    const obol::SceneGroupId foot =
        scene.addGroup(translation(0.0f, -1.5f, 0.5f), calf);
    scene.addPrimitive(obol::Primitive::Cube,
                       material,
                       obol::Transform{},
                       box(0.8f, 0.8f, 2.0f),
                       foot);
}

obol::Scene makeRobot()
{
    obol::Scene scene;
    obol::DirectionalLight light;
    light.direction = {-0.4f, -0.8f, -1.0f};
    scene.addDirectionalLight(light);

    const obol::Material bronze = bronzeMaterial();
    const obol::Material silver = silverMaterial();

    const obol::SceneGroupId body =
        scene.addGroup(translation(0.0f, 3.0f, 0.0f));
    obol::PrimitiveOptions bodyOptions;
    bodyOptions.radius = 2.5f;
    bodyOptions.height = 6.0f;
    scene.addPrimitive(obol::Primitive::Cylinder,
                       bronze,
                       obol::Transform{},
                       bodyOptions,
                       body);
    addLeg(scene, body, 1.0f, bronze);
    addLeg(scene, body, -1.0f, bronze);

    obol::Transform headTransform = translation(0.0f, 7.5f, 0.0f);
    headTransform.scale = {1.5f, 1.5f, 1.5f};
    const obol::SceneGroupId head = scene.addGroup(headTransform);
    scene.addPrimitive(obol::Primitive::Sphere, silver, obol::Transform{},
                       obol::PrimitiveOptions{}, head);

    return scene;
}

bool renderView(obol::Renderer & renderer,
                obol::Scene & scene,
                const obol::RenderTarget & target,
                const char * filename,
                const obol::Vec3 & position)
{
    obol::PerspectiveCamera camera;
    camera.position = position;
    camera.target = {0.0f, 1.0f, 0.0f};
    camera.verticalFieldOfViewRadians = 0.75f;
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

    obol::Scene scene = makeRobot();
    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::Renderer renderer(backend);

    const char *baseFilename = (argc > 1) ? argv[1] : "03.2.Robot";
    char filename[256];
    
    snprintf(filename, sizeof(filename), "%s_front.rgb", baseFilename);
    if (!renderView(renderer, scene, target, filename, {0.0f, 1.0f, 18.0f})) {
        fprintf(stderr, "Error: Failed to render front robot view with Obol v2 API\n");
        return 1;
    }
    
    snprintf(filename, sizeof(filename), "%s_side.rgb", baseFilename);
    if (!renderView(renderer, scene, target, filename, {18.0f, 1.0f, 0.0f})) {
        fprintf(stderr, "Error: Failed to render side robot view with Obol v2 API\n");
        return 1;
    }
    
    snprintf(filename, sizeof(filename), "%s_angle.rgb", baseFilename);
    if (!renderView(renderer, scene, target, filename, {12.0f, 7.0f, 12.0f})) {
        fprintf(stderr, "Error: Failed to render angled robot view with Obol v2 API\n");
        return 1;
    }

    printf("Successfully rendered robot views to %s_*.rgb (%dx%d) [Obol v2]\n",
           baseFilename, DEFAULT_WIDTH, DEFAULT_HEIGHT);
    return 0;
}
