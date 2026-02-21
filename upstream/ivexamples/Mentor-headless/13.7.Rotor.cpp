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
 * Headless version of Inventor Mentor example 13.7
 * 
 * Original: Rotor - rotating windmill vanes
 * Headless: Renders rotation sequence using windmill data files
 */

#include "headless_utils.h"
#include <Inventor/SoDB.h>
#include <Inventor/SoInput.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoRotor.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <cstdio>
#include <cmath>

// Load an Inventor scene graph from a file.  Returns NULL on failure.
static SoSeparator *readFile(const char *filename)
{
    SoInput input;
    if (!input.openFile(filename)) {
        fprintf(stderr, "Cannot open file %s\n", filename);
        return NULL;
    }
    SoSeparator *graph = SoDB::readAll(&input);
    input.closeFile();
    return graph;
}

int main(int argc, char **argv)
{
    // Initialize Coin for headless operation
    initCoinHeadless();

    SoSeparator *root = new SoSeparator;
    root->ref();

    // Add camera and light
    SoPerspectiveCamera *camera = new SoPerspectiveCamera;
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);

    // Locate data directory (same search order as other file-reading examples)
    const char *dataDir = getenv("COIN_DATA_DIR");
    if (!dataDir) dataDir = getenv("IVEXAMPLES_DATA_DIR");
    if (!dataDir) dataDir = "../../data";

    char towerPath[512], vanesPath[512];
    snprintf(towerPath, sizeof(towerPath), "%s/windmillTower.iv", dataDir);
    snprintf(vanesPath, sizeof(vanesPath), "%s/windmillVanes.iv", dataDir);

    SoSeparator *windmillTower = readFile(towerPath);
    SoSeparator *windmillVanes = readFile(vanesPath);

    // Add a rotor node to spin the vanes
    SoRotor *myRotor = new SoRotor;
    myRotor->rotation.setValue(SbVec3f(0, 0, 1), 0); // rotate around Z axis
    myRotor->speed = 0.2;

    if (windmillTower && windmillVanes) {
        // Use the rich windmill geometry from the data files
        root->addChild(windmillTower);
        root->addChild(myRotor);
        root->addChild(windmillVanes);
    } else {
        fprintf(stderr, "Windmill data files not found, using fallback geometry\n");

        // Fallback: brown cylindrical tower
        SoMaterial *towerMat = new SoMaterial;
        towerMat->diffuseColor.setValue(0.5f, 0.3f, 0.1f);
        root->addChild(towerMat);

        SoTransform *towerXf = new SoTransform;
        towerXf->translation.setValue(0.0f, -1.5f, 0.0f);
        towerXf->scaleFactor.setValue(0.4f, 4.0f, 0.4f);
        root->addChild(towerXf);
        root->addChild(new SoCylinder);

        root->addChild(myRotor);

        // Four blades with visible thickness, arranged as a cross
        SoSeparator *vanesSep = new SoSeparator;
        root->addChild(vanesSep);

        SoMaterial *vanesMat = new SoMaterial;
        vanesMat->diffuseColor.setValue(0.75f, 0.75f, 0.85f);
        vanesSep->addChild(vanesMat);

        float angles[4] = {0.0f, M_PI/2.0f, M_PI, 3.0f*M_PI/2.0f};
        for (int b = 0; b < 4; b++) {
            SoSeparator *bladeSep = new SoSeparator;
            vanesSep->addChild(bladeSep);

            SoTransform *bladeXf = new SoTransform;
            // Offset each blade from hub centre along the blade direction
            float bx = cosf(angles[b]) * 1.25f;
            float by = sinf(angles[b]) * 1.25f;
            bladeXf->translation.setValue(bx, by, 0.0f);
            bladeXf->rotation.setValue(SbVec3f(0, 0, 1), angles[b]);
            bladeXf->scaleFactor.setValue(0.35f, 2.5f, 0.15f);
            bladeSep->addChild(bladeXf);
            bladeSep->addChild(new SoCube);
        }
    }

    // Place camera for a 3/4 view: slightly to the side and from above so the
    // tower and rotating blades are both clearly visible.
    SbViewportRegion myRegion(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    camera->viewAll(root, myRegion);
    rotateCamera(camera, M_PI / 6.0f, M_PI / 8.0f);

    const char *baseFilename = (argc > 1) ? argv[1] : "13.7.Rotor";
    char filename[256];

    // Enable the rotor and render a 360° rotation sequence in 30° steps
    myRotor->on = TRUE;
    const float ROTATION_INCREMENT = M_PI / 6.0f;  // 30 degrees per frame
    for (int i = 0; i <= 12; i++) {
        float angle = i * ROTATION_INCREMENT;
        myRotor->rotation.setValue(SbVec3f(0, 0, 1), angle);

        SoDB::getSensorManager()->processTimerQueue();
        SoDB::getSensorManager()->processDelayQueue(TRUE);

        printf("Frame %d: Rotation angle = %.1f degrees\n", i, angle * 180.0 / M_PI);

        snprintf(filename, sizeof(filename), "%s_frame%02d.rgb", baseFilename, i);
        renderToFile(root, filename);
    }

    if (windmillTower) windmillTower->unref();
    if (windmillVanes) windmillVanes->unref();
    root->unref();
    return 0;
}
