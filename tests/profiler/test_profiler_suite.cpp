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
 * @file test_profiler_suite.cpp
 * @brief Tests for Coin3D profiler subsystem.
 *
 * Covers:
 *   SoProfiler      - init, enable/disable, isEnabled, isOverlayActive, isConsoleActive
 *   SbProfilingData - construction, setActionType/getActionType, timing, copy constructor,
 *                     operator+=
 *   SoProfilerStats - getClassTypeId
 */

#include "../test_utils.h"

#include <Inventor/annex/Profiler/SoProfiler.h>
#include <Inventor/annex/Profiler/SbProfilingData.h>
#include <Inventor/annex/Profiler/nodes/SoProfilerStats.h>
#include <Inventor/SbTime.h>
#include <Inventor/SoType.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>

using namespace SimpleTest;

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // -----------------------------------------------------------------------
    // SoProfiler::init can be called without crashing
    // -----------------------------------------------------------------------
    runner.startTest("SoProfiler::init does not crash");
    {
        SoProfiler::init();
        runner.endTest(true);
    }

    // -----------------------------------------------------------------------
    // SoProfilerStats class type ID is valid (requires SoProfiler::init first)
    // -----------------------------------------------------------------------
    runner.startTest("SoProfilerStats::getClassTypeId is not badType");
    {
        bool pass = (SoProfilerStats::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoProfilerStats has bad class type");
    }

    // -----------------------------------------------------------------------
    // isEnabled returns FALSE before any explicit enable call
    // -----------------------------------------------------------------------
    runner.startTest("SoProfiler::isEnabled returns FALSE before enable");
    {
        // Ensure profiler is disabled (init() alone does not enable it)
        SoProfiler::enable(FALSE);
        bool pass = (SoProfiler::isEnabled() == FALSE);
        runner.endTest(pass, pass ? "" : "SoProfiler::isEnabled should be FALSE before enable");
    }

    // -----------------------------------------------------------------------
    // enable(TRUE) / isEnabled
    // -----------------------------------------------------------------------
    runner.startTest("SoProfiler::enable(TRUE) makes isEnabled return TRUE");
    {
        SoProfiler::enable(TRUE);
        bool pass = (SoProfiler::isEnabled() == TRUE);
        SoProfiler::enable(FALSE); // restore
        runner.endTest(pass, pass ? "" : "SoProfiler::isEnabled did not return TRUE after enable(TRUE)");
    }

    // -----------------------------------------------------------------------
    // enable(FALSE) / isEnabled
    // -----------------------------------------------------------------------
    runner.startTest("SoProfiler::enable(FALSE) makes isEnabled return FALSE");
    {
        SoProfiler::enable(TRUE);
        SoProfiler::enable(FALSE);
        bool pass = (SoProfiler::isEnabled() == FALSE);
        runner.endTest(pass, pass ? "" : "SoProfiler::isEnabled did not return FALSE after enable(FALSE)");
    }

    // -----------------------------------------------------------------------
    // isOverlayActive returns FALSE when no overlay is configured
    // -----------------------------------------------------------------------
    runner.startTest("SoProfiler::isOverlayActive returns FALSE with no overlay");
    {
        bool pass = (SoProfiler::isOverlayActive() == FALSE);
        runner.endTest(pass, pass ? "" : "SoProfiler::isOverlayActive should be FALSE when no overlay configured");
    }

    // -----------------------------------------------------------------------
    // isConsoleActive returns FALSE when not configured
    // -----------------------------------------------------------------------
    runner.startTest("SoProfiler::isConsoleActive returns FALSE when not configured");
    {
        bool pass = (SoProfiler::isConsoleActive() == FALSE);
        runner.endTest(pass, pass ? "" : "SoProfiler::isConsoleActive should be FALSE when not configured");
    }

    // -----------------------------------------------------------------------
    // SbProfilingData default construction does not crash
    // -----------------------------------------------------------------------
    runner.startTest("SbProfilingData default construction does not crash");
    {
        SbProfilingData data;
        runner.endTest(true);
    }

    // -----------------------------------------------------------------------
    // setActionType / getActionType round-trip
    // -----------------------------------------------------------------------
    runner.startTest("SbProfilingData::setActionType/getActionType round-trip");
    {
        SbProfilingData data;
        SoType actionType = SoGetBoundingBoxAction::getClassTypeId();
        data.setActionType(actionType);
        bool pass = (data.getActionType() == actionType);
        runner.endTest(pass, pass ? "" : "getActionType did not return the type set by setActionType");
    }

    // -----------------------------------------------------------------------
    // setActionStartTime / setActionStopTime / getActionDuration
    // -----------------------------------------------------------------------
    runner.startTest("SbProfilingData timing: duration ~0.4s for start=0.1 stop=0.5");
    {
        SbProfilingData data;
        data.setActionStartTime(SbTime(0.1));
        data.setActionStopTime(SbTime(0.5));
        double duration = data.getActionDuration().getValue();
        // Allow a small floating-point tolerance
        bool pass = (duration > 0.399 && duration < 0.401);
        runner.endTest(pass, pass ? "" :
            "getActionDuration did not return ~0.4s for start=0.1, stop=0.5");
    }

    // -----------------------------------------------------------------------
    // Copy constructor preserves action type
    // -----------------------------------------------------------------------
    runner.startTest("SbProfilingData copy constructor preserves action type");
    {
        SbProfilingData original;
        SoType actionType = SoGetBoundingBoxAction::getClassTypeId();
        original.setActionType(actionType);

        SbProfilingData copy(original);
        bool pass = (copy.getActionType() == actionType);
        runner.endTest(pass, pass ? "" :
            "Copy constructor did not preserve the action type");
    }

    // -----------------------------------------------------------------------
    // operator+= on two SbProfilingData objects (both with stop times set)
    // -----------------------------------------------------------------------
    runner.startTest("SbProfilingData::operator+= does not crash with two timed objects");
    {
        SbProfilingData lhs;
        lhs.setActionStartTime(SbTime(0.0));
        lhs.setActionStopTime(SbTime(0.2));

        SbProfilingData rhs;
        rhs.setActionStartTime(SbTime(0.0));
        rhs.setActionStopTime(SbTime(0.3));

        lhs += rhs;
        runner.endTest(true);
    }

    return runner.getSummary();
}
