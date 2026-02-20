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
 *  Headless version of Inventor Mentor example 14.3
 *
 *  Converted from interactive render area to headless rendering.
 *  This example illustrates the creation of motion hierarchies
 *  using nodekits by creating a model of a balance-style scale.
 *  
 *  Simulates keyboard events (LEFT_ARROW and RIGHT_ARROW) to
 *  tip the balance scale left and right.
 */

#include "headless_utils.h"
#include <Inventor/events/SoKeyboardEvent.h>
#include <Inventor/nodekits/SoCameraKit.h>
#include <Inventor/nodekits/SoLightKit.h>
#include <Inventor/nodekits/SoSceneKit.h>
#include <Inventor/nodekits/SoShapeKit.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoEventCallback.h>
#include <Inventor/nodes/SoText2.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <cstdio>

// Callback Function for Animating the Balance Scale.
void
tipTheBalance(
   void *userData,
   SoEventCallback *eventCB)
{
   const SoEvent *ev = eventCB->getEvent();
   
   // Which Key was pressed?
   // If Right or Left Arrow key, then continue...
   if (SO_KEY_PRESS_EVENT(ev, RIGHT_ARROW) || 
        SO_KEY_PRESS_EVENT(ev, LEFT_ARROW)) {
      SoShapeKit  *support, *beam1, *string1, *string2;
      SbRotation  startRot, beamIncrement, stringIncrement;

      // Get the different nodekits from the userData.
      support = (SoShapeKit *) userData;

      // These three parts are extracted based on knowledge of the
      // motion hierarchy
      beam1   = (SoShapeKit *) support->getPart("childList[0]",TRUE);
      string1 = (SoShapeKit *)   beam1->getPart("childList[0]",TRUE);
      string2 = (SoShapeKit *)   beam1->getPart("childList[1]",TRUE);

      // Set angular increments to be .1 Radians about the Z-Axis
      // The strings rotate opposite the beam, and the two types
      // of key press produce opposite effects.
      if (SO_KEY_PRESS_EVENT(ev, RIGHT_ARROW)) {
         beamIncrement.setValue(SbVec3f(0, 0, 1), -.1);
         stringIncrement.setValue(SbVec3f(0, 0, 1), .1);
      } 
      else {
         beamIncrement.setValue(SbVec3f(0, 0, 1), .1);
         stringIncrement.setValue(SbVec3f(0, 0, 1), -.1);
      }

      // Use SO_GET_PART to find the transform for each of the 
      // rotating parts and modify their rotations.

      SoTransform *xf;
      xf = SO_GET_PART(beam1, "transform", SoTransform);
      startRot = xf->rotation.getValue();
      xf->rotation.setValue(startRot *  beamIncrement);

      xf = SO_GET_PART(string1, "transform", SoTransform);
      startRot = xf->rotation.getValue();
      xf->rotation.setValue(startRot *  stringIncrement);

      xf = SO_GET_PART(string2, "transform", SoTransform);
      startRot = xf->rotation.getValue();
      xf->rotation.setValue(startRot *  stringIncrement);

      eventCB->setHandled();
   }
}

int
main(int, char **)
{
   initCoinHeadless();

   SoSceneKit *myScene = new SoSceneKit;
   myScene->ref();

   myScene->setPart("lightList[0]", new SoLightKit);
   myScene->setPart("cameraList[0]", new SoCameraKit);
   myScene->setCameraNumber(0);

   // Create the Balance Scale -- put each part in the 
   // childList of its parent, to build up this hierarchy:
   //
   //                    myScene
   //                       |
   //                     support
   //                       |
   //                     beam
   //                       |
   //                   --------
   //                   |       |
   //                string1  string2
   //                   |       |
   //                tray1     tray2

   SoShapeKit *support = new SoShapeKit();
   support->setPart("shape", new SoCone);
   support->set("shape { height 3 bottomRadius .3 }");
   myScene->setPart("childList[0]", support);

   SoShapeKit *beam = new SoShapeKit();
   beam->setPart("shape", new SoCube);
   beam->set("shape { width 3 height .2 depth .2 }");
   beam->set("transform { translation 0 1.5 0 } ");
   support->setPart("childList[0]", beam);

   SoShapeKit *string1 = new SoShapeKit;
   string1->setPart("shape", new SoCylinder);
   string1->set("shape { radius .05 height 2}");
   string1->set("transform { translation -1.5 -1 0 }");
   string1->set("transform { center 0 1 0 }");
   beam->setPart("childList[0]", string1);

   SoShapeKit *string2 = new SoShapeKit;
   string2->setPart("shape", new SoCylinder);
   string2->set("shape { radius .05 height 2}");
   string2->set("transform { translation 1.5 -1 0 } ");
   string2->set("transform { center 0 1 0 } ");
   beam->setPart("childList[1]", string2);

   SoShapeKit *tray1 = new SoShapeKit;
   tray1->setPart("shape", new SoCylinder);
   tray1->set("shape { radius .75 height .1 }");
   tray1->set("transform { translation 0 -1 0 } ");
   string1->setPart("childList[0]", tray1);

   SoShapeKit *tray2 = new SoShapeKit;
   tray2->setPart("shape", new SoCylinder);
   tray2->set("shape { radius .75 height .1 }");
   tray2->set("transform { translation 0 -1 0 } ");
   string2->setPart("childList[0]", tray2);

   // Add EventCallback so Balance Responds to Events
   SoEventCallback *myCallbackNode = new SoEventCallback;
   myCallbackNode->addEventCallback(
        SoKeyboardEvent::getClassTypeId(), 
	tipTheBalance, support); 
   support->setPart("callbackList[0]", myCallbackNode);

   // Add Instructions as Text in the Scene...
   SoShapeKit *myText = new SoShapeKit;
   myText->setPart("shape", new SoText2);
   myText->set("shape { string \"Press Left or Right Arrow Key\" }");
   myText->set("shape { justification CENTER }");
   myText->set("font { name \"Helvetica\" }");
   myText->set("font { size 16.0 }");
   myText->set("transform { translation 0 -2 0 }");
   myScene->setPart("childList[1]", myText);

   // Get camera from scene and position it
   SoPerspectiveCamera *myCamera = SO_GET_PART(myScene,
      "cameraList[0].camera", SoPerspectiveCamera);
   SbViewportRegion viewport(DEFAULT_WIDTH, DEFAULT_HEIGHT);
   myCamera->viewAll(myScene, viewport);

   char filename[64];
   printf("Rendering Balance Scale with keyboard event simulation...\n");
   
   // Render initial balanced state
   sprintf(filename, "output/14.3.Balance_00_initial.rgb");
   renderToFile(myScene, filename);
   
   // Simulate pressing RIGHT_ARROW key multiple times
   for (int i = 0; i < 5; i++) {
      simulateKeyPress(myScene, viewport, SoKeyboardEvent::RIGHT_ARROW);
      sprintf(filename, "output/14.3.Balance_%02d_right.rgb", i+1);
      renderToFile(myScene, filename);
   }
   
   // Simulate pressing LEFT_ARROW key to rebalance and tip left
   for (int i = 0; i < 10; i++) {
      simulateKeyPress(myScene, viewport, SoKeyboardEvent::LEFT_ARROW);
      sprintf(filename, "output/14.3.Balance_%02d_left.rgb", i+6);
      renderToFile(myScene, filename);
   }
   
   printf("Done! Rendered 16 frames showing balance tipping.\n");

   myScene->unref();

   return 0;
}
