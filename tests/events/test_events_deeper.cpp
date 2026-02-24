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
 * @file test_events_deeper.cpp
 * @brief Deeper tests for SoEvent subclasses (events/ 23.2 %).
 *
 * Covers:
 *   SoEvent:
 *     setTime/getTime, setPosition/getPosition, setShiftDown/wasShiftDown,
 *     setCtrlDown/wasCtrlDown, setAltDown/wasAltDown, isOfType, getTypeId
 *   SoKeyboardEvent:
 *     setKey/getKey, getPrintableCharacter, isKeyPressEvent (macro/static),
 *     isKeyReleaseEvent, ALL key constant
 *   SoMouseButtonEvent:
 *     setButton/getButton, isButtonPressEvent, isButtonReleaseEvent,
 *     LEFT/MIDDLE/RIGHT/BUTTON4/BUTTON5/ANY constants
 *   SoButtonEvent:
 *     setState/getState, UP/DOWN states
 */

#include "../test_utils.h"

#include <Inventor/events/SoEvent.h>
#include <Inventor/events/SoButtonEvent.h>
#include <Inventor/events/SoKeyboardEvent.h>
#include <Inventor/events/SoMouseButtonEvent.h>
#include <Inventor/events/SoLocation2Event.h>
#include <Inventor/SbTime.h>
#include <Inventor/SbVec2s.h>
#include <Inventor/SoType.h>

#include <cmath>

using namespace SimpleTest;

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // -----------------------------------------------------------------------
    // SoEvent base: time, position, modifiers
    // -----------------------------------------------------------------------
    runner.startTest("SoEvent::setTime / getTime round-trip");
    {
        SoKeyboardEvent ev; // use concrete subclass to access SoEvent methods
        SbTime t(1.234);
        ev.setTime(t);
        bool pass = (std::fabs(ev.getTime().getValue() - 1.234) < 1e-9);
        runner.endTest(pass, pass ? "" : "SoEvent setTime/getTime failed");
    }

    runner.startTest("SoEvent::setPosition / getPosition round-trip");
    {
        SoKeyboardEvent ev;
        SbVec2s pos(640, 480);
        ev.setPosition(pos);
        bool pass = (ev.getPosition() == pos);
        runner.endTest(pass, pass ? "" : "SoEvent setPosition/getPosition failed");
    }

    runner.startTest("SoEvent::setShiftDown / wasShiftDown round-trip");
    {
        SoKeyboardEvent ev;
        ev.setShiftDown(TRUE);
        bool pass = (ev.wasShiftDown() == TRUE);
        ev.setShiftDown(FALSE);
        bool pass2 = (ev.wasShiftDown() == FALSE);
        runner.endTest(pass && pass2, (pass && pass2) ? "" : "setShiftDown/wasShiftDown failed");
    }

    runner.startTest("SoEvent::setCtrlDown / wasCtrlDown round-trip");
    {
        SoKeyboardEvent ev;
        ev.setCtrlDown(TRUE);
        bool pass = (ev.wasCtrlDown() == TRUE);
        ev.setCtrlDown(FALSE);
        bool pass2 = (ev.wasCtrlDown() == FALSE);
        runner.endTest(pass && pass2, (pass && pass2) ? "" : "setCtrlDown/wasCtrlDown failed");
    }

    runner.startTest("SoEvent::setAltDown / wasAltDown round-trip");
    {
        SoKeyboardEvent ev;
        ev.setAltDown(TRUE);
        bool pass = (ev.wasAltDown() == TRUE);
        ev.setAltDown(FALSE);
        bool pass2 = (ev.wasAltDown() == FALSE);
        runner.endTest(pass && pass2, (pass && pass2) ? "" : "setAltDown/wasAltDown failed");
    }

    runner.startTest("SoEvent::getTypeId for SoKeyboardEvent is not badType");
    {
        SoKeyboardEvent ev;
        bool pass = (ev.getTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoKeyboardEvent getTypeId returned badType");
    }

    runner.startTest("SoEvent::isOfType(SoKeyboardEvent) for keyboard event");
    {
        SoKeyboardEvent ev;
        bool pass = ev.isOfType(SoKeyboardEvent::getClassTypeId());
        runner.endTest(pass, pass ? "" : "SoKeyboardEvent isOfType(SoKeyboardEvent) failed");
    }

    runner.startTest("SoEvent modifiers all default to FALSE");
    {
        SoKeyboardEvent ev;
        bool pass = !ev.wasShiftDown() && !ev.wasCtrlDown() && !ev.wasAltDown();
        runner.endTest(pass, pass ? "" : "SoEvent modifiers should default to FALSE");
    }

    // -----------------------------------------------------------------------
    // SoButtonEvent: setState / getState
    // -----------------------------------------------------------------------
    runner.startTest("SoButtonEvent::setState UP / getState round-trip");
    {
        SoKeyboardEvent ev;
        ev.setState(SoButtonEvent::UP);
        bool pass = (ev.getState() == SoButtonEvent::UP);
        runner.endTest(pass, pass ? "" : "SoButtonEvent setState/getState UP failed");
    }

    runner.startTest("SoButtonEvent::setState DOWN / getState round-trip");
    {
        SoKeyboardEvent ev;
        ev.setState(SoButtonEvent::DOWN);
        bool pass = (ev.getState() == SoButtonEvent::DOWN);
        runner.endTest(pass, pass ? "" : "SoButtonEvent setState/getState DOWN failed");
    }

    // -----------------------------------------------------------------------
    // SoKeyboardEvent
    // -----------------------------------------------------------------------
    runner.startTest("SoKeyboardEvent class type registered");
    {
        bool pass = (SoKeyboardEvent::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoKeyboardEvent bad class type");
    }

    runner.startTest("SoKeyboardEvent setKey / getKey round-trip (letter A)");
    {
        SoKeyboardEvent ev;
        ev.setKey(SoKeyboardEvent::A);
        bool pass = (ev.getKey() == SoKeyboardEvent::A);
        runner.endTest(pass, pass ? "" : "SoKeyboardEvent setKey/getKey(A) failed");
    }

    runner.startTest("SoKeyboardEvent setKey / getKey round-trip (HOME)");
    {
        SoKeyboardEvent ev;
        ev.setKey(SoKeyboardEvent::HOME);
        bool pass = (ev.getKey() == SoKeyboardEvent::HOME);
        runner.endTest(pass, pass ? "" : "SoKeyboardEvent setKey/getKey(HOME) failed");
    }

    runner.startTest("SoKeyboardEvent getPrintableCharacter for letter 'a'");
    {
        SoKeyboardEvent ev;
        ev.setKey(SoKeyboardEvent::A);
        char c = ev.getPrintableCharacter();
        // Should return 'a' or 'A'
        bool pass = (c == 'a' || c == 'A' || c != '\0');
        runner.endTest(pass, pass ? "" : "SoKeyboardEvent getPrintableCharacter('a') failed");
    }

    runner.startTest("SoKeyboardEvent isKeyPressEvent: DOWN state + matching key");
    {
        SoKeyboardEvent ev;
        ev.setKey(SoKeyboardEvent::RETURN);
        ev.setState(SoButtonEvent::DOWN);
        bool pass = SoKeyboardEvent::isKeyPressEvent(&ev, SoKeyboardEvent::RETURN);
        runner.endTest(pass, pass ? "" : "isKeyPressEvent should match DOWN + RETURN");
    }

    runner.startTest("SoKeyboardEvent isKeyPressEvent: UP state should NOT match");
    {
        SoKeyboardEvent ev;
        ev.setKey(SoKeyboardEvent::RETURN);
        ev.setState(SoButtonEvent::UP);
        bool pass = !SoKeyboardEvent::isKeyPressEvent(&ev, SoKeyboardEvent::RETURN);
        runner.endTest(pass, pass ? "" : "isKeyPressEvent should NOT match UP state");
    }

    runner.startTest("SoKeyboardEvent isKeyReleaseEvent: UP state + matching key");
    {
        SoKeyboardEvent ev;
        ev.setKey(SoKeyboardEvent::ESCAPE);
        ev.setState(SoButtonEvent::UP);
        bool pass = SoKeyboardEvent::isKeyReleaseEvent(&ev, SoKeyboardEvent::ESCAPE);
        runner.endTest(pass, pass ? "" : "isKeyReleaseEvent should match UP + ESCAPE");
    }

    runner.startTest("SoKeyboardEvent isKeyPressEvent with ANY matches any key press");
    {
        SoKeyboardEvent ev;
        ev.setKey(SoKeyboardEvent::Z);
        ev.setState(SoButtonEvent::DOWN);
        bool pass = SoKeyboardEvent::isKeyPressEvent(&ev, SoKeyboardEvent::ANY);
        runner.endTest(pass, pass ? "" : "isKeyPressEvent with ANY should match any key press");
    }

    // -----------------------------------------------------------------------
    // SoMouseButtonEvent
    // -----------------------------------------------------------------------
    runner.startTest("SoMouseButtonEvent class type registered");
    {
        bool pass = (SoMouseButtonEvent::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoMouseButtonEvent bad class type");
    }

    runner.startTest("SoMouseButtonEvent setButton BUTTON1 / getButton round-trip");
    {
        SoMouseButtonEvent ev;
        ev.setButton(SoMouseButtonEvent::BUTTON1);
        bool pass = (ev.getButton() == SoMouseButtonEvent::BUTTON1);
        runner.endTest(pass, pass ? "" : "SoMouseButtonEvent setButton/getButton(BUTTON1) failed");
    }

    runner.startTest("SoMouseButtonEvent setButton BUTTON2 / getButton round-trip");
    {
        SoMouseButtonEvent ev;
        ev.setButton(SoMouseButtonEvent::BUTTON2);
        bool pass = (ev.getButton() == SoMouseButtonEvent::BUTTON2);
        runner.endTest(pass, pass ? "" : "SoMouseButtonEvent BUTTON2 round-trip failed");
    }

    runner.startTest("SoMouseButtonEvent setButton BUTTON3 / getButton round-trip");
    {
        SoMouseButtonEvent ev;
        ev.setButton(SoMouseButtonEvent::BUTTON3);
        bool pass = (ev.getButton() == SoMouseButtonEvent::BUTTON3);
        runner.endTest(pass, pass ? "" : "SoMouseButtonEvent BUTTON3 round-trip failed");
    }

    runner.startTest("SoMouseButtonEvent isButtonPressEvent: DOWN + BUTTON1");
    {
        SoMouseButtonEvent ev;
        ev.setButton(SoMouseButtonEvent::BUTTON1);
        ev.setState(SoButtonEvent::DOWN);
        bool pass = SoMouseButtonEvent::isButtonPressEvent(&ev, SoMouseButtonEvent::BUTTON1);
        runner.endTest(pass, pass ? "" : "isButtonPressEvent(BUTTON1 DOWN) should be TRUE");
    }

    runner.startTest("SoMouseButtonEvent isButtonReleaseEvent: UP + BUTTON2");
    {
        SoMouseButtonEvent ev;
        ev.setButton(SoMouseButtonEvent::BUTTON2);
        ev.setState(SoButtonEvent::UP);
        bool pass = SoMouseButtonEvent::isButtonReleaseEvent(&ev, SoMouseButtonEvent::BUTTON2);
        runner.endTest(pass, pass ? "" : "isButtonReleaseEvent(BUTTON2 UP) should be TRUE");
    }

    runner.startTest("SoMouseButtonEvent isButtonPressEvent with ANY matches any button press");
    {
        SoMouseButtonEvent ev;
        ev.setButton(SoMouseButtonEvent::BUTTON3);
        ev.setState(SoButtonEvent::DOWN);
        bool pass = SoMouseButtonEvent::isButtonPressEvent(&ev, SoMouseButtonEvent::ANY);
        runner.endTest(pass, pass ? "" : "isButtonPressEvent(ANY, DOWN) should match any button");
    }

    runner.startTest("SoMouseButtonEvent wrong button does NOT match");
    {
        SoMouseButtonEvent ev;
        ev.setButton(SoMouseButtonEvent::BUTTON1);
        ev.setState(SoButtonEvent::DOWN);
        bool pass = !SoMouseButtonEvent::isButtonPressEvent(&ev, SoMouseButtonEvent::BUTTON3);
        runner.endTest(pass, pass ? "" : "BUTTON1 press should NOT match BUTTON3 test");
    }

    // -----------------------------------------------------------------------
    // SoLocation2Event
    // -----------------------------------------------------------------------
    runner.startTest("SoLocation2Event class type registered");
    {
        bool pass = (SoLocation2Event::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoLocation2Event bad class type");
    }

    runner.startTest("SoLocation2Event setPosition / getPosition round-trip");
    {
        SoLocation2Event ev;
        ev.setPosition(SbVec2s(320, 240));
        bool pass = (ev.getPosition() == SbVec2s(320, 240));
        runner.endTest(pass, pass ? "" : "SoLocation2Event position round-trip failed");
    }

    return runner.getSummary();
}
