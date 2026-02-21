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

// Eight polygons. The first four are triangles, the second four are quadrilaterals
static const float vertices[28][3] =
{
   { 0, 30, 0}, {-2,27, 2}, { 2,27, 2},            //front tri
   { 0, 30, 0}, {-2,27,-2}, {-2,27, 2},            //left  tri
   { 0, 30, 0}, { 2,27,-2}, {-2,27,-2},            //rear  tri
   { 0, 30, 0}, { 2,27, 2}, { 2,27,-2},            //right tri
   {-2, 27, 2}, {-4,0, 4}, { 4,0, 4}, { 2,27, 2},  //front quad
   {-2, 27,-2}, {-4,0,-4}, {-4,0, 4}, {-2,27, 2},  //left  quad
   { 2, 27,-2}, { 4,0,-4}, {-4,0,-4}, {-2,27,-2},  //rear  quad
   { 2, 27, 2}, { 4,0, 4}, { 4,0,-4}, { 2,27,-2}   //right quad
};

// Number of vertices in each polygon
static int32_t numvertices[8] = {3, 3, 3, 3, 4, 4, 4, 4};

// Normals for each polygon
static const float norms[8][3] =
{ 
   {0, .555,  .832}, {-.832, .555, 0}, //front, left tris
   {0, .555, -.832}, { .832, .555, 0}, //rear, right tris
   
   {0, .0739,  .9973}, {-.9972, .0739, 0},//front, left quads
   {0, .0739, -.9973}, { .9972, .0739, 0},//rear, right quads
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

    // Define material for obelisk
    SoMaterial *myMaterial = new SoMaterial;
    myMaterial->diffuseColor.setValue(.4, .4, .4);
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

    // Add camera and light
    SoPerspectiveCamera *camera = new SoPerspectiveCamera;
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);

    root->addChild(makeObeliskFaceSet());

    // Setup camera
    camera->viewAll(root, SbViewportRegion(DEFAULT_WIDTH, DEFAULT_HEIGHT));

    const char *baseFilename = (argc > 1) ? argv[1] : "05.1.FaceSet";
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
    rotateCamera(camera, M_PI / 4, M_PI / 6);
    snprintf(filename, sizeof(filename), "%s_angle.rgb", baseFilename);
    renderToFile(root, filename);

    root->unref();
    return 0;
}
