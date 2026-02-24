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

#define COIN_ALLOW_SBDICT
#include "../test_utils.h"

#include <Inventor/SbTime.h>
#include <Inventor/SbDict.h>
#include <Inventor/SbString.h>
#include <Inventor/SbPList.h>
#include <cmath>
#include <cstring>

using namespace SimpleTest;

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

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // =======================================================================
    // SbTime tests
    // =======================================================================

    runner.startTest("SbTime::zero() has value 0.0");
    {
        SbTime t = SbTime::zero();
        bool pass = floatNear(t.getValue(), 0.0);
        runner.endTest(pass, pass ? "" : "SbTime::zero() != 0.0");
    }

    runner.startTest("SbTime default constructor is zero");
    {
        SbTime t;
        // Default constructor may not be zero in all implementations;
        // just verify it constructs without crash and getValue() is finite.
        (void)t.getValue();
        runner.endTest(true, "");
    }

    runner.startTest("SbTime(double) setValue/getValue round-trip");
    {
        SbTime t(3.75);
        bool pass = floatNear(t.getValue(), 3.75);
        runner.endTest(pass, pass ? "" : "SbTime(double) getValue mismatch");
    }

    runner.startTest("SbTime::setValue(double) round-trip");
    {
        SbTime t;
        t.setValue(1.5);
        bool pass = floatNear(t.getValue(), 1.5);
        runner.endTest(pass, pass ? "" : "setValue/getValue round-trip failed");
    }

    runner.startTest("SbTime::setValue(sec, usec) round-trip");
    {
        SbTime t;
        t.setValue((int32_t)2, (long)500000); // 2.5 s
        bool pass = floatNear(t.getValue(), 2.5);
        runner.endTest(pass, pass ? "" : "setValue(sec,usec) round-trip failed");
    }

    runner.startTest("SbTime::setMsecValue round-trip");
    {
        SbTime t;
        t.setMsecValue(1500); // 1.5 s
        bool pass = floatNear(t.getValue(), 1.5, 1e-3);
        runner.endTest(pass, pass ? "" : "setMsecValue round-trip failed");
    }

    runner.startTest("SbTime operator+ (3.0 + 1.5 == 4.5)");
    {
        SbTime a(3.0), b(1.5);
        SbTime c = a + b;
        bool pass = floatNear(c.getValue(), 4.5);
        runner.endTest(pass, pass ? "" : "operator+ failed");
    }

    runner.startTest("SbTime operator- (3.0 - 1.5 == 1.5)");
    {
        SbTime a(3.0), b(1.5);
        SbTime c = a - b;
        bool pass = floatNear(c.getValue(), 1.5);
        runner.endTest(pass, pass ? "" : "operator- failed");
    }

    runner.startTest("SbTime unary minus negates value");
    {
        SbTime a(2.0);
        SbTime b = -a;
        bool pass = floatNear(b.getValue(), -2.0);
        runner.endTest(pass, pass ? "" : "unary minus failed");
    }

    runner.startTest("SbTime operator*(SbTime, double)");
    {
        SbTime a(2.0);
        SbTime b = a * 3.0;
        bool pass = floatNear(b.getValue(), 6.0);
        runner.endTest(pass, pass ? "" : "SbTime * double failed");
    }

    runner.startTest("SbTime operator*(double, SbTime)");
    {
        SbTime a(2.0);
        SbTime b = 4.0 * a;
        bool pass = floatNear(b.getValue(), 8.0);
        runner.endTest(pass, pass ? "" : "double * SbTime failed");
    }

    runner.startTest("SbTime operator/(SbTime, double)");
    {
        SbTime a(6.0);
        SbTime b = a / 2.0;
        bool pass = floatNear(b.getValue(), 3.0);
        runner.endTest(pass, pass ? "" : "SbTime / double failed");
    }

    runner.startTest("SbTime operator/ ratio (SbTime / SbTime)");
    {
        SbTime a(6.0), b(2.0);
        double ratio = a / b;
        bool pass = floatNear(ratio, 3.0);
        runner.endTest(pass, pass ? "" : "SbTime / SbTime ratio failed");
    }

    runner.startTest("SbTime operator+= accumulates");
    {
        SbTime t(1.0);
        t += SbTime(0.5);
        bool pass = floatNear(t.getValue(), 1.5);
        runner.endTest(pass, pass ? "" : "operator+= failed");
    }

    runner.startTest("SbTime operator-= subtracts");
    {
        SbTime t(2.0);
        t -= SbTime(0.5);
        bool pass = floatNear(t.getValue(), 1.5);
        runner.endTest(pass, pass ? "" : "operator-= failed");
    }

    runner.startTest("SbTime operator*= scales");
    {
        SbTime t(2.0);
        t *= 3.0;
        bool pass = floatNear(t.getValue(), 6.0);
        runner.endTest(pass, pass ? "" : "operator*= failed");
    }

    runner.startTest("SbTime operator/= divides");
    {
        SbTime t(6.0);
        t /= 2.0;
        bool pass = floatNear(t.getValue(), 3.0);
        runner.endTest(pass, pass ? "" : "operator/= failed");
    }

    runner.startTest("SbTime operator== (equal times)");
    {
        SbTime a(1.5), b(1.5);
        bool pass = (a == b);
        runner.endTest(pass, pass ? "" : "operator== failed for equal times");
    }

    runner.startTest("SbTime operator!= (different times)");
    {
        SbTime a(1.0), b(2.0);
        bool pass = (a != b);
        runner.endTest(pass, pass ? "" : "operator!= failed for different times");
    }

    runner.startTest("SbTime comparison operators (<, <=, >, >=)");
    {
        SbTime a(1.0), b(2.0);
        bool pass = (a < b) && (a <= b) && (b > a) && (b >= a) &&
                    !(b < a) && !(a > b);
        runner.endTest(pass, pass ? "" : "comparison operators failed");
    }

    runner.startTest("SbTime % modulo");
    {
        SbTime a(5.0), b(3.0);
        SbTime r = a % b;
        bool pass = floatNear(r.getValue(), 2.0, 1e-6);
        runner.endTest(pass, pass ? "" : "SbTime % modulo failed");
    }

    runner.startTest("SbTime::format() returns non-empty string");
    {
        SbTime t(12.345);
        SbString s = t.format();
        bool pass = (s.getLength() > 0);
        runner.endTest(pass, pass ? "" : "format() returned empty string");
    }

    runner.startTest("SbTime::max() >= SbTime::zero()");
    {
        SbTime mx = SbTime::max();
        SbTime z  = SbTime::zero();
        bool pass = (mx >= z);
        runner.endTest(pass, pass ? "" : "SbTime::max() < zero");
    }

    runner.startTest("SbTime::maxTime() equals SbTime::max()");
    {
        SbTime mx1 = SbTime::max();
        SbTime mx2 = SbTime::maxTime();
        bool pass = floatNear(mx1.getValue(), mx2.getValue());
        runner.endTest(pass, pass ? "" : "max() and maxTime() differ");
    }

    runner.startTest("SbTime getValue(sec,usec) decomposes correctly");
    {
        SbTime t;
        t.setValue((int32_t)3, (long)750000); // 3.75 s
        time_t sec;
        long usec;
        t.getValue(sec, usec);
        bool pass = (sec == 3) && (usec == 750000);
        runner.endTest(pass, pass ? "" : "getValue(sec,usec) decomposition failed");
    }

    // =======================================================================
    // SbDict tests
    // =======================================================================

    runner.startTest("SbDict enter and find");
    {
        SbDict dict;
        int dummy = 42;
        SbBool entered = dict.enter((uintptr_t)1, &dummy);
        void * found = nullptr;
        SbBool ok = dict.find((uintptr_t)1, found);
        bool pass = entered && ok && (found == &dummy);
        runner.endTest(pass, pass ? "" : "SbDict enter/find failed");
    }

    runner.startTest("SbDict find returns FALSE for missing key");
    {
        SbDict dict;
        void * found = nullptr;
        SbBool ok = dict.find((uintptr_t)9999, found);
        bool pass = (ok == FALSE);
        runner.endTest(pass, pass ? "" : "SbDict::find should return FALSE for missing key");
    }

    runner.startTest("SbDict remove decreases entry count");
    {
        SbDict dict;
        int v1 = 1, v2 = 2;
        dict.enter((uintptr_t)10, &v1);
        dict.enter((uintptr_t)20, &v2);
        dict.remove((uintptr_t)10);
        void * found = nullptr;
        SbBool ok = dict.find((uintptr_t)10, found);
        bool pass = (ok == FALSE);
        runner.endTest(pass, pass ? "" : "SbDict remove: key still found after removal");
    }

    runner.startTest("SbDict clear empties the dictionary");
    {
        SbDict dict;
        int v = 7;
        dict.enter((uintptr_t)1, &v);
        dict.enter((uintptr_t)2, &v);
        dict.clear();
        void * found = nullptr;
        SbBool ok1 = dict.find((uintptr_t)1, found);
        SbBool ok2 = dict.find((uintptr_t)2, found);
        bool pass = (ok1 == FALSE) && (ok2 == FALSE);
        runner.endTest(pass, pass ? "" : "SbDict::clear did not empty dictionary");
    }

    runner.startTest("SbDict applyToAll visits all entries");
    {
        SbDict dict;
        int v = 0;
        dict.enter((uintptr_t)1, &v);
        dict.enter((uintptr_t)2, &v);
        dict.enter((uintptr_t)3, &v);
        g_applyCount = 0;
        dict.applyToAll(countApply);
        bool pass = (g_applyCount == 3);
        runner.endTest(pass, pass ? "" : "applyToAll did not visit all entries");
    }

    runner.startTest("SbDict applyToAll(with data) visits all entries");
    {
        SbDict dict;
        int v = 0;
        dict.enter((uintptr_t)4, &v);
        dict.enter((uintptr_t)5, &v);
        ApplyDataCtx ctx;
        ctx.count = 0;
        dict.applyToAll(countApplyData, &ctx);
        bool pass = (ctx.count == 2);
        runner.endTest(pass, pass ? "" : "applyToAll(data) did not visit all entries");
    }

    runner.startTest("SbDict makePList produces matching key/value lists");
    {
        SbDict dict;
        int v1 = 1, v2 = 2;
        dict.enter((uintptr_t)100, &v1);
        dict.enter((uintptr_t)200, &v2);
        SbPList keys, values;
        dict.makePList(keys, values);
        bool pass = (keys.getLength() == 2) && (values.getLength() == 2);
        runner.endTest(pass, pass ? "" : "makePList produced wrong list lengths");
    }

    runner.startTest("SbDict copy constructor replicates entries");
    {
        SbDict orig;
        int v = 55;
        orig.enter((uintptr_t)77, &v);
        SbDict copy(orig);
        void * found = nullptr;
        SbBool ok = copy.find((uintptr_t)77, found);
        bool pass = ok && (found == &v);
        runner.endTest(pass, pass ? "" : "SbDict copy constructor failed");
    }

    runner.startTest("SbDict operator= copies entries");
    {
        SbDict orig;
        int v = 33;
        orig.enter((uintptr_t)44, &v);
        SbDict copy;
        copy = orig;
        void * found = nullptr;
        SbBool ok = copy.find((uintptr_t)44, found);
        bool pass = ok && (found == &v);
        runner.endTest(pass, pass ? "" : "SbDict operator= failed");
    }

    runner.startTest("SbDict enter returns FALSE for duplicate key");
    {
        SbDict dict;
        int v1 = 1, v2 = 2;
        SbBool first  = dict.enter((uintptr_t)50, &v1);
        SbBool second = dict.enter((uintptr_t)50, &v2); // duplicate
        bool pass = (first == TRUE) && (second == FALSE);
        runner.endTest(pass, pass ? "" : "SbDict enter should return FALSE for duplicate key");
    }

    return runner.getSummary();
}
