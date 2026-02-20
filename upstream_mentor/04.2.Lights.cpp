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
 * Headless version of Inventor Mentor example 4.2
 * 
 * Original: Lights - demonstrates directional and point lights with shuttle
 * Headless: Renders scene with different light positions (simulating animation)
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPointLight.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoTransformSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <cstdio>
#include <cmath>

int main(int argc, char **argv)
{
    // Initialize Coin for headless operation
    initCoinHeadless();

    SoSeparator *root = new SoSeparator;
    root->ref();

    // Add camera
    SoPerspectiveCamera *camera = new SoPerspectiveCamera;
    root->addChild(camera);

    // Add a red directional light
    SoDirectionalLight *myDirLight = new SoDirectionalLight;
    myDirLight->direction.setValue(0, -1, -1);
    myDirLight->color.setValue(1, 0, 0);
    root->addChild(myDirLight);

    // Put the transform and the point light below a transform separator
    SoTransformSeparator *myTransformSeparator = new SoTransformSeparator;
    root->addChild(myTransformSeparator);

    // Transform to move the point light
    SoTransform *lightTransform = new SoTransform;
    myTransformSeparator->addChild(lightTransform);

    // Add the green point light
    SoPointLight *myPointLight = new SoPointLight;
    myTransformSeparator->addChild(myPointLight);
    myPointLight->color.setValue(0, 1, 0);

    // Add a cone to the scene
    root->addChild(new SoCone);

    // Setup camera
    camera->viewAll(root, SbViewportRegion(DEFAULT_WIDTH, DEFAULT_HEIGHT));

    const char *baseFilename = (argc > 1) ? argv[1] : "04.2.Lights";
    char filename[256];

    // Render with point light at different positions (simulating shuttle animation)
    SbVec3f pos1(-2, -1, 3);
    SbVec3f pos2(1, 2, -3);

    int numFrames = 5;
    for (int i = 0; i < numFrames; i++) {
        float t = (float)i / (float)(numFrames - 1);
        SbVec3f pos = pos1 + t * (pos2 - pos1);
        lightTransform->translation.setValue(pos);
        
        snprintf(filename, sizeof(filename), "%s_frame%02d.rgb", baseFilename, i);
        renderToFile(root, filename);
    }

    printf("Rendered %d frames showing lighting variation\n", numFrames);

    root->unref();
    return 0;
}
