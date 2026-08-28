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

using namespace ObolTest;

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

TEST(NodesSelectionNodes, SoSelectionClassTypeIdValid)
{
    EXPECT_TRUE((SoSelection::getClassTypeId() != SoType::badType())) << "SoSelection has bad class type";
}

TEST(NodesSelectionNodes, SoSelectionIsOfTypeSoSeparator)
{
    SoSelection * sel = new SoSelection;
    sel->ref();
    EXPECT_TRUE(sel->isOfType(SoSeparator::getClassTypeId())) << "SoSelection not a SoSeparator subtype";
    sel->unref();
}

TEST(NodesSelectionNodes, SoSelectionDefaultPolicyIsSHIFT)
{
    SoSelection * sel = new SoSelection;
    sel->ref();
    // Default policy in Open Inventor is SHIFT
    EXPECT_TRUE((sel->policy.getValue() == SoSelection::SHIFT)) << "SoSelection default policy is not SHIFT";
    sel->unref();
}

TEST(NodesSelectionNodes, SoSelectionSelectIsSelectedGetNumSelected)
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
    EXPECT_TRUE(sel->isSelected(cube) &&
                (sel->getNumSelected() == 1)) << "SoSelection select/isSelected/getNumSelected failed";

    root->unref();
}

TEST(NodesSelectionNodes, SoSelectionDeselectDecrementsCount)
{
    SoSelection * sel = new SoSelection;
    sel->ref();
    sel->policy = SoSelection::SINGLE;
    SoCube * cube = new SoCube;
    sel->addChild(cube);

    sel->select(cube);
    sel->deselect(cube);
    EXPECT_TRUE(!sel->isSelected(cube) &&
                (sel->getNumSelected() == 0)) << "SoSelection deselect did not decrement count";

    sel->unref();
}

TEST(NodesSelectionNodes, SoSelectionToggleSelectsThenDeselects)
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
    EXPECT_TRUE((selected && deselected)) << (selected && deselected ? "" :
        "SoSelection toggle did not work correctly");
}

TEST(NodesSelectionNodes, SoSelectionDeselectAllClearsSelection)
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
    EXPECT_TRUE((sel->getNumSelected() == 0)) << "SoSelection deselectAll did not clear selection";

    sel->unref();
}

TEST(NodesSelectionNodes, SoSelectionAddSelectionCallbackFiresOnSelect)
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
    EXPECT_TRUE((g_selectionCount == 1)) << "SoSelection addSelectionCallback did not fire";
    sel->unref();
}

TEST(NodesSelectionNodes, SoSelectionAddDeselectionCallbackFiresOnDeselect)
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

    EXPECT_TRUE((g_deselectionCount == 1)) << "SoSelection addDeselectionCallback did not fire";
    sel->unref();
}

TEST(NodesSelectionNodes, SoSelectionSoGetBoundingBoxActionDoesNotCrash)
{
    SoSelection * sel = new SoSelection;
    sel->ref();
    sel->addChild(new SoCube);
    SbViewportRegion vp(100, 100);
    SoGetBoundingBoxAction bba(vp);
    bba.apply(sel);
    sel->unref();
    SUCCEED();
}

TEST(NodesSelectionNodes, SoSelectionGetListReturnsNonNull)
{
    SoSelection * sel = new SoSelection;
    sel->ref();
    EXPECT_TRUE((sel->getList() != nullptr)) << "SoSelection getList() returned null";
    sel->unref();
}

// =========================================================================
// SoExtSelection
// =========================================================================

TEST(NodesSelectionNodes, SoExtSelectionClassTypeIdValid)
{
    EXPECT_TRUE((SoExtSelection::getClassTypeId() != SoType::badType())) << "SoExtSelection has bad class type";
}

TEST(NodesSelectionNodes, SoExtSelectionIsOfTypeSoSelection)
{
    SoExtSelection * sel = new SoExtSelection;
    sel->ref();
    EXPECT_TRUE(sel->isOfType(SoSelection::getClassTypeId())) << "SoExtSelection not a SoSelection subtype";
    sel->unref();
}

TEST(NodesSelectionNodes, SoExtSelectionLassoTypeFieldAccessible)
{
    SoExtSelection * sel = new SoExtSelection;
    sel->ref();
    // Default lassoType should be NOLASSO or similar
    int lt = sel->lassoType.getValue();
    EXPECT_GE(lt, 0); // any valid enum value
    sel->unref();
}

TEST(NodesSelectionNodes, SoExtSelectionLassoPolicyFieldAccessible)
{
    SoExtSelection * sel = new SoExtSelection;
    sel->ref();
    int lp = sel->lassoPolicy.getValue();
    EXPECT_TRUE((lp >= 0)) << "SoExtSelection lassoPolicy field returned invalid value";
    sel->unref();
}

TEST(NodesSelectionNodes, SoExtSelectionLassoModeFieldAccessible)
{
    SoExtSelection * sel = new SoExtSelection;
    sel->ref();
    int lm = sel->lassoMode.getValue();
    EXPECT_TRUE((lm >= 0)) << "SoExtSelection lassoMode field returned invalid value";
    sel->unref();
}

TEST(NodesSelectionNodes, SoExtSelectionSelectWith2DLassoDoesNotCrash)
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
    sel->unref();
    SUCCEED();
}

TEST(NodesSelectionNodes, SoExtSelectionSoSearchActionFindsSoExtSelection)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    SoExtSelection * sel = new SoExtSelection;
    root->addChild(sel);

    SoSearchAction sa;
    sa.setType(SoExtSelection::getClassTypeId());
    sa.setInterest(SoSearchAction::FIRST);
    sa.apply(root);

    EXPECT_TRUE((sa.getPath() != nullptr)) << "SoSearchAction did not find SoExtSelection";
    root->unref();
}
