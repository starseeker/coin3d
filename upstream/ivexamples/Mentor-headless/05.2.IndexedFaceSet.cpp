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
 * Headless version of Inventor Mentor example 5.2
 * 
 * Original: IndexedFaceSet - creates first stellation of dodecahedron
 * Headless: Renders the stellated dodecahedron from multiple angles
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoMaterialBinding.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <cmath>
#include <cstdio>

// Positions of all of the vertices
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

// Connectivity information; 12 faces with 5 vertices each
static int32_t indices[72] =
{
   1,  2,  3,  4, 5, SO_END_FACE_INDEX, // top face

   0,  1,  8,  7, 3, SO_END_FACE_INDEX, // 5 faces about top
   0,  2,  7,  6, 4, SO_END_FACE_INDEX,
   0,  3,  6, 10, 5, SO_END_FACE_INDEX,
   0,  4, 10,  9, 1, SO_END_FACE_INDEX,
   0,  5,  9,  8, 2, SO_END_FACE_INDEX, 

    9,  5, 4, 6, 11, SO_END_FACE_INDEX, // 5 faces about bottom
   10,  4, 3, 7, 11, SO_END_FACE_INDEX,
    6,  3, 2, 8, 11, SO_END_FACE_INDEX,
    7,  2, 1, 9, 11, SO_END_FACE_INDEX,
    8,  1, 5,10, 11, SO_END_FACE_INDEX,

    6,  7, 8, 9, 10, SO_END_FACE_INDEX, // bottom face
};
 
// Colors for the 12 faces
static const float colors[12][3] =
{
   {1.0, .0, 0}, { .0,  .0, 1.0}, {0, .7,  .7}, { .0, 1.0,  0},
   { .7, .7, 0}, { .7,  .0,  .7}, {0, .0, 1.0}, { .7,  .0, .7},
   { .7, .7, 0}, { .0, 1.0,  .0}, {0, .7,  .7}, {1.0,  .0,  0}
};

SoSeparator *makeStellatedDodecahedron()
{
    SoSeparator *result = new SoSeparator;
    result->ref();

    // Define material bindings - one color per face
    SoMaterialBinding *myBinding = new SoMaterialBinding;
    myBinding->value = SoMaterialBinding::PER_FACE;
    result->addChild(myBinding);

    // Define materials
    SoMaterial *myMaterials = new SoMaterial;
    myMaterials->diffuseColor.setValues(0, 12, colors);
    result->addChild(myMaterials);

    // Define coordinates
    SoCoordinate3 *myCoords = new SoCoordinate3;
    myCoords->point.setValues(0, 12, vertexPositions);
    result->addChild(myCoords);

    // Define the IndexedFaceSet
    SoIndexedFaceSet *myFaceSet = new SoIndexedFaceSet;
    myFaceSet->coordIndex.setValues(0, 72, indices);
    result->addChild(myFaceSet);

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

    root->addChild(makeStellatedDodecahedron());

    // Setup camera
    camera->viewAll(root, SbViewportRegion(DEFAULT_WIDTH, DEFAULT_HEIGHT));

    const char *baseFilename = (argc > 1) ? argv[1] : "05.2.IndexedFaceSet";
    char filename[256];

    // Front view
    snprintf(filename, sizeof(filename), "%s_front.rgb", baseFilename);
    renderToFile(root, filename);

    // Side view
    rotateCamera(camera, M_PI / 2, 0);
    snprintf(filename, sizeof(filename), "%s_side.rgb", baseFilename);
    renderToFile(root, filename);

    // Top view
    camera->viewAll(root, SbViewportRegion(DEFAULT_WIDTH, DEFAULT_HEIGHT));
    rotateCamera(camera, 0, M_PI / 2);
    snprintf(filename, sizeof(filename), "%s_top.rgb", baseFilename);
    renderToFile(root, filename);

    root->unref();
    return 0;
}
