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
 * @file test_events_suite.cpp
 * @brief Tests for SoEvent subclasses and SoEventCallback dispatch.
 *
 * Covers:
 *   SoKeyboardEvent    - key, state, position, shift modifier
 *   SoMouseButtonEvent - button, state, isButtonPressEvent
 *   SoLocation2Event   - position
 *   SoMotion3Event     - class initialization
 *   SoSpaceballButtonEvent - class initialization
 *   SoEventCallback    - addEventCallback, dispatch via SoHandleEventAction
 */

#include "../test_utils.h"

#include <Inventor/events/SoEvent.h>
#include <Inventor/events/SoButtonEvent.h>
#include <Inventor/events/SoKeyboardEvent.h>
#include <Inventor/events/SoMouseButtonEvent.h>
#include <Inventor/events/SoLocation2Event.h>
#include <Inventor/events/SoMotion3Event.h>
#include <Inventor/events/SoSpaceballButtonEvent.h>
#include <Inventor/nodes/SoEventCallback.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/actions/SoHandleEventAction.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoType.h>

using namespace ObolTest;

// Callback data for dispatch tests
struct EventCapture {
    bool fired;
    EventCapture() : fired(false) {}
};

static void keyboardEventCb(void * userdata, SoEventCallback * /*node*/)
{
    EventCapture * cap = static_cast<EventCapture *>(userdata);
    cap->fired = true;
}

TEST(EventsSuite, SoKeyboardEventSetGetKeyAndState)
{
    SoKeyboardEvent evt;
    evt.setKey(SoKeyboardEvent::A);
    evt.setState(SoButtonEvent::DOWN);
    bool pass = (evt.getKey() == SoKeyboardEvent::A) &&
                (evt.getState() == SoButtonEvent::DOWN);
    EXPECT_TRUE(pass) << "SoKeyboardEvent key or state mismatch";
}

TEST(EventsSuite, SoKeyboardEventSetPositionGetPosition)
{
    SoKeyboardEvent evt;
    evt.setPosition(SbVec2s(100, 200));
    const SbVec2s & pos = evt.getPosition();
    bool pass = (pos[0] == 100) && (pos[1] == 200);
    EXPECT_TRUE(pass) << "SoKeyboardEvent position mismatch";
}

TEST(EventsSuite, SoKeyboardEventWasShiftDownDefaultFalse)
{
    SoKeyboardEvent evt;
    // No shift modifier set — should be false
    bool pass = (evt.wasShiftDown() == FALSE);
    EXPECT_TRUE(pass) << "SoKeyboardEvent wasShiftDown should default to false";
}

// -----------------------------------------------------------------------
// SoMouseButtonEvent: button, state, isButtonPressEvent
// -----------------------------------------------------------------------

TEST(EventsSuite, SoMouseButtonEventSetGetButtonAndState)
{
    SoMouseButtonEvent evt;
    evt.setButton(SoMouseButtonEvent::BUTTON1);
    evt.setState(SoButtonEvent::DOWN);
    bool pass = (evt.getButton() == SoMouseButtonEvent::BUTTON1) &&
                (evt.getState() == SoButtonEvent::DOWN);
    EXPECT_TRUE(pass) << "SoMouseButtonEvent button or state mismatch";
}

TEST(EventsSuite, SoMouseButtonEventIsButtonPressEvent)
{
    SoMouseButtonEvent evt;
    evt.setButton(SoMouseButtonEvent::BUTTON1);
    evt.setState(SoButtonEvent::DOWN);
    bool pass = SoMouseButtonEvent::isButtonPressEvent(
                    &evt, SoMouseButtonEvent::BUTTON1) == TRUE;
    EXPECT_TRUE(pass) << "SoMouseButtonEvent::isButtonPressEvent failed";
}

// -----------------------------------------------------------------------
// SoLocation2Event: setPosition / getPosition
// -----------------------------------------------------------------------

TEST(EventsSuite, SoLocation2EventSetPositionGetPosition)
{
    SoLocation2Event evt;
    evt.setPosition(SbVec2s(42, 17));
    const SbVec2s & pos = evt.getPosition();
    bool pass = (pos[0] == 42) && (pos[1] == 17);
    EXPECT_TRUE(pass) << "SoLocation2Event position mismatch";
}

// -----------------------------------------------------------------------
// SoMotion3Event: class initialized
// -----------------------------------------------------------------------

TEST(EventsSuite, SoMotion3EventClassInitialized)
{
    SoMotion3Event evt;
    bool pass = (evt.getTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoMotion3Event has bad type";
}

// -----------------------------------------------------------------------
// SoSpaceballButtonEvent: class initialized
// -----------------------------------------------------------------------

TEST(EventsSuite, SoSpaceballButtonEventClassInitialized)
{
    SoSpaceballButtonEvent evt;
    bool pass = (evt.getTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoSpaceballButtonEvent has bad type";
}

// -----------------------------------------------------------------------
// SoEventCallback: no callback registered — event not handled
// -----------------------------------------------------------------------

TEST(EventsSuite, SoHandleEventActionEventNotHandledWithoutCallback)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    SoEventCallback * ecb = new SoEventCallback;
    root->addChild(ecb);

    SoKeyboardEvent evt;
    evt.setKey(SoKeyboardEvent::A);
    evt.setState(SoButtonEvent::DOWN);

    SbViewportRegion vp(256, 256);
    SoHandleEventAction action(vp);
    action.setEvent(&evt);
    action.apply(root);

    bool pass = (action.isHandled() == FALSE);
    root->unref();
    EXPECT_TRUE(pass) << "Event should not be handled when no callback is registered";
}

// -----------------------------------------------------------------------
// SoEventCallback: registered callback fires on matching event type
// -----------------------------------------------------------------------

TEST(EventsSuite, SoEventCallbackAddEventCallbackFiresOnMatchingEvent)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    SoEventCallback * ecb = new SoEventCallback;

    EventCapture cap;
    ecb->addEventCallback(SoKeyboardEvent::getClassTypeId(),
                          keyboardEventCb, &cap);
    root->addChild(ecb);

    SoKeyboardEvent evt;
    evt.setKey(SoKeyboardEvent::A);
    evt.setState(SoButtonEvent::DOWN);

    SbViewportRegion vp(256, 256);
    SoHandleEventAction action(vp);
    action.setEvent(&evt);
    action.apply(root);

    bool pass = cap.fired;
    root->unref();
    EXPECT_TRUE(pass) << "SoEventCallback did not fire for matching event type";
}

// -----------------------------------------------------------------------
// SoEventCallback: callback does NOT fire for mismatched event type
// -----------------------------------------------------------------------

TEST(EventsSuite, SoEventCallbackDoesNotFireForMismatchedEventType)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    SoEventCallback * ecb = new SoEventCallback;

    EventCapture cap;
    // Register only for mouse button events
    ecb->addEventCallback(SoMouseButtonEvent::getClassTypeId(),
                          keyboardEventCb, &cap);
    root->addChild(ecb);

    // Dispatch a keyboard event — should not trigger mouse callback
    SoKeyboardEvent evt;
    evt.setKey(SoKeyboardEvent::B);
    evt.setState(SoButtonEvent::DOWN);

    SbViewportRegion vp(256, 256);
    SoHandleEventAction action(vp);
    action.setEvent(&evt);
    action.apply(root);

    bool pass = !cap.fired;
    root->unref();
    EXPECT_TRUE(pass) << "SoEventCallback fired for wrong event type";
}
