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
 *  Headless version of Inventor Mentor example 17.2
 *
 *  Converted from interactive render area to headless rendering.
 *  Example of combining Inventor and OpenGL rendering.
 *  Draw a red cube and a blue sphere with Inventor.
 *  Render the floor with OpenGL through a Callback node.
 *  
 *  Demonstrates that SoCallback nodes work in headless mode,
 *  allowing custom OpenGL rendering within the Coin scene graph.
 */

#include "headless_utils.h"

#if HAVE_CONFIG_H
#include <config.h>
#endif
#if HAVE_WINDOWS_H
#include <windows.h>
#endif

#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <Inventor/SbLinear.h>
#include <Inventor/nodes/SoCallback.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoLightModel.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/elements/SoGLLazyElement.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <cstdio>

float   floorObj[81][3];

// Build a scene with two objects and some light
void
buildScene(SoGroup *root)
{
   // Some light
   root->addChild(new SoLightModel);
   root->addChild(new SoDirectionalLight);

   // A red cube translated to the left and down
   SoTransform *myTrans = new SoTransform;    
   myTrans->translation.setValue(-2.0, -2.0, 0.0);
   root->addChild(myTrans);

   SoMaterial *myMtl = new SoMaterial;
   myMtl->diffuseColor.setValue(1.0, 0.0, 0.0);
   root->addChild(myMtl);
   
   root->addChild(new SoCube);

   // A blue sphere translated right
   myTrans = new SoTransform;    
   myTrans->translation.setValue(4.0, 0.0, 0.0);
   root->addChild(myTrans);

   myMtl = new SoMaterial;
   myMtl->diffuseColor.setValue(0.0, 0.0, 1.0);
   root->addChild(myMtl);
   
   root->addChild(new SoSphere);
}

// Build the floor that will be rendered using OpenGL.
void
buildFloor()
{
   int a = 0;

   for (float i = -5.0; i <= 5.0; i += 1.25) {
      for (float j = -5.0; j <= 5.0; j += 1.25, a++) {
         floorObj[a][0] = j;
         floorObj[a][1] = 0.0;
         floorObj[a][2] = i;
      }
   }
}

// Draw the lines that make up the floor, using OpenGL
void
drawFloor()
{
   int i;

   glBegin(GL_LINES);
   for (i=0; i<4; i++) {
      glVertex3fv(floorObj[i*18]);
      glVertex3fv(floorObj[(i*18)+8]);
      glVertex3fv(floorObj[(i*18)+17]);
      glVertex3fv(floorObj[(i*18)+9]);
   }

   glVertex3fv(floorObj[i*18]);
   glVertex3fv(floorObj[(i*18)+8]);
   glEnd();

   glBegin(GL_LINES);
   for (i=0; i<4; i++) {
      glVertex3fv(floorObj[i*2]);
      glVertex3fv(floorObj[(i*2)+72]);
      glVertex3fv(floorObj[(i*2)+73]);
      glVertex3fv(floorObj[(i*2)+1]);
   }
   glVertex3fv(floorObj[i*2]);
   glVertex3fv(floorObj[(i*2)+72]);
   glEnd();
}

// Callback routine to render the floor using OpenGL
void
myCallbackRoutine(void *, SoAction *action)
{
   // only render the floor during GLRender actions:
   if(!action->isOfType(SoGLRenderAction::getClassTypeId())) return;
   
   glPushMatrix();
   glTranslatef(0.0, -3.0, 0.0);
   glColor3f(0.0, 0.7, 0.0);
   glLineWidth(2);
   glDisable(GL_LIGHTING);  // so we don't have to set normals
   drawFloor();
   glEnable(GL_LIGHTING);   
   glLineWidth(1);
   glPopMatrix();
   
   // With Inventor 2.1+, it's necessary to reset SoGLLazyElement after
   // making calls (such as glColor3f()) that affect material state.
   SoState *state = action->getState();
   SoGLLazyElement* lazyElt = 
	(SoGLLazyElement*)SoLazyElement::getInstance(state);
   lazyElt->reset(state, 
	(SoLazyElement::DIFFUSE_MASK)|(SoLazyElement::LIGHT_MODEL_MASK));
}

int
main(int, char **)
{
   initCoinHeadless();

   buildFloor();

   // Build a simple scene graph, including a camera and
   // a SoCallback node for performing some GL rendering.
   SoSeparator *root = new SoSeparator;
   root->ref();

   SoPerspectiveCamera *myCamera = new SoPerspectiveCamera;
   myCamera->position.setValue(0.0, 0.0, 5.0);
   myCamera->heightAngle  = M_PI/2.0;  // 90 degrees
   myCamera->nearDistance = 2.0;
   myCamera->farDistance  = 12.0;
   root->addChild(myCamera);

   SoCallback *myCallback = new SoCallback;
   myCallback->setCallback(myCallbackRoutine);
   root->addChild(myCallback);

   buildScene(root);
   
   printf("Rendering scene with OpenGL callback for floor...\n");
   
   // Render from default viewpoint
   renderToFile(root, "output/17.2.GLCallback_00_default.rgb", 
                DEFAULT_WIDTH, DEFAULT_HEIGHT, SbColor(0.8f, 0.8f, 0.8f));
   
   // Render from different camera angles to show the OpenGL floor
   myCamera->position.setValue(-3.0, 2.0, 5.0);
   myCamera->orientation.setValue(SbRotation(SbVec3f(0, 1, 0), 0.3f));
   renderToFile(root, "output/17.2.GLCallback_01_angle1.rgb",
                DEFAULT_WIDTH, DEFAULT_HEIGHT, SbColor(0.8f, 0.8f, 0.8f));
   
   myCamera->position.setValue(3.0, 2.0, 5.0);
   myCamera->orientation.setValue(SbRotation(SbVec3f(0, 1, 0), -0.3f));
   renderToFile(root, "output/17.2.GLCallback_02_angle2.rgb",
                DEFAULT_WIDTH, DEFAULT_HEIGHT, SbColor(0.8f, 0.8f, 0.8f));
   
   myCamera->position.setValue(0.0, 4.0, 5.0);
   myCamera->orientation.setValue(SbRotation(SbVec3f(1, 0, 0), -0.4f));
   renderToFile(root, "output/17.2.GLCallback_03_top.rgb",
                DEFAULT_WIDTH, DEFAULT_HEIGHT, SbColor(0.8f, 0.8f, 0.8f));
   
   printf("Done! Rendered 4 views showing OpenGL callback integration.\n");

   root->unref();

   return 0;
}
