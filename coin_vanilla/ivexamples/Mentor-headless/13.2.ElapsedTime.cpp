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
 * Headless version of Inventor Mentor example 13.2
 * 
 * Original: ElapsedTime - sliding figure using elapsed time engine
 * Headless: Renders sliding animation sequence at different time points
 */

#include "headless_utils.h"
#include <Inventor/SoDB.h>
#include <Inventor/SoInput.h>
#include <Inventor/engines/SoCompose.h>
#include <Inventor/engines/SoElapsedTime.h>
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
    myCamera->position.setValue(-2.0, -2.0, 5.0);
    myCamera->heightAngle = M_PI/2.5;
    myCamera->nearDistance = 2.0;
    myCamera->farDistance = 7.0;
    root->addChild(myCamera);
    root->addChild(new SoDirectionalLight);

    // Set up transformations
    SoTranslation *slideTranslation = new SoTranslation;
    root->addChild(slideTranslation);
    
    SoTransform *initialTransform = new SoTransform;
    initialTransform->translation.setValue(-5., 0., 0.);
    initialTransform->scaleFactor.setValue(1., 1., 1.);
    root->addChild(initialTransform);

    // Use a cube instead of reading jumpyMan.iv (which may not exist)
    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.8, 0.3, 0.1);
    root->addChild(mat);
    root->addChild(new SoCube);

    // Make the X translation value change over time
    SoElapsedTime *myCounter = new SoElapsedTime;
    myCounter->ref();
    
    SoComposeVec3f *slideDistance = new SoComposeVec3f;
    slideDistance->ref();
    slideDistance->x.connectFrom(&myCounter->timeOut);
    slideTranslation->translation.connectFrom(&slideDistance->vector);

    const char *baseFilename = (argc > 1) ? argv[1] : "13.2.ElapsedTime";
    char filename[256];

    // Render sliding animation at different time points
    for (int i = 0; i <= 10; i++) {
        float timeValue = i * 0.5f;  // 0, 0.5, 1.0, 1.5, ... 5.0
        
        // Set the time explicitly
        myCounter->timeIn.setValue(timeValue);
        
        // Process engines
        SoDB::getSensorManager()->processTimerQueue();
        SoDB::getSensorManager()->processDelayQueue(TRUE);
        
        // Get current translation
        SbVec3f currentPos = slideTranslation->translation.getValue();
        printf("Time %.1f: X position = %.2f\n", timeValue, currentPos[0]);
        
        // Render this frame
        snprintf(filename, sizeof(filename), "%s_frame%02d.rgb", baseFilename, i);
        renderToFile(root, filename);
    }

    myCounter->unref();
    slideDistance->unref();
    root->unref();

    return 0;
}
