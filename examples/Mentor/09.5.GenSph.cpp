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
#include <Inventor/SoDB.h>
#include <Inventor/SoPrimitiveVertex.h>
#include <Inventor/actions/SoCallbackAction.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSphere.h>
#include <cstdio>

// Global counter for triangles
static int triangleCount = 0;

SoCallbackAction::Response printHeaderCallback(void *, 
    SoCallbackAction *, const SoNode *node)
{
    printf("\nSphere ");
    if (node->getName().getLength() > 0)
        printf("named \"%s\" ", node->getName().getString());
    printf("at address %p\n", node);
    triangleCount = 0;
    return SoCallbackAction::CONTINUE;
}

void printTriangleCallback(void *, SoCallbackAction *,
    const SoPrimitiveVertex *vertex1,
    const SoPrimitiveVertex *vertex2,
    const SoPrimitiveVertex *vertex3)
{
    triangleCount++;
    
    // Print only first few triangles to avoid overwhelming output
    if (triangleCount <= 3) {
        printf("  Triangle %d:\n", triangleCount);
        printf("    v1: (%.2f, %.2f, %.2f)\n", 
            vertex1->getPoint()[0],
            vertex1->getPoint()[1],
            vertex1->getPoint()[2]);
        printf("    v2: (%.2f, %.2f, %.2f)\n",
            vertex2->getPoint()[0],
            vertex2->getPoint()[1],
            vertex2->getPoint()[2]);
        printf("    v3: (%.2f, %.2f, %.2f)\n",
            vertex3->getPoint()[0],
            vertex3->getPoint()[1],
            vertex3->getPoint()[2]);
    }
}

void printSpheres(SoNode *root)
{
    SoCallbackAction myAction;
    myAction.addPreCallback(SoSphere::getClassTypeId(), 
        printHeaderCallback, NULL);
    myAction.addTriangleCallback(SoSphere::getClassTypeId(), 
        printTriangleCallback, NULL);
    myAction.apply(root);
    
    printf("  Total triangles generated: %d\n", triangleCount);
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

    // Create a named sphere
    SoSphere *mySphere = new SoSphere;
    mySphere->setName("TestSphere");
    
    SoMaterial *myMaterial = new SoMaterial;
    myMaterial->diffuseColor.setValue(0.8, 0.2, 0.2);
    root->addChild(myMaterial);
    root->addChild(mySphere);

    // Setup camera
    camera->viewAll(root, SbViewportRegion(DEFAULT_WIDTH, DEFAULT_HEIGHT));

    // Use callback action to print generated primitives
    printf("Generating primitives for sphere...\n");
    printSpheres(root);

    const char *baseFilename = (argc > 1) ? argv[1] : "09.5.GenSph";
    char filename[256];
    
    // Render the sphere
    snprintf(filename, sizeof(filename), "%s.rgb", baseFilename);
    renderToFile(root, filename);

    root->unref();
    return 0;
}
