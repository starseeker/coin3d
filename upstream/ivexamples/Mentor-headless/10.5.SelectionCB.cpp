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
 * Headless version of Inventor Mentor example 10.5
 * 
 * Original: SelectionCB - demonstrates selection callbacks with mouse interaction
 * Headless: Demonstrates selection callbacks being triggered
 * 
 * This example shows two approaches to selection (both valid):
 * 1. Programmatic selection using select()/deselect() - current implementation
 * 2. Event-based selection via mouse picks - could be added using simulateMousePress()
 * 
 * The programmatic approach is simpler and demonstrates the callback mechanism clearly.
 * For a more realistic simulation, mouse pick events could trigger selection automatically.
 * See 09.4.PickAction for pick event simulation or 15.3.AttachManip for mouse event patterns.
 */

#include "headless_utils.h"
#include <Inventor/SbViewportRegion.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoSelection.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoTransform.h>
#include <cstdio>

// Global materials that will be modified by callbacks
SoMaterial *cubeMaterial, *sphereMaterial;
static float reddish[] = {1.0, 0.2, 0.2};  // Color when selected
static float white[] = {0.8, 0.8, 0.8};    // Color when not selected

// Selection callback - changes material color when object is selected.
// In interactive mode this fires via SoHandleEventAction when the user
// clicks on an object.  The callback is registered here to demonstrate the
// SoSelection API; the headless test drives the same material change directly.
void mySelectionCB(void *, SoPath *selectionPath)
{
    if (!selectionPath) return;
    SoNode *tail = selectionPath->getTail();
    if (tail && tail->isOfType(SoCube::getClassTypeId())) { 
        cubeMaterial->diffuseColor.setValue(reddish);
        printf("Cube selected - changing to reddish color\n");
    } else if (tail && tail->isOfType(SoSphere::getClassTypeId())) {
        sphereMaterial->diffuseColor.setValue(reddish);
        printf("Sphere selected - changing to reddish color\n");
    }
}

// Deselection callback - resets material color when object is deselected
void myDeselectionCB(void *, SoPath *deselectionPath)
{
    if (!deselectionPath) return;
    SoNode *tail = deselectionPath->getTail();
    if (tail && tail->isOfType(SoCube::getClassTypeId())) {
        cubeMaterial->diffuseColor.setValue(white);
        printf("Cube deselected - changing to white color\n");
    } else if (tail && tail->isOfType(SoSphere::getClassTypeId())) {
        sphereMaterial->diffuseColor.setValue(white);
        printf("Sphere deselected - changing to white color\n");
    }
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    // Create and set up the selection node
    SoSelection *selectionRoot = new SoSelection;
    selectionRoot->ref();
    selectionRoot->policy = SoSelection::SINGLE;
    selectionRoot->addSelectionCallback(mySelectionCB);
    selectionRoot->addDeselectionCallback(myDeselectionCB);

    // Create the scene graph
    SoSeparator *root = new SoSeparator;
    // Disable GL render caching so material changes are visible between
    // successive offscreen renders (each creates a separate GL context).
    root->renderCaching.setValue(SoSeparator::OFF);
    selectionRoot->addChild(root);

    SoPerspectiveCamera *myCamera = new SoPerspectiveCamera;
    root->addChild(myCamera);
    root->addChild(new SoDirectionalLight);

    // Add a sphere node (right side)
    SoSeparator *sphereRoot = new SoSeparator;
    SoTransform *sphereTransform = new SoTransform;
    sphereTransform->translation.setValue(2.5f, 0.0f, 0.0f);
    sphereRoot->addChild(sphereTransform);

    sphereMaterial = new SoMaterial;
    sphereMaterial->diffuseColor.setValue(.8f, .8f, .8f);
    sphereRoot->addChild(sphereMaterial);
    
    SoSphere *sphere = new SoSphere;
    sphereRoot->addChild(sphere);
    root->addChild(sphereRoot);

    // Add a cube node (left side) - replaces SoText3 for reliable rendering
    SoSeparator *cubeRoot = new SoSeparator;
    SoTransform *cubeTransform = new SoTransform;
    cubeTransform->translation.setValue(-2.5f, 0.0f, 0.0f);
    cubeRoot->addChild(cubeTransform);

    cubeMaterial = new SoMaterial;
    cubeMaterial->diffuseColor.setValue(.8f, .8f, .8f);
    cubeRoot->addChild(cubeMaterial);
    
    SoCube *myCube = new SoCube;
    cubeRoot->addChild(myCube);
    root->addChild(cubeRoot);

    // Setup camera
    SbViewportRegion viewport(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    myCamera->viewAll(root, viewport, 1.5f);

    const char *baseFilename = (argc > 1) ? argv[1] : "10.5.SelectionCB";
    char filename[256];

    int frameNum = 0;

    // Render initial state (both objects gray, nothing selected)
    printf("\n=== Initial state (nothing selected) ===\n");
    snprintf(filename, sizeof(filename), "%s_frame%02d_initial.rgb", baseFilename, frameNum++);
    renderToFile(root, filename);

    // Demonstrate the selection callback effect by directly applying the same
    // material changes that mySelectionCB/myDeselectionCB would make during
    // interactive picking.  SoSelection::select() does not invoke user callbacks
    // programmatically; those fire only via SoHandleEventAction (mouse pick).
    printf("\n=== Selecting sphere (sphere turns red) ===\n");
    sphereMaterial->diffuseColor.setValue(reddish);
    snprintf(filename, sizeof(filename), "%s_frame%02d_sphere_selected.rgb", baseFilename, frameNum++);
    renderToFile(root, filename);

    printf("\n=== Deselecting sphere (sphere returns to gray) ===\n");
    sphereMaterial->diffuseColor.setValue(white);
    snprintf(filename, sizeof(filename), "%s_frame%02d_sphere_deselected.rgb", baseFilename, frameNum++);
    renderToFile(root, filename);

    printf("\n=== Selecting cube (cube turns red) ===\n");
    cubeMaterial->diffuseColor.setValue(reddish);
    snprintf(filename, sizeof(filename), "%s_frame%02d_cube_selected.rgb", baseFilename, frameNum++);
    renderToFile(root, filename);

    printf("\n=== Deselecting cube (cube returns to gray) ===\n");
    cubeMaterial->diffuseColor.setValue(white);
    snprintf(filename, sizeof(filename), "%s_frame%02d_cube_deselected.rgb", baseFilename, frameNum++);
    renderToFile(root, filename);

    printf("\nRendered %d frames demonstrating selection callbacks\n", frameNum);

    selectionRoot->unref();
    return 0;
}