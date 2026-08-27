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
 * @file test_draggers.cpp
 * @brief Tests for Coin3D dragger classes.
 *
 * Exercises the dragger infrastructure without requiring a display or GL
 * context:
 *   - SoTranslate1Dragger: instantiation, type, translation field, motionMatrix
 *   - SoTranslate2Dragger: instantiation, type, translation field
 *   - SoScale1Dragger:     instantiation, type, scaleFactor field
 *   - SoRotateDiscDragger: instantiation, type, rotation field
 *   - SoDragger base API:  isActive, minGesture, projectorEpsilon,
 *                          callback registration, enableValueChangedCallbacks,
 *                          static matrix utilities (appendTranslation,
 *                          appendScale, appendRotation)
 *   - SoSearchAction traversal over a dragger-containing scene graph
 *
 * Complex draggers (previously crashing due to SbString::vsprintf va_list bug):
 *   - SoHandleBoxDragger:   instantiation, type, scaleFactor/translation fields,
 *                           nodekit catalog, callback registration
 *   - SoTabBoxDragger:      instantiation, type, scaleFactor/translation fields,
 *                           nodekit catalog, SoSearchAction traversal
 *   - SoTransformBoxDragger: instantiation, type, rotation/translation/scaleFactor
 *   - SoTransformerDragger: instantiation, type, fields, minDiscRotDot
 *   - SoCenterballDragger:  instantiation, type, rotation/center fields
 *
 * Subsystems improved: draggers (Tier 3, COVERAGE_PLAN.md item 21)
 */

#include "../test_utils.h"

#include <Inventor/SoDB.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbRotation.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/draggers/SoDragger.h>
#include <Inventor/draggers/SoTranslate1Dragger.h>
#include <Inventor/draggers/SoTranslate2Dragger.h>
#include <Inventor/draggers/SoScale1Dragger.h>
#include <Inventor/draggers/SoRotateDiscDragger.h>
#include <Inventor/draggers/SoHandleBoxDragger.h>
#include <Inventor/draggers/SoTabBoxDragger.h>
#include <Inventor/draggers/SoTransformBoxDragger.h>
#include <Inventor/draggers/SoTransformerDragger.h>
#include <Inventor/draggers/SoCenterballDragger.h>
#include <Inventor/nodekits/SoNodekitCatalog.h>

#include <cmath>

using namespace ObolTest;

// ---------------------------------------------------------------------------
// Callback tracking helpers
// ---------------------------------------------------------------------------
static int s_start_count  = 0;
static int s_motion_count = 0;
static int s_finish_count = 0;
static int s_changed_count= 0;

static void countStartCB (void * /*data*/, SoDragger * /*d*/) { ++s_start_count;  }
static void countMotionCB(void * /*data*/, SoDragger * /*d*/) { ++s_motion_count; }
static void countFinishCB(void * /*data*/, SoDragger * /*d*/) { ++s_finish_count; }
static void countChangedCB(void* /*data*/, SoDragger* /*d*/) { ++s_changed_count; }

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static bool matrixIsIdentity(const SbMatrix & m)
{
    SbMatrix id = SbMatrix::identity();
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            if (fabsf(m[r][c] - id[r][c]) > 1e-5f) return false;
    return true;
}

TEST(DraggersDraggers, SoTranslate1DraggerInstantiationAndTypeCheck)
{
    SoTranslate1Dragger *d = new SoTranslate1Dragger;
    d->ref();
    bool pass = (d->getTypeId() != SoType::badType()) &&
                d->isOfType(SoDragger::getClassTypeId());
    d->unref();
    EXPECT_TRUE(pass) << "SoTranslate1Dragger bad type or not SoDragger subtype";
}

// -----------------------------------------------------------------------
// SoTranslate1Dragger: translation field default is (0,0,0)
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoTranslate1DraggerTranslationDefaultIs000)
{
    SoTranslate1Dragger *d = new SoTranslate1Dragger;
    d->ref();
    SbVec3f t = d->translation.getValue();
    bool pass = (fabsf(t[0]) < 1e-5f) &&
                (fabsf(t[1]) < 1e-5f) &&
                (fabsf(t[2]) < 1e-5f);
    d->unref();
    EXPECT_TRUE(pass) << "SoTranslate1Dragger default translation is not (0,0,0)";
}

// -----------------------------------------------------------------------
// SoTranslate1Dragger: set/get translation field
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoTranslate1DraggerSetGetTranslationField)
{
    SoTranslate1Dragger *d = new SoTranslate1Dragger;
    d->ref();
    d->translation.setValue(1.0f, 2.0f, 3.0f);
    SbVec3f t = d->translation.getValue();
    bool pass = (fabsf(t[0] - 1.0f) < 1e-5f) &&
                (fabsf(t[1] - 2.0f) < 1e-5f) &&
                (fabsf(t[2] - 3.0f) < 1e-5f);
    d->unref();
    EXPECT_TRUE(pass) << "SoTranslate1Dragger translation set/get failed";
}

// -----------------------------------------------------------------------
// SoTranslate2Dragger: instantiation and type
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoTranslate2DraggerInstantiationAndTypeCheck)
{
    SoTranslate2Dragger *d = new SoTranslate2Dragger;
    d->ref();
    bool pass = (d->getTypeId() != SoType::badType()) &&
                d->isOfType(SoDragger::getClassTypeId());
    d->unref();
    EXPECT_TRUE(pass) << "SoTranslate2Dragger bad type or not SoDragger subtype";
}

// -----------------------------------------------------------------------
// SoTranslate2Dragger: translation field default is (0,0,0)
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoTranslate2DraggerTranslationDefaultIs000)
{
    SoTranslate2Dragger *d = new SoTranslate2Dragger;
    d->ref();
    SbVec3f t = d->translation.getValue();
    bool pass = (fabsf(t[0]) < 1e-5f) &&
                (fabsf(t[1]) < 1e-5f) &&
                (fabsf(t[2]) < 1e-5f);
    d->unref();
    EXPECT_TRUE(pass) << "SoTranslate2Dragger default translation not (0,0,0)";
}

// -----------------------------------------------------------------------
// SoTranslate2Dragger: set/get translation field
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoTranslate2DraggerSetGetTranslationField)
{
    SoTranslate2Dragger *d = new SoTranslate2Dragger;
    d->ref();
    d->translation.setValue(4.0f, 5.0f, 0.0f);
    SbVec3f t = d->translation.getValue();
    bool pass = (fabsf(t[0] - 4.0f) < 1e-5f) &&
                (fabsf(t[1] - 5.0f) < 1e-5f) &&
                (fabsf(t[2])        < 1e-5f);
    d->unref();
    EXPECT_TRUE(pass) << "SoTranslate2Dragger translation set/get failed";
}

// -----------------------------------------------------------------------
// SoScale1Dragger: instantiation and type
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoScale1DraggerInstantiationAndTypeCheck)
{
    SoScale1Dragger *d = new SoScale1Dragger;
    d->ref();
    bool pass = (d->getTypeId() != SoType::badType()) &&
                d->isOfType(SoDragger::getClassTypeId());
    d->unref();
    EXPECT_TRUE(pass) << "SoScale1Dragger bad type or not SoDragger subtype";
}

// -----------------------------------------------------------------------
// SoScale1Dragger: scaleFactor field default is (1,1,1)
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoScale1DraggerScaleFactorDefaultIs111)
{
    SoScale1Dragger *d = new SoScale1Dragger;
    d->ref();
    SbVec3f sf = d->scaleFactor.getValue();
    bool pass = (fabsf(sf[0] - 1.0f) < 1e-5f) &&
                (fabsf(sf[1] - 1.0f) < 1e-5f) &&
                (fabsf(sf[2] - 1.0f) < 1e-5f);
    d->unref();
    EXPECT_TRUE(pass) << "SoScale1Dragger default scaleFactor is not (1,1,1)";
}

// -----------------------------------------------------------------------
// SoScale1Dragger: set/get scaleFactor field
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoScale1DraggerSetGetScaleFactorField)
{
    SoScale1Dragger *d = new SoScale1Dragger;
    d->ref();
    d->scaleFactor.setValue(2.0f, 2.0f, 2.0f);
    SbVec3f sf = d->scaleFactor.getValue();
    bool pass = (fabsf(sf[0] - 2.0f) < 1e-5f) &&
                (fabsf(sf[1] - 2.0f) < 1e-5f) &&
                (fabsf(sf[2] - 2.0f) < 1e-5f);
    d->unref();
    EXPECT_TRUE(pass) << "SoScale1Dragger scaleFactor set/get failed";
}

// -----------------------------------------------------------------------
// SoRotateDiscDragger: instantiation and type
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoRotateDiscDraggerInstantiationAndTypeCheck)
{
    SoRotateDiscDragger *d = new SoRotateDiscDragger;
    d->ref();
    bool pass = (d->getTypeId() != SoType::badType()) &&
                d->isOfType(SoDragger::getClassTypeId());
    d->unref();
    EXPECT_TRUE(pass) << "SoRotateDiscDragger bad type or not SoDragger subtype";
}

// -----------------------------------------------------------------------
// SoRotateDiscDragger: rotation field default is identity
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoRotateDiscDraggerRotationDefaultIsIdentity)
{
    SoRotateDiscDragger *d = new SoRotateDiscDragger;
    d->ref();
    SbRotation rot = d->rotation.getValue();
    SbVec3f axis;
    float   angle;
    rot.getValue(axis, angle);
    bool pass = fabsf(angle) < 1e-5f;
    d->unref();
    EXPECT_TRUE(pass) << "SoRotateDiscDragger default rotation is not identity";
}

// -----------------------------------------------------------------------
// SoDragger base: isActive defaults to FALSE
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoDraggerIsActiveDefaultsToFALSE)
{
    SoTranslate1Dragger *d = new SoTranslate1Dragger;
    d->ref();
    bool pass = (d->isActive.getValue() == FALSE);
    d->unref();
    EXPECT_TRUE(pass) << "SoDragger isActive should default to FALSE";
}

// -----------------------------------------------------------------------
// SoDragger: getMotionMatrix returns identity by default
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoDraggerGetMotionMatrixReturnsIdentityByDefault)
{
    SoTranslate1Dragger *d = new SoTranslate1Dragger;
    d->ref();
    const SbMatrix &m = d->getMotionMatrix();
    bool pass = matrixIsIdentity(m);
    d->unref();
    EXPECT_TRUE(pass) << "SoDragger getMotionMatrix should be identity by default";
}

// -----------------------------------------------------------------------
// SoDragger: setMotionMatrix / getMotionMatrix round-trip
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoDraggerSetMotionMatrixGetMotionMatrixRoundTrip)
{
    SoTranslate1Dragger *d = new SoTranslate1Dragger;
    d->ref();

    // Build a translation-only matrix
    SbMatrix mat = SbMatrix::identity();
    mat[3][0] = 1.5f;
    mat[3][1] = 2.5f;
    mat[3][2] = 3.5f;

    // setMotionMatrix is public on the SoDragger base; cast to use it
    SoDragger *base = d;
    base->setMotionMatrix(mat);
    const SbMatrix &got = base->getMotionMatrix();

    bool pass = (fabsf(got[3][0] - 1.5f) < 1e-4f) &&
                (fabsf(got[3][1] - 2.5f) < 1e-4f) &&
                (fabsf(got[3][2] - 3.5f) < 1e-4f);
    d->unref();
    EXPECT_TRUE(pass) << "SoDragger setMotionMatrix/getMotionMatrix round-trip failed";
}

// -----------------------------------------------------------------------
// SoDragger: setMinGesture / getMinGesture
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoDraggerSetMinGestureGetMinGesture)
{
    SoTranslate1Dragger *d = new SoTranslate1Dragger;
    d->ref();
    d->setMinGesture(8);
    bool pass = (d->getMinGesture() == 8);
    d->unref();
    EXPECT_TRUE(pass) << "SoDragger setMinGesture/getMinGesture mismatch";
}

// -----------------------------------------------------------------------
// SoDragger: setProjectorEpsilon / getProjectorEpsilon
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoDraggerSetProjectorEpsilonGetProjectorEpsilon)
{
    SoTranslate1Dragger *d = new SoTranslate1Dragger;
    d->ref();
    d->setProjectorEpsilon(0.001f);
    bool pass = fabsf(d->getProjectorEpsilon() - 0.001f) < 1e-6f;
    d->unref();
    EXPECT_TRUE(pass) << "SoDragger projectorEpsilon set/get mismatch";
}

// -----------------------------------------------------------------------
// SoDragger: addStartCallback / removeStartCallback
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoDraggerAddRemoveStartCallback)
{
    SoTranslate1Dragger *d = new SoTranslate1Dragger;
    d->ref();
    s_start_count = 0;
    // Adding callback must not crash
    d->addStartCallback(countStartCB, nullptr);
    // Removing the same callback must not crash
    d->removeStartCallback(countStartCB, nullptr);
    bool pass = (s_start_count == 0); // callback was never invoked
    d->unref();
    EXPECT_TRUE(pass) << "start callback counter unexpectedly non-zero";
}

// -----------------------------------------------------------------------
// SoDragger: addMotionCallback / removeMotionCallback
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoDraggerAddRemoveMotionCallback)
{
    SoTranslate2Dragger *d = new SoTranslate2Dragger;
    d->ref();
    s_motion_count = 0;
    d->addMotionCallback(countMotionCB, nullptr);
    d->removeMotionCallback(countMotionCB, nullptr);
    bool pass = (s_motion_count == 0);
    d->unref();
    EXPECT_TRUE(pass) << "motion callback counter unexpectedly non-zero";
}

// -----------------------------------------------------------------------
// SoDragger: addFinishCallback / removeFinishCallback
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoDraggerAddRemoveFinishCallback)
{
    SoScale1Dragger *d = new SoScale1Dragger;
    d->ref();
    s_finish_count = 0;
    d->addFinishCallback(countFinishCB, nullptr);
    d->removeFinishCallback(countFinishCB, nullptr);
    bool pass = (s_finish_count == 0);
    d->unref();
    EXPECT_TRUE(pass) << "finish callback counter unexpectedly non-zero";
}

// -----------------------------------------------------------------------
// SoDragger: addValueChangedCallback / removeValueChangedCallback
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoDraggerAddRemoveValueChangedCallback)
{
    SoRotateDiscDragger *d = new SoRotateDiscDragger;
    d->ref();
    s_changed_count = 0;
    d->addValueChangedCallback(countChangedCB, nullptr);
    d->removeValueChangedCallback(countChangedCB, nullptr);
    bool pass = (s_changed_count == 0);
    d->unref();
    EXPECT_TRUE(pass) << "valueChanged callback counter unexpectedly non-zero";
}

// -----------------------------------------------------------------------
// SoDragger: enableValueChangedCallbacks returns previous state
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoDraggerEnableValueChangedCallbacksReturnsPreviousState)
{
    SoTranslate1Dragger *d = new SoTranslate1Dragger;
    d->ref();
    // Default should be TRUE (enabled)
    SbBool prev = d->enableValueChangedCallbacks(FALSE);
    bool pass = (prev == TRUE);
    // Restore
    d->enableValueChangedCallbacks(TRUE);
    d->unref();
    EXPECT_TRUE(pass) << "enableValueChangedCallbacks should return TRUE by default";
}

// -----------------------------------------------------------------------
// SoDragger: setMinScale / getMinScale (static)
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoDraggerSetMinScaleGetMinScaleStatic)
{
    float old = SoDragger::getMinScale();
    SoDragger::setMinScale(0.05f);
    bool pass = fabsf(SoDragger::getMinScale() - 0.05f) < 1e-6f;
    SoDragger::setMinScale(old); // restore
    EXPECT_TRUE(pass) << "SoDragger static setMinScale/getMinScale mismatch";
}

// -----------------------------------------------------------------------
// SoDragger: static appendTranslation
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoDraggerAppendTranslationProducesCorrectMatrix)
{
    SbMatrix m = SbMatrix::identity();
    SbVec3f  t(3.0f, 0.0f, 0.0f);
    SbMatrix result = SoDragger::appendTranslation(m, t);
    // The translation should appear in the last row
    bool pass = fabsf(result[3][0] - 3.0f) < 1e-5f &&
                fabsf(result[3][1])          < 1e-5f &&
                fabsf(result[3][2])          < 1e-5f;
    EXPECT_TRUE(pass) << "appendTranslation produced unexpected matrix";
}

// -----------------------------------------------------------------------
// SoDragger: static appendScale
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoDraggerAppendScaleProducesCorrectMatrix)
{
    SbMatrix m    = SbMatrix::identity();
    SbVec3f  sc(2.0f, 2.0f, 2.0f);
    SbVec3f  ctr(0.0f, 0.0f, 0.0f);
    SbMatrix result = SoDragger::appendScale(m, sc, ctr);
    // Diagonal should be scaled
    bool pass = fabsf(result[0][0] - 2.0f) < 1e-5f &&
                fabsf(result[1][1] - 2.0f) < 1e-5f &&
                fabsf(result[2][2] - 2.0f) < 1e-5f;
    EXPECT_TRUE(pass) << "appendScale produced unexpected matrix";
}

// -----------------------------------------------------------------------
// SoDragger: static appendRotation
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoDraggerAppendRotationProducesCorrectMatrix)
{
    SbMatrix m = SbMatrix::identity();
    // 90-degree rotation around Z
    SbRotation rot(SbVec3f(0.0f, 0.0f, 1.0f),
                   static_cast<float>(M_PI) / 2.0f);
    SbVec3f ctr(0.0f, 0.0f, 0.0f);
    SbMatrix result = SoDragger::appendRotation(m, rot, ctr);
    // Applying to X-axis unit vector should give Y-axis
    SbVec3f xhat(1.0f, 0.0f, 0.0f);
    SbVec3f rotated;
    result.multDirMatrix(xhat, rotated);
    bool pass = fabsf(rotated[0])        < 1e-4f &&
                fabsf(rotated[1] - 1.0f) < 1e-4f &&
                fabsf(rotated[2])        < 1e-4f;
    EXPECT_TRUE(pass) << "appendRotation produced unexpected rotation";
}

// -----------------------------------------------------------------------
// SoSearchAction traversal over a scene containing a dragger
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoSearchActionFindsSoTranslate1DraggerInSceneGraph)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoTranslate1Dragger *d = new SoTranslate1Dragger;
    root->addChild(d);

    SoSearchAction sa;
    sa.setType(SoTranslate1Dragger::getClassTypeId());
    sa.setInterest(SoSearchAction::FIRST);
    sa.apply(root);

    bool pass = (sa.getPath() != nullptr);
    root->unref();
    EXPECT_TRUE(pass) << "SoSearchAction did not find SoTranslate1Dragger";
}

// -----------------------------------------------------------------------
// SoSearchAction traversal for SoTranslate2Dragger
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoSearchActionFindsSoTranslate2DraggerInSceneGraph)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoTranslate2Dragger *d = new SoTranslate2Dragger;
    root->addChild(d);

    SoSearchAction sa;
    sa.setType(SoTranslate2Dragger::getClassTypeId());
    sa.setInterest(SoSearchAction::FIRST);
    sa.apply(root);

    bool pass = (sa.getPath() != nullptr);
    root->unref();
    EXPECT_TRUE(pass) << "SoSearchAction did not find SoTranslate2Dragger";
}

// -----------------------------------------------------------------------
// SoDragger: dragger nodekit catalog is non-null
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoTranslate1DraggerNodekitCatalogIsNonNull)
{
    SoTranslate1Dragger *d = new SoTranslate1Dragger;
    d->ref();
    const SoNodekitCatalog *cat = d->getNodekitCatalog();
    bool pass = (cat != nullptr) && (cat->getNumEntries() > 0);
    d->unref();
    EXPECT_TRUE(pass) << "SoTranslate1Dragger nodekit catalog null or empty";
}

// =======================================================================
// Complex draggers (previously crashing due to SbString::vsprintf bug)
// =======================================================================

// -----------------------------------------------------------------------
// SoHandleBoxDragger: instantiation and type check
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoHandleBoxDraggerInstantiationAndTypeCheck)
{
    SoHandleBoxDragger *d = new SoHandleBoxDragger;
    d->ref();
    bool pass = (d->getTypeId() != SoType::badType()) &&
                d->isOfType(SoDragger::getClassTypeId());
    d->unref();
    EXPECT_TRUE(pass) << "SoHandleBoxDragger bad type or not SoDragger subtype";
}

// -----------------------------------------------------------------------
// SoHandleBoxDragger: scaleFactor field default is (1,1,1)
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoHandleBoxDraggerScaleFactorDefaultIs111)
{
    SoHandleBoxDragger *d = new SoHandleBoxDragger;
    d->ref();
    SbVec3f sf = d->scaleFactor.getValue();
    bool pass = (fabsf(sf[0] - 1.0f) < 1e-5f) &&
                (fabsf(sf[1] - 1.0f) < 1e-5f) &&
                (fabsf(sf[2] - 1.0f) < 1e-5f);
    d->unref();
    EXPECT_TRUE(pass) << "SoHandleBoxDragger default scaleFactor is not (1,1,1)";
}

// -----------------------------------------------------------------------
// SoHandleBoxDragger: translation field default is (0,0,0)
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoHandleBoxDraggerTranslationDefaultIs000)
{
    SoHandleBoxDragger *d = new SoHandleBoxDragger;
    d->ref();
    SbVec3f t = d->translation.getValue();
    bool pass = (fabsf(t[0]) < 1e-5f) &&
                (fabsf(t[1]) < 1e-5f) &&
                (fabsf(t[2]) < 1e-5f);
    d->unref();
    EXPECT_TRUE(pass) << "SoHandleBoxDragger default translation is not (0,0,0)";
}

// -----------------------------------------------------------------------
// SoHandleBoxDragger: nodekit catalog has expected parts
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoHandleBoxDraggerNodekitCatalogHasSwitchParts)
{
    SoHandleBoxDragger *d = new SoHandleBoxDragger;
    d->ref();
    const SoNodekitCatalog *cat = d->getNodekitCatalog();
    bool pass = (cat != nullptr) &&
                (cat->getPartNumber("translator1Switch") != SO_CATALOG_NAME_NOT_FOUND) &&
                (cat->getPartNumber("extruder1Switch")   != SO_CATALOG_NAME_NOT_FOUND) &&
                (cat->getPartNumber("uniform1Switch")    != SO_CATALOG_NAME_NOT_FOUND);
    d->unref();
    EXPECT_TRUE(pass) << "SoHandleBoxDragger catalog missing expected switch parts";
}

// -----------------------------------------------------------------------
// SoHandleBoxDragger: set/get translation and scaleFactor fields
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoHandleBoxDraggerSetGetTranslationAndScaleFactor)
{
    SoHandleBoxDragger *d = new SoHandleBoxDragger;
    d->ref();
    d->translation.setValue(1.0f, 2.0f, 3.0f);
    d->scaleFactor.setValue(2.0f, 3.0f, 4.0f);
    SbVec3f t  = d->translation.getValue();
    SbVec3f sf = d->scaleFactor.getValue();
    bool pass = (fabsf(t[0] - 1.0f) < 1e-5f) &&
                (fabsf(t[1] - 2.0f) < 1e-5f) &&
                (fabsf(t[2] - 3.0f) < 1e-5f) &&
                (fabsf(sf[0] - 2.0f) < 1e-5f) &&
                (fabsf(sf[1] - 3.0f) < 1e-5f) &&
                (fabsf(sf[2] - 4.0f) < 1e-5f);
    d->unref();
    EXPECT_TRUE(pass) << "SoHandleBoxDragger field set/get failed";
}

// -----------------------------------------------------------------------
// SoHandleBoxDragger: callback registration does not crash
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoHandleBoxDraggerCallbackRegistrationRemoval)
{
    SoHandleBoxDragger *d = new SoHandleBoxDragger;
    d->ref();
    s_start_count = 0;
    d->addStartCallback(countStartCB, nullptr);
    d->removeStartCallback(countStartCB, nullptr);
    bool pass = (s_start_count == 0);
    d->unref();
    EXPECT_TRUE(pass) << "SoHandleBoxDragger callback registration crashed or misfired";
}

// -----------------------------------------------------------------------
// SoTabBoxDragger: instantiation and type check
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoTabBoxDraggerInstantiationAndTypeCheck)
{
    SoTabBoxDragger *d = new SoTabBoxDragger;
    d->ref();
    bool pass = (d->getTypeId() != SoType::badType()) &&
                d->isOfType(SoDragger::getClassTypeId());
    d->unref();
    EXPECT_TRUE(pass) << "SoTabBoxDragger bad type or not SoDragger subtype";
}

// -----------------------------------------------------------------------
// SoTabBoxDragger: scaleFactor and translation fields default values
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoTabBoxDraggerDefaultFieldValues)
{
    SoTabBoxDragger *d = new SoTabBoxDragger;
    d->ref();
    SbVec3f t  = d->translation.getValue();
    SbVec3f sf = d->scaleFactor.getValue();
    bool pass = (fabsf(t[0]) < 1e-5f) &&
                (fabsf(t[1]) < 1e-5f) &&
                (fabsf(t[2]) < 1e-5f) &&
                (fabsf(sf[0] - 1.0f) < 1e-5f) &&
                (fabsf(sf[1] - 1.0f) < 1e-5f) &&
                (fabsf(sf[2] - 1.0f) < 1e-5f);
    d->unref();
    EXPECT_TRUE(pass) << "SoTabBoxDragger default field values incorrect";
}

// -----------------------------------------------------------------------
// SoTabBoxDragger: nodekit catalog includes tabPlane sub-draggers
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoTabBoxDraggerNodekitCatalogHasTabPlaneParts)
{
    SoTabBoxDragger *d = new SoTabBoxDragger;
    d->ref();
    const SoNodekitCatalog *cat = d->getNodekitCatalog();
    bool pass = (cat != nullptr) &&
                (cat->getPartNumber("tabPlane1") != SO_CATALOG_NAME_NOT_FOUND) &&
                (cat->getPartNumber("tabPlane6") != SO_CATALOG_NAME_NOT_FOUND);
    d->unref();
    EXPECT_TRUE(pass) << "SoTabBoxDragger catalog missing tabPlane parts";
}

// -----------------------------------------------------------------------
// SoTabBoxDragger: SoSearchAction traversal
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoSearchActionFindsSoTabBoxDraggerInSceneGraph)
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoTabBoxDragger *d = new SoTabBoxDragger;
    root->addChild(d);
    SoSearchAction sa;
    sa.setType(SoTabBoxDragger::getClassTypeId());
    sa.setInterest(SoSearchAction::FIRST);
    sa.apply(root);
    bool pass = (sa.getPath() != nullptr);
    root->unref();
    EXPECT_TRUE(pass) << "SoSearchAction did not find SoTabBoxDragger";
}

// -----------------------------------------------------------------------
// SoTransformBoxDragger: instantiation and type check
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoTransformBoxDraggerInstantiationAndTypeCheck)
{
    SoTransformBoxDragger *d = new SoTransformBoxDragger;
    d->ref();
    bool pass = (d->getTypeId() != SoType::badType()) &&
                d->isOfType(SoDragger::getClassTypeId());
    d->unref();
    EXPECT_TRUE(pass) << "SoTransformBoxDragger bad type or not SoDragger subtype";
}

// -----------------------------------------------------------------------
// SoTransformBoxDragger: rotation/translation/scaleFactor field defaults
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoTransformBoxDraggerDefaultFieldValues)
{
    SoTransformBoxDragger *d = new SoTransformBoxDragger;
    d->ref();
    SbVec3f  t    = d->translation.getValue();
    SbVec3f  sf   = d->scaleFactor.getValue();
    SbRotation rot = d->rotation.getValue();
    SbVec3f   ax; float ang;
    rot.getValue(ax, ang);
    bool pass = (fabsf(t[0]) < 1e-5f) &&
                (fabsf(t[1]) < 1e-5f) &&
                (fabsf(t[2]) < 1e-5f) &&
                (fabsf(sf[0] - 1.0f) < 1e-5f) &&
                (fabsf(sf[1] - 1.0f) < 1e-5f) &&
                (fabsf(sf[2] - 1.0f) < 1e-5f) &&
                (fabsf(ang) < 1e-5f); // identity rotation
    d->unref();
    EXPECT_TRUE(pass) << "SoTransformBoxDragger default field values incorrect";
}

// -----------------------------------------------------------------------
// SoTransformerDragger: instantiation and type check
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoTransformerDraggerInstantiationAndTypeCheck)
{
    SoTransformerDragger *d = new SoTransformerDragger;
    d->ref();
    bool pass = (d->getTypeId() != SoType::badType()) &&
                d->isOfType(SoDragger::getClassTypeId());
    d->unref();
    EXPECT_TRUE(pass) << "SoTransformerDragger bad type or not SoDragger subtype";
}

// -----------------------------------------------------------------------
// SoTransformerDragger: field defaults
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoTransformerDraggerDefaultFieldValues)
{
    SoTransformerDragger *d = new SoTransformerDragger;
    d->ref();
    SbVec3f  t    = d->translation.getValue();
    SbVec3f  sf   = d->scaleFactor.getValue();
    SbRotation rot = d->rotation.getValue();
    SbVec3f ax; float ang;
    rot.getValue(ax, ang);
    // minDiscRotDot default is 0.025f per the Coin source
    float minDot = d->minDiscRotDot.getValue();
    bool pass = (fabsf(t[0]) < 1e-5f) &&
                (fabsf(t[1]) < 1e-5f) &&
                (fabsf(t[2]) < 1e-5f) &&
                (fabsf(sf[0] - 1.0f) < 1e-5f) &&
                (fabsf(ang) < 1e-5f) &&
                (minDot > 0.0f);
    d->unref();
    EXPECT_TRUE(pass) << "SoTransformerDragger default field values incorrect";
}

// -----------------------------------------------------------------------
// SoCenterballDragger: instantiation and type check
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoCenterballDraggerInstantiationAndTypeCheck)
{
    SoCenterballDragger *d = new SoCenterballDragger;
    d->ref();
    bool pass = (d->getTypeId() != SoType::badType()) &&
                d->isOfType(SoDragger::getClassTypeId());
    d->unref();
    EXPECT_TRUE(pass) << "SoCenterballDragger bad type or not SoDragger subtype";
}

// -----------------------------------------------------------------------
// SoCenterballDragger: rotation and center field defaults
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoCenterballDraggerDefaultRotationIsIdentityCenterIs000)
{
    SoCenterballDragger *d = new SoCenterballDragger;
    d->ref();
    SbRotation rot = d->rotation.getValue();
    SbVec3f    ctr = d->center.getValue();
    SbVec3f ax; float ang;
    rot.getValue(ax, ang);
    bool pass = (fabsf(ang) < 1e-5f) &&
                (fabsf(ctr[0]) < 1e-5f) &&
                (fabsf(ctr[1]) < 1e-5f) &&
                (fabsf(ctr[2]) < 1e-5f);
    d->unref();
    EXPECT_TRUE(pass) << "SoCenterballDragger default rotation/center incorrect";
}

// -----------------------------------------------------------------------
// SoCenterballDragger: set/get rotation and center fields
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoCenterballDraggerSetGetRotationAndCenterFields)
{
    SoCenterballDragger *d = new SoCenterballDragger;
    d->ref();
    SbRotation rot(SbVec3f(0.0f, 1.0f, 0.0f),
                   static_cast<float>(M_PI) / 3.0f);
    d->rotation.setValue(rot);
    d->center.setValue(1.0f, 2.0f, 3.0f);
    SbRotation gotRot = d->rotation.getValue();
    SbVec3f    gotCtr = d->center.getValue();
    SbVec3f ax; float ang;
    SbVec3f rax; float rang;
    gotRot.getValue(ax, ang);
    rot.getValue(rax, rang);
    bool pass = (fabsf(ang - rang) < 1e-4f) &&
                (fabsf(gotCtr[0] - 1.0f) < 1e-5f) &&
                (fabsf(gotCtr[1] - 2.0f) < 1e-5f) &&
                (fabsf(gotCtr[2] - 3.0f) < 1e-5f);
    d->unref();
    EXPECT_TRUE(pass) << "SoCenterballDragger field set/get failed";
}

// -----------------------------------------------------------------------
// SoSearchAction finds complex draggers in a scene graph
// -----------------------------------------------------------------------

TEST(DraggersDraggers, SoSearchActionFindsSoHandleBoxDraggerInSceneGraph)
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoHandleBoxDragger *d = new SoHandleBoxDragger;
    root->addChild(d);
    SoSearchAction sa;
    sa.setType(SoHandleBoxDragger::getClassTypeId());
    sa.setInterest(SoSearchAction::FIRST);
    sa.apply(root);
    bool pass = (sa.getPath() != nullptr);
    root->unref();
    EXPECT_TRUE(pass) << "SoSearchAction did not find SoHandleBoxDragger";
}

TEST(DraggersDraggers, SoSearchActionFindsSoTransformerDraggerInSceneGraph)
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoTransformerDragger *d = new SoTransformerDragger;
    root->addChild(d);
    SoSearchAction sa;
    sa.setType(SoTransformerDragger::getClassTypeId());
    sa.setInterest(SoSearchAction::FIRST);
    sa.apply(root);
    bool pass = (sa.getPath() != nullptr);
    root->unref();
    EXPECT_TRUE(pass) << "SoSearchAction did not find SoTransformerDragger";
}

TEST(DraggersDraggers, SoSearchActionFindsSoCenterballDraggerInSceneGraph)
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoCenterballDragger *d = new SoCenterballDragger;
    root->addChild(d);
    SoSearchAction sa;
    sa.setType(SoCenterballDragger::getClassTypeId());
    sa.setInterest(SoSearchAction::FIRST);
    sa.apply(root);
    bool pass = (sa.getPath() != nullptr);
    root->unref();
    EXPECT_TRUE(pass) << "SoSearchAction did not find SoCenterballDragger";
}
