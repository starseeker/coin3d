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
 * Headless version of Inventor Mentor example 13.5
 * 
 * Original: Boolean - uses boolean engine to toggle between objects
 * Headless: Demonstrates boolean engine and conditional rendering
 */

#include "headless_utils.h"
#include <Inventor/SoDB.h>
#include <Inventor/engines/SoBoolOperation.h>
#include <Inventor/engines/SoElapsedTime.h>
#include <Inventor/engines/SoTimeCounter.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoSwitch.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoSeparator.h>
#include <cstdio>
#include <cmath>

int main(int argc, char **argv)
{
    // Initialize Coin for headless operation
    initCoinHeadless();

    SoSeparator *root = new SoSeparator;
    root->ref();

    // Add a camera and light
    SoPerspectiveCamera *myCamera = new SoPerspectiveCamera;
    root->addChild(myCamera);
    root->addChild(new SoDirectionalLight);

    // Create switch node with two objects
    SoSwitch *mySwitch = new SoSwitch;
    root->addChild(mySwitch);

    // Object 1: Cube
    SoSeparator *cube = new SoSeparator;
    mySwitch->addChild(cube);
    SoMaterial *cubeMat = new SoMaterial;
    cubeMat->diffuseColor.setValue(1.0, 0.0, 0.0);
    cube->addChild(cubeMat);
    cube->addChild(new SoCube);

    // Object 2: Sphere
    SoSeparator *sphere = new SoSeparator;
    mySwitch->addChild(sphere);
    SoMaterial *sphereMat = new SoMaterial;
    sphereMat->diffuseColor.setValue(0.0, 0.0, 1.0);
    sphere->addChild(sphereMat);
    sphere->addChild(new SoSphere);

    // Use time counter to toggle
    SoTimeCounter *counter = new SoTimeCounter;
    counter->ref();
    counter->max = 1;  // counts 0, 1, 0, 1, ...
    counter->frequency = 1.0;  // 1 cycle per second

    // Use boolean operation to demonstrate logic
    SoBoolOperation *boolOp = new SoBoolOperation;
    boolOp->ref();
    boolOp->a.connectFrom(&counter->output);
    boolOp->operation = SoBoolOperation::A;  // Just pass through A
    
    mySwitch->whichChild.connectFrom(&boolOp->output);

    // Setup camera
    myCamera->viewAll(root, SbViewportRegion(DEFAULT_WIDTH, DEFAULT_HEIGHT));

    const char *baseFilename = (argc > 1) ? argv[1] : "13.5.Boolean";
    char filename[256];

    // Render sequence showing alternation
    for (int i = 0; i <= 8; i++) {
        float timeValue = i * 0.5f;
        
        counter->timeIn.setValue(timeValue);
        
        SoDB::getSensorManager()->processTimerQueue();
        SoDB::getSensorManager()->processDelayQueue(TRUE);
        
        int which = mySwitch->whichChild.getValue();
        printf("Time %.1f: Showing %s (whichChild=%d)\n", 
               timeValue, which == 0 ? "Cube" : "Sphere", which);
        
        snprintf(filename, sizeof(filename), "%s_frame%02d.rgb", baseFilename, i);
        renderToFile(root, filename);
    }

    counter->unref();
    boolOp->unref();
    root->unref();

    return 0;
}
