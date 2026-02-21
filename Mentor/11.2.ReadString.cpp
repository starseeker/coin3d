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
 * Headless version of Inventor Mentor example 11.2
 * 
 * Original: ReadString - reads scene from string buffer
 * Headless: Parses scene from string and renders to image
 */

#include "headless_utils.h"
#include <Inventor/SoDB.h>
#include <Inventor/SoInput.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <cstdio>
#include <cstring>

// Scene data as a string
static const char *sceneBuffer = 
"#Inventor V2.0 ascii\n"
"\n"
"Separator {\n"
"   Material {\n"
"      diffuseColor [ 1 0 0, 0 1 0, 0 0 1 ]\n"
"   }\n"
"   MaterialBinding { value PER_PART }\n"
"   Cone {}\n"
"}\n";

SoSeparator *readFromString(const char *buffer)
{
    SoInput mySceneInput;
    mySceneInput.setBuffer((void *)buffer, strlen(buffer));

    SoSeparator *myGraph = SoDB::readAll(&mySceneInput);
    if (myGraph == NULL) {
        fprintf(stderr, "Problem parsing scene from string\n");
        return NULL;
    }

    return myGraph;
}

int main(int argc, char **argv)
{
    // Initialize Coin for headless operation
    initCoinHeadless();

    // Parse scene from string buffer
    SoSeparator *scene = readFromString(sceneBuffer);
    if (!scene) {
        fprintf(stderr, "Failed to parse scene\n");
        return 1;
    }
    scene->ref();
    
    printf("Successfully parsed scene from string buffer\n");

    // Create root with camera and light
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *camera = new SoPerspectiveCamera;
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);
    root->addChild(scene);

    // Setup camera
    camera->viewAll(root, SbViewportRegion(DEFAULT_WIDTH, DEFAULT_HEIGHT));

    const char *baseFilename = (argc > 1) ? argv[1] : "11.2.ReadString";
    char filename[256];
    
    snprintf(filename, sizeof(filename), "%s.rgb", baseFilename);
    renderToFile(root, filename);

    root->unref();
    scene->unref();
    return 0;
}
