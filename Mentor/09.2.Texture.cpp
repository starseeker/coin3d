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
 * Headless version of Inventor Mentor example 09.2
 * 
 * Original: Texture - using offscreen renderer to generate texture map
 * Headless: Renders scene to texture, then applies to cube
 */

#include "headless_utils.h"
#include <Inventor/SoDB.h>
#include <Inventor/SoInput.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoRotationXYZ.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoRotation.h>
#include <cstdio>
#include <cmath>

// Embedded scene to use as texture source (red cone)
static const char red_cone_iv[] = 
    "#Inventor V2.1 ascii\n\n"
    "Separator {\n"
    "  BaseColor { rgb 0.8 0 0 }\n"
    "  Rotation { rotation 1 1 0  1.57 }\n"
    "  Cone { }\n"
    "}\n";

SbBool generateTextureMap(SoNode *root, SoTexture2 *texture, 
                          short textureWidth, short textureHeight)
{
    // Use the shared persistent renderer so we don't create a second GL context.
    // (In headless Mesa/GLX environments only one offscreen context can be
    // created successfully per process; a second attempt fails.)
    // The renderer always operates at DEFAULT_WIDTH x DEFAULT_HEIGHT.
    SoOffscreenRenderer *myRenderer = getSharedRenderer();
    myRenderer->setComponents(SoOffscreenRenderer::RGB);
    myRenderer->setBackgroundColor(SbColor(0.8, 0.8, 0.0));
    
    if (!myRenderer->render(root)) {
        return FALSE;
    }

    // Apply the rendered buffer as texture at full DEFAULT_WIDTH x DEFAULT_HEIGHT
    texture->image.setValue(SbVec2s(DEFAULT_WIDTH, DEFAULT_HEIGHT),
                           SoOffscreenRenderer::RGB, myRenderer->getBuffer());

    return TRUE;
}

int main(int argc, char **argv)
{
    // Initialize Coin for headless operation
    initCoinHeadless();

    // Make a scene to render into a texture
    SoSeparator *texRoot = new SoSeparator;
    texRoot->ref();

    // Parse the embedded red cone scene
    SoInput in;
    // sizeof() includes null terminator, so subtract 1 for actual string length
    in.setBuffer(red_cone_iv, sizeof(red_cone_iv) - 1);
    SoSeparator *result = SoDB::readAll(&in);
    if (result == NULL) {
        fprintf(stderr, "Error: Could not parse scene\n");
        texRoot->unref();
        return 1;
    }

    // Set up camera and lighting for texture generation
    SoPerspectiveCamera *myCamera = new SoPerspectiveCamera;
    SoRotationXYZ *rot = new SoRotationXYZ;
    rot->axis = SoRotationXYZ::X;
    rot->angle = M_PI_2;
    myCamera->position.setValue(SbVec3f(-0.2, -0.2, 2.0));
    myCamera->scaleHeight(0.4);
    
    texRoot->addChild(myCamera);
    texRoot->addChild(new SoDirectionalLight);
    texRoot->addChild(rot);
    texRoot->addChild(result);

    myCamera->viewAll(texRoot, SbViewportRegion());

    // Generate the texture map
    SoTexture2 *texture = new SoTexture2;
    texture->ref();
    
    printf("Generating texture map (128x128)...\n");
    if (generateTextureMap(texRoot, texture, 128, 128)) {
        printf("Successfully generated texture map\n");
    } else {
        fprintf(stderr, "Error: Could not generate texture map\n");
        texture->unref();
        texRoot->unref();
        return 1;
    }
    texRoot->unref();

    // Make a scene with a cube and apply the texture to it
    SoSeparator *root = new SoSeparator;
    root->ref();

    // Add camera and light for final scene
    SoPerspectiveCamera *camera = new SoPerspectiveCamera;
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);

    // Add the texture and cube
    root->addChild(texture);
    root->addChild(new SoCube);

    // Setup camera
    camera->viewAll(root, SbViewportRegion(DEFAULT_WIDTH, DEFAULT_HEIGHT));

    const char *baseFilename = (argc > 1) ? argv[1] : "09.2.Texture";
    char filename[256];

    // Render cube with generated texture - front view
    printf("\nRendering textured cube...\n");
    snprintf(filename, sizeof(filename), "%s_front.rgb", baseFilename);
    renderToFile(root, filename);

    // Rotate to see different sides of the textured cube
    SoRotation *cubeRot = new SoRotation;
    root->insertChild(cubeRot, root->getNumChildren() - 1);
    
    cubeRot->rotation.setValue(SbVec3f(0, 1, 0), M_PI / 4);
    snprintf(filename, sizeof(filename), "%s_angle1.rgb", baseFilename);
    renderToFile(root, filename);

    cubeRot->rotation.setValue(SbVec3f(1, 1, 0), M_PI / 3);
    snprintf(filename, sizeof(filename), "%s_angle2.rgb", baseFilename);
    renderToFile(root, filename);

    texture->unref();
    root->unref();

    printf("\nSuccessfully completed offscreen texture rendering example\n");
    return 0;
}
