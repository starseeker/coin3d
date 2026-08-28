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
 * @file test_sensors_deeper.cpp
 * @brief Deeper sensor API tests (sensors/ 73.3 %).
 *
 * Covers:
 *   SoSensor base:
 *     setFunction/getFunction, setData/getData
 *   SoFieldSensor:
 *     attach/detach, getAttachedField, callback fires on field change
 *   SoNodeSensor:
 *     attach/detach, getAttachedNode, callback fires on node change
 *   SoTimerSensor:
 *     setInterval/getInterval, setBaseTime/getBaseTime, isScheduled
 *   SoAlarmSensor:
 *     setTime/getTime, setTimeFromNow, isScheduled
 *   SoOneShotSensor:
 *     schedule, isScheduled, unschedule
 *   SoIdleSensor:
 *     schedule, isScheduled, unschedule
 *   SoDataSensor:
 *     setTriggerPathFlag, getTriggerPathFlag
 */

#include "../test_utils.h"

#include <Inventor/sensors/SoSensor.h>
#include <Inventor/sensors/SoFieldSensor.h>
#include <Inventor/sensors/SoNodeSensor.h>
#include <Inventor/sensors/SoTimerSensor.h>
#include <Inventor/sensors/SoAlarmSensor.h>
#include <Inventor/sensors/SoOneShotSensor.h>
#include <Inventor/sensors/SoIdleSensor.h>
#include <Inventor/sensors/SoDataSensor.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/fields/SoSFFloat.h>
#include <Inventor/SbTime.h>
#include <Inventor/SoDB.h>

#include <cmath>
#include <cstdio>

using namespace ObolTest;

// Simple callback that increments a counter
static void countCB(void * data, SoSensor *)
{
    int * count = static_cast<int *>(data);
    (*count)++;
}

TEST(SensorsDeeper, SoFieldSensorSetFunctionGetFunctionRoundTrip)
{
    SoFieldSensor sensor;
    sensor.setFunction(countCB);
    EXPECT_TRUE((sensor.getFunction() == countCB)) << "SoSensor setFunction/getFunction failed";
}

TEST(SensorsDeeper, SoFieldSensorSetDataGetDataRoundTrip)
{
    SoFieldSensor sensor;
    int dummy = 42;
    sensor.setData(&dummy);
    EXPECT_TRUE((sensor.getData() == &dummy)) << "SoSensor setData/getData failed";
}

// -----------------------------------------------------------------------
// SoFieldSensor
// -----------------------------------------------------------------------

TEST(SensorsDeeper, SoFieldSensorAttachGetAttachedField)
{
    SoCube * cube = new SoCube;
    cube->ref();
    SoFieldSensor sensor;
    sensor.attach(&cube->width);
    SoField * attached = sensor.getAttachedField();
    EXPECT_TRUE((attached == &cube->width)) << "SoFieldSensor getAttachedField returned wrong field";
    sensor.detach();
    cube->unref();
}

TEST(SensorsDeeper, SoFieldSensorDetachSetsGetAttachedFieldToNULL)
{
    SoCube * cube = new SoCube;
    cube->ref();
    SoFieldSensor sensor;
    sensor.attach(&cube->width);
    sensor.detach();
    EXPECT_TRUE((sensor.getAttachedField() == nullptr)) << "After detach, getAttachedField should be NULL";
    cube->unref();
}

TEST(SensorsDeeper, SoFieldSensorFiresCallbackOnFieldChange)
{
    SoCube * cube = new SoCube;
    cube->ref();
    int callCount = 0;
    SoFieldSensor sensor(countCB, &callCount);
    sensor.attach(&cube->width);
    cube->width.setValue(5.0f);
    // The sensor queues a callback; process the sensor queue
    SoDB::getSensorManager()->processDelayQueue(FALSE);
    EXPECT_TRUE((callCount >= 1)) << "SoFieldSensor callback did not fire on field change";
    sensor.detach();
    cube->unref();
}

// -----------------------------------------------------------------------
// SoNodeSensor
// -----------------------------------------------------------------------

TEST(SensorsDeeper, SoNodeSensorAttachGetAttachedNode)
{
    SoCube * cube = new SoCube;
    cube->ref();
    SoNodeSensor sensor;
    sensor.attach(cube);
    SoNode * attached = sensor.getAttachedNode();
    EXPECT_TRUE((attached == cube)) << "SoNodeSensor getAttachedNode returned wrong node";
    sensor.detach();
    cube->unref();
}

TEST(SensorsDeeper, SoNodeSensorDetachSetsGetAttachedNodeToNULL)
{
    SoCube * cube = new SoCube;
    cube->ref();
    SoNodeSensor sensor;
    sensor.attach(cube);
    sensor.detach();
    EXPECT_TRUE((sensor.getAttachedNode() == nullptr)) << "After detach, getAttachedNode should be NULL";
    cube->unref();
}

TEST(SensorsDeeper, SoNodeSensorFiresCallbackOnNodeFieldChange)
{
    SoCube * cube = new SoCube;
    cube->ref();
    int callCount = 0;
    SoNodeSensor sensor(countCB, &callCount);
    sensor.attach(cube);
    cube->height.setValue(3.0f);
    SoDB::getSensorManager()->processDelayQueue(FALSE);
    EXPECT_TRUE((callCount >= 1)) << "SoNodeSensor callback did not fire on node change";
    sensor.detach();
    cube->unref();
}

// -----------------------------------------------------------------------
// SoTimerSensor
// -----------------------------------------------------------------------

TEST(SensorsDeeper, SoTimerSensorSetIntervalGetIntervalRoundTrip)
{
    SoTimerSensor sensor;
    SbTime interval(0.5);
    sensor.setInterval(interval);
    EXPECT_TRUE((std::fabs(sensor.getInterval().getValue() - 0.5) < 1e-9)) << "SoTimerSensor setInterval/getInterval failed";
}

TEST(SensorsDeeper, SoTimerSensorSetBaseTimeGetBaseTimeRoundTrip)
{
    SoTimerSensor sensor;
    SbTime base(1.0);
    sensor.setBaseTime(base);
    EXPECT_TRUE((std::fabs(sensor.getBaseTime().getValue() - 1.0) < 1e-9)) << "SoTimerSensor setBaseTime/getBaseTime failed";
}

TEST(SensorsDeeper, SoTimerSensorIsScheduledFALSEBeforeSchedule)
{
    SoTimerSensor sensor;
    EXPECT_TRUE(!sensor.isScheduled()) << "SoTimerSensor should not be scheduled initially";
}

TEST(SensorsDeeper, SoTimerSensorIsScheduledTRUEAfterSchedule)
{
    SoTimerSensor sensor;
    sensor.setInterval(SbTime(1.0));
    sensor.schedule();
    EXPECT_TRUE(sensor.isScheduled()) << "SoTimerSensor should be scheduled after schedule()";
    sensor.unschedule();
}

// -----------------------------------------------------------------------
// SoAlarmSensor
// -----------------------------------------------------------------------

TEST(SensorsDeeper, SoAlarmSensorSetTimeGetTimeRoundTrip)
{
    SoAlarmSensor sensor;
    SbTime t(10.0);
    sensor.setTime(t);
    EXPECT_TRUE((std::fabs(sensor.getTime().getValue() - 10.0) < 1e-9)) << "SoAlarmSensor setTime/getTime failed";
}

TEST(SensorsDeeper, SoAlarmSensorIsScheduledFALSEBeforeSchedule)
{
    SoAlarmSensor sensor;
    EXPECT_TRUE(!sensor.isScheduled()) << "SoAlarmSensor should not be scheduled initially";
}

TEST(SensorsDeeper, SoAlarmSensorSetTimeFromNowScheduleUnschedule)
{
    SoAlarmSensor sensor;
    sensor.setTimeFromNow(SbTime(100.0));
    sensor.schedule();
    bool isScheduled = sensor.isScheduled();
    sensor.unschedule();
    EXPECT_TRUE(isScheduled) << "SoAlarmSensor should be scheduled after schedule()";
}

// -----------------------------------------------------------------------
// SoOneShotSensor
// -----------------------------------------------------------------------

TEST(SensorsDeeper, SoOneShotSensorIsScheduledFALSEBeforeSchedule)
{
    SoOneShotSensor sensor;
    EXPECT_TRUE(!sensor.isScheduled()) << "SoOneShotSensor should not be scheduled initially";
}

TEST(SensorsDeeper, SoOneShotSensorIsScheduledTRUEAfterSchedule)
{
    SoOneShotSensor sensor;
    sensor.schedule();
    EXPECT_TRUE(sensor.isScheduled()) << "SoOneShotSensor should be scheduled after schedule()";
    sensor.unschedule();
}

// -----------------------------------------------------------------------
// SoIdleSensor
// -----------------------------------------------------------------------

TEST(SensorsDeeper, SoIdleSensorIsScheduledFALSEBeforeSchedule)
{
    SoIdleSensor sensor;
    EXPECT_TRUE(!sensor.isScheduled()) << "SoIdleSensor should not be scheduled initially";
}

TEST(SensorsDeeper, SoIdleSensorIsScheduledTRUEAfterSchedule)
{
    SoIdleSensor sensor;
    sensor.schedule();
    EXPECT_TRUE(sensor.isScheduled()) << "SoIdleSensor should be scheduled after schedule()";
    sensor.unschedule();
}

// -----------------------------------------------------------------------
// SoDataSensor: setTriggerPathFlag
// -----------------------------------------------------------------------

TEST(SensorsDeeper, SoDataSensorSetTriggerPathFlagTRUEGetTriggerPathFlag)
{
    SoFieldSensor sensor; // SoFieldSensor is a SoDataSensor
    sensor.setTriggerPathFlag(TRUE);
    EXPECT_TRUE((sensor.getTriggerPathFlag() == TRUE)) << "setTriggerPathFlag(TRUE) failed";
}

TEST(SensorsDeeper, SoDataSensorSetTriggerPathFlagFALSEGetTriggerPathFlag)
{
    SoFieldSensor sensor;
    sensor.setTriggerPathFlag(FALSE);
    EXPECT_TRUE((sensor.getTriggerPathFlag() == FALSE)) << "setTriggerPathFlag(FALSE) failed";
}
