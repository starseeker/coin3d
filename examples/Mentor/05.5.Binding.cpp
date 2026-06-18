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
 * Headless version of Inventor Mentor example 5.5
 * 
 * Original: Binding - demonstrates material binding variations
 * Headless: Renders stellated dodecahedron with different material bindings
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cstdio>
#include <cstdint>

// Positions of all vertices (stellated dodecahedron)
static const float vertexPositions[12][3] =
{
   { 0.0000,  1.2142,  0.7453},  // top
   { 0.0000,  1.2142, -0.7453},  // points surrounding top
   {-1.2142,  0.7453,  0.0000},
   {-0.7453,  0.0000,  1.2142}, 
   { 0.7453,  0.0000,  1.2142}, 
   { 1.2142,  0.7453,  0.0000},
   { 0.0000, -1.2142,  0.7453},  // points surrounding bottom
   {-1.2142, -0.7453,  0.0000}, 
   {-0.7453,  0.0000, -1.2142},
   { 0.7453,  0.0000, -1.2142}, 
   { 1.2142, -0.7453,  0.0000}, 
   { 0.0000, -1.2142, -0.7453}, // bottom
};

// Connectivity information
static const int32_t indices[72] =
{
   1,  2,  3,  4, 5, -1, // top face
   0,  1,  8,  7, 3, -1, // 5 faces about top
   0,  2,  7,  6, 4, -1,
   0,  3,  6, 10, 5, -1,
   0,  4, 10,  9, 1, -1,
   0,  5,  9,  8, 2, -1,
    9,  5, 4, 6, 11, -1, // 5 faces about bottom
   10,  4, 3, 7, 11, -1,
    6,  3, 2, 8, 11, -1,
    7,  2, 1, 9, 11, -1,
    8,  1, 5,10, 11, -1,
    6,  7, 8, 9, 10, -1, // bottom face
};
 
// Colors for the 12 faces
static const float colors[12][3] =
{
   {1.0, .0, 0}, {.0,  .0, 1.0}, {0, .7,  .7}, { .0, 1.0,  0},
   { .7, .7, 0}, {.7,  .0,  .7}, {0, .0, 1.0}, { .7,  .0, .7},
   { .7, .7, 0}, {.0, 1.0,  .0}, {0, .7,  .7}, {1.0,  .0,  0}
};

namespace {

void addConnectivity(obol::Mesh & mesh)
{
    uint32_t currentFaceCount = 0;
    for (int32_t index : indices) {
        if (index < 0) {
            if (currentFaceCount > 0) {
                mesh.faceVertexCounts.push_back(currentFaceCount);
                currentFaceCount = 0;
            }
        } else {
            mesh.indices.push_back(static_cast<uint32_t>(index));
            ++currentFaceCount;
        }
    }
}

void addPalette(std::vector<obol::Color> & palette)
{
    for (const auto & color : colors) {
        palette.push_back({color[0], color[1], color[2], 1.0f});
    }
}

obol::Mesh makeStellatedDodecahedron(int bindingType)
{
    obol::Mesh mesh;
    mesh.topology = obol::MeshTopology::Polygons;

    for (const auto & position : vertexPositions) {
        mesh.positions.push_back({position[0], position[1], position[2]});
    }
    addConnectivity(mesh);

    if (bindingType == 1) {
        addPalette(mesh.vertexColors);
    } else {
        addPalette(mesh.faceColors);
        if (bindingType == 2) {
            static const uint32_t faceIndices[12] = {
                11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0
            };
            for (uint32_t index : faceIndices) {
                mesh.faceColorIndices.push_back(index);
            }
        }
    }

    return mesh;
}

bool renderBinding(const char * filename, int bindingType)
{
    obol::Scene scene;
    scene.addDirectionalLight(obol::DirectionalLight{});
    scene.addMesh(makeStellatedDodecahedron(bindingType));

    obol::PerspectiveCamera camera;
    camera.position = {0.0f, 0.0f, 7.0f};
    camera.target = {0.0f, 0.0f, 0.0f};
    camera.verticalFieldOfViewRadians = 0.55f;
    scene.setCamera(camera);

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::OffscreenRenderer renderer(backend, target);
    renderer.setBackgroundColor({0.0f, 0.0f, 0.0f, 1.0f});

    const obol::FrameResult result = renderer.render(scene);
    return result.success && renderer.writeRGB(filename);
}

} // namespace

int main(int argc, char **argv)
{
    initCoinHeadless();

    const char *baseFilename = (argc > 1) ? argv[1] : "05.5.Binding";
    char filename[256];

    const char *bindingNames[] = {"per_face", "per_vertex_indexed", "per_face_indexed"};
    
    for (int binding = 0; binding < 3; binding++) {
        snprintf(filename, sizeof(filename), "%s_%s.rgb", baseFilename, bindingNames[binding]);
        if (!renderBinding(filename, binding)) {
            fprintf(stderr,
                    "Error: Failed to render Binding view %s with Obol v2 API\n",
                    bindingNames[binding]);
            return 1;
        }
        
        printf("Rendered binding type %d: %s [Obol v2]\n", binding, bindingNames[binding]);
    }

    return 0;
}
