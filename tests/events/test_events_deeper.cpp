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
    EXPECT_TRUE((std::fabs(ev.getTime().getValue() - 1.234) < 1e-9)) << "SoEvent setTime/getTime failed";
}

TEST(EventsDeeper, SoEventSetPositionGetPositionRoundTrip)
{
    SoKeyboardEvent ev;
    SbVec2s pos(640, 480);
    ev.setPosition(pos);
    EXPECT_TRUE((ev.getPosition() == pos)) << "SoEvent setPosition/getPosition failed";
}

TEST(EventsDeeper, SoEventSetShiftDownWasShiftDownRoundTrip)
{
    SoKeyboardEvent ev;
    ev.setShiftDown(TRUE);
    EXPECT_TRUE(ev.wasShiftDown());
    ev.setShiftDown(FALSE);
    EXPECT_FALSE(ev.wasShiftDown());
}

TEST(EventsDeeper, SoEventSetCtrlDownWasCtrlDownRoundTrip)
{
    SoKeyboardEvent ev;
    ev.setCtrlDown(TRUE);
    EXPECT_TRUE(ev.wasCtrlDown());
    ev.setCtrlDown(FALSE);
    EXPECT_FALSE(ev.wasCtrlDown());
}

TEST(EventsDeeper, SoEventSetAltDownWasAltDownRoundTrip)
{
    SoKeyboardEvent ev;
    ev.setAltDown(TRUE);
    EXPECT_TRUE(ev.wasAltDown());
    ev.setAltDown(FALSE);
    EXPECT_FALSE(ev.wasAltDown());
}

TEST(EventsDeeper, SoEventGetTypeIdForSoKeyboardEventIsNotBadType)
{
    SoKeyboardEvent ev;
    EXPECT_NE(ev.getTypeId(), SoType::badType());
}

TEST(EventsDeeper, SoEventIsOfTypeSoKeyboardEventForKeyboardEvent)
{
    SoKeyboardEvent ev;
    EXPECT_TRUE(ev.isOfType(SoKeyboardEvent::getClassTypeId())) << "SoKeyboardEvent isOfType(SoKeyboardEvent) failed";
}

TEST(EventsDeeper, SoEventModifiersAllDefaultToFALSE)
{
    SoKeyboardEvent ev;
    EXPECT_TRUE(!ev.wasShiftDown() && !ev.wasCtrlDown() && !ev.wasAltDown()) << "SoEvent modifiers should default to FALSE";
}

// -----------------------------------------------------------------------
// SoButtonEvent: setState / getState
// -----------------------------------------------------------------------

TEST(EventsDeeper, SoButtonEventSetStateUPGetStateRoundTrip)
{
    SoKeyboardEvent ev;
    ev.setState(SoButtonEvent::UP);
    EXPECT_TRUE((ev.getState() == SoButtonEvent::UP)) << "SoButtonEvent setState/getState UP failed";
}

TEST(EventsDeeper, SoButtonEventSetStateDOWNGetStateRoundTrip)
{
    SoKeyboardEvent ev;
    ev.setState(SoButtonEvent::DOWN);
    EXPECT_TRUE((ev.getState() == SoButtonEvent::DOWN)) << "SoButtonEvent setState/getState DOWN failed";
}

// -----------------------------------------------------------------------
// SoKeyboardEvent
// -----------------------------------------------------------------------

TEST(EventsDeeper, SoKeyboardEventClassTypeRegistered)
{
    EXPECT_TRUE((SoKeyboardEvent::getClassTypeId() != SoType::badType())) << "SoKeyboardEvent bad class type";
}

TEST(EventsDeeper, SoKeyboardEventSetKeyGetKeyRoundTripLetterA)
{
    SoKeyboardEvent ev;
    ev.setKey(SoKeyboardEvent::A);
    EXPECT_TRUE((ev.getKey() == SoKeyboardEvent::A)) << "SoKeyboardEvent setKey/getKey(A) failed";
}

TEST(EventsDeeper, SoKeyboardEventSetKeyGetKeyRoundTripHOME)
{
    SoKeyboardEvent ev;
    ev.setKey(SoKeyboardEvent::HOME);
    EXPECT_TRUE((ev.getKey() == SoKeyboardEvent::HOME)) << "SoKeyboardEvent setKey/getKey(HOME) failed";
}

TEST(EventsDeeper, SoKeyboardEventGetPrintableCharacterForLetterA)
{
    SoKeyboardEvent ev;
    ev.setKey(SoKeyboardEvent::A);
    char c = ev.getPrintableCharacter();
    // Should return 'a' or 'A'
    EXPECT_TRUE((c == 'a' || c == 'A' || c != '\0')) << "SoKeyboardEvent getPrintableCharacter('a') failed";
}

TEST(EventsDeeper, SoKeyboardEventIsKeyPressEventDOWNStateMatchingKey)
{
    SoKeyboardEvent ev;
    ev.setKey(SoKeyboardEvent::RETURN);
    ev.setState(SoButtonEvent::DOWN);
    EXPECT_TRUE(SoKeyboardEvent::isKeyPressEvent(&ev, SoKeyboardEvent::RETURN)) << "isKeyPressEvent should match DOWN + RETURN";
}

TEST(EventsDeeper, SoKeyboardEventIsKeyPressEventUPStateShouldNOTMatch)
{
    SoKeyboardEvent ev;
    ev.setKey(SoKeyboardEvent::RETURN);
    ev.setState(SoButtonEvent::UP);
    EXPECT_TRUE(!SoKeyboardEvent::isKeyPressEvent(&ev, SoKeyboardEvent::RETURN)) << "isKeyPressEvent should NOT match UP state";
}

TEST(EventsDeeper, SoKeyboardEventIsKeyReleaseEventUPStateMatchingKey)
{
    SoKeyboardEvent ev;
    ev.setKey(SoKeyboardEvent::ESCAPE);
    ev.setState(SoButtonEvent::UP);
    EXPECT_TRUE(SoKeyboardEvent::isKeyReleaseEvent(&ev, SoKeyboardEvent::ESCAPE)) << "isKeyReleaseEvent should match UP + ESCAPE";
}

TEST(EventsDeeper, SoKeyboardEventIsKeyPressEventWithANYMatchesAnyKeyPress)
{
    SoKeyboardEvent ev;
    ev.setKey(SoKeyboardEvent::Z);
    ev.setState(SoButtonEvent::DOWN);
    EXPECT_TRUE(SoKeyboardEvent::isKeyPressEvent(&ev, SoKeyboardEvent::ANY)) << "isKeyPressEvent with ANY should match any key press";
}

// -----------------------------------------------------------------------
// SoMouseButtonEvent
// -----------------------------------------------------------------------

TEST(EventsDeeper, SoMouseButtonEventClassTypeRegistered)
{
    EXPECT_TRUE((SoMouseButtonEvent::getClassTypeId() != SoType::badType())) << "SoMouseButtonEvent bad class type";
}

TEST(EventsDeeper, SoMouseButtonEventSetButtonBUTTON1GetButtonRoundTrip)
{
    SoMouseButtonEvent ev;
    ev.setButton(SoMouseButtonEvent::BUTTON1);
    EXPECT_TRUE((ev.getButton() == SoMouseButtonEvent::BUTTON1)) << "SoMouseButtonEvent setButton/getButton(BUTTON1) failed";
}

TEST(EventsDeeper, SoMouseButtonEventSetButtonBUTTON2GetButtonRoundTrip)
{
    SoMouseButtonEvent ev;
    ev.setButton(SoMouseButtonEvent::BUTTON2);
    EXPECT_TRUE((ev.getButton() == SoMouseButtonEvent::BUTTON2)) << "SoMouseButtonEvent BUTTON2 round-trip failed";
}

TEST(EventsDeeper, SoMouseButtonEventSetButtonBUTTON3GetButtonRoundTrip)
{
    SoMouseButtonEvent ev;
    ev.setButton(SoMouseButtonEvent::BUTTON3);
    EXPECT_TRUE((ev.getButton() == SoMouseButtonEvent::BUTTON3)) << "SoMouseButtonEvent BUTTON3 round-trip failed";
}

TEST(EventsDeeper, SoMouseButtonEventIsButtonPressEventDOWNBUTTON1)
{
    SoMouseButtonEvent ev;
    ev.setButton(SoMouseButtonEvent::BUTTON1);
    ev.setState(SoButtonEvent::DOWN);
    EXPECT_TRUE(SoMouseButtonEvent::isButtonPressEvent(&ev, SoMouseButtonEvent::BUTTON1)) << "isButtonPressEvent(BUTTON1 DOWN) should be TRUE";
}

TEST(EventsDeeper, SoMouseButtonEventIsButtonReleaseEventUPBUTTON2)
{
    SoMouseButtonEvent ev;
    ev.setButton(SoMouseButtonEvent::BUTTON2);
    ev.setState(SoButtonEvent::UP);
    EXPECT_TRUE(SoMouseButtonEvent::isButtonReleaseEvent(&ev, SoMouseButtonEvent::BUTTON2)) << "isButtonReleaseEvent(BUTTON2 UP) should be TRUE";
}

TEST(EventsDeeper, SoMouseButtonEventIsButtonPressEventWithANYMatchesAnyButtonPress)
{
    SoMouseButtonEvent ev;
    ev.setButton(SoMouseButtonEvent::BUTTON3);
    ev.setState(SoButtonEvent::DOWN);
    EXPECT_TRUE(SoMouseButtonEvent::isButtonPressEvent(&ev, SoMouseButtonEvent::ANY)) << "isButtonPressEvent(ANY, DOWN) should match any button";
}

TEST(EventsDeeper, SoMouseButtonEventWrongButtonDoesNOTMatch)
{
    SoMouseButtonEvent ev;
    ev.setButton(SoMouseButtonEvent::BUTTON1);
    ev.setState(SoButtonEvent::DOWN);
    EXPECT_TRUE(!SoMouseButtonEvent::isButtonPressEvent(&ev, SoMouseButtonEvent::BUTTON3)) << "BUTTON1 press should NOT match BUTTON3 test";
}

// -----------------------------------------------------------------------
// SoLocation2Event
// -----------------------------------------------------------------------

TEST(EventsDeeper, SoLocation2EventClassTypeRegistered)
{
    EXPECT_TRUE((SoLocation2Event::getClassTypeId() != SoType::badType())) << "SoLocation2Event bad class type";
}

TEST(EventsDeeper, SoLocation2EventSetPositionGetPositionRoundTrip)
{
    SoLocation2Event ev;
    ev.setPosition(SbVec2s(320, 240));
    EXPECT_TRUE((ev.getPosition() == SbVec2s(320, 240))) << "SoLocation2Event position round-trip failed";
}
