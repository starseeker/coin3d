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

using namespace ObolTest;

static void countCB(void * data, SoSensor *)
{
    int * count = static_cast<int *>(data);
    (*count)++;
}

TEST(SensorsExtra, SoAlarmSensorDefaultConstructionDestruction)
{
    SoAlarmSensor * sensor = new SoAlarmSensor;
    EXPECT_TRUE((sensor != nullptr)) << "SoAlarmSensor default construction failed";
    delete sensor;
}

TEST(SensorsExtra, SoAlarmSensorCallbackConstructionDestruction)
{
    int count = 0;
    SoAlarmSensor * sensor = new SoAlarmSensor(countCB, &count);
    EXPECT_TRUE((sensor != nullptr)
             && (sensor->getFunction() == countCB)
             && (sensor->getData()     == &count)) << "SoAlarmSensor callback construction failed";
    delete sensor;
}

// -----------------------------------------------------------------------
// SoTimerSensor: construction / destruction (class type valid)
// -----------------------------------------------------------------------

TEST(SensorsExtra, SoTimerSensorDefaultConstructionDestruction)
{
    SoTimerSensor * sensor = new SoTimerSensor;
    EXPECT_TRUE((sensor != nullptr)) << "SoTimerSensor default construction failed";
    delete sensor;
}

TEST(SensorsExtra, SoTimerSensorCallbackConstructionDestruction)
{
    int count = 0;
    SoTimerSensor * sensor = new SoTimerSensor(countCB, &count);
    EXPECT_TRUE((sensor != nullptr)
             && (sensor->getFunction() == countCB)
             && (sensor->getData()     == &count)) << "SoTimerSensor callback construction failed";
    delete sensor;
}

// -----------------------------------------------------------------------
// SoFieldSensor: attach to standalone SoSFFloat field
// -----------------------------------------------------------------------

TEST(SensorsExtra, SoFieldSensorAttachToStandaloneSoSFFloat)
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
    EXPECT_TRUE(attached)
        << "SoFieldSensor failed to attach to SoSFFloat field";
}

// -----------------------------------------------------------------------
// SoFieldSensor: no callback fires after detach
// -----------------------------------------------------------------------

TEST(SensorsExtra, SoFieldSensorNoCallbackAfterDetach)
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
    EXPECT_TRUE((count == 0)) << "SoFieldSensor callback fired after detach — should not have";
    cube->unref();
}

// -----------------------------------------------------------------------
// SoNodeSensor: callback fires on structural change (addChild)
// -----------------------------------------------------------------------

TEST(SensorsExtra, SoNodeSensorFiresOnAddChildToSoSeparator)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    int count = 0;
    SoNodeSensor sensor(countCB, &count);
    sensor.attach(root);
    root->addChild(new SoCube);
    SoDB::getSensorManager()->processDelayQueue(FALSE);
    EXPECT_TRUE((count >= 1)) << "SoNodeSensor callback did not fire on addChild";
    sensor.detach();
    root->unref();
}

TEST(SensorsExtra, SoNodeSensorNoCallbackAfterDetachAddChild)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    int count = 0;
    SoNodeSensor sensor(countCB, &count);
    sensor.attach(root);
    sensor.detach();
    root->addChild(new SoCube);
    SoDB::getSensorManager()->processDelayQueue(FALSE);
    EXPECT_TRUE((count == 0)) << "SoNodeSensor callback fired after detach — should not have";
    root->unref();
}

// -----------------------------------------------------------------------
// SoPathSensor: setTriggerFilter / getTriggerFilter round-trip
// -----------------------------------------------------------------------

TEST(SensorsExtra, SoPathSensorSetTriggerFilterPATHGetTriggerFilter)
{
    SoPathSensor sensor;
    sensor.setTriggerFilter(SoPathSensor::PATH);
    EXPECT_TRUE((sensor.getTriggerFilter() == SoPathSensor::PATH)) << "SoPathSensor setTriggerFilter(PATH) round-trip failed";
}

TEST(SensorsExtra, SoPathSensorSetTriggerFilterNODESGetTriggerFilter)
{
    SoPathSensor sensor;
    sensor.setTriggerFilter(SoPathSensor::NODES);
    EXPECT_TRUE((sensor.getTriggerFilter() == SoPathSensor::NODES)) << "SoPathSensor setTriggerFilter(NODES) round-trip failed";
}

TEST(SensorsExtra, SoPathSensorSetTriggerFilterPATHANDNODESGetTriggerFilter)
{
    SoPathSensor sensor;
    sensor.setTriggerFilter(SoPathSensor::PATH_AND_NODES);
    EXPECT_TRUE((sensor.getTriggerFilter() == SoPathSensor::PATH_AND_NODES)) << "SoPathSensor setTriggerFilter(PATH_AND_NODES) round-trip failed";
}

// -----------------------------------------------------------------------
// SoOneShotSensor: callback fires after processDelayQueue(FALSE)
// -----------------------------------------------------------------------

TEST(SensorsExtra, SoOneShotSensorCallbackFiresViaProcessDelayQueue)
{
    int count = 0;
    SoOneShotSensor sensor(countCB, &count);
    sensor.schedule();
    SoDB::getSensorManager()->processDelayQueue(FALSE);
    EXPECT_TRUE((count >= 1)) << "SoOneShotSensor callback did not fire via processDelayQueue";
    if (sensor.isScheduled()) sensor.unschedule();
}

// -----------------------------------------------------------------------
// SoIdleSensor: callback fires after processDelayQueue(TRUE) (idle pass)
// -----------------------------------------------------------------------

TEST(SensorsExtra, SoIdleSensorCallbackFiresViaProcessDelayQueueIdle)
{
    int count = 0;
    SoIdleSensor sensor(countCB, &count);
    sensor.schedule();
    // SoIdleSensor is idle-only; it fires only when isidle=TRUE.
    SoDB::getSensorManager()->processDelayQueue(TRUE);
    EXPECT_TRUE((count >= 1)) << "SoIdleSensor callback did not fire via processDelayQueue(TRUE)";
    if (sensor.isScheduled()) sensor.unschedule();
}

// -----------------------------------------------------------------------
// SoDataSensor: getTriggerNode / getTriggerPath null before any trigger
// -----------------------------------------------------------------------

TEST(SensorsExtra, SoDataSensorGetTriggerNodeNullBeforeTrigger)
{
    // SoFieldSensor IS-A SoDataSensor — use it to reach the base API.
    SoFieldSensor sensor;
    SoNode * tn = sensor.getTriggerNode();
    EXPECT_TRUE((tn == nullptr)) << "SoDataSensor::getTriggerNode() should be null before any trigger";
}

TEST(SensorsExtra, SoDataSensorGetTriggerPathNullBeforeTrigger)
{
    SoFieldSensor sensor;
    SoPath * tp = sensor.getTriggerPath();
    EXPECT_TRUE((tp == nullptr)) << "SoDataSensor::getTriggerPath() should be null before any trigger";
}

TEST(SensorsExtra, SoDataSensorGetTriggerFieldNullBeforeTrigger)
{
    SoFieldSensor sensor;
    SoField * tf = sensor.getTriggerField();
    EXPECT_TRUE((tf == nullptr)) << "SoDataSensor::getTriggerField() should be null before any trigger";
}
