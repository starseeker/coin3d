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

using namespace SimpleTest;

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

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // -----------------------------------------------------------------------
    // SoKeyboardEvent: set/get key, state, position, shift modifier
    // -----------------------------------------------------------------------
    runner.startTest("SoKeyboardEvent set/get key and state");
    {
        SoKeyboardEvent evt;
        evt.setKey(SoKeyboardEvent::A);
        evt.setState(SoButtonEvent::DOWN);
        bool pass = (evt.getKey() == SoKeyboardEvent::A) &&
                    (evt.getState() == SoButtonEvent::DOWN);
        runner.endTest(pass, pass ? "" : "SoKeyboardEvent key or state mismatch");
    }

    runner.startTest("SoKeyboardEvent setPosition / getPosition");
    {
        SoKeyboardEvent evt;
        evt.setPosition(SbVec2s(100, 200));
        const SbVec2s & pos = evt.getPosition();
        bool pass = (pos[0] == 100) && (pos[1] == 200);
        runner.endTest(pass, pass ? "" : "SoKeyboardEvent position mismatch");
    }

    runner.startTest("SoKeyboardEvent wasShiftDown default false");
    {
        SoKeyboardEvent evt;
        // No shift modifier set — should be false
        bool pass = (evt.wasShiftDown() == FALSE);
        runner.endTest(pass, pass ? "" : "SoKeyboardEvent wasShiftDown should default to false");
    }

    // -----------------------------------------------------------------------
    // SoMouseButtonEvent: button, state, isButtonPressEvent
    // -----------------------------------------------------------------------
    runner.startTest("SoMouseButtonEvent set/get button and state");
    {
        SoMouseButtonEvent evt;
        evt.setButton(SoMouseButtonEvent::BUTTON1);
        evt.setState(SoButtonEvent::DOWN);
        bool pass = (evt.getButton() == SoMouseButtonEvent::BUTTON1) &&
                    (evt.getState() == SoButtonEvent::DOWN);
        runner.endTest(pass, pass ? "" : "SoMouseButtonEvent button or state mismatch");
    }

    runner.startTest("SoMouseButtonEvent isButtonPressEvent");
    {
        SoMouseButtonEvent evt;
        evt.setButton(SoMouseButtonEvent::BUTTON1);
        evt.setState(SoButtonEvent::DOWN);
        bool pass = SoMouseButtonEvent::isButtonPressEvent(
                        &evt, SoMouseButtonEvent::BUTTON1) == TRUE;
        runner.endTest(pass, pass ? "" : "SoMouseButtonEvent::isButtonPressEvent failed");
    }

    // -----------------------------------------------------------------------
    // SoLocation2Event: setPosition / getPosition
    // -----------------------------------------------------------------------
    runner.startTest("SoLocation2Event setPosition / getPosition");
    {
        SoLocation2Event evt;
        evt.setPosition(SbVec2s(42, 17));
        const SbVec2s & pos = evt.getPosition();
        bool pass = (pos[0] == 42) && (pos[1] == 17);
        runner.endTest(pass, pass ? "" : "SoLocation2Event position mismatch");
    }

    // -----------------------------------------------------------------------
    // SoMotion3Event: class initialized
    // -----------------------------------------------------------------------
    runner.startTest("SoMotion3Event class initialized");
    {
        SoMotion3Event evt;
        bool pass = (evt.getTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoMotion3Event has bad type");
    }

    // -----------------------------------------------------------------------
    // SoSpaceballButtonEvent: class initialized
    // -----------------------------------------------------------------------
    runner.startTest("SoSpaceballButtonEvent class initialized");
    {
        SoSpaceballButtonEvent evt;
        bool pass = (evt.getTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoSpaceballButtonEvent has bad type");
    }

    // -----------------------------------------------------------------------
    // SoEventCallback: no callback registered — event not handled
    // -----------------------------------------------------------------------
    runner.startTest("SoHandleEventAction event not handled without callback");
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
        runner.endTest(pass, pass ? "" :
            "Event should not be handled when no callback is registered");
    }

    // -----------------------------------------------------------------------
    // SoEventCallback: registered callback fires on matching event type
    // -----------------------------------------------------------------------
    runner.startTest("SoEventCallback addEventCallback fires on matching event");
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
        runner.endTest(pass, pass ? "" :
            "SoEventCallback did not fire for matching event type");
    }

    // -----------------------------------------------------------------------
    // SoEventCallback: callback does NOT fire for mismatched event type
    // -----------------------------------------------------------------------
    runner.startTest("SoEventCallback does not fire for mismatched event type");
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
        runner.endTest(pass, pass ? "" :
            "SoEventCallback fired for wrong event type");
    }

    return runner.getSummary();
}
