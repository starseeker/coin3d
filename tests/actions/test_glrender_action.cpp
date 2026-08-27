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
 *   - setContextManager / getContextManager
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

using namespace ObolTest;

TEST(ActionsGlrenderAction, SoGLRenderActionGetViewportRegionMatchesConstructorArg)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    const SbViewportRegion & got = action.getViewportRegion();
    bool pass = (got.getWindowSize()[0] == 512) &&
                (got.getWindowSize()[1] == 384);
    EXPECT_TRUE(pass) << "SoGLRenderAction viewport region mismatch";
}

TEST(ActionsGlrenderAction, SoGLRenderActionSetViewportRegionRoundTrip)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    SbViewportRegion vp2(128, 64);
    action.setViewportRegion(vp2);
    const SbViewportRegion & got = action.getViewportRegion();
    bool pass = (got.getWindowSize()[0] == 128) &&
                (got.getWindowSize()[1] == 64);
    EXPECT_TRUE(pass) << "SoGLRenderAction setViewportRegion round-trip failed";
}

// -----------------------------------------------------------------------
// setTransparencyType / getTransparencyType
// -----------------------------------------------------------------------

TEST(ActionsGlrenderAction, SoGLRenderActionDefaultTransparencyTypeIsBLEND)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    bool pass = (action.getTransparencyType() ==
                 SoGLRenderAction::BLEND);
    EXPECT_TRUE(pass) << "SoGLRenderAction default transparency should be BLEND";
}

TEST(ActionsGlrenderAction, SoGLRenderActionSetTransparencyTypeBLENDRoundTrip)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    action.setTransparencyType(SoGLRenderAction::BLEND);
    bool pass = (action.getTransparencyType() == SoGLRenderAction::BLEND);
    EXPECT_TRUE(pass) << "SoGLRenderAction transparency BLEND round-trip failed";
}

TEST(ActionsGlrenderAction, SoGLRenderActionSetTransparencyTypeDELAYEDBLENDRoundTrip)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    action.setTransparencyType(SoGLRenderAction::DELAYED_BLEND);
    bool pass = (action.getTransparencyType() ==
                 SoGLRenderAction::DELAYED_BLEND);
    EXPECT_TRUE(pass) << "SoGLRenderAction transparency DELAYED_BLEND round-trip failed";
}

TEST(ActionsGlrenderAction, SoGLRenderActionSetTransparencyTypeSORTEDOBJECTBLENDRoundTrip)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    action.setTransparencyType(SoGLRenderAction::SORTED_OBJECT_BLEND);
    bool pass = (action.getTransparencyType() ==
                 SoGLRenderAction::SORTED_OBJECT_BLEND);
    EXPECT_TRUE(pass) << "SoGLRenderAction transparency SORTED_OBJECT_BLEND round-trip failed";
}

TEST(ActionsGlrenderAction, SoGLRenderActionSetTransparencyTypeADDRoundTrip)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    action.setTransparencyType(SoGLRenderAction::ADD);
    bool pass = (action.getTransparencyType() == SoGLRenderAction::ADD);
    EXPECT_TRUE(pass) << "SoGLRenderAction transparency ADD round-trip failed";
}

TEST(ActionsGlrenderAction, SoGLRenderActionSetTransparencyTypeNONERoundTrip)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    action.setTransparencyType(SoGLRenderAction::NONE);
    bool pass = (action.getTransparencyType() == SoGLRenderAction::NONE);
    EXPECT_TRUE(pass) << "SoGLRenderAction transparency NONE round-trip failed";
}

// -----------------------------------------------------------------------
// setSmoothing / isSmoothing
// -----------------------------------------------------------------------

TEST(ActionsGlrenderAction, SoGLRenderActionIsSmoothingDefaultsToFALSE)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    bool pass = (action.isSmoothing() == FALSE);
    EXPECT_TRUE(pass) << "SoGLRenderAction isSmoothing should default to FALSE";
}

TEST(ActionsGlrenderAction, SoGLRenderActionSetSmoothingTRUERoundTrip)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    action.setSmoothing(TRUE);
    bool pass = (action.isSmoothing() == TRUE);
    EXPECT_TRUE(pass) << "SoGLRenderAction setSmoothing(TRUE) failed";
}

TEST(ActionsGlrenderAction, SoGLRenderActionSetSmoothingFALSERoundTrip)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    action.setSmoothing(TRUE);
    action.setSmoothing(FALSE);
    bool pass = (action.isSmoothing() == FALSE);
    EXPECT_TRUE(pass) << "SoGLRenderAction setSmoothing(FALSE) failed";
}

// -----------------------------------------------------------------------
// setNumPasses / getNumPasses
// -----------------------------------------------------------------------

TEST(ActionsGlrenderAction, SoGLRenderActionGetNumPassesDefaultsTo1)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    bool pass = (action.getNumPasses() == 1);
    EXPECT_TRUE(pass) << "SoGLRenderAction getNumPasses should default to 1";
}

TEST(ActionsGlrenderAction, SoGLRenderActionSetNumPasses4RoundTrip)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    action.setNumPasses(4);
    bool pass = (action.getNumPasses() == 4);
    EXPECT_TRUE(pass) << "SoGLRenderAction setNumPasses(4) round-trip failed";
}

// -----------------------------------------------------------------------
// setCacheContext / getCacheContext
// -----------------------------------------------------------------------

TEST(ActionsGlrenderAction, SoGLRenderActionSetCacheContextGetCacheContextRoundTrip)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    action.setCacheContext(42u);
    bool pass = (action.getCacheContext() == 42u);
    EXPECT_TRUE(pass) << "SoGLRenderAction setCacheContext round-trip failed";
}

TEST(ActionsGlrenderAction, SoGLRenderActionContextManagerRoundTrip)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    SoDB::ContextManager * manager = SoDB::getContextManager();
    action.setContextManager(manager);
    bool pass = manager && action.getContextManager() == manager;
    action.setContextManager(NULL);
    pass = pass && action.getContextManager() == NULL;
    EXPECT_TRUE(pass) << "SoGLRenderAction context manager round-trip failed";
}

// -----------------------------------------------------------------------
// setPassUpdate / isPassUpdate
// -----------------------------------------------------------------------

TEST(ActionsGlrenderAction, SoGLRenderActionIsPassUpdateDefaultsToFALSE)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    bool pass = (action.isPassUpdate() == FALSE);
    EXPECT_TRUE(pass) << "SoGLRenderAction isPassUpdate should default to FALSE";
}

TEST(ActionsGlrenderAction, SoGLRenderActionSetPassUpdateTRUERoundTrip)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    action.setPassUpdate(TRUE);
    bool pass = (action.isPassUpdate() == TRUE);
    EXPECT_TRUE(pass) << "SoGLRenderAction setPassUpdate(TRUE) failed";
}

// -----------------------------------------------------------------------
// setRenderingIsRemote / getRenderingIsRemote
// -----------------------------------------------------------------------

TEST(ActionsGlrenderAction, SoGLRenderActionGetRenderingIsRemoteDefaultsToFALSE)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    bool pass = (action.getRenderingIsRemote() == FALSE);
    EXPECT_TRUE(pass) << "SoGLRenderAction getRenderingIsRemote should default to FALSE";
}

TEST(ActionsGlrenderAction, SoGLRenderActionSetRenderingIsRemoteTRUERoundTrip)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    action.setRenderingIsRemote(TRUE);
    bool pass = (action.getRenderingIsRemote() == TRUE);
    EXPECT_TRUE(pass) << "SoGLRenderAction setRenderingIsRemote(TRUE) failed";
}

// -----------------------------------------------------------------------
// setSortedLayersNumPasses / getSortedLayersNumPasses
// -----------------------------------------------------------------------

TEST(ActionsGlrenderAction, SoGLRenderActionSetSortedLayersNumPassesRoundTrip)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    action.setSortedLayersNumPasses(8);
    bool pass = (action.getSortedLayersNumPasses() == 8);
    EXPECT_TRUE(pass) << "SoGLRenderAction setSortedLayersNumPasses round-trip failed";
}

// -----------------------------------------------------------------------
// isRenderingDelayedPaths
// -----------------------------------------------------------------------

TEST(ActionsGlrenderAction, SoGLRenderActionIsRenderingDelayedPathsDefaultsToFALSE)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    bool pass = (action.isRenderingDelayedPaths() == FALSE);
    EXPECT_TRUE(pass) << "SoGLRenderAction isRenderingDelayedPaths should default to FALSE";
}

// -----------------------------------------------------------------------
// setDelayedObjDepthWrite / getDelayedObjDepthWrite
// -----------------------------------------------------------------------

TEST(ActionsGlrenderAction, SoGLRenderActionSetDelayedObjDepthWriteFALSERoundTrip)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    action.setDelayedObjDepthWrite(FALSE);
    bool pass = (action.getDelayedObjDepthWrite() == FALSE);
    EXPECT_TRUE(pass) << "SoGLRenderAction setDelayedObjDepthWrite(FALSE) failed";
}

TEST(ActionsGlrenderAction, SoGLRenderActionSetDelayedObjDepthWriteTRUERoundTrip)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    action.setDelayedObjDepthWrite(TRUE);
    bool pass = (action.getDelayedObjDepthWrite() == TRUE);
    EXPECT_TRUE(pass) << "SoGLRenderAction setDelayedObjDepthWrite(TRUE) failed";
}

// -----------------------------------------------------------------------
// isRenderingTranspPaths / isRenderingTranspBackfaces
// -----------------------------------------------------------------------

TEST(ActionsGlrenderAction, SoGLRenderActionIsRenderingTranspPathsDefaultsToFALSE)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    bool pass = (action.isRenderingTranspPaths() == FALSE);
    EXPECT_TRUE(pass) << "SoGLRenderAction isRenderingTranspPaths should default to FALSE";
}

TEST(ActionsGlrenderAction, SoGLRenderActionIsRenderingTranspBackfacesDefaultsToFALSE)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    bool pass = (action.isRenderingTranspBackfaces() == FALSE);
    EXPECT_TRUE(pass) << "SoGLRenderAction isRenderingTranspBackfaces should default to FALSE";
}

// -----------------------------------------------------------------------
// setUpdateArea / getUpdateArea
// -----------------------------------------------------------------------

TEST(ActionsGlrenderAction, SoGLRenderActionSetUpdateAreaGetUpdateAreaRoundTrip)
{
    const SbViewportRegion vp(512, 384);
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
    EXPECT_TRUE(pass) << "SoGLRenderAction setUpdateArea/getUpdateArea round-trip failed";
}

// -----------------------------------------------------------------------
// Class type
// -----------------------------------------------------------------------

TEST(ActionsGlrenderAction, SoGLRenderActionGetClassTypeIdIsNotBadType)
{
    const SbViewportRegion vp(512, 384);
    bool pass = (SoGLRenderAction::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoGLRenderAction class type should be registered";
}
