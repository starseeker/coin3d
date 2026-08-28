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
 * @file test_actions_suite.cpp
 * @brief Tests for Coin3D action classes.
 *
 * Baselined against upstream OBOL_TEST_SUITE blocks.
 *
 * Vanilla sources:
 *   src/actions/SoCallbackAction.cpp - callbackall (SoCallbackAction::setCallbackAll)
 *   src/actions/SoWriteAction.cpp    - checkWriteWithMultiref (multi-ref node naming)
 */

#include "../test_utils.h"

#include <Inventor/SoDB.h>
#include <Inventor/SoInput.h>
#include <Inventor/SoOutput.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoType.h>
#include <Inventor/SoPickedPoint.h>
#include <Inventor/SoPath.h>
#include <Inventor/actions/SoCallbackAction.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/actions/SoWriteAction.h>
#include <Inventor/actions/SoGetMatrixAction.h>
#include <Inventor/actions/SoGetPrimitiveCountAction.h>
#include <Inventor/actions/SoRayPickAction.h>
#include <Inventor/actions/SoHandleEventAction.h>
#include <Inventor/actions/SoReorganizeAction.h>
#include <Inventor/events/SoMouseButtonEvent.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSwitch.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoFaceSet.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <Inventor/nodes/SoNormal.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/details/SoFaceDetail.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbMatrix.h>

#include <cstring>
#include <cstdlib>

using namespace ObolTest;

// ---------------------------------------------------------------------------
// Helper: callback that accumulates node names
// ---------------------------------------------------------------------------
static SoCallbackAction::Response
collectNames(void* userdata, SoCallbackAction*, const SoNode* node)
{
    SbString* str = static_cast<SbString*>(userdata);
    (*str) += node->getName();
    return SoCallbackAction::CONTINUE;
}

// ---------------------------------------------------------------------------
// Helper for write-action tests: growable buffer
// ---------------------------------------------------------------------------
static char*  s_buffer      = nullptr;
static size_t s_buffer_size = 0;

static void* bufferRealloc(void* ptr, size_t size)
{
    s_buffer      = static_cast<char*>(std::realloc(ptr, size));
    s_buffer_size = size;
    return s_buffer;
}

TEST(ActionsSuite, SoCallbackActionDefaultSkipsSwitchChildren)
{
    SbString names;
    SoSwitch* sw = new SoSwitch;
    sw->setName("switch");
    SoCube* cube = new SoCube;
    cube->setName("cube");
    sw->addChild(cube);
    sw->ref();

    SoCallbackAction cba;
    cba.addPreCallback(SoNode::getClassTypeId(), collectNames, &names);
    cba.apply(sw);

    // Default: switch node visited, but not its child (whichChild == SO_SWITCH_NONE)
    EXPECT_TRUE((names == SbString("switch"))) << std::string("Expected 'switch', got '") + names.getString() + "'";
    sw->unref();
}

TEST(ActionsSuite, SoCallbackActionSetCallbackAllTraversesSwitchChildren)
{
    SbString names;
    SoSwitch* sw = new SoSwitch;
    sw->setName("switch");
    SoCube* cube = new SoCube;
    cube->setName("cube");
    sw->addChild(cube);
    sw->ref();

    SoCallbackAction cba;
    cba.addPreCallback(SoNode::getClassTypeId(), collectNames, &names);
    cba.setCallbackAll(true);
    cba.apply(sw);

    // With callbackAll: both switch and cube visited
    EXPECT_TRUE((names == SbString("switchcube"))) << std::string("Expected 'switchcube', got '") + names.getString() + "'";
    sw->unref();
}

// -----------------------------------------------------------------------
// SoWriteAction: scene graph with multiply-referenced node
// Baseline: src/actions/SoWriteAction.cpp OBOL_TEST_SUITE (checkWriteWithMultiref)
// The test verifies that multi-ref nodes are written with DEF/USE.
// -----------------------------------------------------------------------

TEST(ActionsSuite, SoWriteActionWritesMultiRefNodeWithDEFUSE)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    // Add the same child node twice (multi-ref)
    SoSeparator* shared = new SoSeparator;
    shared->setName("SharedNode");
    root->addChild(shared);
    root->addChild(shared);

    // Write to a buffer
    s_buffer      = nullptr;
    s_buffer_size = 0;

    SoOutput out;
    out.setBuffer(nullptr, 1, bufferRealloc);

    SoWriteAction wa(&out);
    wa.apply(root);
    root->unref();

    // The output should contain "DEF SharedNode" and "USE SharedNode"
    bool hasDef = (s_buffer != nullptr) &&
                  (std::strstr(s_buffer, "DEF") != nullptr);
    bool hasUse = (s_buffer != nullptr) &&
                  (std::strstr(s_buffer, "USE") != nullptr);

    std::free(s_buffer);
    s_buffer      = nullptr;
    s_buffer_size = 0;

    EXPECT_TRUE(hasDef && hasUse) << "SoWriteAction output missing DEF/USE for multi-ref node";
}

// -----------------------------------------------------------------------
// SoSearchAction: find node by name
// -----------------------------------------------------------------------

TEST(ActionsSuite, SoSearchActionFindByName)
{
    SoSeparator* root = new SoSeparator;
    root->ref();
    SoCube* cube = new SoCube;
    cube->setName("MyCube");
    root->addChild(cube);

    SoSearchAction search;
    search.setName(SbName("MyCube"));
    search.apply(root);

    EXPECT_TRUE((search.getPath() != nullptr)) << "SoSearchAction could not find node named 'MyCube'";
    root->unref();
}

TEST(ActionsSuite, SoSearchActionFindByType)
{
    SoSeparator* root = new SoSeparator;
    root->ref();
    root->addChild(new SoCube);

    SoSearchAction search;
    search.setType(SoCube::getClassTypeId());
    search.apply(root);

    EXPECT_TRUE((search.getPath() != nullptr)) << "SoSearchAction could not find SoCube by type";
    root->unref();
}

// -----------------------------------------------------------------------
// SoGetBoundingBoxAction: unit cube bounding box
// -----------------------------------------------------------------------

TEST(ActionsSuite, SoGetBoundingBoxActionUnitCube)
{
    SoSeparator* root = new SoSeparator;
    root->ref();
    SoCube* cube = new SoCube; // default 2x2x2
    root->addChild(cube);

    SoGetBoundingBoxAction bba(SbViewportRegion(100, 100));
    bba.apply(root);

    SbBox3f bbox = bba.getBoundingBox();
    EXPECT_FALSE(bbox.isEmpty());
    if (!bbox.isEmpty()) {
        // Default SoCube is 2x2x2 centred at origin -> min=-1 max=1
        SbVec3f lo, hi;
        bbox.getBounds(lo, hi);
        EXPECT_EQ(lo, SbVec3f(-1.0f, -1.0f, -1.0f));
        EXPECT_EQ(hi, SbVec3f(1.0f, 1.0f, 1.0f));
    }
    root->unref();
}

// -----------------------------------------------------------------------
// SoGetMatrixAction: identity matrix for empty separator
// -----------------------------------------------------------------------

TEST(ActionsSuite, SoGetMatrixActionIdentityForEmptySeparator)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoGetMatrixAction gma(SbViewportRegion(100, 100));
    gma.apply(root);

    SbMatrix mat = gma.getMatrix();
    SbMatrix identity = SbMatrix::identity();
    EXPECT_TRUE((mat == identity)) << "SoGetMatrixAction did not return identity for empty scene";
    root->unref();
}

TEST(ActionsSuite, SoGetMatrixActionClassInitialized)
{
    SoGetMatrixAction gma(SbViewportRegion(100, 100));
    EXPECT_TRUE((gma.getTypeId() != SoType::badType())) << "SoGetMatrixAction has bad type";
}

// -----------------------------------------------------------------------
// SoGetPrimitiveCountAction: count primitives in a scene
// -----------------------------------------------------------------------

TEST(ActionsSuite, SoGetPrimitiveCountActionClassInitialized)
{
    SoGetPrimitiveCountAction gpca(SbViewportRegion(100, 100));
    EXPECT_TRUE((gpca.getTypeId() != SoType::badType())) << "SoGetPrimitiveCountAction has bad type";
}

TEST(ActionsSuite, SoGetPrimitiveCountActionEmptyScene)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoGetPrimitiveCountAction gpca(SbViewportRegion(100, 100));
    gpca.apply(root);

    // Empty scene: no triangles, no lines, no points
    EXPECT_TRUE((gpca.getTriangleCount() == 0)) << "SoGetPrimitiveCountAction should count 0 triangles for empty scene";
    root->unref();
}

// -----------------------------------------------------------------------
// SoRayPickAction: class initialized and basic ray cast
// -----------------------------------------------------------------------

TEST(ActionsSuite, SoRayPickActionClassInitialized)
{
    SoRayPickAction rpa(SbViewportRegion(100, 100));
    EXPECT_TRUE((rpa.getTypeId() != SoType::badType())) << "SoRayPickAction has bad type";
}

TEST(ActionsSuite, SoRayPickActionNoPicksOnEmptyScene)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoRayPickAction rpa(SbViewportRegion(100, 100));
    // Aim ray from (0,0,10) pointing in -Z direction
    rpa.setRay(SbVec3f(0.0f, 0.0f, 10.0f), SbVec3f(0.0f, 0.0f, -1.0f));
    rpa.apply(root);

    EXPECT_TRUE((rpa.getPickedPoint() == nullptr)) << "SoRayPickAction should find no pick in an empty scene";
    root->unref();
}

TEST(ActionsSuite, SoRayPickActionPicksCubeAtOrigin)
{
    SoSeparator* root = new SoSeparator;
    root->ref();
    root->addChild(new SoCube); // 2x2x2 centred at origin

    SoRayPickAction rpa(SbViewportRegion(100, 100));
    // Aim ray from (0,0,10) pointing straight in -Z; should hit the cube
    rpa.setRay(SbVec3f(0.0f, 0.0f, 10.0f), SbVec3f(0.0f, 0.0f, -1.0f));
    rpa.apply(root);

    EXPECT_TRUE((rpa.getPickedPoint() != nullptr)) << "SoRayPickAction should pick the cube at origin";
    root->unref();
}

// -----------------------------------------------------------------------
// SoRayPickAction: verify pick returns sensible intersection point and path
// -----------------------------------------------------------------------

TEST(ActionsSuite, SoRayPickActionPickPointIsOnCubeSurface)
{
    SoSeparator* root = new SoSeparator;
    root->ref();
    SoCube* cube = new SoCube; // 2x2x2 at origin, front face at z=+1
    root->addChild(cube);

    SoRayPickAction rpa(SbViewportRegion(100, 100));
    rpa.setRay(SbVec3f(0.0f, 0.0f, 10.0f), SbVec3f(0.0f, 0.0f, -1.0f));
    rpa.apply(root);

    SoPickedPoint* pp = rpa.getPickedPoint();
    EXPECT_NE(pp, nullptr);
    if (pp != nullptr) {
        // The ray hits the front face of the cube at z = +1.0
        SbVec3f pt = pp->getPoint();
        EXPECT_NEAR(pt[2], 1.0f, 0.01f);

        // The pick path should end at the cube node
        SoPath* path = pp->getPath();
        EXPECT_NE(path, nullptr);
        if (path) EXPECT_EQ(path->getTail(), cube);
    }
    root->unref();
}

// -----------------------------------------------------------------------
// SoHandleEventAction: class initialized and basic dispatch
// -----------------------------------------------------------------------

TEST(ActionsSuite, SoHandleEventActionClassInitialized)
{
    SoHandleEventAction hea(SbViewportRegion(100, 100));
    EXPECT_TRUE((hea.getTypeId() != SoType::badType())) << "SoHandleEventAction has bad type";
}

TEST(ActionsSuite, SoHandleEventActionDispatchOnEmptySceneDoesNotCrash)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoMouseButtonEvent ev;
    ev.setButton(SoMouseButtonEvent::BUTTON1);
    ev.setState(SoButtonEvent::DOWN);

    SoHandleEventAction hea(SbViewportRegion(100, 100));
    hea.setEvent(&ev);
    hea.apply(root); // should complete without crash; not handled

    EXPECT_TRUE(!hea.isHandled()) << "SoHandleEventAction should not be handled for empty scene";
    root->unref();
}

// -----------------------------------------------------------------------
// SoReorganizeAction: class initialized (type registered)
// -----------------------------------------------------------------------

TEST(ActionsSuite, SoReorganizeActionClassInitialized)
{
    SoReorganizeAction ra;
    EXPECT_TRUE((ra.getTypeId() != SoType::badType())) << "SoReorganizeAction has bad type";
}

// -----------------------------------------------------------------------
// SoRayPickAction integration: face set + transform scene
// Tier 2: validates picked path, intersection point, and face detail
// -----------------------------------------------------------------------

TEST(ActionsSuite, SoRayPickActionPicksSoFaceSetQuad)
{
    // A simple quad at z=0 facing +Z, covering the XY range [-1,1]
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoCoordinate3* coords = new SoCoordinate3;
    coords->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
    coords->point.set1Value(1, SbVec3f( 1.0f, -1.0f, 0.0f));
    coords->point.set1Value(2, SbVec3f( 1.0f,  1.0f, 0.0f));
    coords->point.set1Value(3, SbVec3f(-1.0f,  1.0f, 0.0f));
    root->addChild(coords);

    SoFaceSet* fs = new SoFaceSet;
    fs->numVertices.set1Value(0, 4);
    root->addChild(fs);

    SoRayPickAction rpa(SbViewportRegion(100, 100));
    // Ray from (0,0,10) pointing -Z: should hit the quad at (0,0,0)
    rpa.setRay(SbVec3f(0.0f, 0.0f, 10.0f), SbVec3f(0.0f, 0.0f, -1.0f));
    rpa.apply(root);

    SoPickedPoint* pp = rpa.getPickedPoint();
    EXPECT_NE(pp, nullptr);
    if (pp) {
        SbVec3f pt = pp->getPoint();
        EXPECT_NEAR(pt[2], 0.0f, 0.01f); // hit at z~0
    }
    root->unref();
}

TEST(ActionsSuite, SoRayPickActionMissesSoFaceSetWhenRayOffset)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoCoordinate3* coords = new SoCoordinate3;
    coords->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
    coords->point.set1Value(1, SbVec3f( 1.0f, -1.0f, 0.0f));
    coords->point.set1Value(2, SbVec3f( 1.0f,  1.0f, 0.0f));
    coords->point.set1Value(3, SbVec3f(-1.0f,  1.0f, 0.0f));
    root->addChild(coords);

    SoFaceSet* fs = new SoFaceSet;
    fs->numVertices.set1Value(0, 4);
    root->addChild(fs);

    SoRayPickAction rpa(SbViewportRegion(100, 100));
    // Ray offset to x=5: should miss the quad
    rpa.setRay(SbVec3f(5.0f, 0.0f, 10.0f), SbVec3f(0.0f, 0.0f, -1.0f));
    rpa.apply(root);

    EXPECT_TRUE((rpa.getPickedPoint() == nullptr)) << "SoRayPickAction should miss the quad when ray is offset";
    root->unref();
}

TEST(ActionsSuite, SoRayPickActionPicksThroughSoTranslationCorrectly)
{
    // Cube translated to (3,0,0); ray aimed at (3,0,10) -> should hit
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoTranslation* t = new SoTranslation;
    t->translation.setValue(3.0f, 0.0f, 0.0f);
    root->addChild(t);

    SoCube* cube = new SoCube; // 2x2x2, now centred at (3,0,0)
    root->addChild(cube);

    SoRayPickAction rpa(SbViewportRegion(100, 100));
    rpa.setRay(SbVec3f(3.0f, 0.0f, 10.0f), SbVec3f(0.0f, 0.0f, -1.0f));
    rpa.apply(root);

    EXPECT_TRUE((rpa.getPickedPoint() != nullptr)) << "SoRayPickAction should pick translated cube";
    root->unref();
}

TEST(ActionsSuite, SoRayPickActionMissesWhenRayPassesBesideTranslatedCube)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoTranslation* t = new SoTranslation;
    t->translation.setValue(3.0f, 0.0f, 0.0f);
    root->addChild(t);

    root->addChild(new SoCube);

    SoRayPickAction rpa(SbViewportRegion(100, 100));
    // Ray at x=0 should miss a cube at x=3
    rpa.setRay(SbVec3f(0.0f, 0.0f, 10.0f), SbVec3f(0.0f, 0.0f, -1.0f));
    rpa.apply(root);

    EXPECT_TRUE((rpa.getPickedPoint() == nullptr)) << "SoRayPickAction should miss cube that is out of ray path";
    root->unref();
}

TEST(ActionsSuite, SoRayPickActionPickPathEndsAtSoCubeNode)
{
    SoSeparator* root = new SoSeparator;
    root->ref();
    SoCube* cube = new SoCube;
    root->addChild(cube);

    SoRayPickAction rpa(SbViewportRegion(100, 100));
    rpa.setRay(SbVec3f(0.0f, 0.0f, 10.0f), SbVec3f(0.0f, 0.0f, -1.0f));
    rpa.apply(root);

    SoPickedPoint* pp = rpa.getPickedPoint();
    EXPECT_NE(pp, nullptr);
    if (pp != nullptr) {
        SoPath* path = pp->getPath();
        EXPECT_NE(path, nullptr);
        if (path) EXPECT_EQ(path->getTail(), cube);
    }
    root->unref();
}

TEST(ActionsSuite, SoRayPickActionGetPickedPointListReturnsAllPicks)
{
    // Two cubes offset in Y; shoot a ray that hits the front one
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoSeparator* s1 = new SoSeparator;
    SoTranslation* t1 = new SoTranslation;
    t1->translation.setValue(0.0f, 0.0f, 1.0f);
    s1->addChild(t1);
    s1->addChild(new SoCube);
    root->addChild(s1);

    SoSeparator* s2 = new SoSeparator;
    SoTranslation* t2 = new SoTranslation;
    t2->translation.setValue(0.0f, 0.0f, -1.0f);
    s2->addChild(t2);
    s2->addChild(new SoCube);
    root->addChild(s2);

    SoRayPickAction rpa(SbViewportRegion(100, 100));
    rpa.setPickAll(TRUE);  // collect all intersections
    rpa.setRay(SbVec3f(0.0f, 0.0f, 10.0f), SbVec3f(0.0f, 0.0f, -1.0f));
    rpa.apply(root);

    const SoPickedPointList& list = rpa.getPickedPointList();
    // Both cubes are on the ray path; expect 2 picked points
    EXPECT_TRUE((list.getLength() >= 2)) << "SoRayPickAction getPickedPointList should return >= 2 for two cubes on ray";
    root->unref();
}

TEST(ActionsSuite, SoRayPickActionSoIndexedFaceSetPick)
{
    // Build a simple triangle via SoIndexedFaceSet
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoCoordinate3* coords = new SoCoordinate3;
    coords->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
    coords->point.set1Value(1, SbVec3f( 1.0f, -1.0f, 0.0f));
    coords->point.set1Value(2, SbVec3f( 0.0f,  1.0f, 0.0f));
    root->addChild(coords);

    SoIndexedFaceSet* ifs = new SoIndexedFaceSet;
    ifs->coordIndex.set1Value(0, 0);
    ifs->coordIndex.set1Value(1, 1);
    ifs->coordIndex.set1Value(2, 2);
    ifs->coordIndex.set1Value(3, -1); // end-of-face
    root->addChild(ifs);

    SoRayPickAction rpa(SbViewportRegion(100, 100));
    rpa.setRay(SbVec3f(0.0f, 0.0f, 10.0f), SbVec3f(0.0f, 0.0f, -1.0f));
    rpa.apply(root);

    SoPickedPoint* pp = rpa.getPickedPoint();
    EXPECT_NE(pp, nullptr);
    if (pp) {
        SbVec3f pt = pp->getPoint();
        EXPECT_NEAR(pt[2], 0.0f, 0.01f);
    }
    root->unref();
}
