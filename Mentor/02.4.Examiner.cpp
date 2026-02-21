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
 * Headless version of Inventor Mentor example 2.4
 * 
 * Original: Examiner - uses an examiner viewer to look at a cone
 * Headless: Simulates examiner viewer operations (tumble, dolly, pan)
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoSeparator.h>
#include <cmath>
#include <cstdio>

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *myCamera = new SoPerspectiveCamera;
    root->addChild(myCamera);
    root->addChild(new SoDirectionalLight);

    SoMaterial *myMaterial = new SoMaterial;
    myMaterial->diffuseColor.setValue(1.0, 0.0, 0.0);  // Red cone
    root->addChild(myMaterial);
    root->addChild(new SoCone);

    // Setup camera to view the scene
    SbViewportRegion viewport(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    myCamera->viewAll(root, viewport);

    const char *baseFilename = (argc > 1) ? argv[1] : "02.4.Examiner";
    char filename[256];

    int frameNum = 0;

    // Initial view
    snprintf(filename, sizeof(filename), "%s_frame%02d_initial.rgb", baseFilename, frameNum++);
    renderToFile(root, filename);

    // Simulate tumbling (rotating camera around scene)
    SbVec3f origPos = myCamera->position.getValue();
    SbRotation origOrient = myCamera->orientation.getValue();
    
    for (int i = 1; i <= 8; i++) {
        float angle = (M_PI / 4.0f) * i;  // 45 degree increments
        
        // Rotate around Y axis
        float radius = origPos.length();
        myCamera->position.setValue(
            radius * sin(angle),
            origPos[1],
            radius * cos(angle)
        );
        myCamera->pointAt(SbVec3f(0, 0, 0));
        
        snprintf(filename, sizeof(filename), "%s_frame%02d_tumble.rgb", baseFilename, frameNum++);
        renderToFile(root, filename);
    }

    // Reset position for dolly operations
    myCamera->position.setValue(origPos);
    myCamera->orientation.setValue(origOrient);

    // Record original near/far distances set by viewAll
    float origNear = myCamera->nearDistance.getValue();
    float origFar  = myCamera->farDistance.getValue();

    // Simulate dollying (zooming in/out by moving camera).
    // Scale near/far clipping distances proportionally so the scene stays
    // visible at every dolly position.
    for (int i = 0; i < 4; i++) {
        float scale = 0.5f + i * 0.5f;  // 0.5, 1.0, 1.5, 2.0
        SbVec3f scaledPos = origPos * scale;
        myCamera->position.setValue(scaledPos);
        myCamera->nearDistance.setValue(origNear * scale);
        myCamera->farDistance.setValue(origFar  * scale);
        
        snprintf(filename, sizeof(filename), "%s_frame%02d_dolly.rgb", baseFilename, frameNum++);
        renderToFile(root, filename);
    }

    printf("Rendered %d frames simulating examiner viewer operations\n", frameNum);

    root->unref();
    return 0;
}
