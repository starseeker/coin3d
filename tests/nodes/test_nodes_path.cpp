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
 * @file test_nodes_path.cpp
 * @brief Tests for SoPath and SoFullPath APIs.
 *
 * Covers:
 *   SoPath  - getClassTypeId, constructor, setHead, append, getNodeFromTail,
 *             getLength, truncate, containsNode, copy, operator==, operator!=,
 *             append(SoPath*), findFork, containsPath, push/pop
 *   SoFullPath - getLength, getTail, getNodeFromTail
 */

#include "../test_utils.h"
#include <Inventor/SoPath.h>
#include <Inventor/SoFullPath.h>
#include <Inventor/SoType.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <cmath>

using namespace ObolTest;

TEST(NodesPath, SoPathGetClassTypeIdIsNotBadType)
{
    bool pass = (SoPath::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoPath has bad class type";
}

// -----------------------------------------------------------------------
// 2. SoPath(head) constructor — getHead() returns head, getLength() == 1
// -----------------------------------------------------------------------

TEST(NodesPath, SoPathHeadConstructorGetHeadReturnsHeadAndGetLength1)
{
    SoSeparator * root = new SoSeparator;
    root->ref();

    SoPath * path = new SoPath(root);
    path->ref();

    bool pass = (path->getHead() == root) && (path->getLength() == 1);

    path->unref();
    root->unref();
    EXPECT_TRUE(pass) << "SoPath(head): getHead or getLength incorrect";
}

// -----------------------------------------------------------------------
// 3. SoPath::setHead() — set and get head
// -----------------------------------------------------------------------

TEST(NodesPath, SoPathSetHeadSetsHeadCorrectly)
{
    SoSeparator * root = new SoSeparator;
    root->ref();

    SoPath * path = new SoPath;
    path->ref();
    path->setHead(root);

    bool pass = (path->getHead() == root);

    path->unref();
    root->unref();
    EXPECT_TRUE(pass) << "SoPath::setHead: getHead did not return the set node";
}

// -----------------------------------------------------------------------
// 4. SoPath::append(index) — append child by index 0:
//    getLength() == 2, getTail() returns the child
// -----------------------------------------------------------------------

TEST(NodesPath, SoPathAppendIndexGetLength2AndGetTailReturnsChild)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    SoSeparator * child = new SoSeparator;
    root->addChild(child);

    SoPath * path = new SoPath(root);
    path->ref();
    path->append(0); // child is at index 0 under root

    bool pass = (path->getLength() == 2) && (path->getTail() == child);

    path->unref();
    root->unref();
    EXPECT_TRUE(pass) << "SoPath::append(index): getLength or getTail incorrect";
}

// -----------------------------------------------------------------------
// 5. SoPath::getNodeFromTail(0) == getTail(),
//    getNodeFromTail(1) == getHead() when path length is 2
// -----------------------------------------------------------------------

TEST(NodesPath, SoPathGetNodeFromTailIndex0IsTailAndIndex1IsHeadLength2)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    SoSeparator * child = new SoSeparator;
    root->addChild(child);

    SoPath * path = new SoPath(root);
    path->ref();
    path->append(0);

    bool pass = (path->getNodeFromTail(0) == path->getTail()) &&
                (path->getNodeFromTail(1) == path->getHead());

    path->unref();
    root->unref();
    EXPECT_TRUE(pass) << "SoPath::getNodeFromTail: unexpected node at tail-relative index";
}

// -----------------------------------------------------------------------
// 6. SoPath::getLength() correct after append
// -----------------------------------------------------------------------

TEST(NodesPath, SoPathGetLengthIncreasesAfterEachAppend)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    SoSeparator * child = new SoSeparator;
    root->addChild(child);
    SoCube * leaf = new SoCube;
    child->addChild(leaf);

    SoPath * path = new SoPath(root);
    path->ref();

    bool pass = (path->getLength() == 1);
    path->append(0); // root -> child
    pass = pass && (path->getLength() == 2);
    path->append(0); // child -> leaf
    pass = pass && (path->getLength() == 3);

    path->unref();
    root->unref();
    EXPECT_TRUE(pass) << "SoPath::getLength did not increase correctly after appends";
}

// -----------------------------------------------------------------------
// 7. SoPath::truncate() — truncate to length 1, verify getTail() == getHead()
// -----------------------------------------------------------------------

TEST(NodesPath, SoPathTruncateToLength1GetTailGetHead)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    SoSeparator * child = new SoSeparator;
    root->addChild(child);

    SoPath * path = new SoPath(root);
    path->ref();
    path->append(0);
    path->truncate(1);

    bool pass = (path->getLength() == 1) &&
                (path->getTail() == path->getHead());

    path->unref();
    root->unref();
    EXPECT_TRUE(pass) << "SoPath::truncate: length or tail incorrect after truncate(1)";
}

// -----------------------------------------------------------------------
// 8. SoPath::containsNode() — TRUE for head and child, FALSE for unrelated
// -----------------------------------------------------------------------

TEST(NodesPath, SoPathContainsNodeTRUEForHeadAndChildFALSEForUnrelatedNode)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    SoSeparator * child = new SoSeparator;
    root->addChild(child);
    SoSphere * unrelated = new SoSphere;
    unrelated->ref();

    SoPath * path = new SoPath(root);
    path->ref();
    path->append(0);

    bool pass = (path->containsNode(root)      == TRUE)  &&
                (path->containsNode(child)     == TRUE)  &&
                (path->containsNode(unrelated) == FALSE);

    path->unref();
    unrelated->unref();
    root->unref();
    EXPECT_TRUE(pass) << "SoPath::containsNode returned unexpected result";
}

// -----------------------------------------------------------------------
// 9. SoPath::copy() — copy has same length and same head
// -----------------------------------------------------------------------

TEST(NodesPath, SoPathCopyCopiedPathHasSameLengthAndHead)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    SoSeparator * child = new SoSeparator;
    root->addChild(child);

    SoPath * path = new SoPath(root);
    path->ref();
    path->append(0);

    SoPath * copied = path->copy();
    copied->ref();

    bool pass = (copied->getLength() == path->getLength()) &&
                (copied->getHead()   == path->getHead());

    copied->unref();
    path->unref();
    root->unref();
    EXPECT_TRUE(pass) << "SoPath::copy: copied path has different length or head";
}

// -----------------------------------------------------------------------
// 10. SoPath::operator== — two paths with same head/nodes are equal
// -----------------------------------------------------------------------

TEST(NodesPath, SoPathOperatorPathsWithSameNodesAreEqual)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    SoSeparator * child = new SoSeparator;
    root->addChild(child);

    SoPath * pathA = new SoPath(root);
    pathA->ref();
    pathA->append(0);

    SoPath * pathB = new SoPath(root);
    pathB->ref();
    pathB->append(0);

    bool pass = (*pathA == *pathB);

    pathA->unref();
    pathB->unref();
    root->unref();
    EXPECT_TRUE(pass) << "SoPath::operator==: identical paths not equal";
}

// -----------------------------------------------------------------------
// 11. SoPath::operator!= — two paths with different tails are not equal
// -----------------------------------------------------------------------

TEST(NodesPath, SoPathOperatorPathsWithDifferentTailsAreNotEqual)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    SoSeparator * childA = new SoSeparator;
    root->addChild(childA);
    SoSeparator * childB = new SoSeparator;
    root->addChild(childB);

    SoPath * pathA = new SoPath(root);
    pathA->ref();
    pathA->append(0); // -> childA

    SoPath * pathB = new SoPath(root);
    pathB->ref();
    pathB->append(1); // -> childB

    bool pass = (*pathA != *pathB);

    pathA->unref();
    pathB->unref();
    root->unref();
    EXPECT_TRUE(pass) << "SoPath::operator!=: paths with different tails reported equal";
}

// -----------------------------------------------------------------------
// 12. SoPath::append(SoPath*) — append one path onto another
//     Build: root -> child1 -> child2 via path-appending
// -----------------------------------------------------------------------

TEST(NodesPath, SoPathAppendSoPathAppendedPathYieldsCorrectLengthAndTail)
{
    SoSeparator * root   = new SoSeparator;
    root->ref();
    SoSeparator * child1 = new SoSeparator;
    root->addChild(child1);
    SoSeparator * child2 = new SoSeparator;
    child1->addChild(child2);

    // pathA: root -> child1
    SoPath * pathA = new SoPath(root);
    pathA->ref();
    pathA->append(0);

    // pathB: child1 -> child2
    SoPath * pathB = new SoPath(child1);
    pathB->ref();
    pathB->append(0);

    // Append pathB onto pathA; the head of pathB must be the tail of pathA
    pathA->append(pathB);

    bool pass = (pathA->getLength() == 3) &&
                (pathA->getTail()   == child2);

    pathB->unref();
    pathA->unref();
    root->unref();
    EXPECT_TRUE(pass) << "SoPath::append(SoPath*): unexpected length or tail after path-append";
}

// -----------------------------------------------------------------------
// 13. SoPath::findFork() — returns index of last common node
// -----------------------------------------------------------------------

TEST(NodesPath, SoPathFindForkReturnsIndexOfLastCommonAncestor)
{
    SoSeparator * root   = new SoSeparator;
    root->ref();
    SoSeparator * child0 = new SoSeparator;
    root->addChild(child0);
    SoSeparator * child1 = new SoSeparator;
    root->addChild(child1);

    // pathA: root -> child0
    SoPath * pathA = new SoPath(root);
    pathA->ref();
    pathA->append(0);

    // pathB: root -> child1
    SoPath * pathB = new SoPath(root);
    pathB->ref();
    pathB->append(1);

    // Fork is at index 0 (root is the last common node)
    int forkIndex = pathA->findFork(pathB);
    bool pass = (forkIndex == 0);

    pathA->unref();
    pathB->unref();
    root->unref();
    EXPECT_TRUE(pass) << "SoPath::findFork: unexpected fork index";
}

// -----------------------------------------------------------------------
// 14. SoPath::containsPath() — superset path contains subpath
// -----------------------------------------------------------------------

TEST(NodesPath, SoPathContainsPathLongerPathContainsItsSubPath)
{
    SoSeparator * root  = new SoSeparator;
    root->ref();
    SoSeparator * child = new SoSeparator;
    root->addChild(child);
    SoCube * leaf = new SoCube;
    child->addChild(leaf);

    // full: root -> child -> leaf
    SoPath * full = new SoPath(root);
    full->ref();
    full->append(0); // -> child
    full->append(0); // -> leaf

    // sub: root -> child
    SoPath * sub = new SoPath(root);
    sub->ref();
    sub->append(0);

    bool pass = (full->containsPath(sub)  == TRUE)  &&
                (sub->containsPath(full)  == FALSE);

    full->unref();
    sub->unref();
    root->unref();
    EXPECT_TRUE(pass) << "SoPath::containsPath: unexpected containment result";
}

// -----------------------------------------------------------------------
// 15. SoPath::push() / pop() — push increases length; pop decreases it
// -----------------------------------------------------------------------

TEST(NodesPath, SoPathPushPopLengthChangesCorrectly)
{
    SoSeparator * root  = new SoSeparator;
    root->ref();
    SoSeparator * child = new SoSeparator;
    root->addChild(child);

    SoPath * path = new SoPath(root);
    path->ref();

    int lenBefore = path->getLength(); // 1
    path->push(0);                     // push child index 0
    int lenAfterPush = path->getLength();
    path->pop();
    int lenAfterPop = path->getLength();

    bool pass = (lenBefore == 1) &&
                (lenAfterPush == 2) &&
                (lenAfterPop  == 1);

    path->unref();
    root->unref();
    EXPECT_TRUE(pass) << "SoPath::push/pop: unexpected path length";
}
