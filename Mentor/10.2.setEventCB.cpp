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
 * Headless version of Inventor Mentor example 10.2
 * 
 * Original: setEventCB - RenderArea event callback (Xt-specific)
 * Headless: Demonstrates generic event translation pattern
 * 
 * This example demonstrates:
 * - How toolkits translate native events to SoEvent (toolkit-agnostic pattern)
 * - Application event callbacks that intercept events before scene graph
 * - Mouse event handling (button press/release/drag)
 * - The minimal event interface ANY toolkit must provide
 * 
 * Key insight: The event handling logic in Coin is toolkit-independent.
 * A toolkit must:
 * 1. Capture native events (X11, Win32, etc.)
 * 2. Translate to SoEvent (position, button, state)
 * 3. Either apply to scene graph OR call application callback
 * 4. Trigger redraw if event was handled
 * 
 * Original used XButtonEvent, XMotionEvent - Xt-specific types
 * Mock version shows the GENERIC pattern any toolkit follows
 */

#include "headless_utils.h"
#include "mock_gui_toolkit.h"
#include <Inventor/Sb.h>
#include <Inventor/nodes/SoCamera.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/nodes/SoLightModel.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoPointSet.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/sensors/SoTimerSensor.h>
#include <Inventor/events/SoMouseButtonEvent.h>
#include <Inventor/events/SoLocation2Event.h>
#include <cstdio>
#include <cmath>

// Timer sensor for camera rotation
SoTimerSensor *myTicker;
#define UPDATE_RATE 1.0/30.0
#define ROTATION_ANGLE M_PI/60.0

// Camera rotation state
bool rotating = false;

// Global pointers to the coordinate and point set nodes
static SoCoordinate3 *g_myCoord = NULL;
static SoPointSet    *g_myPointSet = NULL;

// Project mouse position to 3D point
void myProjectPoint(MockRenderArea *myRenderArea, 
   int mousex, int mousey, SbVec3f &intersection)
{
    // Take the x,y position of mouse, and normalize to [0,1].
    SbVec2s size = myRenderArea->getSize();
    float x = float(mousex) / size[0];
    float y = float(mousey) / size[1];
    
    // Get the camera and view volume
    SoGroup *root = (SoGroup *) myRenderArea->getSceneGraph();
    SoCamera *myCamera = (SoCamera *) root->getChild(0);
    SbViewVolume myViewVolume;
    myViewVolume = myCamera->getViewVolume();
    
    // Project the mouse point to a line
    SbVec3f p0, p1;
    myViewVolume.projectPointToLine(SbVec2f(x,y), p0, p1);
    
    // Midpoint of the line intersects a plane thru the origin
    intersection = (p0 + p1) / 2.0f;
}

// Add point to the point set
void myAddPoint(MockRenderArea *myRenderArea, const SbVec3f point)
{
    g_myCoord->point.set1Value(g_myCoord->point.getNum(), point);
    g_myPointSet->numPoints.setValue(g_myCoord->point.getNum());
}

// Clear all points
void myClearPoints(MockRenderArea *myRenderArea)
{
    g_myCoord->point.deleteValues(0); 
    g_myPointSet->numPoints.setValue(0);
}

// Timer callback for camera rotation
void tickerCallback(void *userData, SoSensor *)
{
    SoCamera *myCamera = (SoCamera *) userData;
    SbRotation rot;
    SbMatrix mtx;
    SbVec3f pos;
    
    // Adjust the position
    pos = myCamera->position.getValue();
    rot = SbRotation(SbVec3f(0,1,0), (float)ROTATION_ANGLE);
    mtx.setRotate(rot);
    mtx.multVecMatrix(pos, pos);
    myCamera->position.setValue(pos);
    
    // Adjust the orientation
    myCamera->orientation.setValue(
             myCamera->orientation.getValue() * rot);
}

// Application event handler
// This is the key function - it receives events INSTEAD of the scene graph
// In original: receives XAnyEvent* from Xt
// In mock: receives SoEvent* (already translated from native events)
SbBool myAppEventHandler(void *userData, void *eventPtr)
{
    MockRenderArea *myRenderArea = (MockRenderArea *) userData;
    const SoEvent *event = (const SoEvent*)eventPtr;
    SbVec3f vec;
    SbBool handled = TRUE;
    
    // Check event type and handle appropriately
    if (event->isOfType(SoMouseButtonEvent::getClassTypeId())) {
        const SoMouseButtonEvent *buttonEvent = (const SoMouseButtonEvent*)event;
        SbVec2s pos = buttonEvent->getPosition();
        
        if (buttonEvent->getState() == SoButtonEvent::DOWN) {
            // Button press
            if (buttonEvent->getButton() == SoMouseButtonEvent::BUTTON1) {
                printf("LEFT button pressed at (%d, %d) - adding point\n", pos[0], pos[1]);
                myProjectPoint(myRenderArea, pos[0], pos[1], vec);
                myAddPoint(myRenderArea, vec);
            } else if (buttonEvent->getButton() == SoMouseButtonEvent::BUTTON2) {
                printf("MIDDLE button pressed - starting rotation\n");
                rotating = true;
                myTicker->schedule();
            } else if (buttonEvent->getButton() == SoMouseButtonEvent::BUTTON3) {
                printf("RIGHT button pressed - clearing points\n");
                myClearPoints(myRenderArea);
            }
        } else {
            // Button release
            if (buttonEvent->getButton() == SoMouseButtonEvent::BUTTON2) {
                printf("MIDDLE button released - stopping rotation\n");
                rotating = false;
                myTicker->unschedule();
            }
        }
    } else if (event->isOfType(SoLocation2Event::getClassTypeId())) {
        const SoLocation2Event *motionEvent = (const SoLocation2Event*)event;
        SbVec2s pos = motionEvent->getPosition();
        
        // Check if button 1 is held during motion (dragging)
        // Note: In real implementation, would check button state from event
        // For simulation, we'll handle this in the test sequence
        // printf("Motion at (%d, %d)\n", pos[0], pos[1]);
    } else {
        handled = FALSE;
    }
    
    return handled;
}

int main(int argc, char **argv)
{
    printf("=== Mentor Example 10.2: RenderArea Event Callback ===\n");
    printf("This demonstrates toolkit-agnostic event translation pattern\n");
    printf("\nOriginal used Xt-specific XButtonEvent, XMotionEvent\n");
    printf("This version shows the GENERIC pattern for any toolkit\n\n");
    
    // Initialize Coin
    initCoinHeadless();
    
    // Mock toolkit initialization
    void* mockWindow = mockToolkitInit(argv[0]);
    if (!mockWindow) {
        fprintf(stderr, "Failed to initialize mock toolkit\n");
        return 1;
    }

    // Create and set up the root node
    SoSeparator *root = new SoSeparator;
    root->ref();

    // Add a camera
    SoPerspectiveCamera *myCamera = new SoPerspectiveCamera;
    root->addChild(myCamera);  // child 0
    
    // Use the base color light model
    SoLightModel *myLightModel = new SoLightModel;
    myLightModel->model = SoLightModel::BASE_COLOR;
    root->addChild(myLightModel);  // child 1
    
    // Set up the camera view volume
    myCamera->position.setValue(0, 0, 4);
    myCamera->nearDistance.setValue(1.0f);
    myCamera->farDistance.setValue(7.0f);
    myCamera->heightAngle.setValue(float(M_PI)/3.0f);

    // Add a background sphere so the initial scene is not blank.
    // The sphere is translated behind the projected click points (which land at z≈0)
    // so that clicked points always appear in front of the sphere.
    // A sub-separator isolates the sphere's material from the point rendering.
    SoSeparator *bgSep = new SoSeparator;
    SoMaterial  *bgMtl = new SoMaterial;
    bgMtl->diffuseColor.setValue(0.4f, 0.6f, 0.8f);  // steel blue
    bgSep->addChild(bgMtl);
    SoTranslation *bgTrans = new SoTranslation;
    bgTrans->translation.setValue(0.0f, 0.0f, -2.0f);  // behind projected points
    bgSep->addChild(bgTrans);
    SoSphere *bgSphere = new SoSphere;
    bgSphere->radius.setValue(1.5f);
    bgSep->addChild(bgSphere);
    root->addChild(bgSep);  // child 2

    // Bright yellow material and larger point size for the point set
    SoMaterial *pointMtl = new SoMaterial;
    pointMtl->diffuseColor.setValue(1.0f, 1.0f, 0.0f);  // yellow
    root->addChild(pointMtl);  // child 3

    SoDrawStyle *pointStyle = new SoDrawStyle;
    pointStyle->pointSize.setValue(6.0f);
    root->addChild(pointStyle);  // child 4

    // Add a coordinate and point set
    SoCoordinate3 *myCoord = new SoCoordinate3;
    SoPointSet *myPointSet = new SoPointSet;
    myPointSet->numPoints.setValue(0);  // start with no points rendered
    g_myCoord    = myCoord;
    g_myPointSet = myPointSet;
    root->addChild(myCoord);     // child 5
    root->addChild(myPointSet);  // child 6

    // Timer sensor for camera rotation
    myTicker = new SoTimerSensor(tickerCallback, myCamera);
    myTicker->setInterval(SbTime(UPDATE_RATE));

    // Create a render area
    MockRenderArea *myRenderArea = new MockRenderArea(800, 600);
    myRenderArea->setSceneGraph(root);
    myRenderArea->setTitle("Event Handler Demo");

    // Set event callback - events go to application instead of scene graph
    // This is the KEY pattern: toolkit sends events to callback instead of scene
    printf("Setting event callback - events will go to app handler\n");
    myRenderArea->setEventCallback(
        [](void* userData, void* eventPtr) -> SbBool {
            return myAppEventHandler(userData, eventPtr);
        },
        myRenderArea);

    // Now simulate a sequence of user interactions
    // In real toolkit, these would come from actual user input
    
    printf("\n=== Simulating user interactions ===\n\n");

    const char *baseFilename = (argc > 1) ? argv[1] : "10.2.setEventCB";
    char filename[512];

    // State 1: Initial empty scene
    printf("--- State 1: Initial empty scene ---\n");
    snprintf(filename, sizeof(filename), "%s_initial.rgb", baseFilename);
    myRenderArea->render(filename);
    
    // Simulate left button clicks to add points
    printf("\n--- Simulating LEFT button clicks to add points ---\n");
    MockAnyEvent nativeEvent;
    SoEvent* coinEvent;
    
    // Add a spread of 12 points across the view.  Using more points makes the
    // visual difference between "with points" and "cleared" frames obvious.
    // Coordinates are chosen to land in front of the background sphere.
    static const int clickCoords[][2] = {
        {400, 300}, {250, 200}, {550, 200}, {250, 400}, {550, 400},
        {150, 300}, {650, 300}, {400, 150}, {400, 450},
        {300, 250}, {500, 250}, {300, 350}
    };
    static const int numClicks = 12;

    nativeEvent.type   = MockButtonPress;
    nativeEvent.button = MockButton1;

    for (int i = 0; i < numClicks; i++) {
        nativeEvent.x = clickCoords[i][0];
        nativeEvent.y = clickCoords[i][1];
        coinEvent = translateNativeEvent(&nativeEvent, myRenderArea->getViewportRegion());
        myRenderArea->processEvent(coinEvent);
        delete coinEvent;
    }
    
    printf("--- State 2: After adding %d points ---\n", numClicks);
    snprintf(filename, sizeof(filename), "%s_points.rgb", baseFilename);
    myRenderArea->render(filename);
    
    // Simulate middle button press to start rotation
    printf("\n--- Simulating MIDDLE button for rotation ---\n");
    nativeEvent.button = MockButton2;
    nativeEvent.x = 400;
    nativeEvent.y = 300;
    coinEvent = translateNativeEvent(&nativeEvent, myRenderArea->getViewportRegion());
    myRenderArea->processEvent(coinEvent);
    delete coinEvent;
    
    // Process timer events to rotate camera
    printf("Processing timer sensor for rotation...\n");
    for (int i = 0; i < 10; i++) {
        SoDB::getSensorManager()->processTimerQueue();
    }
    
    printf("--- State 3: After camera rotation ---\n");
    snprintf(filename, sizeof(filename), "%s_rotated.rgb", baseFilename);
    myRenderArea->render(filename);
    
    // Release middle button to stop rotation
    nativeEvent.type = MockButtonRelease;
    coinEvent = translateNativeEvent(&nativeEvent, myRenderArea->getViewportRegion());
    myRenderArea->processEvent(coinEvent);
    delete coinEvent;
    
    // Simulate right button to clear
    printf("\n--- Simulating RIGHT button to clear points ---\n");
    nativeEvent.type = MockButtonPress;
    nativeEvent.button = MockButton3;
    coinEvent = translateNativeEvent(&nativeEvent, myRenderArea->getViewportRegion());
    myRenderArea->processEvent(coinEvent);
    delete coinEvent;
    
    printf("--- State 4: After clearing points ---\n");
    snprintf(filename, sizeof(filename), "%s_cleared.rgb", baseFilename);
    myRenderArea->render(filename);

    printf("\n=== Summary ===\n");
    printf("Generated 4 images showing event-driven interaction\n");
    printf("\nKey architectural insight:\n");
    printf("Event translation is a GENERIC pattern that works with ANY toolkit.\n");
    printf("\nToolkit responsibilities:\n");
    printf("  1. Capture native events (X11 XEvent, Win32 MSG, etc.)\n");
    printf("  2. Translate to SoEvent (normalize coordinates, map buttons)\n");
    printf("  3. Send to application callback OR scene graph\n");
    printf("  4. Trigger redraw if event was handled\n");
    printf("\nCoin responsibilities:\n");
    printf("  - Define SoEvent abstraction (toolkit-independent)\n");
    printf("  - Process events through SoHandleEventAction\n");
    printf("  - Handle events in nodes (manipulators, event callbacks)\n");
    printf("\nThis exact pattern works with:\n");
    printf("  - X11/Xt (original): XEvent -> SoEvent\n");
    printf("  - Qt: QMouseEvent -> SoEvent\n");
    printf("  - FLTK: Fl_Event -> SoEvent\n");
    printf("  - Win32: MSG -> SoEvent\n");
    printf("  - Web: JavaScript Event -> SoEvent\n");
    printf("  - Custom/mock: Generic struct -> SoEvent\n");

    // Cleanup
    delete myTicker;
    delete myRenderArea;
    root->unref();

    return 0;
}
