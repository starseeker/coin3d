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
 * @file test_engines_deep.cpp
 * @brief Additional engine tests supplementing test_engines_suite.cpp.
 *
 * Covers:
 *   SoDecomposeMatrix       - identity matrix decomposition
 *   SoDecomposeRotation     - identity rotation decomposition
 *   SoComposeRotationFromTo - non-trivial rotation output
 *   SoTransformVec3f        - vector transform with identity matrix
 *   SoInterpolateRotation   - interpolation at alpha=0 and alpha=1
 *   SoOnOff                 - on/off/toggle triggers
 *   SoTriggerAny            - class type check
 */

#include "../test_utils.h"

#include <Inventor/engines/SoDecomposeMatrix.h>
#include <Inventor/engines/SoDecomposeRotation.h>
#include <Inventor/engines/SoComposeRotationFromTo.h>
#include <Inventor/engines/SoTransformVec3f.h>
#include <Inventor/engines/SoInterpolateRotation.h>
#include <Inventor/engines/SoOnOff.h>
#include <Inventor/engines/SoTriggerAny.h>
#include <Inventor/fields/SoMFVec3f.h>
#include <Inventor/fields/SoMFFloat.h>
#include <Inventor/fields/SoMFRotation.h>
#include <Inventor/fields/SoSFBool.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbRotation.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SoType.h>
#include <cmath>

using namespace ObolTest;

TEST(EnginesDeep, SoDecomposeMatrixIdentityTranslationIsZero)
{
    SoDecomposeMatrix * eng = new SoDecomposeMatrix;
    eng->ref();
    eng->matrix.setValue(SbMatrix::identity());
    eng->center.setValue(SbVec3f(0.0f, 0.0f, 0.0f));

    SoMFVec3f result;
    result.connectFrom(&eng->translation);
    result.evaluate();

    EXPECT_GT(result.getNum(), 0);
    if (result.getNum() > 0) {
        const SbVec3f & t = result[0];
        EXPECT_NEAR(t[0], 0.0f, 1e-5f);
        EXPECT_NEAR(t[1], 0.0f, 1e-5f);
        EXPECT_NEAR(t[2], 0.0f, 1e-5f);
    }
    eng->unref();
}

TEST(EnginesDeep, SoDecomposeMatrixIdentityScaleFactorIsOne)
{
    SoDecomposeMatrix * eng = new SoDecomposeMatrix;
    eng->ref();
    eng->matrix.setValue(SbMatrix::identity());
    eng->center.setValue(SbVec3f(0.0f, 0.0f, 0.0f));

    SoMFVec3f result;
    result.connectFrom(&eng->scaleFactor);
    result.evaluate();

    EXPECT_GT(result.getNum(), 0);
    if (result.getNum() > 0) {
        const SbVec3f & s = result[0];
        EXPECT_NEAR(s[0], 1.0f, 1e-4f);
        EXPECT_NEAR(s[1], 1.0f, 1e-4f);
        EXPECT_NEAR(s[2], 1.0f, 1e-4f);
    }
    eng->unref();
}

// -----------------------------------------------------------------------
// SoDecomposeRotation: identity quaternion → angle ~0
// -----------------------------------------------------------------------

TEST(EnginesDeep, SoDecomposeRotationIdentityRotationAngleIsZero)
{
    SoDecomposeRotation * eng = new SoDecomposeRotation;
    eng->ref();
    eng->rotation.setValue(SbRotation::identity());

    SoMFFloat result;
    result.connectFrom(&eng->angle);
    result.evaluate();

    EXPECT_TRUE((result.getNum() > 0) &&
                (std::fabs(result[0]) < 1e-5f)) << "SoDecomposeRotation identity angle should be ~0";
    eng->unref();
}

// -----------------------------------------------------------------------
// SoComposeRotationFromTo: from=(1,0,0), to=(0,1,0) → non-identity output
// -----------------------------------------------------------------------

TEST(EnginesDeep, SoComposeRotationFromToProducesNonIdentityRotation)
{
    SoComposeRotationFromTo * eng = new SoComposeRotationFromTo;
    eng->ref();
    eng->from.setValue(SbVec3f(1.0f, 0.0f, 0.0f));
    eng->to  .setValue(SbVec3f(0.0f, 1.0f, 0.0f));

    SoMFRotation result;
    result.connectFrom(&eng->rotation);
    result.evaluate();

    EXPECT_GT(result.getNum(), 0);
    if (result.getNum() > 0) {
        SbVec3f axis; float angle;
        result[0].getValue(axis, angle);
        EXPECT_GT(std::fabs(angle), 1e-3f);
    }
    eng->unref();
}

// -----------------------------------------------------------------------
// SoTransformVec3f: (1,0,0) × identity → point ~(1,0,0)
// -----------------------------------------------------------------------

TEST(EnginesDeep, SoTransformVec3fIdentityTransformPreservesVector)
{
    SoTransformVec3f * eng = new SoTransformVec3f;
    eng->ref();
    eng->vector.setValue(SbVec3f(1.0f, 0.0f, 0.0f));
    eng->matrix.setValue(SbMatrix::identity());

    SoMFVec3f result;
    result.connectFrom(&eng->point);
    result.evaluate();

    EXPECT_GT(result.getNum(), 0);
    if (result.getNum() > 0) {
        const SbVec3f & p = result[0];
        EXPECT_NEAR(p[0], 1.0f, 1e-5f);
        EXPECT_NEAR(p[1], 0.0f, 1e-5f);
        EXPECT_NEAR(p[2], 0.0f, 1e-5f);
    }
    eng->unref();
}

// -----------------------------------------------------------------------
// SoInterpolateRotation: alpha=0 gives first input, alpha=1 gives second
// -----------------------------------------------------------------------

TEST(EnginesDeep, SoInterpolateRotationAlpha0ReturnsFirstInput)
{
    SoInterpolateRotation * eng = new SoInterpolateRotation;
    eng->ref();
    eng->input0.setValue(SbRotation::identity());
    eng->input1.setValue(SbRotation(SbVec3f(0, 0, 1),
                                    static_cast<float>(M_PI / 2.0)));
    eng->alpha.setValue(0.0f);

    SoMFRotation result;
    result.connectFrom(&eng->output);
    result.evaluate();

    EXPECT_GT(result.getNum(), 0);
    if (result.getNum() > 0) {
        SbVec3f axis; float angle;
        result[0].getValue(axis, angle);
        EXPECT_NEAR(angle, 0.0f, 1e-3f);
    }
    eng->unref();
}

TEST(EnginesDeep, SoInterpolateRotationAlpha1ReturnsSecondInput)
{
    SoInterpolateRotation * eng = new SoInterpolateRotation;
    eng->ref();
    eng->input0.setValue(SbRotation::identity());
    float target = static_cast<float>(M_PI / 2.0);
    eng->input1.setValue(SbRotation(SbVec3f(0.0f, 0.0f, 1.0f), target));
    eng->alpha.setValue(1.0f);

    SoMFRotation result;
    result.connectFrom(&eng->output);
    result.evaluate();

    EXPECT_GT(result.getNum(), 0);
    if (result.getNum() > 0) {
        SbVec3f axis; float angle;
        result[0].getValue(axis, angle);
        EXPECT_NEAR(std::fabs(angle), target, 0.01f);
    }
    eng->unref();
}

// -----------------------------------------------------------------------
// SoOnOff: on/off/toggle triggers
// -----------------------------------------------------------------------

TEST(EnginesDeep, SoOnOffTriggerOnSetsIsOnToTRUE)
{
    SoOnOff * eng = new SoOnOff;
    eng->ref();
    eng->on.touch();

    SoSFBool result;
    result.connectFrom(&eng->isOn);
    result.evaluate();

    EXPECT_TRUE((result.getValue() == TRUE)) << "SoOnOff: isOn should be TRUE after on trigger";
    eng->unref();
}

TEST(EnginesDeep, SoOnOffTriggerOffSetsIsOnToFALSE)
{
    SoOnOff * eng = new SoOnOff;
    eng->ref();
    eng->on.touch();   // turn on first
    eng->off.touch();  // then turn off

    SoSFBool result;
    result.connectFrom(&eng->isOn);
    result.evaluate();

    EXPECT_TRUE((result.getValue() == FALSE)) << "SoOnOff: isOn should be FALSE after off trigger";
    eng->unref();
}

TEST(EnginesDeep, SoOnOffToggleFlipsState)
{
    SoOnOff * eng = new SoOnOff;
    eng->ref();

    SoSFBool result;
    result.connectFrom(&eng->isOn);

    // Initial state: off
    result.evaluate();
    bool initial = (result.getValue() == FALSE);

    // Toggle once
    eng->toggle.touch();
    result.evaluate();
    bool afterToggle = (result.getValue() == TRUE);

    EXPECT_TRUE(initial && afterToggle) << "SoOnOff: toggle should flip state from FALSE to TRUE";
    eng->unref();
}

// -----------------------------------------------------------------------
// SoTriggerAny: class type check
// -----------------------------------------------------------------------

TEST(EnginesDeep, SoTriggerAnyClassInitialized)
{
    SoTriggerAny * eng = new SoTriggerAny;
    eng->ref();
    EXPECT_TRUE((eng->getTypeId() != SoType::badType())) << "SoTriggerAny has bad type";
    eng->unref();
}
