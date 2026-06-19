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
 * Headless version of Inventor Mentor example 09.2
 * 
 * Original: Texture - using offscreen renderer to generate texture map
 * Headless: Renders scene to texture, then applies to cube
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cstdio>
#include <cmath>
#include <memory>

namespace {

obol::Scene createTextureSourceScene()
{
    obol::Scene scene;
    obol::PerspectiveCamera camera;
    camera.position = {0.0f, 0.0f, 8.0f};
    camera.target = {0.0f, 0.0f, 0.0f};
    camera.verticalFieldOfViewRadians = 0.75f;
    scene.setCamera(camera);
    scene.addDirectionalLight(obol::DirectionalLight{});

    obol::Material red;
    red.baseColor = {0.8f, 0.0f, 0.0f, 1.0f};

    obol::Transform rootRotation;
    rootRotation.rotationAxis = {1.0f, 0.0f, 0.0f};
    rootRotation.rotationRadians = static_cast<float>(M_PI_2);
    const obol::SceneGroupId rotatedRoot = scene.addGroup(rootRotation);

    obol::Transform rotation;
    rotation.rotationAxis = {1.0f, 1.0f, 0.0f};
    rotation.rotationRadians = 1.57f;
    scene.addPrimitive(obol::Primitive::Cone, red, rotation, obol::PrimitiveOptions{}, rotatedRoot);
    return scene;
}

bool generateTextureMap(obol::Renderer & renderer,
                        const obol::RenderTarget & target,
                        obol::Texture2D & texture)
{
    obol::Scene textureScene = createTextureSourceScene();
    obol::FrameRequest request;
    request.scene = &textureScene;
    request.target = target;
    request.background = {0.8f, 0.8f, 0.0f, 1.0f};
    const obol::FrameResult result = renderer.render(request);
    const unsigned char * pixels = renderer.pixels();
    if (!result.success || !pixels) {
        return false;
    }

    texture.image.width = target.width;
    texture.image.height = target.height;
    texture.image.format = obol::ImageFormat::RGB;
    texture.image.pixels.assign(
        pixels,
        pixels + texture.image.width * texture.image.height * 3);
    texture.model = obol::TextureModel::Modulate;
    return true;
}

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

obol::Transform rotation(const obol::Vec3 & axis, float radians)
{
    obol::Transform transform;
    transform.rotationAxis = axis;
    transform.rotationRadians = radians;
    return transform;
}

} // namespace

int main(int argc, char **argv)
{
    initCoinHeadless();

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::Renderer renderer(backend);

    std::shared_ptr<obol::Texture2D> texture(new obol::Texture2D);
    printf("Generating texture map (%dx%d)...\n", DEFAULT_WIDTH, DEFAULT_HEIGHT);
    if (!generateTextureMap(renderer, target, *texture)) {
        fprintf(stderr, "Error: Could not generate texture map with Obol v2 API\n");
        return 1;
    }
    printf("Successfully generated texture map\n");

    obol::Scene scene;
    obol::PerspectiveCamera camera;
    camera.position = {0.0f, 0.0f, 4.0f};
    camera.target = {0.0f, 0.0f, 0.0f};
    scene.setCamera(camera);
    scene.addDirectionalLight(obol::DirectionalLight{});

    obol::Material material;
    material.baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
    material.baseColorTexture = texture;
    const obol::SceneObjectId cube =
        scene.addPrimitive(obol::Primitive::Cube, material);

    const char *baseFilename = (argc > 1) ? argv[1] : "09.2.Texture";
    char filename[256];

    printf("\nRendering textured cube...\n");
    snprintf(filename, sizeof(filename), "%s_front.rgb", baseFilename);
    if (!renderScene(renderer, scene, target, filename)) {
        fprintf(stderr, "Error: Failed to render textured cube front view with Obol v2 API\n");
        return 1;
    }

    scene.setObjectTransform(cube, rotation({0.0f, 1.0f, 0.0f}, static_cast<float>(M_PI / 4.0)));
    snprintf(filename, sizeof(filename), "%s_angle1.rgb", baseFilename);
    if (!renderScene(renderer, scene, target, filename)) {
        fprintf(stderr, "Error: Failed to render textured cube angle1 with Obol v2 API\n");
        return 1;
    }

    scene.setObjectTransform(cube, rotation({1.0f, 1.0f, 0.0f}, static_cast<float>(M_PI / 3.0)));
    snprintf(filename, sizeof(filename), "%s_angle2.rgb", baseFilename);
    if (!renderScene(renderer, scene, target, filename)) {
        fprintf(stderr, "Error: Failed to render textured cube angle2 with Obol v2 API\n");
        return 1;
    }

    printf("\nSuccessfully completed offscreen texture rendering example [Obol v2]\n");
    return 0;
}
