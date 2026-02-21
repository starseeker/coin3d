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
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoFaceSet.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoNormal.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <cmath>
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
static int32_t numvertices[8] = {3, 3, 3, 3, 4, 4, 4, 4};

// Normals for each polygon (recalculated for the scaled vertex positions)
static const float norms[8][3] =
{ 
   {0, .8f,  .6f}, {-.6f, .8f, 0}, //front, left tris
   {0, .8f, -.6f}, { .6f, .8f, 0}, //rear, right tris
   
   {0, .1466f,  .9892f}, {-.9892f, .1466f, 0},//front, left quads
   {0, .1466f, -.9892f}, { .9892f, .1466f, 0},//rear, right quads
};

SoSeparator *makeObeliskFaceSet()
{
    SoSeparator *obelisk = new SoSeparator();
    obelisk->ref();

    // Define the normals
    SoNormal *myNormals = new SoNormal;
    myNormals->vector.setValues(0, 8, norms);
    obelisk->addChild(myNormals);
    
    SoNormalBinding *myNormalBinding = new SoNormalBinding;
    myNormalBinding->value = SoNormalBinding::PER_FACE;
    obelisk->addChild(myNormalBinding);

    // Define material for obelisk (warm sandstone tone for good contrast)
    SoMaterial *myMaterial = new SoMaterial;
    myMaterial->diffuseColor.setValue(.75f, .60f, .35f);
    obelisk->addChild(myMaterial);

    // Define coordinates for vertices
    SoCoordinate3 *myCoords = new SoCoordinate3;
    myCoords->point.setValues(0, 28, vertices);
    obelisk->addChild(myCoords);

    // Define the FaceSet
    SoFaceSet *myFaceSet = new SoFaceSet;
    myFaceSet->numVertices.setValues(0, 8, numvertices);
    obelisk->addChild(myFaceSet);

    obelisk->unrefNoDelete();
    return obelisk;
}

int main(int argc, char **argv)
{
    // Initialize Coin for headless operation
    initCoinHeadless();

    SoSeparator *root = new SoSeparator;
    root->ref();

    // Add camera and two lights (key + fill) so no face is completely black
    SoPerspectiveCamera *camera = new SoPerspectiveCamera;
    root->addChild(camera);
    SoDirectionalLight *keyLight = new SoDirectionalLight;
    keyLight->direction.setValue(-1, -1, -1);
    root->addChild(keyLight);
    SoDirectionalLight *fillLight = new SoDirectionalLight;
    fillLight->direction.setValue(1, 0.5, 1);
    fillLight->intensity = 0.4f;
    root->addChild(fillLight);

    root->addChild(makeObeliskFaceSet());

    // Setup camera
    camera->viewAll(root, SbViewportRegion(DEFAULT_WIDTH, DEFAULT_HEIGHT));

    const char *baseFilename = (argc > 1) ? argv[1] : "05.1.FaceSet";
    char filename[256];

    // Front view – slight elevation so the apex is visible
    camera->viewAll(root, SbViewportRegion(DEFAULT_WIDTH, DEFAULT_HEIGHT));
    rotateCamera(camera, 0, M_PI / 8);
    snprintf(filename, sizeof(filename), "%s_front.rgb", baseFilename);
    renderToFile(root, filename);

    // Side view – 3/4 angle from the left with elevation
    camera->viewAll(root, SbViewportRegion(DEFAULT_WIDTH, DEFAULT_HEIGHT));
    rotateCamera(camera, M_PI / 3, M_PI / 8);
    snprintf(filename, sizeof(filename), "%s_side.rgb", baseFilename);
    renderToFile(root, filename);

    // Angled view – isometric-like vantage from above-left
    camera->viewAll(root, SbViewportRegion(DEFAULT_WIDTH, DEFAULT_HEIGHT));
    rotateCamera(camera, M_PI / 4, M_PI / 5);
    snprintf(filename, sizeof(filename), "%s_angle.rgb", baseFilename);
    renderToFile(root, filename);

    root->unref();
    return 0;
}
