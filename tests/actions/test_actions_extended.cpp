/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
\**************************************************************************/

/**
 * @file test_actions_extended.cpp
 * @brief Additional action tests to improve actions/ coverage (41.3 %).
 *
 * Covers:
 *   SoGetPrimitiveCountAction
 *     - getTriangleCount / getLineCount / getPointCount on a scene
 *     - containsNoPrimitives on empty separator
 *     - setCanApproximate / canApproximateCount round-trip
 *     - setDecimationValue / getDecimationType / getDecimationPercentage
 *     - addNumTriangles / addNumLines / addNumPoints / addNumText / addNumImage
 *     - setCount3DTextAsTriangles / is3DTextCountedAsTriangles round-trip
 *     - getTextCount / getImageCount
 *   SoSearchAction
 *     - setNode / getPath finds specific node instance
 *     - setType / getPath finds first node of a type
 *     - setType + Interest::ALL + getPaths finds all nodes of a type
 *     - setName / getPath finds node by name
 *     - setFind(TYPE | NAME) compound search
 *     - reset() clears previous results
 *     - setSearchingAll / isSearchingAll round-trip
 *     - isFound() after apply
 *   SoGetMatrixAction
 *     - getMatrix / getInverse after applying to a translation node
 *   SoGetBoundingBoxAction
 *     - getBoundingBox on a unit cube is approx 2×2×2
 */

#include "../test_utils.h"

#include <Inventor/SoDB.h>
#include <Inventor/SoInput.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoType.h>
#include <Inventor/SoPath.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbBox3f.h>

#include <Inventor/actions/SoGetPrimitiveCountAction.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/actions/SoGetMatrixAction.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/elements/SoDecimationTypeElement.h>

#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoLineSet.h>
#include <Inventor/nodes/SoPointSet.h>
#include <Inventor/nodes/SoVertexProperty.h>

#include <cmath>
#include <cstring>
#include <cstdlib>

using namespace ObolTest;

static bool floatNear(float a, float b, float eps = 0.01f)
{
    return std::fabs(a - b) < eps;
}

TEST(ActionsExtended, SoGetPrimitiveCountActionGetTriangleCountOnCubeScene)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    root->addChild(new SoCube);

    SbViewportRegion vp(512, 512);
    SoGetPrimitiveCountAction action(vp);
    action.apply(root);

    int tris = action.getTriangleCount();
    EXPECT_TRUE((tris > 0)) << "Expected >0 triangles from a cube";
    root->unref();
}

TEST(ActionsExtended, SoGetPrimitiveCountActionContainsNoPrimitivesOnEmptySep)
{
    SoSeparator * root = new SoSeparator;
    root->ref();

    SbViewportRegion vp(512, 512);
    SoGetPrimitiveCountAction action(vp);
    action.apply(root);

    EXPECT_TRUE(action.containsNoPrimitives()) << "Empty scene should have no primitives";
    root->unref();
}

TEST(ActionsExtended, SoGetPrimitiveCountActionContainsNonTriangleShapesOnLineScene)
{
    // A LineSet contains line primitives, not triangles.
    SoSeparator * root = new SoSeparator;
    root->ref();

    SoVertexProperty * vp_node = new SoVertexProperty;
    SbVec3f pts[2] = { SbVec3f(0,0,0), SbVec3f(1,0,0) };
    vp_node->vertex.setValues(0, 2, pts);
    SoLineSet * ls = new SoLineSet;
    ls->vertexProperty = vp_node;
    root->addChild(ls);

    SbViewportRegion vp(512, 512);
    SoGetPrimitiveCountAction action(vp);
    action.apply(root);

    EXPECT_TRUE(action.containsNonTriangleShapes()) << "Line scene should have non-triangle shapes";
    root->unref();
}

TEST(ActionsExtended, SoGetPrimitiveCountActionSetCanApproximateRoundTrip)
{
    SbViewportRegion vp(512, 512);
    SoGetPrimitiveCountAction action(vp);
    action.setCanApproximate(TRUE);
    EXPECT_TRUE((action.canApproximateCount() == TRUE)) << "setCanApproximate/canApproximateCount round-trip failed";
}

TEST(ActionsExtended, SoGetPrimitiveCountActionSetDecimationValueRoundTrip)
{
    SbViewportRegion vp(512, 512);
    SoGetPrimitiveCountAction action(vp);
    action.setDecimationValue(SoDecimationTypeElement::PERCENTAGE, 0.5f);
    EXPECT_EQ(action.getDecimationType(), SoDecimationTypeElement::PERCENTAGE);
    EXPECT_TRUE(floatNear(action.getDecimationPercentage(), 0.5f));
}

TEST(ActionsExtended, SoGetPrimitiveCountActionSetCount3DTextAsTrianglesRoundTrip)
{
    SbViewportRegion vp(512, 512);
    SoGetPrimitiveCountAction action(vp);
    action.setCount3DTextAsTriangles(TRUE);
    EXPECT_TRUE((action.is3DTextCountedAsTriangles() == TRUE)) << "setCount3DTextAsTriangles/is3DTextCountedAsTriangles failed";
}

TEST(ActionsExtended, SoGetPrimitiveCountActionAddNumTrianglesLinesPoints)
{
    SbViewportRegion vp(512, 512);
    SoGetPrimitiveCountAction action(vp);
    // Apply to empty scene first to reset counters.
    SoSeparator * empty = new SoSeparator;
    empty->ref();
    action.apply(empty);
    empty->unref();

    // Now add manually.
    action.addNumTriangles(10);
    action.addNumLines(5);
    action.addNumPoints(3);
    action.addNumText(2);
    action.addNumImage(1);

    EXPECT_TRUE((action.getTriangleCount() >= 10) &&
                (action.getLineCount()     >= 5)  &&
                (action.getPointCount()    >= 3)  &&
                (action.getTextCount()     >= 2)  &&
                (action.getImageCount()    >= 1)) << "addNum* methods failed";
}

// =======================================================================
// SoSearchAction
// =======================================================================

TEST(ActionsExtended, SoSearchActionFindNodeByInstance)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    SoCube * cube = new SoCube;
    root->addChild(new SoSphere);
    root->addChild(cube);

    SoSearchAction sa;
    sa.setNode(cube);
    sa.apply(root);

    SoPath * path = sa.getPath();
    EXPECT_TRUE((path != nullptr) &&
                (path->getTail() == cube)) << "SoSearchAction by node failed";
    root->unref();
}

TEST(ActionsExtended, SoSearchActionFindFirstNodeOfType)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    root->addChild(new SoSphere);
    SoCube * cube = new SoCube;
    root->addChild(cube);

    SoSearchAction sa;
    sa.setType(SoCube::getClassTypeId());
    sa.apply(root);

    SoPath * path = sa.getPath();
    EXPECT_TRUE((path != nullptr) &&
                (path->getTail() == cube)) << "SoSearchAction find by type failed";
    root->unref();
}

TEST(ActionsExtended, SoSearchActionFindALLNodesOfType)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    root->addChild(new SoCube);
    root->addChild(new SoCube);
    root->addChild(new SoCube);

    SoSearchAction sa;
    sa.setType(SoCube::getClassTypeId(), TRUE);
    sa.setInterest(SoSearchAction::ALL);
    sa.apply(root);

    SoPathList & paths = sa.getPaths();
    EXPECT_TRUE((paths.getLength() == 3)) << "SoSearchAction ALL paths count wrong";
    root->unref();
}

TEST(ActionsExtended, SoSearchActionFindNodeByName)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    SoSphere * sphere = new SoSphere;
    sphere->setName("named_sphere_search");
    root->addChild(sphere);

    SoSearchAction sa;
    sa.setName(SbName("named_sphere_search"));
    sa.apply(root);

    SoPath * path = sa.getPath();
    EXPECT_TRUE((path != nullptr) && (path->getTail() == sphere)) << "SoSearchAction find by name failed";
    root->unref();
}

TEST(ActionsExtended, SoSearchActionSetFindGetterRoundTrip)
{
    SoSearchAction sa;
    sa.setFind(SoSearchAction::TYPE | SoSearchAction::NAME);
    int find = sa.getFind();
    EXPECT_TRUE((find == (SoSearchAction::TYPE | SoSearchAction::NAME))) << "SoSearchAction setFind/getFind failed";
}

TEST(ActionsExtended, SoSearchActionSetInterestGetterRoundTrip)
{
    SoSearchAction sa;
    sa.setInterest(SoSearchAction::LAST);
    EXPECT_TRUE((sa.getInterest() == SoSearchAction::LAST)) << "SoSearchAction setInterest/getInterest failed";
}

TEST(ActionsExtended, SoSearchActionSetSearchingAllRoundTrip)
{
    SoSearchAction sa;
    sa.setSearchingAll(TRUE);
    EXPECT_TRUE((sa.isSearchingAll() == TRUE)) << "setSearchingAll/isSearchingAll failed";
}

TEST(ActionsExtended, SoSearchActionResetClearsPreviousResult)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    SoCube * cube = new SoCube;
    root->addChild(cube);

    SoSearchAction sa;
    sa.setType(SoCube::getClassTypeId());
    sa.apply(root);
    bool foundBefore = (sa.getPath() != nullptr);

    sa.reset();
    bool foundAfter = (sa.getPath() != nullptr);

    EXPECT_TRUE(foundBefore && !foundAfter) << "SoSearchAction::reset() did not clear path";
    root->unref();
}

TEST(ActionsExtended, SoSearchActionIsFoundAfterApply)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    root->addChild(new SoCube);

    SoSearchAction sa;
    sa.setType(SoCube::getClassTypeId());
    sa.apply(root);

    EXPECT_TRUE((sa.isFound() == TRUE)) << "SoSearchAction::isFound() returned FALSE after successful search";
    root->unref();
}

TEST(ActionsExtended, SoSearchActionReturnsNullPathWhenNotFound)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    root->addChild(new SoSphere);

    SoSearchAction sa;
    sa.setType(SoCube::getClassTypeId()); // search for cube, none present
    sa.apply(root);

    EXPECT_TRUE((sa.getPath() == nullptr)) << "SoSearchAction getPath should be null when not found";
    root->unref();
}

// =======================================================================
// SoGetMatrixAction
// =======================================================================

TEST(ActionsExtended, SoGetMatrixActionTranslationMatrixCorrect)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    SoTranslation * trans = new SoTranslation;
    trans->translation.setValue(1.0f, 2.0f, 3.0f);
    root->addChild(trans);

    SbViewportRegion vp(512, 512);
    // Apply to the translation node directly (not the root separator)
    // to get the accumulated matrix from root to that node
    SoGetMatrixAction ma(vp);
    ma.apply(trans);
    SbMatrix mat = ma.getMatrix();

    SbVec3f pt(0, 0, 0);
    SbVec3f result;
    mat.multVecMatrix(pt, result);
    EXPECT_TRUE(floatNear(result[0], 1.0f) &&
                floatNear(result[1], 2.0f) &&
                floatNear(result[2], 3.0f)) << "SoGetMatrixAction translation matrix incorrect";
    root->unref();
}

// =======================================================================
// SoGetBoundingBoxAction
// =======================================================================

TEST(ActionsExtended, SoGetBoundingBoxActionUnitCubeBboxApprox2x2x2)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    root->addChild(new SoCube); // default 2×2×2

    SbViewportRegion vp(512, 512);
    SoGetBoundingBoxAction bba(vp);
    bba.apply(root);
    SbBox3f bbox = bba.getBoundingBox();

    SbVec3f bmin, bmax;
    bbox.getBounds(bmin, bmax);
    EXPECT_TRUE(floatNear(bmin[0], -1.0f) &&
                floatNear(bmin[1], -1.0f) &&
                floatNear(bmin[2], -1.0f) &&
                floatNear(bmax[0],  1.0f) &&
                floatNear(bmax[1],  1.0f) &&
                floatNear(bmax[2],  1.0f)) << "Unit cube bbox not ±1 in all axes";
    root->unref();
}
