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

    // Create a scene with three distinct 3D shapes at different depths.
    // Depth variation makes the perspective vs. orthographic difference obvious.

    // Red cone – left, slightly in front
    SoSeparator *coneSep = new SoSeparator;
    SoMaterial *coneMat = new SoMaterial;
    coneMat->diffuseColor.setValue(0.85f, 0.15f, 0.10f);
    coneSep->addChild(coneMat);
    SoTransform *coneXf = new SoTransform;
    coneXf->translation.setValue(-2.5f, 0.0f, 1.0f);
    coneSep->addChild(coneXf);
    coneSep->addChild(new SoCone);
    root->addChild(coneSep);

    // Green sphere – centre
    SoSeparator *sphereSep = new SoSeparator;
    SoMaterial *sphereMat = new SoMaterial;
    sphereMat->diffuseColor.setValue(0.15f, 0.70f, 0.20f);
    sphereSep->addChild(sphereMat);
    sphereSep->addChild(new SoSphere);
    root->addChild(sphereSep);

    // Blue cube – right, slightly behind
    SoSeparator *cubeSep = new SoSeparator;
    SoMaterial *cubeMat = new SoMaterial;
    cubeMat->diffuseColor.setValue(0.15f, 0.30f, 0.85f);
    cubeSep->addChild(cubeMat);
    SoTransform *cubeXf = new SoTransform;
    cubeXf->translation.setValue(2.5f, 0.0f, -1.0f);
    cubeSep->addChild(cubeXf);
    cubeSep->addChild(new SoCube);
    root->addChild(cubeSep);

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
