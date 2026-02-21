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
 * Headless version of Inventor Mentor example 14.2
 * 
 * Original: Editors - Material and directional light editors attached to nodekits
 * Headless: Demonstrates mock editor patterns with nodekits
 * 
 * This example demonstrates:
 * - SoSceneKit with lightList, cameraList, and childList organization
 * - SoWrapperKit for wrapping external geometry
 * - Material editor attached to nodekit material part
 * - Directional light editor attached to light within SoLightKit
 * - Multiple editor coordination
 * - NodeKit part access with SO_GET_PART macro pattern
 * 
 * Key insight: NodeKit organization and editor attachment are toolkit-agnostic.
 * The editors don't need actual UI - the attachment pattern and synchronization
 * logic work identically regardless of how the editor UI is implemented.
 */

#include "headless_utils.h"
#include "mock_gui_toolkit.h"
#include <Inventor/SoDB.h>
#include <Inventor/SoInput.h>
#include <Inventor/nodekits/SoCameraKit.h>
#include <Inventor/nodekits/SoLightKit.h>
#include <Inventor/nodekits/SoSceneKit.h>
#include <Inventor/nodekits/SoWrapperKit.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoSeparator.h>
#include <cstdio>

int main(int argc, char **argv)
{
    printf("=== Mentor Example 14.2: NodeKit Editors ===\n");
    printf("This demonstrates toolkit-agnostic editor patterns with nodekits\n\n");
    
    // Initialize Coin for headless operation
    initCoinHeadless();
    
    // Mock toolkit initialization
    void* mockWindow = mockToolkitInit(argv[0]);
    if (!mockWindow) {
        fprintf(stderr, "Failed to initialize mock toolkit\n");
        return 1;
    }

    // SCENE! Create an SoSceneKit
    SoSceneKit *myScene = new SoSceneKit;
    myScene->ref();

    // LIGHTS! Add an SoLightKit to the "lightList." 
    // The SoLightKit creates an SoDirectionalLight by default.
    printf("Setting up scene with SoLightKit...\n");
    myScene->setPart("lightList[0]", new SoLightKit);

    // CAMERA!! Add an SoCameraKit to the "cameraList." 
    // The SoCameraKit creates an SoPerspectiveCamera by default.
    printf("Adding SoCameraKit...\n");
    myScene->setPart("cameraList[0]", new SoCameraKit);
    myScene->setCameraNumber(0);

    // Read an object from file
    printf("Reading desk.iv...\n");
    SoInput myInput;
    // Search data dir: prefer COIN_DATA_DIR env var, then relative paths that
    // work both when run from source directory and from the build test_output dir.
    const char *dataDir = getenv("COIN_DATA_DIR");
    if (dataDir) {
        SoInput::addDirectoryFirst(dataDir);
    } else {
        SoInput::addDirectoryFirst("../../data");
        SoInput::addDirectoryFirst("data");
    }
    if (!myInput.openFile("desk.iv")) {
        fprintf(stderr, "Error: Could not open desk.iv\n");
        fprintf(stderr, "Make sure data/desk.iv exists\n");
        myScene->unref();
        return 1;
    }
    SoSeparator *fileContents = SoDB::readAll(&myInput);
    if (fileContents == NULL) {
        fprintf(stderr, "Error: Could not read desk.iv\n");
        myScene->unref();
        return 1;
    }

    // OBJECT!! Create an SoWrapperKit and set its contents to
    // be what was read from file.
    printf("Creating SoWrapperKit for desk...\n");
    SoWrapperKit *myDesk = new SoWrapperKit();
    myDesk->setPart("contents", fileContents);
    myScene->setPart("childList[0]", myDesk);

    // Give the desk a good starting color
    myDesk->set("material { diffuseColor .8 .3 .1 }");

    // Create mock render area
    MockRenderArea* myRenderArea = new MockRenderArea(800, 600);

    // Set up Camera with ViewAll
    // Use the SO_GET_PART macro pattern to get the camera node.
    // The part we ask for is 'cameraList[0].camera' (which is of type 
    // SoPerspectiveCamera), not 'cameraList[0]' (which is SoCameraKit).
    printf("Setting up camera...\n");
    SoPerspectiveCamera *myCamera = (SoPerspectiveCamera*)
        myScene->getPart("cameraList[0].camera", TRUE);
    if (!myCamera) {
        fprintf(stderr, "Error: Could not get camera from scene\n");
        myScene->unref();
        delete myRenderArea;
        return 1;
    }
    SbViewportRegion myRegion(myRenderArea->getSize());
    myCamera->viewAll(myScene, myRegion);

    myRenderArea->setSceneGraph(myScene);
    myRenderArea->setTitle("NodeKit Editors Demo");

    const char *baseFilename = (argc > 1) ? argv[1] : "14.2.Editors";
    char filename[512];

    // Render initial state
    printf("\n--- State 1: Initial desk with default lighting ---\n");
    snprintf(filename, sizeof(filename), "%s_initial.rgb", baseFilename);
    myRenderArea->render(filename);

    // MATERIAL EDITOR!! Attach it to myDesk's material node.
    // Use the getPart pattern to get this part from myDesk.
    printf("\n--- Creating material editor and attaching to desk material ---\n");
    MockMaterialEditor *mtlEditor = new MockMaterialEditor();
    SoMaterial *mtl = (SoMaterial*)myDesk->getPart("material", TRUE);
    if (!mtl) {
        fprintf(stderr, "Error: Could not get material from desk\n");
        myScene->unref();
        delete myRenderArea;
        delete mtlEditor;
        return 1;
    }
    mtlEditor->attach(mtl);
    mtlEditor->setTitle("Material of Desk");
    printf("Material editor attached to desk material\n");

    // DIRECTIONAL LIGHT EDITOR!! Attach it to the 
    // SoDirectionalLight node within the SoLightKit we made.
    printf("\n--- Creating light editor and attaching to directional light ---\n");
    MockDirectionalLightEditor *ltEditor = new MockDirectionalLightEditor();
    SoPath *ltPath = myScene->createPathToPart("lightList[0].light", TRUE);
    if (!ltPath) {
        fprintf(stderr, "Error: Could not create path to light\n");
        myScene->unref();
        delete myRenderArea;
        delete mtlEditor;
        delete ltEditor;
        return 1;
    }
    ltEditor->attach(ltPath);
    ltEditor->setTitle("Lighting of Desk");
    printf("Light editor attached to directional light\n");

    // Simulate user changing material to a darker wood color
    printf("\n--- State 2: User changes desk to darker wood via material editor ---\n");
    mtlEditor->setDiffuseColor(SbColor(0.5f, 0.25f, 0.1f));
    mtlEditor->setAmbientColor(SbColor(0.15f, 0.075f, 0.03f));
    mtlEditor->setSpecularColor(SbColor(0.3f, 0.3f, 0.3f));
    mtlEditor->setShininess(0.3f);
    snprintf(filename, sizeof(filename), "%s_dark_wood.rgb", baseFilename);
    myRenderArea->render(filename);

    // Simulate user changing light direction
    printf("\n--- State 3: User changes light direction via light editor ---\n");
    ltEditor->setDirection(SbVec3f(1.0f, -1.0f, -1.0f));
    snprintf(filename, sizeof(filename), "%s_light_direction.rgb", baseFilename);
    myRenderArea->render(filename);

    // Simulate user making light brighter and more yellow
    printf("\n--- State 4: User changes light color and intensity ---\n");
    ltEditor->setColor(SbColor(1.0f, 1.0f, 0.8f));  // Warm white
    ltEditor->setIntensity(1.2f);
    snprintf(filename, sizeof(filename), "%s_warm_bright_light.rgb", baseFilename);
    myRenderArea->render(filename);

    // Simulate user changing material to lighter finish
    printf("\n--- State 5: User changes desk to lighter finish ---\n");
    mtlEditor->setDiffuseColor(SbColor(0.9f, 0.7f, 0.4f));
    mtlEditor->setAmbientColor(SbColor(0.3f, 0.2f, 0.1f));
    mtlEditor->setShininess(0.6f);
    snprintf(filename, sizeof(filename), "%s_light_finish.rgb", baseFilename);
    myRenderArea->render(filename);

    // Simulate turning light off and back on
    printf("\n--- State 6: User turns light off (demonstrates on/off control) ---\n");
    ltEditor->setOn(FALSE);
    snprintf(filename, sizeof(filename), "%s_light_off.rgb", baseFilename);
    myRenderArea->render(filename);

    printf("\n--- State 7: User turns light back on ---\n");
    ltEditor->setOn(TRUE);
    snprintf(filename, sizeof(filename), "%s_light_on.rgb", baseFilename);
    myRenderArea->render(filename);

    printf("\n=== Summary ===\n");
    printf("Generated 7 images showing nodekit editor patterns\n");
    printf("\nKey architectural insights:\n");
    printf("\n1. NodeKit Organization (100%% toolkit-agnostic):\n");
    printf("   - SoSceneKit organizes scene with lightList, cameraList, childList\n");
    printf("   - SoLightKit, SoCameraKit provide structured light/camera management\n");
    printf("   - SoWrapperKit wraps external geometry into nodekit structure\n");
    printf("   - All part access through getPart() or createPathToPart()\n");
    printf("\n2. Editor Attachment Pattern (generic for any toolkit):\n");
    printf("   - Material editor attaches to nodekit material part\n");
    printf("   - Light editor attaches to path or node within nodekit\n");
    printf("   - Editors synchronize with attached nodes automatically\n");
    printf("   - Multiple editors can coordinate on same scene\n");
    printf("\n3. Toolkit Responsibilities (minimal):\n");
    printf("   - Display editor controls (sliders, color pickers, direction controls)\n");
    printf("   - Call editor methods when user changes values\n");
    printf("   - Update controls when attached nodes change externally\n");
    printf("   - Display scene rendering\n");
    printf("\n4. Coin Responsibilities:\n");
    printf("   - NodeKit structure and part management\n");
    printf("   - Material and light field management\n");
    printf("   - Field change notifications\n");
    printf("   - Scene graph rendering\n");
    printf("\nThis EXACT pattern works with:\n");
    printf("  - Qt (custom property editor widgets)\n");
    printf("  - FLTK (Fl_Value_Slider, Fl_Color_Chooser)\n");
    printf("  - Xt/Motif (SoXtMaterialEditor, SoXtDirectionalLightEditor) [original]\n");
    printf("  - Win32 (native dialogs and controls)\n");
    printf("  - Web (HTML sliders, color inputs)\n");
    printf("  - ImGui (immediate mode GUI)\n");
    printf("  - Headless/mock (for testing core logic)\n");

    // Cleanup
    delete ltEditor;
    delete mtlEditor;
    delete myRenderArea;
    myScene->unref();

    return 0;
}
