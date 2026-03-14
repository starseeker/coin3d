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
 *
 * Note: The original used SoText3 nodes. This version uses basic geometric
 * shapes (cube, sphere, cylinder) for reliable rendering in all GL modes.
 * SoBlinker whichChild=0 shows the child, SO_SWITCH_NONE (-1) hides it.
 */

#include "headless_utils.h"
#include <Inventor/SoDB.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoBlinker.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCylinder.h>
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

    // Add the non-blinking part: a white cube at the top
    SoSeparator *staticSep = new SoSeparator;
    SoTransform *staticXf = new SoTransform;
    staticXf->translation.setValue(0.0f, 2.5f, 0.0f);
    staticXf->scaleFactor.setValue(3.0f, 0.5f, 1.0f);
    staticSep->addChild(staticXf);
    SoMaterial *staticMat = new SoMaterial;
    staticMat->diffuseColor.setValue(0.8f, 0.8f, 0.8f);
    staticSep->addChild(staticMat);
    staticSep->addChild(new SoCube);
    root->addChild(staticSep);

    // Fast-blinking part: a red cone in the center.
    // SoBlinker shows child[0] when on, hides it (SO_SWITCH_NONE) when off.
    SoBlinker *fastBlinker = new SoBlinker;
    fastBlinker->speed = 2.0f;  // 2 Hz
    root->addChild(fastBlinker);

    SoSeparator *fastSep = new SoSeparator;
    SoMaterial *fastMat = new SoMaterial;
    fastMat->diffuseColor.setValue(1.0f, 0.0f, 0.0f);
    fastSep->addChild(fastMat);
    fastSep->addChild(new SoCone);
    fastBlinker->addChild(fastSep);

    // Slow-blinking part: a green cylinder at the bottom
    SoBlinker *slowBlinker = new SoBlinker;
    slowBlinker->speed = 0.5f;  // 0.5 Hz
    root->addChild(slowBlinker);

    SoSeparator *slowSep = new SoSeparator;
    SoMaterial *slowMat = new SoMaterial;
    slowMat->diffuseColor.setValue(0.0f, 1.0f, 0.0f);
    slowSep->addChild(slowMat);
    SoTransform *slowXf = new SoTransform;
    slowXf->translation.setValue(0.0f, -2.5f, 0.0f);
    slowSep->addChild(slowXf);
    slowSep->addChild(new SoCylinder);
    slowBlinker->addChild(slowSep);

    // Setup camera to frame all objects
    myCamera->viewAll(root, SbViewportRegion(DEFAULT_WIDTH, DEFAULT_HEIGHT));

    const char *baseFilename = (argc > 1) ? argv[1] : "13.8.Blinker";
    char filename[256];

    // Render blink sequence by directly controlling the blinker states.
    // SoBlinker::whichChild=0 shows the child; SO_SWITCH_NONE (-1) hides it.
    // Fast blinker: 2 Hz -> toggles every 0.25 s
    // Slow blinker: 0.5 Hz -> toggles every 1.0 s
    for (int i = 0; i <= 16; i++) {
        float time = i * 0.25f;  // 0, 0.25, 0.5, ... 4.0 seconds

        // Fast blinker: period=0.5s, on for first half, off for second half
        bool fastOn = (int(time / 0.5f) % 2) == 0;
        fastBlinker->whichChild.setValue(fastOn ? 0 : SO_SWITCH_NONE);

        // Slow blinker: period=2.0s, on for first half, off for second half
        bool slowOn = (int(time / 2.0f) % 2) == 0;
        slowBlinker->whichChild.setValue(slowOn ? 0 : SO_SWITCH_NONE);

        printf("Time %.2f: Fast=%s, Slow=%s\n",
               time,
               fastOn ? "ON " : "OFF",
               slowOn ? "ON " : "OFF");

        SoDB::getSensorManager()->processTimerQueue();
        SoDB::getSensorManager()->processDelayQueue(TRUE);

        snprintf(filename, sizeof(filename), "%s_frame%02d.rgb", baseFilename, i);
        renderToFile(root, filename);
    }

    root->unref();
    return 0;
}
