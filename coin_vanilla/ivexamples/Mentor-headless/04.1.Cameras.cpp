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
 * Headless version of Inventor Mentor example 4.1
 * 
 * Original: Cameras - demonstrates different camera types with blinker
 * Headless: Renders scene from three different camera perspectives
 */

#include "headless_utils.h"
#include <Inventor/SbLinear.h>
#include <Inventor/SoDB.h>
#include <Inventor/SoInput.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoTransform.h>
#include <cstdio>

int main(int argc, char **argv)
{
    // Initialize Coin for headless operation
    initCoinHeadless();

    SoSeparator *root = new SoSeparator;
    root->ref();

    // Create a light
    root->addChild(new SoDirectionalLight);

    // Create a simple scene (replace file reading with built-in geometry)
    // Create a bench-like structure
    SoSeparator *sceneContent = new SoSeparator;
    
    SoMaterial *myMaterial = new SoMaterial;
    myMaterial->diffuseColor.setValue(0.8, 0.23, 0.03);
    sceneContent->addChild(myMaterial);
    
    // Seat
    SoTransform *seatTransform = new SoTransform;
    seatTransform->translation.setValue(0, 1, 0);
    seatTransform->scaleFactor.setValue(3, 0.2, 1);
    sceneContent->addChild(seatTransform);
    sceneContent->addChild(new SoCube);
    
    // Back
    SoSeparator *backSep = new SoSeparator;
    SoTransform *backTransform = new SoTransform;
    backTransform->translation.setValue(0, 2, -0.4);
    backTransform->scaleFactor.setValue(3, 1.5, 0.2);
    backSep->addChild(backTransform);
    backSep->addChild(new SoCube);
    sceneContent->addChild(backSep);
    
    // Legs
    for (int i = 0; i < 4; i++) {
        SoSeparator *legSep = new SoSeparator;
        SoTransform *legTransform = new SoTransform;
        float x = (i < 2) ? -1.2 : 1.2;
        float z = (i % 2 == 0) ? -0.4 : 0.4;
        legTransform->translation.setValue(x, 0.0, z);
        legTransform->scaleFactor.setValue(0.2, 1, 0.2);
        legSep->addChild(legTransform);
        legSep->addChild(new SoCube);
        sceneContent->addChild(legSep);
    }
    
    root->addChild(sceneContent);

    // Create three cameras
    SoOrthographicCamera *orthoViewAll = new SoOrthographicCamera;
    SoPerspectiveCamera *perspViewAll = new SoPerspectiveCamera;
    SoPerspectiveCamera *perspOffCenter = new SoPerspectiveCamera;

    // Setup viewport
    SbViewportRegion myRegion(DEFAULT_WIDTH, DEFAULT_HEIGHT);

    const char *baseFilename = (argc > 1) ? argv[1] : "04.1.Cameras";
    char filename[256];

    // Render from orthographic camera
    root->insertChild(orthoViewAll, 0);
    orthoViewAll->viewAll(root, myRegion);
    snprintf(filename, sizeof(filename), "%s_orthographic.rgb", baseFilename);
    renderToFile(root, filename);
    root->removeChild(0);

    // Render from perspective camera (view all)
    root->insertChild(perspViewAll, 0);
    perspViewAll->viewAll(root, myRegion);
    snprintf(filename, sizeof(filename), "%s_perspective.rgb", baseFilename);
    renderToFile(root, filename);
    root->removeChild(0);

    // Render from off-center perspective camera
    root->insertChild(perspOffCenter, 0);
    perspOffCenter->viewAll(root, myRegion);
    SbVec3f initialPos = perspOffCenter->position.getValue();
    float x, y, z;
    initialPos.getValue(x, y, z);
    perspOffCenter->position.setValue(x + x/2., y + y/2., z + z/4.);
    snprintf(filename, sizeof(filename), "%s_offcenter.rgb", baseFilename);
    renderToFile(root, filename);
    root->removeChild(0);

    printf("Rendered scene from 3 different camera perspectives\n");

    root->unref();
    return 0;
}
