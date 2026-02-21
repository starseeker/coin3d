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
 * Headless version of Inventor Mentor example 8.4
 * 
 * Original: TrimSurf - displays trimmed NURBS surface
 * Headless: Renders trimmed NURBS surface from multiple angles
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoComplexity.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoNurbsSurface.h>
#include <Inventor/nodes/SoProfileCoordinate2.h>
#include <Inventor/nodes/SoNurbsProfile.h>
#include <Inventor/nodes/SoProfile.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <cmath>
#include <cstdio>

// The array of trim coordinates (only 10 defined, matches original)
const float tpts[10][2] = {
   {0.0, 0.0},
   {1.0, 0.0},
   {1.0, 1.0},
   {0.0, 1.0},
   {0.2, 0.2},
   {0.2, 0.7},
   {0.9, 0.7},
   {0.9, 0.2},
   {0.7, 0.0},
   {0.4, 0.8}};

// The 16 coordinates defining the Bezier surface
const float pts[16][3] = {
   {-4.5, -2.0,  8.0},
   {-2.0,  1.0,  8.0},
   { 2.0, -3.0,  6.0},
   { 5.0, -1.0,  8.0},
   {-3.0,  3.0,  4.0},
   { 0.0, -1.0,  4.0},
   { 1.0, -1.0,  4.0},
   { 3.0,  2.0,  4.0},
   {-5.0, -2.0, -2.0},
   {-2.0, -4.0, -2.0},
   { 2.0, -1.0, -2.0},
   { 5.0,  0.0, -2.0},
   {-4.5,  2.0, -6.0},
   {-2.0, -4.0, -5.0},
   { 2.0,  3.0, -5.0},
   { 4.5, -2.0, -6.0}};

// The 3 knot vectors for the 3 trim curves
const float tknots1[7] = {0, 0, 1, 2, 3, 4, 4};
const float tknots2[6] = {0, 0, 1, 2, 3, 3};
const float tknots3[8] = {0, 0, 0, 0, 1, 1, 1, 1};

// The Bezier knot vector for the surface
const float knots[8] = {0, 0, 0, 0, 1, 1, 1, 1};

// Create the nodes needed for the Bezier patch and its trim curves
SoSeparator *makeSurface()
{
    SoSeparator *surfSep = new SoSeparator();
    surfSep->ref();

    // Define the Bezier surface including the control points and complexity
    SoComplexity *complexity = new SoComplexity;
    SoCoordinate3 *controlPts = new SoCoordinate3;
    SoNurbsSurface *surface = new SoNurbsSurface;
    complexity->value = 0.7;
    controlPts->point.setValues(0, 16, pts);
    surface->numUControlPoints.setValue(4);
    surface->numVControlPoints.setValue(4);
    surface->uKnotVector.setValues(0, 8, knots);
    surface->vKnotVector.setValues(0, 8, knots);
    surfSep->addChild(complexity);
    surfSep->addChild(controlPts);

    // Define trim curves
    SoProfileCoordinate2 *trimPts = new SoProfileCoordinate2;
    SoNurbsProfile *nTrim1 = new SoNurbsProfile;
    SoNurbsProfile *nTrim2 = new SoNurbsProfile;
    SoNurbsProfile *nTrim3 = new SoNurbsProfile;
    int32_t trimInds[5];

    trimPts->point.setValues(0, 10, tpts);
    trimInds[0] = 0;
    trimInds[1] = 1;
    trimInds[2] = 2;
    trimInds[3] = 3;
    trimInds[4] = 0;
    nTrim1->index.setValues(0, 5, trimInds);
    nTrim1->knotVector.setValues(0, 7, tknots1);
    trimInds[0] = 4;
    trimInds[1] = 5;
    trimInds[2] = 6;
    trimInds[3] = 7;
    nTrim2->linkage.setValue(SoProfile::START_NEW);
    nTrim2->index.setValues(0, 4, trimInds);
    nTrim2->knotVector.setValues(0, 6, tknots2);
    trimInds[0] = 7;
    trimInds[1] = 8;
    trimInds[2] = 9;
    trimInds[3] = 4;
    nTrim3->linkage.setValue(SoProfile::ADD_TO_CURRENT);
    nTrim3->index.setValues(0, 4, trimInds);
    nTrim3->knotVector.setValues(0, 8, tknots3);

    surfSep->addChild(trimPts);
    surfSep->addChild(nTrim1);
    surfSep->addChild(nTrim2);
    surfSep->addChild(nTrim3);
    surfSep->addChild(surface);

    surfSep->unrefNoDelete();
    return surfSep;
}

int main(int argc, char **argv)
{
    // Initialize Coin for headless operation
    initCoinHeadless();

    SoSeparator *root = new SoSeparator;
    root->ref();

    // Add camera and light
    SoPerspectiveCamera *camera = new SoPerspectiveCamera;
    root->addChild(camera);
    root->addChild(new SoDirectionalLight);

    // Add material for the surface
    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.8, 0.3, 0.1);
    root->addChild(mat);

    // Create the trimmed Bezier surface
    SoSeparator *surfSep = makeSurface();
    root->addChild(surfSep);

    // Setup camera
    camera->position.setValue(SbVec3f(-6.0, 8.0, 20.0));
    camera->pointAt(SbVec3f(0.0, 0.0, 0.0));

    const char *baseFilename = (argc > 1) ? argv[1] : "08.4.TrimSurf";
    char filename[256];

    // View from camera position
    snprintf(filename, sizeof(filename), "%s_view1.rgb", baseFilename);
    renderToFile(root, filename);

    // Side view
    camera->position.setValue(SbVec3f(20.0, 0.0, 0.0));
    camera->pointAt(SbVec3f(0.0, 0.0, 0.0));
    snprintf(filename, sizeof(filename), "%s_side.rgb", baseFilename);
    renderToFile(root, filename);

    // Top view
    camera->position.setValue(SbVec3f(0.0, 20.0, 0.0));
    camera->pointAt(SbVec3f(0.0, 0.0, 0.0));
    snprintf(filename, sizeof(filename), "%s_top.rgb", baseFilename);
    renderToFile(root, filename);

    root->unref();
    return 0;
}
