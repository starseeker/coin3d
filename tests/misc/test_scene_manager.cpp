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
 * @file test_scene_manager.cpp
 * @brief API round-trip tests for SoSceneManager (no GL context required).
 *
 * Covers (Tier 5, priority 52):
 *   - Construction / destruction (no crash)
 *   - setSceneGraph / getSceneGraph round-trip
 *   - setBackgroundColor / getBackgroundColor round-trip
 *   - setViewportRegion / getViewportRegion round-trip
 *   - setWindowSize / getWindowSize round-trip
 *   - setSize / getSize round-trip
 *   - setOrigin / getOrigin round-trip
 *   - setRGBMode / isRGBMode round-trip
 *   - setGLRenderAction / getGLRenderAction round-trip
 *   - setRenderCallback (set and retrieve via isAutoRedraw)
 *   - setRedrawPriority / getRedrawPriority round-trip
 *   - setAntialiasing / getAntialiasing round-trip
 *   - getDefaultRedrawPriority is non-zero
 *
 * Subsystems improved: misc (SoSceneManager)
 */

#include "../test_utils.h"

#include <Inventor/SoSceneManager.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/actions/SoHandleEventAction.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbColor.h>
#include <Inventor/SbVec2s.h>
#include <cmath>

using namespace ObolTest;

// Render callback capture
struct RenderCap {
    int count;
    RenderCap() : count(0) {}
};

static void renderCb(void * data, SoSceneManager * /*mgr*/)
{
    static_cast<RenderCap *>(data)->count++;
}

TEST(MiscSceneManager, SoSceneManagerConstructAndDestroyWithoutCrash)
{
    SoSceneManager * mgr = new SoSceneManager;
    EXPECT_TRUE((mgr != nullptr));
    delete mgr;
}

// -----------------------------------------------------------------------
// setSceneGraph / getSceneGraph
// -----------------------------------------------------------------------

TEST(MiscSceneManager, SoSceneManagerSetSceneGraphGetSceneGraphRoundTrip)
{
    SoSceneManager * mgr = new SoSceneManager;
    SoSeparator * root = new SoSeparator;
    root->ref();
    mgr->setSceneGraph(root);
    EXPECT_TRUE((mgr->getSceneGraph() == root)) << "SoSceneManager setSceneGraph/getSceneGraph round-trip failed";
    delete mgr;
    root->unref();
}

TEST(MiscSceneManager, SoSceneManagerSetSceneGraphNullReturnsNullFromGetSceneGraph)
{
    SoSceneManager * mgr = new SoSceneManager;
    SoSeparator * root = new SoSeparator;
    root->ref();
    mgr->setSceneGraph(root);
    mgr->setSceneGraph(nullptr);
    EXPECT_TRUE((mgr->getSceneGraph() == nullptr)) << "SoSceneManager getSceneGraph should return null after setSceneGraph(null)";
    delete mgr;
    root->unref();
}

// -----------------------------------------------------------------------
// setBackgroundColor / getBackgroundColor
// -----------------------------------------------------------------------

TEST(MiscSceneManager, SoSceneManagerSetBackgroundColorGetBackgroundColorRoundTrip)
{
    SoSceneManager * mgr = new SoSceneManager;
    SbColor bg(0.1f, 0.2f, 0.3f);
    mgr->setBackgroundColor(bg);
    const SbColor & got = mgr->getBackgroundColor();
    EXPECT_TRUE((std::fabs(got[0] - 0.1f) < 1e-5f) &&
                (std::fabs(got[1] - 0.2f) < 1e-5f) &&
                (std::fabs(got[2] - 0.3f) < 1e-5f)) << "SoSceneManager background colour round-trip failed";
    delete mgr;
}

// -----------------------------------------------------------------------
// setViewportRegion / getViewportRegion
// -----------------------------------------------------------------------

TEST(MiscSceneManager, SoSceneManagerSetViewportRegionGetViewportRegionRoundTrip)
{
    SoSceneManager * mgr = new SoSceneManager;
    SbViewportRegion vp(320, 240);
    mgr->setViewportRegion(vp);
    const SbViewportRegion & got = mgr->getViewportRegion();
    EXPECT_TRUE((got.getWindowSize()[0] == 320) &&
                (got.getWindowSize()[1] == 240)) << "SoSceneManager setViewportRegion round-trip failed";
    delete mgr;
}

// -----------------------------------------------------------------------
// setWindowSize / getWindowSize
// -----------------------------------------------------------------------

TEST(MiscSceneManager, SoSceneManagerSetWindowSizeGetWindowSizeRoundTrip)
{
    SoSceneManager * mgr = new SoSceneManager;
    mgr->setWindowSize(SbVec2s(800, 600));
    const SbVec2s & got = mgr->getWindowSize();
    EXPECT_TRUE((got[0] == 800) && (got[1] == 600)) << "SoSceneManager setWindowSize/getWindowSize round-trip failed";
    delete mgr;
}

// -----------------------------------------------------------------------
// setSize / getSize
// -----------------------------------------------------------------------

TEST(MiscSceneManager, SoSceneManagerSetSizeGetSizeRoundTrip)
{
    SoSceneManager * mgr = new SoSceneManager;
    mgr->setSize(SbVec2s(400, 300));
    const SbVec2s & got = mgr->getSize();
    EXPECT_TRUE((got[0] == 400) && (got[1] == 300)) << "SoSceneManager setSize/getSize round-trip failed";
    delete mgr;
}

// -----------------------------------------------------------------------
// setOrigin / getOrigin
// -----------------------------------------------------------------------

TEST(MiscSceneManager, SoSceneManagerSetOriginGetOriginRoundTrip)
{
    SoSceneManager * mgr = new SoSceneManager;
    mgr->setOrigin(SbVec2s(10, 20));
    const SbVec2s & got = mgr->getOrigin();
    EXPECT_TRUE((got[0] == 10) && (got[1] == 20)) << "SoSceneManager setOrigin/getOrigin round-trip failed";
    delete mgr;
}

// -----------------------------------------------------------------------
// setRGBMode / isRGBMode
// -----------------------------------------------------------------------

TEST(MiscSceneManager, SoSceneManagerIsRGBModeDefaultsToTRUE)
{
    SoSceneManager * mgr = new SoSceneManager;
    EXPECT_TRUE((mgr->isRGBMode() == TRUE)) << "SoSceneManager isRGBMode should default to TRUE";
    delete mgr;
}

TEST(MiscSceneManager, SoSceneManagerSetRGBModeFALSERoundTrip)
{
    SoSceneManager * mgr = new SoSceneManager;
    mgr->setRGBMode(FALSE);
    EXPECT_TRUE((mgr->isRGBMode() == FALSE)) << "SoSceneManager setRGBMode(FALSE) failed";
    mgr->setRGBMode(TRUE); // restore
    delete mgr;
}

// -----------------------------------------------------------------------
// setGLRenderAction / getGLRenderAction
// -----------------------------------------------------------------------

TEST(MiscSceneManager, SoSceneManagerGetGLRenderActionIsNonNullByDefault)
{
    SoSceneManager * mgr = new SoSceneManager;
    EXPECT_TRUE((mgr->getGLRenderAction() != nullptr)) << "SoSceneManager getGLRenderAction should not be null";
    delete mgr;
}

TEST(MiscSceneManager, SoSceneManagerSetGLRenderActionGetGLRenderActionRoundTrip)
{
    SoSceneManager * mgr = new SoSceneManager;
    SoGLRenderAction * ra = new SoGLRenderAction(SbViewportRegion(256, 256));
    mgr->setGLRenderAction(ra);
    EXPECT_TRUE((mgr->getGLRenderAction() == ra)) << "SoSceneManager setGLRenderAction round-trip failed";
    delete mgr;
    // The caller retains ownership of an explicitly supplied action.
    delete ra;
}

// -----------------------------------------------------------------------
// setHandleEventAction / getHandleEventAction
// -----------------------------------------------------------------------

TEST(MiscSceneManager, SoSceneManagerGetHandleEventActionIsNonNullByDefault)
{
    SoSceneManager * mgr = new SoSceneManager;
    EXPECT_TRUE((mgr->getHandleEventAction() != nullptr)) << "SoSceneManager getHandleEventAction should not be null";
    delete mgr;
}

// -----------------------------------------------------------------------
// setRedrawPriority / getRedrawPriority
// -----------------------------------------------------------------------

TEST(MiscSceneManager, SoSceneManagerSetRedrawPriorityGetRedrawPriorityRoundTrip)
{
    SoSceneManager * mgr = new SoSceneManager;
    mgr->setRedrawPriority(5u);
    EXPECT_TRUE((mgr->getRedrawPriority() == 5u)) << "SoSceneManager setRedrawPriority round-trip failed";
    delete mgr;
}

TEST(MiscSceneManager, SoSceneManagerGetDefaultRedrawPriorityIsNonZero)
{
    EXPECT_TRUE((SoSceneManager::getDefaultRedrawPriority() != 0u)) << "SoSceneManager::getDefaultRedrawPriority should be non-zero";
}

// -----------------------------------------------------------------------
// setAntialiasing / getAntialiasing
// -----------------------------------------------------------------------

TEST(MiscSceneManager, SoSceneManagerSetAntialiasingGetAntialiasingRoundTrip)
{
    SoSceneManager * mgr = new SoSceneManager;
    mgr->setAntialiasing(TRUE, 4);
    SbBool smooth = FALSE;
    int    passes = 0;
    mgr->getAntialiasing(smooth, passes);
    EXPECT_TRUE((smooth == TRUE) && (passes == 4)) << "SoSceneManager setAntialiasing round-trip failed";
    delete mgr;
}

// -----------------------------------------------------------------------
// setRenderCallback / isAutoRedraw
// -----------------------------------------------------------------------

TEST(MiscSceneManager, SoSceneManagerIsAutoRedrawFALSEBeforeSetRenderCallback)
{
    SoSceneManager * mgr = new SoSceneManager;
    EXPECT_TRUE((mgr->isAutoRedraw() == FALSE)) << "SoSceneManager isAutoRedraw should be FALSE before setRenderCallback";
    delete mgr;
}

TEST(MiscSceneManager, SoSceneManagerIsAutoRedrawTRUEAfterSetRenderCallback)
{
    SoSceneManager * mgr = new SoSceneManager;
    RenderCap cap;
    mgr->setRenderCallback(renderCb, &cap);
    EXPECT_TRUE((mgr->isAutoRedraw() == TRUE)) << "SoSceneManager isAutoRedraw should be TRUE after setRenderCallback";
    delete mgr;
}

TEST(MiscSceneManager, SoSceneManagerSetRenderCallbackNullDoesNotCrash)
{
    SoSceneManager * mgr = new SoSceneManager;
    RenderCap cap;
    mgr->setRenderCallback(renderCb, &cap);
    // Setting null callback should not crash
    mgr->setRenderCallback(nullptr, nullptr);
    delete mgr;
    SUCCEED();
}

// -----------------------------------------------------------------------
// enableRealTimeUpdate / isRealTimeUpdateEnabled
// -----------------------------------------------------------------------

TEST(MiscSceneManager, SoSceneManagerIsRealTimeUpdateEnabledDefaultsToTRUE)
{
    EXPECT_TRUE((SoSceneManager::isRealTimeUpdateEnabled() == TRUE)) << "SoSceneManager isRealTimeUpdateEnabled should default to TRUE";
}

TEST(MiscSceneManager, SoSceneManagerEnableRealTimeUpdateFALSERoundTrip)
{
    SoSceneManager::enableRealTimeUpdate(FALSE);
    EXPECT_TRUE((SoSceneManager::isRealTimeUpdateEnabled() == FALSE)) << "SoSceneManager enableRealTimeUpdate(FALSE) round-trip failed";
    SoSceneManager::enableRealTimeUpdate(TRUE); // restore
}
