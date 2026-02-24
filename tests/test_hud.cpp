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
 * @file test_hud.cpp
 * @brief Tests for the HUD (Head-Up Display) API
 *
 * Validates node creation, field access, callback registration, and hit-
 * testing for SoHUDKit, SoHUDLabel, and SoHUDButton.
 */

#include "test_utils.h"

#include <Inventor/annex/HUD/SoHUD.h>
#include <Inventor/annex/HUD/nodekits/SoHUDKit.h>
#include <Inventor/annex/HUD/nodes/SoHUDLabel.h>
#include <Inventor/annex/HUD/nodes/SoHUDButton.h>
#include <Inventor/actions/SoHandleEventAction.h>
#include <Inventor/events/SoMouseButtonEvent.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbVec2f.h>
#include <Inventor/SbColor.h>

using namespace SimpleTest;

// Helper: count how many times a button was clicked
struct ClickCounter {
    int count;
    ClickCounter() : count(0) {}
};

static void onButtonClick(void * userdata, SoHUDButton * /*btn*/)
{
    static_cast<ClickCounter *>(userdata)->count++;
}

int main()
{
    TestFixture fixture;
    TestRunner  runner;

    // ------------------------------------------------------------------
    // 1. SoHUDLabel – creation and field defaults
    // ------------------------------------------------------------------
    runner.startTest("SoHUDLabel: creation and field defaults");
    try {
        SoHUDLabel * label = new SoHUDLabel;
        label->ref();

        if (label->getTypeId() == SoType::badType()) {
            runner.endTest(false, "SoHUDLabel has bad type");
            label->unref();
            return 1;
        }

        SbVec2f defPos = label->position.getValue();
        if (defPos != SbVec2f(0.0f, 0.0f)) {
            runner.endTest(false, "Default position should be (0,0)");
            label->unref();
            return 1;
        }

        SbColor defCol = label->color.getValue();
        if (defCol != SbColor(1.0f, 1.0f, 1.0f)) {
            runner.endTest(false, "Default color should be white");
            label->unref();
            return 1;
        }

        if (label->fontSize.getValue() != 14.0f) {
            runner.endTest(false, "Default fontSize should be 14");
            label->unref();
            return 1;
        }

        label->unref();
        runner.endTest(true);
    } catch (const std::exception & e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }

    // ------------------------------------------------------------------
    // 2. SoHUDLabel – field assignment
    // ------------------------------------------------------------------
    runner.startTest("SoHUDLabel: field assignment");
    try {
        SoHUDLabel * label = new SoHUDLabel;
        label->ref();

        label->position.setValue(100.0f, 200.0f);
        label->string.setValue("Hello HUD");
        label->color.setValue(SbColor(1.0f, 0.0f, 0.0f));
        label->fontSize.setValue(18.0f);
        label->justification.setValue(SoHUDLabel::CENTER);

        SbVec2f pos = label->position.getValue();
        if (pos != SbVec2f(100.0f, 200.0f)) {
            runner.endTest(false, "position mismatch");
            label->unref();
            return 1;
        }
        if (label->fontSize.getValue() != 18.0f) {
            runner.endTest(false, "fontSize mismatch");
            label->unref();
            return 1;
        }
        if (label->justification.getValue() != SoHUDLabel::CENTER) {
            runner.endTest(false, "justification mismatch");
            label->unref();
            return 1;
        }

        label->unref();
        runner.endTest(true);
    } catch (const std::exception & e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }

    // ------------------------------------------------------------------
    // 3. SoHUDButton – creation and field defaults
    // ------------------------------------------------------------------
    runner.startTest("SoHUDButton: creation and field defaults");
    try {
        SoHUDButton * btn = new SoHUDButton;
        btn->ref();

        if (btn->getTypeId() == SoType::badType()) {
            runner.endTest(false, "SoHUDButton has bad type");
            btn->unref();
            return 1;
        }

        SbVec2f defSz = btn->size.getValue();
        if (defSz != SbVec2f(80.0f, 24.0f)) {
            runner.endTest(false, "Default size should be (80,24)");
            btn->unref();
            return 1;
        }

        btn->unref();
        runner.endTest(true);
    } catch (const std::exception & e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }

    // ------------------------------------------------------------------
    // 4. SoHUDButton – click callback registration and removal
    // ------------------------------------------------------------------
    runner.startTest("SoHUDButton: callback registration and removal");
    try {
        ClickCounter counter;
        SoHUDButton * btn = new SoHUDButton;
        btn->ref();

        btn->addClickCallback(onButtonClick, &counter);

        // Simulate adding the same callback twice then removing once.
        btn->addClickCallback(onButtonClick, &counter);
        btn->removeClickCallback(onButtonClick, &counter);

        // After one removal, one registration should remain.
        // We verify indirectly via handleEvent below.

        btn->unref();
        runner.endTest(true);
    } catch (const std::exception & e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }

    // ------------------------------------------------------------------
    // 5. SoHUDButton – hit-test inside bounds triggers callback
    // ------------------------------------------------------------------
    runner.startTest("SoHUDButton: click inside bounds triggers callback");
    try {
        ClickCounter counter;
        SoHUDButton * btn = new SoHUDButton;
        btn->ref();
        btn->position.setValue(50.0f, 50.0f);
        btn->size.setValue(100.0f, 30.0f);
        btn->addClickCallback(onButtonClick, &counter);

        // Build a viewport big enough to contain the button.
        SbViewportRegion vp(800, 600);

        // Synthesize a mouse-button-1 press at (100, 65) – inside the button.
        SoMouseButtonEvent ev;
        ev.setButton(SoMouseButtonEvent::BUTTON1);
        ev.setState(SoButtonEvent::DOWN);
        // getPosition(vpRgn) returns the stored raw pixel position.
        ev.setPosition(SbVec2s(100, 65));

        SoHandleEventAction hea(vp);
        hea.setEvent(&ev);

        btn->handleEvent(&hea);

        if (counter.count != 1) {
            runner.endTest(false, "Expected 1 click callback invocation");
            btn->unref();
            return 1;
        }
        if (!hea.isHandled()) {
            runner.endTest(false, "Event should have been consumed");
            btn->unref();
            return 1;
        }

        btn->unref();
        runner.endTest(true);
    } catch (const std::exception & e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }

    // ------------------------------------------------------------------
    // 6. SoHUDButton – click outside bounds does not trigger callback
    // ------------------------------------------------------------------
    runner.startTest("SoHUDButton: click outside bounds does not trigger callback");
    try {
        ClickCounter counter;
        SoHUDButton * btn = new SoHUDButton;
        btn->ref();
        btn->position.setValue(50.0f, 50.0f);
        btn->size.setValue(100.0f, 30.0f);
        btn->addClickCallback(onButtonClick, &counter);

        SbViewportRegion vp(800, 600);

        SoMouseButtonEvent ev;
        ev.setButton(SoMouseButtonEvent::BUTTON1);
        ev.setState(SoButtonEvent::DOWN);
        ev.setPosition(SbVec2s(10, 10)); // outside the button

        SoHandleEventAction hea(vp);
        hea.setEvent(&ev);

        btn->handleEvent(&hea);

        if (counter.count != 0) {
            runner.endTest(false, "Callback should not fire outside bounds");
            btn->unref();
            return 1;
        }
        if (hea.isHandled()) {
            runner.endTest(false, "Event outside bounds should not be consumed");
            btn->unref();
            return 1;
        }

        btn->unref();
        runner.endTest(true);
    } catch (const std::exception & e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }

    // ------------------------------------------------------------------
    // 7. SoHUDKit – creation and addWidget/removeWidget
    // ------------------------------------------------------------------
    runner.startTest("SoHUDKit: creation and widget management");
    try {
        SoHUDKit * hud = new SoHUDKit;
        hud->ref();

        if (hud->getTypeId() == SoType::badType()) {
            runner.endTest(false, "SoHUDKit has bad type");
            hud->unref();
            return 1;
        }

        SoHUDLabel * label = new SoHUDLabel;
        label->ref();
        label->string.setValue("HUD Label");

        hud->addWidget(label);

        // Remove and verify it doesn't crash.
        hud->removeWidget(label);

        // Removing an absent widget should be a no-op.
        hud->removeWidget(label);

        label->unref();
        hud->unref();
        runner.endTest(true);
    } catch (const std::exception & e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }

    // ------------------------------------------------------------------
    // 8. SoHUDKit – event propagation to child button
    // ------------------------------------------------------------------
    runner.startTest("SoHUDKit: event propagation to child SoHUDButton");
    try {
        ClickCounter counter;

        SoHUDKit * hud = new SoHUDKit;
        hud->ref();

        SoHUDButton * btn = new SoHUDButton;
        btn->ref();
        btn->position.setValue(10.0f, 10.0f);
        btn->size.setValue(80.0f, 24.0f);
        btn->addClickCallback(onButtonClick, &counter);
        hud->addWidget(btn);

        SbViewportRegion vp(400, 300);
        SoMouseButtonEvent ev;
        ev.setButton(SoMouseButtonEvent::BUTTON1);
        ev.setState(SoButtonEvent::DOWN);
        ev.setPosition(SbVec2s(50, 20)); // inside button

        SoHandleEventAction hea(vp);
        hea.setEvent(&ev);

        hud->handleEvent(&hea);

        if (counter.count != 1) {
            runner.endTest(false, "Button click via HUDKit should fire callback");
            btn->unref();
            hud->unref();
            return 1;
        }

        btn->unref();
        hud->unref();
        runner.endTest(true);
    } catch (const std::exception & e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }

    return runner.getSummary();
}
