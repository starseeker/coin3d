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
 * Headless version of Inventor Mentor example 15.3
 * 
 * Original: AttachManip - Demonstrates attaching/detaching manipulators
 * Headless: Shows manipulator attachment without interactive manipulation
 * 
 * This example demonstrates how manipulators can be attached to different
 * objects in a scene. Three different manipulator types are shown:
 * - SoHandleBoxManip for the sphere
 * - SoTrackballManip for the cube
 * - SoTransformBoxManip for a node kit
 * 
 * In headless mode, we programmatically attach/detach manipulators and
 * render the scene to show the different manipulator types.
 */

#include "headless_utils.h"
#include <Inventor/SoDB.h>
#include <Inventor/SoInput.h>
#include <Inventor/manips/SoHandleBoxManip.h>
#include <Inventor/manips/SoTrackballManip.h>
#include <Inventor/manips/SoTransformBoxManip.h>
#include <Inventor/nodekits/SoBaseKit.h>
#include <Inventor/nodekits/SoWrapperKit.h>
#include <Inventor/nodes/SoCamera.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/nodes/SoLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSelection.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoShape.h>
#include <Inventor/actions/SoSearchAction.h>
#include <cstdio>

// Global data
SoSeparator *root;
SoHandleBoxManip    *myHandleBox;
SoTrackballManip    *myTrackball;
SoTransformBoxManip *myTransformBox;
SoPath *handleBoxPath    = NULL;
SoPath *trackballPath    = NULL;
SoPath *transformBoxPath = NULL;

// Is this node of a type that is influenced by transforms?
SbBool isTransformable(SoNode *myNode)
{
    if (myNode->isOfType(SoGroup::getClassTypeId())
        || myNode->isOfType(SoShape::getClassTypeId())
        || myNode->isOfType(SoCamera::getClassTypeId())
        || myNode->isOfType(SoLight::getClassTypeId()))
        return TRUE;
    else 
        return FALSE;
}

//  Create a path to the transform node that affects the tail
//  of the input path.  Three possible cases:
//   [1] The path-tail is a node kit. Just ask the node kit for
//       a path to the part called "transform"
//   [2] The path-tail is NOT a group.  Search siblings of path
//       tail from right to left until you find a transform. If
//       none is found, or if another transformable object is 
//       found (shape,group,light,or camera), then insert a 
//       transform just to the left of the tail. This way, the 
//       manipulator only effects the selected object.
//   [3] The path-tail IS a group.  Search its children left to
//       right until a transform is found. If a transformable
//       node is found first, insert a transform just left of 
//       that node.  This way the manip will affect all nodes
//       in the group.
SoPath *createTransformPath(SoPath *inputPath)
{
    int pathLength = inputPath->getLength();
    if (pathLength < 2) // Won't be able to get parent of tail
        return NULL;

    SoNode *tail = inputPath->getTail();

    // CASE 1: The tail is a node kit.
    // Nodekits have built in policy for creating parts.
    // The kit copies inputPath, then extends it past the 
    // kit all the way down to the transform. It creates the
    // transform if necessary.
    if (tail->isOfType(SoBaseKit::getClassTypeId())) {
        SoBaseKit *kit = (SoBaseKit *) tail;
        return kit->createPathToPart("transform", TRUE, inputPath);
    }

    SoTransform *editXf = NULL;
    SoGroup     *parent;
    SbBool      existedBefore = FALSE;

    // CASE 2: The tail is not a group.
    SbBool isTailGroup;
    isTailGroup = tail->isOfType(SoGroup::getClassTypeId());
    if (!isTailGroup) {
        // 'parent' is node above tail. Search under parent right
        // to left for a transform. If we find a 'movable' node
        // insert a transform just left of tail.  
        parent = (SoGroup *) inputPath->getNode(pathLength - 2);
        int tailIndx = parent->findChild(tail);

        for (int i = tailIndx; (i >= 0) && (editXf == NULL); i--) {
            SoNode *myNode = parent->getChild(i);
            if (myNode->isOfType(SoTransform::getClassTypeId()))
                editXf = (SoTransform *) myNode;
            else if (i != tailIndx && (isTransformable(myNode)))
                break;
        }
        if (editXf == NULL) {
            existedBefore = FALSE;
            editXf = new SoTransform;
            parent->insertChild(editXf, tailIndx);
        }
        else
            existedBefore = TRUE;
    }
    // CASE 3: The tail is a group.
    else {
        // Search the children from left to right for transform 
        // nodes. Stop the search if we come to a movable node.
        // and insert a transform before it.
        parent = (SoGroup *) tail;
        int i;
        for (i = 0;
             (i < parent->getNumChildren()) && (editXf == NULL); 
             i++) {
            SoNode *myNode = parent->getChild(i);
            if (myNode->isOfType(SoTransform::getClassTypeId()))
                editXf = (SoTransform *) myNode;
            else if (isTransformable(myNode))
                break;
        }
        if (editXf == NULL) {
            existedBefore = FALSE;
            editXf = new SoTransform;
            parent->insertChild(editXf, i);
        }
        else 
            existedBefore = TRUE;
    }

    // Create 'pathToXform.' Copy inputPath, then make last
    // node be editXf.
    SoPath *pathToXform = NULL;
    pathToXform = inputPath->copy();
    pathToXform->ref();
    if (!isTailGroup) // pop off the last entry.
        pathToXform->pop();
    // add editXf to the end
    int xfIndex   = parent->findChild(editXf);
    pathToXform->append(xfIndex);
    pathToXform->unrefNoDelete();

    return pathToXform;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    // Create the scene graph
    root = new SoSeparator;
    root->ref();

    // Add camera and light
    SoPerspectiveCamera *camera = new SoPerspectiveCamera;
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);

    // Create a cube with its own transform (left side)
    SoSeparator *cubeRoot = new SoSeparator;
    SoTransform *cubeXform = new SoTransform;
    cubeXform->translation.setValue(-2.5, 0, 0);
    root->addChild(cubeRoot);
    cubeRoot->addChild(cubeXform);

    SoMaterial *cubeMat = new SoMaterial;
    cubeMat->diffuseColor.setValue(.8, .8, .8);
    cubeRoot->addChild(cubeMat);
    cubeRoot->addChild(new SoCube);

    // Add a sphere node without a transform (center)
    // (one will be added when we attach the manipulator)
    SoSeparator *sphereRoot = new SoSeparator;
    SoMaterial *sphereMat = new SoMaterial;
    root->addChild(sphereRoot);
    sphereMat->diffuseColor.setValue(.8, .8, .8);
    sphereRoot->addChild(sphereMat);
    sphereRoot->addChild(new SoSphere);

    // Add a simple cone for the third object (right side)
    // Using a wrapper kit like in original, but simplified
    SoSeparator *coneRoot = new SoSeparator;
    SoTransform *coneXform = new SoTransform;
    coneXform->translation.setValue(2.5, 0, 0);
    root->addChild(coneRoot);
    coneRoot->addChild(coneXform);
    
    SoMaterial *coneMat = new SoMaterial;
    coneMat->diffuseColor.setValue(.8, .8, .8);
    coneRoot->addChild(coneMat);
    
    SoCone *cone = new SoCone;
    coneRoot->addChild(cone);

    // Create the manipulators
    myHandleBox = new SoHandleBoxManip;
    myHandleBox->ref();
    myTrackball = new SoTrackballManip;
    myTrackball->ref();
    myTransformBox = new SoTransformBoxManip;
    myTransformBox->ref();

    // Setup camera to view the scene
    SbViewportRegion viewport(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    camera->viewAll(root, viewport);

    const char *baseFilename = (argc > 1) ? argv[1] : "15.3.AttachManip";
    char filename[256];

    int frameNum = 0;

    // Render initial scene without manipulators
    printf("\n=== Manipulator Attachment Demo ===\n");
    printf("Frame %d: Initial scene (no manipulators)\n", frameNum);
    snprintf(filename, sizeof(filename), "%s_frame%02d_initial.rgb", baseFilename, frameNum++);
    renderToFile(root, filename);

    // Create paths to the objects for manipulator attachment
    SoSearchAction search;

    // Attach HandleBox to sphere
    printf("\nFrame %d: Attaching HandleBox manipulator to sphere\n", frameNum);
    search.reset();
    search.setType(SoSphere::getClassTypeId());
    search.setInterest(SoSearchAction::FIRST);
    search.apply(root);
    if (search.getPath()) {
        SoPath *spherePath = search.getPath()->copy();
        spherePath->ref();
        handleBoxPath = createTransformPath(spherePath);
        if (handleBoxPath) {
            handleBoxPath->ref();
            myHandleBox->replaceNode(handleBoxPath);
            sphereMat->diffuseColor.setValue(1, 0.2, 0.2); // Highlight when selected
        }
        spherePath->unref();
    }
    snprintf(filename, sizeof(filename), "%s_frame%02d_sphere_handlebox.rgb", baseFilename, frameNum++);
    renderToFile(root, filename);

    // Detach from sphere
    printf("Frame %d: Detaching manipulator from sphere\n", frameNum);
    if (handleBoxPath) {
        myHandleBox->replaceManip(handleBoxPath, new SoTransform);
        handleBoxPath->unref();
        handleBoxPath = NULL;
        sphereMat->diffuseColor.setValue(.8, .8, .8);
    }
    snprintf(filename, sizeof(filename), "%s_frame%02d_sphere_detached.rgb", baseFilename, frameNum++);
    renderToFile(root, filename);

    // Attach Trackball to cube
    printf("\nFrame %d: Attaching Trackball manipulator to cube\n", frameNum);
    search.reset();
    search.setType(SoCube::getClassTypeId());
    search.setInterest(SoSearchAction::FIRST);
    search.apply(root);
    if (search.getPath()) {
        SoPath *cubePath = search.getPath()->copy();
        cubePath->ref();
        trackballPath = createTransformPath(cubePath);
        if (trackballPath) {
            trackballPath->ref();
            myTrackball->replaceNode(trackballPath);
            cubeMat->diffuseColor.setValue(0.2, 1, 0.2); // Highlight when selected
        }
        cubePath->unref();
    }
    snprintf(filename, sizeof(filename), "%s_frame%02d_cube_trackball.rgb", baseFilename, frameNum++);
    renderToFile(root, filename);

    // Detach from cube
    printf("Frame %d: Detaching manipulator from cube\n", frameNum);
    if (trackballPath) {
        myTrackball->replaceManip(trackballPath, new SoTransform);
        trackballPath->unref();
        trackballPath = NULL;
        cubeMat->diffuseColor.setValue(.8, .8, .8);
    }
    snprintf(filename, sizeof(filename), "%s_frame%02d_cube_detached.rgb", baseFilename, frameNum++);
    renderToFile(root, filename);

    // Attach TransformBox to cone
    printf("\nFrame %d: Attaching TransformBox manipulator to cone\n", frameNum);
    search.reset();
    search.setType(SoCone::getClassTypeId());
    search.setInterest(SoSearchAction::FIRST);
    search.apply(root);
    if (search.getPath()) {
        SoPath *conePath = search.getPath()->copy();
        conePath->ref();
        transformBoxPath = createTransformPath(conePath);
        if (transformBoxPath) {
            transformBoxPath->ref();
            myTransformBox->replaceNode(transformBoxPath);
            coneMat->diffuseColor.setValue(0.2, 0.2, 1); // Highlight when selected
        }
        conePath->unref();
    }
    snprintf(filename, sizeof(filename), "%s_frame%02d_cone_transformbox.rgb", baseFilename, frameNum++);
    renderToFile(root, filename);

    // Detach from cone
    printf("Frame %d: Detaching manipulator from cone\n", frameNum);
    if (transformBoxPath) {
        myTransformBox->replaceManip(transformBoxPath, new SoTransform);
        transformBoxPath->unref();
        transformBoxPath = NULL;
        coneMat->diffuseColor.setValue(.8, .8, .8);
    }
    snprintf(filename, sizeof(filename), "%s_frame%02d_cone_detached.rgb", baseFilename, frameNum++);
    renderToFile(root, filename);

    printf("\n=== Summary ===\n");
    printf("Demonstrated three manipulator types:\n");
    printf("  - SoHandleBoxManip: Box with corner/edge/face handles\n");
    printf("  - SoTrackballManip: Sphere with rotation bands\n");
    printf("  - SoTransformBoxManip: Box with scale/rotate handles\n");
    printf("In interactive mode, users would drag these handles to transform objects.\n");
    printf("Rendered %d frames showing attachment/detachment\n", frameNum);

    myHandleBox->unref();
    myTrackball->unref();
    myTransformBox->unref();
    root->unref();

    return 0;
}
