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
 * Headless version of Inventor Mentor example 10.5
 * 
 * Original: SelectionCB - demonstrates selection callbacks with mouse interaction
 * Headless: Demonstrates selection callbacks being triggered
 * 
 * This example shows two approaches to selection (both valid):
 * 1. Programmatic selection using select()/deselect() - current implementation
 * 2. Event-based selection via mouse picks - could be added using simulateMousePress()
 * 
 * The programmatic approach is simpler and demonstrates the callback mechanism clearly.
 * For a more realistic simulation, mouse pick events could trigger selection automatically.
 * See 09.4.PickAction for pick event simulation or 15.3.AttachManip for mouse event patterns.
 */

#include "headless_utils.h"
#include <Inventor/SbViewportRegion.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoSelection.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoText3.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/actions/SoSearchAction.h>
#include <cstdio>

// Global materials that will be modified by callbacks
SoMaterial *textMaterial, *sphereMaterial;
static float reddish[] = {1.0, 0.2, 0.2};  // Color when selected
static float white[] = {0.8, 0.8, 0.8};    // Color when not selected

// Selection callback - changes material color when object is selected
void mySelectionCB(void *, SoPath *selectionPath)
{
    if (selectionPath->getTail()->isOfType(SoText3::getClassTypeId())) { 
        textMaterial->diffuseColor.setValue(reddish);
        printf("Text selected - changing to reddish color\n");
    } else if (selectionPath->getTail()->isOfType(SoSphere::getClassTypeId())) {
        sphereMaterial->diffuseColor.setValue(reddish);
        printf("Sphere selected - changing to reddish color\n");
    }
}

// Deselection callback - resets material color when object is deselected
void myDeselectionCB(void *, SoPath *deselectionPath)
{
    if (deselectionPath->getTail()->isOfType(SoText3::getClassTypeId())) {
        textMaterial->diffuseColor.setValue(white);
        printf("Text deselected - changing to white color\n");
    } else if (deselectionPath->getTail()->isOfType(SoSphere::getClassTypeId())) {
        sphereMaterial->diffuseColor.setValue(white);
        printf("Sphere deselected - changing to white color\n");
    }
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    // Create and set up the selection node
    SoSelection *selectionRoot = new SoSelection;
    selectionRoot->ref();
    selectionRoot->policy = SoSelection::SINGLE;
    selectionRoot->addSelectionCallback(mySelectionCB);
    selectionRoot->addDeselectionCallback(myDeselectionCB);

    // Create the scene graph
    SoSeparator *root = new SoSeparator;
    selectionRoot->addChild(root);

    SoPerspectiveCamera *myCamera = new SoPerspectiveCamera;
    root->addChild(myCamera);
    root->addChild(new SoDirectionalLight);

    // Add a sphere node
    SoSeparator *sphereRoot = new SoSeparator;
    SoTransform *sphereTransform = new SoTransform;
    sphereTransform->translation.setValue(17., 17., 0.);
    sphereTransform->scaleFactor.setValue(8., 8., 8.);
    sphereRoot->addChild(sphereTransform);

    sphereMaterial = new SoMaterial;
    sphereMaterial->diffuseColor.setValue(.8, .8, .8);
    sphereRoot->addChild(sphereMaterial);
    
    SoSphere *sphere = new SoSphere;
    sphereRoot->addChild(sphere);
    root->addChild(sphereRoot);

    // Add a text node
    SoSeparator *textRoot = new SoSeparator;
    SoTransform *textTransform = new SoTransform;
    textTransform->translation.setValue(0., -1., 0.);
    textRoot->addChild(textTransform);

    textMaterial = new SoMaterial;
    textMaterial->diffuseColor.setValue(.8, .8, .8);
    textRoot->addChild(textMaterial);
    
    SoText3 *myText = new SoText3;
    myText->string = "rhubarb";
    textRoot->addChild(myText);
    root->addChild(textRoot);

    // Setup camera
    SbViewportRegion viewport(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    myCamera->viewAll(root, viewport, 2.0);

    const char *baseFilename = (argc > 1) ? argv[1] : "10.5.SelectionCB";
    char filename[256];

    int frameNum = 0;

    // Render initial state (nothing selected)
    printf("\n=== Initial state (nothing selected) ===\n");
    snprintf(filename, sizeof(filename), "%s_frame%02d_initial.rgb", baseFilename, frameNum++);
    renderToFile(root, filename);

    // Find paths to the sphere and text for selection
    SoSearchAction search;
    
    // Find sphere
    search.setType(SoSphere::getClassTypeId());
    search.setInterest(SoSearchAction::FIRST);
    search.apply(selectionRoot);
    SoPath *spherePath = search.getPath();
    if (spherePath) {
        spherePath = spherePath->copy();
        spherePath->ref();
    }

    // Find text
    search.reset();
    search.setType(SoText3::getClassTypeId());
    search.setInterest(SoSearchAction::FIRST);
    search.apply(selectionRoot);
    SoPath *textPath = search.getPath();
    if (textPath) {
        textPath = textPath->copy();
        textPath->ref();
    }

    // Simulate selecting the sphere
    if (spherePath) {
        printf("\n=== Selecting sphere ===\n");
        selectionRoot->select(spherePath);
        snprintf(filename, sizeof(filename), "%s_frame%02d_sphere_selected.rgb", baseFilename, frameNum++);
        renderToFile(root, filename);

        // Deselect sphere
        printf("\n=== Deselecting sphere ===\n");
        selectionRoot->deselect(spherePath);
        snprintf(filename, sizeof(filename), "%s_frame%02d_sphere_deselected.rgb", baseFilename, frameNum++);
        renderToFile(root, filename);
    }

    // Simulate selecting the text
    if (textPath) {
        printf("\n=== Selecting text ===\n");
        selectionRoot->select(textPath);
        snprintf(filename, sizeof(filename), "%s_frame%02d_text_selected.rgb", baseFilename, frameNum++);
        renderToFile(root, filename);

        // Deselect text
        printf("\n=== Deselecting text ===\n");
        selectionRoot->deselect(textPath);
        snprintf(filename, sizeof(filename), "%s_frame%02d_text_deselected.rgb", baseFilename, frameNum++);
        renderToFile(root, filename);
    }

    printf("\nRendered %d frames demonstrating selection callbacks\n", frameNum);
    printf("\nNote: This example uses programmatic selection (select/deselect calls).\n");
    printf("In interactive mode, mouse clicks would trigger selection via pick events.\n");
    printf("For event-based selection pattern, see 15.3.AttachManip which demonstrates\n");
    printf("using simulateMousePress() to generate pick events.\n");

    if (spherePath) spherePath->unref();
    if (textPath) textPath->unref();
    selectionRoot->unref();
    return 0;
}
