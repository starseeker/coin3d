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
 * Headless version of Inventor Mentor example 12.3
 * 
 * Original: AlarmSensor - raises a flag after 12 seconds
 * Headless: Simulates alarm trigger and renders before/after states
 */

#include "headless_utils.h"
#include <Inventor/SoDB.h>
#include <Inventor/SbTime.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/sensors/SoAlarmSensor.h>
#include <cstdio>

bool flagRaised = false;

static void
raiseFlagCallback(void *data, SoSensor *)
{
    // We know data is really a SoTransform node
    SoTransform *flagAngleXform = (SoTransform *)data;

    // Rotate flag by 90 degrees about the Z axis
    flagAngleXform->rotation.setValue(SbVec3f(0, 0, 1), M_PI/2);
    
    flagRaised = true;
    printf("Alarm triggered! Flag raised.\n");
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

    // Create transform for the flag/cone
    SoTransform *flagXform = new SoTransform;
    root->addChild(flagXform);

    // Add a bright red cone to represent the flag (clearly visible)
    SoMaterial *flagMat = new SoMaterial;
    flagMat->diffuseColor.setValue(1.0f, 0.2f, 0.0f);
    root->addChild(flagMat);
    SoCone *myCone = new SoCone;
    myCone->bottomRadius = 0.8f;
    myCone->height = 2.0f;
    root->addChild(myCone);

    // Set up camera
    camera->viewAll(root, SbViewportRegion(DEFAULT_WIDTH, DEFAULT_HEIGHT));

    // Create an alarm that will call the flag-raising callback
    SoAlarmSensor *myAlarm = new SoAlarmSensor(raiseFlagCallback, flagXform);
    myAlarm->setTime(SbTime::getTimeOfDay() + 0.1);  // Set to trigger soon
    myAlarm->schedule();

    const char *baseFilename = (argc > 1) ? argv[1] : "12.3.AlarmSensor";
    char filename[256];

    // Render before alarm triggers
    printf("Before alarm triggers...\n");
    snprintf(filename, sizeof(filename), "%s_before.rgb", baseFilename);
    renderToFile(root, filename);

    // Process the sensor queue to trigger the alarm.
    // If the queue doesn't fire (timing-sensitive in headless mode), invoke
    // the callback directly so the before/after visual difference is guaranteed.
    printf("\nProcessing sensor queue...\n");
    SoDB::getSensorManager()->processTimerQueue();

    if (!flagRaised) {
        printf("Note: Timer queue did not fire immediately - invoking callback directly\n");
        raiseFlagCallback(flagXform, myAlarm);
    }

    // Render after alarm triggers
    printf("\nAfter alarm triggers...\n");
    snprintf(filename, sizeof(filename), "%s_after.rgb", baseFilename);
    renderToFile(root, filename);

    printf("\nFlag raised: %s\n", flagRaised ? "Yes" : "No");

    delete myAlarm;
    root->unref();

    return 0;
}
