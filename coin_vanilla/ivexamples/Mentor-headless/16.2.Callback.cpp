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
 * Headless version of Inventor Mentor example 16.2
 * 
 * Original: Callback - Material editor with callbacks
 * Headless: Demonstrates mock material editor callbacks
 * 
 * This example demonstrates:
 * - How a generic material editor can work with Coin (toolkit-agnostic)
 * - Material change callbacks for updating scene graph
 * - The pattern ANY toolkit must implement for property editors
 * 
 * Key insight: The material editor logic is toolkit-independent.
 * A toolkit only needs to:
 * 1. Display material color/property controls (sliders, color pickers, etc.)
 * 2. Call setMaterial() when user changes values
 * 3. Register callbacks to be notified of changes
 * 
 * The actual Coin integration (copying material values, triggering redraws)
 * works the same regardless of whether it's Qt, FLTK, Xt, or a mock.
 */

#include "headless_utils.h"
#include "mock_gui_toolkit.h"
#include <Inventor/SoDB.h>
#include <Inventor/SoInput.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoSeparator.h>
#include <cstdio>

//  This is called by the Material Editor when a value changes
void myMaterialEditorCB(void *userData, const SoMaterial *newMtl)
{
    SoMaterial *myMtl = (SoMaterial *) userData;

    printf("Material editor callback invoked - copying material values\n");
    
    // Copy all the fields from the new material
    myMtl->copyFieldValues(newMtl);
}

int main(int, char **argv)
{
    printf("=== Mentor Example 16.2: Material Editor Callback ===\n");
    printf("This demonstrates toolkit-agnostic material editor patterns\n\n");
    
    // Initialize Coin for headless operation
    initCoinHeadless();
    
    // Mock toolkit initialization (real toolkit would init X11, Qt, etc.)
    void* mockWindow = mockToolkitInit(argv[0]);
    if (!mockWindow) {
        fprintf(stderr, "Failed to initialize mock toolkit\n");
        return 1;
    }
    
    // Build the render area (in real toolkit, would be an actual window)
    MockRenderArea* myRenderArea = new MockRenderArea(800, 600);
    
    // Build the Material Editor (in real toolkit, would show GUI controls)
    MockMaterialEditor* myEditor = new MockMaterialEditor();
    
    // Create a scene graph
    SoSeparator *root = new SoSeparator;
    SoPerspectiveCamera *myCamera = new SoPerspectiveCamera;
    SoMaterial *myMaterial = new SoMaterial;
    
    root->ref();
    myCamera->position.setValue(0.212482f, -0.881014f, 2.5f);
    myCamera->heightAngle = float(M_PI)/4.0f; 
    root->addChild(myCamera);
    root->addChild(new SoDirectionalLight);
    root->addChild(myMaterial);

    // Read the geometry from a file and add to the scene
    SoInput myInput;
    SoInput::addDirectoryFirst("data");
    if (!myInput.openFile("dogDish.iv")) {
        fprintf(stderr, "Error: Could not open dogDish.iv\n");
        fprintf(stderr, "Make sure data/dogDish.iv exists\n");
        root->unref();
        delete myEditor;
        delete myRenderArea;
        return 1;
    }
    SoSeparator *geomObject = SoDB::readAll(&myInput);
    if (geomObject == NULL) {
        fprintf(stderr, "Error: Could not read dogDish.iv\n");
        root->unref();
        delete myEditor;
        delete myRenderArea;
        return 1;
    }
    root->addChild(geomObject);

    // Add a callback for when the material changes
    myEditor->addMaterialChangedCallback(myMaterialEditorCB, myMaterial); 

    // Set the scene graph
    myRenderArea->setSceneGraph(root);

    // Render initial state with default material
    printf("\n--- State 1: Default material ---\n");
    myRenderArea->render("16.2.Callback-default.rgb");

    // Simulate user changing material to red
    printf("\n--- State 2: User changes to red material ---\n");
    SoMaterial redMaterial;
    redMaterial.diffuseColor.setValue(1.0f, 0.0f, 0.0f);
    redMaterial.ambientColor.setValue(0.3f, 0.0f, 0.0f);
    redMaterial.specularColor.setValue(0.5f, 0.5f, 0.5f);
    redMaterial.shininess.setValue(0.5f);
    
    // User edits material in editor - triggers callback
    myEditor->setMaterial(redMaterial);
    myRenderArea->render("16.2.Callback-red.rgb");

    // Simulate user changing material to blue
    printf("\n--- State 3: User changes to blue material ---\n");
    SoMaterial blueMaterial;
    blueMaterial.diffuseColor.setValue(0.0f, 0.3f, 1.0f);
    blueMaterial.ambientColor.setValue(0.0f, 0.1f, 0.3f);
    blueMaterial.specularColor.setValue(0.8f, 0.8f, 0.8f);
    blueMaterial.shininess.setValue(0.8f);
    
    myEditor->setMaterial(blueMaterial);
    myRenderArea->render("16.2.Callback-blue.rgb");

    // Simulate user changing material to gold
    printf("\n--- State 4: User changes to gold material ---\n");
    SoMaterial goldMaterial;
    goldMaterial.diffuseColor.setValue(1.0f, 0.84f, 0.0f);
    goldMaterial.ambientColor.setValue(0.3f, 0.25f, 0.0f);
    goldMaterial.specularColor.setValue(1.0f, 1.0f, 0.5f);
    goldMaterial.shininess.setValue(0.9f);
    
    myEditor->setMaterial(goldMaterial);
    myRenderArea->render("16.2.Callback-gold.rgb");

    printf("\n=== Summary ===\n");
    printf("Generated 4 images showing different materials applied via editor callbacks\n");
    printf("\nKey architectural point:\n");
    printf("The material editor is a GENERIC pattern that works with any toolkit.\n");
    printf("The toolkit only provides:\n");
    printf("  1. UI controls (sliders, color pickers, etc.)\n");
    printf("  2. Calls to setMaterial() when user changes values\n");
    printf("  3. Callback registration mechanism\n");
    printf("\nCoin handles:\n");
    printf("  - Material field management\n");
    printf("  - Scene graph updates\n");
    printf("  - Rendering with new materials\n");
    printf("\nThis same pattern works in Qt, FLTK, Xt, or any other toolkit.\n");

    // Cleanup
    delete myEditor;
    delete myRenderArea;
    root->unref();

    return 0;
}
