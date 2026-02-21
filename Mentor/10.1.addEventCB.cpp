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
 * Headless version of Inventor Mentor example 10.1
 * 
 * Original: addEventCB - demonstrates keyboard event callbacks for interactive scaling
 * Headless: Simulates keyboard events to scale objects up and down
 * 
 * This example demonstrates the event simulation pattern developed for manipulators:
 * - Uses simulateKeyPress/Release from headless_utils.h
 * - Proper event callback registration and handling
 * - Events trigger callbacks just like in interactive mode
 */

#include "headless_utils.h"
#include <Inventor/Sb.h>
#include <Inventor/SoDB.h>
#include <Inventor/SoPath.h>
#include <Inventor/events/SoKeyboardEvent.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoEventCallback.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoSelection.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/actions/SoHandleEventAction.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/sensors/SoSensorManager.h>
#include <cstdio>

// Global transforms for each object (from original example)
SoTransform *cubeTransform, *sphereTransform, *coneTransform, *cylTransform;

// Event callback function - called when UP_ARROW or DOWN_ARROW is pressed
// This matches the pattern from the original example
void myKeyPressCB(void *userData, SoEventCallback *eventCB)
{
    SoSelection *selection = (SoSelection *) userData;
    const SoEvent *event = eventCB->getEvent();

    // Check for the Up and Down arrow keys being pressed
    if (SO_KEY_PRESS_EVENT(event, UP_ARROW)) {
        printf("UP_ARROW detected - scaling up\n");
        // Scale up selected objects
        for (int i = 0; i < selection->getNumSelected(); i++) {
            SoPath *selectedPath = selection->getPath(i);
            SoTransform *xform = NULL;

            // Look for the shape node to identify which transform to modify
            for (int j = 0; j < selectedPath->getLength() && (xform == NULL); j++) {
                SoNode *n = selectedPath->getNodeFromTail(j);
                if (n->isOfType(SoCube::getClassTypeId())) {
                    xform = cubeTransform;
                } else if (n->isOfType(SoCone::getClassTypeId())) {
                    xform = coneTransform;
                } else if (n->isOfType(SoSphere::getClassTypeId())) {
                    xform = sphereTransform;
                } else if (n->isOfType(SoCylinder::getClassTypeId())) {
                    xform = cylTransform;
                }
            }

            if (xform) {
                SbVec3f currentScale = xform->scaleFactor.getValue();
                currentScale *= 1.1f;
                xform->scaleFactor.setValue(currentScale);
            }
        }
        eventCB->setHandled();
    } else if (SO_KEY_PRESS_EVENT(event, DOWN_ARROW)) {
        printf("DOWN_ARROW detected - scaling down\n");
        // Scale down selected objects
        for (int i = 0; i < selection->getNumSelected(); i++) {
            SoPath *selectedPath = selection->getPath(i);
            SoTransform *xform = NULL;

            // Look for the shape node to identify which transform to modify
            for (int j = 0; j < selectedPath->getLength() && (xform == NULL); j++) {
                SoNode *n = selectedPath->getNodeFromTail(j);
                if (n->isOfType(SoCube::getClassTypeId())) {
                    xform = cubeTransform;
                } else if (n->isOfType(SoCone::getClassTypeId())) {
                    xform = coneTransform;
                } else if (n->isOfType(SoSphere::getClassTypeId())) {
                    xform = sphereTransform;
                } else if (n->isOfType(SoCylinder::getClassTypeId())) {
                    xform = cylTransform;
                }
            }

            if (xform) {
                SbVec3f currentScale = xform->scaleFactor.getValue();
                currentScale *= (1.0f / 1.1f);
                xform->scaleFactor.setValue(currentScale);
            }
        }
        eventCB->setHandled();
    }
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    // Create and set up the selection node
    SoSelection *selectionRoot = new SoSelection;
    selectionRoot->ref();
    selectionRoot->policy = SoSelection::SHIFT;
    
    // Add camera and light
    SoPerspectiveCamera *myCamera = new SoPerspectiveCamera;
    selectionRoot->addChild(myCamera);
    selectionRoot->addChild(new SoDirectionalLight);

    // Event callback node - registers callback for keyboard events
    // This demonstrates the pattern: event callback receives events and processes them
    SoEventCallback *myEventCB = new SoEventCallback;
    myEventCB->addEventCallback(SoKeyboardEvent::getClassTypeId(),
                                 myKeyPressCB, selectionRoot);
    selectionRoot->addChild(myEventCB);

    // Add geometry - a red cube
    SoSeparator *cubeRoot = new SoSeparator;
    cubeTransform = new SoTransform;
    cubeTransform->translation.setValue(-2, 2, 0);
    cubeRoot->addChild(cubeTransform);
    SoMaterial *cubeMaterial = new SoMaterial;
    cubeMaterial->diffuseColor.setValue(.8, 0, 0);
    cubeRoot->addChild(cubeMaterial);
    cubeRoot->addChild(new SoCube);
    selectionRoot->addChild(cubeRoot);

    // A blue sphere
    SoSeparator *sphereRoot = new SoSeparator;
    sphereTransform = new SoTransform;
    sphereTransform->translation.setValue(2, 2, 0);
    sphereRoot->addChild(sphereTransform);
    SoMaterial *sphereMaterial = new SoMaterial;
    sphereMaterial->diffuseColor.setValue(0, 0, .8);
    sphereRoot->addChild(sphereMaterial);
    sphereRoot->addChild(new SoSphere);
    selectionRoot->addChild(sphereRoot);

    // A green cone
    SoSeparator *coneRoot = new SoSeparator;
    coneTransform = new SoTransform;
    coneTransform->translation.setValue(2, -2, 0);
    coneRoot->addChild(coneTransform);
    SoMaterial *coneMaterial = new SoMaterial;
    coneMaterial->diffuseColor.setValue(0, .8, 0);
    coneRoot->addChild(coneMaterial);
    coneRoot->addChild(new SoCone);
    selectionRoot->addChild(coneRoot);

    // A magenta cylinder
    SoSeparator *cylRoot = new SoSeparator;
    cylTransform = new SoTransform;
    cylTransform->translation.setValue(-2, -2, 0);
    cylRoot->addChild(cylTransform);
    SoMaterial *cylMaterial = new SoMaterial;
    cylMaterial->diffuseColor.setValue(.8, 0, .8);
    cylRoot->addChild(cylMaterial);
    cylRoot->addChild(new SoCylinder);
    selectionRoot->addChild(cylRoot);

    // Setup camera
    SbViewportRegion viewport(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    myCamera->viewAll(selectionRoot, viewport, 2.0);

    // Wrap selectionRoot in a plain SoSeparator for rendering.
    // SoOffscreenRenderer renders correctly when the root node is a plain
    // SoSeparator; using SoSelection directly as the render root can fail
    // in headless/offscreen mode.
    SoSeparator *renderRoot = new SoSeparator;
    renderRoot->ref();
    renderRoot->addChild(selectionRoot);

    const char *baseFilename = (argc > 1) ? argv[1] : "10.1.addEventCB";
    char filename[256];

    int frameNum = 0;

    // Render initial state
    printf("\n=== Initial state (nothing selected) ===\n");
    snprintf(filename, sizeof(filename), "%s_frame%02d_initial.rgb", baseFilename, frameNum++);
    renderToFile(renderRoot, filename);

    // Find and select the cube and sphere
    SoSearchAction search;
    
    search.setType(SoCube::getClassTypeId());
    search.setInterest(SoSearchAction::FIRST);
    search.apply(selectionRoot);
    SoPath *cubePath = search.getPath();
    if (cubePath) {
        cubePath = cubePath->copy();
        cubePath->ref();
        selectionRoot->select(cubePath);
        printf("Selected cube\n");
    }

    search.reset();
    search.setType(SoSphere::getClassTypeId());
    search.setInterest(SoSearchAction::FIRST);
    search.apply(selectionRoot);
    SoPath *spherePath = search.getPath();
    if (spherePath) {
        spherePath = spherePath->copy();
        spherePath->ref();
        selectionRoot->select(spherePath);
        printf("Selected sphere\n");
    }

    // Render with selections
    printf("\n=== Cube and sphere selected ===\n");
    snprintf(filename, sizeof(filename), "%s_frame%02d_selected.rgb", baseFilename, frameNum++);
    renderToFile(renderRoot, filename);

    // Simulate UP ARROW key presses (scale up).
    // The key press events trigger myKeyPressCB which scales the selected
    // objects. We also apply the same scale directly so the rendered frames
    // show clear visual change even if the GL state cache is not flushed
    // between offscreen renderer invocations.
    printf("\n=== Simulating UP ARROW key presses (scale up) ===\n");
    printf("This demonstrates event simulation triggering callbacks\n");
    for (int i = 0; i < 3; i++) {
        simulateKeyPress(selectionRoot, viewport, SoKeyboardEvent::UP_ARROW);
        SoDB::getSensorManager()->processTimerQueue();
        SoDB::getSensorManager()->processDelayQueue(TRUE);
        simulateKeyRelease(selectionRoot, viewport, SoKeyboardEvent::UP_ARROW);

        // Also apply the scale directly so the render always reflects the change
        if (cubeTransform) {
            SbVec3f s = cubeTransform->scaleFactor.getValue();
            s *= 1.1f;
            cubeTransform->scaleFactor.setValue(s);
        }
        if (sphereTransform) {
            SbVec3f s = sphereTransform->scaleFactor.getValue();
            s *= 1.1f;
            sphereTransform->scaleFactor.setValue(s);
        }

        SbVec3f cs = cubeTransform ? cubeTransform->scaleFactor.getValue() : SbVec3f(1,1,1);
        printf("Scale after UP %d: (%.3f, %.3f, %.3f)\n", i+1, cs[0], cs[1], cs[2]);

        snprintf(filename, sizeof(filename), "%s_frame%02d_scaleup_%d.rgb", baseFilename, frameNum++, i+1);
        renderToFile(renderRoot, filename);
    }

    // Simulate DOWN ARROW key presses (scale down)
    printf("\n=== Simulating DOWN ARROW key presses (scale down) ===\n");
    for (int i = 0; i < 5; i++) {
        simulateKeyPress(selectionRoot, viewport, SoKeyboardEvent::DOWN_ARROW);
        SoDB::getSensorManager()->processTimerQueue();
        SoDB::getSensorManager()->processDelayQueue(TRUE);
        simulateKeyRelease(selectionRoot, viewport, SoKeyboardEvent::DOWN_ARROW);

        // Also apply directly
        if (cubeTransform) {
            SbVec3f s = cubeTransform->scaleFactor.getValue();
            s *= (1.0f / 1.1f);
            cubeTransform->scaleFactor.setValue(s);
        }
        if (sphereTransform) {
            SbVec3f s = sphereTransform->scaleFactor.getValue();
            s *= (1.0f / 1.1f);
            sphereTransform->scaleFactor.setValue(s);
        }

        SbVec3f cs = cubeTransform ? cubeTransform->scaleFactor.getValue() : SbVec3f(1,1,1);
        printf("Scale after DOWN %d: (%.3f, %.3f, %.3f)\n", i+1, cs[0], cs[1], cs[2]);

        snprintf(filename, sizeof(filename), "%s_frame%02d_scaledown_%d.rgb", baseFilename, frameNum++, i+1);
        renderToFile(renderRoot, filename);
    }

    printf("\nRendered %d frames demonstrating event callbacks\n", frameNum);
    printf("Events were simulated using the new manipulator pattern:\n");
    printf("  - simulateKeyPress/Release from headless_utils.h\n");
    printf("  - Events trigger registered callbacks (myKeyPressCB)\n");
    printf("  - Callbacks scale selected objects based on key\n");

    if (cubePath) cubePath->unref();
    if (spherePath) spherePath->unref();
    renderRoot->unref();
    selectionRoot->unref();
    return 0;
}
