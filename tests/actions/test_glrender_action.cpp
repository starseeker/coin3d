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
 * @file test_glrender_action.cpp
 * @brief API round-trip tests for SoGLRenderAction (no GL context required).
 *
 * Covers (Tier 5, priority 51):
 *   - Constructor / getViewportRegion round-trip
 *   - setTransparencyType / getTransparencyType for all enum values
 *   - setSmoothing / isSmoothing
 *   - setNumPasses / getNumPasses
 *   - setCacheContext / getCacheContext
 *   - setPassUpdate / isPassUpdate
 *   - setRenderingIsRemote / getRenderingIsRemote
 *   - setSortedLayersNumPasses / getSortedLayersNumPasses
 *   - isRenderingDelayedPaths default FALSE
 *   - setDelayedObjDepthWrite / getDelayedObjDepthWrite
 *   - isRenderingTranspPaths / isRenderingTranspBackfaces defaults
 *   - setUpdateArea / getUpdateArea round-trip
 *
 * Subsystems improved: actions
 */

#include "../test_utils.h"

#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbVec2f.h>
#include <cmath>

using namespace SimpleTest;

int main()
{
    TestFixture fixture;
    TestRunner runner;

    const SbViewportRegion vp(512, 384);

    // -----------------------------------------------------------------------
    // Constructor / getViewportRegion
    // -----------------------------------------------------------------------
    runner.startTest("SoGLRenderAction: getViewportRegion matches constructor arg");
    {
        SoGLRenderAction action(vp);
        const SbViewportRegion & got = action.getViewportRegion();
        bool pass = (got.getWindowSize()[0] == 512) &&
                    (got.getWindowSize()[1] == 384);
        runner.endTest(pass, pass ? "" :
            "SoGLRenderAction viewport region mismatch");
    }

    runner.startTest("SoGLRenderAction: setViewportRegion round-trip");
    {
        SoGLRenderAction action(vp);
        SbViewportRegion vp2(128, 64);
        action.setViewportRegion(vp2);
        const SbViewportRegion & got = action.getViewportRegion();
        bool pass = (got.getWindowSize()[0] == 128) &&
                    (got.getWindowSize()[1] == 64);
        runner.endTest(pass, pass ? "" :
            "SoGLRenderAction setViewportRegion round-trip failed");
    }

    // -----------------------------------------------------------------------
    // setTransparencyType / getTransparencyType
    // -----------------------------------------------------------------------
    runner.startTest("SoGLRenderAction: default transparency type is BLEND");
    {
        SoGLRenderAction action(vp);
        bool pass = (action.getTransparencyType() ==
                     SoGLRenderAction::BLEND);
        runner.endTest(pass, pass ? "" :
            "SoGLRenderAction default transparency should be BLEND");
    }

    runner.startTest("SoGLRenderAction: setTransparencyType BLEND round-trip");
    {
        SoGLRenderAction action(vp);
        action.setTransparencyType(SoGLRenderAction::BLEND);
        bool pass = (action.getTransparencyType() == SoGLRenderAction::BLEND);
        runner.endTest(pass, pass ? "" :
            "SoGLRenderAction transparency BLEND round-trip failed");
    }

    runner.startTest("SoGLRenderAction: setTransparencyType DELAYED_BLEND round-trip");
    {
        SoGLRenderAction action(vp);
        action.setTransparencyType(SoGLRenderAction::DELAYED_BLEND);
        bool pass = (action.getTransparencyType() ==
                     SoGLRenderAction::DELAYED_BLEND);
        runner.endTest(pass, pass ? "" :
            "SoGLRenderAction transparency DELAYED_BLEND round-trip failed");
    }

    runner.startTest("SoGLRenderAction: setTransparencyType SORTED_OBJECT_BLEND round-trip");
    {
        SoGLRenderAction action(vp);
        action.setTransparencyType(SoGLRenderAction::SORTED_OBJECT_BLEND);
        bool pass = (action.getTransparencyType() ==
                     SoGLRenderAction::SORTED_OBJECT_BLEND);
        runner.endTest(pass, pass ? "" :
            "SoGLRenderAction transparency SORTED_OBJECT_BLEND round-trip failed");
    }

    runner.startTest("SoGLRenderAction: setTransparencyType ADD round-trip");
    {
        SoGLRenderAction action(vp);
        action.setTransparencyType(SoGLRenderAction::ADD);
        bool pass = (action.getTransparencyType() == SoGLRenderAction::ADD);
        runner.endTest(pass, pass ? "" :
            "SoGLRenderAction transparency ADD round-trip failed");
    }

    runner.startTest("SoGLRenderAction: setTransparencyType NONE round-trip");
    {
        SoGLRenderAction action(vp);
        action.setTransparencyType(SoGLRenderAction::NONE);
        bool pass = (action.getTransparencyType() == SoGLRenderAction::NONE);
        runner.endTest(pass, pass ? "" :
            "SoGLRenderAction transparency NONE round-trip failed");
    }

    // -----------------------------------------------------------------------
    // setSmoothing / isSmoothing
    // -----------------------------------------------------------------------
    runner.startTest("SoGLRenderAction: isSmoothing defaults to FALSE");
    {
        SoGLRenderAction action(vp);
        bool pass = (action.isSmoothing() == FALSE);
        runner.endTest(pass, pass ? "" :
            "SoGLRenderAction isSmoothing should default to FALSE");
    }

    runner.startTest("SoGLRenderAction: setSmoothing(TRUE) round-trip");
    {
        SoGLRenderAction action(vp);
        action.setSmoothing(TRUE);
        bool pass = (action.isSmoothing() == TRUE);
        runner.endTest(pass, pass ? "" :
            "SoGLRenderAction setSmoothing(TRUE) failed");
    }

    runner.startTest("SoGLRenderAction: setSmoothing(FALSE) round-trip");
    {
        SoGLRenderAction action(vp);
        action.setSmoothing(TRUE);
        action.setSmoothing(FALSE);
        bool pass = (action.isSmoothing() == FALSE);
        runner.endTest(pass, pass ? "" :
            "SoGLRenderAction setSmoothing(FALSE) failed");
    }

    // -----------------------------------------------------------------------
    // setNumPasses / getNumPasses
    // -----------------------------------------------------------------------
    runner.startTest("SoGLRenderAction: getNumPasses defaults to 1");
    {
        SoGLRenderAction action(vp);
        bool pass = (action.getNumPasses() == 1);
        runner.endTest(pass, pass ? "" :
            "SoGLRenderAction getNumPasses should default to 1");
    }

    runner.startTest("SoGLRenderAction: setNumPasses(4) round-trip");
    {
        SoGLRenderAction action(vp);
        action.setNumPasses(4);
        bool pass = (action.getNumPasses() == 4);
        runner.endTest(pass, pass ? "" :
            "SoGLRenderAction setNumPasses(4) round-trip failed");
    }

    // -----------------------------------------------------------------------
    // setCacheContext / getCacheContext
    // -----------------------------------------------------------------------
    runner.startTest("SoGLRenderAction: setCacheContext / getCacheContext round-trip");
    {
        SoGLRenderAction action(vp);
        action.setCacheContext(42u);
        bool pass = (action.getCacheContext() == 42u);
        runner.endTest(pass, pass ? "" :
            "SoGLRenderAction setCacheContext round-trip failed");
    }

    // -----------------------------------------------------------------------
    // setPassUpdate / isPassUpdate
    // -----------------------------------------------------------------------
    runner.startTest("SoGLRenderAction: isPassUpdate defaults to FALSE");
    {
        SoGLRenderAction action(vp);
        bool pass = (action.isPassUpdate() == FALSE);
        runner.endTest(pass, pass ? "" :
            "SoGLRenderAction isPassUpdate should default to FALSE");
    }

    runner.startTest("SoGLRenderAction: setPassUpdate(TRUE) round-trip");
    {
        SoGLRenderAction action(vp);
        action.setPassUpdate(TRUE);
        bool pass = (action.isPassUpdate() == TRUE);
        runner.endTest(pass, pass ? "" :
            "SoGLRenderAction setPassUpdate(TRUE) failed");
    }

    // -----------------------------------------------------------------------
    // setRenderingIsRemote / getRenderingIsRemote
    // -----------------------------------------------------------------------
    runner.startTest("SoGLRenderAction: getRenderingIsRemote defaults to FALSE");
    {
        SoGLRenderAction action(vp);
        bool pass = (action.getRenderingIsRemote() == FALSE);
        runner.endTest(pass, pass ? "" :
            "SoGLRenderAction getRenderingIsRemote should default to FALSE");
    }

    runner.startTest("SoGLRenderAction: setRenderingIsRemote(TRUE) round-trip");
    {
        SoGLRenderAction action(vp);
        action.setRenderingIsRemote(TRUE);
        bool pass = (action.getRenderingIsRemote() == TRUE);
        runner.endTest(pass, pass ? "" :
            "SoGLRenderAction setRenderingIsRemote(TRUE) failed");
    }

    // -----------------------------------------------------------------------
    // setSortedLayersNumPasses / getSortedLayersNumPasses
    // -----------------------------------------------------------------------
    runner.startTest("SoGLRenderAction: setSortedLayersNumPasses round-trip");
    {
        SoGLRenderAction action(vp);
        action.setSortedLayersNumPasses(8);
        bool pass = (action.getSortedLayersNumPasses() == 8);
        runner.endTest(pass, pass ? "" :
            "SoGLRenderAction setSortedLayersNumPasses round-trip failed");
    }

    // -----------------------------------------------------------------------
    // isRenderingDelayedPaths
    // -----------------------------------------------------------------------
    runner.startTest("SoGLRenderAction: isRenderingDelayedPaths defaults to FALSE");
    {
        SoGLRenderAction action(vp);
        bool pass = (action.isRenderingDelayedPaths() == FALSE);
        runner.endTest(pass, pass ? "" :
            "SoGLRenderAction isRenderingDelayedPaths should default to FALSE");
    }

    // -----------------------------------------------------------------------
    // setDelayedObjDepthWrite / getDelayedObjDepthWrite
    // -----------------------------------------------------------------------
    runner.startTest("SoGLRenderAction: setDelayedObjDepthWrite(FALSE) round-trip");
    {
        SoGLRenderAction action(vp);
        action.setDelayedObjDepthWrite(FALSE);
        bool pass = (action.getDelayedObjDepthWrite() == FALSE);
        runner.endTest(pass, pass ? "" :
            "SoGLRenderAction setDelayedObjDepthWrite(FALSE) failed");
    }

    runner.startTest("SoGLRenderAction: setDelayedObjDepthWrite(TRUE) round-trip");
    {
        SoGLRenderAction action(vp);
        action.setDelayedObjDepthWrite(TRUE);
        bool pass = (action.getDelayedObjDepthWrite() == TRUE);
        runner.endTest(pass, pass ? "" :
            "SoGLRenderAction setDelayedObjDepthWrite(TRUE) failed");
    }

    // -----------------------------------------------------------------------
    // isRenderingTranspPaths / isRenderingTranspBackfaces
    // -----------------------------------------------------------------------
    runner.startTest("SoGLRenderAction: isRenderingTranspPaths defaults to FALSE");
    {
        SoGLRenderAction action(vp);
        bool pass = (action.isRenderingTranspPaths() == FALSE);
        runner.endTest(pass, pass ? "" :
            "SoGLRenderAction isRenderingTranspPaths should default to FALSE");
    }

    runner.startTest("SoGLRenderAction: isRenderingTranspBackfaces defaults to FALSE");
    {
        SoGLRenderAction action(vp);
        bool pass = (action.isRenderingTranspBackfaces() == FALSE);
        runner.endTest(pass, pass ? "" :
            "SoGLRenderAction isRenderingTranspBackfaces should default to FALSE");
    }

    // -----------------------------------------------------------------------
    // setUpdateArea / getUpdateArea
    // -----------------------------------------------------------------------
    runner.startTest("SoGLRenderAction: setUpdateArea / getUpdateArea round-trip");
    {
        SoGLRenderAction action(vp);
        SbVec2f origin(0.1f, 0.2f);
        SbVec2f size(0.5f, 0.6f);
        action.setUpdateArea(origin, size);
        SbVec2f gotOrigin, gotSize;
        action.getUpdateArea(gotOrigin, gotSize);
        bool pass = (std::fabs(gotOrigin[0] - 0.1f) < 1e-5f) &&
                    (std::fabs(gotOrigin[1] - 0.2f) < 1e-5f) &&
                    (std::fabs(gotSize[0]   - 0.5f) < 1e-5f) &&
                    (std::fabs(gotSize[1]   - 0.6f) < 1e-5f);
        runner.endTest(pass, pass ? "" :
            "SoGLRenderAction setUpdateArea/getUpdateArea round-trip failed");
    }

    // -----------------------------------------------------------------------
    // Class type
    // -----------------------------------------------------------------------
    runner.startTest("SoGLRenderAction: getClassTypeId is not badType");
    {
        bool pass = (SoGLRenderAction::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" :
            "SoGLRenderAction class type should be registered");
    }

    return runner.getSummary();
}
