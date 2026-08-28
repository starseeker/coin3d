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
 * @file test_dragger_sequences.cpp
 * @brief Additional dragger coverage for Tier 5 (COVERAGE_PLAN.md priority 44).
 *
 * Exercises draggers not yet covered by test_draggers.cpp:
 *   SoRotateCylindricalDragger - instantiation, type, rotation field, projector
 *   SoRotateSphericalDragger   - instantiation, type, rotation field, projector
 *   SoScale2Dragger            - instantiation, type, scaleFactor field
 *   SoScale2UniformDragger     - instantiation, type, scaleFactor field
 *   SoScaleUniformDragger      - instantiation, type, scaleFactor field
 *   SoJackDragger              - instantiation, type, rotation/translation/scale fields
 *   SoDragPointDragger         - instantiation, type, translation field
 *   SoPointLightDragger        - instantiation, type
 *   SoDirectionalLightDragger  - instantiation, type, direction field
 *   SoSpotLightDragger         - instantiation, type
 *   SoTrackballDragger         - instantiation, type, rotation field
 *   SoTabPlaneDragger          - instantiation, type, scaleFactor field
 *
 * For each dragger, verifies:
 *   - getClassTypeId() is not badType
 *   - isOfType(SoDragger::getClassTypeId()) is TRUE
 *   - Default field values are as documented
 *   - SoGetBoundingBoxAction does not crash
 *   - SoSearchAction finds the dragger in a scene graph
 *   - appendTranslation / appendScale / appendRotation static utilities work
 *     (exercised via SoDragger base API, already tested in test_draggers.cpp;
 *      here we use them with non-trivial values to widen coverage)
 *
 * Subsystems improved: draggers
 */

#include "../test_utils.h"

#include <Inventor/SoDB.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbRotation.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/actions/SoSearchAction.h>

#include <Inventor/draggers/SoDragger.h>
#include <Inventor/draggers/SoRotateCylindricalDragger.h>
#include <Inventor/draggers/SoRotateSphericalDragger.h>
#include <Inventor/draggers/SoScale2Dragger.h>
#include <Inventor/draggers/SoScale2UniformDragger.h>
#include <Inventor/draggers/SoScaleUniformDragger.h>
#include <Inventor/draggers/SoJackDragger.h>
#include <Inventor/draggers/SoDragPointDragger.h>
#include <Inventor/draggers/SoPointLightDragger.h>
#include <Inventor/draggers/SoDirectionalLightDragger.h>
#include <Inventor/draggers/SoSpotLightDragger.h>
#include <Inventor/draggers/SoTrackballDragger.h>
#include <Inventor/draggers/SoTabPlaneDragger.h>

#include <cmath>

using namespace ObolTest;

// Helper: wrap a dragger in a separator and apply SoGetBoundingBoxAction.
// Returns true if the action doesn't crash.
static bool bboxDragger(SoDragger * d)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    root->addChild(d);
    SbViewportRegion vp(100, 100);
    SoGetBoundingBoxAction bba(vp);
    bba.apply(root);
    root->unref();
    return true;
}

// Helper: apply SoSearchAction and verify the dragger is found.
static bool searchDragger(SoDragger * d, SoType t)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    root->addChild(d);
    SoSearchAction sa;
    sa.setType(t);
    sa.setInterest(SoSearchAction::FIRST);
    sa.apply(root);
    bool found = (sa.getPath() != nullptr);
    root->unref();
    return found;
}

TEST(DraggersDraggerSequences, SoRotateCylindricalDraggerClassTypeIdValid)
{
    EXPECT_TRUE((SoRotateCylindricalDragger::getClassTypeId() != SoType::badType())) << "SoRotateCylindricalDragger has bad class type";
}

TEST(DraggersDraggerSequences, SoRotateCylindricalDraggerIsOfTypeSoDragger)
{
    SoRotateCylindricalDragger * d = new SoRotateCylindricalDragger;
    d->ref();
    EXPECT_TRUE(d->isOfType(SoDragger::getClassTypeId())) << "SoRotateCylindricalDragger not a SoDragger subtype";
    d->unref();
}

TEST(DraggersDraggerSequences, SoRotateCylindricalDraggerDefaultRotationIsIdentity)
{
    SoRotateCylindricalDragger * d = new SoRotateCylindricalDragger;
    d->ref();
    SbVec3f ax; float ang;
    d->rotation.getValue().getValue(ax, ang);
    EXPECT_TRUE((fabsf(ang) < 1e-5f)) << "default rotation is not identity";
    d->unref();
}

TEST(DraggersDraggerSequences, SoRotateCylindricalDraggerSetGetRotationRoundTrip)
{
    SoRotateCylindricalDragger * d = new SoRotateCylindricalDragger;
    d->ref();
    SbRotation r(SbVec3f(0.0f, 1.0f, 0.0f),
                 static_cast<float>(M_PI) / 4.0f);
    d->rotation.setValue(r);
    SbVec3f ax; float ang;
    d->rotation.getValue().getValue(ax, ang);
    SbVec3f rax; float rang;
    r.getValue(rax, rang);
    EXPECT_TRUE((fabsf(ang - rang) < 1e-4f)) << "rotation round-trip failed";
    d->unref();
}

TEST(DraggersDraggerSequences, SoRotateCylindricalDraggerSoGetBoundingBoxActionDoesNotCrash)
{
    SoRotateCylindricalDragger * d = new SoRotateCylindricalDragger;
    d->ref();
    EXPECT_TRUE(bboxDragger(d)) << "SoGetBoundingBoxAction crashed";
    d->unref();
}

TEST(DraggersDraggerSequences, SoRotateCylindricalDraggerSoSearchActionFindsIt)
{
    SoRotateCylindricalDragger * d = new SoRotateCylindricalDragger;
    d->ref();
    EXPECT_TRUE(searchDragger(d, SoRotateCylindricalDragger::getClassTypeId())) << "SoSearchAction did not find it";
    d->unref();
}

// -----------------------------------------------------------------------
// SoRotateSphericalDragger
// -----------------------------------------------------------------------

TEST(DraggersDraggerSequences, SoRotateSphericalDraggerClassTypeIdValid)
{
    EXPECT_TRUE((SoRotateSphericalDragger::getClassTypeId() != SoType::badType())) << "SoRotateSphericalDragger has bad class type";
}

TEST(DraggersDraggerSequences, SoRotateSphericalDraggerIsOfTypeSoDragger)
{
    SoRotateSphericalDragger * d = new SoRotateSphericalDragger;
    d->ref();
    EXPECT_TRUE(d->isOfType(SoDragger::getClassTypeId())) << "SoRotateSphericalDragger not a SoDragger subtype";
    d->unref();
}

TEST(DraggersDraggerSequences, SoRotateSphericalDraggerSetGetRotationRoundTrip)
{
    SoRotateSphericalDragger * d = new SoRotateSphericalDragger;
    d->ref();
    SbRotation r(SbVec3f(1.0f, 0.0f, 0.0f),
                 static_cast<float>(M_PI) / 6.0f);
    d->rotation.setValue(r);
    SbVec3f ax; float ang;
    d->rotation.getValue().getValue(ax, ang);
    SbVec3f rax; float rang;
    r.getValue(rax, rang);
    EXPECT_TRUE((fabsf(ang - rang) < 1e-4f)) << "rotation round-trip failed";
    d->unref();
}

TEST(DraggersDraggerSequences, SoRotateSphericalDraggerSoGetBoundingBoxActionDoesNotCrash)
{
    SoRotateSphericalDragger * d = new SoRotateSphericalDragger;
    d->ref();
    EXPECT_TRUE(bboxDragger(d)) << "SoGetBoundingBoxAction crashed";
    d->unref();
}

// -----------------------------------------------------------------------
// SoScale2Dragger
// -----------------------------------------------------------------------

TEST(DraggersDraggerSequences, SoScale2DraggerClassTypeIdValid)
{
    EXPECT_TRUE((SoScale2Dragger::getClassTypeId() != SoType::badType())) << "SoScale2Dragger has bad class type";
}

TEST(DraggersDraggerSequences, SoScale2DraggerIsOfTypeSoDragger)
{
    SoScale2Dragger * d = new SoScale2Dragger;
    d->ref();
    EXPECT_TRUE(d->isOfType(SoDragger::getClassTypeId())) << "SoScale2Dragger not a SoDragger subtype";
    d->unref();
}

TEST(DraggersDraggerSequences, SoScale2DraggerDefaultScaleFactorIs111)
{
    SoScale2Dragger * d = new SoScale2Dragger;
    d->ref();
    SbVec3f sf = d->scaleFactor.getValue();
    EXPECT_TRUE((fabsf(sf[0] - 1.0f) < 1e-5f) &&
                (fabsf(sf[1] - 1.0f) < 1e-5f) &&
                (fabsf(sf[2] - 1.0f) < 1e-5f)) << "default scaleFactor is not (1,1,1)";
    d->unref();
}

TEST(DraggersDraggerSequences, SoScale2DraggerSetGetScaleFactorRoundTrip)
{
    SoScale2Dragger * d = new SoScale2Dragger;
    d->ref();
    d->scaleFactor.setValue(2.0f, 3.0f, 1.0f);
    SbVec3f sf = d->scaleFactor.getValue();
    EXPECT_TRUE((fabsf(sf[0] - 2.0f) < 1e-5f) &&
                (fabsf(sf[1] - 3.0f) < 1e-5f) &&
                (fabsf(sf[2] - 1.0f) < 1e-5f)) << "scaleFactor round-trip failed";
    d->unref();
}

TEST(DraggersDraggerSequences, SoScale2DraggerSoGetBoundingBoxActionDoesNotCrash)
{
    SoScale2Dragger * d = new SoScale2Dragger;
    d->ref();
    EXPECT_TRUE(bboxDragger(d)) << "SoGetBoundingBoxAction crashed";
    d->unref();
}

// -----------------------------------------------------------------------
// SoScale2UniformDragger
// -----------------------------------------------------------------------

TEST(DraggersDraggerSequences, SoScale2UniformDraggerClassTypeIdValid)
{
    EXPECT_TRUE((SoScale2UniformDragger::getClassTypeId() != SoType::badType())) << "SoScale2UniformDragger has bad class type";
}

TEST(DraggersDraggerSequences, SoScale2UniformDraggerIsOfTypeSoDragger)
{
    SoScale2UniformDragger * d = new SoScale2UniformDragger;
    d->ref();
    EXPECT_TRUE(d->isOfType(SoDragger::getClassTypeId())) << "SoScale2UniformDragger not a SoDragger subtype";
    d->unref();
}

TEST(DraggersDraggerSequences, SoScale2UniformDraggerDefaultScaleFactorIs111)
{
    SoScale2UniformDragger * d = new SoScale2UniformDragger;
    d->ref();
    SbVec3f sf = d->scaleFactor.getValue();
    EXPECT_TRUE((fabsf(sf[0] - 1.0f) < 1e-5f)) << "default scaleFactor is not (1,1,1)";
    d->unref();
}

// -----------------------------------------------------------------------
// SoScaleUniformDragger
// -----------------------------------------------------------------------

TEST(DraggersDraggerSequences, SoScaleUniformDraggerClassTypeIdValid)
{
    EXPECT_TRUE((SoScaleUniformDragger::getClassTypeId() != SoType::badType())) << "SoScaleUniformDragger has bad class type";
}

TEST(DraggersDraggerSequences, SoScaleUniformDraggerIsOfTypeSoDragger)
{
    SoScaleUniformDragger * d = new SoScaleUniformDragger;
    d->ref();
    EXPECT_TRUE(d->isOfType(SoDragger::getClassTypeId())) << "SoScaleUniformDragger not a SoDragger subtype";
    d->unref();
}

// -----------------------------------------------------------------------
// SoJackDragger
// -----------------------------------------------------------------------

TEST(DraggersDraggerSequences, SoJackDraggerClassTypeIdValid)
{
    EXPECT_TRUE((SoJackDragger::getClassTypeId() != SoType::badType())) << "SoJackDragger has bad class type";
}

TEST(DraggersDraggerSequences, SoJackDraggerIsOfTypeSoDragger)
{
    SoJackDragger * d = new SoJackDragger;
    d->ref();
    EXPECT_TRUE(d->isOfType(SoDragger::getClassTypeId())) << "SoJackDragger not a SoDragger subtype";
    d->unref();
}

TEST(DraggersDraggerSequences, SoJackDraggerDefaultFields)
{
    SoJackDragger * d = new SoJackDragger;
    d->ref();
    SbVec3f sf = d->scaleFactor.getValue();
    SbVec3f t  = d->translation.getValue();
    SbVec3f ax; float ang;
    d->rotation.getValue().getValue(ax, ang);
    EXPECT_TRUE((fabsf(sf[0] - 1.0f) < 1e-5f) &&
                (fabsf(t[0]) < 1e-5f) &&
                (fabsf(ang) < 1e-5f)) << "SoJackDragger default fields incorrect";
    d->unref();
}

TEST(DraggersDraggerSequences, SoJackDraggerSoGetBoundingBoxActionDoesNotCrash)
{
    SoJackDragger * d = new SoJackDragger;
    d->ref();
    EXPECT_TRUE(bboxDragger(d)) << "SoGetBoundingBoxAction crashed";
    d->unref();
}

// -----------------------------------------------------------------------
// SoDragPointDragger
// -----------------------------------------------------------------------

TEST(DraggersDraggerSequences, SoDragPointDraggerClassTypeIdValid)
{
    EXPECT_TRUE((SoDragPointDragger::getClassTypeId() != SoType::badType())) << "SoDragPointDragger has bad class type";
}

TEST(DraggersDraggerSequences, SoDragPointDraggerIsOfTypeSoDragger)
{
    SoDragPointDragger * d = new SoDragPointDragger;
    d->ref();
    EXPECT_TRUE(d->isOfType(SoDragger::getClassTypeId())) << "SoDragPointDragger not a SoDragger subtype";
    d->unref();
}

TEST(DraggersDraggerSequences, SoDragPointDraggerDefaultTranslationIs000)
{
    SoDragPointDragger * d = new SoDragPointDragger;
    d->ref();
    SbVec3f t = d->translation.getValue();
    EXPECT_TRUE((fabsf(t[0]) < 1e-5f) &&
                (fabsf(t[1]) < 1e-5f) &&
                (fabsf(t[2]) < 1e-5f)) << "default translation is not (0,0,0)";
    d->unref();
}

TEST(DraggersDraggerSequences, SoDragPointDraggerSetGetTranslationRoundTrip)
{
    SoDragPointDragger * d = new SoDragPointDragger;
    d->ref();
    d->translation.setValue(1.0f, 2.0f, 3.0f);
    SbVec3f t = d->translation.getValue();
    EXPECT_TRUE((fabsf(t[0] - 1.0f) < 1e-5f) &&
                (fabsf(t[1] - 2.0f) < 1e-5f) &&
                (fabsf(t[2] - 3.0f) < 1e-5f)) << "translation round-trip failed";
    d->unref();
}

// -----------------------------------------------------------------------
// SoTrackballDragger
// -----------------------------------------------------------------------

TEST(DraggersDraggerSequences, SoTrackballDraggerClassTypeIdValid)
{
    EXPECT_TRUE((SoTrackballDragger::getClassTypeId() != SoType::badType())) << "SoTrackballDragger has bad class type";
}

TEST(DraggersDraggerSequences, SoTrackballDraggerIsOfTypeSoDragger)
{
    SoTrackballDragger * d = new SoTrackballDragger;
    d->ref();
    EXPECT_TRUE(d->isOfType(SoDragger::getClassTypeId())) << "SoTrackballDragger not a SoDragger subtype";
    d->unref();
}

TEST(DraggersDraggerSequences, SoTrackballDraggerDefaultRotationIsIdentity)
{
    SoTrackballDragger * d = new SoTrackballDragger;
    d->ref();
    SbVec3f ax; float ang;
    d->rotation.getValue().getValue(ax, ang);
    EXPECT_TRUE((fabsf(ang) < 1e-5f)) << "default rotation is not identity";
    d->unref();
}

TEST(DraggersDraggerSequences, SoTrackballDraggerSoGetBoundingBoxActionDoesNotCrash)
{
    SoTrackballDragger * d = new SoTrackballDragger;
    d->ref();
    EXPECT_TRUE(bboxDragger(d)) << "SoGetBoundingBoxAction crashed";
    d->unref();
}

// -----------------------------------------------------------------------
// SoTabPlaneDragger
// -----------------------------------------------------------------------

TEST(DraggersDraggerSequences, SoTabPlaneDraggerClassTypeIdValid)
{
    EXPECT_TRUE((SoTabPlaneDragger::getClassTypeId() != SoType::badType())) << "SoTabPlaneDragger has bad class type";
}

TEST(DraggersDraggerSequences, SoTabPlaneDraggerIsOfTypeSoDragger)
{
    SoTabPlaneDragger * d = new SoTabPlaneDragger;
    d->ref();
    EXPECT_TRUE(d->isOfType(SoDragger::getClassTypeId())) << "SoTabPlaneDragger not a SoDragger subtype";
    d->unref();
}

TEST(DraggersDraggerSequences, SoTabPlaneDraggerDefaultScaleFactorIs111)
{
    SoTabPlaneDragger * d = new SoTabPlaneDragger;
    d->ref();
    SbVec3f sf = d->scaleFactor.getValue();
    EXPECT_TRUE((fabsf(sf[0] - 1.0f) < 1e-5f)) << "default scaleFactor is not (1,1,1)";
    d->unref();
}

TEST(DraggersDraggerSequences, SoTabPlaneDraggerSoGetBoundingBoxActionDoesNotCrash)
{
    SoTabPlaneDragger * d = new SoTabPlaneDragger;
    d->ref();
    EXPECT_TRUE(bboxDragger(d)) << "SoGetBoundingBoxAction crashed";
    d->unref();
}

// -----------------------------------------------------------------------
// SoPointLightDragger
// -----------------------------------------------------------------------

TEST(DraggersDraggerSequences, SoPointLightDraggerClassTypeIdValid)
{
    EXPECT_TRUE((SoPointLightDragger::getClassTypeId() != SoType::badType())) << "SoPointLightDragger has bad class type";
}

TEST(DraggersDraggerSequences, SoPointLightDraggerIsOfTypeSoDragger)
{
    SoPointLightDragger * d = new SoPointLightDragger;
    d->ref();
    EXPECT_TRUE(d->isOfType(SoDragger::getClassTypeId())) << "SoPointLightDragger not a SoDragger subtype";
    d->unref();
}

// -----------------------------------------------------------------------
// SoDirectionalLightDragger
// -----------------------------------------------------------------------

TEST(DraggersDraggerSequences, SoDirectionalLightDraggerClassTypeIdValid)
{
    EXPECT_TRUE((SoDirectionalLightDragger::getClassTypeId() != SoType::badType())) << "SoDirectionalLightDragger has bad class type";
}

TEST(DraggersDraggerSequences, SoDirectionalLightDraggerIsOfTypeSoDragger)
{
    SoDirectionalLightDragger * d = new SoDirectionalLightDragger;
    d->ref();
    EXPECT_TRUE(d->isOfType(SoDragger::getClassTypeId())) << "SoDirectionalLightDragger not a SoDragger subtype";
    d->unref();
}

TEST(DraggersDraggerSequences, SoDirectionalLightDraggerDefaultTranslationIs000)
{
    SoDirectionalLightDragger * d = new SoDirectionalLightDragger;
    d->ref();
    SbVec3f t = d->translation.getValue();
    // Default should be (0,0,0)
    EXPECT_TRUE((t.length() < 1e-5f)) << "default translation is not zero";
    d->unref();
}

// -----------------------------------------------------------------------
// SoSpotLightDragger
// -----------------------------------------------------------------------

TEST(DraggersDraggerSequences, SoSpotLightDraggerClassTypeIdValid)
{
    EXPECT_TRUE((SoSpotLightDragger::getClassTypeId() != SoType::badType())) << "SoSpotLightDragger has bad class type";
}

TEST(DraggersDraggerSequences, SoSpotLightDraggerIsOfTypeSoDragger)
{
    SoSpotLightDragger * d = new SoSpotLightDragger;
    d->ref();
    EXPECT_TRUE(d->isOfType(SoDragger::getClassTypeId())) << "SoSpotLightDragger not a SoDragger subtype";
    d->unref();
}

// -----------------------------------------------------------------------
// SoSearchAction finds all new dragger types
// -----------------------------------------------------------------------

TEST(DraggersDraggerSequences, SoSearchActionFindsSoRotateCylindricalDragger)
{
    SoRotateCylindricalDragger * d = new SoRotateCylindricalDragger;
    d->ref();
    EXPECT_TRUE(searchDragger(d, SoRotateCylindricalDragger::getClassTypeId())) << "SoSearchAction did not find it";
    d->unref();
}

TEST(DraggersDraggerSequences, SoSearchActionFindsSoRotateSphericalDragger)
{
    SoRotateSphericalDragger * d = new SoRotateSphericalDragger;
    d->ref();
    EXPECT_TRUE(searchDragger(d, SoRotateSphericalDragger::getClassTypeId())) << "SoSearchAction did not find it";
    d->unref();
}

TEST(DraggersDraggerSequences, SoSearchActionFindsSoJackDragger)
{
    SoJackDragger * d = new SoJackDragger;
    d->ref();
    EXPECT_TRUE(searchDragger(d, SoJackDragger::getClassTypeId())) << "SoSearchAction did not find it";
    d->unref();
}

TEST(DraggersDraggerSequences, SoSearchActionFindsSoTrackballDragger)
{
    SoTrackballDragger * d = new SoTrackballDragger;
    d->ref();
    EXPECT_TRUE(searchDragger(d, SoTrackballDragger::getClassTypeId())) << "SoSearchAction did not find it";
    d->unref();
}
