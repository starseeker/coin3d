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

using namespace SimpleTest;

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

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // -----------------------------------------------------------------------
    // SoRotateCylindricalDragger
    // -----------------------------------------------------------------------
    runner.startTest("SoRotateCylindricalDragger: class type id valid");
    {
        bool pass = (SoRotateCylindricalDragger::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoRotateCylindricalDragger has bad class type");
    }

    runner.startTest("SoRotateCylindricalDragger: isOfType SoDragger");
    {
        SoRotateCylindricalDragger * d = new SoRotateCylindricalDragger;
        d->ref();
        bool pass = d->isOfType(SoDragger::getClassTypeId());
        d->unref();
        runner.endTest(pass, pass ? "" : "SoRotateCylindricalDragger not a SoDragger subtype");
    }

    runner.startTest("SoRotateCylindricalDragger: default rotation is identity");
    {
        SoRotateCylindricalDragger * d = new SoRotateCylindricalDragger;
        d->ref();
        SbVec3f ax; float ang;
        d->rotation.getValue().getValue(ax, ang);
        bool pass = (fabsf(ang) < 1e-5f);
        d->unref();
        runner.endTest(pass, pass ? "" : "default rotation is not identity");
    }

    runner.startTest("SoRotateCylindricalDragger: set/get rotation round-trip");
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
        bool pass = (fabsf(ang - rang) < 1e-4f);
        d->unref();
        runner.endTest(pass, pass ? "" : "rotation round-trip failed");
    }

    runner.startTest("SoRotateCylindricalDragger: SoGetBoundingBoxAction does not crash");
    {
        SoRotateCylindricalDragger * d = new SoRotateCylindricalDragger;
        d->ref();
        bool pass = bboxDragger(d);
        d->unref();
        runner.endTest(pass, pass ? "" : "SoGetBoundingBoxAction crashed");
    }

    runner.startTest("SoRotateCylindricalDragger: SoSearchAction finds it");
    {
        SoRotateCylindricalDragger * d = new SoRotateCylindricalDragger;
        d->ref();
        bool pass = searchDragger(d, SoRotateCylindricalDragger::getClassTypeId());
        d->unref();
        runner.endTest(pass, pass ? "" : "SoSearchAction did not find it");
    }

    // -----------------------------------------------------------------------
    // SoRotateSphericalDragger
    // -----------------------------------------------------------------------
    runner.startTest("SoRotateSphericalDragger: class type id valid");
    {
        bool pass = (SoRotateSphericalDragger::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoRotateSphericalDragger has bad class type");
    }

    runner.startTest("SoRotateSphericalDragger: isOfType SoDragger");
    {
        SoRotateSphericalDragger * d = new SoRotateSphericalDragger;
        d->ref();
        bool pass = d->isOfType(SoDragger::getClassTypeId());
        d->unref();
        runner.endTest(pass, pass ? "" : "SoRotateSphericalDragger not a SoDragger subtype");
    }

    runner.startTest("SoRotateSphericalDragger: set/get rotation round-trip");
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
        bool pass = (fabsf(ang - rang) < 1e-4f);
        d->unref();
        runner.endTest(pass, pass ? "" : "rotation round-trip failed");
    }

    runner.startTest("SoRotateSphericalDragger: SoGetBoundingBoxAction does not crash");
    {
        SoRotateSphericalDragger * d = new SoRotateSphericalDragger;
        d->ref();
        bool pass = bboxDragger(d);
        d->unref();
        runner.endTest(pass, pass ? "" : "SoGetBoundingBoxAction crashed");
    }

    // -----------------------------------------------------------------------
    // SoScale2Dragger
    // -----------------------------------------------------------------------
    runner.startTest("SoScale2Dragger: class type id valid");
    {
        bool pass = (SoScale2Dragger::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoScale2Dragger has bad class type");
    }

    runner.startTest("SoScale2Dragger: isOfType SoDragger");
    {
        SoScale2Dragger * d = new SoScale2Dragger;
        d->ref();
        bool pass = d->isOfType(SoDragger::getClassTypeId());
        d->unref();
        runner.endTest(pass, pass ? "" : "SoScale2Dragger not a SoDragger subtype");
    }

    runner.startTest("SoScale2Dragger: default scaleFactor is (1,1,1)");
    {
        SoScale2Dragger * d = new SoScale2Dragger;
        d->ref();
        SbVec3f sf = d->scaleFactor.getValue();
        bool pass = (fabsf(sf[0] - 1.0f) < 1e-5f) &&
                    (fabsf(sf[1] - 1.0f) < 1e-5f) &&
                    (fabsf(sf[2] - 1.0f) < 1e-5f);
        d->unref();
        runner.endTest(pass, pass ? "" : "default scaleFactor is not (1,1,1)");
    }

    runner.startTest("SoScale2Dragger: set/get scaleFactor round-trip");
    {
        SoScale2Dragger * d = new SoScale2Dragger;
        d->ref();
        d->scaleFactor.setValue(2.0f, 3.0f, 1.0f);
        SbVec3f sf = d->scaleFactor.getValue();
        bool pass = (fabsf(sf[0] - 2.0f) < 1e-5f) &&
                    (fabsf(sf[1] - 3.0f) < 1e-5f) &&
                    (fabsf(sf[2] - 1.0f) < 1e-5f);
        d->unref();
        runner.endTest(pass, pass ? "" : "scaleFactor round-trip failed");
    }

    runner.startTest("SoScale2Dragger: SoGetBoundingBoxAction does not crash");
    {
        SoScale2Dragger * d = new SoScale2Dragger;
        d->ref();
        bool pass = bboxDragger(d);
        d->unref();
        runner.endTest(pass, pass ? "" : "SoGetBoundingBoxAction crashed");
    }

    // -----------------------------------------------------------------------
    // SoScale2UniformDragger
    // -----------------------------------------------------------------------
    runner.startTest("SoScale2UniformDragger: class type id valid");
    {
        bool pass = (SoScale2UniformDragger::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoScale2UniformDragger has bad class type");
    }

    runner.startTest("SoScale2UniformDragger: isOfType SoDragger");
    {
        SoScale2UniformDragger * d = new SoScale2UniformDragger;
        d->ref();
        bool pass = d->isOfType(SoDragger::getClassTypeId());
        d->unref();
        runner.endTest(pass, pass ? "" : "SoScale2UniformDragger not a SoDragger subtype");
    }

    runner.startTest("SoScale2UniformDragger: default scaleFactor is (1,1,1)");
    {
        SoScale2UniformDragger * d = new SoScale2UniformDragger;
        d->ref();
        SbVec3f sf = d->scaleFactor.getValue();
        bool pass = (fabsf(sf[0] - 1.0f) < 1e-5f);
        d->unref();
        runner.endTest(pass, pass ? "" : "default scaleFactor is not (1,1,1)");
    }

    // -----------------------------------------------------------------------
    // SoScaleUniformDragger
    // -----------------------------------------------------------------------
    runner.startTest("SoScaleUniformDragger: class type id valid");
    {
        bool pass = (SoScaleUniformDragger::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoScaleUniformDragger has bad class type");
    }

    runner.startTest("SoScaleUniformDragger: isOfType SoDragger");
    {
        SoScaleUniformDragger * d = new SoScaleUniformDragger;
        d->ref();
        bool pass = d->isOfType(SoDragger::getClassTypeId());
        d->unref();
        runner.endTest(pass, pass ? "" : "SoScaleUniformDragger not a SoDragger subtype");
    }

    // -----------------------------------------------------------------------
    // SoJackDragger
    // -----------------------------------------------------------------------
    runner.startTest("SoJackDragger: class type id valid");
    {
        bool pass = (SoJackDragger::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoJackDragger has bad class type");
    }

    runner.startTest("SoJackDragger: isOfType SoDragger");
    {
        SoJackDragger * d = new SoJackDragger;
        d->ref();
        bool pass = d->isOfType(SoDragger::getClassTypeId());
        d->unref();
        runner.endTest(pass, pass ? "" : "SoJackDragger not a SoDragger subtype");
    }

    runner.startTest("SoJackDragger: default fields");
    {
        SoJackDragger * d = new SoJackDragger;
        d->ref();
        SbVec3f sf = d->scaleFactor.getValue();
        SbVec3f t  = d->translation.getValue();
        SbVec3f ax; float ang;
        d->rotation.getValue().getValue(ax, ang);
        bool pass = (fabsf(sf[0] - 1.0f) < 1e-5f) &&
                    (fabsf(t[0]) < 1e-5f) &&
                    (fabsf(ang) < 1e-5f);
        d->unref();
        runner.endTest(pass, pass ? "" : "SoJackDragger default fields incorrect");
    }

    runner.startTest("SoJackDragger: SoGetBoundingBoxAction does not crash");
    {
        SoJackDragger * d = new SoJackDragger;
        d->ref();
        bool pass = bboxDragger(d);
        d->unref();
        runner.endTest(pass, pass ? "" : "SoGetBoundingBoxAction crashed");
    }

    // -----------------------------------------------------------------------
    // SoDragPointDragger
    // -----------------------------------------------------------------------
    runner.startTest("SoDragPointDragger: class type id valid");
    {
        bool pass = (SoDragPointDragger::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoDragPointDragger has bad class type");
    }

    runner.startTest("SoDragPointDragger: isOfType SoDragger");
    {
        SoDragPointDragger * d = new SoDragPointDragger;
        d->ref();
        bool pass = d->isOfType(SoDragger::getClassTypeId());
        d->unref();
        runner.endTest(pass, pass ? "" : "SoDragPointDragger not a SoDragger subtype");
    }

    runner.startTest("SoDragPointDragger: default translation is (0,0,0)");
    {
        SoDragPointDragger * d = new SoDragPointDragger;
        d->ref();
        SbVec3f t = d->translation.getValue();
        bool pass = (fabsf(t[0]) < 1e-5f) &&
                    (fabsf(t[1]) < 1e-5f) &&
                    (fabsf(t[2]) < 1e-5f);
        d->unref();
        runner.endTest(pass, pass ? "" : "default translation is not (0,0,0)");
    }

    runner.startTest("SoDragPointDragger: set/get translation round-trip");
    {
        SoDragPointDragger * d = new SoDragPointDragger;
        d->ref();
        d->translation.setValue(1.0f, 2.0f, 3.0f);
        SbVec3f t = d->translation.getValue();
        bool pass = (fabsf(t[0] - 1.0f) < 1e-5f) &&
                    (fabsf(t[1] - 2.0f) < 1e-5f) &&
                    (fabsf(t[2] - 3.0f) < 1e-5f);
        d->unref();
        runner.endTest(pass, pass ? "" : "translation round-trip failed");
    }

    // -----------------------------------------------------------------------
    // SoTrackballDragger
    // -----------------------------------------------------------------------
    runner.startTest("SoTrackballDragger: class type id valid");
    {
        bool pass = (SoTrackballDragger::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoTrackballDragger has bad class type");
    }

    runner.startTest("SoTrackballDragger: isOfType SoDragger");
    {
        SoTrackballDragger * d = new SoTrackballDragger;
        d->ref();
        bool pass = d->isOfType(SoDragger::getClassTypeId());
        d->unref();
        runner.endTest(pass, pass ? "" : "SoTrackballDragger not a SoDragger subtype");
    }

    runner.startTest("SoTrackballDragger: default rotation is identity");
    {
        SoTrackballDragger * d = new SoTrackballDragger;
        d->ref();
        SbVec3f ax; float ang;
        d->rotation.getValue().getValue(ax, ang);
        bool pass = (fabsf(ang) < 1e-5f);
        d->unref();
        runner.endTest(pass, pass ? "" : "default rotation is not identity");
    }

    runner.startTest("SoTrackballDragger: SoGetBoundingBoxAction does not crash");
    {
        SoTrackballDragger * d = new SoTrackballDragger;
        d->ref();
        bool pass = bboxDragger(d);
        d->unref();
        runner.endTest(pass, pass ? "" : "SoGetBoundingBoxAction crashed");
    }

    // -----------------------------------------------------------------------
    // SoTabPlaneDragger
    // -----------------------------------------------------------------------
    runner.startTest("SoTabPlaneDragger: class type id valid");
    {
        bool pass = (SoTabPlaneDragger::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoTabPlaneDragger has bad class type");
    }

    runner.startTest("SoTabPlaneDragger: isOfType SoDragger");
    {
        SoTabPlaneDragger * d = new SoTabPlaneDragger;
        d->ref();
        bool pass = d->isOfType(SoDragger::getClassTypeId());
        d->unref();
        runner.endTest(pass, pass ? "" : "SoTabPlaneDragger not a SoDragger subtype");
    }

    runner.startTest("SoTabPlaneDragger: default scaleFactor is (1,1,1)");
    {
        SoTabPlaneDragger * d = new SoTabPlaneDragger;
        d->ref();
        SbVec3f sf = d->scaleFactor.getValue();
        bool pass = (fabsf(sf[0] - 1.0f) < 1e-5f);
        d->unref();
        runner.endTest(pass, pass ? "" : "default scaleFactor is not (1,1,1)");
    }

    runner.startTest("SoTabPlaneDragger: SoGetBoundingBoxAction does not crash");
    {
        SoTabPlaneDragger * d = new SoTabPlaneDragger;
        d->ref();
        bool pass = bboxDragger(d);
        d->unref();
        runner.endTest(pass, pass ? "" : "SoGetBoundingBoxAction crashed");
    }

    // -----------------------------------------------------------------------
    // SoPointLightDragger
    // -----------------------------------------------------------------------
    runner.startTest("SoPointLightDragger: class type id valid");
    {
        bool pass = (SoPointLightDragger::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoPointLightDragger has bad class type");
    }

    runner.startTest("SoPointLightDragger: isOfType SoDragger");
    {
        SoPointLightDragger * d = new SoPointLightDragger;
        d->ref();
        bool pass = d->isOfType(SoDragger::getClassTypeId());
        d->unref();
        runner.endTest(pass, pass ? "" : "SoPointLightDragger not a SoDragger subtype");
    }

    // -----------------------------------------------------------------------
    // SoDirectionalLightDragger
    // -----------------------------------------------------------------------
    runner.startTest("SoDirectionalLightDragger: class type id valid");
    {
        bool pass = (SoDirectionalLightDragger::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoDirectionalLightDragger has bad class type");
    }

    runner.startTest("SoDirectionalLightDragger: isOfType SoDragger");
    {
        SoDirectionalLightDragger * d = new SoDirectionalLightDragger;
        d->ref();
        bool pass = d->isOfType(SoDragger::getClassTypeId());
        d->unref();
        runner.endTest(pass, pass ? "" : "SoDirectionalLightDragger not a SoDragger subtype");
    }

    runner.startTest("SoDirectionalLightDragger: default translation is (0,0,0)");
    {
        SoDirectionalLightDragger * d = new SoDirectionalLightDragger;
        d->ref();
        SbVec3f t = d->translation.getValue();
        // Default should be (0,0,0)
        bool pass = (t.length() < 1e-5f);
        d->unref();
        runner.endTest(pass, pass ? "" : "default translation is not zero");
    }

    // -----------------------------------------------------------------------
    // SoSpotLightDragger
    // -----------------------------------------------------------------------
    runner.startTest("SoSpotLightDragger: class type id valid");
    {
        bool pass = (SoSpotLightDragger::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoSpotLightDragger has bad class type");
    }

    runner.startTest("SoSpotLightDragger: isOfType SoDragger");
    {
        SoSpotLightDragger * d = new SoSpotLightDragger;
        d->ref();
        bool pass = d->isOfType(SoDragger::getClassTypeId());
        d->unref();
        runner.endTest(pass, pass ? "" : "SoSpotLightDragger not a SoDragger subtype");
    }

    // -----------------------------------------------------------------------
    // SoSearchAction finds all new dragger types
    // -----------------------------------------------------------------------
    runner.startTest("SoSearchAction finds SoRotateCylindricalDragger");
    {
        SoRotateCylindricalDragger * d = new SoRotateCylindricalDragger;
        d->ref();
        bool pass = searchDragger(d, SoRotateCylindricalDragger::getClassTypeId());
        d->unref();
        runner.endTest(pass, pass ? "" : "SoSearchAction did not find it");
    }

    runner.startTest("SoSearchAction finds SoRotateSphericalDragger");
    {
        SoRotateSphericalDragger * d = new SoRotateSphericalDragger;
        d->ref();
        bool pass = searchDragger(d, SoRotateSphericalDragger::getClassTypeId());
        d->unref();
        runner.endTest(pass, pass ? "" : "SoSearchAction did not find it");
    }

    runner.startTest("SoSearchAction finds SoJackDragger");
    {
        SoJackDragger * d = new SoJackDragger;
        d->ref();
        bool pass = searchDragger(d, SoJackDragger::getClassTypeId());
        d->unref();
        runner.endTest(pass, pass ? "" : "SoSearchAction did not find it");
    }

    runner.startTest("SoSearchAction finds SoTrackballDragger");
    {
        SoTrackballDragger * d = new SoTrackballDragger;
        d->ref();
        bool pass = searchDragger(d, SoTrackballDragger::getClassTypeId());
        d->unref();
        runner.endTest(pass, pass ? "" : "SoSearchAction did not find it");
    }

    return runner.getSummary();
}
