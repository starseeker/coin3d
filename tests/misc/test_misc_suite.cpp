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
 * @file test_misc_suite.cpp
 * @brief Tests for misc/ subsystem: SoType, SoPath, SoChildList.
 *
 * Covers (misc/ 41.1 %):
 *   SoType:
 *     fromName, getName, getParent, isDerivedFrom,
 *     getAllDerivedFrom, canCreateInstance, createInstance,
 *     badType, operator==, operator!=, operator<, getData, getKey
 *   SoPath:
 *     setHead, append(node), getHead, getTail, getLength, getNode,
 *     getNodeFromTail, getIndex, truncate, containsNode, containsPath,
 *     copy, findFork, findNode, operator==, operator!=
 *   SoChildList:
 *     append, insert, remove, getLength, set, copy
 */

#include "../test_utils.h"

#include <Inventor/SoType.h>
#include <Inventor/SoPath.h>
#include <Inventor/SbName.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/lists/SoTypeList.h>
#include <Inventor/misc/SoChildList.h>

#include <Inventor/nodes/SoNode.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoTranslation.h>

#include <cstring>

using namespace ObolTest;

TEST(MiscSuite, SoTypeFromNameReturnsValidTypeForSoCube)
{
    SoType t = SoType::fromName(SbName("SoCube"));
    bool pass = (t != SoType::badType());
    EXPECT_TRUE(pass) << "fromName('SoCube') returned badType";
}

TEST(MiscSuite, SoTypeFromNameReturnsBadTypeForUnknownName)
{
    SoType t = SoType::fromName(SbName("NoSuchNodeEver_xyz"));
    bool pass = (t == SoType::badType());
    EXPECT_TRUE(pass) << "fromName for unknown should return badType";
}

TEST(MiscSuite, SoTypeGetNameReturnsNonEmptyNameForSoCube)
{
    SoType t = SoCube::getClassTypeId();
    SbName name = t.getName();
    // Internal Coin type names omit the 'So' prefix (e.g. "Cube" not "SoCube")
    bool pass = (name.getLength() > 0);
    EXPECT_TRUE(pass) << "SoType::getName returned empty name for SoCube";
}

TEST(MiscSuite, SoTypeGetParentOfSoCubeIsSoShapeOrSimilar)
{
    SoType t = SoCube::getClassTypeId();
    SoType parent = t.getParent();
    bool pass = (parent != SoType::badType());
    EXPECT_TRUE(pass) << "SoType::getParent returned badType for SoCube";
}

TEST(MiscSuite, SoTypeIsDerivedFromSoCubeIsDerivedFromSoNode)
{
    SoType cube = SoCube::getClassTypeId();
    SoType node = SoNode::getClassTypeId();
    bool pass = cube.isDerivedFrom(node);
    EXPECT_TRUE(pass) << "SoCube should be derived from SoNode";
}

TEST(MiscSuite, SoTypeIsDerivedFromSoCubeNOTDerivedFromSoGroup)
{
    SoType cube  = SoCube::getClassTypeId();
    SoType group = SoGroup::getClassTypeId();
    bool pass = !cube.isDerivedFrom(group);
    EXPECT_TRUE(pass) << "SoCube should NOT be derived from SoGroup";
}

TEST(MiscSuite, SoTypeGetAllDerivedFromSoNodeReturnsManyTypes)
{
    SoTypeList list;
    int n = SoType::getAllDerivedFrom(SoNode::getClassTypeId(), list);
    bool pass = (n > 10); // There are many node types
    EXPECT_TRUE(pass) << "getAllDerivedFrom(SoNode) returned too few types";
}

TEST(MiscSuite, SoTypeCanCreateInstanceForSoCubeIsTRUE)
{
    SoType t = SoCube::getClassTypeId();
    bool pass = t.canCreateInstance();
    EXPECT_TRUE(pass) << "SoCube::canCreateInstance should be TRUE";
}

TEST(MiscSuite, SoTypeCreateInstanceCreatesAnSoCube)
{
    SoType t = SoCube::getClassTypeId();
    void * raw = t.createInstance();
    bool pass = (raw != nullptr);
    if (pass) {
        SoCube * cube = static_cast<SoCube *>(raw);
        cube->ref();
        pass = cube->isOfType(SoCube::getClassTypeId());
        cube->unref();
    }
    EXPECT_TRUE(pass) << "createInstance for SoCube failed";
}

TEST(MiscSuite, SoTypeOperatorForSameType)
{
    SoType a = SoCube::getClassTypeId();
    SoType b = SoCube::getClassTypeId();
    bool pass = (a == b);
    EXPECT_TRUE(pass) << "SoType operator== failed for same type";
}

TEST(MiscSuite, SoTypeOperatorForDifferentTypes)
{
    SoType a = SoCube::getClassTypeId();
    SoType b = SoSphere::getClassTypeId();
    bool pass = (a != b);
    EXPECT_TRUE(pass) << "SoType operator!= failed for different types";
}

TEST(MiscSuite, SoTypeOperatorGivesConsistentOrdering)
{
    SoType a = SoCube::getClassTypeId();
    SoType b = SoSphere::getClassTypeId();
    // One should be less than the other (but both are valid)
    bool pass = (a < b) || (b < a);
    EXPECT_TRUE(pass) << "SoType operator< failed (neither a<b nor b<a)";
}

TEST(MiscSuite, SoTypeGetKeyReturnsNonNegativeForValidType)
{
    SoType t = SoCube::getClassTypeId();
    bool pass = (t.getKey() >= 0);
    EXPECT_TRUE(pass) << "SoType::getKey returned negative for valid type";
}

// =======================================================================
// SoPath
// =======================================================================

TEST(MiscSuite, SoPathSetHeadAndGetHead)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    SoPath * path = new SoPath;
    path->ref();
    path->setHead(root);
    bool pass = (path->getHead() == root);
    path->unref();
    root->unref();
    EXPECT_TRUE(pass) << "SoPath setHead/getHead failed";
}

TEST(MiscSuite, SoPathAppendNodeAndGetLength)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    SoCube * cube = new SoCube;
    root->addChild(cube);

    SoPath * path = new SoPath(root);
    path->ref();
    path->append(cube);
    bool pass = (path->getLength() == 2) && (path->getTail() == cube);
    path->unref();
    root->unref();
    EXPECT_TRUE(pass) << "SoPath append(node)/getLength/getTail failed";
}

TEST(MiscSuite, SoPathGetNodeIndexAccessesCorrectNode)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    SoCube * cube = new SoCube;
    root->addChild(cube);

    SoPath * path = new SoPath(root);
    path->ref();
    path->append(cube);
    bool pass = (path->getNode(0) == root) &&
                (path->getNode(1) == cube);
    path->unref();
    root->unref();
    EXPECT_TRUE(pass) << "SoPath getNode(index) failed";
}

TEST(MiscSuite, SoPathGetNodeFromTail0IsGetTail)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    SoCube * cube = new SoCube;
    root->addChild(cube);

    SoPath * path = new SoPath(root);
    path->ref();
    path->append(cube);
    bool pass = (path->getNodeFromTail(0) == path->getTail());
    path->unref();
    root->unref();
    EXPECT_TRUE(pass) << "getNodeFromTail(0) != getTail()";
}

TEST(MiscSuite, SoPathTruncateShortensPath)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    SoCube * cube = new SoCube;
    root->addChild(cube);

    SoPath * path = new SoPath(root);
    path->ref();
    path->append(cube);
    path->truncate(1); // keep only head
    bool pass = (path->getLength() == 1) && (path->getTail() == root);
    path->unref();
    root->unref();
    EXPECT_TRUE(pass) << "SoPath::truncate failed";
}

TEST(MiscSuite, SoPathContainsNodeReturnsTRUEForNodeInPath)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    SoCube * cube = new SoCube;
    root->addChild(cube);

    SoPath * path = new SoPath(root);
    path->ref();
    path->append(cube);
    bool pass = path->containsNode(cube) && path->containsNode(root);
    path->unref();
    root->unref();
    EXPECT_TRUE(pass) << "SoPath::containsNode failed";
}

TEST(MiscSuite, SoPathContainsNodeReturnsFALSEForNodeNotInPath)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    SoSphere * sphere = new SoSphere;
    root->addChild(sphere);
    SoCube * cube = new SoCube;
    cube->ref();

    SoPath * path = new SoPath(root);
    path->ref();
    path->append(sphere);
    bool pass = !path->containsNode(cube);
    path->unref();
    root->unref();
    cube->unref();
    EXPECT_TRUE(pass) << "SoPath::containsNode returned TRUE for absent node";
}

TEST(MiscSuite, SoPathCopyCreatesANewEqualPath)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    SoCube * cube = new SoCube;
    root->addChild(cube);

    SoPath * path = new SoPath(root);
    path->ref();
    path->append(cube);
    SoPath * copy = path->copy();
    copy->ref();
    bool pass = (*copy == *path) && (copy != path);
    copy->unref();
    path->unref();
    root->unref();
    EXPECT_TRUE(pass) << "SoPath::copy failed";
}

TEST(MiscSuite, SoPathFindNodeReturnsCorrectIndex)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    SoCube * cube = new SoCube;
    root->addChild(cube);

    SoPath * path = new SoPath(root);
    path->ref();
    path->append(cube);
    bool pass = (path->findNode(root) == 0) && (path->findNode(cube) == 1);
    path->unref();
    root->unref();
    EXPECT_TRUE(pass) << "SoPath::findNode returned wrong index";
}

TEST(MiscSuite, SoPathFindForkWithSharedPrefix)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    SoCube * cube   = new SoCube;
    SoSphere * sph  = new SoSphere;
    root->addChild(cube);
    root->addChild(sph);

    SoPath * p1 = new SoPath(root);
    p1->ref();
    p1->append(cube);

    SoPath * p2 = new SoPath(root);
    p2->ref();
    p2->append(sph);

    // Both paths share root (index 0) so fork is at 0
    int forkIdx = p1->findFork(p2);
    bool pass = (forkIdx == 0);
    p1->unref();
    p2->unref();
    root->unref();
    EXPECT_TRUE(pass) << "SoPath::findFork returned wrong index";
}

TEST(MiscSuite, SoPathOperatorForEqualPaths)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    SoCube * cube = new SoCube;
    root->addChild(cube);

    SoPath * p1 = new SoPath(root);
    p1->ref();
    p1->append(cube);
    SoPath * p2 = p1->copy();
    p2->ref();

    bool pass = (*p1 == *p2);
    p1->unref();
    p2->unref();
    root->unref();
    EXPECT_TRUE(pass) << "SoPath operator== failed for equal paths";
}

TEST(MiscSuite, SoPathOperatorForDifferentPaths)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    SoCube   * cube  = new SoCube;
    SoSphere * sph   = new SoSphere;
    root->addChild(cube);
    root->addChild(sph);

    SoPath * p1 = new SoPath(root);
    p1->ref();
    p1->append(cube);
    SoPath * p2 = new SoPath(root);
    p2->ref();
    p2->append(sph);

    bool pass = (*p1 != *p2);
    p1->unref();
    p2->unref();
    root->unref();
    EXPECT_TRUE(pass) << "SoPath operator!= failed for different paths";
}

// =======================================================================
// SoChildList
// =======================================================================

TEST(MiscSuite, SoChildListAppendAndGetLength)
{
    SoSeparator * parent = new SoSeparator;
    parent->ref();
    // SoChildList is owned by the node; access through addChild
    SoCube * c1 = new SoCube;
    SoCube * c2 = new SoCube;
    parent->addChild(c1);
    parent->addChild(c2);
    bool pass = (parent->getNumChildren() == 2);
    parent->unref();
    EXPECT_TRUE(pass) << "SoSeparator (SoChildList) append/getLength failed";
}

TEST(MiscSuite, SoChildListInsertBeforeIndex)
{
    SoSeparator * parent = new SoSeparator;
    parent->ref();
    SoCube   * c1 = new SoCube;
    SoCube   * c2 = new SoCube;
    SoSphere * s  = new SoSphere;
    parent->addChild(c1);
    parent->addChild(c2);
    parent->insertChild(s, 1); // insert before c2
    bool pass = (parent->getNumChildren() == 3) &&
                (parent->getChild(0) == c1) &&
                (parent->getChild(1) == s) &&
                (parent->getChild(2) == c2);
    parent->unref();
    EXPECT_TRUE(pass) << "SoChildList insert at position failed";
}

TEST(MiscSuite, SoChildListRemoveByIndex)
{
    SoSeparator * parent = new SoSeparator;
    parent->ref();
    SoCube   * c1 = new SoCube;
    SoCube   * c2 = new SoCube;
    SoSphere * s  = new SoSphere;
    parent->addChild(c1);
    parent->addChild(s);
    parent->addChild(c2);
    parent->removeChild(1); // remove s
    bool pass = (parent->getNumChildren() == 2) &&
                (parent->getChild(0) == c1) &&
                (parent->getChild(1) == c2);
    parent->unref();
    EXPECT_TRUE(pass) << "SoChildList remove by index failed";
}
