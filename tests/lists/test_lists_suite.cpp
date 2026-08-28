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
#include <Inventor/lists/SoCallbackList.h>
#include <Inventor/SoPath.h>
#include <Inventor/SoType.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/fields/SoSFFloat.h>
#include <Inventor/fields/SoMFFloat.h>

using namespace ObolTest;

namespace {

struct CallbackPayload {
    int value;
};

struct CallbackState {
    SoCallbackList * list = nullptr;
    std::vector<int> calls;
    bool removed = false;
};

void callbackSecond(void * userdata, CallbackPayload * payload)
{
    auto * state = static_cast<CallbackState *>(userdata);
    state->calls.push_back(20 + payload->value);
}

void callbackRemoving(void * userdata, CallbackPayload * payload)
{
    auto * state = static_cast<CallbackState *>(userdata);
    state->calls.push_back(10 + payload->value);
    if (!state->removed) {
        state->removed = true;
        state->list->removeCallback(callbackSecond, state);
    }
}

void callbackIncrement(void * userdata, CallbackPayload * payload)
{
    *static_cast<int *>(userdata) += payload->value;
}

} // namespace

TEST(CallbackList, TypedCallbacksUseSnapshotSemantics)
{
    SoCallbackList callbacks;
    CallbackState state;
    state.list = &callbacks;
    callbacks.addCallback(callbackRemoving, &state);
    callbacks.addCallback(callbackSecond, &state);

    CallbackPayload payload{3};
    callbacks.invokeCallbacks(&payload);
    EXPECT_EQ(state.calls, (std::vector<int>{13, 23}));

    state.calls.clear();
    callbacks.invokeCallbacks(&payload);
    EXPECT_EQ(state.calls, (std::vector<int>{13}));
}

TEST(CallbackList, CopiesOwnIndependentTypedEntries)
{
    SoCallbackList original;
    int total = 0;
    original.addCallback(callbackIncrement, &total);

    SoCallbackList copied(original);
    SoCallbackList assigned;
    assigned = original;
    original.clearCallbacks();

    CallbackPayload payload{4};
    copied.invokeCallbacks(&payload);
    assigned.invokeCallbacks(&payload);
    EXPECT_EQ(total, 8);
}

TEST(ListsSuite, SbPListDefaultConstructionGetLength0)
{
    SbPList list;
    EXPECT_TRUE((list.getLength() == 0)) << "SbPList default length should be 0";
}

TEST(ListsSuite, SbPListAppendPtrGetLength1AndOperator0EqualsPtr)
{
    SbPList list;
    int dummy = 42;
    void * ptr = static_cast<void *>(&dummy);
    list.append(ptr);
    EXPECT_TRUE((list.getLength() == 1) && (list[0] == ptr)) << "SbPList append failed: wrong length or mismatched pointer";
}

TEST(ListsSuite, SbPListFindReturnsCorrectIndexForAppendedPointer)
{
    SbPList list;
    int a = 1, b = 2;
    list.append(static_cast<void *>(&a));
    list.append(static_cast<void *>(&b));
    EXPECT_TRUE((list.find(static_cast<void *>(&b)) == 1)) << "SbPList find() should return index 1 for second element";
}

TEST(ListsSuite, SbPListFindReturns1ForAbsentPointer)
{
    SbPList list;
    int a = 1, b = 2;
    list.append(static_cast<void *>(&a));
    EXPECT_TRUE((list.find(static_cast<void *>(&b)) == -1)) << "SbPList find() should return -1 for pointer not in list";
}

TEST(ListsSuite, SbPListInsertAtIndex0ShiftsExistingElement)
{
    SbPList list;
    int a = 1, b = 2;
    list.append(static_cast<void *>(&a));
    list.insert(static_cast<void *>(&b), 0);
    EXPECT_TRUE((list.getLength() == 2) &&
                (list[0] == static_cast<void *>(&b)) &&
                (list[1] == static_cast<void *>(&a))) << "SbPList insert() at 0 should shift existing element to index 1";
}

TEST(ListsSuite, SbPListRemoveIndexReducesLengthBy1)
{
    SbPList list;
    int a = 1, b = 2, c = 3;
    list.append(static_cast<void *>(&a));
    list.append(static_cast<void *>(&b));
    list.append(static_cast<void *>(&c));
    list.remove(1);
    EXPECT_TRUE((list.getLength() == 2)) << "SbPList remove() should reduce length from 3 to 2";
}

TEST(ListsSuite, SbPListRemoveFastIndexReducesLengthBy1)
{
    SbPList list;
    int a = 1, b = 2, c = 3;
    list.append(static_cast<void *>(&a));
    list.append(static_cast<void *>(&b));
    list.append(static_cast<void *>(&c));
    list.removeFast(0);
    EXPECT_TRUE((list.getLength() == 2)) << "SbPList removeFast() should reduce length from 3 to 2";
}

TEST(ListsSuite, SbPListTruncate1ReducesLengthTo1)
{
    SbPList list;
    int a = 1, b = 2, c = 3;
    list.append(static_cast<void *>(&a));
    list.append(static_cast<void *>(&b));
    list.append(static_cast<void *>(&c));
    list.truncate(1);
    EXPECT_TRUE((list.getLength() == 1)) << "SbPList truncate(1) should reduce length to 1";
}

TEST(ListsSuite, SbPListOperatorOnTwoEqualListsReturnsNonZero)
{
    SbPList a, b;
    int x = 5;
    a.append(static_cast<void *>(&x));
    b.append(static_cast<void *>(&x));
    EXPECT_TRUE((a == b)) << "SbPList operator== should return true for equal lists";
}

TEST(ListsSuite, SbPListOperatorOnTwoDifferentListsReturnsNonZero)
{
    SbPList a, b;
    int x = 5, y = 6;
    a.append(static_cast<void *>(&x));
    b.append(static_cast<void *>(&y));
    EXPECT_TRUE((a != b)) << "SbPList operator!= should return true for different lists";
}

// -----------------------------------------------------------------------
// SoNodeList
// -----------------------------------------------------------------------

TEST(ListsSuite, SoNodeListDefaultConstructionGetLength0)
{
    SoNodeList list;
    EXPECT_TRUE((list.getLength() == 0)) << "SoNodeList default length should be 0";
}

TEST(ListsSuite, SoNodeListAppendNodeGetLength1AndOperatorReturnsNode)
{
    SoNodeList list;
    SoCube * cube = new SoCube;
    cube->ref();
    list.append(cube);
    EXPECT_TRUE((list.getLength() == 1) && (list[0] == cube)) << "SoNodeList append failed: wrong length or mismatched node pointer";
    cube->unref();
}

TEST(ListsSuite, SoNodeListAppendedNodeHasRefcount1WhenReferencingIsTRUE)
{
    SoNodeList list;
    // SoBaseList defaults to referencing=TRUE, so append calls ref()
    SoCube * cube = new SoCube;
    cube->ref();                       // keep alive during test
    int refBefore = cube->getRefCount();
    list.append(cube);
    EXPECT_TRUE((cube->getRefCount() > refBefore)) << "SoNodeList should increment refcount on append when referencing=TRUE";
    cube->unref();
}

// -----------------------------------------------------------------------
// SoPathList
// -----------------------------------------------------------------------

TEST(ListsSuite, SoPathListDefaultConstructionGetLength0)
{
    SoPathList list;
    EXPECT_TRUE((list.getLength() == 0)) << "SoPathList default length should be 0";
}

TEST(ListsSuite, SoPathListAppendPathGetLength1)
{
    SoPathList list;
    SoSeparator * root = new SoSeparator;
    root->ref();
    SoPath * path = new SoPath(root);
    path->ref();
    list.append(path);
    EXPECT_TRUE((list.getLength() == 1)) << "SoPathList append failed: length should be 1";
    path->unref();
    root->unref();
}

TEST(ListsSuite, SoPathListFindPathReturnsCorrectIndex)
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
    EXPECT_TRUE((idx == 1)) << "SoPathList findPath() should return index 1 for second path";
    p1->unref();
    p2->unref();
    root1->unref();
    root2->unref();
}

TEST(ListsSuite, SoPathListFindPathReturns1ForAbsentPath)
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
    EXPECT_TRUE((idx == -1)) << "SoPathList findPath() should return -1 for path not in list";
    p1->unref();
    p2->unref();
    root1->unref();
    root2->unref();
}

// -----------------------------------------------------------------------
// SoTypeList
// -----------------------------------------------------------------------

TEST(ListsSuite, SoTypeListDefaultConstructionGetLength0)
{
    SoTypeList list;
    EXPECT_TRUE((list.getLength() == 0)) << "SoTypeList default length should be 0";
}

TEST(ListsSuite, SoTypeListAppendTypeGetLength1)
{
    SoTypeList list;
    list.append(SoCube::getClassTypeId());
    EXPECT_TRUE((list.getLength() == 1)) << "SoTypeList append failed: length should be 1";
}

TEST(ListsSuite, SoTypeListFindReturnsCorrectIndex)
{
    SoTypeList list;
    list.append(SoCube::getClassTypeId());
    list.append(SoSphere::getClassTypeId());
    int idx = list.find(SoSphere::getClassTypeId());
    EXPECT_TRUE((idx == 1)) << "SoTypeList find() should return 1 for second appended type";
}

TEST(ListsSuite, SoTypeListFindReturns1ForAbsentType)
{
    SoTypeList list;
    list.append(SoCube::getClassTypeId());
    int idx = list.find(SoSphere::getClassTypeId());
    EXPECT_TRUE((idx == -1)) << "SoTypeList find() should return -1 for type not in list";
}

TEST(ListsSuite, SoTypeListOperatorReturnsCorrectType)
{
    SoTypeList list;
    list.append(SoCube::getClassTypeId());
    list.append(SoSphere::getClassTypeId());
    EXPECT_TRUE((list[0] == SoCube::getClassTypeId()) &&
                (list[1] == SoSphere::getClassTypeId())) << "SoTypeList operator[] returned wrong type";
}

TEST(ListsSuite, SoTypeListInsertPlacesTypeAtSpecifiedIndex)
{
    SoTypeList list;
    list.append(SoCube::getClassTypeId());
    list.append(SoSphere::getClassTypeId());
    list.insert(SoSeparator::getClassTypeId(), 1);
    EXPECT_TRUE((list.getLength() == 3) &&
                (list[1] == SoSeparator::getClassTypeId())) << "SoTypeList insert() failed: wrong length or wrong type at index 1";
}

TEST(ListsSuite, SoTypeListSetReplacesTypeAtIndex)
{
    SoTypeList list;
    list.append(SoCube::getClassTypeId());
    list.set(0, SoSphere::getClassTypeId());
    EXPECT_TRUE((list[0] == SoSphere::getClassTypeId())) << "SoTypeList set() failed: type at index 0 was not replaced";
}

// -----------------------------------------------------------------------
// SoFieldList
// -----------------------------------------------------------------------

TEST(ListsSuite, SoFieldListDefaultConstructionGetLength0)
{
    SoFieldList list;
    EXPECT_TRUE((list.getLength() == 0)) << "SoFieldList default length should be 0";
}

TEST(ListsSuite, SoFieldListAppendFieldGetLength1)
{
    SoFieldList list;
    SoSFFloat field;
    list.append(&field);
    EXPECT_TRUE((list.getLength() == 1)) << "SoFieldList append failed: length should be 1";
}

TEST(ListsSuite, SoFieldListOperatorReturnsAppendedFieldPointer)
{
    SoFieldList list;
    SoSFFloat field;
    list.append(&field);
    EXPECT_TRUE((list[0] == &field)) << "SoFieldList operator[] should return the appended field pointer";
}
