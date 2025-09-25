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
 * @file test_events.cpp
 * @brief Simple tests for Coin3D events API
 *
 * Tests basic functionality of event classes without external frameworks.
 * Returns 0 for success, non-zero for failure.
 */

#include "test_utils.h"

// Core event classes
#include <Inventor/events/SoEvent.h>
#include <Inventor/events/SoMouseButtonEvent.h>
#include <Inventor/events/SoKeyboardEvent.h>
#include <Inventor/events/SoLocation2Event.h>
#include <Inventor/events/SoMotion3Event.h>
#include <Inventor/events/SoButtonEvent.h>

using namespace SimpleTest;

int main() {
    TestFixture fixture;
    TestRunner runner;
    
    // Test 1: Basic event creation and type checking
    runner.startTest("Basic event creation and type checking");
    try {
        SoMouseButtonEvent* mouseEvent = new SoMouseButtonEvent;
        SoKeyboardEvent* keyEvent = new SoKeyboardEvent;
        SoLocation2Event* locEvent = new SoLocation2Event;
        
        if (mouseEvent->getTypeId() == SoType::badType()) {
            runner.endTest(false, "SoMouseButtonEvent has bad type");
            delete mouseEvent;
            delete keyEvent;
            delete locEvent;
            return 1;
        }
        
        if (keyEvent->getTypeId() == SoType::badType()) {
            runner.endTest(false, "SoKeyboardEvent has bad type");
            delete mouseEvent;
            delete keyEvent;
            delete locEvent;
            return 1;
        }
        
        if (locEvent->getTypeId() == SoType::badType()) {
            runner.endTest(false, "SoLocation2Event has bad type");
            delete mouseEvent;
            delete keyEvent;
            delete locEvent;
            return 1;
        }
        
        // Check inheritance
        if (!mouseEvent->isOfType(SoEvent::getClassTypeId())) {
            runner.endTest(false, "SoMouseButtonEvent is not an SoEvent");
            delete mouseEvent;
            delete keyEvent;
            delete locEvent;
            return 1;
        }
        
        delete mouseEvent;
        delete keyEvent;
        delete locEvent;
        
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    // Test 2: Mouse button event functionality
    runner.startTest("Mouse button event functionality");
    try {
        SoMouseButtonEvent* mouseEvent = new SoMouseButtonEvent;
        
        // Test setting button
        mouseEvent->setButton(SoMouseButtonEvent::BUTTON1);
        if (mouseEvent->getButton() != SoMouseButtonEvent::BUTTON1) {
            runner.endTest(false, "Mouse button not set correctly");
            delete mouseEvent;
            return 1;
        }
        
        // Test setting state
        mouseEvent->setState(SoButtonEvent::DOWN);
        if (mouseEvent->getState() != SoButtonEvent::DOWN) {
            runner.endTest(false, "Mouse button state not set correctly");
            delete mouseEvent;
            return 1;
        }
        
        // Test position
        mouseEvent->setPosition(SbVec2s(100, 200));
        SbVec2s pos = mouseEvent->getPosition();
        if (pos[0] != 100 || pos[1] != 200) {
            runner.endTest(false, "Mouse position not set correctly");
            delete mouseEvent;
            return 1;
        }
        
        delete mouseEvent;
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    // Test 3: Keyboard event functionality
    runner.startTest("Keyboard event functionality");
    try {
        SoKeyboardEvent* keyEvent = new SoKeyboardEvent;
        
        // Test setting key
        keyEvent->setKey(SoKeyboardEvent::A);
        if (keyEvent->getKey() != SoKeyboardEvent::A) {
            runner.endTest(false, "Keyboard key not set correctly");
            delete keyEvent;
            return 1;
        }
        
        // Test setting state
        keyEvent->setState(SoButtonEvent::DOWN);
        if (keyEvent->getState() != SoButtonEvent::DOWN) {
            runner.endTest(false, "Keyboard state not set correctly");
            delete keyEvent;
            return 1;
        }
        
        delete keyEvent;
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    // Test 4: Location event functionality
    runner.startTest("Location event functionality");
    try {
        SoLocation2Event* locEvent = new SoLocation2Event;
        
        // Test position setting
        locEvent->setPosition(SbVec2s(150, 250));
        SbVec2s pos = locEvent->getPosition();
        if (pos[0] != 150 || pos[1] != 250) {
            runner.endTest(false, "Location position not set correctly");
            delete locEvent;
            return 1;
        }
        
        delete locEvent;
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    // Test 5: Event time and modifiers
    runner.startTest("Event time and modifiers");
    try {
        SoMouseButtonEvent* mouseEvent = new SoMouseButtonEvent;
        
        // Test time setting
        SbTime testTime(12.5);
        mouseEvent->setTime(testTime);
        SbTime eventTime = mouseEvent->getTime();
        if (eventTime.getValue() != 12.5) {
            runner.endTest(false, "Event time not set correctly");
            delete mouseEvent;
            return 1;
        }
        
        // Test modifier setting
        mouseEvent->setShiftDown(TRUE);
        mouseEvent->setCtrlDown(TRUE);
        mouseEvent->setAltDown(FALSE);
        
        if (!mouseEvent->wasShiftDown()) {
            runner.endTest(false, "Shift modifier not set correctly");
            delete mouseEvent;
            return 1;
        }
        
        if (!mouseEvent->wasCtrlDown()) {
            runner.endTest(false, "Ctrl modifier not set correctly");
            delete mouseEvent;
            return 1;
        }
        
        if (mouseEvent->wasAltDown()) {
            runner.endTest(false, "Alt modifier should be false");
            delete mouseEvent;
            return 1;
        }
        
        delete mouseEvent;
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    return runner.getSummary();
}