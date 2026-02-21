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
 * Headless version of Inventor Mentor example 12.4
 * 
 * Original: TimerSensor - rotating object with timer-based scheduling
 * Headless: Simulates timer callbacks and renders rotation sequence
 */

#include "headless_utils.h"
#include <Inventor/SoDB.h>
#include <Inventor/SbTime.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoRotation.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/sensors/SoTimerSensor.h>
#include <cstdio>
#include <cmath>

int rotationCount = 0;

// This function rotates an object
static void
rotatingSensorCallback(void *data, SoSensor *)
{
    SoRotation *myRotation = (SoRotation *)data;
    SbRotation currentRotation = myRotation->rotation.getValue();
    // Rotate by 2 degrees per callback
    currentRotation = SbRotation(SbVec3f(0, 0, 1), M_PI/90.0) * currentRotation;
    myRotation->rotation.setValue(currentRotation);
    rotationCount++;
    
    SbVec3f axis;
    float angle;
    currentRotation.getValue(axis, angle);
    printf("Rotation %d: angle = %.2f degrees\n", rotationCount, angle * 180.0 / M_PI);
}

// This function reschedules the rotating sensor
static void
schedulingSensorCallback(void *data, SoSensor *)
{
    SoTimerSensor *rotatingSensor = (SoTimerSensor *)data;
    rotatingSensor->unschedule();
    
    if (rotatingSensor->getInterval().getValue() == 1.0) {
        rotatingSensor->setInterval(SbTime(1.0/10.0));
        printf("\n*** Changed rotation interval to 0.1 seconds (10x per second) ***\n\n");
    } else {
        rotatingSensor->setInterval(SbTime(1.0));
        printf("\n*** Changed rotation interval to 1.0 second ***\n\n");
    }
    
    rotatingSensor->schedule();
}

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

    // Create rotation node and cone
    SoRotation *myRotation = new SoRotation;
    root->addChild(myRotation);
    root->addChild(new SoCone);

    // Set up camera
    camera->viewAll(root, SbViewportRegion(DEFAULT_WIDTH, DEFAULT_HEIGHT));

    // Create timer sensors
    SoTimerSensor *rotatingSensor = 
        new SoTimerSensor(rotatingSensorCallback, myRotation);
    rotatingSensor->setInterval(SbTime(1.0)); // once per second
    rotatingSensor->schedule();

    SoTimerSensor *schedulingSensor =
        new SoTimerSensor(schedulingSensorCallback, rotatingSensor);
    schedulingSensor->setInterval(SbTime(5.0)); // once per 5 seconds
    schedulingSensor->schedule();

    const char *baseFilename = (argc > 1) ? argv[1] : "12.4.TimerSensor";
    char filename[256];

    // Render initial state
    printf("Initial state\n");
    snprintf(filename, sizeof(filename), "%s_initial.rgb", baseFilename);
    renderToFile(root, filename);

    // Simulate timer callbacks over time
    int frameCount = 0;
    for (int i = 0; i < 8; i++) {
        // Manually trigger the rotating sensor callback
        rotatingSensorCallback(myRotation, rotatingSensor);
        
        // Process sensor queue
        SoDB::getSensorManager()->processTimerQueue();
        
        // Render this state
        snprintf(filename, sizeof(filename), "%s_frame%02d.rgb", baseFilename, ++frameCount);
        renderToFile(root, filename);
        
        // After 5 rotations, trigger the scheduling sensor
        if (i == 4) {
            printf("\n5 seconds elapsed, triggering scheduling sensor...\n");
            schedulingSensorCallback(rotatingSensor, schedulingSensor);
        }
    }

    printf("\nTotal rotations: %d\n", rotationCount);

    delete rotatingSensor;
    delete schedulingSensor;
    root->unref();

    return 0;
}
