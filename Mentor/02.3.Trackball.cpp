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
 * Headless version of Inventor Mentor example 2.3
 * 
 * Original: Trackball - uses a trackball manipulator to rotate a cone
 * Headless: Simulates orbital camera movement around the cone
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

    const char *baseFilename = (argc > 1) ? argv[1] : "02.3.Trackball";
    char filename[256];

    // Simulate trackball rotation by orbiting camera around the cone
    // This demonstrates what a trackball manipulator would do
    const int numFrames = 16;
    const float radius = 5.0f;
    const float height = 3.0f;

    for (int i = 0; i < numFrames; i++) {
        float angle = (2.0f * M_PI * i) / numFrames;
        
        // Position camera in an orbit
        myCamera->position.setValue(
            radius * cos(angle),
            height,
            radius * sin(angle)
        );
        
        // Point camera at center of scene
        myCamera->pointAt(SbVec3f(0, 0, 0));
        
        snprintf(filename, sizeof(filename), "%s_frame%02d.rgb", baseFilename, i);
        renderToFile(root, filename);
    }

    printf("Rendered %d frames simulating trackball rotation\n", numFrames);

    root->unref();
    return 0;
}
