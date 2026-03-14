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
 * Headless version of Inventor Mentor example 7.3
 * 
 * Original: TextureFunction - spheres with texture coordinate generation
 * Headless: Renders three spheres with different texture repeat frequencies
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/nodes/SoTexture2Transform.h>
#include <Inventor/nodes/SoTextureCoordinatePlane.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <cmath>
#include <cstdio>

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

    // Create a simple face texture (smiley-like pattern)
    unsigned char face[32*32*3];
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            int idx = (y * 32 + x) * 3;
            int dx = x - 16, dy = y - 16;
            int dist = dx*dx + dy*dy;
            
            // Yellow circle for face
            if (dist < 225) {
                face[idx] = 255;
                face[idx+1] = 220;
                face[idx+2] = 0;
                
                // Eyes
                if ((dx == -6 && dy > 2 && dy < 6) || (dx == 6 && dy > 2 && dy < 6)) {
                    face[idx] = 0;
                    face[idx+1] = 0;
                    face[idx+2] = 0;
                }
                
                // Smile
                if (dy < -4 && dy > -8 && abs(dx) < 8 && abs(dx) > 5) {
                    face[idx] = 0;
                    face[idx+1] = 0;
                    face[idx+2] = 0;
                }
            } else {
                face[idx] = 100;
                face[idx+1] = 100;
                face[idx+2] = 150;
            }
        }
    }

    SoTexture2 *faceTexture = new SoTexture2;
    faceTexture->image.setValue(SbVec2s(32, 32), 3, face);
    root->addChild(faceTexture);

    // Make the diffuse color pure white
    SoMaterial *myMaterial = new SoMaterial;
    myMaterial->diffuseColor.setValue(1, 1, 1);
    root->addChild(myMaterial);

    // This texture2Transform centers the texture about (0,0,0)
    SoTexture2Transform *myTexXf = new SoTexture2Transform;
    myTexXf->translation.setValue(.5, .5);
    root->addChild(myTexXf);

    // Define a texture coordinate plane node with frequency of 2
    SoTextureCoordinatePlane *texPlane1 = new SoTextureCoordinatePlane;
    texPlane1->directionS.setValue(SbVec3f(2, 0, 0));
    texPlane1->directionT.setValue(SbVec3f(0, 2, 0));
    root->addChild(texPlane1);
    root->addChild(new SoSphere);

    // A translation node for spacing the three spheres
    SoTranslation *myTranslation = new SoTranslation;
    myTranslation->translation.setValue(2.5, 0, 0);

    // Create a second sphere with a repeat frequency of 1
    SoTextureCoordinatePlane *texPlane2 = new SoTextureCoordinatePlane;
    texPlane2->directionS.setValue(SbVec3f(1, 0, 0));
    texPlane2->directionT.setValue(SbVec3f(0, 1, 0));
    root->addChild(myTranslation);
    root->addChild(texPlane2);
    root->addChild(new SoSphere);

    // The third sphere has a repeat frequency of 0.5
    SoTextureCoordinatePlane *texPlane3 = new SoTextureCoordinatePlane;
    texPlane3->directionS.setValue(SbVec3f(.5, 0, 0));
    texPlane3->directionT.setValue(SbVec3f(0, .5, 0));
    root->addChild(myTranslation);
    root->addChild(texPlane3);
    root->addChild(new SoSphere);

    // Setup camera to view all spheres
    camera->viewAll(root, SbViewportRegion(DEFAULT_WIDTH, DEFAULT_HEIGHT));

    const char *baseFilename = (argc > 1) ? argv[1] : "07.3.TextureFunction";
    char filename[256];

    // Front view
    snprintf(filename, sizeof(filename), "%s_front.rgb", baseFilename);
    renderToFile(root, filename);

    // Angled view
    rotateCamera(camera, M_PI / 4, M_PI / 6);
    snprintf(filename, sizeof(filename), "%s_angle.rgb", baseFilename);
    renderToFile(root, filename);

    root->unref();
    return 0;
}
