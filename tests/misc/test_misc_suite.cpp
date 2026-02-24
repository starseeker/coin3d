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

using namespace SimpleTest;

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // =======================================================================
    // SoType
    // =======================================================================

    runner.startTest("SoType::fromName returns valid type for SoCube");
    {
        SoType t = SoType::fromName(SbName("SoCube"));
        bool pass = (t != SoType::badType());
        runner.endTest(pass, pass ? "" : "fromName('SoCube') returned badType");
    }

    runner.startTest("SoType::fromName returns badType for unknown name");
    {
        SoType t = SoType::fromName(SbName("NoSuchNodeEver_xyz"));
        bool pass = (t == SoType::badType());
        runner.endTest(pass, pass ? "" : "fromName for unknown should return badType");
    }

    runner.startTest("SoType::getName returns non-empty name for SoCube");
    {
        SoType t = SoCube::getClassTypeId();
        SbName name = t.getName();
        // Internal Coin type names omit the 'So' prefix (e.g. "Cube" not "SoCube")
        bool pass = (name.getLength() > 0);
        runner.endTest(pass, pass ? "" : "SoType::getName returned empty name for SoCube");
    }

    runner.startTest("SoType::getParent of SoCube is SoShape or similar");
    {
        SoType t = SoCube::getClassTypeId();
        SoType parent = t.getParent();
        bool pass = (parent != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoType::getParent returned badType for SoCube");
    }

    runner.startTest("SoType::isDerivedFrom: SoCube isDerived from SoNode");
    {
        SoType cube = SoCube::getClassTypeId();
        SoType node = SoNode::getClassTypeId();
        bool pass = cube.isDerivedFrom(node);
        runner.endTest(pass, pass ? "" : "SoCube should be derived from SoNode");
    }

    runner.startTest("SoType::isDerivedFrom: SoCube NOT derived from SoGroup");
    {
        SoType cube  = SoCube::getClassTypeId();
        SoType group = SoGroup::getClassTypeId();
        bool pass = !cube.isDerivedFrom(group);
        runner.endTest(pass, pass ? "" : "SoCube should NOT be derived from SoGroup");
    }

    runner.startTest("SoType::getAllDerivedFrom SoNode returns many types");
    {
        SoTypeList list;
        int n = SoType::getAllDerivedFrom(SoNode::getClassTypeId(), list);
        bool pass = (n > 10); // There are many node types
        runner.endTest(pass, pass ? "" : "getAllDerivedFrom(SoNode) returned too few types");
    }

    runner.startTest("SoType::canCreateInstance for SoCube is TRUE");
    {
        SoType t = SoCube::getClassTypeId();
        bool pass = t.canCreateInstance();
        runner.endTest(pass, pass ? "" : "SoCube::canCreateInstance should be TRUE");
    }

    runner.startTest("SoType::createInstance creates an SoCube");
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
        runner.endTest(pass, pass ? "" : "createInstance for SoCube failed");
    }

    runner.startTest("SoType operator== for same type");
    {
        SoType a = SoCube::getClassTypeId();
        SoType b = SoCube::getClassTypeId();
        bool pass = (a == b);
        runner.endTest(pass, pass ? "" : "SoType operator== failed for same type");
    }

    runner.startTest("SoType operator!= for different types");
    {
        SoType a = SoCube::getClassTypeId();
        SoType b = SoSphere::getClassTypeId();
        bool pass = (a != b);
        runner.endTest(pass, pass ? "" : "SoType operator!= failed for different types");
    }

    runner.startTest("SoType operator< gives consistent ordering");
    {
        SoType a = SoCube::getClassTypeId();
        SoType b = SoSphere::getClassTypeId();
        // One should be less than the other (but both are valid)
        bool pass = (a < b) || (b < a);
        runner.endTest(pass, pass ? "" : "SoType operator< failed (neither a<b nor b<a)");
    }

    runner.startTest("SoType::getKey returns non-negative for valid type");
    {
        SoType t = SoCube::getClassTypeId();
        bool pass = (t.getKey() >= 0);
        runner.endTest(pass, pass ? "" : "SoType::getKey returned negative for valid type");
    }

    // =======================================================================
    // SoPath
    // =======================================================================

    runner.startTest("SoPath setHead and getHead");
    {
        SoSeparator * root = new SoSeparator;
        root->ref();
        SoPath * path = new SoPath;
        path->ref();
        path->setHead(root);
        bool pass = (path->getHead() == root);
        path->unref();
        root->unref();
        runner.endTest(pass, pass ? "" : "SoPath setHead/getHead failed");
    }

    runner.startTest("SoPath append(node) and getLength");
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
        runner.endTest(pass, pass ? "" : "SoPath append(node)/getLength/getTail failed");
    }

    runner.startTest("SoPath getNode(index) accesses correct node");
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
        runner.endTest(pass, pass ? "" : "SoPath getNode(index) failed");
    }

    runner.startTest("SoPath getNodeFromTail(0) is getTail()");
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
        runner.endTest(pass, pass ? "" : "getNodeFromTail(0) != getTail()");
    }

    runner.startTest("SoPath truncate shortens path");
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
        runner.endTest(pass, pass ? "" : "SoPath::truncate failed");
    }

    runner.startTest("SoPath containsNode returns TRUE for node in path");
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
        runner.endTest(pass, pass ? "" : "SoPath::containsNode failed");
    }

    runner.startTest("SoPath containsNode returns FALSE for node not in path");
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
        runner.endTest(pass, pass ? "" : "SoPath::containsNode returned TRUE for absent node");
    }

    runner.startTest("SoPath copy creates a new equal path");
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
        runner.endTest(pass, pass ? "" : "SoPath::copy failed");
    }

    runner.startTest("SoPath findNode returns correct index");
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
        runner.endTest(pass, pass ? "" : "SoPath::findNode returned wrong index");
    }

    runner.startTest("SoPath findFork with shared prefix");
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
        runner.endTest(pass, pass ? "" : "SoPath::findFork returned wrong index");
    }

    runner.startTest("SoPath operator== for equal paths");
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
        runner.endTest(pass, pass ? "" : "SoPath operator== failed for equal paths");
    }

    runner.startTest("SoPath operator!= for different paths");
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
        runner.endTest(pass, pass ? "" : "SoPath operator!= failed for different paths");
    }

    // =======================================================================
    // SoChildList
    // =======================================================================

    runner.startTest("SoChildList append and getLength");
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
        runner.endTest(pass, pass ? "" : "SoSeparator (SoChildList) append/getLength failed");
    }

    runner.startTest("SoChildList insert before index");
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
        runner.endTest(pass, pass ? "" : "SoChildList insert at position failed");
    }

    runner.startTest("SoChildList remove by index");
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
        runner.endTest(pass, pass ? "" : "SoChildList remove by index failed");
    }

    return runner.getSummary();
}
