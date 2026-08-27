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
#include <Inventor/annex/Profiler/elements/SoProfilerElement.h>
#include <Inventor/SbTime.h>
#include <Inventor/SoType.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/actions/SoGLRenderAction.h>

#include "profiler/SoProfilerP.h"

#include <atomic>
#include <thread>

using namespace ObolTest;

namespace {

class ProfilerGLRenderActionAccess : public SoGLRenderAction {
public:
    static const SoEnabledElementsList & enabledElements()
    {
        return *getClassEnabledElements();
    }
};

} // namespace

TEST(ProfilerSuite, SoProfilerInitDoesNotCrash)
{
    SoProfiler::init();
    const bool pass = (SoProfiler::isEnabled() == FALSE);
    EXPECT_TRUE(pass) << "SoProfiler::init unexpectedly enabled runtime profiling";
}

// -----------------------------------------------------------------------
// SoProfilerStats class type ID is valid (requires SoProfiler::init first)
// -----------------------------------------------------------------------

TEST(ProfilerSuite, SoProfilerStatsGetClassTypeIdIsNotBadType)
{
    bool pass = (SoProfilerStats::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoProfilerStats has bad class type";
}

// -----------------------------------------------------------------------
// isEnabled returns FALSE before any explicit enable call
// -----------------------------------------------------------------------

TEST(ProfilerSuite, SoProfilerIsEnabledReturnsFALSEBeforeEnable)
{
    bool pass = (SoProfiler::isEnabled() == FALSE);
    EXPECT_TRUE(pass) << "SoProfiler::isEnabled should be FALSE before enable";
}

// -----------------------------------------------------------------------
// enable(TRUE) / isEnabled
// -----------------------------------------------------------------------

TEST(ProfilerSuite, SoProfilerEnableTRUEMakesIsEnabledReturnTRUE)
{
    SoProfiler::enable(TRUE);
    // Profiling is an optional compile-time feature.  In a normal
    // library build enable(TRUE) is intentionally a no-op from the
    // public isEnabled() contract's perspective.
#ifdef OBOL_PROFILING
    bool pass = (SoProfiler::isEnabled() == TRUE);
#else
    bool pass = (SoProfiler::isEnabled() == FALSE);
#endif
    SoProfiler::enable(FALSE); // restore
    EXPECT_TRUE(pass) << "SoProfiler::enable(TRUE) did not match the build configuration";
}

// -----------------------------------------------------------------------
// enable(FALSE) / isEnabled
// -----------------------------------------------------------------------

TEST(ProfilerSuite, SoProfilerEnableFALSEMakesIsEnabledReturnFALSE)
{
    SoProfiler::enable(TRUE);
    SoProfiler::enable(FALSE);
    bool pass = (SoProfiler::isEnabled() == FALSE);
    EXPECT_TRUE(pass) << "SoProfiler::isEnabled did not return FALSE after enable(FALSE)";
}

// -----------------------------------------------------------------------
// isOverlayActive returns FALSE when no overlay is configured
// -----------------------------------------------------------------------

TEST(ProfilerSuite, SoProfilerIsOverlayActiveReturnsFALSEWithNoOverlay)
{
    bool pass = (SoProfiler::isOverlayActive() == FALSE);
    EXPECT_TRUE(pass) << "SoProfiler::isOverlayActive should be FALSE when no overlay configured";
}

// -----------------------------------------------------------------------
// isConsoleActive returns FALSE when not configured
// -----------------------------------------------------------------------

TEST(ProfilerSuite, SoProfilerIsConsoleActiveReturnsFALSEWhenNotConfigured)
{
    bool pass = (SoProfiler::isConsoleActive() == FALSE);
    EXPECT_TRUE(pass) << "SoProfiler::isConsoleActive should be FALSE when not configured";
}

// -----------------------------------------------------------------------
// SbProfilingData default construction does not crash
// -----------------------------------------------------------------------

TEST(ProfilerSuite, SbProfilingDataDefaultConstructionDoesNotCrash)
{
    SbProfilingData data;
    SUCCEED();
}

// -----------------------------------------------------------------------
// setActionType / getActionType round-trip
// -----------------------------------------------------------------------

TEST(ProfilerSuite, SbProfilingDataSetActionTypeGetActionTypeRoundTrip)
{
    SbProfilingData data;
    SoType actionType = SoGetBoundingBoxAction::getClassTypeId();
    data.setActionType(actionType);
    bool pass = (data.getActionType() == actionType);
    EXPECT_TRUE(pass) << "getActionType did not return the type set by setActionType";
}

// -----------------------------------------------------------------------
// setActionStartTime / setActionStopTime / getActionDuration
// -----------------------------------------------------------------------

TEST(ProfilerSuite, SbProfilingDataTimingDuration04sForStart01Stop05)
{
    SbProfilingData data;
    data.setActionStartTime(SbTime(0.1));
    data.setActionStopTime(SbTime(0.5));
    double duration = data.getActionDuration().getValue();
    // Allow a small floating-point tolerance
    bool pass = (duration > 0.399 && duration < 0.401);
    EXPECT_TRUE(pass) << "getActionDuration did not return ~0.4s for start=0.1, stop=0.5";
}

// -----------------------------------------------------------------------
// Copy constructor preserves action type
// -----------------------------------------------------------------------

TEST(ProfilerSuite, SbProfilingDataCopyConstructorPreservesActionType)
{
    SbProfilingData original;
    SoType actionType = SoGetBoundingBoxAction::getClassTypeId();
    original.setActionType(actionType);

    SbProfilingData copy(original);
    bool pass = (copy.getActionType() == actionType);
    EXPECT_TRUE(pass) << "Copy constructor did not preserve the action type";
}

// -----------------------------------------------------------------------
// operator+= on two SbProfilingData objects (both with stop times set)
// -----------------------------------------------------------------------

TEST(ProfilerSuite, SbProfilingDataOperatorDoesNotCrashWithTwoTimedObjects)
{
    SbProfilingData lhs;
    lhs.setActionStartTime(SbTime(0.0));
    lhs.setActionStopTime(SbTime(0.2));

    SbProfilingData rhs;
    rhs.setActionStartTime(SbTime(0.0));
    rhs.setActionStopTime(SbTime(0.3));

    lhs += rhs;
    SUCCEED();
}

// -----------------------------------------------------------------------
// SoProfiler enable + isEnabled + enable(FALSE) round-trip (after init)
// -----------------------------------------------------------------------

TEST(ProfilerSuite, SoProfilerEnableDisableRoundTripAfterInit)
{
    // Verify that runtime control remains usable after initialization.
    SbBool originalState = SoProfiler::isEnabled();
    SoProfiler::enable(FALSE);
    bool disabledOk = (SoProfiler::isEnabled() == FALSE);
    SoProfiler::enable(TRUE);
#ifdef OBOL_PROFILING
    bool enabledOk = (SoProfiler::isEnabled() == TRUE);
#else
    bool enabledOk = (SoProfiler::isEnabled() == FALSE);
#endif
    // Restore original state
    SoProfiler::enable(originalState);
    bool pass = disabledOk && enabledOk;
    EXPECT_TRUE(pass) << "enable/disable round-trip failed after SoProfiler::init()";
}

// -----------------------------------------------------------------------
// SoProfilerElement is registered for SoGLRenderAction after initialization
// -----------------------------------------------------------------------

TEST(ProfilerSuite, SoProfilerElementIsRegisteredForSoGLRenderAction)
{
    // SoProfilerStats::initClass() calls SO_ENABLE(SoGLRenderAction, SoProfilerElement)
    // so after SoProfiler::init(), the element must appear in that action's
    // class metadata even while collection is disabled.
    SoProfiler::enable(FALSE);
    const SoTypeList & elements =
        ProfilerGLRenderActionAccess::enabledElements().getElements();
    const int stackIndex = SoProfilerElement::getClassStackIndex();
    const bool pass = stackIndex >= 0 && stackIndex < elements.getLength() &&
        elements[stackIndex] == SoProfilerElement::getClassTypeId();
    EXPECT_TRUE(pass) << "SoProfilerElement not registered for SoGLRenderAction after profiler init";
}

// -----------------------------------------------------------------------
// Internal traversal pauses are local to the current thread
// -----------------------------------------------------------------------

TEST(ProfilerSuite, ProfilerTraversalPauseIsScopedAndThreadLocal)
{
    SoProfiler::enable(TRUE);
    std::atomic<bool> otherThreadEnabled{false};
    bool pausedHere = false;
    {
        SoProfilerP::ScopedPause pause;
        pausedHere = (SoProfiler::isEnabled() == FALSE);
        std::thread observer([&] {
#ifdef OBOL_PROFILING
            otherThreadEnabled.store(SoProfiler::isEnabled() == TRUE,
                                     std::memory_order_release);
#else
            otherThreadEnabled.store(SoProfiler::isEnabled() == FALSE,
                                     std::memory_order_release);
#endif
        });
        observer.join();
    }
#ifdef OBOL_PROFILING
    const bool restored = (SoProfiler::isEnabled() == TRUE);
#else
    const bool restored = (SoProfiler::isEnabled() == FALSE);
#endif
    SoProfiler::enable(FALSE);
    const bool pass = pausedHere && restored &&
        otherThreadEnabled.load(std::memory_order_acquire);
    EXPECT_TRUE(pass) << "profiler pause leaked across threads or outlived its scope";
}

// -----------------------------------------------------------------------
// SbProfilingData::reset clears action type
// -----------------------------------------------------------------------

TEST(ProfilerSuite, SbProfilingDataResetClearsActionType)
{
    SbProfilingData data;
    data.setActionType(SoGetBoundingBoxAction::getClassTypeId());
    data.reset();
    bool pass = (data.getActionType() == SoType::badType());
    EXPECT_TRUE(pass) << "reset() did not clear the action type";
}

// -----------------------------------------------------------------------
// SbProfilingData setActionStartTime/StopTime survive reset
// -----------------------------------------------------------------------

TEST(ProfilerSuite, SbProfilingDataTimingResetClearsDuration)
{
    SbProfilingData data;
    data.setActionStartTime(SbTime(1.0));
    data.setActionStopTime(SbTime(2.0));
    data.reset();
    // After reset, both start and stop times are SbTime::zero(), so duration == 0.0
    SbTime duration = data.getActionDuration();
    bool pass = (duration.getValue() == 0.0);
    EXPECT_TRUE(pass) << "reset() did not clear timing data to zero";
}
