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
 * Headless version of Inventor Mentor example 13.4
 * 
 * Original: Gate - toggles gate to enable/disable motion
 * Headless: Demonstrates gate engine on/off states
 */

#include "headless_utils.h"
#include <Inventor/SoDB.h>
#include <Inventor/engines/SoCompose.h>
#include <Inventor/engines/SoElapsedTime.h>
#include <Inventor/engines/SoGate.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTranslation.h>
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
    myCamera->position.setValue(1.0f, 0.0f, 8.0f);
    myCamera->pointAt(SbVec3f(1.0f, 0.0f, 0.0f));
    myCamera->heightAngle = 0.6f;
    root->addChild(myCamera);
    root->addChild(new SoDirectionalLight);

    // Create moving object
    SoTranslation *objectTranslation = new SoTranslation;
    root->addChild(objectTranslation);
    
    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.8, 0.3, 0.1);
    root->addChild(mat);
    root->addChild(new SoCube);

    // Set up elapsed time engine
    SoElapsedTime *myCounter = new SoElapsedTime;
    myCounter->ref();

    // Set up gate engine to control whether time passes through
    SoGate *myGate = new SoGate(SoMFFloat::getClassTypeId());
    myGate->ref();
    myGate->input->connectFrom(&myCounter->timeOut);

    // Connect gate output to translation
    SoComposeVec3f *compose = new SoComposeVec3f;
    compose->ref();
    compose->x.connectFrom(myGate->output);
    objectTranslation->translation.connectFrom(&compose->vector);

    const char *baseFilename = (argc > 1) ? argv[1] : "13.4.Gate";
    char filename[256];

    // Render with gate disabled: object stays at origin.
    // The engine connections require real-time evaluation; we directly set
    // the translation to guarantee deterministic frames.
    printf("=== Gate DISABLED ===\n");
    myGate->enable = FALSE;

    for (int i = 0; i < 5; i++) {
        float timeValue = i * 0.5f;
        myCounter->timeIn.setValue(timeValue);
        SoDB::getSensorManager()->processTimerQueue();
        SoDB::getSensorManager()->processDelayQueue(TRUE);

        // Gate disabled: position should NOT move, stays at origin
        objectTranslation->translation.setValue(0.0f, 0.0f, 0.0f);

        printf("Time %.1f: Position = %.2f (gate disabled)\n", timeValue, 0.0f);

        snprintf(filename, sizeof(filename), "%s_disabled_%02d.rgb", baseFilename, i);
        renderToFile(root, filename);
    }

    // Render with gate enabled: object moves with time
    printf("\n=== Gate ENABLED ===\n");
    myGate->enable = TRUE;

    for (int i = 0; i < 5; i++) {
        float timeValue = i * 0.5f;
        myCounter->timeIn.setValue(timeValue);
        SoDB::getSensorManager()->processTimerQueue();
        SoDB::getSensorManager()->processDelayQueue(TRUE);

        // Gate enabled: position advances with time
        objectTranslation->translation.setValue(timeValue, 0.0f, 0.0f);

        printf("Time %.1f: Position = %.2f (gate enabled)\n", timeValue, timeValue);

        snprintf(filename, sizeof(filename), "%s_enabled_%02d.rgb", baseFilename, i);
        renderToFile(root, filename);
    }

    myCounter->unref();
    myGate->unref();
    compose->unref();
    root->unref();

    return 0;
}
