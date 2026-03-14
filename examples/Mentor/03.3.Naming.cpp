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
 * Headless version of Inventor Mentor example 3.3
 * 
 * Original: Naming - demonstrates named nodes and node lookup
 * Headless: Creates scene with named nodes, performs lookup, and renders result
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <cstdio>

// Function prototype
void RemoveCube();

int main(int argc, char **argv)
{
    // Initialize Coin for headless operation
    initCoinHeadless();

    // Create some objects and give them names:
    SoSeparator *root = new SoSeparator;
    root->ref();
    root->setName("Root");

    // Add camera and light for rendering
    SoPerspectiveCamera *camera = new SoPerspectiveCamera;
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);

    // Add material for the cube
    SoMaterial *cubeMaterial = new SoMaterial;
    cubeMaterial->diffuseColor.setValue(1.0, 0.5, 0.0);  // Orange
    root->addChild(cubeMaterial);

    SoCube *myCube = new SoCube;
    root->addChild(myCube);
    myCube->setName("MyCube");

    // Add material for the sphere
    SoMaterial *sphereMaterial = new SoMaterial;
    sphereMaterial->diffuseColor.setValue(0.0, 0.5, 1.0);  // Blue
    root->addChild(sphereMaterial);

    SoSphere *mySphere = new SoSphere;
    root->addChild(mySphere);
    mySphere->setName("MySphere");

    // Setup camera to view all
    camera->viewAll(root, SbViewportRegion(DEFAULT_WIDTH, DEFAULT_HEIGHT));

    // Render with both cube and sphere
    const char *baseFilename = (argc > 1) ? argv[1] : "03.3.Naming";
    char filename[256];
    snprintf(filename, sizeof(filename), "%s_before.rgb", baseFilename);
    renderToFile(root, filename);

    // Remove the cube (demonstrates name lookup)
    RemoveCube();

    // Render with only sphere
    snprintf(filename, sizeof(filename), "%s_after.rgb", baseFilename);
    renderToFile(root, filename);

    printf("Demonstrated named node lookup and removal\n");

    root->unref();
    return 0;
}

void RemoveCube()
{
    // Remove the cube named 'MyCube' from the separator named 'Root'.
    SoSeparator *myRoot;
    myRoot = (SoSeparator *)SoNode::getByName("Root");

    SoCube *myCube;
    myCube = (SoCube *)SoNode::getByName("MyCube");

    if (myRoot && myCube) {
        myRoot->removeChild(myCube);
        printf("Removed cube named 'MyCube' from scene\n");
    }
}
