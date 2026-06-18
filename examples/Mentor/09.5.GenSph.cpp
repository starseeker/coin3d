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
 * Headless version of Inventor Mentor example 9.5
 * 
 * Original: GenSph - uses callback to generate sphere primitives
 * Headless: Demonstrates callback action with primitive generation
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cstdio>

namespace {

void printSphereTriangles(const char * name, const obol::Mesh & mesh)
{
    printf("\nSphere named \"%s\"\n", name);
    const size_t triangleCount = mesh.indices.size() / 3;
    for (size_t triangle = 0; triangle < triangleCount && triangle < 3; ++triangle) {
        const obol::Vec3 & v1 = mesh.positions[mesh.indices[triangle * 3 + 0]];
        const obol::Vec3 & v2 = mesh.positions[mesh.indices[triangle * 3 + 1]];
        const obol::Vec3 & v3 = mesh.positions[mesh.indices[triangle * 3 + 2]];

        printf("  Triangle %zu:\n", triangle + 1);
        printf("    v1: (%.2f, %.2f, %.2f)\n", 
               v1.x, v1.y, v1.z);
        printf("    v2: (%.2f, %.2f, %.2f)\n",
               v2.x, v2.y, v2.z);
        printf("    v3: (%.2f, %.2f, %.2f)\n",
               v3.x, v3.y, v3.z);
    }
    printf("  Total triangles generated: %zu\n", triangleCount);
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

    const obol::Mesh sphereMesh = obol::makeSphereMesh(1.0f, 24, 12);

    obol::Scene scene;
    obol::PerspectiveCamera camera;
    camera.position = {0.0f, 0.0f, 5.0f};
    camera.target = {0.0f, 0.0f, 0.0f};
    scene.setCamera(camera);
    scene.addDirectionalLight(obol::DirectionalLight{});

    obol::Material material;
    material.baseColor = {0.8f, 0.2f, 0.2f, 1.0f};
    scene.addMesh(sphereMesh, material);

    printf("Generating primitives for sphere...\n");
    printSphereTriangles("TestSphere", sphereMesh);

    const char *baseFilename = (argc > 1) ? argv[1] : "09.5.GenSph";
    char filename[256];
    snprintf(filename, sizeof(filename), "%s.rgb", baseFilename);

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::OffscreenRenderer renderer(backend, target);
    renderer.setBackgroundColor({0.0f, 0.0f, 0.0f, 1.0f});
    if (!renderScene(renderer, scene, filename)) {
        fprintf(stderr, "Error: Failed to render generated sphere mesh with Obol v2 API\n");
        return 1;
    }

    return 0;
}
