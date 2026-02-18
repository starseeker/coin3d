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
 * Headless version of Inventor Mentor example 11.1
 * 
 * Original: ReadFile - reads .iv file and displays
 * Headless: Reads .iv file and renders to image
 */

#include "headless_utils.h"
#include <Inventor/SoDB.h>
#include <Inventor/SoInput.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoMaterial.h>
#include <cstdio>

SoSeparator *readFile(const char *filename)
{
    // Open the input file
    SoInput mySceneInput;
    if (!mySceneInput.openFile(filename)) {
        fprintf(stderr, "Cannot open file %s\n", filename);
        return NULL;
    }

    // Read the whole file into the database
    SoSeparator *myGraph = SoDB::readAll(&mySceneInput);
    if (myGraph == NULL) {
        fprintf(stderr, "Problem reading file\n");
        return NULL;
    } 

    mySceneInput.closeFile();
    return myGraph;
}

int main(int argc, char **argv)
{
    // Initialize Coin for headless operation
    initCoinHeadless();

    // Read the file (create simple scene if file not found)
    SoSeparator *scene = NULL;
    
    // Try to read from data directory
    const char *dataDir = "/home/runner/work/coin/coin/data";
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/star.iv", dataDir);
    
    scene = readFile(filepath);
    
    if (!scene) {
        printf("Could not read file, creating simple fallback scene\n");
        // Create a simple scene as fallback
        scene = new SoSeparator;
        scene->ref();
        
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(1.0, 0.8, 0.2);
        scene->addChild(mat);
        scene->addChild(new SoCone);
    } else {
        scene->ref();
        printf("Successfully read scene from file\n");
    }

    // Create root with camera and light
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *camera = new SoPerspectiveCamera;
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);
    root->addChild(scene);

    // Setup camera
    camera->viewAll(root, SbViewportRegion(DEFAULT_WIDTH, DEFAULT_HEIGHT));

    const char *baseFilename = (argc > 1) ? argv[1] : "11.1.ReadFile";
    char filename[256];
    
    snprintf(filename, sizeof(filename), "%s.rgb", baseFilename);
    renderToFile(root, filename);

    root->unref();
    scene->unref();
    return 0;
}
