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
 * @file test_sb_plist.cpp
 * @brief Tests for SbPList, SbStringList, SbIntList, and SbVec3fList.
 *
 * Covers (lists/ subsystem, 75.6 %):
 *   SbPList:
 *     append, getLength, operator[], find, insert, remove, removeFast,
 *     truncate, copy, operator=, operator==, operator!=, fit,
 *     getArrayPtr, get, set
 *   SbStringList:
 *     append strings, getLength, operator[]
 *   SbIntList:
 *     append, operator[], find
 *   SbVec3fList:
 *     append, getLength, operator[]
 */

#include "../test_utils.h"

// Public header that pulls in SbPList + companions
#include <Inventor/SbPList.h>
#include <Inventor/SbString.h>
#include <Inventor/SbVec3f.h>

using namespace ObolTest;

TEST(BaseSbPlist, SbPListDefaultIsEmpty)
{
    SbPList list;
    bool pass = (list.getLength() == 0);
    EXPECT_TRUE(pass) << "SbPList should start empty";
}

TEST(BaseSbPlist, SbPListAppendIncreasesLength)
{
    SbPList list;
    int a = 1, b = 2, c = 3;
    list.append(&a);
    list.append(&b);
    list.append(&c);
    bool pass = (list.getLength() == 3);
    EXPECT_TRUE(pass) << "SbPList::append did not increase length";
}

TEST(BaseSbPlist, SbPListOperatorReturnsCorrectElement)
{
    SbPList list;
    int a = 10, b = 20;
    list.append(&a);
    list.append(&b);
    bool pass = (list[0] == &a) && (list[1] == &b);
    EXPECT_TRUE(pass) << "SbPList operator[] returned wrong element";
}

TEST(BaseSbPlist, SbPListFindReturnsCorrectIndex)
{
    SbPList list;
    int a = 1, b = 2, c = 3;
    list.append(&a);
    list.append(&b);
    list.append(&c);
    bool pass = (list.find(&b) == 1) && (list.find(&c) == 2);
    EXPECT_TRUE(pass) << "SbPList::find returned wrong index";
}

TEST(BaseSbPlist, SbPListFindReturns1ForMissingElement)
{
    SbPList list;
    int a = 5;
    bool pass = (list.find(&a) == -1);
    EXPECT_TRUE(pass) << "SbPList::find should return -1 for missing element";
}

TEST(BaseSbPlist, SbPListInsertAtPosition)
{
    SbPList list;
    int a = 1, b = 2, c = 99;
    list.append(&a);
    list.append(&b);
    list.insert(&c, 1); // insert before index 1
    // list: a, c, b
    bool pass = (list.getLength() == 3) &&
                (list[0] == &a) &&
                (list[1] == &c) &&
                (list[2] == &b);
    EXPECT_TRUE(pass) << "SbPList::insert at position failed";
}

TEST(BaseSbPlist, SbPListRemoveByIndex)
{
    SbPList list;
    int a = 1, b = 2, c = 3;
    list.append(&a);
    list.append(&b);
    list.append(&c);
    list.remove(1); // remove b
    bool pass = (list.getLength() == 2) &&
                (list[0] == &a) &&
                (list[1] == &c);
    EXPECT_TRUE(pass) << "SbPList::remove by index failed";
}

TEST(BaseSbPlist, SbPListRemoveFastRemovesLastElementIntoSlot)
{
    SbPList list;
    int a = 1, b = 2, c = 3;
    list.append(&a);
    list.append(&b);
    list.append(&c);
    list.removeFast(0); // removes a, moves c into slot 0
    bool pass = (list.getLength() == 2) &&
                (list.find(&a) == -1);
    EXPECT_TRUE(pass) << "SbPList::removeFast failed";
}

TEST(BaseSbPlist, SbPListRemoveItemByPointer)
{
    SbPList list;
    int a = 1, b = 2, c = 3;
    list.append(&a);
    list.append(&b);
    list.append(&c);
    list.removeItem(&b);
    bool pass = (list.getLength() == 2) && (list.find(&b) == -1);
    EXPECT_TRUE(pass) << "SbPList::removeItem failed";
}

TEST(BaseSbPlist, SbPListTruncateShortensTheList)
{
    SbPList list;
    int a = 1, b = 2, c = 3, d = 4;
    list.append(&a);
    list.append(&b);
    list.append(&c);
    list.append(&d);
    list.truncate(2);
    bool pass = (list.getLength() == 2) &&
                (list[0] == &a) &&
                (list[1] == &b);
    EXPECT_TRUE(pass) << "SbPList::truncate failed";
}

TEST(BaseSbPlist, SbPListCopyConstructorReplicatesElements)
{
    SbPList orig;
    int a = 1, b = 2;
    orig.append(&a);
    orig.append(&b);
    SbPList copy(orig);
    bool pass = (copy.getLength() == 2) &&
                (copy[0] == &a) && (copy[1] == &b);
    EXPECT_TRUE(pass) << "SbPList copy constructor failed";
}

TEST(BaseSbPlist, SbPListOperatorCopiesElements)
{
    SbPList orig;
    int a = 1, b = 2;
    orig.append(&a);
    orig.append(&b);
    SbPList copy;
    copy = orig;
    bool pass = (copy.getLength() == 2) &&
                (copy[0] == &a) && (copy[1] == &b);
    EXPECT_TRUE(pass) << "SbPList operator= failed";
}

TEST(BaseSbPlist, SbPListCopyMemberFunctionReplicatesElements)
{
    SbPList orig;
    int a = 10, b = 20;
    orig.append(&a);
    orig.append(&b);
    SbPList copy;
    copy.copy(orig);
    bool pass = (copy.getLength() == 2) &&
                (copy[0] == &a) && (copy[1] == &b);
    EXPECT_TRUE(pass) << "SbPList::copy() failed";
}

TEST(BaseSbPlist, SbPListOperatorForEqualLists)
{
    SbPList a, b;
    int v1 = 1, v2 = 2;
    a.append(&v1); a.append(&v2);
    b.append(&v1); b.append(&v2);
    bool pass = (a == b);
    EXPECT_TRUE(pass) << "SbPList operator== failed for equal lists";
}

TEST(BaseSbPlist, SbPListOperatorForDifferentLists)
{
    SbPList a, b;
    int v1 = 1, v2 = 2;
    a.append(&v1);
    b.append(&v2);
    bool pass = (a != b);
    EXPECT_TRUE(pass) << "SbPList operator!= failed for different lists";
}

TEST(BaseSbPlist, SbPListGetAndSet)
{
    SbPList list;
    int a = 5, b = 10;
    list.append(&a);
    // get
    bool passGet = (list.get(0) == &a);
    // set
    list.set(0, &b);
    bool passSet = (list.get(0) == &b);
    bool pass = passGet && passSet;
    EXPECT_TRUE(pass) << "SbPList get/set failed";
}

TEST(BaseSbPlist, SbPListGetArrayPtrReturnsNonNullForNonEmptyList)
{
    SbPList list;
    int a = 1, b = 2;
    list.append(&a);
    list.append(&b);
    void ** arr = list.getArrayPtr();
    bool pass = (arr != nullptr) && (arr[0] == &a) && (arr[1] == &b);
    EXPECT_TRUE(pass) << "SbPList::getArrayPtr failed";
}

TEST(BaseSbPlist, SbPListFitDoesNotCrash)
{
    SbPList list;
    int a = 1;
    list.append(&a);
    list.fit(); // should compact internal storage
    bool pass = (list.getLength() == 1) && (list[0] == &a);
    EXPECT_TRUE(pass) << "SbPList::fit changed list contents";
}

// =======================================================================
// SbStringList tests
// =======================================================================

TEST(BaseSbPlist, SbStringListAppendAndRetrieveStrings)
{
    SbStringList list;
    SbString * s1 = new SbString("hello");
    SbString * s2 = new SbString("world");
    list.append(s1);
    list.append(s2);
    bool pass = (list.getLength() == 2) &&
                (list[0] == s1) &&
                (list[1] == s2);
    delete s1;
    delete s2;
    EXPECT_TRUE(pass) << "SbStringList append/retrieve failed";
}

// =======================================================================
// SbIntList tests
// =======================================================================

TEST(BaseSbPlist, SbIntListAppendAndOperator)
{
    SbIntList list;
    list.append(10);
    list.append(20);
    list.append(30);
    bool pass = (list.getLength() == 3) &&
                (list[0] == 10) &&
                (list[1] == 20) &&
                (list[2] == 30);
    EXPECT_TRUE(pass) << "SbIntList append/operator[] failed";
}

TEST(BaseSbPlist, SbIntListFindReturnsCorrectIndex)
{
    SbIntList list;
    list.append(5);
    list.append(10);
    list.append(15);
    bool pass = (list.find(10) == 1) && (list.find(99) == -1);
    EXPECT_TRUE(pass) << "SbIntList::find failed";
}

// =======================================================================
// SbVec3fList tests
// =======================================================================

TEST(BaseSbPlist, SbVec3fListAppendAndRetrieve)
{
    SbVec3fList list;
    SbVec3f v1(1.0f, 0.0f, 0.0f);
    SbVec3f v2(0.0f, 1.0f, 0.0f);
    list.append(&v1);
    list.append(&v2);
    bool pass = (list.getLength() == 2) &&
                (*list[0] == v1) &&
                (*list[1] == v2);
    EXPECT_TRUE(pass) << "SbVec3fList append/retrieve failed";
}
