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
 * Headless version of Inventor Mentor example 5.3
 * 
 * Original: TriangleStripSet - creates a pennant-shaped flag
 * Headless: Renders the flag from multiple angles
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cmath>
#include <cstdio>
#include <cstdint>

// Positions of all vertices
static const float vertexPositions[40][3] =
{
   {  0,   12,    0 }, {   0,   15,    0},
   {2.1, 12.1,  -.2 }, { 2.1, 14.6,  -.2},
   {  4, 12.5,  -.7 }, {   4, 14.5,  -.7},
   {4.5, 12.6,  -.8 }, { 4.5, 14.4,  -.8},
   {  5, 12.7,   -1 }, {   5, 14.4,   -1},
   {4.5, 12.8, -1.4 }, { 4.5, 14.6, -1.4},
   {  4, 12.9, -1.6 }, {   4, 14.8, -1.6},
   {3.3, 12.9, -1.8 }, { 3.3, 14.9, -1.8},
   {  3,   13, -2.0 }, {   3, 14.9, -2.0}, 
   {3.3, 13.1, -2.2 }, { 3.3, 15.0, -2.2},
   {  4, 13.2, -2.5 }, {   4, 15.0, -2.5},
   {  6, 13.5, -2.2 }, {   6, 14.8, -2.2},
   {  8, 13.4,   -2 }, {   8, 14.6,   -2},
   { 10, 13.7, -1.8 }, {  10, 14.4, -1.8},
   { 12,   14, -1.3 }, {  12, 14.5, -1.3},
   { 15, 14.9, -1.2 }, {  15,   15, -1.2},

   {-.5, 15,   0 }, { -.5, 0,   0},   // the flagpole
   {  0, 15,  .5 }, {   0, 0,  .5},
   {  0, 15, -.5 }, {   0, 0, -.5},
   {-.5, 15,   0 }, { -.5, 0,   0}
};

// Number of vertices in each strip
static uint32_t numVertices[2] = { 32, 8 };  // flag, pole

// Colors for the strips
static const float colors[2][3] =
{
   { .5, .5,  1 }, // purple flag
   { .4, .4, .4 }, // grey flagpole
};

namespace {

obol::Mesh makePennantMesh()
{
    obol::Mesh mesh;
    mesh.topology = obol::MeshTopology::TriangleStrips;

    for (const auto & position : vertexPositions) {
        mesh.positions.push_back({position[0], position[1], position[2]});
        mesh.indices.push_back(static_cast<uint32_t>(mesh.indices.size()));
    }
    for (uint32_t count : numVertices) {
        mesh.stripVertexCounts.push_back(count);
    }
    for (const auto & color : colors) {
        mesh.faceColors.push_back({color[0], color[1], color[2], 1.0f});
    }

    return mesh;
}

bool renderView(obol::OffscreenRenderer & renderer,
                obol::Scene & scene,
                const char * filename,
                const obol::PerspectiveCamera & camera)
{
    scene.setCamera(camera);
    const obol::FrameResult result = renderer.render(scene);
    return result.success && renderer.writeRGB(filename);
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
                 obol::PerspectiveCamera & sideCamera,
                 obol::PerspectiveCamera & angleCamera)
{
    obol::ViewAllRequest request;
    request.viewportWidth = DEFAULT_WIDTH;
    request.viewportHeight = DEFAULT_HEIGHT;
    frontCamera = obol::CameraFraming::viewAllPerspective(scene, request);
    sideCamera = orbitCamera(frontCamera, static_cast<float>(M_PI / 2.0), 0.0f);
    angleCamera = orbitCamera(frontCamera,
                              static_cast<float>(M_PI / 4.0),
                              static_cast<float>(M_PI / 8.0));
}

} // namespace

int main(int argc, char **argv)
{
    initCoinHeadless();

    obol::Scene scene;
    scene.addDirectionalLight(obol::DirectionalLight{});
    scene.addMesh(makePennantMesh());

    obol::PerspectiveCamera frontCamera;
    obol::PerspectiveCamera sideCamera;
    obol::PerspectiveCamera angleCamera;
    makeCameras(scene, frontCamera, sideCamera, angleCamera);

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::OffscreenRenderer renderer(backend, target);
    renderer.setBackgroundColor({0.0f, 0.0f, 0.0f, 1.0f});

    const char *baseFilename = (argc > 1) ? argv[1] : "05.3.TriangleStripSet";
    char filename[256];

    snprintf(filename, sizeof(filename), "%s_front.rgb", baseFilename);
    if (!renderView(renderer, scene, filename, frontCamera)) {
        fprintf(stderr, "Error: Failed to render front TriangleStripSet view with Obol v2 API\n");
        return 1;
    }

    snprintf(filename, sizeof(filename), "%s_side.rgb", baseFilename);
    if (!renderView(renderer, scene, filename, sideCamera)) {
        fprintf(stderr, "Error: Failed to render side TriangleStripSet view with Obol v2 API\n");
        return 1;
    }

    snprintf(filename, sizeof(filename), "%s_angle.rgb", baseFilename);
    if (!renderView(renderer, scene, filename, angleCamera)) {
        fprintf(stderr, "Error: Failed to render angled TriangleStripSet view with Obol v2 API\n");
        return 1;
    }

    printf("Rendered pennant TriangleStripSet views [Obol v2]\n");
    return 0;
}
