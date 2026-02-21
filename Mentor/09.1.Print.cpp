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
 * Headless version of Inventor Mentor example 9.1
 * 
 * Original: Print - renders scene to PostScript
 * Headless: Demonstrates offscreen rendering (already headless by nature)
 */

#include "headless_utils.h"
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoDB.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <cstdio>

int main(int argc, char **argv)
{
    // Initialize Coin for headless operation
    initCoinHeadless();

    // Create a simple scene for demonstration
    SoSeparator *root = new SoSeparator;
    root->ref();

    // Add camera and light
    SoPerspectiveCamera *camera = new SoPerspectiveCamera;
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);

    // Add some geometry
    SoMaterial *redMat = new SoMaterial;
    redMat->diffuseColor.setValue(1.0, 0.0, 0.0);
    root->addChild(redMat);
    
    SoTransform *leftTrans = new SoTransform;
    leftTrans->translation.setValue(-2, 0, 0);
    root->addChild(leftTrans);
    root->addChild(new SoCube);

    SoMaterial *blueMat = new SoMaterial;
    blueMat->diffuseColor.setValue(0.0, 0.5, 1.0);
    root->addChild(blueMat);
    
    SoTransform *rightTrans = new SoTransform;
    rightTrans->translation.setValue(4, 0, 0);
    root->addChild(rightTrans);
    root->addChild(new SoSphere);

    // Setup camera
    camera->viewAll(root, SbViewportRegion(DEFAULT_WIDTH, DEFAULT_HEIGHT));

    const char *baseFilename = (argc > 1) ? argv[1] : "09.1.Print";
    char filename[256];

    // Render to RGB file (demonstrates offscreen rendering)
    snprintf(filename, sizeof(filename), "%s.rgb", baseFilename);
    renderToFile(root, filename);
    
    printf("Rendered scene using offscreen renderer\n");
    printf("Note: Original example printed to PostScript\n");

    root->unref();
    return 0;
}
