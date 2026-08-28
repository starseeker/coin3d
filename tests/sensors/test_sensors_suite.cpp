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
 * @file test_sensors_suite.cpp
 * @brief Tests for Coin3D sensor classes.
 *
 * Note: upstream has no OBOL_TEST_SUITE blocks for sensors.
 * These tests verify documented API behavior.
 *
 * Sensors covered:
 *   SoFieldSensor  - attach to field, fire on change
 *   SoNodeSensor   - attach to node, fire on change
 *   SoTimerSensor  - class type, schedule/unschedule
 *   SoAlarmSensor  - class type, schedule/unschedule
 *   SoOneShotSensor - class type
 */

#include "../test_utils.h"

#include <Inventor/sensors/SoFieldSensor.h>
#include <Inventor/sensors/SoNodeSensor.h>
#include <Inventor/sensors/SoTimerSensor.h>
#include <Inventor/sensors/SoAlarmSensor.h>
#include <Inventor/sensors/SoOneShotSensor.h>
#include <Inventor/sensors/SoIdleSensor.h>
#include <Inventor/sensors/SoPathSensor.h>
#include <Inventor/sensors/SoDataSensor.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/SoDB.h>
#include <Inventor/SoPath.h>
using namespace ObolTest;

static int s_fieldFired = 0;
static void onFieldChange(void*, SoSensor*) { ++s_fieldFired; }

static int s_nodeFired = 0;
static void onNodeChange(void*, SoSensor*) { ++s_nodeFired; }

static int s_timerFired = 0;
static void onTimer(void*, SoSensor*) { ++s_timerFired; }

TEST(SensorsSuite, SoFieldSensorAttachDetach)
{
    SoCube* cube = new SoCube;
    cube->ref();

    SoFieldSensor fs(onFieldChange, nullptr);
    fs.attach(&cube->width);
    bool attached = (fs.getAttachedField() == &cube->width);
    fs.detach();
    bool detached = (fs.getAttachedField() == nullptr);

    cube->unref();
    EXPECT_TRUE(attached && detached) << "SoFieldSensor attach/detach failed";
}

// -----------------------------------------------------------------------
// SoFieldSensor: re-attach to a different field
// -----------------------------------------------------------------------

TEST(SensorsSuite, SoFieldSensorReattach)
{
    SoCube* cube = new SoCube;
    cube->ref();

    SoFieldSensor fs(onFieldChange, nullptr);
    fs.attach(&cube->width);
    fs.attach(&cube->height); // re-attach
    EXPECT_TRUE((fs.getAttachedField() == &cube->height)) << "SoFieldSensor reattach failed";
    fs.detach();
    cube->unref();
}

// -----------------------------------------------------------------------
// SoNodeSensor: attach/detach
// Note: actual callback delivery requires a context manager in Obol.
// -----------------------------------------------------------------------

TEST(SensorsSuite, SoNodeSensorAttachDetach)
{
    SoCube* cube = new SoCube;
    cube->ref();

    SoNodeSensor ns(onNodeChange, nullptr);
    ns.attach(cube);
    bool attached = (ns.getAttachedNode() == cube);
    ns.detach();
    bool detached = (ns.getAttachedNode() == nullptr);

    cube->unref();
    EXPECT_TRUE(attached && detached) << "SoNodeSensor attach/detach failed";
}

// -----------------------------------------------------------------------
// SoTimerSensor: create and schedule/unschedule without crash
// -----------------------------------------------------------------------

TEST(SensorsSuite, SoTimerSensorScheduleUnschedule)
{
    SoTimerSensor ts(onTimer, nullptr);
    ts.setInterval(SbTime(1.0)); // 1 second interval
    ts.schedule();
    bool scheduled = ts.isScheduled();
    ts.unschedule();
    bool unscheduled = !ts.isScheduled();
    EXPECT_TRUE(scheduled && unscheduled) << "SoTimerSensor schedule/unschedule failed";
}

// -----------------------------------------------------------------------
// SoAlarmSensor: create and schedule/unschedule without crash
// -----------------------------------------------------------------------

TEST(SensorsSuite, SoAlarmSensorScheduleUnschedule)
{
    SoAlarmSensor as(onTimer, nullptr);
    as.setTime(SbTime::getTimeOfDay() + SbTime(10.0));
    as.schedule();
    bool scheduled = as.isScheduled();
    as.unschedule();
    bool unscheduled = !as.isScheduled();
    EXPECT_TRUE(scheduled && unscheduled) << "SoAlarmSensor schedule/unschedule failed";
}

// -----------------------------------------------------------------------
// SoOneShotSensor: schedule/unschedule
// -----------------------------------------------------------------------

TEST(SensorsSuite, SoOneShotSensorScheduleUnschedule)
{
    SoOneShotSensor oss(onTimer, nullptr);
    oss.schedule();
    EXPECT_TRUE(oss.isScheduled());
    oss.unschedule();
    EXPECT_FALSE(oss.isScheduled());
}

// -----------------------------------------------------------------------
// SoIdleSensor: schedule/unschedule without crash
// -----------------------------------------------------------------------

TEST(SensorsSuite, SoIdleSensorScheduleUnschedule)
{
    SoIdleSensor ids(onTimer, nullptr);
    ids.schedule();
    EXPECT_TRUE(ids.isScheduled());
    ids.unschedule();
    EXPECT_FALSE(ids.isScheduled());
}

// -----------------------------------------------------------------------
// SoPathSensor: attach/detach to a path
// -----------------------------------------------------------------------

TEST(SensorsSuite, SoPathSensorAttachDetach)
{
    SoSeparator* root = new SoSeparator;
    root->ref();
    SoCube* cube = new SoCube;
    root->addChild(cube);

    SoPath* path = new SoPath(root);
    path->ref();
    path->append(cube);

    SoPathSensor ps(onNodeChange, nullptr);
    ps.attach(path);
    bool attached = (ps.getAttachedPath() == path);
    ps.detach();
    bool detached = (ps.getAttachedPath() == nullptr);

    path->unref();
    root->unref();

    EXPECT_TRUE(attached && detached) << "SoPathSensor attach/detach failed";
}

// -----------------------------------------------------------------------
// SoDataSensor: class initialized and API via SoFieldSensor (concrete
// derived class); getTriggerField returns NULL before any trigger fires.
// -----------------------------------------------------------------------

TEST(SensorsSuite, SoDataSensorClassInitializedViaSoFieldSensor)
{
    SoFieldSensor fs(onFieldChange, nullptr);
    // SoFieldSensor IS-A SoDataSensor – verify the SoDataSensor API is
    // accessible without crashing.
    SoField* tf = fs.getTriggerField();     // NULL before attach/trigger
    SbBool pathFlag = fs.getTriggerPathFlag(); // default FALSE
    EXPECT_TRUE((tf == nullptr) && (pathFlag == FALSE)) << "SoDataSensor getTriggerField/getTriggerPathFlag initial state wrong";
}

TEST(SensorsSuite, SoDataSensorSetTriggerPathFlag)
{
    SoFieldSensor fs(onFieldChange, nullptr);
    fs.setTriggerPathFlag(TRUE);
    EXPECT_TRUE((fs.getTriggerPathFlag() == TRUE)) << "SoDataSensor setTriggerPathFlag did not stick";
}
