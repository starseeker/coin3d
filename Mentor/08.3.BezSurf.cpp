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
 * Headless version of Inventor Mentor example 8.3
 * 
 * Original: BezSurf - displays Bezier surface with floor
 * Headless: Renders Bezier surface from multiple angles
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoComplexity.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoNurbsSurface.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <cmath>
#include <cstdio>

// The control points for this surface
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

// The knot vector
const float knots[8] = {
   0, 0, 0, 0, 1, 1, 1, 1};

// Create the nodes needed for the Bezier surface
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
    surface->numUControlPoints = 4;
    surface->numVControlPoints = 4;
    surface->uKnotVector.setValues(0, 8, knots);
    surface->vKnotVector.setValues(0, 8, knots);
    surfSep->addChild(complexity);
    surfSep->addChild(controlPts);
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

    // Create the Bezier surface
    SoSeparator *surfSep = makeSurface();
    root->addChild(surfSep);

    // Setup camera
    camera->position.setValue(SbVec3f(-6.0, 8.0, 20.0));
    camera->pointAt(SbVec3f(0.0, 0.0, 0.0));

    const char *baseFilename = (argc > 1) ? argv[1] : "08.3.BezSurf";
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
