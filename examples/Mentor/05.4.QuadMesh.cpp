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
 * Headless version of Inventor Mentor example 5.4
 * 
 * Original: QuadMesh - creates St. Louis Arch using QuadMesh
 * Headless: Renders the arch from multiple angles
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cstdio>
#include <cstdint>

// Positions of all vertices (St. Louis Arch)
static const float vertexPositions[60][3] =
{  // 1st row
   {-13.0,  0.0, 1.5}, {-10.3, 13.7, 1.2}, { -7.6, 21.7, 1.0}, 
   { -5.0, 26.1, 0.8}, { -2.3, 28.2, 0.6}, { -0.3, 28.8, 0.5},
   {  0.3, 28.8, 0.5}, {  2.3, 28.2, 0.6}, {  5.0, 26.1, 0.8}, 
   {  7.6, 21.7, 1.0}, { 10.3, 13.7, 1.2}, { 13.0,  0.0, 1.5},
   // 2nd row
   {-10.0,  0.0, 1.5}, { -7.9, 13.2, 1.2}, { -5.8, 20.8, 1.0}, 
   { -3.8, 25.0, 0.8}, { -1.7, 27.1, 0.6}, { -0.2, 27.6, 0.5},
   {  0.2, 27.6, 0.5}, {  1.7, 27.1, 0.6}, {  3.8, 25.0, 0.8}, 
   {  5.8, 20.8, 1.0}, {  7.9, 13.2, 1.2}, { 10.0,  0.0, 1.5},
   // 3rd row
   {-10.0,  0.0,-1.5}, { -7.9, 13.2,-1.2}, { -5.8, 20.8,-1.0}, 
   { -3.8, 25.0,-0.8}, { -1.7, 27.1,-0.6}, { -0.2, 27.6,-0.5},
   {  0.2, 27.6,-0.5}, {  1.7, 27.1,-0.6}, {  3.8, 25.0,-0.8}, 
   {  5.8, 20.8,-1.0}, {  7.9, 13.2,-1.2}, { 10.0,  0.0,-1.5},
   // 4th row 
   {-13.0,  0.0,-1.5}, {-10.3, 13.7,-1.2}, { -7.6, 21.7,-1.0}, 
   { -5.0, 26.1,-0.8}, { -2.3, 28.2,-0.6}, { -0.3, 28.8,-0.5},
   {  0.3, 28.8,-0.5}, {  2.3, 28.2,-0.6}, {  5.0, 26.1,-0.8}, 
   {  7.6, 21.7,-1.0}, { 10.3, 13.7,-1.2}, { 13.0,  0.0,-1.5},
   // 5th row
   {-13.0,  0.0, 1.5}, {-10.3, 13.7, 1.2}, { -7.6, 21.7, 1.0}, 
   { -5.0, 26.1, 0.8}, { -2.3, 28.2, 0.6}, { -0.3, 28.8, 0.5},
   {  0.3, 28.8, 0.5}, {  2.3, 28.2, 0.6}, {  5.0, 26.1, 0.8}, 
   {  7.6, 21.7, 1.0}, { 10.3, 13.7, 1.2}, { 13.0,  0.0, 1.5}
};

namespace {

obol::Mesh makeArchMesh()
{
    obol::Mesh mesh;
    mesh.topology = obol::MeshTopology::QuadGrid;
    mesh.gridVertexRows = 5;
    mesh.gridVertexColumns = 12;
    for (const auto & position : vertexPositions) {
        mesh.positions.push_back({position[0], position[1], position[2]});
        mesh.indices.push_back(static_cast<uint32_t>(mesh.indices.size()));
    }
    return mesh;
}

obol::Material archMaterial()
{
    obol::Material material;
    material.baseColor = {0.78f, 0.57f, 0.11f, 1.0f};
    return material;
}

bool renderView(obol::OffscreenRenderer & renderer,
                obol::Scene & scene,
                const char * filename,
                const obol::Vec3 & position)
{
    obol::PerspectiveCamera camera;
    camera.position = position;
    camera.target = {0.0f, 14.0f, 0.0f};
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
    keyLight.direction = {-0.5f, -0.8f, -1.0f};
    scene.addDirectionalLight(keyLight);
    obol::DirectionalLight fillLight;
    fillLight.direction = {1.0f, -0.4f, 1.0f};
    fillLight.intensity = 0.35f;
    scene.addDirectionalLight(fillLight);
    scene.addMesh(makeArchMesh(), archMaterial());

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::OffscreenRenderer renderer(backend, target);
    renderer.setBackgroundColor({0.0f, 0.0f, 0.0f, 1.0f});

    const char *baseFilename = (argc > 1) ? argv[1] : "05.4.QuadMesh";
    char filename[256];

    snprintf(filename, sizeof(filename), "%s_front.rgb", baseFilename);
    if (!renderView(renderer, scene, filename, {0.0f, 14.0f, 56.0f})) {
        fprintf(stderr, "Error: Failed to render front QuadMesh view with Obol v2 API\n");
        return 1;
    }

    snprintf(filename, sizeof(filename), "%s_side.rgb", baseFilename);
    if (!renderView(renderer, scene, filename, {56.0f, 14.0f, 0.0f})) {
        fprintf(stderr, "Error: Failed to render side QuadMesh view with Obol v2 API\n");
        return 1;
    }

    snprintf(filename, sizeof(filename), "%s_angle.rgb", baseFilename);
    if (!renderView(renderer, scene, filename, {40.0f, 24.0f, 40.0f})) {
        fprintf(stderr, "Error: Failed to render angled QuadMesh view with Obol v2 API\n");
        return 1;
    }

    printf("Rendered arch QuadMesh views [Obol v2]\n");
    return 0;
}
