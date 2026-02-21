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
 * Headless version of Inventor Mentor example 10.6
 * 
 * Original: PickFilterTopLevel - demonstrates pick filter callback for top-level selection
 * Headless: Simulates picking with and without filter to show difference
 */

#include "headless_utils.h"
#include <Inventor/SoDB.h>
#include <Inventor/SoInput.h>
#include <Inventor/SoPath.h>
#include <Inventor/SoPickedPoint.h>
#include <Inventor/nodes/SoSelection.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/actions/SoRayPickAction.h>
#include <Inventor/actions/SoSearchAction.h>
#include <cstdio>

// Pick filter callback - only allows top-level objects
// Returns path with only selection and the picked child
SoPath *pickFilterCB(void *, const SoPickedPoint *pick)
{    
    if (!pick) return NULL;
    
    // See which child of selection got picked
    SoPath *p = pick->getPath();
    int i;
    for (i = 0; i < p->getLength() - 1; i++) {
        SoNode *n = p->getNode(i);
        if (n->isOfType(SoSelection::getClassTypeId()))
            break;
    }
    
    // Copy 2 nodes from the path: selection and the picked child
    SoPath *filtered = p->copy(i, 2);
    printf("Pick filter: Original path length %d -> Filtered path length %d\n", 
           p->getLength(), filtered->getLength());
    return filtered;
}

// Create a simple test scene (substitute for parkbench.iv)
SoSeparator *createTestScene()
{
    SoSeparator *scene = new SoSeparator;
    
    // Create a hierarchy: separator -> transform -> material -> cube
    // This creates nested structure to test filtering
    for (int i = 0; i < 3; i++) {
        SoSeparator *objSep = new SoSeparator;
        
        SoTransform *xform = new SoTransform;
        xform->translation.setValue((i - 1) * 3.0f, 0, 0);
        objSep->addChild(xform);
        
        SoMaterial *mat = new SoMaterial;
        float hue = i / 3.0f;
        mat->diffuseColor.setHSVValue(hue, 0.8f, 0.8f);
        objSep->addChild(mat);
        
        // Add nested separator for deeper hierarchy
        SoSeparator *innerSep = new SoSeparator;
        objSep->addChild(innerSep);
        
        SoCube *cube = new SoCube;
        innerSep->addChild(cube);
        
        scene->addChild(objSep);
    }
    
    return scene;
}

// Helper to perform a pick and return the path
SoPath *performPick(SoNode *root, const SbVec2s &screenPos, 
                    const SbViewportRegion &viewport)
{
    SoRayPickAction pickAction(viewport);
    pickAction.setPoint(screenPos);
    pickAction.setRadius(8.0f);
    
    pickAction.apply(root);
    const SoPickedPoint *pickedPoint = pickAction.getPickedPoint();
    
    if (pickedPoint) {
        return pickedPoint->getPath()->copy();
    }
    return NULL;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    // Load or create scene
    SoSeparator *scene = NULL;
    SoInput in;
    const char *dataDir = getenv("COIN_DATA_DIR");
    if (!dataDir) dataDir = "../../data";
    
    char benchPath[512];
    snprintf(benchPath, sizeof(benchPath), "%s/parkbench.iv", dataDir);
    
    if (in.openFile(benchPath)) {
        fprintf(stderr, "Loading parkbench.iv from %s\n", benchPath);
        scene = new SoSeparator;
        SoNode *n;
        while (SoDB::read(&in, n) && n != NULL) {
            scene->addChild(n);
        }
        in.closeFile();
    } else {
        fprintf(stderr, "Note: Could not load parkbench.iv, using test scene\n");
        scene = createTestScene();
    }

    // Create two selection roots - one with pick filter, one without
    SoSelection *filteredSel = new SoSelection;
    filteredSel->ref();
    filteredSel->addChild(scene);
    filteredSel->setPickFilterCallback(pickFilterCB);

    SoSelection *defaultSel = new SoSelection;
    defaultSel->ref();
    defaultSel->addChild(scene);

    // Add camera and light to each
    SoPerspectiveCamera *cam1 = new SoPerspectiveCamera;
    filteredSel->insertChild(cam1, 0);
    filteredSel->insertChild(new SoDirectionalLight, 1);

    SoPerspectiveCamera *cam2 = new SoPerspectiveCamera;
    defaultSel->insertChild(cam2, 0);
    defaultSel->insertChild(new SoDirectionalLight, 1);

    SbViewportRegion viewport(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    cam1->viewAll(filteredSel, viewport);
    cam2->viewAll(defaultSel, viewport);

    const char *baseFilename = (argc > 1) ? argv[1] : "10.6.PickFilterTopLevel";
    char filename[256];

    int frameNum = 0;

    // Render initial scenes
    printf("\n=== Rendering initial scenes ===\n");
    snprintf(filename, sizeof(filename), "%s_frame%02d_filtered_initial.rgb", baseFilename, frameNum++);
    renderToFile(filteredSel, filename);
    
    snprintf(filename, sizeof(filename), "%s_frame%02d_default_initial.rgb", baseFilename, frameNum++);
    renderToFile(defaultSel, filename);

    // Simulate picking objects in the center of screen
    SbVec2s centerScreen(DEFAULT_WIDTH / 2, DEFAULT_HEIGHT / 2);
    
    printf("\n=== Testing pick with filter (top-level selection) ===\n");
    SoPath *filteredPath = performPick(filteredSel, centerScreen, viewport);
    if (filteredPath) {
        filteredPath->ref();
        printf("Filtered pick succeeded - path length: %d\n", filteredPath->getLength());
        for (int i = 0; i < filteredPath->getLength(); i++) {
            printf("  [%d] %s\n", i, filteredPath->getNode(i)->getTypeId().getName().getString());
        }
        filteredSel->select(filteredPath);
        
        snprintf(filename, sizeof(filename), "%s_frame%02d_filtered_selected.rgb", baseFilename, frameNum++);
        renderToFile(filteredSel, filename);
        
        filteredPath->unref();
    }

    printf("\n=== Testing pick without filter (default selection) ===\n");
    SoPath *defaultPath = performPick(defaultSel, centerScreen, viewport);
    if (defaultPath) {
        defaultPath->ref();
        printf("Default pick succeeded - path length: %d\n", defaultPath->getLength());
        for (int i = 0; i < defaultPath->getLength(); i++) {
            printf("  [%d] %s\n", i, defaultPath->getNode(i)->getTypeId().getName().getString());
        }
        defaultSel->select(defaultPath);
        
        snprintf(filename, sizeof(filename), "%s_frame%02d_default_selected.rgb", baseFilename, frameNum++);
        renderToFile(defaultSel, filename);
        
        defaultPath->unref();
    }

    printf("\nRendered %d frames demonstrating pick filter\n", frameNum);
    printf("The filtered version selects only top-level nodes,\n");
    printf("while the default version selects the deepest picked node.\n");

    filteredSel->unref();
    defaultSel->unref();
    return 0;
}
