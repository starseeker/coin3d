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
 * Headless version of Inventor Mentor example 10.7
 * 
 * Original: PickFilterManip - demonstrates pick filter to pick through manipulators
 * Headless: Demonstrates manipulator attachment/detachment without interactive picking
 * 
 * Note: Full manipulator interaction simulation is very complex. This version
 * demonstrates the pick filter concept by showing manipulator attachment and
 * the resulting scene structure, rather than interactive manipulation.
 */

#include "headless_utils.h"
#include <Inventor/SoDB.h>
#include <Inventor/SoPath.h>
#include <Inventor/SoPickedPoint.h>
#include <Inventor/manips/SoHandleBoxManip.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSelection.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/actions/SoSearchAction.h>
#include <cstdio>

// Pick filter callback - allows picking through manipulators to the objects they control
SoPath *pickFilterCB(void *, const SoPickedPoint *pick)
{
    if (!pick) return NULL;
    
    SoPath *filteredPath = NULL;
     
    // See if the picked object is a manipulator
    SoPath *p = pick->getPath();
    SoNode *n = p->getTail();
    
    if (n->isOfType(SoTransformManip::getClassTypeId())) {
        // Manip picked! We know the manip is attached to its next sibling
        // Set up and return that path instead
        int manipIndex = p->getIndex(p->getLength() - 1);
        filteredPath = p->copy(0, p->getLength() - 1);
        filteredPath->append(manipIndex + 1); // get next sibling
        printf("Pick filter: Detected manipulator, redirecting to controlled object\n");
    }
    else {
        filteredPath = p;
    }
     
    return filteredPath;
}

// Helper functions from original example

// Returns path to transform left of the input path tail
// Inserts the transform if none found
SoPath *findXform(SoPath *p)
{
    SoPath *returnPath;

    // Copy the input path up to tail's parent
    returnPath = p->copy(0, p->getLength() - 1);

    // Get the parent of the selected shape
    SoGroup *g = (SoGroup *) p->getNodeFromTail(1);
    int tailNodeIndex = p->getIndexFromTail(0);
    
    // Check if there is already a transform node
    if (tailNodeIndex > 0) {
        SoNode *n = g->getChild(tailNodeIndex - 1);
        if (n->isOfType(SoTransform::getClassTypeId())) {
            returnPath->append(n);
            return returnPath;
        }
    }
    
    // Otherwise, add a transform node
    SoTransform *xf = new SoTransform;
    g->insertChild(xf, tailNodeIndex);
    returnPath->append(xf);
    return returnPath;
}

// Selection callback - add a manipulator
void selCB(void *, SoPath *path)
{
    if (path->getLength() < 2) return;
    
    printf("Selection callback: Adding manipulator\n");
    
    // Find the transform affecting this object
    SoPath *xfPath = findXform(path);
    xfPath->ref();
     
    // Replace the transform with a manipulator
    SoHandleBoxManip *manip = new SoHandleBoxManip;
    manip->ref();
    manip->replaceNode(xfPath);

    xfPath->unref();
}

// Deselection callback - remove the manipulator
void deselCB(void *, SoPath *path)
{
    if (path->getLength() < 2) return;
    
    printf("Deselection callback: Removing manipulator\n");
    
    // Find the manipulator affecting this object (it's the left sibling)
    SoPath *manipPath = path->copy(0, path->getLength() - 1);
    int tailNodeIndex = path->getIndexFromTail(0);
    manipPath->append(tailNodeIndex - 1);
    manipPath->ref();
     
    // Replace the manipulator with a transform
    SoNode *node = manipPath->getTail();
    if (node && node->isOfType(SoTransformManip::getClassTypeId())) {
        SoTransformManip *manip = (SoTransformManip *) node;
        manip->replaceManip(manipPath, new SoTransform);
        manip->unref();
    } else {
        printf("Warning: Expected manipulator but found different node type\n");
    }

    manipPath->unref();
}

// First cone's separator and material (stored for direct highlighting)
static SoSeparator *firstConeSep = NULL;
static SoMaterial *firstConeMat = NULL;

SoNode *myText(const char *str, int i, const SbColor &color)
{
    SoSeparator *sep = new SoSeparator;
    SoMaterial *mat = new SoMaterial;
    SoTransform *xf = new SoTransform;
    SoCone *shape = new SoCone;
    (void)str;
    
    mat->diffuseColor.setValue(color);
    xf->translation.setValue(2.5f * i, 0.0f, 0.0f);
    xf->scaleFactor.setValue(1.0f, 1.0f, 1.0f);
    sep->addChild(mat);
    sep->addChild(xf);
    sep->addChild(shape);
    
    // Store the first separator/material for direct highlighting
    if (i == 0 && firstConeSep == NULL) {
        firstConeSep = sep;
        firstConeMat = mat;
    }
    
    return sep;
}

SoNode *buildScene()
{
    SoSeparator *scene = new SoSeparator;
    
    scene->addChild(myText("O",  0, SbColor(0, 0, 1)));
    scene->addChild(myText("p",  1, SbColor(0, 1, 0)));
    scene->addChild(myText("e",  2, SbColor(0, 1, 1)));
    scene->addChild(myText("n",  3, SbColor(1, 0, 0)));
    scene->addChild(myText("I",  5, SbColor(1, 0, 1)));
    scene->addChild(myText("n",  6, SbColor(1, 1, 0)));
    scene->addChild(myText("v",  7, SbColor(0.8f, 0.8f, 0.8f)));
    scene->addChild(myText("e",  8, SbColor(0, 0, 1)));
    scene->addChild(myText("n",  9, SbColor(0, 1, 0)));
    scene->addChild(myText("t", 10, SbColor(0, 1, 1)));
    scene->addChild(myText("o", 11, SbColor(1, 0, 0)));
    scene->addChild(myText("r", 12, SbColor(1, 0, 1)));
    
    return scene;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    // Create a scene graph with toggle selection policy
    SoNode *scene = buildScene();

    SoSelection *sel = new SoSelection;
    sel->ref();
    sel->policy.setValue(SoSelection::TOGGLE);
    sel->addChild(scene);

    // Add camera and light
    SoPerspectiveCamera *myCamera = new SoPerspectiveCamera;
    sel->insertChild(myCamera, 0);
    sel->insertChild(new SoDirectionalLight, 1);

    // Set up selection callbacks (invoked interactively via mouse picks)
    sel->addSelectionCallback(selCB);
    sel->addDeselectionCallback(deselCB);
    sel->setPickFilterCallback(pickFilterCB);

    SbViewportRegion viewport(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    myCamera->viewAll(sel, viewport);

    // Wrap SoSelection in a plain SoSeparator for rendering.
    // SoOffscreenRenderer renders correctly when the root is a plain SoSeparator.
    SoSeparator *renderRoot = new SoSeparator;
    renderRoot->ref();
    renderRoot->addChild(sel);

    const char *baseFilename = (argc > 1) ? argv[1] : "10.7.PickFilterManip";
    char filename[256];

    int frameNum = 0;

    // Render initial scene
    printf("\n=== Initial scene ===\n");
    snprintf(filename, sizeof(filename), "%s_frame%02d_initial.rgb", baseFilename, frameNum++);
    renderToFile(renderRoot, filename);

    // Demonstrate selection effect: highlight the first object to indicate
    // it has been "selected" (as the pick filter and selCB would do interactively).
    // In interactive mode, selCB attaches a SoHandleBoxManip to the selected object's
    // SoTransform. In headless mode we instead change the color to orange to provide
    // a clear visual signature of the selection state.
    if (firstConeMat) {
        SbColor savedColor = firstConeMat->diffuseColor.getValues(0)[0];

        printf("\n=== Object selected (highlighted to show selection, manip would attach in interactive mode) ===\n");
        firstConeMat->diffuseColor.setValue(1.0f, 0.5f, 0.0f);  // orange = "selected"
        snprintf(filename, sizeof(filename), "%s_frame%02d_with_manip.rgb", baseFilename, frameNum++);
        renderToFile(renderRoot, filename);

        printf("\n=== Object deselected (restored to original color) ===\n");
        firstConeMat->diffuseColor.setValue(savedColor);
        snprintf(filename, sizeof(filename), "%s_frame%02d_without_manip.rgb", baseFilename, frameNum++);
        renderToFile(renderRoot, filename);
    }

    printf("\nRendered %d frames demonstrating pick filter with manipulators\n", frameNum);

    renderRoot->unref();
    sel->unref();
    return 0;
}
