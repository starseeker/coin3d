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
 * Headless version of Inventor Mentor example 7.2
 * 
 * Original: TextureCoordinates - textured square with explicit texture coords
 * Headless: Renders textured square from multiple angles
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoFaceSet.h>
#include <Inventor/nodes/SoNormal.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/nodes/SoTextureCoordinate2.h>
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

    // Create a brick-pattern texture
    unsigned char brick[64*64*3];
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 64; x++) {
            int idx = (y * 64 + x) * 3;
            // Create a brick pattern
            bool isHorizontalLine = (y % 16 == 0);
            bool isVerticalLine = ((x + (y/16)*8) % 32 == 0);
            bool isMortar = isHorizontalLine || isVerticalLine;
            
            if (isMortar) {
                brick[idx] = 180;
                brick[idx+1] = 180;
                brick[idx+2] = 180;
            } else {
                brick[idx] = 150;
                brick[idx+1] = 80;
                brick[idx+2] = 60;
            }
        }
    }

    SoTexture2 *texture = new SoTexture2;
    texture->image.setValue(SbVec2s(64, 64), 3, brick);
    root->addChild(texture);

    // Define the square's spatial coordinates
    SoCoordinate3 *coord = new SoCoordinate3;
    root->addChild(coord);
    coord->point.set1Value(0, SbVec3f(-3, -3, 0));
    coord->point.set1Value(1, SbVec3f( 3, -3, 0));
    coord->point.set1Value(2, SbVec3f( 3,  3, 0));
    coord->point.set1Value(3, SbVec3f(-3,  3, 0));

    // Define the square's normal
    SoNormal *normal = new SoNormal;
    root->addChild(normal);
    normal->vector.set1Value(0, SbVec3f(0, 0, 1));

    // Define the square's texture coordinates
    SoTextureCoordinate2 *texCoord = new SoTextureCoordinate2;
    root->addChild(texCoord);
    texCoord->point.set1Value(0, SbVec2f(0, 0));
    texCoord->point.set1Value(1, SbVec2f(1, 0));
    texCoord->point.set1Value(2, SbVec2f(1, 1));
    texCoord->point.set1Value(3, SbVec2f(0, 1));

    // Define normal binding
    SoNormalBinding *nBind = new SoNormalBinding;
    root->addChild(nBind);
    nBind->value.setValue(SoNormalBinding::OVERALL);

    // Define a FaceSet
    SoFaceSet *myFaceSet = new SoFaceSet;
    root->addChild(myFaceSet);
    myFaceSet->numVertices.set1Value(0, 4);

    // Setup camera
    camera->viewAll(root, SbViewportRegion(DEFAULT_WIDTH, DEFAULT_HEIGHT));

    const char *baseFilename = (argc > 1) ? argv[1] : "07.2.TextureCoordinates";
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
