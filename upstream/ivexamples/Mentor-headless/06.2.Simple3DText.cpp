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
 * Headless version of Inventor Mentor example 6.2
 * 
 * Original: Simple3DText - renders globe with 3D text labels
 * Headless: Renders sphere with 3D text from multiple angles
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoComplexity.h>
#include <Inventor/nodes/SoFont.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoMaterialBinding.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoText3.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <cmath>
#include <cstdio>

int main(int argc, char **argv)
{
    // Initialize Coin for headless operation
    initCoinHeadless();

    SoGroup *root = new SoGroup;
    root->ref();

    // Add camera and light
    SoPerspectiveCamera *camera = new SoPerspectiveCamera;
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);

    // Choose a font
    SoFont *myFont = new SoFont;
    myFont->name.setValue("Times");
    myFont->size.setValue(.2);
    root->addChild(myFont);

    // Color the front of text white, sides dark grey
    SoMaterial *myMaterial = new SoMaterial;
    SoMaterialBinding *myBinding = new SoMaterialBinding;
    myMaterial->diffuseColor.set1Value(0, SbColor(1, 1, 1));
    myMaterial->diffuseColor.set1Value(1, SbColor(.1, .1, .1));
    myBinding->value = SoMaterialBinding::PER_PART;
    root->addChild(myMaterial);
    root->addChild(myBinding);

    // Create the globe
    SoSeparator *sphereSep = new SoSeparator;
    SoComplexity *sphereComplexity = new SoComplexity;
    sphereComplexity->value = 0.55;
    root->addChild(sphereSep);
    sphereSep->addChild(sphereComplexity);
    sphereSep->addChild(new SoSphere);

    // Add 3D text for AFRICA
    SoSeparator *africaSep = new SoSeparator;
    SoTransform *africaTransform = new SoTransform;
    SoText3 *africaText = new SoText3;
    africaTransform->translation.setValue(.25, .0, 1.25);
    africaText->parts = SoText3::ALL;
    africaText->string = "AFRICA";
    root->addChild(africaSep);
    africaSep->addChild(africaTransform);
    africaSep->addChild(africaText);

    // Add 3D text for ASIA
    SoSeparator *asiaSep = new SoSeparator;
    SoTransform *asiaTransform = new SoTransform;
    SoText3 *asiaText = new SoText3;
    asiaTransform->translation.setValue(.8, .6, .5);
    asiaTransform->scaleFactor.setValue(.7, .7, .7);
    asiaText->parts = SoText3::ALL;
    asiaText->string = "ASIA";
    root->addChild(asiaSep);
    asiaSep->addChild(asiaTransform);
    asiaSep->addChild(asiaText);

    // Setup camera
    camera->viewAll(root, SbViewportRegion(DEFAULT_WIDTH, DEFAULT_HEIGHT));

    const char *baseFilename = (argc > 1) ? argv[1] : "06.2.Simple3DText";
    char filename[256];

    // Front view
    snprintf(filename, sizeof(filename), "%s_front.rgb", baseFilename);
    renderToFile(root, filename);

    // Side view
    rotateCamera(camera, M_PI / 2, 0);
    snprintf(filename, sizeof(filename), "%s_side.rgb", baseFilename);
    renderToFile(root, filename);

    // Angled view
    camera->viewAll(root, SbViewportRegion(DEFAULT_WIDTH, DEFAULT_HEIGHT));
    rotateCamera(camera, M_PI / 4, M_PI / 6);
    snprintf(filename, sizeof(filename), "%s_angle.rgb", baseFilename);
    renderToFile(root, filename);

    root->unref();
    return 0;
}
