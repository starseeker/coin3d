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
 * @file test_sensors_extra.cpp
 * @brief Additional sensor API tests beyond test_sensors_deeper.cpp.
 *
 * Covers behaviors not yet tested in test_sensors_suite.cpp or
 * test_sensors_deeper.cpp:
 *
 *   SoAlarmSensor:   construction/destruction (class type valid)
 *   SoTimerSensor:   construction/destruction (class type valid)
 *   SoFieldSensor:   no callback after detach; attach to standalone SoSFFloat
 *   SoNodeSensor:    callback fires on addChild (structural change);
 *                    no callback after detach + addChild
 *   SoPathSensor:    setTriggerFilter / getTriggerFilter round-trip
 *   SoOneShotSensor: callback fires after processDelayQueue(FALSE)
 *   SoIdleSensor:    callback fires after processDelayQueue(TRUE)
 *   SoDataSensor:    getTriggerNode() / getTriggerPath() null before trigger
 */

#include "../test_utils.h"

#include <Inventor/sensors/SoAlarmSensor.h>
#include <Inventor/sensors/SoTimerSensor.h>
#include <Inventor/sensors/SoFieldSensor.h>
#include <Inventor/sensors/SoNodeSensor.h>
#include <Inventor/sensors/SoPathSensor.h>
#include <Inventor/sensors/SoOneShotSensor.h>
#include <Inventor/sensors/SoIdleSensor.h>
#include <Inventor/sensors/SoDataSensor.h>
#include <Inventor/SbTime.h>
#include <Inventor/SoDB.h>
#include <Inventor/fields/SoSFFloat.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/SoPath.h>

using namespace SimpleTest;

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
    // SoAlarmSensor: construction / destruction (class type valid)
    // -----------------------------------------------------------------------
    runner.startTest("SoAlarmSensor default construction / destruction");
    {
        SoAlarmSensor * sensor = new SoAlarmSensor;
        bool pass = (sensor != nullptr);
        delete sensor;
        runner.endTest(pass, pass ? "" : "SoAlarmSensor default construction failed");
    }

    runner.startTest("SoAlarmSensor callback construction / destruction");
    {
        int count = 0;
        SoAlarmSensor * sensor = new SoAlarmSensor(countCB, &count);
        bool pass = (sensor != nullptr)
                 && (sensor->getFunction() == countCB)
                 && (sensor->getData()     == &count);
        delete sensor;
        runner.endTest(pass, pass ? "" : "SoAlarmSensor callback construction failed");
    }

    // -----------------------------------------------------------------------
    // SoTimerSensor: construction / destruction (class type valid)
    // -----------------------------------------------------------------------
    runner.startTest("SoTimerSensor default construction / destruction");
    {
        SoTimerSensor * sensor = new SoTimerSensor;
        bool pass = (sensor != nullptr);
        delete sensor;
        runner.endTest(pass, pass ? "" : "SoTimerSensor default construction failed");
    }

    runner.startTest("SoTimerSensor callback construction / destruction");
    {
        int count = 0;
        SoTimerSensor * sensor = new SoTimerSensor(countCB, &count);
        bool pass = (sensor != nullptr)
                 && (sensor->getFunction() == countCB)
                 && (sensor->getData()     == &count);
        delete sensor;
        runner.endTest(pass, pass ? "" : "SoTimerSensor callback construction failed");
    }

    // -----------------------------------------------------------------------
    // SoFieldSensor: attach to standalone SoSFFloat field
    // -----------------------------------------------------------------------
    runner.startTest("SoFieldSensor attach to standalone SoSFFloat");
    {
        // SoSFFloat must be owned by a container node for ref-counting, so we
        // use a SoCube's field which is already a SoSFFloat.
        SoCube * cube = new SoCube;
        cube->ref();
        int count = 0;
        SoFieldSensor sensor(countCB, &count);
        sensor.attach(&cube->width);
        bool attached = (sensor.getAttachedField() == &cube->width);
        sensor.detach();
        cube->unref();
        runner.endTest(attached, attached ? "" :
            "SoFieldSensor failed to attach to SoSFFloat field");
    }

    // -----------------------------------------------------------------------
    // SoFieldSensor: no callback fires after detach
    // -----------------------------------------------------------------------
    runner.startTest("SoFieldSensor no callback after detach");
    {
        SoCube * cube = new SoCube;
        cube->ref();
        int count = 0;
        SoFieldSensor sensor(countCB, &count);
        sensor.attach(&cube->width);
        sensor.detach();
        // Modify the field after detach — callback must not fire.
        cube->width.setValue(99.0f);
        SoDB::getSensorManager()->processDelayQueue(FALSE);
        bool pass = (count == 0);
        cube->unref();
        runner.endTest(pass, pass ? "" :
            "SoFieldSensor callback fired after detach — should not have");
    }

    // -----------------------------------------------------------------------
    // SoNodeSensor: callback fires on structural change (addChild)
    // -----------------------------------------------------------------------
    runner.startTest("SoNodeSensor fires on addChild to SoSeparator");
    {
        SoSeparator * root = new SoSeparator;
        root->ref();
        int count = 0;
        SoNodeSensor sensor(countCB, &count);
        sensor.attach(root);
        root->addChild(new SoCube);
        SoDB::getSensorManager()->processDelayQueue(FALSE);
        bool pass = (count >= 1);
        sensor.detach();
        root->unref();
        runner.endTest(pass, pass ? "" :
            "SoNodeSensor callback did not fire on addChild");
    }

    runner.startTest("SoNodeSensor no callback after detach + addChild");
    {
        SoSeparator * root = new SoSeparator;
        root->ref();
        int count = 0;
        SoNodeSensor sensor(countCB, &count);
        sensor.attach(root);
        sensor.detach();
        root->addChild(new SoCube);
        SoDB::getSensorManager()->processDelayQueue(FALSE);
        bool pass = (count == 0);
        root->unref();
        runner.endTest(pass, pass ? "" :
            "SoNodeSensor callback fired after detach — should not have");
    }

    // -----------------------------------------------------------------------
    // SoPathSensor: setTriggerFilter / getTriggerFilter round-trip
    // -----------------------------------------------------------------------
    runner.startTest("SoPathSensor setTriggerFilter PATH / getTriggerFilter");
    {
        SoPathSensor sensor;
        sensor.setTriggerFilter(SoPathSensor::PATH);
        bool pass = (sensor.getTriggerFilter() == SoPathSensor::PATH);
        runner.endTest(pass, pass ? "" :
            "SoPathSensor setTriggerFilter(PATH) round-trip failed");
    }

    runner.startTest("SoPathSensor setTriggerFilter NODES / getTriggerFilter");
    {
        SoPathSensor sensor;
        sensor.setTriggerFilter(SoPathSensor::NODES);
        bool pass = (sensor.getTriggerFilter() == SoPathSensor::NODES);
        runner.endTest(pass, pass ? "" :
            "SoPathSensor setTriggerFilter(NODES) round-trip failed");
    }

    runner.startTest("SoPathSensor setTriggerFilter PATH_AND_NODES / getTriggerFilter");
    {
        SoPathSensor sensor;
        sensor.setTriggerFilter(SoPathSensor::PATH_AND_NODES);
        bool pass = (sensor.getTriggerFilter() == SoPathSensor::PATH_AND_NODES);
        runner.endTest(pass, pass ? "" :
            "SoPathSensor setTriggerFilter(PATH_AND_NODES) round-trip failed");
    }

    // -----------------------------------------------------------------------
    // SoOneShotSensor: callback fires after processDelayQueue(FALSE)
    // -----------------------------------------------------------------------
    runner.startTest("SoOneShotSensor callback fires via processDelayQueue");
    {
        int count = 0;
        SoOneShotSensor sensor(countCB, &count);
        sensor.schedule();
        SoDB::getSensorManager()->processDelayQueue(FALSE);
        bool pass = (count >= 1);
        if (sensor.isScheduled()) sensor.unschedule();
        runner.endTest(pass, pass ? "" :
            "SoOneShotSensor callback did not fire via processDelayQueue");
    }

    // -----------------------------------------------------------------------
    // SoIdleSensor: callback fires after processDelayQueue(TRUE) (idle pass)
    // -----------------------------------------------------------------------
    runner.startTest("SoIdleSensor callback fires via processDelayQueue(idle)");
    {
        int count = 0;
        SoIdleSensor sensor(countCB, &count);
        sensor.schedule();
        // SoIdleSensor is idle-only; it fires only when isidle=TRUE.
        SoDB::getSensorManager()->processDelayQueue(TRUE);
        bool pass = (count >= 1);
        if (sensor.isScheduled()) sensor.unschedule();
        runner.endTest(pass, pass ? "" :
            "SoIdleSensor callback did not fire via processDelayQueue(TRUE)");
    }

    // -----------------------------------------------------------------------
    // SoDataSensor: getTriggerNode / getTriggerPath null before any trigger
    // -----------------------------------------------------------------------
    runner.startTest("SoDataSensor getTriggerNode null before trigger");
    {
        // SoFieldSensor IS-A SoDataSensor — use it to reach the base API.
        SoFieldSensor sensor;
        SoNode * tn = sensor.getTriggerNode();
        bool pass = (tn == nullptr);
        runner.endTest(pass, pass ? "" :
            "SoDataSensor::getTriggerNode() should be null before any trigger");
    }

    runner.startTest("SoDataSensor getTriggerPath null before trigger");
    {
        SoFieldSensor sensor;
        SoPath * tp = sensor.getTriggerPath();
        bool pass = (tp == nullptr);
        runner.endTest(pass, pass ? "" :
            "SoDataSensor::getTriggerPath() should be null before any trigger");
    }

    runner.startTest("SoDataSensor getTriggerField null before trigger");
    {
        SoFieldSensor sensor;
        SoField * tf = sensor.getTriggerField();
        bool pass = (tf == nullptr);
        runner.endTest(pass, pass ? "" :
            "SoDataSensor::getTriggerField() should be null before any trigger");
    }

    return runner.getSummary() != 0 ? 1 : 0;
}
