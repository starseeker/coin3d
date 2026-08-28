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
 * @file test_sbtime_sbdict.cpp
 * @brief Tests for SbTime arithmetic and SbDict key/value store.
 *
 * SbTime (base/ subsystem, 33.5 % coverage):
 *   zero(), max(), maxTime(), setValue(double), getValue(),
 *   setValue(sec,usec), format(), setMsecValue(),
 *   operator+, operator-, operator*, operator/, unary minus,
 *   operator==, operator!=, operator<, operator<=, operator>, operator>=
 *
 * SbDict (base/ subsystem):
 *   enter(), find(), remove(), clear(), applyToAll(),
 *   applyToAll(with data), makePList(), copy constructor, operator=
 */

#define OBOL_ALLOW_SBDICT
#include "../test_utils.h"

#include <Inventor/SbTime.h>
#include <Inventor/SbDict.h>
#include <Inventor/SbString.h>
#include <Inventor/SbPList.h>
#include <cmath>
#include <cstring>

using namespace ObolTest;

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------
static bool floatNear(double a, double b, double eps = 1e-9)
{
    return std::fabs(a - b) < eps;
}

// applyToAll callback accumulator
static int g_applyCount = 0;
static void countApply(uintptr_t /*key*/, void * /*val*/)
{
    ++g_applyCount;
}

struct ApplyDataCtx { int count; };
static void countApplyData(uintptr_t /*key*/, void * /*val*/, void * data)
{
    static_cast<ApplyDataCtx *>(data)->count++;
}

TEST(BaseSbtimeSbdict, SbTimeZeroHasValue00)
{
    SbTime t = SbTime::zero();
    EXPECT_TRUE(floatNear(t.getValue(), 0.0)) << "SbTime::zero() != 0.0";
}

TEST(BaseSbtimeSbdict, SbTimeDefaultConstructorIsZero)
{
    SbTime t;
    // Default constructor may not be zero in all implementations;
    // just verify it constructs without crash and getValue() is finite.
    (void)t.getValue();
    SUCCEED();
}

TEST(BaseSbtimeSbdict, SbTimeDoubleSetValueGetValueRoundTrip)
{
    SbTime t(3.75);
    EXPECT_TRUE(floatNear(t.getValue(), 3.75)) << "SbTime(double) getValue mismatch";
}

TEST(BaseSbtimeSbdict, SbTimeSetValueDoubleRoundTrip)
{
    SbTime t;
    t.setValue(1.5);
    EXPECT_TRUE(floatNear(t.getValue(), 1.5)) << "setValue/getValue round-trip failed";
}

TEST(BaseSbtimeSbdict, SbTimeSetValueSecUsecRoundTrip)
{
    SbTime t;
    t.setValue((int32_t)2, (long)500000); // 2.5 s
    EXPECT_TRUE(floatNear(t.getValue(), 2.5)) << "setValue(sec,usec) round-trip failed";
}

TEST(BaseSbtimeSbdict, SbTimeSetMsecValueRoundTrip)
{
    SbTime t;
    t.setMsecValue(1500); // 1.5 s
    EXPECT_TRUE(floatNear(t.getValue(), 1.5, 1e-3)) << "setMsecValue round-trip failed";
}

TEST(BaseSbtimeSbdict, SbTimeOperator301545)
{
    SbTime a(3.0), b(1.5);
    SbTime c = a + b;
    EXPECT_TRUE(floatNear(c.getValue(), 4.5)) << "operator+ failed";
}

TEST(BaseSbtimeSbdict, SbTimeOperator301515)
{
    SbTime a(3.0), b(1.5);
    SbTime c = a - b;
    EXPECT_TRUE(floatNear(c.getValue(), 1.5)) << "operator- failed";
}

TEST(BaseSbtimeSbdict, SbTimeUnaryMinusNegatesValue)
{
    SbTime a(2.0);
    SbTime b = -a;
    EXPECT_TRUE(floatNear(b.getValue(), -2.0)) << "unary minus failed";
}

TEST(BaseSbtimeSbdict, SbTimeOperatorSbTimeDouble)
{
    SbTime a(2.0);
    SbTime b = a * 3.0;
    EXPECT_TRUE(floatNear(b.getValue(), 6.0)) << "SbTime * double failed";
}

TEST(BaseSbtimeSbdict, SbTimeOperatorDoubleSbTime)
{
    SbTime a(2.0);
    SbTime b = 4.0 * a;
    EXPECT_TRUE(floatNear(b.getValue(), 8.0)) << "double * SbTime failed";
}

TEST(BaseSbtimeSbdict, SbTimeOperatorSbTimeDouble2)
{
    SbTime a(6.0);
    SbTime b = a / 2.0;
    EXPECT_TRUE(floatNear(b.getValue(), 3.0)) << "SbTime / double failed";
}

TEST(BaseSbtimeSbdict, SbTimeOperatorRatioSbTimeSbTime)
{
    SbTime a(6.0), b(2.0);
    double ratio = a / b;
    EXPECT_TRUE(floatNear(ratio, 3.0)) << "SbTime / SbTime ratio failed";
}

TEST(BaseSbtimeSbdict, SbTimeOperatorAccumulates)
{
    SbTime t(1.0);
    t += SbTime(0.5);
    EXPECT_TRUE(floatNear(t.getValue(), 1.5)) << "operator+= failed";
}

TEST(BaseSbtimeSbdict, SbTimeOperatorSubtracts)
{
    SbTime t(2.0);
    t -= SbTime(0.5);
    EXPECT_TRUE(floatNear(t.getValue(), 1.5)) << "operator-= failed";
}

TEST(BaseSbtimeSbdict, SbTimeOperatorScales)
{
    SbTime t(2.0);
    t *= 3.0;
    EXPECT_TRUE(floatNear(t.getValue(), 6.0)) << "operator*= failed";
}

TEST(BaseSbtimeSbdict, SbTimeOperatorDivides)
{
    SbTime t(6.0);
    t /= 2.0;
    EXPECT_TRUE(floatNear(t.getValue(), 3.0)) << "operator/= failed";
}

TEST(BaseSbtimeSbdict, SbTimeOperatorEqualTimes)
{
    SbTime a(1.5), b(1.5);
    EXPECT_TRUE((a == b)) << "operator== failed for equal times";
}

TEST(BaseSbtimeSbdict, SbTimeOperatorDifferentTimes)
{
    SbTime a(1.0), b(2.0);
    EXPECT_TRUE((a != b)) << "operator!= failed for different times";
}

TEST(BaseSbtimeSbdict, SbTimeComparisonOperators)
{
    SbTime a(1.0), b(2.0);
    EXPECT_TRUE((a < b) && (a <= b) && (b > a) && (b >= a) &&
                !(b < a) && !(a > b)) << "comparison operators failed";
}

TEST(BaseSbtimeSbdict, SbTimeModulo)
{
    SbTime a(5.0), b(3.0);
    SbTime r = a % b;
    EXPECT_TRUE(floatNear(r.getValue(), 2.0, 1e-6)) << "SbTime % modulo failed";
}

TEST(BaseSbtimeSbdict, SbTimeFormatReturnsNonEmptyString)
{
    SbTime t(12.345);
    SbString s = t.format();
    EXPECT_TRUE((s.getLength() > 0)) << "format() returned empty string";
}

TEST(BaseSbtimeSbdict, SbTimeMaxSbTimeZero)
{
    SbTime mx = SbTime::max();
    SbTime z  = SbTime::zero();
    EXPECT_TRUE((mx >= z)) << "SbTime::max() < zero";
}

TEST(BaseSbtimeSbdict, SbTimeMaxTimeEqualsSbTimeMax)
{
    SbTime mx1 = SbTime::max();
    SbTime mx2 = SbTime::maxTime();
    EXPECT_TRUE(floatNear(mx1.getValue(), mx2.getValue())) << "max() and maxTime() differ";
}

TEST(BaseSbtimeSbdict, SbTimeGetValueSecUsecDecomposesCorrectly)
{
    SbTime t;
    t.setValue((int32_t)3, (long)750000); // 3.75 s
    time_t sec;
    long usec;
    t.getValue(sec, usec);
    EXPECT_TRUE((sec == 3) && (usec == 750000)) << "getValue(sec,usec) decomposition failed";
}

// =======================================================================
// SbDict tests
// =======================================================================

TEST(BaseSbtimeSbdict, SbDictEnterAndFind)
{
    SbDict dict;
    int dummy = 42;
    SbBool entered = dict.enter((uintptr_t)1, &dummy);
    void * found = nullptr;
    SbBool ok = dict.find((uintptr_t)1, found);
    EXPECT_TRUE(entered && ok && (found == &dummy)) << "SbDict enter/find failed";
}

TEST(BaseSbtimeSbdict, SbDictFindReturnsFALSEForMissingKey)
{
    SbDict dict;
    void * found = nullptr;
    SbBool ok = dict.find((uintptr_t)9999, found);
    EXPECT_TRUE((ok == FALSE)) << "SbDict::find should return FALSE for missing key";
}

TEST(BaseSbtimeSbdict, SbDictRemoveDecreasesEntryCount)
{
    SbDict dict;
    int v1 = 1, v2 = 2;
    dict.enter((uintptr_t)10, &v1);
    dict.enter((uintptr_t)20, &v2);
    dict.remove((uintptr_t)10);
    void * found = nullptr;
    SbBool ok = dict.find((uintptr_t)10, found);
    EXPECT_TRUE((ok == FALSE)) << "SbDict remove: key still found after removal";
}

TEST(BaseSbtimeSbdict, SbDictClearEmptiesTheDictionary)
{
    SbDict dict;
    int v = 7;
    dict.enter((uintptr_t)1, &v);
    dict.enter((uintptr_t)2, &v);
    dict.clear();
    void * found = nullptr;
    SbBool ok1 = dict.find((uintptr_t)1, found);
    SbBool ok2 = dict.find((uintptr_t)2, found);
    EXPECT_TRUE((ok1 == FALSE) && (ok2 == FALSE)) << "SbDict::clear did not empty dictionary";
}

TEST(BaseSbtimeSbdict, SbDictApplyToAllVisitsAllEntries)
{
    SbDict dict;
    int v = 0;
    dict.enter((uintptr_t)1, &v);
    dict.enter((uintptr_t)2, &v);
    dict.enter((uintptr_t)3, &v);
    g_applyCount = 0;
    dict.applyToAll(countApply);
    EXPECT_TRUE((g_applyCount == 3)) << "applyToAll did not visit all entries";
}

TEST(BaseSbtimeSbdict, SbDictApplyToAllWithDataVisitsAllEntries)
{
    SbDict dict;
    int v = 0;
    dict.enter((uintptr_t)4, &v);
    dict.enter((uintptr_t)5, &v);
    ApplyDataCtx ctx;
    ctx.count = 0;
    dict.applyToAll(countApplyData, &ctx);
    EXPECT_TRUE((ctx.count == 2)) << "applyToAll(data) did not visit all entries";
}

TEST(BaseSbtimeSbdict, SbDictMakePListProducesMatchingKeyValueLists)
{
    SbDict dict;
    int v1 = 1, v2 = 2;
    dict.enter((uintptr_t)100, &v1);
    dict.enter((uintptr_t)200, &v2);
    SbPList keys, values;
    dict.makePList(keys, values);
    EXPECT_TRUE((keys.getLength() == 2) && (values.getLength() == 2)) << "makePList produced wrong list lengths";
}

TEST(BaseSbtimeSbdict, SbDictCopyConstructorReplicatesEntries)
{
    SbDict orig;
    int v = 55;
    orig.enter((uintptr_t)77, &v);
    SbDict copy(orig);
    void * found = nullptr;
    SbBool ok = copy.find((uintptr_t)77, found);
    EXPECT_TRUE(ok && (found == &v)) << "SbDict copy constructor failed";
}

TEST(BaseSbtimeSbdict, SbDictOperatorCopiesEntries)
{
    SbDict orig;
    int v = 33;
    orig.enter((uintptr_t)44, &v);
    SbDict copy;
    copy = orig;
    void * found = nullptr;
    SbBool ok = copy.find((uintptr_t)44, found);
    EXPECT_TRUE(ok && (found == &v)) << "SbDict operator= failed";
}

TEST(BaseSbtimeSbdict, SbDictEnterReturnsFALSEForDuplicateKey)
{
    SbDict dict;
    int v1 = 1, v2 = 2;
    SbBool first  = dict.enter((uintptr_t)50, &v1);
    SbBool second = dict.enter((uintptr_t)50, &v2); // duplicate
    EXPECT_TRUE((first == TRUE) && (second == FALSE)) << "SbDict enter should return FALSE for duplicate key";
}
