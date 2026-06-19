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
 * Headless version of Inventor Mentor example 7.2
 * 
 * Original: TextureCoordinates - textured square with explicit texture coords
 * Headless: Renders textured square from multiple angles
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cmath>
#include <cstdio>
#include <memory>

namespace {

std::shared_ptr<obol::Texture2D> brickTexture()
{
    std::shared_ptr<obol::Texture2D> texture(new obol::Texture2D);
    texture->image.width = 64;
    texture->image.height = 64;
    texture->image.format = obol::ImageFormat::RGB;
    texture->image.pixels.resize(64 * 64 * 3);

    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 64; x++) {
            int idx = (y * 64 + x) * 3;
            bool isHorizontalLine = (y % 16 == 0);
            bool isVerticalLine = ((x + (y/16)*8) % 32 == 0);
            bool isMortar = isHorizontalLine || isVerticalLine;
            
            if (isMortar) {
                texture->image.pixels[idx] = 180;
                texture->image.pixels[idx + 1] = 180;
                texture->image.pixels[idx + 2] = 180;
            } else {
                texture->image.pixels[idx] = 150;
                texture->image.pixels[idx + 1] = 80;
                texture->image.pixels[idx + 2] = 60;
            }
        }
    }
    return texture;
}

obol::Mesh texturedSquare()
{
    obol::Mesh mesh;
    mesh.topology = obol::MeshTopology::Polygons;
    mesh.positions = {
        {-3.0f, -3.0f, 0.0f},
        { 3.0f, -3.0f, 0.0f},
        { 3.0f,  3.0f, 0.0f},
        {-3.0f,  3.0f, 0.0f}
    };
    mesh.texCoords = {
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {1.0f, 1.0f},
        {0.0f, 1.0f}
    };
    mesh.indices = {0, 1, 2, 3};
    mesh.texCoordIndices = {0, 1, 2, 3};
    mesh.faceVertexCounts = {4};
    mesh.faceNormals = {{0.0f, 0.0f, 1.0f}};
    return mesh;
}

obol::Material texturedMaterial()
{
    obol::Material material;
    material.baseColorTexture = brickTexture();
    return material;
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

void makeCameras(const obol::Scene & scene,
                 obol::PerspectiveCamera & frontCamera,
                 obol::PerspectiveCamera & angleCamera)
{
    obol::ViewAllRequest request;
    request.viewportWidth = DEFAULT_WIDTH;
    request.viewportHeight = DEFAULT_HEIGHT;
    frontCamera = obol::CameraFraming::viewAllPerspective(scene, request);
    angleCamera = orbitCamera(frontCamera,
                              static_cast<float>(M_PI / 4.0),
                              static_cast<float>(M_PI / 6.0));
}

bool renderView(obol::Renderer & renderer,
                obol::Scene & scene,
                const obol::RenderTarget & target,
                const char * filename,
                const obol::PerspectiveCamera & camera)
{
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
    scene.addDirectionalLight(light);
    scene.addMesh(texturedSquare(), texturedMaterial());

    obol::PerspectiveCamera frontCamera;
    obol::PerspectiveCamera angleCamera;
    makeCameras(scene, frontCamera, angleCamera);

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::Renderer renderer(backend);

    const char *baseFilename = (argc > 1) ? argv[1] : "07.2.TextureCoordinates";
    char filename[256];

    snprintf(filename, sizeof(filename), "%s_front.rgb", baseFilename);
    if (!renderView(renderer, scene, target, filename, frontCamera)) {
        fprintf(stderr, "Error: Failed to render front TextureCoordinates view with Obol v2 API\n");
        return 1;
    }

    snprintf(filename, sizeof(filename), "%s_angle.rgb", baseFilename);
    if (!renderView(renderer, scene, target, filename, angleCamera)) {
        fprintf(stderr, "Error: Failed to render angled TextureCoordinates view with Obol v2 API\n");
        return 1;
    }

    printf("Rendered textured square with explicit UVs [Obol v2]\n");
    return 0;
}
