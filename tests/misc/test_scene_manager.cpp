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

using namespace SimpleTest;

// Render callback capture
struct RenderCap {
    int count;
    RenderCap() : count(0) {}
};

static void renderCb(void * data, SoSceneManager * /*mgr*/)
{
    static_cast<RenderCap *>(data)->count++;
}

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // -----------------------------------------------------------------------
    // Construction / destruction
    // -----------------------------------------------------------------------
    runner.startTest("SoSceneManager: construct and destroy without crash");
    {
        SoSceneManager * mgr = new SoSceneManager;
        bool pass = (mgr != nullptr);
        delete mgr;
        runner.endTest(pass, "");
    }

    // -----------------------------------------------------------------------
    // setSceneGraph / getSceneGraph
    // -----------------------------------------------------------------------
    runner.startTest("SoSceneManager: setSceneGraph / getSceneGraph round-trip");
    {
        SoSceneManager * mgr = new SoSceneManager;
        SoSeparator * root = new SoSeparator;
        root->ref();
        mgr->setSceneGraph(root);
        bool pass = (mgr->getSceneGraph() == root);
        delete mgr;
        root->unref();
        runner.endTest(pass, pass ? "" :
            "SoSceneManager setSceneGraph/getSceneGraph round-trip failed");
    }

    runner.startTest("SoSceneManager: setSceneGraph(null) returns null from getSceneGraph");
    {
        SoSceneManager * mgr = new SoSceneManager;
        SoSeparator * root = new SoSeparator;
        root->ref();
        mgr->setSceneGraph(root);
        mgr->setSceneGraph(nullptr);
        bool pass = (mgr->getSceneGraph() == nullptr);
        delete mgr;
        root->unref();
        runner.endTest(pass, pass ? "" :
            "SoSceneManager getSceneGraph should return null after setSceneGraph(null)");
    }

    // -----------------------------------------------------------------------
    // setBackgroundColor / getBackgroundColor
    // -----------------------------------------------------------------------
    runner.startTest("SoSceneManager: setBackgroundColor / getBackgroundColor round-trip");
    {
        SoSceneManager * mgr = new SoSceneManager;
        SbColor bg(0.1f, 0.2f, 0.3f);
        mgr->setBackgroundColor(bg);
        const SbColor & got = mgr->getBackgroundColor();
        bool pass = (std::fabs(got[0] - 0.1f) < 1e-5f) &&
                    (std::fabs(got[1] - 0.2f) < 1e-5f) &&
                    (std::fabs(got[2] - 0.3f) < 1e-5f);
        delete mgr;
        runner.endTest(pass, pass ? "" :
            "SoSceneManager background colour round-trip failed");
    }

    // -----------------------------------------------------------------------
    // setViewportRegion / getViewportRegion
    // -----------------------------------------------------------------------
    runner.startTest("SoSceneManager: setViewportRegion / getViewportRegion round-trip");
    {
        SoSceneManager * mgr = new SoSceneManager;
        SbViewportRegion vp(320, 240);
        mgr->setViewportRegion(vp);
        const SbViewportRegion & got = mgr->getViewportRegion();
        bool pass = (got.getWindowSize()[0] == 320) &&
                    (got.getWindowSize()[1] == 240);
        delete mgr;
        runner.endTest(pass, pass ? "" :
            "SoSceneManager setViewportRegion round-trip failed");
    }

    // -----------------------------------------------------------------------
    // setWindowSize / getWindowSize
    // -----------------------------------------------------------------------
    runner.startTest("SoSceneManager: setWindowSize / getWindowSize round-trip");
    {
        SoSceneManager * mgr = new SoSceneManager;
        mgr->setWindowSize(SbVec2s(800, 600));
        const SbVec2s & got = mgr->getWindowSize();
        bool pass = (got[0] == 800) && (got[1] == 600);
        delete mgr;
        runner.endTest(pass, pass ? "" :
            "SoSceneManager setWindowSize/getWindowSize round-trip failed");
    }

    // -----------------------------------------------------------------------
    // setSize / getSize
    // -----------------------------------------------------------------------
    runner.startTest("SoSceneManager: setSize / getSize round-trip");
    {
        SoSceneManager * mgr = new SoSceneManager;
        mgr->setSize(SbVec2s(400, 300));
        const SbVec2s & got = mgr->getSize();
        bool pass = (got[0] == 400) && (got[1] == 300);
        delete mgr;
        runner.endTest(pass, pass ? "" :
            "SoSceneManager setSize/getSize round-trip failed");
    }

    // -----------------------------------------------------------------------
    // setOrigin / getOrigin
    // -----------------------------------------------------------------------
    runner.startTest("SoSceneManager: setOrigin / getOrigin round-trip");
    {
        SoSceneManager * mgr = new SoSceneManager;
        mgr->setOrigin(SbVec2s(10, 20));
        const SbVec2s & got = mgr->getOrigin();
        bool pass = (got[0] == 10) && (got[1] == 20);
        delete mgr;
        runner.endTest(pass, pass ? "" :
            "SoSceneManager setOrigin/getOrigin round-trip failed");
    }

    // -----------------------------------------------------------------------
    // setRGBMode / isRGBMode
    // -----------------------------------------------------------------------
    runner.startTest("SoSceneManager: isRGBMode defaults to TRUE");
    {
        SoSceneManager * mgr = new SoSceneManager;
        bool pass = (mgr->isRGBMode() == TRUE);
        delete mgr;
        runner.endTest(pass, pass ? "" :
            "SoSceneManager isRGBMode should default to TRUE");
    }

    runner.startTest("SoSceneManager: setRGBMode(FALSE) round-trip");
    {
        SoSceneManager * mgr = new SoSceneManager;
        mgr->setRGBMode(FALSE);
        bool pass = (mgr->isRGBMode() == FALSE);
        mgr->setRGBMode(TRUE); // restore
        delete mgr;
        runner.endTest(pass, pass ? "" :
            "SoSceneManager setRGBMode(FALSE) failed");
    }

    // -----------------------------------------------------------------------
    // setGLRenderAction / getGLRenderAction
    // -----------------------------------------------------------------------
    runner.startTest("SoSceneManager: getGLRenderAction is non-null by default");
    {
        SoSceneManager * mgr = new SoSceneManager;
        bool pass = (mgr->getGLRenderAction() != nullptr);
        delete mgr;
        runner.endTest(pass, pass ? "" :
            "SoSceneManager getGLRenderAction should not be null");
    }

    runner.startTest("SoSceneManager: setGLRenderAction / getGLRenderAction round-trip");
    {
        SoSceneManager * mgr = new SoSceneManager;
        SoGLRenderAction * ra = new SoGLRenderAction(SbViewportRegion(256, 256));
        mgr->setGLRenderAction(ra);
        bool pass = (mgr->getGLRenderAction() == ra);
        delete mgr;
        // The manager takes ownership; do NOT delete ra separately
        runner.endTest(pass, pass ? "" :
            "SoSceneManager setGLRenderAction round-trip failed");
    }

    // -----------------------------------------------------------------------
    // setHandleEventAction / getHandleEventAction
    // -----------------------------------------------------------------------
    runner.startTest("SoSceneManager: getHandleEventAction is non-null by default");
    {
        SoSceneManager * mgr = new SoSceneManager;
        bool pass = (mgr->getHandleEventAction() != nullptr);
        delete mgr;
        runner.endTest(pass, pass ? "" :
            "SoSceneManager getHandleEventAction should not be null");
    }

    // -----------------------------------------------------------------------
    // setRedrawPriority / getRedrawPriority
    // -----------------------------------------------------------------------
    runner.startTest("SoSceneManager: setRedrawPriority / getRedrawPriority round-trip");
    {
        SoSceneManager * mgr = new SoSceneManager;
        mgr->setRedrawPriority(5u);
        bool pass = (mgr->getRedrawPriority() == 5u);
        delete mgr;
        runner.endTest(pass, pass ? "" :
            "SoSceneManager setRedrawPriority round-trip failed");
    }

    runner.startTest("SoSceneManager: getDefaultRedrawPriority is non-zero");
    {
        bool pass = (SoSceneManager::getDefaultRedrawPriority() != 0u);
        runner.endTest(pass, pass ? "" :
            "SoSceneManager::getDefaultRedrawPriority should be non-zero");
    }

    // -----------------------------------------------------------------------
    // setAntialiasing / getAntialiasing
    // -----------------------------------------------------------------------
    runner.startTest("SoSceneManager: setAntialiasing / getAntialiasing round-trip");
    {
        SoSceneManager * mgr = new SoSceneManager;
        mgr->setAntialiasing(TRUE, 4);
        SbBool smooth = FALSE;
        int    passes = 0;
        mgr->getAntialiasing(smooth, passes);
        bool pass = (smooth == TRUE) && (passes == 4);
        delete mgr;
        runner.endTest(pass, pass ? "" :
            "SoSceneManager setAntialiasing round-trip failed");
    }

    // -----------------------------------------------------------------------
    // setRenderCallback / isAutoRedraw
    // -----------------------------------------------------------------------
    runner.startTest("SoSceneManager: isAutoRedraw FALSE before setRenderCallback");
    {
        SoSceneManager * mgr = new SoSceneManager;
        bool pass = (mgr->isAutoRedraw() == FALSE);
        delete mgr;
        runner.endTest(pass, pass ? "" :
            "SoSceneManager isAutoRedraw should be FALSE before setRenderCallback");
    }

    runner.startTest("SoSceneManager: isAutoRedraw TRUE after setRenderCallback");
    {
        SoSceneManager * mgr = new SoSceneManager;
        RenderCap cap;
        mgr->setRenderCallback(renderCb, &cap);
        bool pass = (mgr->isAutoRedraw() == TRUE);
        delete mgr;
        runner.endTest(pass, pass ? "" :
            "SoSceneManager isAutoRedraw should be TRUE after setRenderCallback");
    }

    runner.startTest("SoSceneManager: setRenderCallback(null) does not crash");
    {
        SoSceneManager * mgr = new SoSceneManager;
        RenderCap cap;
        mgr->setRenderCallback(renderCb, &cap);
        // Setting null callback should not crash
        mgr->setRenderCallback(nullptr, nullptr);
        bool pass = true; // no crash = pass
        delete mgr;
        runner.endTest(pass, "");
    }

    // -----------------------------------------------------------------------
    // enableRealTimeUpdate / isRealTimeUpdateEnabled
    // -----------------------------------------------------------------------
    runner.startTest("SoSceneManager: isRealTimeUpdateEnabled defaults to TRUE");
    {
        bool pass = (SoSceneManager::isRealTimeUpdateEnabled() == TRUE);
        runner.endTest(pass, pass ? "" :
            "SoSceneManager isRealTimeUpdateEnabled should default to TRUE");
    }

    runner.startTest("SoSceneManager: enableRealTimeUpdate(FALSE) round-trip");
    {
        SoSceneManager::enableRealTimeUpdate(FALSE);
        bool pass = (SoSceneManager::isRealTimeUpdateEnabled() == FALSE);
        SoSceneManager::enableRealTimeUpdate(TRUE); // restore
        runner.endTest(pass, pass ? "" :
            "SoSceneManager enableRealTimeUpdate(FALSE) round-trip failed");
    }

    return runner.getSummary();
}
