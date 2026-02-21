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
 * Headless version of Inventor Mentor example 5.6
 * 
 * Original: TransformOrdering - shows transform order effects
 * Headless: Renders objects with different transform orderings
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoRotationXYZ.h>
#include <Inventor/nodes/SoScale.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <cmath>
#include <cstdio>

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

    // Create two separators for left and right objects
    SoSeparator *leftSep = new SoSeparator;
    SoSeparator *rightSep = new SoSeparator;
    root->addChild(leftSep);
    root->addChild(rightSep);

    // Create transformation nodes
    SoTranslation *leftTranslation = new SoTranslation;
    SoTranslation *rightTranslation = new SoTranslation;
    SoRotationXYZ *myRotation = new SoRotationXYZ;
    SoScale *myScale = new SoScale;

    // Fill in values
    leftTranslation->translation.setValue(-1.5, 0.0, 0.0);
    rightTranslation->translation.setValue(1.5, 0.0, 0.0);
    myRotation->angle = M_PI/2;  // 90 degrees
    myRotation->axis = SoRotationXYZ::X;
    myScale->scaleFactor.setValue(2., 1., 3.);

    // Left: translate, then rotate, then scale
    leftSep->addChild(leftTranslation);
    leftSep->addChild(myRotation);
    leftSep->addChild(myScale);
    
    // Add material and object
    SoMaterial *leftMat = new SoMaterial;
    leftMat->diffuseColor.setValue(1.0, 0.5, 0.0);
    leftSep->addChild(leftMat);
    leftSep->addChild(new SoCube);

    // Right: translate, then scale, then rotate
    rightSep->addChild(rightTranslation);
    rightSep->addChild(myScale);
    rightSep->addChild(myRotation);
    
    // Add material and object
    SoMaterial *rightMat = new SoMaterial;
    rightMat->diffuseColor.setValue(0.0, 0.5, 1.0);
    rightSep->addChild(rightMat);
    rightSep->addChild(new SoCube);

    // Setup camera
    camera->viewAll(root, SbViewportRegion(DEFAULT_WIDTH, DEFAULT_HEIGHT));

    const char *baseFilename = (argc > 1) ? argv[1] : "05.6.TransformOrdering";
    char filename[256];

    // Render front view
    snprintf(filename, sizeof(filename), "%s_front.rgb", baseFilename);
    renderToFile(root, filename);
    
    printf("Rendered transform ordering example\n");
    printf("Left: translate->rotate->scale, Right: translate->scale->rotate\n");

    // Render from angle
    rotateCamera(camera, M_PI/4, M_PI/6);
    snprintf(filename, sizeof(filename), "%s_angle.rgb", baseFilename);
    renderToFile(root, filename);

    root->unref();
    return 0;
}
