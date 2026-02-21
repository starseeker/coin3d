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
 * Headless version of Inventor Mentor example 10.8
 * 
 * Original: PickFilterNodeKit - Pick filter with material editor (Xt-dependent)
 * Headless: Demonstrates toolkit-agnostic pick filtering and editor patterns
 * 
 * This example demonstrates:
 * - Pick filter callbacks (completely toolkit-agnostic)
 * - Material editor integration with selection (toolkit-agnostic pattern)
 * - NodeKit selection and material editing
 * - How ANY toolkit can implement this same functionality
 * 
 * Key insight: The CORE LOGIC is toolkit-independent:
 * - SoSelection handles picking and maintains selected paths
 * - Pick filter callback truncates paths to nodekits
 * - Material editor updates selected nodekit materials
 * - Selection callbacks coordinate editor with selection
 * 
 * The original used Xt/Motif for:
 * - Window creation (not needed for core logic)
 * - ExaminerViewer widget (just a render area + camera controls)
 * - MaterialEditor widget (property editor with UI controls)
 * 
 * ALL of this core Coin logic works without any GUI toolkit!
 */

#include "headless_utils.h"
#include "mock_gui_toolkit.h"
#include <Inventor/SoPath.h>
#include <Inventor/SoPickedPoint.h>
#include <Inventor/actions/SoBoxHighlightRenderAction.h>
#include <Inventor/nodekits/SoShapeKit.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSelection.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <cstdio>
#include <cmath>

struct UserData {
    SoSelection *sel;
    MockMaterialEditor *editor;
    SbBool ignore;
};

// Truncate the pick path so a nodekit is selected
// This is PURE Coin logic - no toolkit dependencies
SoPath *pickFilterCB(void *, const SoPickedPoint *pick)
{    
    // See which child of selection got picked
    SoPath *p = pick->getPath();
    int i;
    for (i = p->getLength() - 1; i >= 0; i--) {
        SoNode *n = p->getNode(i);
        if (n->isOfType(SoShapeKit::getClassTypeId()))
            break;
    }
    
    // Copy the path down to the nodekit
    return p->copy(0, i+1);
}

// Create a sample scene graph
SoNode *buildScene()
{
    SoGroup *g = new SoGroup;
    SoShapeKit *k;
    SoTransform *xf;
     
    // Place a dozen shapes in circular formation
    for (int i = 0; i < 12; i++) {
        k = new SoShapeKit;
        k->setPart("shape", new SoCube);
        xf = (SoTransform *) k->getPart("transform", TRUE);
        xf->translation.setValue(
            8.0f*sinf(i*float(M_PI)/6.0f), 
            8.0f*cosf(i*float(M_PI)/6.0f), 
            0.0f);
        g->addChild(k);
    }
     
    return g;
}

// Update the material editor to reflect the selected object
void selectCB(void *userData, SoPath *path)
{
    SoShapeKit *kit = (SoShapeKit *) path->getTail();
    SoMaterial *kitMtl = (SoMaterial *) kit->getPart("material", TRUE);
    
    UserData *ud = (UserData *) userData;
    ud->ignore = TRUE;
    ud->editor->setMaterial(*kitMtl);
    ud->ignore = FALSE;
    
    printf("Selection callback: Updated editor for selected nodekit\n");
}

// This is called when the user changes material in editor
// Updates the material part of each selected node kit
void mtlChangeCB(void *userData, const SoMaterial *mtl)
{
    // Ignore callback when we're just syncing editor to selection
    UserData *ud = (UserData *) userData;
    if (ud->ignore)
        return;

    printf("Material change callback: Updating %d selected nodekits\n", 
           ud->sel->getNumSelected());
    
    SoSelection *sel = ud->sel;
     
    // Update material for all selected nodekits
    for (int i = 0; i < sel->getNumSelected(); i++) {
        SoPath *p = sel->getPath(i);
        SoShapeKit *kit = (SoShapeKit *) p->getTail();
        SoMaterial *kitMtl = (SoMaterial *) kit->getPart("material", TRUE);
        kitMtl->copyFieldValues(mtl);
    }
}

int main(int, char **argv)
{
    printf("=== Mentor Example 10.8: Pick Filter for NodeKits ===\n");
    printf("This demonstrates toolkit-agnostic pick filtering and material editing\n");
    printf("\nOriginal used Xt/Motif for window/viewer/editor widgets\n");
    printf("This version shows ALL the core logic is toolkit-independent!\n\n");
    
    // Initialize Coin
    initCoinHeadless();
    
    // Mock toolkit initialization
    void* mockWindow = mockToolkitInit(argv[0]);
    if (!mockWindow) {
        fprintf(stderr, "Failed to initialize mock toolkit\n");
        return 1;
    }

    // Create our scene graph with selection node
    SoSelection *sel = new SoSelection;
    sel->ref();
    
    // Add camera and light
    SoPerspectiveCamera *camera = new SoPerspectiveCamera;
    camera->position.setValue(0, 0, 30);
    camera->heightAngle = float(M_PI) / 4.0f;
    sel->addChild(camera);
    sel->addChild(new SoDirectionalLight);
    
    // Add the scene content
    sel->addChild(buildScene());

    // Create a mock viewer (in real toolkit, would be ExaminerViewer widget)
    printf("Creating mock examiner viewer...\n");
    MockExaminerViewer *viewer = new MockExaminerViewer(800, 600);
    viewer->setSceneGraph(sel);
    viewer->setTitle("Select Node Kits");

    // Create a material editor (in real toolkit, would be a widget with UI controls)
    printf("Creating mock material editor...\n");
    MockMaterialEditor *ed = new MockMaterialEditor();

    // User data for our callbacks
    UserData userData;
    userData.sel = sel;
    userData.editor = ed;
    userData.ignore = FALSE;
    
    // Register callbacks - this is the KEY toolkit-agnostic pattern
    printf("Registering callbacks...\n");
    ed->addMaterialChangedCallback(mtlChangeCB, &userData);
    sel->setPickFilterCallback(pickFilterCB);
    sel->addSelectionCallback(selectCB, &userData);
    
    printf("\nCallbacks registered. Now simulating user interactions...\n");
    
    // Render initial scene
    printf("\n--- State 1: Initial scene (nothing selected) ---\n");
    viewer->render("10.8.PickFilterNodeKit-initial.rgb");
    
    // Simulate picking a nodekit by path
    // In real toolkit, user would click with mouse
    printf("\n--- Simulating pick on nodekit 0 (top) ---\n");
    SoPath *path0 = new SoPath(sel);
    path0->append(sel);
    path0->append(sel->getChild(2));  // First shape kit after camera and light
    path0->append(((SoShapeKit*)sel->getChild(2))->getChild(0));
    sel->select(path0);
    
    printf("--- State 2: Nodekit 0 selected (default material) ---\n");
    viewer->render("10.8.PickFilterNodeKit-selected-default.rgb");
    
    // Simulate user changing material to red in editor
    printf("\n--- User changes material to red in editor ---\n");
    SoMaterial redMtl;
    redMtl.diffuseColor.setValue(1.0f, 0.0f, 0.0f);
    redMtl.ambientColor.setValue(0.3f, 0.0f, 0.0f);
    redMtl.specularColor.setValue(0.5f, 0.5f, 0.5f);
    redMtl.shininess.setValue(0.5f);
    ed->setMaterial(redMtl);
    
    printf("--- State 3: Selected nodekit now red ---\n");
    viewer->render("10.8.PickFilterNodeKit-red.rgb");
    
    // Select a different nodekit
    printf("\n--- Simulating pick on nodekit 3 (right side) ---\n");
    sel->deselectAll();
    SoPath *path3 = new SoPath(sel);
    path3->append(sel);
    path3->append(sel->getChild(5));  // Different shape kit
    path3->append(((SoShapeKit*)sel->getChild(5))->getChild(0));
    sel->select(path3);
    
    printf("--- State 4: Different nodekit selected ---\n");
    printf("(Editor should sync to show this nodekit's material)\n");
    viewer->render("10.8.PickFilterNodeKit-select-different.rgb");
    
    // Change this one to blue
    printf("\n--- User changes this nodekit's material to blue ---\n");
    SoMaterial blueMtl;
    blueMtl.diffuseColor.setValue(0.0f, 0.3f, 1.0f);
    blueMtl.ambientColor.setValue(0.0f, 0.1f, 0.3f);
    blueMtl.specularColor.setValue(0.8f, 0.8f, 0.8f);
    blueMtl.shininess.setValue(0.8f);
    ed->setMaterial(blueMtl);
    
    printf("--- State 5: Now have both red and blue nodekits ---\n");
    viewer->render("10.8.PickFilterNodeKit-multiple-colors.rgb");
    
    // Select multiple nodekits
    printf("\n--- Selecting multiple nodekits ---\n");
    sel->deselectAll();
    sel->select(path0);
    
    SoPath *path6 = new SoPath(sel);
    path6->append(sel);
    path6->append(sel->getChild(8));  // Another shape kit
    path6->append(((SoShapeKit*)sel->getChild(8))->getChild(0));
    sel->select(path6);
    
    printf("--- State 6: Multiple nodekits selected ---\n");
    viewer->render("10.8.PickFilterNodeKit-multi-select.rgb");
    
    // Change material of all selected
    printf("\n--- User changes material to green (affects all selected) ---\n");
    SoMaterial greenMtl;
    greenMtl.diffuseColor.setValue(0.0f, 0.8f, 0.1f);
    greenMtl.ambientColor.setValue(0.0f, 0.3f, 0.05f);
    greenMtl.specularColor.setValue(0.6f, 0.6f, 0.6f);
    greenMtl.shininess.setValue(0.6f);
    ed->setMaterial(greenMtl);
    
    printf("--- State 7: Multiple nodekits changed to green ---\n");
    viewer->render("10.8.PickFilterNodeKit-multi-edit.rgb");

    printf("\n=== Summary ===\n");
    printf("Generated 7 images showing pick filtering and material editing\n");
    printf("\nKey architectural insights:\n");
    printf("\n1. Pick Filtering (100%% toolkit-agnostic):\n");
    printf("   - SoSelection::setPickFilterCallback() - Coin API\n");
    printf("   - Callback receives SoPickedPoint - Coin type\n");
    printf("   - Returns truncated SoPath - Coin type\n");
    printf("   - Works identically in ANY toolkit\n");
    printf("\n2. Material Editor Pattern (generic for any toolkit):\n");
    printf("   - Editor maintains callbacks for material changes\n");
    printf("   - Selection callback syncs editor to selected material\n");
    printf("   - Material change callback updates selected nodekits\n");
    printf("   - Ignore flag prevents callback loops\n");
    printf("\n3. Toolkit Responsibilities (minimal):\n");
    printf("   - Display scene (render area or viewer widget)\n");
    printf("   - Capture mouse clicks and translate to pick rays\n");
    printf("   - Display material controls (sliders, color pickers)\n");
    printf("   - Trigger redraws when scene changes\n");
    printf("\n4. Coin Responsibilities:\n");
    printf("   - Scene graph management (SoSelection, SoShapeKit)\n");
    printf("   - Pick action processing\n");
    printf("   - Path management\n");
    printf("   - Material field management\n");
    printf("   - Rendering\n");
    printf("\nThis EXACT pattern works with:\n");
    printf("  - Qt (QWidget viewer + QColorDialog editor)\n");
    printf("  - FLTK (Fl_Gl_Window viewer + Fl_Color_Chooser editor)\n");
    printf("  - Xt/Motif (SoXtExaminerViewer + SoXtMaterialEditor) [original]\n");
    printf("  - Win32 (native window + color picker dialog)\n");
    printf("  - Web (Canvas + HTML color inputs)\n");
    printf("  - Headless/mock (for testing core logic)\n");

    // Cleanup
    delete ed;
    delete viewer;
    sel->unref();

    return 0;
}
