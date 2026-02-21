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
 * Headless version of Inventor Mentor example 13.1
 * 
 * Original: GlobalFlds - digital clock using realTime global field
 * Headless: Connects to realTime and renders at different time points
 */

#include "headless_utils.h"
#include <Inventor/SoDB.h>
#include <Inventor/SbTime.h>
#include <Inventor/fields/SoSFTime.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoText3.h>
#include <cstdio>

int main(int argc, char **argv)
{
    // Initialize Coin for headless operation
    initCoinHeadless();

    SoSeparator *root = new SoSeparator;
    root->ref();

    // Add a camera, light, and material
    SoPerspectiveCamera *myCamera = new SoPerspectiveCamera;
    root->addChild(myCamera);
    root->addChild(new SoDirectionalLight);
    
    SoMaterial *myMaterial = new SoMaterial;
    myMaterial->diffuseColor.setValue(1.0, 0.0, 0.0);
    root->addChild(myMaterial);

    // Create a Text3 object, and connect to the realTime field
    SoText3 *myText = new SoText3;
    root->addChild(myText);
    myText->string.connectFrom(SoDB::getGlobalField("realTime"));

    // Set up camera
    myCamera->viewAll(root, SbViewportRegion(DEFAULT_WIDTH, DEFAULT_HEIGHT));

    const char *baseFilename = (argc > 1) ? argv[1] : "13.1.GlobalFlds";
    char filename[256];

    // Render at current time
    printf("Current realTime value: %s\n", myText->string[0].getString());
    snprintf(filename, sizeof(filename), "%s_time1.rgb", baseFilename);
    renderToFile(root, filename);

    // Wait a bit and render again (simulate time passing)
    // We can't actually control realTime directly as it's maintained by the system
    // But we can demonstrate the connection is working
    printf("\nProcessing any pending updates...\n");
    SoDB::getSensorManager()->processTimerQueue();
    SoDB::getSensorManager()->processDelayQueue(TRUE);

    printf("Updated realTime value: %s\n", myText->string[0].getString());
    snprintf(filename, sizeof(filename), "%s_time2.rgb", baseFilename);
    renderToFile(root, filename);

    // Render once more
    SoDB::getSensorManager()->processTimerQueue();
    SoDB::getSensorManager()->processDelayQueue(TRUE);
    
    printf("Final realTime value: %s\n", myText->string[0].getString());
    snprintf(filename, sizeof(filename), "%s_time3.rgb", baseFilename);
    renderToFile(root, filename);

    root->unref();
    return 0;
}
