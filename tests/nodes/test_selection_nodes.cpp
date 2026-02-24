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
 * @file test_selection_nodes.cpp
 * @brief Coverage for SoSelection and SoExtSelection nodes.
 *
 * SoExtSelection is at 0% coverage.  This test exercises:
 *   SoSelection   - type, policy field, select/deselect/toggle, isSelected,
 *                   getNumSelected, deselectAll, selection/deselection callbacks
 *   SoExtSelection - type, lassoType/lassoPolicy/lassoMode fields, select(lasso)
 *
 * Subsystems improved: nodes/ (SoExtSelection.cpp 0%)
 */

#include "../test_utils.h"

#include <Inventor/SoDB.h>
#include <Inventor/SoType.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoSelection.h>
#include <Inventor/nodes/SoExtSelection.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbVec2f.h>
#include <Inventor/SbVec3f.h>

using namespace SimpleTest;

// Track selection/deselection callbacks
static int g_selectionCount   = 0;
static int g_deselectionCount = 0;

static void onSelected(void * /*userdata*/, SoPath * /*path*/)
{
    ++g_selectionCount;
}

static void onDeselected(void * /*userdata*/, SoPath * /*path*/)
{
    ++g_deselectionCount;
}

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // =========================================================================
    // SoSelection
    // =========================================================================
    runner.startTest("SoSelection: class type id valid");
    {
        bool pass = (SoSelection::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoSelection has bad class type");
    }

    runner.startTest("SoSelection: isOfType SoSeparator");
    {
        SoSelection * sel = new SoSelection;
        sel->ref();
        bool pass = sel->isOfType(SoSeparator::getClassTypeId());
        sel->unref();
        runner.endTest(pass, pass ? "" : "SoSelection not a SoSeparator subtype");
    }

    runner.startTest("SoSelection: default policy is SHIFT");
    {
        SoSelection * sel = new SoSelection;
        sel->ref();
        // Default policy in Open Inventor is SHIFT
        bool pass = (sel->policy.getValue() == SoSelection::SHIFT);
        sel->unref();
        runner.endTest(pass, pass ? "" : "SoSelection default policy is not SHIFT");
    }

    runner.startTest("SoSelection: select/isSelected/getNumSelected");
    {
        SoSeparator * root = new SoSeparator;
        root->ref();
        SoSelection * sel = new SoSelection;
        sel->policy = SoSelection::SINGLE;
        SoCube * cube = new SoCube;
        sel->addChild(cube);
        root->addChild(sel);

        // Select the cube via the convenience node API
        sel->select(cube);
        bool pass = sel->isSelected(cube) &&
                    (sel->getNumSelected() == 1);

        root->unref();
        runner.endTest(pass, pass ? "" :
            "SoSelection select/isSelected/getNumSelected failed");
    }

    runner.startTest("SoSelection: deselect decrements count");
    {
        SoSelection * sel = new SoSelection;
        sel->ref();
        sel->policy = SoSelection::SINGLE;
        SoCube * cube = new SoCube;
        sel->addChild(cube);

        sel->select(cube);
        sel->deselect(cube);
        bool pass = !sel->isSelected(cube) &&
                    (sel->getNumSelected() == 0);

        sel->unref();
        runner.endTest(pass, pass ? "" :
            "SoSelection deselect did not decrement count");
    }

    runner.startTest("SoSelection: toggle selects then deselects");
    {
        SoSelection * sel = new SoSelection;
        sel->ref();
        sel->policy = SoSelection::TOGGLE;
        SoCube * cube = new SoCube;
        sel->addChild(cube);

        sel->toggle(cube);          // first call: select
        bool selected = sel->isSelected(cube);
        sel->toggle(cube);          // second call: deselect
        bool deselected = !sel->isSelected(cube);

        sel->unref();
        runner.endTest(selected && deselected, selected && deselected ? "" :
            "SoSelection toggle did not work correctly");
    }

    runner.startTest("SoSelection: deselectAll clears selection");
    {
        SoSelection * sel = new SoSelection;
        sel->ref();
        sel->policy = SoSelection::SHIFT;
        SoCube * c1 = new SoCube;
        SoCube * c2 = new SoCube;
        sel->addChild(c1);
        sel->addChild(c2);

        sel->select(c1);
        sel->select(c2);
        sel->deselectAll();
        bool pass = (sel->getNumSelected() == 0);

        sel->unref();
        runner.endTest(pass, pass ? "" :
            "SoSelection deselectAll did not clear selection");
    }

    runner.startTest("SoSelection: addSelectionCallback fires on select");
    {
        g_selectionCount = 0;

        SoSelection * sel = new SoSelection;
        sel->ref();
        sel->policy = SoSelection::SINGLE;
        SoCube * cube = new SoCube;
        sel->addChild(cube);

        sel->addSelectionCallback(onSelected, nullptr);
        sel->select(cube);

        // Callback fires synchronously
        bool pass = (g_selectionCount == 1);
        sel->unref();
        runner.endTest(pass, pass ? "" :
            "SoSelection addSelectionCallback did not fire");
    }

    runner.startTest("SoSelection: addDeselectionCallback fires on deselect");
    {
        g_deselectionCount = 0;

        SoSelection * sel = new SoSelection;
        sel->ref();
        sel->policy = SoSelection::SINGLE;
        SoCube * cube = new SoCube;
        sel->addChild(cube);

        sel->addDeselectionCallback(onDeselected, nullptr);
        sel->select(cube);
        sel->deselect(cube);

        bool pass = (g_deselectionCount == 1);
        sel->unref();
        runner.endTest(pass, pass ? "" :
            "SoSelection addDeselectionCallback did not fire");
    }

    runner.startTest("SoSelection: SoGetBoundingBoxAction does not crash");
    {
        SoSelection * sel = new SoSelection;
        sel->ref();
        sel->addChild(new SoCube);
        SbViewportRegion vp(100, 100);
        SoGetBoundingBoxAction bba(vp);
        bba.apply(sel);
        sel->unref();
        runner.endTest(true, "");
    }

    runner.startTest("SoSelection: getList returns non-null");
    {
        SoSelection * sel = new SoSelection;
        sel->ref();
        bool pass = (sel->getList() != nullptr);
        sel->unref();
        runner.endTest(pass, pass ? "" : "SoSelection getList() returned null");
    }

    // =========================================================================
    // SoExtSelection
    // =========================================================================
    runner.startTest("SoExtSelection: class type id valid");
    {
        bool pass = (SoExtSelection::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoExtSelection has bad class type");
    }

    runner.startTest("SoExtSelection: isOfType SoSelection");
    {
        SoExtSelection * sel = new SoExtSelection;
        sel->ref();
        bool pass = sel->isOfType(SoSelection::getClassTypeId());
        sel->unref();
        runner.endTest(pass, pass ? "" :
            "SoExtSelection not a SoSelection subtype");
    }

    runner.startTest("SoExtSelection: lassoType field accessible");
    {
        SoExtSelection * sel = new SoExtSelection;
        sel->ref();
        // Default lassoType should be NOLASSO or similar
        int lt = sel->lassoType.getValue();
        bool pass = (lt >= 0); // any valid enum value
        sel->unref();
        runner.endTest(pass, pass ? "" :
            "SoExtSelection lassoType field returned invalid value");
    }

    runner.startTest("SoExtSelection: lassoPolicy field accessible");
    {
        SoExtSelection * sel = new SoExtSelection;
        sel->ref();
        int lp = sel->lassoPolicy.getValue();
        bool pass = (lp >= 0);
        sel->unref();
        runner.endTest(pass, pass ? "" :
            "SoExtSelection lassoPolicy field returned invalid value");
    }

    runner.startTest("SoExtSelection: lassoMode field accessible");
    {
        SoExtSelection * sel = new SoExtSelection;
        sel->ref();
        int lm = sel->lassoMode.getValue();
        bool pass = (lm >= 0);
        sel->unref();
        runner.endTest(pass, pass ? "" :
            "SoExtSelection lassoMode field returned invalid value");
    }

    runner.startTest("SoExtSelection: select with 2D lasso does not crash");
    {
        SoExtSelection * sel = new SoExtSelection;
        sel->ref();
        SoCube * cube = new SoCube;
        sel->addChild(cube);

        // select() with a small lasso around the centre; needs a viewport
        SbVec2f lasso[4] = {
            SbVec2f(0.4f, 0.4f),
            SbVec2f(0.6f, 0.4f),
            SbVec2f(0.6f, 0.6f),
            SbVec2f(0.4f, 0.6f)
        };
        SbViewportRegion vp(256, 256);
        // The root argument to select() is the scene to pick from
        sel->select(sel, 4, lasso, vp, TRUE);
        bool pass = true; // reaching here without crash = pass
        sel->unref();
        runner.endTest(pass, "");
    }

    runner.startTest("SoExtSelection: SoSearchAction finds SoExtSelection");
    {
        SoSeparator * root = new SoSeparator;
        root->ref();
        SoExtSelection * sel = new SoExtSelection;
        root->addChild(sel);

        SoSearchAction sa;
        sa.setType(SoExtSelection::getClassTypeId());
        sa.setInterest(SoSearchAction::FIRST);
        sa.apply(root);

        bool pass = (sa.getPath() != nullptr);
        root->unref();
        runner.endTest(pass, pass ? "" :
            "SoSearchAction did not find SoExtSelection");
    }

    return runner.getSummary();
}
