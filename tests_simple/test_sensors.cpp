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
 * @file test_sensors.cpp  
 * @brief Simple tests for Coin3D sensors API
 *
 * Tests basic functionality of sensor classes without external frameworks.
 * Returns 0 for success, non-zero for failure.
 */

#include "test_utils.h"

// Core sensor classes
#include <Inventor/sensors/SoSensor.h>
#include <Inventor/sensors/SoTimerSensor.h>
#include <Inventor/sensors/SoNodeSensor.h>
#include <Inventor/sensors/SoFieldSensor.h>
#include <Inventor/sensors/SoDataSensor.h>
#include <Inventor/sensors/SoAlarmSensor.h>

// For testing with nodes
#include <Inventor/nodes/SoCube.h>

using namespace SimpleTest;

// Simple callback function for sensor testing
static void testSensorCallback(void* data, SoSensor* sensor) {
    // Just mark that callback was called
    int* flag = static_cast<int*>(data);
    *flag = 1;
}

int main() {
    TestFixture fixture;
    TestRunner runner;
    
    // Test 1: Basic sensor type checking
    runner.startTest("Basic sensor type checking");
    try {
        SoTimerSensor* timer = new SoTimerSensor;
        SoAlarmSensor* alarm = new SoAlarmSensor;
        
        // Sensors don't have getTypeId() like nodes, so we just test creation
        if (timer == nullptr) {
            runner.endTest(false, "SoTimerSensor creation failed");
            delete alarm;
            return 1;
        }
        
        if (alarm == nullptr) {
            runner.endTest(false, "SoAlarmSensor creation failed");
            delete timer;
            return 1;
        }
        
        delete timer;
        delete alarm;
        
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    // Test 2: Timer sensor basic setup
    runner.startTest("Timer sensor basic setup");
    try {
        int callbackFlag = 0;
        SoTimerSensor* timer = new SoTimerSensor(testSensorCallback, &callbackFlag);
        
        // Test setting interval
        SbTime interval(1.0);
        timer->setInterval(interval);
        
        if (timer->getInterval() != interval) {
            runner.endTest(false, "Timer sensor interval not set correctly");
            delete timer;
            return 1;
        }
        
        // Test that sensor is not scheduled initially
        if (timer->isScheduled()) {
            runner.endTest(false, "Timer sensor should not be scheduled initially");
            delete timer;
            return 1;
        }
        
        delete timer;
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    // Test 3: Alarm sensor basic setup  
    runner.startTest("Alarm sensor basic setup");
    try {
        int callbackFlag = 0;
        SoAlarmSensor* alarm = new SoAlarmSensor(testSensorCallback, &callbackFlag);
        
        // Test setting alarm time
        SbTime alarmTime = SbTime::getTimeOfDay() + SbTime(5.0); // 5 seconds from now
        alarm->setTime(alarmTime);
        
        SbTime retrievedTime = alarm->getTime();
        if (retrievedTime != alarmTime) {
            runner.endTest(false, "Alarm sensor time not set correctly");
            delete alarm;
            return 1;
        }
        
        delete alarm;
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    // Test 4: Node sensor basic setup
    runner.startTest("Node sensor basic setup");
    try {
        int callbackFlag = 0;
        SoNodeSensor* nodeSensor = new SoNodeSensor(testSensorCallback, &callbackFlag);
        
        SoCube* cube = new SoCube;
        cube->ref();
        
        // Test attaching to a node
        nodeSensor->attach(cube);
        
        if (nodeSensor->getAttachedNode() != cube) {
            runner.endTest(false, "Node sensor not attached correctly");
            cube->unref();
            delete nodeSensor;
            return 1;
        }
        
        // Detach before cleanup
        nodeSensor->detach();
        
        cube->unref();
        delete nodeSensor;
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    // Test 5: Field sensor basic setup
    runner.startTest("Field sensor basic setup");
    try {
        int callbackFlag = 0;
        SoFieldSensor* fieldSensor = new SoFieldSensor(testSensorCallback, &callbackFlag);
        
        SoCube* cube = new SoCube;
        cube->ref();
        
        // Test attaching to a field
        fieldSensor->attach(&cube->width);
        
        if (fieldSensor->getAttachedField() != &cube->width) {
            runner.endTest(false, "Field sensor not attached correctly");
            cube->unref();
            delete fieldSensor;
            return 1;
        }
        
        // Detach before cleanup
        fieldSensor->detach();
        
        cube->unref();
        delete fieldSensor;
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    return runner.getSummary();
}