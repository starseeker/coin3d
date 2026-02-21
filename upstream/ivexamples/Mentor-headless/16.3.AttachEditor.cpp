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
 * Headless version of Inventor Mentor example 16.3
 * 
 * Original: AttachEditor - Material editor attached to material node
 * Headless: Demonstrates mock material editor attachment pattern
 * 
 * This example demonstrates:
 * - Bidirectional material editor attachment (toolkit-agnostic)
 * - Editor automatically updates when material changes programmatically
 * - Material automatically updates when editor changes
 * - The pattern ANY toolkit must implement for attached editors
 * 
 * Key insight: Material attachment is a generic pattern.
 * The editor maintains a reference to the material node and:
 * 1. Syncs its UI when the material changes externally
 * 2. Updates the material when user edits values
 * 
 * This works the same in Qt, FLTK, Xt, or any toolkit that can
 * display property controls and handle user input.
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

int main(int argc, char **argv)
{
    printf("=== Mentor Example 16.3: Attach Material Editor ===\n");
    printf("This demonstrates toolkit-agnostic material editor attachment\n\n");
    
    // Initialize Coin for headless operation
    initCoinHeadless();
    
    // Mock toolkit initialization
    void* mockWindow = mockToolkitInit(argv[0]);
    if (!mockWindow) {
        fprintf(stderr, "Failed to initialize mock toolkit\n");
        return 1;
    }
    
    // Build the render area
    MockRenderArea* myRenderArea = new MockRenderArea(800, 600);
    
    // Build the material editor
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
    const char *dataDir16_3 = getenv("COIN_DATA_DIR");
    if (dataDir16_3) {
        SoInput::addDirectoryFirst(dataDir16_3);
    } else {
        SoInput::addDirectoryFirst("../../data");
        SoInput::addDirectoryFirst("data");
    }
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
    
    // Set the scene graph 
    myRenderArea->setSceneGraph(root);

    const char *baseFilename = (argc > 1) ? argv[1] : "16.3.AttachEditor";
    char filename[512];
    
    // Render initial state with default material
    printf("\n--- State 1: Default material (before attach) ---\n");
    snprintf(filename, sizeof(filename), "%s_default.rgb", baseFilename);
    myRenderArea->render(filename);
    
    // Attach material editor to the material
    printf("\n--- Attaching editor to material node ---\n");
    myEditor->attach(myMaterial);
    printf("Editor is now synchronized with material node\n");
    
    // Simulate user changing material through editor
    printf("\n--- State 2: User edits to red via attached editor ---\n");
    myEditor->setDiffuseColor(SbColor(1.0f, 0.0f, 0.0f));
    myEditor->setAmbientColor(SbColor(0.3f, 0.0f, 0.0f));
    myEditor->setSpecularColor(SbColor(0.5f, 0.5f, 0.5f));
    myEditor->setShininess(0.5f);
    snprintf(filename, sizeof(filename), "%s_red.rgb", baseFilename);
    myRenderArea->render(filename);
    
    // Verify material node was updated
    SbColor diffuse = myMaterial->diffuseColor[0];
    printf("Material node diffuse color: (%.2f, %.2f, %.2f)\n", 
           diffuse[0], diffuse[1], diffuse[2]);
    
    // Change to blue
    printf("\n--- State 3: User edits to blue via attached editor ---\n");
    myEditor->setDiffuseColor(SbColor(0.0f, 0.3f, 1.0f));
    myEditor->setAmbientColor(SbColor(0.0f, 0.1f, 0.3f));
    myEditor->setSpecularColor(SbColor(0.8f, 0.8f, 0.8f));
    myEditor->setShininess(0.8f);
    snprintf(filename, sizeof(filename), "%s_blue.rgb", baseFilename);
    myRenderArea->render(filename);
    
    // Change to green
    printf("\n--- State 4: User edits to green via attached editor ---\n");
    myEditor->setDiffuseColor(SbColor(0.0f, 0.8f, 0.1f));
    myEditor->setAmbientColor(SbColor(0.0f, 0.3f, 0.05f));
    myEditor->setSpecularColor(SbColor(0.6f, 0.6f, 0.6f));
    myEditor->setShininess(0.6f);
    snprintf(filename, sizeof(filename), "%s_green.rgb", baseFilename);
    myRenderArea->render(filename);
    
    // Demonstrate programmatic material change also syncs to editor
    printf("\n--- State 5: Programmatic material change (should sync to editor) ---\n");
    SoMaterial *tempMaterial = new SoMaterial;
    tempMaterial->ref();
    tempMaterial->diffuseColor.setValue(1.0f, 0.5f, 0.0f);  // Orange
    tempMaterial->ambientColor.setValue(0.3f, 0.15f, 0.0f);
    myMaterial->copyFieldValues(tempMaterial);
    tempMaterial->unref();
    // In a real GUI editor, this would update the UI controls
    printf("Material changed programmatically - attached editor syncs automatically\n");
    snprintf(filename, sizeof(filename), "%s_orange.rgb", baseFilename);
    myRenderArea->render(filename);

    printf("\n=== Summary ===\n");
    printf("Generated 5 images showing bidirectional material editor attachment\n");
    printf("\nKey architectural point:\n");
    printf("Material editor attachment is a GENERIC pattern for any toolkit.\n");
    printf("\nThe editor must:\n");
    printf("  1. Keep reference to attached material node\n");
    printf("  2. Update material when user edits values\n");
    printf("  3. Update UI when material changes externally\n");
    printf("\nCoin provides:\n");
    printf("  - SoMaterial node with fields\n");
    printf("  - Field change notifications (for editor UI updates)\n");
    printf("  - Scene graph rendering\n");
    printf("\nToolkit provides:\n");
    printf("  - UI controls (color pickers, sliders, etc.)\n");
    printf("  - Event handling (user input)\n");
    printf("  - Display/window management\n");
    printf("\nThis pattern works with Qt, FLTK, Xt, web UI, or any toolkit.\n");

    // Cleanup
    delete myEditor;
    delete myRenderArea;
    root->unref();

    return 0;
}
