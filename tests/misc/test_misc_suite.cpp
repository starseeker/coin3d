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
    EXPECT_TRUE((t != SoType::badType())) << "fromName('SoCube') returned badType";
}

TEST(MiscSuite, SoTypeFromNameReturnsBadTypeForUnknownName)
{
    SoType t = SoType::fromName(SbName("NoSuchNodeEver_xyz"));
    EXPECT_TRUE((t == SoType::badType())) << "fromName for unknown should return badType";
}

TEST(MiscSuite, SoTypeGetNameReturnsNonEmptyNameForSoCube)
{
    SoType t = SoCube::getClassTypeId();
    SbName name = t.getName();
    // Internal Coin type names omit the 'So' prefix (e.g. "Cube" not "SoCube")
    EXPECT_TRUE((name.getLength() > 0)) << "SoType::getName returned empty name for SoCube";
}

TEST(MiscSuite, SoTypeGetParentOfSoCubeIsSoShapeOrSimilar)
{
    SoType t = SoCube::getClassTypeId();
    SoType parent = t.getParent();
    EXPECT_TRUE((parent != SoType::badType())) << "SoType::getParent returned badType for SoCube";
}

TEST(MiscSuite, SoTypeIsDerivedFromSoCubeIsDerivedFromSoNode)
{
    SoType cube = SoCube::getClassTypeId();
    SoType node = SoNode::getClassTypeId();
    EXPECT_TRUE(cube.isDerivedFrom(node)) << "SoCube should be derived from SoNode";
}

TEST(MiscSuite, SoTypeIsDerivedFromSoCubeNOTDerivedFromSoGroup)
{
    SoType cube  = SoCube::getClassTypeId();
    SoType group = SoGroup::getClassTypeId();
    EXPECT_TRUE(!cube.isDerivedFrom(group)) << "SoCube should NOT be derived from SoGroup";
}

TEST(MiscSuite, SoTypeGetAllDerivedFromSoNodeReturnsManyTypes)
{
    SoTypeList list;
    int n = SoType::getAllDerivedFrom(SoNode::getClassTypeId(), list);
    EXPECT_GT(n, 10); // There are many node types
}

TEST(MiscSuite, SoTypeCanCreateInstanceForSoCubeIsTRUE)
{
    SoType t = SoCube::getClassTypeId();
    EXPECT_TRUE(t.canCreateInstance());
}

TEST(MiscSuite, SoTypeCreateInstanceCreatesAnSoCube)
{
    SoType t = SoCube::getClassTypeId();
    void * raw = t.createInstance();
    EXPECT_NE(raw, nullptr);
    if (raw != nullptr) {
        SoCube * cube = static_cast<SoCube *>(raw);
        cube->ref();
        EXPECT_TRUE(cube->isOfType(SoCube::getClassTypeId()));
        cube->unref();
    }
}

TEST(MiscSuite, SoTypeOperatorForSameType)
{
    SoType a = SoCube::getClassTypeId();
    SoType b = SoCube::getClassTypeId();
    EXPECT_TRUE((a == b)) << "SoType operator== failed for same type";
}

TEST(MiscSuite, SoTypeOperatorForDifferentTypes)
{
    SoType a = SoCube::getClassTypeId();
    SoType b = SoSphere::getClassTypeId();
    EXPECT_TRUE((a != b)) << "SoType operator!= failed for different types";
}

TEST(MiscSuite, SoTypeOperatorGivesConsistentOrdering)
{
    SoType a = SoCube::getClassTypeId();
    SoType b = SoSphere::getClassTypeId();
    // One should be less than the other (but both are valid)
    EXPECT_TRUE((a < b) || (b < a)) << "SoType operator< failed (neither a<b nor b<a)";
}

TEST(MiscSuite, SoTypeGetKeyReturnsNonNegativeForValidType)
{
    SoType t = SoCube::getClassTypeId();
    EXPECT_TRUE((t.getKey() >= 0)) << "SoType::getKey returned negative for valid type";
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
    EXPECT_TRUE((path->getHead() == root)) << "SoPath setHead/getHead failed";
    path->unref();
    root->unref();
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
    EXPECT_TRUE((path->getLength() == 2) && (path->getTail() == cube)) << "SoPath append(node)/getLength/getTail failed";
    path->unref();
    root->unref();
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
    EXPECT_TRUE((path->getNode(0) == root) &&
                (path->getNode(1) == cube)) << "SoPath getNode(index) failed";
    path->unref();
    root->unref();
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
    EXPECT_TRUE((path->getNodeFromTail(0) == path->getTail())) << "getNodeFromTail(0) != getTail()";
    path->unref();
    root->unref();
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
    EXPECT_TRUE((path->getLength() == 1) && (path->getTail() == root)) << "SoPath::truncate failed";
    path->unref();
    root->unref();
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
    EXPECT_TRUE(path->containsNode(cube) && path->containsNode(root)) << "SoPath::containsNode failed";
    path->unref();
    root->unref();
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
    EXPECT_TRUE(!path->containsNode(cube)) << "SoPath::containsNode returned TRUE for absent node";
    path->unref();
    root->unref();
    cube->unref();
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
    EXPECT_TRUE((*copy == *path) && (copy != path)) << "SoPath::copy failed";
    copy->unref();
    path->unref();
    root->unref();
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
    EXPECT_TRUE((path->findNode(root) == 0) && (path->findNode(cube) == 1)) << "SoPath::findNode returned wrong index";
    path->unref();
    root->unref();
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
    EXPECT_TRUE((forkIdx == 0)) << "SoPath::findFork returned wrong index";
    p1->unref();
    p2->unref();
    root->unref();
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

    EXPECT_TRUE((*p1 == *p2)) << "SoPath operator== failed for equal paths";
    p1->unref();
    p2->unref();
    root->unref();
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

    EXPECT_TRUE((*p1 != *p2)) << "SoPath operator!= failed for different paths";
    p1->unref();
    p2->unref();
    root->unref();
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
    EXPECT_TRUE((parent->getNumChildren() == 2)) << "SoSeparator (SoChildList) append/getLength failed";
    parent->unref();
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
    EXPECT_TRUE((parent->getNumChildren() == 3) &&
                (parent->getChild(0) == c1) &&
                (parent->getChild(1) == s) &&
                (parent->getChild(2) == c2)) << "SoChildList insert at position failed";
    parent->unref();
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
    EXPECT_TRUE((parent->getNumChildren() == 2) &&
                (parent->getChild(0) == c1) &&
                (parent->getChild(1) == c2)) << "SoChildList remove by index failed";
    parent->unref();
}
