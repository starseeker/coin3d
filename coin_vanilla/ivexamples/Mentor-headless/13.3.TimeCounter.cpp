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
 * Headless version of Inventor Mentor example 13.3
 * 
 * Original: TimeCounter - jumping figure using time counter engines
 * Headless: Renders jumping animation sequence
 */

#include "headless_utils.h"
#include <Inventor/SoDB.h>
#include <Inventor/SoInput.h>
#include <Inventor/engines/SoCompose.h>
#include <Inventor/engines/SoElapsedTime.h>
#include <Inventor/engines/SoTimeCounter.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoCube.h>
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
    myCamera->position.setValue(-8.0, -7.0, 20.0);
    myCamera->heightAngle = M_PI/2.5;
    myCamera->nearDistance = 15.0;
    myCamera->farDistance = 25.0;
    root->addChild(myCamera);
    root->addChild(new SoDirectionalLight);

    // Set up transformations
    SoTranslation *jumpTranslation = new SoTranslation;
    root->addChild(jumpTranslation);
    
    SoTransform *initialTransform = new SoTransform;
    initialTransform->translation.setValue(-20., 0., 0.);
    initialTransform->scaleFactor.setValue(4., 4., 4.);
    root->addChild(initialTransform);

    // Use a cube instead of reading jumpyMan.iv
    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.1, 0.3, 0.8);
    root->addChild(mat);
    root->addChild(new SoCube);

    // Create two counters for X and Y translations
    // Y counter is small and high frequency (jumping)
    // X counter is large and low frequency (moving across screen)
    SoTimeCounter *jumpHeightCounter = new SoTimeCounter;
    jumpHeightCounter->ref();
    SoTimeCounter *jumpWidthCounter = new SoTimeCounter;
    jumpWidthCounter->ref();
    SoComposeVec3f *jump = new SoComposeVec3f;
    jump->ref();

    jumpHeightCounter->max = 4;
    jumpHeightCounter->frequency = 1.5;
    jumpWidthCounter->max = 40;
    jumpWidthCounter->frequency = 0.15;

    jump->x.connectFrom(&jumpWidthCounter->output);
    jump->y.connectFrom(&jumpHeightCounter->output);
    jumpTranslation->translation.connectFrom(&jump->vector);

    const char *baseFilename = (argc > 1) ? argv[1] : "13.3.TimeCounter";
    char filename[256];

    // Render jumping animation at different time points
    for (int i = 0; i <= 20; i++) {
        float timeValue = i * 0.5f;  // 0, 0.5, 1.0, ... 10.0 seconds
        
        // Set the time for both counters
        jumpHeightCounter->timeIn.setValue(timeValue);
        jumpWidthCounter->timeIn.setValue(timeValue);
        
        // Process engines
        SoDB::getSensorManager()->processTimerQueue();
        SoDB::getSensorManager()->processDelayQueue(TRUE);
        
        // Get current position
        SbVec3f currentPos = jumpTranslation->translation.getValue();
        printf("Time %.1f: Position = (%.1f, %.1f)\n", 
               timeValue, currentPos[0], currentPos[1]);
        
        // Render this frame
        snprintf(filename, sizeof(filename), "%s_frame%02d.rgb", baseFilename, i);
        renderToFile(root, filename);
    }

    root->unref();
    jumpHeightCounter->unref();
    jumpWidthCounter->unref();
    jump->unref();

    return 0;
}
