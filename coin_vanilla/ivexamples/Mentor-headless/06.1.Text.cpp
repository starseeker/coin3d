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
 * Headless version of Inventor Mentor example 6.1
 * 
 * Original: Text - renders globe with 2D text labels
 * Headless: Renders sphere with 2D text from multiple angles
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoComplexity.h>
#include <Inventor/nodes/SoFont.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoText2.h>
#include <Inventor/nodes/SoTranslation.h>
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
    myFont->size.setValue(24.0);
    root->addChild(myFont);

    // Add the globe - a sphere
    SoSeparator *sphereSep = new SoSeparator;
    SoComplexity *sphereComplexity = new SoComplexity;
    sphereComplexity->value = 0.55;
    root->addChild(sphereSep);
    sphereSep->addChild(sphereComplexity);
    sphereSep->addChild(new SoSphere);

    // Add Text2 for AFRICA
    SoSeparator *africaSep = new SoSeparator;
    SoTranslation *africaTranslate = new SoTranslation;
    SoText2 *africaText = new SoText2;
    africaTranslate->translation.setValue(.25, .0, 1.25);
    africaText->string = "AFRICA";
    root->addChild(africaSep);
    africaSep->addChild(africaTranslate);
    africaSep->addChild(africaText);

    // Add Text2 for ASIA
    SoSeparator *asiaSep = new SoSeparator;
    SoTranslation *asiaTranslate = new SoTranslation;
    SoText2 *asiaText = new SoText2;
    asiaTranslate->translation.setValue(.8, .6, .5);
    asiaText->string = "ASIA";
    root->addChild(asiaSep);
    asiaSep->addChild(asiaTranslate);
    asiaSep->addChild(asiaText);

    // Setup camera
    camera->viewAll(root, SbViewportRegion(DEFAULT_WIDTH, DEFAULT_HEIGHT));

    const char *baseFilename = (argc > 1) ? argv[1] : "06.1.Text";
    char filename[256];

    // Front view
    snprintf(filename, sizeof(filename), "%s_front.rgb", baseFilename);
    renderToFile(root, filename);

    // Rotated view
    rotateCamera(camera, M_PI / 4, M_PI / 6);
    snprintf(filename, sizeof(filename), "%s_angle.rgb", baseFilename);
    renderToFile(root, filename);

    root->unref();
    return 0;
}
