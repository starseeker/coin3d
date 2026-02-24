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
 * @file test_lists_suite.cpp
 * @brief Tests for Coin3D list classes.
 *
 * Covers:
 *   SbPList     - construction, append, find, insert, remove, removeFast,
 *                 truncate, operator==, operator!=
 *   SoNodeList  - construction, append, ref-counting
 *   SoPathList  - construction, append, findPath
 *   SoTypeList  - construction, append, find, operator[], insert, set
 *   SoFieldList - construction, append, getLength, operator[]
 */

#include "../test_utils.h"
#include <Inventor/lists/SbPList.h>
#include <Inventor/lists/SoNodeList.h>
#include <Inventor/lists/SoPathList.h>
#include <Inventor/lists/SoTypeList.h>
#include <Inventor/lists/SoFieldList.h>
#include <Inventor/SoPath.h>
#include <Inventor/SoType.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/fields/SoSFFloat.h>
#include <Inventor/fields/SoMFFloat.h>

using namespace SimpleTest;

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // -----------------------------------------------------------------------
    // SbPList
    // -----------------------------------------------------------------------

    runner.startTest("SbPList: default construction - getLength() == 0");
    {
        SbPList list;
        bool pass = (list.getLength() == 0);
        runner.endTest(pass, pass ? "" : "SbPList default length should be 0");
    }

    runner.startTest("SbPList: append(ptr) - getLength() == 1 and operator[](0) equals ptr");
    {
        SbPList list;
        int dummy = 42;
        void * ptr = static_cast<void *>(&dummy);
        list.append(ptr);
        bool pass = (list.getLength() == 1) && (list[0] == ptr);
        runner.endTest(pass, pass ? "" :
            "SbPList append failed: wrong length or mismatched pointer");
    }

    runner.startTest("SbPList: find() returns correct index for appended pointer");
    {
        SbPList list;
        int a = 1, b = 2;
        list.append(static_cast<void *>(&a));
        list.append(static_cast<void *>(&b));
        bool pass = (list.find(static_cast<void *>(&b)) == 1);
        runner.endTest(pass, pass ? "" :
            "SbPList find() should return index 1 for second element");
    }

    runner.startTest("SbPList: find() returns -1 for absent pointer");
    {
        SbPList list;
        int a = 1, b = 2;
        list.append(static_cast<void *>(&a));
        bool pass = (list.find(static_cast<void *>(&b)) == -1);
        runner.endTest(pass, pass ? "" :
            "SbPList find() should return -1 for pointer not in list");
    }

    runner.startTest("SbPList: insert() at index 0 shifts existing element");
    {
        SbPList list;
        int a = 1, b = 2;
        list.append(static_cast<void *>(&a));
        list.insert(static_cast<void *>(&b), 0);
        bool pass = (list.getLength() == 2) &&
                    (list[0] == static_cast<void *>(&b)) &&
                    (list[1] == static_cast<void *>(&a));
        runner.endTest(pass, pass ? "" :
            "SbPList insert() at 0 should shift existing element to index 1");
    }

    runner.startTest("SbPList: remove(index) reduces length by 1");
    {
        SbPList list;
        int a = 1, b = 2, c = 3;
        list.append(static_cast<void *>(&a));
        list.append(static_cast<void *>(&b));
        list.append(static_cast<void *>(&c));
        list.remove(1);
        bool pass = (list.getLength() == 2);
        runner.endTest(pass, pass ? "" :
            "SbPList remove() should reduce length from 3 to 2");
    }

    runner.startTest("SbPList: removeFast(index) reduces length by 1");
    {
        SbPList list;
        int a = 1, b = 2, c = 3;
        list.append(static_cast<void *>(&a));
        list.append(static_cast<void *>(&b));
        list.append(static_cast<void *>(&c));
        list.removeFast(0);
        bool pass = (list.getLength() == 2);
        runner.endTest(pass, pass ? "" :
            "SbPList removeFast() should reduce length from 3 to 2");
    }

    runner.startTest("SbPList: truncate(1) reduces length to 1");
    {
        SbPList list;
        int a = 1, b = 2, c = 3;
        list.append(static_cast<void *>(&a));
        list.append(static_cast<void *>(&b));
        list.append(static_cast<void *>(&c));
        list.truncate(1);
        bool pass = (list.getLength() == 1);
        runner.endTest(pass, pass ? "" :
            "SbPList truncate(1) should reduce length to 1");
    }

    runner.startTest("SbPList: operator== on two equal lists returns non-zero");
    {
        SbPList a, b;
        int x = 5;
        a.append(static_cast<void *>(&x));
        b.append(static_cast<void *>(&x));
        bool pass = (a == b);
        runner.endTest(pass, pass ? "" :
            "SbPList operator== should return true for equal lists");
    }

    runner.startTest("SbPList: operator!= on two different lists returns non-zero");
    {
        SbPList a, b;
        int x = 5, y = 6;
        a.append(static_cast<void *>(&x));
        b.append(static_cast<void *>(&y));
        bool pass = (a != b);
        runner.endTest(pass, pass ? "" :
            "SbPList operator!= should return true for different lists");
    }

    // -----------------------------------------------------------------------
    // SoNodeList
    // -----------------------------------------------------------------------

    runner.startTest("SoNodeList: default construction - getLength() == 0");
    {
        SoNodeList list;
        bool pass = (list.getLength() == 0);
        runner.endTest(pass, pass ? "" : "SoNodeList default length should be 0");
    }

    runner.startTest("SoNodeList: append(node) - getLength() == 1 and operator[] returns node");
    {
        SoNodeList list;
        SoCube * cube = new SoCube;
        cube->ref();
        list.append(cube);
        bool pass = (list.getLength() == 1) && (list[0] == cube);
        cube->unref();
        runner.endTest(pass, pass ? "" :
            "SoNodeList append failed: wrong length or mismatched node pointer");
    }

    runner.startTest("SoNodeList: appended node has refcount >= 1 when referencing is TRUE");
    {
        SoNodeList list;
        // SoBaseList defaults to referencing=TRUE, so append calls ref()
        SoCube * cube = new SoCube;
        cube->ref();                       // keep alive during test
        int refBefore = cube->getRefCount();
        list.append(cube);
        bool pass = (cube->getRefCount() > refBefore);
        cube->unref();
        runner.endTest(pass, pass ? "" :
            "SoNodeList should increment refcount on append when referencing=TRUE");
    }

    // -----------------------------------------------------------------------
    // SoPathList
    // -----------------------------------------------------------------------

    runner.startTest("SoPathList: default construction - getLength() == 0");
    {
        SoPathList list;
        bool pass = (list.getLength() == 0);
        runner.endTest(pass, pass ? "" : "SoPathList default length should be 0");
    }

    runner.startTest("SoPathList: append(path) - getLength() == 1");
    {
        SoPathList list;
        SoSeparator * root = new SoSeparator;
        root->ref();
        SoPath * path = new SoPath(root);
        path->ref();
        list.append(path);
        bool pass = (list.getLength() == 1);
        path->unref();
        root->unref();
        runner.endTest(pass, pass ? "" :
            "SoPathList append failed: length should be 1");
    }

    runner.startTest("SoPathList: findPath() returns correct index");
    {
        SoPathList list;
        SoSeparator * root1 = new SoSeparator;
        root1->ref();
        SoSeparator * root2 = new SoSeparator;
        root2->ref();
        SoPath * p1 = new SoPath(root1);
        p1->ref();
        SoPath * p2 = new SoPath(root2);
        p2->ref();
        list.append(p1);
        list.append(p2);
        int idx = list.findPath(*p2);
        bool pass = (idx == 1);
        p1->unref();
        p2->unref();
        root1->unref();
        root2->unref();
        runner.endTest(pass, pass ? "" :
            "SoPathList findPath() should return index 1 for second path");
    }

    runner.startTest("SoPathList: findPath() returns -1 for absent path");
    {
        SoPathList list;
        SoSeparator * root1 = new SoSeparator;
        root1->ref();
        SoSeparator * root2 = new SoSeparator;
        root2->ref();
        SoPath * p1 = new SoPath(root1);
        p1->ref();
        SoPath * p2 = new SoPath(root2);
        p2->ref();
        list.append(p1);
        int idx = list.findPath(*p2);
        bool pass = (idx == -1);
        p1->unref();
        p2->unref();
        root1->unref();
        root2->unref();
        runner.endTest(pass, pass ? "" :
            "SoPathList findPath() should return -1 for path not in list");
    }

    // -----------------------------------------------------------------------
    // SoTypeList
    // -----------------------------------------------------------------------

    runner.startTest("SoTypeList: default construction - getLength() == 0");
    {
        SoTypeList list;
        bool pass = (list.getLength() == 0);
        runner.endTest(pass, pass ? "" : "SoTypeList default length should be 0");
    }

    runner.startTest("SoTypeList: append(type) - getLength() == 1");
    {
        SoTypeList list;
        list.append(SoCube::getClassTypeId());
        bool pass = (list.getLength() == 1);
        runner.endTest(pass, pass ? "" :
            "SoTypeList append failed: length should be 1");
    }

    runner.startTest("SoTypeList: find() returns correct index");
    {
        SoTypeList list;
        list.append(SoCube::getClassTypeId());
        list.append(SoSphere::getClassTypeId());
        int idx = list.find(SoSphere::getClassTypeId());
        bool pass = (idx == 1);
        runner.endTest(pass, pass ? "" :
            "SoTypeList find() should return 1 for second appended type");
    }

    runner.startTest("SoTypeList: find() returns -1 for absent type");
    {
        SoTypeList list;
        list.append(SoCube::getClassTypeId());
        int idx = list.find(SoSphere::getClassTypeId());
        bool pass = (idx == -1);
        runner.endTest(pass, pass ? "" :
            "SoTypeList find() should return -1 for type not in list");
    }

    runner.startTest("SoTypeList: operator[] returns correct type");
    {
        SoTypeList list;
        list.append(SoCube::getClassTypeId());
        list.append(SoSphere::getClassTypeId());
        bool pass = (list[0] == SoCube::getClassTypeId()) &&
                    (list[1] == SoSphere::getClassTypeId());
        runner.endTest(pass, pass ? "" :
            "SoTypeList operator[] returned wrong type");
    }

    runner.startTest("SoTypeList: insert() places type at specified index");
    {
        SoTypeList list;
        list.append(SoCube::getClassTypeId());
        list.append(SoSphere::getClassTypeId());
        list.insert(SoSeparator::getClassTypeId(), 1);
        bool pass = (list.getLength() == 3) &&
                    (list[1] == SoSeparator::getClassTypeId());
        runner.endTest(pass, pass ? "" :
            "SoTypeList insert() failed: wrong length or wrong type at index 1");
    }

    runner.startTest("SoTypeList: set() replaces type at index");
    {
        SoTypeList list;
        list.append(SoCube::getClassTypeId());
        list.set(0, SoSphere::getClassTypeId());
        bool pass = (list[0] == SoSphere::getClassTypeId());
        runner.endTest(pass, pass ? "" :
            "SoTypeList set() failed: type at index 0 was not replaced");
    }

    // -----------------------------------------------------------------------
    // SoFieldList
    // -----------------------------------------------------------------------

    runner.startTest("SoFieldList: default construction - getLength() == 0");
    {
        SoFieldList list;
        bool pass = (list.getLength() == 0);
        runner.endTest(pass, pass ? "" : "SoFieldList default length should be 0");
    }

    runner.startTest("SoFieldList: append(field) - getLength() == 1");
    {
        SoFieldList list;
        SoSFFloat field;
        list.append(&field);
        bool pass = (list.getLength() == 1);
        runner.endTest(pass, pass ? "" :
            "SoFieldList append failed: length should be 1");
    }

    runner.startTest("SoFieldList: operator[] returns appended field pointer");
    {
        SoFieldList list;
        SoSFFloat field;
        list.append(&field);
        bool pass = (list[0] == &field);
        runner.endTest(pass, pass ? "" :
            "SoFieldList operator[] should return the appended field pointer");
    }

    return runner.getSummary() != 0 ? 1 : 0;
}
