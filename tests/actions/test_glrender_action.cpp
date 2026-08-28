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
    EXPECT_TRUE((got.getWindowSize()[0] == 512) &&
                (got.getWindowSize()[1] == 384)) << "SoGLRenderAction viewport region mismatch";
}

TEST(ActionsGlrenderAction, SoGLRenderActionSetViewportRegionRoundTrip)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    SbViewportRegion vp2(128, 64);
    action.setViewportRegion(vp2);
    const SbViewportRegion & got = action.getViewportRegion();
    EXPECT_TRUE((got.getWindowSize()[0] == 128) &&
                (got.getWindowSize()[1] == 64)) << "SoGLRenderAction setViewportRegion round-trip failed";
}

// -----------------------------------------------------------------------
// setTransparencyType / getTransparencyType
// -----------------------------------------------------------------------

TEST(ActionsGlrenderAction, SoGLRenderActionDefaultTransparencyTypeIsBLEND)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    EXPECT_TRUE((action.getTransparencyType() ==
                 SoGLRenderAction::BLEND)) << "SoGLRenderAction default transparency should be BLEND";
}

TEST(ActionsGlrenderAction, SoGLRenderActionSetTransparencyTypeBLENDRoundTrip)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    action.setTransparencyType(SoGLRenderAction::BLEND);
    EXPECT_TRUE((action.getTransparencyType() == SoGLRenderAction::BLEND)) << "SoGLRenderAction transparency BLEND round-trip failed";
}

TEST(ActionsGlrenderAction, SoGLRenderActionSetTransparencyTypeDELAYEDBLENDRoundTrip)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    action.setTransparencyType(SoGLRenderAction::DELAYED_BLEND);
    EXPECT_TRUE((action.getTransparencyType() ==
                 SoGLRenderAction::DELAYED_BLEND)) << "SoGLRenderAction transparency DELAYED_BLEND round-trip failed";
}

TEST(ActionsGlrenderAction, SoGLRenderActionSetTransparencyTypeSORTEDOBJECTBLENDRoundTrip)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    action.setTransparencyType(SoGLRenderAction::SORTED_OBJECT_BLEND);
    EXPECT_TRUE((action.getTransparencyType() ==
                 SoGLRenderAction::SORTED_OBJECT_BLEND)) << "SoGLRenderAction transparency SORTED_OBJECT_BLEND round-trip failed";
}

TEST(ActionsGlrenderAction, SoGLRenderActionSetTransparencyTypeADDRoundTrip)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    action.setTransparencyType(SoGLRenderAction::ADD);
    EXPECT_TRUE((action.getTransparencyType() == SoGLRenderAction::ADD)) << "SoGLRenderAction transparency ADD round-trip failed";
}

TEST(ActionsGlrenderAction, SoGLRenderActionSetTransparencyTypeNONERoundTrip)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    action.setTransparencyType(SoGLRenderAction::NONE);
    EXPECT_TRUE((action.getTransparencyType() == SoGLRenderAction::NONE)) << "SoGLRenderAction transparency NONE round-trip failed";
}

// -----------------------------------------------------------------------
// setSmoothing / isSmoothing
// -----------------------------------------------------------------------

TEST(ActionsGlrenderAction, SoGLRenderActionIsSmoothingDefaultsToFALSE)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    EXPECT_TRUE((action.isSmoothing() == FALSE)) << "SoGLRenderAction isSmoothing should default to FALSE";
}

TEST(ActionsGlrenderAction, SoGLRenderActionSetSmoothingTRUERoundTrip)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    action.setSmoothing(TRUE);
    EXPECT_TRUE((action.isSmoothing() == TRUE)) << "SoGLRenderAction setSmoothing(TRUE) failed";
}

TEST(ActionsGlrenderAction, SoGLRenderActionSetSmoothingFALSERoundTrip)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    action.setSmoothing(TRUE);
    action.setSmoothing(FALSE);
    EXPECT_TRUE((action.isSmoothing() == FALSE)) << "SoGLRenderAction setSmoothing(FALSE) failed";
}

// -----------------------------------------------------------------------
// setNumPasses / getNumPasses
// -----------------------------------------------------------------------

TEST(ActionsGlrenderAction, SoGLRenderActionGetNumPassesDefaultsTo1)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    EXPECT_TRUE((action.getNumPasses() == 1)) << "SoGLRenderAction getNumPasses should default to 1";
}

TEST(ActionsGlrenderAction, SoGLRenderActionSetNumPasses4RoundTrip)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    action.setNumPasses(4);
    EXPECT_TRUE((action.getNumPasses() == 4)) << "SoGLRenderAction setNumPasses(4) round-trip failed";
}

// -----------------------------------------------------------------------
// setCacheContext / getCacheContext
// -----------------------------------------------------------------------

TEST(ActionsGlrenderAction, SoGLRenderActionSetCacheContextGetCacheContextRoundTrip)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    action.setCacheContext(42u);
    EXPECT_TRUE((action.getCacheContext() == 42u)) << "SoGLRenderAction setCacheContext round-trip failed";
}

TEST(ActionsGlrenderAction, SoGLRenderActionContextManagerRoundTrip)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    SoDB::ContextManager * manager = SoDB::getContextManager();
    action.setContextManager(manager);
    EXPECT_NE(manager, nullptr);
    EXPECT_EQ(action.getContextManager(), manager);
    action.setContextManager(NULL);
    EXPECT_EQ(action.getContextManager(), nullptr);
}

// -----------------------------------------------------------------------
// setPassUpdate / isPassUpdate
// -----------------------------------------------------------------------

TEST(ActionsGlrenderAction, SoGLRenderActionIsPassUpdateDefaultsToFALSE)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    EXPECT_TRUE((action.isPassUpdate() == FALSE)) << "SoGLRenderAction isPassUpdate should default to FALSE";
}

TEST(ActionsGlrenderAction, SoGLRenderActionSetPassUpdateTRUERoundTrip)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    action.setPassUpdate(TRUE);
    EXPECT_TRUE((action.isPassUpdate() == TRUE)) << "SoGLRenderAction setPassUpdate(TRUE) failed";
}

// -----------------------------------------------------------------------
// setRenderingIsRemote / getRenderingIsRemote
// -----------------------------------------------------------------------

TEST(ActionsGlrenderAction, SoGLRenderActionGetRenderingIsRemoteDefaultsToFALSE)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    EXPECT_TRUE((action.getRenderingIsRemote() == FALSE)) << "SoGLRenderAction getRenderingIsRemote should default to FALSE";
}

TEST(ActionsGlrenderAction, SoGLRenderActionSetRenderingIsRemoteTRUERoundTrip)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    action.setRenderingIsRemote(TRUE);
    EXPECT_TRUE((action.getRenderingIsRemote() == TRUE)) << "SoGLRenderAction setRenderingIsRemote(TRUE) failed";
}

// -----------------------------------------------------------------------
// setSortedLayersNumPasses / getSortedLayersNumPasses
// -----------------------------------------------------------------------

TEST(ActionsGlrenderAction, SoGLRenderActionSetSortedLayersNumPassesRoundTrip)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    action.setSortedLayersNumPasses(8);
    EXPECT_TRUE((action.getSortedLayersNumPasses() == 8)) << "SoGLRenderAction setSortedLayersNumPasses round-trip failed";
}

// -----------------------------------------------------------------------
// isRenderingDelayedPaths
// -----------------------------------------------------------------------

TEST(ActionsGlrenderAction, SoGLRenderActionIsRenderingDelayedPathsDefaultsToFALSE)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    EXPECT_TRUE((action.isRenderingDelayedPaths() == FALSE)) << "SoGLRenderAction isRenderingDelayedPaths should default to FALSE";
}

// -----------------------------------------------------------------------
// setDelayedObjDepthWrite / getDelayedObjDepthWrite
// -----------------------------------------------------------------------

TEST(ActionsGlrenderAction, SoGLRenderActionSetDelayedObjDepthWriteFALSERoundTrip)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    action.setDelayedObjDepthWrite(FALSE);
    EXPECT_TRUE((action.getDelayedObjDepthWrite() == FALSE)) << "SoGLRenderAction setDelayedObjDepthWrite(FALSE) failed";
}

TEST(ActionsGlrenderAction, SoGLRenderActionSetDelayedObjDepthWriteTRUERoundTrip)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    action.setDelayedObjDepthWrite(TRUE);
    EXPECT_TRUE((action.getDelayedObjDepthWrite() == TRUE)) << "SoGLRenderAction setDelayedObjDepthWrite(TRUE) failed";
}

// -----------------------------------------------------------------------
// isRenderingTranspPaths / isRenderingTranspBackfaces
// -----------------------------------------------------------------------

TEST(ActionsGlrenderAction, SoGLRenderActionIsRenderingTranspPathsDefaultsToFALSE)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    EXPECT_TRUE((action.isRenderingTranspPaths() == FALSE)) << "SoGLRenderAction isRenderingTranspPaths should default to FALSE";
}

TEST(ActionsGlrenderAction, SoGLRenderActionIsRenderingTranspBackfacesDefaultsToFALSE)
{
    const SbViewportRegion vp(512, 384);
    SoGLRenderAction action(vp);
    EXPECT_TRUE((action.isRenderingTranspBackfaces() == FALSE)) << "SoGLRenderAction isRenderingTranspBackfaces should default to FALSE";
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
    EXPECT_TRUE((std::fabs(gotOrigin[0] - 0.1f) < 1e-5f) &&
                (std::fabs(gotOrigin[1] - 0.2f) < 1e-5f) &&
                (std::fabs(gotSize[0]   - 0.5f) < 1e-5f) &&
                (std::fabs(gotSize[1]   - 0.6f) < 1e-5f)) << "SoGLRenderAction setUpdateArea/getUpdateArea round-trip failed";
}

// -----------------------------------------------------------------------
// Class type
// -----------------------------------------------------------------------

TEST(ActionsGlrenderAction, SoGLRenderActionGetClassTypeIdIsNotBadType)
{
    const SbViewportRegion vp(512, 384);
    EXPECT_TRUE((SoGLRenderAction::getClassTypeId() != SoType::badType())) << "SoGLRenderAction class type should be registered";
}
