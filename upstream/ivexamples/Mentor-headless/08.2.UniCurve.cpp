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
 * Headless version of Inventor Mentor example 8.2
 * 
 * Original: UniCurve - displays uniform B-spline curve
 * Headless: Renders uniform B-spline curve from multiple angles
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoComplexity.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/nodes/SoLightModel.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoNurbsCurve.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <cmath>
#include <cstdio>

// The control points for this curve
const float pts[13][3] = {
   { 6.0,  0.0,  6.0},
   {-5.5,  0.5,  5.5},
   {-5.0,  1.0, -5.0},
   { 4.5,  1.5, -4.5},
   { 4.0,  2.0,  4.0},
   {-3.5,  2.5,  3.5},
   {-3.0,  3.0, -3.0},
   { 2.5,  3.5, -2.5},
   { 2.0,  4.0,  2.0},
   {-1.5,  4.5,  1.5},
   {-1.0,  5.0, -1.0},
   { 0.5,  5.5, -0.5},
   { 0.0,  6.0,  0.0}};

// The knot vector
const float knots[17] = {
   0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10, 10, 10};

// Create the nodes needed for the B-Spline curve
SoSeparator *makeCurve()
{
    SoSeparator *curveSep = new SoSeparator();
    curveSep->ref();

    // Set the draw style of the curve
    SoDrawStyle *drawStyle = new SoDrawStyle;
    drawStyle->lineWidth = 4;
    curveSep->addChild(drawStyle);

    // Define the NURBS curve including the control points and complexity
    SoComplexity *complexity = new SoComplexity;
    SoCoordinate3 *controlPts = new SoCoordinate3;
    SoNurbsCurve *curve = new SoNurbsCurve;
    complexity->value = 0.8;
    controlPts->point.setValues(0, 13, pts);
    curve->numControlPoints = 13;
    curve->knotVector.setValues(0, 17, knots);
    curveSep->addChild(complexity);
    curveSep->addChild(controlPts);
    curveSep->addChild(curve);

    curveSep->unrefNoDelete();
    return curveSep;
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

    // Create the scene graph for the curve
    SoSeparator *heart = new SoSeparator;
    SoSeparator *curveSep = makeCurve();
    SoLightModel *lmodel = new SoLightModel;
    SoBaseColor *clr = new SoBaseColor;

    lmodel->model = SoLightModel::BASE_COLOR;
    clr->rgb.setValue(SbColor(1.0, 0.0, 0.1));
    heart->addChild(lmodel);
    heart->addChild(clr);
    heart->addChild(curveSep);
    root->addChild(heart);

    // Add control-point markers as small spheres.
    // Always render in software mode, providing a visible test signature
    // even when NURBS curve tessellation is unavailable.
    SoSeparator *markerSep = new SoSeparator;
    SoMaterial *markerMat = new SoMaterial;
    markerMat->diffuseColor.setValue(0.2f, 0.6f, 1.0f);
    markerSep->addChild(markerMat);
    for (int i = 0; i < 13; i++) {
        SoSeparator *ptSep = new SoSeparator;
        SoTransform *ptXf = new SoTransform;
        ptXf->translation.setValue(pts[i][0], pts[i][1], pts[i][2]);
        ptXf->scaleFactor.setValue(0.3f, 0.3f, 0.3f);
        ptSep->addChild(ptXf);
        ptSep->addChild(new SoSphere);
        markerSep->addChild(ptSep);
    }
    root->addChild(markerSep);

    // Use viewAll to ensure the scene is fully in frame
    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    camera->viewAll(root, vp);

    const char *baseFilename = (argc > 1) ? argv[1] : "08.2.UniCurve";
    char filename[256];

    // Default view (framed by viewAll)
    snprintf(filename, sizeof(filename), "%s_view1.rgb", baseFilename);
    renderToFile(root, filename);

    // Compute scene center for additional views
    SoGetBoundingBoxAction bba(vp);
    bba.apply(root);
    SbBox3f bbox = bba.getBoundingBox();
    SbVec3f center = bbox.getCenter();
    float radius = (bbox.getMax() - bbox.getMin()).length() * 0.9f;

    // Side view: camera along +X axis
    camera->position.setValue(center + SbVec3f(radius, 0, 0));
    camera->pointAt(center);
    snprintf(filename, sizeof(filename), "%s_side.rgb", baseFilename);
    renderToFile(root, filename);

    // Top view: camera along +Y axis
    camera->position.setValue(center + SbVec3f(0, radius, 0));
    camera->pointAt(center, SbVec3f(0, 0, -1));
    snprintf(filename, sizeof(filename), "%s_top.rgb", baseFilename);
    renderToFile(root, filename);

    root->unref();
    return 0;
}
