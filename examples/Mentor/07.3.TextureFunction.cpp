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
 * Headless version of Inventor Mentor example 7.3
 * 
 * Original: TextureFunction - spheres with texture coordinate generation
 * Headless: Renders three spheres with different texture repeat frequencies
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cmath>
#include <cstdio>
#include <memory>

namespace {

std::shared_ptr<obol::Texture2D> faceTexture()
{
    std::shared_ptr<obol::Texture2D> texture(new obol::Texture2D);
    texture->image.width = 32;
    texture->image.height = 32;
    texture->image.format = obol::ImageFormat::RGB;
    texture->image.pixels.resize(32 * 32 * 3);

    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            int idx = (y * 32 + x) * 3;
            int dx = x - 16, dy = y - 16;
            int dist = dx*dx + dy*dy;
            
            if (dist < 225) {
                texture->image.pixels[idx] = 255;
                texture->image.pixels[idx + 1] = 220;
                texture->image.pixels[idx + 2] = 0;
                
                if ((dx == -6 && dy > 2 && dy < 6) || (dx == 6 && dy > 2 && dy < 6)) {
                    texture->image.pixels[idx] = 0;
                    texture->image.pixels[idx + 1] = 0;
                    texture->image.pixels[idx + 2] = 0;
                }
                
                if (dy < -4 && dy > -8 && std::abs(dx) < 8 && std::abs(dx) > 5) {
                    texture->image.pixels[idx] = 0;
                    texture->image.pixels[idx + 1] = 0;
                    texture->image.pixels[idx + 2] = 0;
                }
            } else {
                texture->image.pixels[idx] = 100;
                texture->image.pixels[idx + 1] = 100;
                texture->image.pixels[idx + 2] = 150;
            }
        }
    }
    return texture;
}

obol::Material faceMaterial()
{
    obol::Material material;
    material.baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
    material.baseColorTexture = faceTexture();
    return material;
}

obol::Mesh mappedSphere(float frequency)
{
    constexpr float pi = 3.14159265359f;
    constexpr int slices = 32;
    constexpr int stacks = 16;

    obol::Mesh mesh;
    for (int stack = 0; stack <= stacks; ++stack) {
        const float v = static_cast<float>(stack) / static_cast<float>(stacks);
        const float theta = pi * v;
        const float y = std::cos(theta);
        const float ring = std::sin(theta);
        for (int slice = 0; slice <= slices; ++slice) {
            const float u = static_cast<float>(slice) / static_cast<float>(slices);
            const float phi = 2.0f * pi * u;
            const float x = ring * std::cos(phi);
            const float z = ring * std::sin(phi);
            mesh.positions.push_back({x, y, z});
            mesh.normals.push_back({x, y, z});
            mesh.texCoords.push_back({frequency * x + 0.5f,
                                      frequency * y + 0.5f});
        }
    }

    const uint32_t rowStride = slices + 1;
    for (int stack = 0; stack < stacks; ++stack) {
        for (int slice = 0; slice < slices; ++slice) {
            const uint32_t a = static_cast<uint32_t>(stack * rowStride + slice);
            const uint32_t b = a + 1;
            const uint32_t c = a + rowStride + 1;
            const uint32_t d = a + rowStride;
            mesh.indices.push_back(a);
            mesh.indices.push_back(b);
            mesh.indices.push_back(c);
            mesh.indices.push_back(a);
            mesh.indices.push_back(c);
            mesh.indices.push_back(d);
        }
    }
    return mesh;
}

obol::Transform translation(float x, float y, float z)
{
    obol::Transform transform;
    transform.translation = {x, y, z};
    return transform;
}

bool renderView(obol::OffscreenRenderer & renderer,
                obol::Scene & scene,
                const char * filename,
                const obol::Vec3 & position)
{
    obol::PerspectiveCamera camera;
    camera.position = position;
    camera.target = {2.5f, 0.0f, 0.0f};
    camera.verticalFieldOfViewRadians = 0.62f;
    scene.setCamera(camera);
    const obol::FrameResult result = renderer.render(scene);
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

    const obol::Material material = faceMaterial();
    scene.addMesh(mappedSphere(2.0f), material);
    scene.addMesh(mappedSphere(1.0f), material, translation(2.5f, 0.0f, 0.0f));
    scene.addMesh(mappedSphere(0.5f), material, translation(5.0f, 0.0f, 0.0f));

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::OffscreenRenderer renderer(backend, target);
    renderer.setBackgroundColor({0.0f, 0.0f, 0.0f, 1.0f});


    const char *baseFilename = (argc > 1) ? argv[1] : "07.3.TextureFunction";
    char filename[256];

    snprintf(filename, sizeof(filename), "%s_front.rgb", baseFilename);
    if (!renderView(renderer, scene, filename, {2.5f, 0.0f, 8.0f})) {
        fprintf(stderr, "Error: Failed to render front TextureFunction view with Obol v2 API\n");
        return 1;
    }

    snprintf(filename, sizeof(filename), "%s_angle.rgb", baseFilename);
    if (!renderView(renderer, scene, filename, {8.0f, 4.0f, 6.0f})) {
        fprintf(stderr, "Error: Failed to render angled TextureFunction view with Obol v2 API\n");
        return 1;
    }

    printf("Rendered texture-function spheres with generated UVs [Obol v2]\n");
    return 0;
}
