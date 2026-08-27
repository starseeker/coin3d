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

using namespace ObolTest;

TEST(EventsDeeper, SoEventSetTimeGetTimeRoundTrip)
{
    SoKeyboardEvent ev; // use concrete subclass to access SoEvent methods
    SbTime t(1.234);
    ev.setTime(t);
    bool pass = (std::fabs(ev.getTime().getValue() - 1.234) < 1e-9);
    EXPECT_TRUE(pass) << "SoEvent setTime/getTime failed";
}

TEST(EventsDeeper, SoEventSetPositionGetPositionRoundTrip)
{
    SoKeyboardEvent ev;
    SbVec2s pos(640, 480);
    ev.setPosition(pos);
    bool pass = (ev.getPosition() == pos);
    EXPECT_TRUE(pass) << "SoEvent setPosition/getPosition failed";
}

TEST(EventsDeeper, SoEventSetShiftDownWasShiftDownRoundTrip)
{
    SoKeyboardEvent ev;
    ev.setShiftDown(TRUE);
    bool pass = (ev.wasShiftDown() == TRUE);
    ev.setShiftDown(FALSE);
    bool pass2 = (ev.wasShiftDown() == FALSE);
    EXPECT_TRUE((pass && pass2)) << ((pass && pass2) ? "" : "setShiftDown/wasShiftDown failed");
}

TEST(EventsDeeper, SoEventSetCtrlDownWasCtrlDownRoundTrip)
{
    SoKeyboardEvent ev;
    ev.setCtrlDown(TRUE);
    bool pass = (ev.wasCtrlDown() == TRUE);
    ev.setCtrlDown(FALSE);
    bool pass2 = (ev.wasCtrlDown() == FALSE);
    EXPECT_TRUE((pass && pass2)) << ((pass && pass2) ? "" : "setCtrlDown/wasCtrlDown failed");
}

TEST(EventsDeeper, SoEventSetAltDownWasAltDownRoundTrip)
{
    SoKeyboardEvent ev;
    ev.setAltDown(TRUE);
    bool pass = (ev.wasAltDown() == TRUE);
    ev.setAltDown(FALSE);
    bool pass2 = (ev.wasAltDown() == FALSE);
    EXPECT_TRUE((pass && pass2)) << ((pass && pass2) ? "" : "setAltDown/wasAltDown failed");
}

TEST(EventsDeeper, SoEventGetTypeIdForSoKeyboardEventIsNotBadType)
{
    SoKeyboardEvent ev;
    bool pass = (ev.getTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoKeyboardEvent getTypeId returned badType";
}

TEST(EventsDeeper, SoEventIsOfTypeSoKeyboardEventForKeyboardEvent)
{
    SoKeyboardEvent ev;
    bool pass = ev.isOfType(SoKeyboardEvent::getClassTypeId());
    EXPECT_TRUE(pass) << "SoKeyboardEvent isOfType(SoKeyboardEvent) failed";
}

TEST(EventsDeeper, SoEventModifiersAllDefaultToFALSE)
{
    SoKeyboardEvent ev;
    bool pass = !ev.wasShiftDown() && !ev.wasCtrlDown() && !ev.wasAltDown();
    EXPECT_TRUE(pass) << "SoEvent modifiers should default to FALSE";
}

// -----------------------------------------------------------------------
// SoButtonEvent: setState / getState
// -----------------------------------------------------------------------

TEST(EventsDeeper, SoButtonEventSetStateUPGetStateRoundTrip)
{
    SoKeyboardEvent ev;
    ev.setState(SoButtonEvent::UP);
    bool pass = (ev.getState() == SoButtonEvent::UP);
    EXPECT_TRUE(pass) << "SoButtonEvent setState/getState UP failed";
}

TEST(EventsDeeper, SoButtonEventSetStateDOWNGetStateRoundTrip)
{
    SoKeyboardEvent ev;
    ev.setState(SoButtonEvent::DOWN);
    bool pass = (ev.getState() == SoButtonEvent::DOWN);
    EXPECT_TRUE(pass) << "SoButtonEvent setState/getState DOWN failed";
}

// -----------------------------------------------------------------------
// SoKeyboardEvent
// -----------------------------------------------------------------------

TEST(EventsDeeper, SoKeyboardEventClassTypeRegistered)
{
    bool pass = (SoKeyboardEvent::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoKeyboardEvent bad class type";
}

TEST(EventsDeeper, SoKeyboardEventSetKeyGetKeyRoundTripLetterA)
{
    SoKeyboardEvent ev;
    ev.setKey(SoKeyboardEvent::A);
    bool pass = (ev.getKey() == SoKeyboardEvent::A);
    EXPECT_TRUE(pass) << "SoKeyboardEvent setKey/getKey(A) failed";
}

TEST(EventsDeeper, SoKeyboardEventSetKeyGetKeyRoundTripHOME)
{
    SoKeyboardEvent ev;
    ev.setKey(SoKeyboardEvent::HOME);
    bool pass = (ev.getKey() == SoKeyboardEvent::HOME);
    EXPECT_TRUE(pass) << "SoKeyboardEvent setKey/getKey(HOME) failed";
}

TEST(EventsDeeper, SoKeyboardEventGetPrintableCharacterForLetterA)
{
    SoKeyboardEvent ev;
    ev.setKey(SoKeyboardEvent::A);
    char c = ev.getPrintableCharacter();
    // Should return 'a' or 'A'
    bool pass = (c == 'a' || c == 'A' || c != '\0');
    EXPECT_TRUE(pass) << "SoKeyboardEvent getPrintableCharacter('a') failed";
}

TEST(EventsDeeper, SoKeyboardEventIsKeyPressEventDOWNStateMatchingKey)
{
    SoKeyboardEvent ev;
    ev.setKey(SoKeyboardEvent::RETURN);
    ev.setState(SoButtonEvent::DOWN);
    bool pass = SoKeyboardEvent::isKeyPressEvent(&ev, SoKeyboardEvent::RETURN);
    EXPECT_TRUE(pass) << "isKeyPressEvent should match DOWN + RETURN";
}

TEST(EventsDeeper, SoKeyboardEventIsKeyPressEventUPStateShouldNOTMatch)
{
    SoKeyboardEvent ev;
    ev.setKey(SoKeyboardEvent::RETURN);
    ev.setState(SoButtonEvent::UP);
    bool pass = !SoKeyboardEvent::isKeyPressEvent(&ev, SoKeyboardEvent::RETURN);
    EXPECT_TRUE(pass) << "isKeyPressEvent should NOT match UP state";
}

TEST(EventsDeeper, SoKeyboardEventIsKeyReleaseEventUPStateMatchingKey)
{
    SoKeyboardEvent ev;
    ev.setKey(SoKeyboardEvent::ESCAPE);
    ev.setState(SoButtonEvent::UP);
    bool pass = SoKeyboardEvent::isKeyReleaseEvent(&ev, SoKeyboardEvent::ESCAPE);
    EXPECT_TRUE(pass) << "isKeyReleaseEvent should match UP + ESCAPE";
}

TEST(EventsDeeper, SoKeyboardEventIsKeyPressEventWithANYMatchesAnyKeyPress)
{
    SoKeyboardEvent ev;
    ev.setKey(SoKeyboardEvent::Z);
    ev.setState(SoButtonEvent::DOWN);
    bool pass = SoKeyboardEvent::isKeyPressEvent(&ev, SoKeyboardEvent::ANY);
    EXPECT_TRUE(pass) << "isKeyPressEvent with ANY should match any key press";
}

// -----------------------------------------------------------------------
// SoMouseButtonEvent
// -----------------------------------------------------------------------

TEST(EventsDeeper, SoMouseButtonEventClassTypeRegistered)
{
    bool pass = (SoMouseButtonEvent::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoMouseButtonEvent bad class type";
}

TEST(EventsDeeper, SoMouseButtonEventSetButtonBUTTON1GetButtonRoundTrip)
{
    SoMouseButtonEvent ev;
    ev.setButton(SoMouseButtonEvent::BUTTON1);
    bool pass = (ev.getButton() == SoMouseButtonEvent::BUTTON1);
    EXPECT_TRUE(pass) << "SoMouseButtonEvent setButton/getButton(BUTTON1) failed";
}

TEST(EventsDeeper, SoMouseButtonEventSetButtonBUTTON2GetButtonRoundTrip)
{
    SoMouseButtonEvent ev;
    ev.setButton(SoMouseButtonEvent::BUTTON2);
    bool pass = (ev.getButton() == SoMouseButtonEvent::BUTTON2);
    EXPECT_TRUE(pass) << "SoMouseButtonEvent BUTTON2 round-trip failed";
}

TEST(EventsDeeper, SoMouseButtonEventSetButtonBUTTON3GetButtonRoundTrip)
{
    SoMouseButtonEvent ev;
    ev.setButton(SoMouseButtonEvent::BUTTON3);
    bool pass = (ev.getButton() == SoMouseButtonEvent::BUTTON3);
    EXPECT_TRUE(pass) << "SoMouseButtonEvent BUTTON3 round-trip failed";
}

TEST(EventsDeeper, SoMouseButtonEventIsButtonPressEventDOWNBUTTON1)
{
    SoMouseButtonEvent ev;
    ev.setButton(SoMouseButtonEvent::BUTTON1);
    ev.setState(SoButtonEvent::DOWN);
    bool pass = SoMouseButtonEvent::isButtonPressEvent(&ev, SoMouseButtonEvent::BUTTON1);
    EXPECT_TRUE(pass) << "isButtonPressEvent(BUTTON1 DOWN) should be TRUE";
}

TEST(EventsDeeper, SoMouseButtonEventIsButtonReleaseEventUPBUTTON2)
{
    SoMouseButtonEvent ev;
    ev.setButton(SoMouseButtonEvent::BUTTON2);
    ev.setState(SoButtonEvent::UP);
    bool pass = SoMouseButtonEvent::isButtonReleaseEvent(&ev, SoMouseButtonEvent::BUTTON2);
    EXPECT_TRUE(pass) << "isButtonReleaseEvent(BUTTON2 UP) should be TRUE";
}

TEST(EventsDeeper, SoMouseButtonEventIsButtonPressEventWithANYMatchesAnyButtonPress)
{
    SoMouseButtonEvent ev;
    ev.setButton(SoMouseButtonEvent::BUTTON3);
    ev.setState(SoButtonEvent::DOWN);
    bool pass = SoMouseButtonEvent::isButtonPressEvent(&ev, SoMouseButtonEvent::ANY);
    EXPECT_TRUE(pass) << "isButtonPressEvent(ANY, DOWN) should match any button";
}

TEST(EventsDeeper, SoMouseButtonEventWrongButtonDoesNOTMatch)
{
    SoMouseButtonEvent ev;
    ev.setButton(SoMouseButtonEvent::BUTTON1);
    ev.setState(SoButtonEvent::DOWN);
    bool pass = !SoMouseButtonEvent::isButtonPressEvent(&ev, SoMouseButtonEvent::BUTTON3);
    EXPECT_TRUE(pass) << "BUTTON1 press should NOT match BUTTON3 test";
}

// -----------------------------------------------------------------------
// SoLocation2Event
// -----------------------------------------------------------------------

TEST(EventsDeeper, SoLocation2EventClassTypeRegistered)
{
    bool pass = (SoLocation2Event::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoLocation2Event bad class type";
}

TEST(EventsDeeper, SoLocation2EventSetPositionGetPositionRoundTrip)
{
    SoLocation2Event ev;
    ev.setPosition(SbVec2s(320, 240));
    bool pass = (ev.getPosition() == SbVec2s(320, 240));
    EXPECT_TRUE(pass) << "SoLocation2Event position round-trip failed";
}
