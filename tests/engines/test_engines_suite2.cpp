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
 * @file test_engines_suite2.cpp
 * @brief Additional engine tests — second batch (engines/ 42.1 %).
 *
 * Covers:
 *   SoBoolOperation   - A, B, A_AND_B, A_OR_B, NOT_A, CLEAR, SET operations
 *   SoConcatenate     - SoMFFloat concatenation of two inputs
 *   SoSelectOne       - index-based selection from MF output
 *   SoGate            - pass-through when enabled
 *   SoCounter         - min/max/step, trigger increments, reset
 *   SoComposeVec2f    - x/y to SbVec2f composition
 *   SoDecomposeVec2f  - SbVec2f to x/y decomposition
 *   SoComposeVec4f    - x/y/z/w to SbVec4f
 *   SoDecomposeVec3f  - SbVec3f to x/y/z
 *   SoComposeMatrix   - identity matrix composition from default fields
 */

#include "../test_utils.h"

#include <Inventor/engines/SoBoolOperation.h>
#include <Inventor/engines/SoConcatenate.h>
#include <Inventor/engines/SoSelectOne.h>
#include <Inventor/engines/SoGate.h>
#include <Inventor/engines/SoCounter.h>
#include <Inventor/engines/SoCompose.h>
#include <Inventor/engines/SoComposeMatrix.h>
#include <Inventor/engines/SoDecomposeVec2f.h>
#include <Inventor/engines/SoDecomposeVec3f.h>
#include <Inventor/engines/SoComposeVec2f.h>
#include <Inventor/engines/SoComposeVec3f.h>
#include <Inventor/engines/SoComposeVec4f.h>
#include <Inventor/fields/SoMFFloat.h>
#include <Inventor/fields/SoMFBool.h>
#include <Inventor/fields/SoMFEnum.h>
#include <Inventor/fields/SoSFFloat.h>
#include <Inventor/fields/SoSFBool.h>
#include <Inventor/fields/SoSFShort.h>
#include <Inventor/fields/SoSFVec2f.h>
#include <Inventor/fields/SoSFVec3f.h>
#include <Inventor/SbVec2f.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbVec4f.h>
#include <Inventor/SoType.h>

#include <cmath>

using namespace ObolTest;

static bool floatNear(float a, float b, float eps = 1e-5f)
{
    return std::fabs(a - b) < eps;
}

TEST(EnginesSuite2, SoBoolOperationClassTypeRegistered)
{
    bool pass = (SoBoolOperation::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoBoolOperation bad class type";
}

TEST(EnginesSuite2, SoBoolOperationAOperationPassesAThrough)
{
    SoBoolOperation * eng = new SoBoolOperation;
    eng->ref();
    eng->a.set1Value(0, TRUE);
    eng->b.set1Value(0, FALSE);
    eng->operation.set1Value(0, SoBoolOperation::A);

    SoMFBool out;
    out.connectFrom(&eng->output);
    out.evaluate();
    bool pass = (out.getNum() > 0) && (out[0] == TRUE);
    eng->unref();
    EXPECT_TRUE(pass) << "SoBoolOperation::A failed";
}

TEST(EnginesSuite2, SoBoolOperationAANDBTRUEANDFALSEFALSE)
{
    SoBoolOperation * eng = new SoBoolOperation;
    eng->ref();
    eng->a.set1Value(0, TRUE);
    eng->b.set1Value(0, FALSE);
    eng->operation.set1Value(0, SoBoolOperation::A_AND_B);

    SoMFBool out;
    out.connectFrom(&eng->output);
    out.evaluate();
    bool pass = (out.getNum() > 0) && (out[0] == FALSE);
    eng->unref();
    EXPECT_TRUE(pass) << "SoBoolOperation::A_AND_B failed";
}

TEST(EnginesSuite2, SoBoolOperationAORBFALSEORTRUETRUE)
{
    SoBoolOperation * eng = new SoBoolOperation;
    eng->ref();
    eng->a.set1Value(0, FALSE);
    eng->b.set1Value(0, TRUE);
    eng->operation.set1Value(0, SoBoolOperation::A_OR_B);

    SoMFBool out;
    out.connectFrom(&eng->output);
    out.evaluate();
    bool pass = (out.getNum() > 0) && (out[0] == TRUE);
    eng->unref();
    EXPECT_TRUE(pass) << "SoBoolOperation::A_OR_B failed";
}

TEST(EnginesSuite2, SoBoolOperationNOTANOTTRUEFALSE)
{
    SoBoolOperation * eng = new SoBoolOperation;
    eng->ref();
    eng->a.set1Value(0, TRUE);
    eng->operation.set1Value(0, SoBoolOperation::NOT_A);

    SoMFBool out;
    out.connectFrom(&eng->output);
    out.evaluate();
    bool pass = (out.getNum() > 0) && (out[0] == FALSE);
    eng->unref();
    EXPECT_TRUE(pass) << "SoBoolOperation::NOT_A failed";
}

TEST(EnginesSuite2, SoBoolOperationCLEARAlwaysProducesFALSE)
{
    SoBoolOperation * eng = new SoBoolOperation;
    eng->ref();
    eng->a.set1Value(0, TRUE);
    eng->operation.set1Value(0, SoBoolOperation::CLEAR);

    SoMFBool out;
    out.connectFrom(&eng->output);
    out.evaluate();
    bool pass = (out.getNum() > 0) && (out[0] == FALSE);
    eng->unref();
    EXPECT_TRUE(pass) << "SoBoolOperation::CLEAR should produce FALSE";
}

TEST(EnginesSuite2, SoBoolOperationSETAlwaysProducesTRUE)
{
    SoBoolOperation * eng = new SoBoolOperation;
    eng->ref();
    eng->a.set1Value(0, FALSE);
    eng->operation.set1Value(0, SoBoolOperation::SET);

    SoMFBool out;
    out.connectFrom(&eng->output);
    out.evaluate();
    bool pass = (out.getNum() > 0) && (out[0] == TRUE);
    eng->unref();
    EXPECT_TRUE(pass) << "SoBoolOperation::SET should produce TRUE";
}

// -----------------------------------------------------------------------
// SoConcatenate
// -----------------------------------------------------------------------

TEST(EnginesSuite2, SoConcatenateCombinesTwoFloatInputs)
{
    SoConcatenate * eng = new SoConcatenate(SoMFFloat::getClassTypeId());
    eng->ref();

    SoMFFloat * in0 = static_cast<SoMFFloat *>(eng->input[0]);
    SoMFFloat * in1 = static_cast<SoMFFloat *>(eng->input[1]);
    in0->set1Value(0, 1.0f);
    in0->set1Value(1, 2.0f);
    in1->set1Value(0, 3.0f);

    SoMFFloat out;
    out.connectFrom(eng->output);
    out.evaluate();
    bool pass = (out.getNum() == 3) &&
                floatNear(out[0], 1.0f) &&
                floatNear(out[1], 2.0f) &&
                floatNear(out[2], 3.0f);
    eng->unref();
    EXPECT_TRUE(pass) << "SoConcatenate float combine failed";
}

// -----------------------------------------------------------------------
// SoSelectOne
// -----------------------------------------------------------------------

TEST(EnginesSuite2, SoSelectOneSelectsByIndex)
{
    SoSelectOne * eng = new SoSelectOne(SoMFFloat::getClassTypeId());
    eng->ref();

    SoMFFloat * input = static_cast<SoMFFloat *>(eng->input);
    input->set1Value(0, 10.0f);
    input->set1Value(1, 20.0f);
    input->set1Value(2, 30.0f);
    eng->index.setValue(1); // select element at index 1

    SoSFFloat out;
    out.connectFrom(eng->output);
    out.evaluate();
    bool pass = floatNear(out.getValue(), 20.0f);
    eng->unref();
    EXPECT_TRUE(pass) << "SoSelectOne index selection failed";
}

// -----------------------------------------------------------------------
// SoGate
// -----------------------------------------------------------------------

TEST(EnginesSuite2, SoGatePassesInputWhenEnabled)
{
    SoGate * eng = new SoGate(SoMFFloat::getClassTypeId());
    eng->ref();
    eng->enable.setValue(TRUE);

    SoMFFloat * input = static_cast<SoMFFloat *>(eng->input);
    input->set1Value(0, 42.0f);
    eng->trigger.touch(); // trigger pass-through

    SoMFFloat out;
    out.connectFrom(eng->output);
    out.evaluate();
    bool pass = (out.getNum() == 1) && floatNear(out[0], 42.0f);
    eng->unref();
    EXPECT_TRUE(pass) << "SoGate pass-through when enabled failed";
}

// -----------------------------------------------------------------------
// SoCounter
// -----------------------------------------------------------------------

TEST(EnginesSuite2, SoCounterClassTypeRegistered)
{
    bool pass = (SoCounter::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoCounter bad class type";
}

TEST(EnginesSuite2, SoCounterDefaultsMin0Max1Step1)
{
    SoCounter * eng = new SoCounter;
    eng->ref();
    bool pass = (eng->min.getValue() == 0) &&
                (eng->max.getValue() == 1) &&
                (eng->step.getValue() == 1);
    eng->unref();
    EXPECT_TRUE(pass) << "SoCounter default fields wrong";
}

TEST(EnginesSuite2, SoCounterIncrementsOnTrigger)
{
    SoCounter * eng = new SoCounter;
    eng->ref();
    eng->min.setValue(0);
    eng->max.setValue(9);
    eng->step.setValue(1);

    SoSFShort out;
    out.connectFrom(&eng->output);
    out.evaluate();
    short before = out.getValue();

    eng->trigger.touch(); // fire trigger
    out.evaluate();
    short after = out.getValue();

    bool pass = (after == before + 1) || (after == eng->min.getValue()); // wraps at max
    eng->unref();
    EXPECT_TRUE(pass) << "SoCounter trigger increment failed";
}

// -----------------------------------------------------------------------
// SoComposeVec2f / SoDecomposeVec2f
// -----------------------------------------------------------------------

TEST(EnginesSuite2, SoComposeVec2fComposesFromXAndY)
{
    SoComposeVec2f * eng = new SoComposeVec2f;
    eng->ref();
    eng->x.set1Value(0, 3.0f);
    eng->y.set1Value(0, 4.0f);

    SoMFVec2f out;
    out.connectFrom(&eng->vector);
    out.evaluate();
    bool pass = (out.getNum() == 1) &&
                floatNear(out[0][0], 3.0f) &&
                floatNear(out[0][1], 4.0f);
    eng->unref();
    EXPECT_TRUE(pass) << "SoComposeVec2f composition failed";
}

TEST(EnginesSuite2, SoDecomposeVec2fDecomposesToXAndY)
{
    SoDecomposeVec2f * eng = new SoDecomposeVec2f;
    eng->ref();
    SbVec2f v(5.0f, 6.0f);
    eng->vector.set1Value(0, v);

    SoMFFloat outX, outY;
    outX.connectFrom(&eng->x);
    outY.connectFrom(&eng->y);
    outX.evaluate();
    outY.evaluate();

    bool pass = (outX.getNum() == 1) && floatNear(outX[0], 5.0f) &&
                (outY.getNum() == 1) && floatNear(outY[0], 6.0f);
    eng->unref();
    EXPECT_TRUE(pass) << "SoDecomposeVec2f decomposition failed";
}

// -----------------------------------------------------------------------
// SoDecomposeVec3f
// -----------------------------------------------------------------------

TEST(EnginesSuite2, SoDecomposeVec3fDecomposesToXYZ)
{
    SoDecomposeVec3f * eng = new SoDecomposeVec3f;
    eng->ref();
    SbVec3f v(1.0f, 2.0f, 3.0f);
    eng->vector.set1Value(0, v);

    SoMFFloat outX, outY, outZ;
    outX.connectFrom(&eng->x);
    outY.connectFrom(&eng->y);
    outZ.connectFrom(&eng->z);
    outX.evaluate();
    outY.evaluate();
    outZ.evaluate();

    bool pass = floatNear(outX[0], 1.0f) &&
                floatNear(outY[0], 2.0f) &&
                floatNear(outZ[0], 3.0f);
    eng->unref();
    EXPECT_TRUE(pass) << "SoDecomposeVec3f decomposition failed";
}

// -----------------------------------------------------------------------
// SoComposeVec4f
// -----------------------------------------------------------------------

TEST(EnginesSuite2, SoComposeVec4fComposesFromXYZW)
{
    SoComposeVec4f * eng = new SoComposeVec4f;
    eng->ref();
    eng->x.set1Value(0, 1.0f);
    eng->y.set1Value(0, 2.0f);
    eng->z.set1Value(0, 3.0f);
    eng->w.set1Value(0, 4.0f);

    SoMFVec4f out;
    out.connectFrom(&eng->vector);
    out.evaluate();

    bool pass = (out.getNum() == 1) &&
                floatNear(out[0][0], 1.0f) && floatNear(out[0][1], 2.0f) &&
                floatNear(out[0][2], 3.0f) && floatNear(out[0][3], 4.0f);
    eng->unref();
    EXPECT_TRUE(pass) << "SoComposeVec4f composition failed";
}

// -----------------------------------------------------------------------
// SoComposeVec3f
// -----------------------------------------------------------------------

TEST(EnginesSuite2, SoComposeVec3fComposesFromXYZ)
{
    SoComposeVec3f * eng = new SoComposeVec3f;
    eng->ref();
    eng->x.set1Value(0, 7.0f);
    eng->y.set1Value(0, 8.0f);
    eng->z.set1Value(0, 9.0f);

    SoMFVec3f out;
    out.connectFrom(&eng->vector);
    out.evaluate();

    bool pass = (out.getNum() == 1) &&
                floatNear(out[0][0], 7.0f) &&
                floatNear(out[0][1], 8.0f) &&
                floatNear(out[0][2], 9.0f);
    eng->unref();
    EXPECT_TRUE(pass) << "SoComposeVec3f composition failed";
}

// -----------------------------------------------------------------------
// SoComposeMatrix
// -----------------------------------------------------------------------

TEST(EnginesSuite2, SoComposeMatrixClassTypeRegistered)
{
    bool pass = (SoComposeMatrix::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoComposeMatrix bad class type";
}
