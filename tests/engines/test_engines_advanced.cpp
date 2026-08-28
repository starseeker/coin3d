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
 * @file test_engines_advanced.cpp
 * @brief Advanced engine tests — third batch, covering functionality not
 *        exercised by test_engines_suite.cpp, test_engines_suite2.cpp, or
 *        test_engines_deep.cpp.
 *
 * Covers:
 *   SoInterpolateFloat    - mid-point interpolation (alpha = 0.5)
 *   SoInterpolateVec3f    - vector mid-point interpolation
 *   SoDecomposeVec4f      - decompose SbVec4f into x/y/z/w components
 *   SoComposeRotation     - compose axis + angle into SbRotation
 *   SoDecomposeRotation   - decompose non-identity rotation into axis/angle
 *   SoEngineOutput        - isEnabled, getConnectionType, getNumConnections
 *   SoBoolOperation       - A_EQUALS_B, NOT_A_AND_B, and inverse output
 *   SoOneShot             - class type and initial isActive state
 *   SoEngine ref/unref    - ref-count management via getRefCount()
 *   SoCounter             - reset trigger returns output to min
 *   SoConcatenate         - concatenate two MFVec3f arrays
 */

#include "../test_utils.h"

#include <Inventor/engines/SoInterpolateFloat.h>
#include <Inventor/engines/SoInterpolateVec3f.h>
#include <Inventor/engines/SoDecomposeVec4f.h>
#include <Inventor/engines/SoComposeRotation.h>
#include <Inventor/engines/SoDecomposeRotation.h>
#include <Inventor/engines/SoEngineOutput.h>
#include <Inventor/engines/SoBoolOperation.h>
#include <Inventor/engines/SoOneShot.h>
#include <Inventor/engines/SoCounter.h>
#include <Inventor/engines/SoConcatenate.h>
#include <Inventor/engines/SoCalculator.h>
#include <Inventor/fields/SoMFFloat.h>
#include <Inventor/fields/SoMFVec3f.h>
#include <Inventor/fields/SoMFVec4f.h>
#include <Inventor/fields/SoMFBool.h>
#include <Inventor/fields/SoMFRotation.h>
#include <Inventor/fields/SoSFBool.h>
#include <Inventor/fields/SoSFShort.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbVec4f.h>
#include <Inventor/SbRotation.h>
#include <Inventor/SoType.h>
#include <cmath>

using namespace ObolTest;

static bool floatNear(float a, float b, float eps = 1e-4f)
{
    return std::fabs(a - b) < eps;
}

TEST(EnginesAdvanced, SoInterpolateFloatAlpha05GivesMidpoint)
{
    SoInterpolateFloat * eng = new SoInterpolateFloat;
    eng->ref();
    eng->input0.set1Value(0, 0.0f);
    eng->input1.set1Value(0, 10.0f);
    eng->alpha.setValue(0.5f);

    SoMFFloat result;
    result.connectFrom(&eng->output);
    result.evaluate();

    EXPECT_TRUE((result.getNum() > 0) && floatNear(result[0], 5.0f)) << "SoInterpolateFloat alpha=0.5 should yield 5.0";
    eng->unref();
}

TEST(EnginesAdvanced, SoInterpolateFloatAlpha0GivesInput0)
{
    SoInterpolateFloat * eng = new SoInterpolateFloat;
    eng->ref();
    eng->input0.set1Value(0, 3.0f);
    eng->input1.set1Value(0, 7.0f);
    eng->alpha.setValue(0.0f);

    SoMFFloat result;
    result.connectFrom(&eng->output);
    result.evaluate();

    EXPECT_TRUE((result.getNum() > 0) && floatNear(result[0], 3.0f)) << "SoInterpolateFloat alpha=0 should yield input0 (3.0)";
    eng->unref();
}

TEST(EnginesAdvanced, SoInterpolateFloatAlpha1GivesInput1)
{
    SoInterpolateFloat * eng = new SoInterpolateFloat;
    eng->ref();
    eng->input0.set1Value(0, 3.0f);
    eng->input1.set1Value(0, 7.0f);
    eng->alpha.setValue(1.0f);

    SoMFFloat result;
    result.connectFrom(&eng->output);
    result.evaluate();

    EXPECT_TRUE((result.getNum() > 0) && floatNear(result[0], 7.0f)) << "SoInterpolateFloat alpha=1 should yield input1 (7.0)";
    eng->unref();
}

// -----------------------------------------------------------------------
// SoInterpolateVec3f: alpha=0.5 midpoints each component
// -----------------------------------------------------------------------

TEST(EnginesAdvanced, SoInterpolateVec3fAlpha05MidpointsEachComponent)
{
    SoInterpolateVec3f * eng = new SoInterpolateVec3f;
    eng->ref();
    eng->input0.set1Value(0, SbVec3f(0.0f, 0.0f, 0.0f));
    eng->input1.set1Value(0, SbVec3f(2.0f, 4.0f, 6.0f));
    eng->alpha.setValue(0.5f);

    SoMFVec3f result;
    result.connectFrom(&eng->output);
    result.evaluate();

    EXPECT_GT(result.getNum(), 0);
    if (result.getNum() > 0) {
        const SbVec3f & v = result[0];
        EXPECT_NEAR(v[0], 1.0f, 1e-5f);
        EXPECT_NEAR(v[1], 2.0f, 1e-5f);
        EXPECT_NEAR(v[2], 3.0f, 1e-5f);
    }
    eng->unref();
}

// -----------------------------------------------------------------------
// SoDecomposeVec4f: decompose (1,2,3,4) into x/y/z/w
// -----------------------------------------------------------------------

TEST(EnginesAdvanced, SoDecomposeVec4fDecomposesToXYZW)
{
    SoDecomposeVec4f * eng = new SoDecomposeVec4f;
    eng->ref();
    eng->vector.set1Value(0, SbVec4f(1.0f, 2.0f, 3.0f, 4.0f));

    SoMFFloat outX, outY, outZ, outW;
    outX.connectFrom(&eng->x);
    outY.connectFrom(&eng->y);
    outZ.connectFrom(&eng->z);
    outW.connectFrom(&eng->w);
    outX.evaluate();
    outY.evaluate();
    outZ.evaluate();
    outW.evaluate();

    EXPECT_TRUE((outX.getNum() > 0) && floatNear(outX[0], 1.0f) &&
                (outY.getNum() > 0) && floatNear(outY[0], 2.0f) &&
                (outZ.getNum() > 0) && floatNear(outZ[0], 3.0f) &&
                (outW.getNum() > 0) && floatNear(outW[0], 4.0f)) << "SoDecomposeVec4f decomposition of (1,2,3,4) failed";
    eng->unref();
}

// -----------------------------------------------------------------------
// SoComposeRotation: axis=(0,0,1), angle=π/2 → non-identity rotation
// -----------------------------------------------------------------------

TEST(EnginesAdvanced, SoComposeRotationProducesNonIdentityRotation)
{
    SoComposeRotation * eng = new SoComposeRotation;
    eng->ref();
    eng->axis.set1Value(0, SbVec3f(0.0f, 0.0f, 1.0f));
    eng->angle.set1Value(0, static_cast<float>(M_PI / 2.0));

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

TEST(EnginesAdvanced, SoComposeRotationAxis001AnglePi2GivesCorrectAngle)
{
    SoComposeRotation * eng = new SoComposeRotation;
    eng->ref();
    float target = static_cast<float>(M_PI / 2.0);
    eng->axis.set1Value(0, SbVec3f(0.0f, 0.0f, 1.0f));
    eng->angle.set1Value(0, target);

    SoMFRotation result;
    result.connectFrom(&eng->rotation);
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
// SoDecomposeRotation: non-identity — Z-axis π/2 → axis~(0,0,1), angle~π/2
// -----------------------------------------------------------------------

TEST(EnginesAdvanced, SoDecomposeRotationNonIdentityAxisIs001)
{
    SoDecomposeRotation * eng = new SoDecomposeRotation;
    eng->ref();
    float target = static_cast<float>(M_PI / 2.0);
    eng->rotation.set1Value(0, SbRotation(SbVec3f(0.0f, 0.0f, 1.0f), target));

    SoMFVec3f resultAxis;
    resultAxis.connectFrom(&eng->axis);
    resultAxis.evaluate();

    EXPECT_GT(resultAxis.getNum(), 0);
    if (resultAxis.getNum() > 0) {
        const SbVec3f & ax = resultAxis[0];
        // axis should point along +Z or -Z (SbRotation may normalise sign)
        EXPECT_NEAR(std::fabs(ax[2]), 1.0f, 0.01f);
    }
    eng->unref();
}

TEST(EnginesAdvanced, SoDecomposeRotationNonIdentityAngleIsPi2)
{
    SoDecomposeRotation * eng = new SoDecomposeRotation;
    eng->ref();
    float target = static_cast<float>(M_PI / 2.0);
    eng->rotation.set1Value(0, SbRotation(SbVec3f(0.0f, 0.0f, 1.0f), target));

    SoMFFloat resultAngle;
    resultAngle.connectFrom(&eng->angle);
    resultAngle.evaluate();

    EXPECT_TRUE((resultAngle.getNum() > 0) &&
                floatNear(std::fabs(resultAngle[0]), target, 0.01f)) << "SoDecomposeRotation Z-axis rotation angle should be ~pi/2";
    eng->unref();
}

// -----------------------------------------------------------------------
// SoEngineOutput: isEnabled, getConnectionType, getNumConnections
// -----------------------------------------------------------------------

TEST(EnginesAdvanced, SoEngineOutputIsEnabledReturnsTRUEByDefault)
{
    SoCalculator * calc = new SoCalculator;
    calc->ref();
    EXPECT_TRUE((calc->oa.isEnabled() == TRUE)) << "SoEngineOutput should be enabled by default";
    calc->unref();
}

TEST(EnginesAdvanced, SoEngineOutputGetConnectionTypeMatchesSoMFFloat)
{
    SoCalculator * calc = new SoCalculator;
    calc->ref();
    SoType connType = calc->oa.getConnectionType();
    EXPECT_TRUE((connType == SoMFFloat::getClassTypeId())) << "SoCalculator::oa connection type should be SoMFFloat";
    calc->unref();
}

TEST(EnginesAdvanced, SoEngineOutputGetNumConnectionsIs0BeforeConnecting)
{
    SoCalculator * calc = new SoCalculator;
    calc->ref();
    EXPECT_TRUE((calc->oa.getNumConnections() == 0)) << "SoEngineOutput should have 0 connections before any connect";
    calc->unref();
}

TEST(EnginesAdvanced, SoEngineOutputGetNumConnectionsIs1AfterConnecting)
{
    SoCalculator * calc = new SoCalculator;
    calc->ref();
    calc->expression.setValue("oa = 1.0");

    SoMFFloat listener;
    listener.connectFrom(&calc->oa);

    EXPECT_TRUE((calc->oa.getNumConnections() == 1)) << "SoEngineOutput should have 1 connection after connecting a field";
    listener.disconnect();
    calc->unref();
}

// -----------------------------------------------------------------------
// SoBoolOperation: A_EQUALS_B, NOT_A_AND_B, inverse output
// -----------------------------------------------------------------------

TEST(EnginesAdvanced, SoBoolOperationAEQUALSBTRUETRUEGivesTRUE)
{
    SoBoolOperation * eng = new SoBoolOperation;
    eng->ref();
    eng->a.set1Value(0, TRUE);
    eng->b.set1Value(0, TRUE);
    eng->operation.set1Value(0, SoBoolOperation::A_EQUALS_B);

    SoMFBool out;
    out.connectFrom(&eng->output);
    out.evaluate();
    EXPECT_TRUE((out.getNum() > 0) && (out[0] == TRUE)) << "SoBoolOperation::A_EQUALS_B TRUE==TRUE should be TRUE";
    eng->unref();
}

TEST(EnginesAdvanced, SoBoolOperationAEQUALSBTRUEFALSEGivesFALSE)
{
    SoBoolOperation * eng = new SoBoolOperation;
    eng->ref();
    eng->a.set1Value(0, TRUE);
    eng->b.set1Value(0, FALSE);
    eng->operation.set1Value(0, SoBoolOperation::A_EQUALS_B);

    SoMFBool out;
    out.connectFrom(&eng->output);
    out.evaluate();
    EXPECT_TRUE((out.getNum() > 0) && (out[0] == FALSE)) << "SoBoolOperation::A_EQUALS_B TRUE==FALSE should be FALSE";
    eng->unref();
}

TEST(EnginesAdvanced, SoBoolOperationNOTAANDBNOTTRUEANDTRUEFALSE)
{
    SoBoolOperation * eng = new SoBoolOperation;
    eng->ref();
    eng->a.set1Value(0, TRUE);
    eng->b.set1Value(0, TRUE);
    eng->operation.set1Value(0, SoBoolOperation::NOT_A_AND_B);

    SoMFBool out;
    out.connectFrom(&eng->output);
    out.evaluate();
    EXPECT_TRUE((out.getNum() > 0) && (out[0] == FALSE)) << "SoBoolOperation::NOT_A_AND_B (TRUE,TRUE) should be FALSE";
    eng->unref();
}

TEST(EnginesAdvanced, SoBoolOperationNOTAANDBNOTFALSEANDTRUETRUE)
{
    SoBoolOperation * eng = new SoBoolOperation;
    eng->ref();
    eng->a.set1Value(0, FALSE);
    eng->b.set1Value(0, TRUE);
    eng->operation.set1Value(0, SoBoolOperation::NOT_A_AND_B);

    SoMFBool out;
    out.connectFrom(&eng->output);
    out.evaluate();
    EXPECT_TRUE((out.getNum() > 0) && (out[0] == TRUE)) << "SoBoolOperation::NOT_A_AND_B (FALSE,TRUE) should be TRUE";
    eng->unref();
}

TEST(EnginesAdvanced, SoBoolOperationInverseOutputIsComplementOfOutput)
{
    SoBoolOperation * eng = new SoBoolOperation;
    eng->ref();
    eng->a.set1Value(0, TRUE);
    eng->b.set1Value(0, FALSE);
    eng->operation.set1Value(0, SoBoolOperation::A);

    SoMFBool outFwd, outInv;
    outFwd.connectFrom(&eng->output);
    outInv.connectFrom(&eng->inverse);
    outFwd.evaluate();
    outInv.evaluate();

    // output should be TRUE, inverse should be FALSE
    EXPECT_TRUE((outFwd.getNum() > 0) && (outInv.getNum() > 0) &&
                (outFwd[0] == TRUE) && (outInv[0] == FALSE)) << "SoBoolOperation inverse should be complement of output";
    eng->unref();
}

// -----------------------------------------------------------------------
// SoOneShot: class type and initial isActive state
// -----------------------------------------------------------------------

TEST(EnginesAdvanced, SoOneShotClassTypeRegistered)
{
    SoOneShot * eng = new SoOneShot;
    eng->ref();
    EXPECT_TRUE((eng->getTypeId() != SoType::badType())) << "SoOneShot has bad type";
    eng->unref();
}

TEST(EnginesAdvanced, SoOneShotIsActiveInitiallyFALSE)
{
    SoOneShot * eng = new SoOneShot;
    eng->ref();

    SoSFBool result;
    result.connectFrom(&eng->isActive);
    result.evaluate();

    EXPECT_TRUE((result.getValue() == FALSE)) << "SoOneShot isActive should be FALSE before trigger";
    eng->unref();
}

// -----------------------------------------------------------------------
// SoEngine ref/unref: ref-count management
// -----------------------------------------------------------------------

TEST(EnginesAdvanced, SoEngineRefIncrementsRefcount)
{
    SoCalculator * calc = new SoCalculator;
    calc->ref();
    EXPECT_TRUE((calc->getRefCount() == 1)) << "SoEngine refcount should be 1 after one ref()";
    calc->unref();
}

TEST(EnginesAdvanced, SoEngineDoubleRefGivesRefcount2)
{
    SoCalculator * calc = new SoCalculator;
    calc->ref();
    calc->ref();
    EXPECT_TRUE((calc->getRefCount() == 2)) << "SoEngine refcount should be 2 after two ref() calls";
    calc->unref();
    calc->unref();
}

// -----------------------------------------------------------------------
// SoCounter: reset returns output to min
// -----------------------------------------------------------------------

TEST(EnginesAdvanced, SoCounterResetReturnsOutputToMin)
{
    SoCounter * eng = new SoCounter;
    eng->ref();
    eng->min.setValue(5);
    eng->max.setValue(20);
    eng->step.setValue(1);

    // Fire trigger a few times to advance past min
    eng->trigger.touch();
    eng->trigger.touch();
    eng->trigger.touch();

    // Now reset
    eng->reset.touch();

    SoSFShort out;
    out.connectFrom(&eng->output);
    out.evaluate();

    EXPECT_TRUE((out.getValue() == 5)) << "SoCounter reset should return output to min (5)";
    eng->unref();
}

// -----------------------------------------------------------------------
// SoConcatenate MFVec3f: two arrays combined
// -----------------------------------------------------------------------

TEST(EnginesAdvanced, SoConcatenateMFVec3fCombinesTwoVec3fInputs)
{
    SoConcatenate * eng = new SoConcatenate(SoMFVec3f::getClassTypeId());
    eng->ref();

    SoMFVec3f * in0 = static_cast<SoMFVec3f *>(eng->input[0]);
    SoMFVec3f * in1 = static_cast<SoMFVec3f *>(eng->input[1]);
    in0->set1Value(0, SbVec3f(1.0f, 0.0f, 0.0f));
    in1->set1Value(0, SbVec3f(0.0f, 1.0f, 0.0f));
    in1->set1Value(1, SbVec3f(0.0f, 0.0f, 1.0f));

    SoMFVec3f out;
    out.connectFrom(eng->output);
    out.evaluate();

    EXPECT_TRUE((out.getNum() == 3) &&
                floatNear(out[0][0], 1.0f) &&
                floatNear(out[1][1], 1.0f) &&
                floatNear(out[2][2], 1.0f)) << "SoConcatenate MFVec3f combination failed";
    eng->unref();
}
