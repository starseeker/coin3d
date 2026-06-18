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
 * Headless version of Inventor Mentor example 5.1
 * 
 * Original: FaceSet - builds an obelisk using Face Set node
 * Headless: Renders the obelisk from multiple angles
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cstdio>

// Eight polygons. The first four are triangles, the second four are quadrilaterals.
// Vertex Y coordinates are scaled and centred at the origin (y = -7.5 to +7.5)
// so that rotateCamera() orbits correctly and the obelisk fills the views
// with a proportional 2:1 height-to-base ratio.
static const float vertices[28][3] =
{
   { 0,  7.5f, 0}, {-2, 6, 2}, { 2, 6, 2},            //front tri
   { 0,  7.5f, 0}, {-2, 6,-2}, {-2, 6, 2},            //left  tri
   { 0,  7.5f, 0}, { 2, 6,-2}, {-2, 6,-2},            //rear  tri
   { 0,  7.5f, 0}, { 2, 6, 2}, { 2, 6,-2},            //right tri
   {-2,  6, 2}, {-4,-7.5f, 4}, { 4,-7.5f, 4}, { 2, 6, 2},  //front quad
   {-2,  6,-2}, {-4,-7.5f,-4}, {-4,-7.5f, 4}, {-2, 6, 2},  //left  quad
   { 2,  6,-2}, { 4,-7.5f,-4}, {-4,-7.5f,-4}, {-2, 6,-2},  //rear  quad
   { 2,  6, 2}, { 4,-7.5f, 4}, { 4,-7.5f,-4}, { 2, 6,-2}   //right quad
};

// Number of vertices in each polygon
static uint32_t numvertices[8] = {3, 3, 3, 3, 4, 4, 4, 4};

// Normals for each polygon (recalculated for the scaled vertex positions)
static const float norms[8][3] =
{ 
   {0, .8f,  .6f}, {-.6f, .8f, 0}, //front, left tris
   {0, .8f, -.6f}, { .6f, .8f, 0}, //rear, right tris
   
   {0, .1466f,  .9892f}, {-.9892f, .1466f, 0},//front, left quads
   {0, .1466f, -.9892f}, { .9892f, .1466f, 0},//rear, right quads
};

namespace {

obol::Mesh makeObeliskMesh()
{
    obol::Mesh mesh;
    mesh.topology = obol::MeshTopology::Polygons;
    for (const auto & vertex : vertices) {
        mesh.positions.push_back({vertex[0], vertex[1], vertex[2]});
        mesh.indices.push_back(static_cast<uint32_t>(mesh.indices.size()));
    }
    for (uint32_t count : numvertices) {
        mesh.faceVertexCounts.push_back(count);
    }
    for (const auto & normal : norms) {
        mesh.faceNormals.push_back({normal[0], normal[1], normal[2]});
    }
    return mesh;
}

obol::Material sandstoneMaterial()
{
    obol::Material material;
    material.baseColor = {0.75f, 0.60f, 0.35f, 1.0f};
    return material;
}

bool renderView(obol::OffscreenRenderer & renderer,
                obol::Scene & scene,
                const char * filename,
                const obol::Vec3 & position)
{
    obol::PerspectiveCamera camera;
    camera.position = position;
    camera.target = {0.0f, 0.0f, 0.0f};
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
    obol::DirectionalLight keyLight;
    keyLight.direction = {-1.0f, -1.0f, -1.0f};
    scene.addDirectionalLight(keyLight);
    obol::DirectionalLight fillLight;
    fillLight.direction = {1.0f, 0.5f, 1.0f};
    fillLight.intensity = 0.4f;
    scene.addDirectionalLight(fillLight);
    scene.addMesh(makeObeliskMesh(), sandstoneMaterial());

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::OffscreenRenderer renderer(backend, target);
    renderer.setBackgroundColor({0.0f, 0.0f, 0.0f, 1.0f});

    const char *baseFilename = (argc > 1) ? argv[1] : "05.1.FaceSet";
    char filename[256];

    snprintf(filename, sizeof(filename), "%s_front.rgb", baseFilename);
    if (!renderView(renderer, scene, filename, {0.0f, 5.0f, 26.0f})) {
        fprintf(stderr, "Error: Failed to render front FaceSet view with Obol v2 API\n");
        return 1;
    }

    snprintf(filename, sizeof(filename), "%s_side.rgb", baseFilename);
    if (!renderView(renderer, scene, filename, {-22.0f, 5.0f, 13.0f})) {
        fprintf(stderr, "Error: Failed to render side FaceSet view with Obol v2 API\n");
        return 1;
    }

    snprintf(filename, sizeof(filename), "%s_angle.rgb", baseFilename);
    if (!renderView(renderer, scene, filename, {-18.0f, 9.0f, 18.0f})) {
        fprintf(stderr, "Error: Failed to render angled FaceSet view with Obol v2 API\n");
        return 1;
    }

    printf("Rendered obelisk FaceSet views [Obol v2]\n");
    return 0;
}
