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
 * Headless version of Inventor Mentor example 9.4
 * 
 * Original: PickAction - demonstrates pick actions with mouse interaction
 * Headless: Simulates pick actions at calculated screen positions of objects
 */

#include "headless_utils.h"
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoDB.h>
#include <Inventor/SoInput.h>
#include <Inventor/SoPickedPoint.h>
#include <Inventor/actions/SoRayPickAction.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoRotationXYZ.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <cmath>
#include <cstdio>

// Convert world coordinates to screen coordinates
SbVec2s worldToScreen(const SbVec3f &worldPos, SoCamera *camera, 
                      const SbViewportRegion &viewport)
{
    // Get camera parameters
    SbViewVolume vv = camera->getViewVolume(viewport.getViewportAspectRatio());
    
    // Project world position to normalized device coordinates
    SbVec3f ndc;
    vv.projectToScreen(worldPos, ndc);
    
    // Convert to screen coordinates
    SbVec2s vpSize = viewport.getViewportSizePixels();
    short x = (short)(ndc[0] * vpSize[0]);
    short y = (short)(ndc[1] * vpSize[1]);
    
    return SbVec2s(x, y);
}

// Get center of an object's bounding box
SbVec3f getObjectCenter(SoNode *node, const SbViewportRegion &viewport)
{
    SoGetBoundingBoxAction bboxAction(viewport);
    bboxAction.apply(node);
    SbBox3f bbox = bboxAction.getBoundingBox();
    return bbox.getCenter();
}

// Perform pick action and return picked path
SoPath* performPick(SoNode *root, const SbVec2s &screenPos,
                    const SbViewportRegion &viewport)
{
    SoRayPickAction pickAction(viewport);
    pickAction.setPoint(screenPos);
    pickAction.setRadius(8.0f);
    
    pickAction.apply(root);
    const SoPickedPoint *pickedPoint = pickAction.getPickedPoint();
    
    if (pickedPoint) {
        return pickedPoint->getPath();
    }
    return NULL;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = new SoSeparator;
    root->ref();

    // Add camera and light
    SoPerspectiveCamera *myCamera = new SoPerspectiveCamera;
    root->addChild(myCamera);
    root->addChild(new SoDirectionalLight);

    // Read star object from file (simulate by creating a simple object)
    // In the original, it reads "star.iv" from data directory
    // For headless version, we'll create two simple cubes instead
    SoInput in;
    SoSeparator *starObject = NULL;
    
    // Try to load the actual star.iv file
    const char *dataDir = getenv("COIN_DATA_DIR");
    if (!dataDir) dataDir = "../../data";
    
    char starPath[512];
    snprintf(starPath, sizeof(starPath), "%s/star.iv", dataDir);
    
    if (in.openFile(starPath)) {
        starObject = SoDB::readAll(&in);
        in.closeFile();
    }
    
    // If we couldn't load the star, create a simple substitute
    if (!starObject) {
        fprintf(stderr, "Note: Could not load star.iv, using cube substitute\n");
        starObject = new SoSeparator;
        SoCube *cube = new SoCube;
        cube->width = 2.0f;
        cube->height = 2.0f;
        cube->depth = 2.0f;
        starObject->addChild(cube);
    }
    starObject->ref();

    // Add rotation to tilt the scene
    SoRotationXYZ *myRotation = new SoRotationXYZ;
    myRotation->axis.setValue(SoRotationXYZ::X);
    myRotation->angle.setValue(M_PI/2.2);
    root->addChild(myRotation);

    // First star object (white by default)
    SoSeparator *star1Sep = new SoSeparator;
    root->addChild(star1Sep);
    star1Sep->addChild(starObject);

    // Second star object (red)
    SoMaterial *myMaterial = new SoMaterial;
    myMaterial->diffuseColor.setValue(1.0, 0.0, 0.0);
    root->addChild(myMaterial);
    
    SoTranslation *myTranslation = new SoTranslation;
    myTranslation->translation.setValue(1., 0., 1.);
    root->addChild(myTranslation);
    
    SoSeparator *star2Sep = new SoSeparator;
    root->addChild(star2Sep);
    star2Sep->addChild(starObject);

    // Setup camera
    SbViewportRegion viewport(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    myCamera->viewAll(root, viewport);

    const char *baseFilename = (argc > 1) ? argv[1] : "09.4.PickAction";
    char filename[256];

    int frameNum = 0;

    // Render initial scene
    snprintf(filename, sizeof(filename), "%s_initial.rgb", baseFilename);
    renderToFile(root, filename);
    frameNum++;

    // Simulate picking each object
    // Get centers of the two star objects in world coordinates
    SbVec3f star1Center = getObjectCenter(star1Sep, viewport);
    SbVec3f star2Center = getObjectCenter(star2Sep, viewport);

    // Convert to screen coordinates
    SbVec2s star1Screen = worldToScreen(star1Center, myCamera, viewport);
    SbVec2s star2Screen = worldToScreen(star2Center, myCamera, viewport);

    printf("Star 1 center: world (%g, %g, %g) -> screen (%d, %d)\n",
           star1Center[0], star1Center[1], star1Center[2],
           star1Screen[0], star1Screen[1]);
    printf("Star 2 center: world (%g, %g, %g) -> screen (%d, %d)\n",
           star2Center[0], star2Center[1], star2Center[2],
           star2Screen[0], star2Screen[1]);

    // Perform pick on first star
    SoPath *pickedPath1 = performPick(root, star1Screen, viewport);
    if (pickedPath1) {
        printf("\nPicked first star at screen position (%d, %d)\n", 
               star1Screen[0], star1Screen[1]);
        printf("Pick path length: %d\n", pickedPath1->getLength());
        for (int i = 0; i < pickedPath1->getLength(); i++) {
            SoNode *node = pickedPath1->getNode(i);
            printf("  [%d] %s\n", i, node->getTypeId().getName().getString());
        }
        
        // Highlight by changing material
        SoMaterial *highlightMat = new SoMaterial;
        highlightMat->emissiveColor.setValue(0.3f, 0.3f, 0.0f);
        star1Sep->insertChild(highlightMat, 0);
        
        snprintf(filename, sizeof(filename), "%s_pick_star1.rgb", baseFilename);
        renderToFile(root, filename);
        frameNum++;
        
        star1Sep->removeChild(0);
    }

    // Perform pick on second star
    SoPath *pickedPath2 = performPick(root, star2Screen, viewport);
    if (pickedPath2) {
        printf("\nPicked second star at screen position (%d, %d)\n",
               star2Screen[0], star2Screen[1]);
        printf("Pick path length: %d\n", pickedPath2->getLength());
        for (int i = 0; i < pickedPath2->getLength(); i++) {
            SoNode *node = pickedPath2->getNode(i);
            printf("  [%d] %s\n", i, node->getTypeId().getName().getString());
        }
        
        // Highlight by changing material
        SoMaterial *highlightMat = new SoMaterial;
        highlightMat->emissiveColor.setValue(0.3f, 0.0f, 0.0f);
        star2Sep->insertChild(highlightMat, 0);
        
        snprintf(filename, sizeof(filename), "%s_pick_star2.rgb", baseFilename);
        renderToFile(root, filename);
        frameNum++;
        
        star2Sep->removeChild(0);
    }

    printf("\nRendered %d frames demonstrating pick action\n", frameNum);

    starObject->unref();
    root->unref();
    return 0;
}
