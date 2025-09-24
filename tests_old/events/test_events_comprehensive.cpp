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

#include "utils/test_common.h"
#include <Inventor/events/SoEvent.h>
#include <Inventor/events/SoMouseButtonEvent.h>
#include <Inventor/events/SoKeyboardEvent.h>
#include <Inventor/events/SoLocation2Event.h>
#include <Inventor/events/SoMotion3Event.h>
#include <Inventor/events/SoButtonEvent.h>
#include <Inventor/events/SoSpaceballButtonEvent.h>
#include <Inventor/SbTime.h>
#include <Inventor/SbViewportRegion.h>

using namespace CoinTestUtils;

TEST_CASE("SoEvent basic functionality", "[events][SoEvent]") {
    CoinTestFixture fixture;
    
    SECTION("Event type system") {
        // Test event type identification
        SoType event_type = SoEvent::getClassTypeId();
        CHECK(event_type != SoType::badType());
        // Note: Type names may include class prefixes
        
        // Test specific event types
        SoType mouse_type = SoMouseButtonEvent::getClassTypeId();
        CHECK(mouse_type.isDerivedFrom(event_type));
        // Note: Type names may include class prefixes
        
        SoType keyboard_type = SoKeyboardEvent::getClassTypeId();
        CHECK(keyboard_type.isDerivedFrom(event_type));
        // Note: Type names may include class prefixes
    }
    
    SECTION("Event creation and basic properties") {
        SoMouseButtonEvent* mouse_event = new SoMouseButtonEvent;
        CHECK(mouse_event != nullptr);
        CHECK(mouse_event->getTypeId() == SoMouseButtonEvent::getClassTypeId());
        
        // Test time setting
        SbTime current_time = SbTime::getTimeOfDay();
        mouse_event->setTime(current_time);
        CHECK(mouse_event->getTime() == current_time);
        
        delete mouse_event;
    }
    
    SECTION("Event position") {
        SoMouseButtonEvent* mouse_event = new SoMouseButtonEvent;
        
        // Test position setting
        SbVec2s position(100, 200);
        mouse_event->setPosition(position);
        CHECK(mouse_event->getPosition() == position);
        
        // Test normalized position
        SbViewportRegion viewport_region(800, 600);
        mouse_event->setPosition(position);
        SbVec2f normalized = mouse_event->getNormalizedPosition(viewport_region);
        
        CHECK(fabs(normalized[0] - (100.0f / 800.0f)) < 1e-6);
        CHECK(fabs(normalized[1] - (200.0f / 600.0f)) < 1e-6);
        
        delete mouse_event;
    }
}

TEST_CASE("SoMouseButtonEvent functionality", "[events][SoMouseButtonEvent]") {
    CoinTestFixture fixture;
    
    SECTION("Mouse button properties") {
        SoMouseButtonEvent* event = new SoMouseButtonEvent;
        
        // Test button setting
        event->setButton(SoMouseButtonEvent::BUTTON1);
        CHECK(event->getButton() == SoMouseButtonEvent::BUTTON1);
        
        event->setButton(SoMouseButtonEvent::BUTTON2);
        CHECK(event->getButton() == SoMouseButtonEvent::BUTTON2);
        
        event->setButton(SoMouseButtonEvent::BUTTON3);
        CHECK(event->getButton() == SoMouseButtonEvent::BUTTON3);
        
        delete event;
    }
    
    SECTION("Mouse button state") {
        SoMouseButtonEvent* event = new SoMouseButtonEvent;
        
        // Test state setting
        event->setState(SoButtonEvent::DOWN);
        CHECK(event->getState() == SoButtonEvent::DOWN);
        
        event->setState(SoButtonEvent::UP);
        CHECK(event->getState() == SoButtonEvent::UP);
        
        delete event;
    }
    
    SECTION("Mouse button event identification") {
        SoMouseButtonEvent* event = new SoMouseButtonEvent;
        event->setButton(SoMouseButtonEvent::BUTTON1);
        event->setState(SoButtonEvent::DOWN);
        
        // Test isButtonPressEvent
        CHECK(SoMouseButtonEvent::isButtonPressEvent(event, SoMouseButtonEvent::BUTTON1));
        CHECK(!SoMouseButtonEvent::isButtonPressEvent(event, SoMouseButtonEvent::BUTTON2));
        
        // Test isButtonReleaseEvent
        event->setState(SoButtonEvent::UP);
        CHECK(SoMouseButtonEvent::isButtonReleaseEvent(event, SoMouseButtonEvent::BUTTON1));
        CHECK(!SoMouseButtonEvent::isButtonReleaseEvent(event, SoMouseButtonEvent::BUTTON2));
        
        delete event;
    }
}

TEST_CASE("SoKeyboardEvent functionality", "[events][SoKeyboardEvent]") {
    CoinTestFixture fixture;
    
    SECTION("Keyboard key properties") {
        SoKeyboardEvent* event = new SoKeyboardEvent;
        
        // Test key setting
        event->setKey(SoKeyboardEvent::A);
        CHECK(event->getKey() == SoKeyboardEvent::A);
        
        event->setKey(SoKeyboardEvent::ESCAPE);
        CHECK(event->getKey() == SoKeyboardEvent::ESCAPE);
        
        event->setKey(SoKeyboardEvent::SPACE);
        CHECK(event->getKey() == SoKeyboardEvent::SPACE);
        
        delete event;
    }
    
    SECTION("Keyboard event identification") {
        SoKeyboardEvent* event = new SoKeyboardEvent;
        event->setKey(SoKeyboardEvent::A);
        event->setState(SoButtonEvent::DOWN);
        
        // Test isKeyPressEvent
        CHECK(SoKeyboardEvent::isKeyPressEvent(event, SoKeyboardEvent::A));
        CHECK(!SoKeyboardEvent::isKeyPressEvent(event, SoKeyboardEvent::B));
        
        // Test isKeyReleaseEvent
        event->setState(SoButtonEvent::UP);
        CHECK(SoKeyboardEvent::isKeyReleaseEvent(event, SoKeyboardEvent::A));
        CHECK(!SoKeyboardEvent::isKeyReleaseEvent(event, SoKeyboardEvent::B));
        
        delete event;
    }
    
    SECTION("Special key handling") {
        SoKeyboardEvent* event = new SoKeyboardEvent;
        
        // Test special keys
        event->setKey(SoKeyboardEvent::LEFT_SHIFT);
        CHECK(event->getKey() == SoKeyboardEvent::LEFT_SHIFT);
        
        event->setKey(SoKeyboardEvent::LEFT_CONTROL);
        CHECK(event->getKey() == SoKeyboardEvent::LEFT_CONTROL);
        
        event->setKey(SoKeyboardEvent::LEFT_ALT);
        CHECK(event->getKey() == SoKeyboardEvent::LEFT_ALT);
        
        delete event;
    }
}

TEST_CASE("SoLocation2Event functionality", "[events][SoLocation2Event]") {
    CoinTestFixture fixture;
    
    SECTION("2D location properties") {
        SoLocation2Event* event = new SoLocation2Event;
        
        // Test position setting
        SbVec2s position(150, 250);
        event->setPosition(position);
        CHECK(event->getPosition() == position);
        
        // Test that it's a location event
        CHECK(event->getTypeId() == SoLocation2Event::getClassTypeId());
        
        delete event;
    }
}

TEST_CASE("SoMotion3Event functionality", "[events][SoMotion3Event]") {
    CoinTestFixture fixture;
    
    SECTION("3D motion properties") {
        SoMotion3Event* event = new SoMotion3Event;
        
        // Test translation setting
        SbVec3f translation(1.0f, 2.0f, 3.0f);
        event->setTranslation(translation);
        CHECK(event->getTranslation() == translation);
        
        // Test rotation setting
        SbRotation rotation(SbVec3f(0.0f, 1.0f, 0.0f), M_PI/4);
        event->setRotation(rotation);
        
        SbRotation result_rotation = event->getRotation();
        SbVec3f result_axis;
        float result_angle;
        result_rotation.getValue(result_axis, result_angle);
        
        CHECK(fabs(result_angle - M_PI/4) < 1e-6);
        
        delete event;
    }
}

TEST_CASE("SoSpaceballButtonEvent functionality", "[events][SoSpaceballButtonEvent]") {
    CoinTestFixture fixture;
    
    SECTION("Spaceball button properties") {
        SoSpaceballButtonEvent* event = new SoSpaceballButtonEvent;
        
        // Test button setting
        event->setButton(SoSpaceballButtonEvent::BUTTON1);
        CHECK(event->getButton() == SoSpaceballButtonEvent::BUTTON1);
        
        event->setButton(SoSpaceballButtonEvent::BUTTON2);
        CHECK(event->getButton() == SoSpaceballButtonEvent::BUTTON2);
        
        // Test state
        event->setState(SoButtonEvent::DOWN);
        CHECK(event->getState() == SoButtonEvent::DOWN);
        
        delete event;
    }
}

TEST_CASE("Event modifiers", "[events][modifiers]") {
    CoinTestFixture fixture;
    
    SECTION("Shift modifier") {
        SoMouseButtonEvent* event = new SoMouseButtonEvent;
        
        // Test shift modifier
        event->setShiftDown(TRUE);
        CHECK(event->wasShiftDown());
        
        event->setShiftDown(FALSE);
        CHECK(!event->wasShiftDown());
        
        delete event;
    }
    
    SECTION("Control modifier") {
        SoMouseButtonEvent* event = new SoMouseButtonEvent;
        
        // Test control modifier
        event->setCtrlDown(TRUE);
        CHECK(event->wasCtrlDown());
        
        event->setCtrlDown(FALSE);
        CHECK(!event->wasCtrlDown());
        
        delete event;
    }
    
    SECTION("Alt modifier") {
        SoMouseButtonEvent* event = new SoMouseButtonEvent;
        
        // Test alt modifier
        event->setAltDown(TRUE);
        CHECK(event->wasAltDown());
        
        event->setAltDown(FALSE);
        CHECK(!event->wasAltDown());
        
        delete event;
    }
    
    SECTION("Multiple modifiers") {
        SoKeyboardEvent* event = new SoKeyboardEvent;
        
        // Test multiple modifiers simultaneously
        event->setShiftDown(TRUE);
        event->setCtrlDown(TRUE);
        event->setAltDown(FALSE);
        
        CHECK(event->wasShiftDown());
        CHECK(event->wasCtrlDown());
        CHECK(!event->wasAltDown());
        
        delete event;
    }
}

TEST_CASE("Event copying and assignment", "[events][copying]") {
    CoinTestFixture fixture;
    
    SECTION("Mouse event copying") {
        SoMouseButtonEvent* original = new SoMouseButtonEvent;
        original->setButton(SoMouseButtonEvent::BUTTON2);
        original->setState(SoButtonEvent::DOWN);
        original->setPosition(SbVec2s(300, 400));
        original->setShiftDown(TRUE);
        
        // Create copy
        SoMouseButtonEvent* copy = new SoMouseButtonEvent;
        *copy = *original;
        
        CHECK(copy->getButton() == original->getButton());
        CHECK(copy->getState() == original->getState());
        CHECK(copy->getPosition() == original->getPosition());
        CHECK(copy->wasShiftDown() == original->wasShiftDown());
        
        delete original;
        delete copy;
    }
    
    SECTION("Keyboard event copying") {
        SoKeyboardEvent* original = new SoKeyboardEvent;
        original->setKey(SoKeyboardEvent::ENTER);
        original->setState(SoButtonEvent::DOWN);
        original->setCtrlDown(TRUE);
        
        SoKeyboardEvent* copy = new SoKeyboardEvent;
        *copy = *original;
        
        CHECK(copy->getKey() == original->getKey());
        CHECK(copy->getState() == original->getState());
        CHECK(copy->wasCtrlDown() == original->wasCtrlDown());
        
        delete original;
        delete copy;
    }
}

TEST_CASE("Event time handling", "[events][time]") {
    CoinTestFixture fixture;
    
    SECTION("Event timing") {
        SoMouseButtonEvent* event = new SoMouseButtonEvent;
        
        SbTime time1 = SbTime::getTimeOfDay();
        event->setTime(time1);
        CHECK(event->getTime() == time1);
        
        // Set different time
        SbTime time2 = time1 + 1.0; // 1 second later
        event->setTime(time2);
        CHECK(event->getTime() == time2);
        CHECK(event->getTime() != time1);
        
        delete event;
    }
}

TEST_CASE("Event type checking", "[events][type_checking]") {
    CoinTestFixture fixture;
    
    SECTION("Type identification") {
        SoMouseButtonEvent* mouse_event = new SoMouseButtonEvent;
        SoKeyboardEvent* keyboard_event = new SoKeyboardEvent;
        SoLocation2Event* location_event = new SoLocation2Event;
        
        // Test type checking
        CHECK(mouse_event->isOfType(SoMouseButtonEvent::getClassTypeId()));
        CHECK(mouse_event->isOfType(SoButtonEvent::getClassTypeId()));
        CHECK(mouse_event->isOfType(SoEvent::getClassTypeId()));
        CHECK(!mouse_event->isOfType(SoKeyboardEvent::getClassTypeId()));
        
        CHECK(keyboard_event->isOfType(SoKeyboardEvent::getClassTypeId()));
        CHECK(keyboard_event->isOfType(SoButtonEvent::getClassTypeId()));
        CHECK(!keyboard_event->isOfType(SoMouseButtonEvent::getClassTypeId()));
        
        CHECK(location_event->isOfType(SoLocation2Event::getClassTypeId()));
        CHECK(location_event->isOfType(SoEvent::getClassTypeId()));
        CHECK(!location_event->isOfType(SoButtonEvent::getClassTypeId()));
        
        delete mouse_event;
        delete keyboard_event;
        delete location_event;
    }
}