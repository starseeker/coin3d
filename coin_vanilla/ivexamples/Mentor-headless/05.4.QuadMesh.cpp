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
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoQuadMesh.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <cmath>
#include <cstdio>

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

SoSeparator *makeArch()
{
    SoSeparator *result = new SoSeparator;
    result->ref();

    // Define the material
    SoMaterial *myMaterial = new SoMaterial;
    myMaterial->diffuseColor.setValue(.78, .57, .11);
    result->addChild(myMaterial);

    // Define coordinates for vertices
    SoCoordinate3 *myCoords = new SoCoordinate3;
    myCoords->point.setValues(0, 60, vertexPositions);
    result->addChild(myCoords);

    // Define the QuadMesh
    SoQuadMesh *myQuadMesh = new SoQuadMesh;
    myQuadMesh->verticesPerRow = 12;
    myQuadMesh->verticesPerColumn = 5;
    result->addChild(myQuadMesh);

    result->unrefNoDelete();
    return result;
}

int main(int argc, char **argv)
{
    // Initialize Coin for headless operation
    initCoinHeadless();

    SoSeparator *root = new SoSeparator;
    root->ref();

    // Add camera and light
    SoPerspectiveCamera *camera = new SoPerspectiveCamera;
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);

    root->addChild(makeArch());

    // Setup camera
    camera->viewAll(root, SbViewportRegion(DEFAULT_WIDTH, DEFAULT_HEIGHT));

    const char *baseFilename = (argc > 1) ? argv[1] : "05.4.QuadMesh";
    char filename[256];

    // Front view
    snprintf(filename, sizeof(filename), "%s_front.rgb", baseFilename);
    renderToFile(root, filename);

    // Side view
    rotateCamera(camera, M_PI / 2, 0);
    snprintf(filename, sizeof(filename), "%s_side.rgb", baseFilename);
    renderToFile(root, filename);

    // Angled view
    camera->viewAll(root, SbViewportRegion(DEFAULT_WIDTH, DEFAULT_HEIGHT));
    rotateCamera(camera, M_PI / 4, M_PI / 8);
    snprintf(filename, sizeof(filename), "%s_angle.rgb", baseFilename);
    renderToFile(root, filename);

    root->unref();
    return 0;
}
