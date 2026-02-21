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
 * Headless version of Inventor Mentor example 13.8
 * 
 * Original: Blinker - blinking neon sign with fast and slow blinkers
 * Headless: Renders blink sequence showing on/off states
 */

#include "headless_utils.h"
#include <Inventor/SoDB.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoBlinker.h>
#include <Inventor/nodes/SoText3.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTransform.h>
#include <cstdio>
#include <cmath>

int main(int argc, char **argv)
{
    // Initialize Coin for headless operation
    initCoinHeadless();

    SoSeparator *root = new SoSeparator;
    root->ref();

    // Set up camera and light
    SoPerspectiveCamera *myCamera = new SoPerspectiveCamera;
    root->addChild(myCamera);
    root->addChild(new SoDirectionalLight);

    // Add the non-blinking part (static text)
    SoMaterial *staticMat = new SoMaterial;
    staticMat->diffuseColor.setValue(0.8, 0.8, 0.8);
    root->addChild(staticMat);
    
    SoTransform *staticXform = new SoTransform;
    staticXform->translation.setValue(0, 2, 0);
    root->addChild(staticXform);
    
    SoText3 *staticText = new SoText3;
    staticText->string = "EAT AT";
    root->addChild(staticText);

    // Add the fast-blinking part to a blinker node
    SoBlinker *fastBlinker = new SoBlinker;
    root->addChild(fastBlinker);
    fastBlinker->speed = 2;  // 2 Hz: 0.5 second period (4 state changes per second)
    
    SoSeparator *fastSep = new SoSeparator;
    fastBlinker->addChild(fastSep);
    
    SoMaterial *fastMat = new SoMaterial;
    fastMat->diffuseColor.setValue(1.0, 0.0, 0.0);
    fastSep->addChild(fastMat);
    
    SoText3 *fastText = new SoText3;
    fastText->string = "JOSIE'S";
    fastSep->addChild(fastText);

    // Add the slow-blinking part to another blinker node
    SoBlinker *slowBlinker = new SoBlinker;
    root->addChild(slowBlinker);
    slowBlinker->speed = 0.5;  // 0.5 Hz: 2 seconds per complete on/off cycle
    
    SoSeparator *slowSep = new SoSeparator;
    slowBlinker->addChild(slowSep);
    
    SoMaterial *slowMat = new SoMaterial;
    slowMat->diffuseColor.setValue(0.0, 1.0, 0.0);
    slowSep->addChild(slowMat);
    
    SoTransform *slowXform = new SoTransform;
    slowXform->translation.setValue(0, -2, 0);
    slowSep->addChild(slowXform);
    
    SoText3 *slowText = new SoText3;
    slowText->string = "OPEN";
    slowSep->addChild(slowText);

    // Setup camera
    myCamera->viewAll(root, SbViewportRegion(DEFAULT_WIDTH, DEFAULT_HEIGHT));

    const char *baseFilename = (argc > 1) ? argv[1] : "13.8.Blinker";
    char filename[256];

    // Render blink sequence
    // Fast blinker cycles at 2 Hz (0.5 sec period)
    // Slow blinker cycles at 0.5 Hz (2 sec period)
    for (int i = 0; i <= 16; i++) {
        float time = i * 0.25f;  // 0, 0.25, 0.5, ... 4.0 seconds
        
        // Manually set the blinker states based on time
        // Fast blinker: on/off every 0.25 seconds
        fastBlinker->whichChild = (int)(time * 2) % 2;
        
        // Slow blinker: on/off every 1 second
        slowBlinker->whichChild = (int)(time * 0.5) % 2;
        
        printf("Time %.2f: Fast=%s, Slow=%s\n", 
               time,
               fastBlinker->whichChild.getValue() == 0 ? "ON " : "OFF",
               slowBlinker->whichChild.getValue() == 0 ? "ON " : "OFF");
        
        // Process updates
        SoDB::getSensorManager()->processTimerQueue();
        SoDB::getSensorManager()->processDelayQueue(TRUE);
        
        // Render this frame
        snprintf(filename, sizeof(filename), "%s_frame%02d.rgb", baseFilename, i);
        renderToFile(root, filename);
    }

    root->unref();
    return 0;
}
