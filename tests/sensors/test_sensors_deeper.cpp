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

using namespace SimpleTest;

// Simple callback that increments a counter
static void countCB(void * data, SoSensor *)
{
    int * count = static_cast<int *>(data);
    (*count)++;
}

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // -----------------------------------------------------------------------
    // SoSensor base: setFunction/getFunction, setData/getData
    // -----------------------------------------------------------------------
    runner.startTest("SoFieldSensor setFunction / getFunction round-trip");
    {
        SoFieldSensor sensor;
        sensor.setFunction(countCB);
        bool pass = (sensor.getFunction() == countCB);
        runner.endTest(pass, pass ? "" : "SoSensor setFunction/getFunction failed");
    }

    runner.startTest("SoFieldSensor setData / getData round-trip");
    {
        SoFieldSensor sensor;
        int dummy = 42;
        sensor.setData(&dummy);
        bool pass = (sensor.getData() == &dummy);
        runner.endTest(pass, pass ? "" : "SoSensor setData/getData failed");
    }

    // -----------------------------------------------------------------------
    // SoFieldSensor
    // -----------------------------------------------------------------------
    runner.startTest("SoFieldSensor attach / getAttachedField");
    {
        SoCube * cube = new SoCube;
        cube->ref();
        SoFieldSensor sensor;
        sensor.attach(&cube->width);
        SoField * attached = sensor.getAttachedField();
        bool pass = (attached == &cube->width);
        sensor.detach();
        cube->unref();
        runner.endTest(pass, pass ? "" : "SoFieldSensor getAttachedField returned wrong field");
    }

    runner.startTest("SoFieldSensor detach sets getAttachedField to NULL");
    {
        SoCube * cube = new SoCube;
        cube->ref();
        SoFieldSensor sensor;
        sensor.attach(&cube->width);
        sensor.detach();
        bool pass = (sensor.getAttachedField() == nullptr);
        cube->unref();
        runner.endTest(pass, pass ? "" : "After detach, getAttachedField should be NULL");
    }

    runner.startTest("SoFieldSensor fires callback on field change");
    {
        SoCube * cube = new SoCube;
        cube->ref();
        int callCount = 0;
        SoFieldSensor sensor(countCB, &callCount);
        sensor.attach(&cube->width);
        cube->width.setValue(5.0f);
        // The sensor queues a callback; process the sensor queue
        SoDB::getSensorManager()->processDelayQueue(FALSE);
        bool pass = (callCount >= 1);
        sensor.detach();
        cube->unref();
        runner.endTest(pass, pass ? "" : "SoFieldSensor callback did not fire on field change");
    }

    // -----------------------------------------------------------------------
    // SoNodeSensor
    // -----------------------------------------------------------------------
    runner.startTest("SoNodeSensor attach / getAttachedNode");
    {
        SoCube * cube = new SoCube;
        cube->ref();
        SoNodeSensor sensor;
        sensor.attach(cube);
        SoNode * attached = sensor.getAttachedNode();
        bool pass = (attached == cube);
        sensor.detach();
        cube->unref();
        runner.endTest(pass, pass ? "" : "SoNodeSensor getAttachedNode returned wrong node");
    }

    runner.startTest("SoNodeSensor detach sets getAttachedNode to NULL");
    {
        SoCube * cube = new SoCube;
        cube->ref();
        SoNodeSensor sensor;
        sensor.attach(cube);
        sensor.detach();
        bool pass = (sensor.getAttachedNode() == nullptr);
        cube->unref();
        runner.endTest(pass, pass ? "" : "After detach, getAttachedNode should be NULL");
    }

    runner.startTest("SoNodeSensor fires callback on node field change");
    {
        SoCube * cube = new SoCube;
        cube->ref();
        int callCount = 0;
        SoNodeSensor sensor(countCB, &callCount);
        sensor.attach(cube);
        cube->height.setValue(3.0f);
        SoDB::getSensorManager()->processDelayQueue(FALSE);
        bool pass = (callCount >= 1);
        sensor.detach();
        cube->unref();
        runner.endTest(pass, pass ? "" : "SoNodeSensor callback did not fire on node change");
    }

    // -----------------------------------------------------------------------
    // SoTimerSensor
    // -----------------------------------------------------------------------
    runner.startTest("SoTimerSensor setInterval / getInterval round-trip");
    {
        SoTimerSensor sensor;
        SbTime interval(0.5);
        sensor.setInterval(interval);
        bool pass = (std::fabs(sensor.getInterval().getValue() - 0.5) < 1e-9);
        runner.endTest(pass, pass ? "" : "SoTimerSensor setInterval/getInterval failed");
    }

    runner.startTest("SoTimerSensor setBaseTime / getBaseTime round-trip");
    {
        SoTimerSensor sensor;
        SbTime base(1.0);
        sensor.setBaseTime(base);
        bool pass = (std::fabs(sensor.getBaseTime().getValue() - 1.0) < 1e-9);
        runner.endTest(pass, pass ? "" : "SoTimerSensor setBaseTime/getBaseTime failed");
    }

    runner.startTest("SoTimerSensor isScheduled FALSE before schedule");
    {
        SoTimerSensor sensor;
        bool pass = !sensor.isScheduled();
        runner.endTest(pass, pass ? "" : "SoTimerSensor should not be scheduled initially");
    }

    runner.startTest("SoTimerSensor isScheduled TRUE after schedule");
    {
        SoTimerSensor sensor;
        sensor.setInterval(SbTime(1.0));
        sensor.schedule();
        bool pass = sensor.isScheduled();
        sensor.unschedule();
        runner.endTest(pass, pass ? "" : "SoTimerSensor should be scheduled after schedule()");
    }

    // -----------------------------------------------------------------------
    // SoAlarmSensor
    // -----------------------------------------------------------------------
    runner.startTest("SoAlarmSensor setTime / getTime round-trip");
    {
        SoAlarmSensor sensor;
        SbTime t(10.0);
        sensor.setTime(t);
        bool pass = (std::fabs(sensor.getTime().getValue() - 10.0) < 1e-9);
        runner.endTest(pass, pass ? "" : "SoAlarmSensor setTime/getTime failed");
    }

    runner.startTest("SoAlarmSensor isScheduled FALSE before schedule");
    {
        SoAlarmSensor sensor;
        bool pass = !sensor.isScheduled();
        runner.endTest(pass, pass ? "" : "SoAlarmSensor should not be scheduled initially");
    }

    runner.startTest("SoAlarmSensor setTimeFromNow / schedule / unschedule");
    {
        SoAlarmSensor sensor;
        sensor.setTimeFromNow(SbTime(100.0));
        sensor.schedule();
        bool isScheduled = sensor.isScheduled();
        sensor.unschedule();
        bool pass = isScheduled;
        runner.endTest(pass, pass ? "" : "SoAlarmSensor should be scheduled after schedule()");
    }

    // -----------------------------------------------------------------------
    // SoOneShotSensor
    // -----------------------------------------------------------------------
    runner.startTest("SoOneShotSensor isScheduled FALSE before schedule");
    {
        SoOneShotSensor sensor;
        bool pass = !sensor.isScheduled();
        runner.endTest(pass, pass ? "" : "SoOneShotSensor should not be scheduled initially");
    }

    runner.startTest("SoOneShotSensor isScheduled TRUE after schedule");
    {
        SoOneShotSensor sensor;
        sensor.schedule();
        bool pass = sensor.isScheduled();
        sensor.unschedule();
        runner.endTest(pass, pass ? "" : "SoOneShotSensor should be scheduled after schedule()");
    }

    // -----------------------------------------------------------------------
    // SoIdleSensor
    // -----------------------------------------------------------------------
    runner.startTest("SoIdleSensor isScheduled FALSE before schedule");
    {
        SoIdleSensor sensor;
        bool pass = !sensor.isScheduled();
        runner.endTest(pass, pass ? "" : "SoIdleSensor should not be scheduled initially");
    }

    runner.startTest("SoIdleSensor isScheduled TRUE after schedule");
    {
        SoIdleSensor sensor;
        sensor.schedule();
        bool pass = sensor.isScheduled();
        sensor.unschedule();
        runner.endTest(pass, pass ? "" : "SoIdleSensor should be scheduled after schedule()");
    }

    // -----------------------------------------------------------------------
    // SoDataSensor: setTriggerPathFlag
    // -----------------------------------------------------------------------
    runner.startTest("SoDataSensor setTriggerPathFlag TRUE / getTriggerPathFlag");
    {
        SoFieldSensor sensor; // SoFieldSensor is a SoDataSensor
        sensor.setTriggerPathFlag(TRUE);
        bool pass = (sensor.getTriggerPathFlag() == TRUE);
        runner.endTest(pass, pass ? "" : "setTriggerPathFlag(TRUE) failed");
    }

    runner.startTest("SoDataSensor setTriggerPathFlag FALSE / getTriggerPathFlag");
    {
        SoFieldSensor sensor;
        sensor.setTriggerPathFlag(FALSE);
        bool pass = (sensor.getTriggerPathFlag() == FALSE);
        runner.endTest(pass, pass ? "" : "setTriggerPathFlag(FALSE) failed");
    }

    return runner.getSummary();
}
