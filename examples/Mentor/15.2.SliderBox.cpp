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
 *  Headless version of Inventor Mentor example 15.2
 *
 *  Converted from interactive viewer to headless rendering.
 *  Uses 3 translate1Draggers to change the x, y, and z 
 *  components of a translation. A calculator engine assembles 
 *  the components.
 *  
 *  Demonstrates that draggers work with programmatic value setting
 *  without requiring interactive mouse manipulation.
 */

#include "headless_utils.h"
#include <Inventor/draggers/SoTranslate1Dragger.h>
#include <Inventor/engines/SoCalculator.h>
#include <Inventor/nodekits/SoShapeKit.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoText3.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <cstdio>

int
main(int, char **)
{
   initCoinHeadless();

   SoSeparator *root = new SoSeparator;
   root->ref();

   // Add camera and light for headless rendering
   SoPerspectiveCamera *camera = new SoPerspectiveCamera;
   camera->position.setValue(0, 0, 35);
   camera->orientation.setValue(SbRotation(SbVec3f(0, 1, 0), 0));
   root->addChild(camera);

   SoDirectionalLight *light = new SoDirectionalLight;
   root->addChild(light);

   // Create 3 translate1Draggers and place them in space.
   SoSeparator *xDragSep = new SoSeparator;
   SoSeparator *yDragSep = new SoSeparator;
   SoSeparator *zDragSep = new SoSeparator;
   root->addChild(xDragSep);
   root->addChild(yDragSep);
   root->addChild(zDragSep);
   
   // Separators will each hold a different transform
   SoTransform *xDragXf = new SoTransform;
   SoTransform *yDragXf = new SoTransform;
   SoTransform *zDragXf = new SoTransform;
   xDragXf->set("translation  0 -4 8");
   yDragXf->set("translation -8  0 8 rotation 0 0 1  1.57");
   zDragXf->set("translation -8 -4 0 rotation 0 1 0 -1.57");
   xDragSep->addChild(xDragXf);
   yDragSep->addChild(yDragXf);
   zDragSep->addChild(zDragXf);

   // Add the draggers under the separators, after transforms
   SoTranslate1Dragger *xDragger = new SoTranslate1Dragger;
   SoTranslate1Dragger *yDragger = new SoTranslate1Dragger;
   SoTranslate1Dragger *zDragger = new SoTranslate1Dragger;
   xDragSep->addChild(xDragger);
   yDragSep->addChild(yDragger);
   zDragSep->addChild(zDragger);

   // Create shape kit for the 3D text
   // The text says 'Slide Arrows To Move Me'
   SoShapeKit *textKit = new SoShapeKit;
   root->addChild(textKit);
   SoText3 *myText3 = new SoText3;
   textKit->setPart("shape", myText3);
   myText3->justification = SoText3::CENTER;
   myText3->string.set1Value(0,"Slide Arrows");
   myText3->string.set1Value(1,"To");
   myText3->string.set1Value(2,"Move Me");
   textKit->set("font { size 2}");
   textKit->set("material { diffuseColor 1 1 0}");

   // Create shape kit for surrounding box.
   // It's an unpickable cube, sized as (16,8,16)
   SoShapeKit *boxKit = new SoShapeKit;
   root->addChild(boxKit);
   boxKit->setPart("shape", new SoCube);
   boxKit->set("drawStyle { style LINES }");
   boxKit->set("pickStyle { style UNPICKABLE }");
   boxKit->set("material { emissiveColor 1 0 1 }");
   boxKit->set("shape { width 16 height 8 depth 16 }");

   // Create the calculator to make a translation
   // for the text.  The x component of a translate1Dragger's 
   // translation field shows how far it moved in that 
   // direction. So our text's translation is:
   // (xDragTranslate[0],yDragTranslate[0],zDragTranslate[0])
   SoCalculator *myCalc = new SoCalculator;
   myCalc->ref();
   myCalc->A.connectFrom(&xDragger->translation);
   myCalc->B.connectFrom(&yDragger->translation);
   myCalc->C.connectFrom(&zDragger->translation);
   myCalc->expression = "oA = vec3f(A[0],B[0],C[0])";

   // Connect the the translation in textKit from myCalc
   SoTransform *textXf 
      = (SoTransform *) textKit->getPart("transform",TRUE);
   textXf->translation.connectFrom(&myCalc->oA);

   char filename[64];
   printf("Rendering Slider Box with programmatic dragger positions...\n");
   
   // Render with different dragger positions to show the text moving
   // The draggers are positioned programmatically, demonstrating toolkit-agnostic control
   
   // Initial position (centered)
   xDragger->translation.setValue(0, 0, 0);
   yDragger->translation.setValue(0, 0, 0);
   zDragger->translation.setValue(0, 0, 0);
   sprintf(filename, "output/15.2.SliderBox_00_center.rgb");
   renderToFile(root, filename);
   
   // Move text to the right (X dragger)
   for (int i = 1; i <= 4; i++) {
      xDragger->translation.setValue(i * 2.0f, 0, 0);
      sprintf(filename, "output/15.2.SliderBox_%02d_x_pos.rgb", i);
      renderToFile(root, filename);
   }
   
   // Reset and move text upward (Y dragger)
   xDragger->translation.setValue(0, 0, 0);
   for (int i = 1; i <= 4; i++) {
      yDragger->translation.setValue(i * 1.5f, 0, 0);
      sprintf(filename, "output/15.2.SliderBox_%02d_y_pos.rgb", i + 4);
      renderToFile(root, filename);
   }
   
   // Reset and move text forward (Z dragger)
   yDragger->translation.setValue(0, 0, 0);
   for (int i = 1; i <= 4; i++) {
      zDragger->translation.setValue(i * 2.0f, 0, 0);
      sprintf(filename, "output/15.2.SliderBox_%02d_z_pos.rgb", i + 8);
      renderToFile(root, filename);
   }
   
   // Combined movement (diagonal)
   xDragger->translation.setValue(4, 0, 0);
   yDragger->translation.setValue(2, 0, 0);
   zDragger->translation.setValue(4, 0, 0);
   sprintf(filename, "output/15.2.SliderBox_13_combined.rgb");
   renderToFile(root, filename);
   
   printf("Done! Rendered 14 frames showing dragger-controlled text movement.\n");

   myCalc->unref();
   root->unref();

   return 0;
}
