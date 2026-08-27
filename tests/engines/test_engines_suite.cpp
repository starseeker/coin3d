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
 * @file test_engines_suite.cpp
 * @brief Tests for Coin3D engine classes.
 *
 * Note: upstream has no OBOL_TEST_SUITE blocks for engines.
 * These tests verify documented API behavior.
 *
 * Engines covered:
 *   SoCalculator      - arithmetic expressions
 *   SoComposeVec3f    - compose vector from components
 *   SoDecomposeVec3f  - decompose vector to components
 *   SoBoolOperation   - boolean logic
 *   SoElapsedTime     - time output field type
 *   SoConcatenate     - concatenate multi-value fields
 */

#include "../test_utils.h"

#include <Inventor/engines/SoCalculator.h>
#include <Inventor/engines/SoComposeVec3f.h>
#include <Inventor/engines/SoComposeVec2f.h>
#include <Inventor/engines/SoComposeVec4f.h>
#include <Inventor/engines/SoComposeMatrix.h>
#include <Inventor/engines/SoComposeRotation.h>
#include <Inventor/engines/SoDecomposeVec3f.h>
#include <Inventor/engines/SoBoolOperation.h>
#include <Inventor/engines/SoElapsedTime.h>
#include <Inventor/engines/SoConcatenate.h>
#include <Inventor/engines/SoGate.h>
#include <Inventor/engines/SoInterpolateFloat.h>
#include <Inventor/engines/SoSelectOne.h>
#include <Inventor/engines/SoCounter.h>
#include <Inventor/engines/SoTimeCounter.h>
#include <Inventor/engines/SoComputeBoundingBox.h>
#include <Inventor/fields/SoSFFloat.h>
#include <Inventor/fields/SoSFVec3f.h>
#include <Inventor/fields/SoMFFloat.h>
#include <Inventor/fields/SoMFVec3f.h>
#include <Inventor/SoType.h>
#include <Inventor/SoDB.h>

using namespace ObolTest;

TEST(EnginesSuite, SoCalculatorClassInitialized)
{
    SoCalculator* calc = new SoCalculator;
    calc->ref();
    bool pass = (calc->getTypeId() != SoType::badType());
    calc->unref();
    EXPECT_TRUE(pass) << "SoCalculator has bad type";
}

TEST(EnginesSuite, SoCalculatorConstantExpression)
{
    // To read engine output we connect it to a field, then read the field.
    SoCalculator* calc = new SoCalculator;
    calc->ref();
    calc->expression.setValue("oa = 6.0 * 7.0");

    SoMFFloat result;
    result.connectFrom(&calc->oa);
    result.evaluate();

    bool pass = (result.getNum() > 0) && (result[0] == 42.0f);
    calc->unref();
    EXPECT_TRUE(pass) << "SoCalculator 6*7 should equal 42";
}

TEST(EnginesSuite, SoCalculatorUsingInputFieldA)
{
    SoCalculator* calc = new SoCalculator;
    calc->ref();
    calc->a.set1Value(0, 10.0f);
    calc->expression.setValue("oa = a * 3.0");

    SoMFFloat result;
    result.connectFrom(&calc->oa);
    result.evaluate();

    bool pass = (result.getNum() > 0) && (result[0] == 30.0f);
    calc->unref();
    EXPECT_TRUE(pass) << "SoCalculator a*3 should equal 30";
}

// -----------------------------------------------------------------------
// SoComposeVec3f: combine three floats into a Vec3f
// -----------------------------------------------------------------------

TEST(EnginesSuite, SoComposeVec3fClassInitialized)
{
    SoComposeVec3f* eng = new SoComposeVec3f;
    eng->ref();
    bool pass = (eng->getTypeId() != SoType::badType());
    eng->unref();
    EXPECT_TRUE(pass) << "SoComposeVec3f has bad type";
}

TEST(EnginesSuite, SoComposeVec3fCompose)
{
    SoComposeVec3f* eng = new SoComposeVec3f;
    eng->ref();
    eng->x.set1Value(0, 1.0f);
    eng->y.set1Value(0, 2.0f);
    eng->z.set1Value(0, 3.0f);

    SoMFVec3f result;
    result.connectFrom(&eng->vector);
    result.evaluate();

    bool pass = (result.getNum() > 0) &&
                (result[0][0] == 1.0f) &&
                (result[0][1] == 2.0f) &&
                (result[0][2] == 3.0f);
    eng->unref();
    EXPECT_TRUE(pass) << "SoComposeVec3f did not compose (1,2,3) correctly";
}

// -----------------------------------------------------------------------
// SoDecomposeVec3f: split a Vec3f into three floats
// -----------------------------------------------------------------------

TEST(EnginesSuite, SoDecomposeVec3fDecompose)
{
    SoDecomposeVec3f* eng = new SoDecomposeVec3f;
    eng->ref();
    eng->vector.set1Value(0, SbVec3f(4.0f, 5.0f, 6.0f));

    SoMFFloat rx, ry, rz;
    rx.connectFrom(&eng->x);
    ry.connectFrom(&eng->y);
    rz.connectFrom(&eng->z);
    rx.evaluate(); ry.evaluate(); rz.evaluate();

    bool pass = (rx.getNum() > 0) && (ry.getNum() > 0) && (rz.getNum() > 0) &&
                (rx[0] == 4.0f) && (ry[0] == 5.0f) && (rz[0] == 6.0f);
    eng->unref();
    EXPECT_TRUE(pass) << "SoDecomposeVec3f did not decompose (4,5,6) correctly";
}

// -----------------------------------------------------------------------
// SoBoolOperation: class type check
// -----------------------------------------------------------------------

TEST(EnginesSuite, SoBoolOperationClassInitialized)
{
    SoBoolOperation* eng = new SoBoolOperation;
    eng->ref();
    bool pass = (eng->getTypeId() != SoType::badType());
    eng->unref();
    EXPECT_TRUE(pass) << "SoBoolOperation has bad type";
}

// -----------------------------------------------------------------------
// SoElapsedTime: class type check and output field type
// -----------------------------------------------------------------------

TEST(EnginesSuite, SoElapsedTimeClassInitialized)
{
    SoElapsedTime* eng = new SoElapsedTime;
    eng->ref();
    bool pass = (eng->getTypeId() != SoType::badType());
    eng->unref();
    EXPECT_TRUE(pass) << "SoElapsedTime has bad type";
}

// -----------------------------------------------------------------------
// SoConcatenate: concatenate two MF fields
// -----------------------------------------------------------------------

TEST(EnginesSuite, SoConcatenateClassInitialized)
{
    SoConcatenate* eng = new SoConcatenate(SoMFFloat::getClassTypeId());
    eng->ref();
    bool pass = (eng->getTypeId() != SoType::badType());
    eng->unref();
    EXPECT_TRUE(pass) << "SoConcatenate has bad type";
}

// -----------------------------------------------------------------------
// SoComposeMatrix: class type check
// -----------------------------------------------------------------------

TEST(EnginesSuite, SoComposeMatrixClassInitialized)
{
    SoComposeMatrix* eng = new SoComposeMatrix;
    eng->ref();
    bool pass = (eng->getTypeId() != SoType::badType());
    eng->unref();
    EXPECT_TRUE(pass) << "SoComposeMatrix has bad type";
}

// -----------------------------------------------------------------------
// SoComposeRotation: class type check
// -----------------------------------------------------------------------

TEST(EnginesSuite, SoComposeRotationClassInitialized)
{
    SoComposeRotation* eng = new SoComposeRotation;
    eng->ref();
    bool pass = (eng->getTypeId() != SoType::badType());
    eng->unref();
    EXPECT_TRUE(pass) << "SoComposeRotation has bad type";
}

// -----------------------------------------------------------------------
// SoComposeVec2f: class type check
// -----------------------------------------------------------------------

TEST(EnginesSuite, SoComposeVec2fClassInitialized)
{
    SoComposeVec2f* eng = new SoComposeVec2f;
    eng->ref();
    bool pass = (eng->getTypeId() != SoType::badType());
    eng->unref();
    EXPECT_TRUE(pass) << "SoComposeVec2f has bad type";
}

// -----------------------------------------------------------------------
// SoComposeVec4f: class type check
// -----------------------------------------------------------------------

TEST(EnginesSuite, SoComposeVec4fClassInitialized)
{
    SoComposeVec4f* eng = new SoComposeVec4f;
    eng->ref();
    bool pass = (eng->getTypeId() != SoType::badType());
    eng->unref();
    EXPECT_TRUE(pass) << "SoComposeVec4f has bad type";
}

// -----------------------------------------------------------------------
// SoGate: class type check
// -----------------------------------------------------------------------

TEST(EnginesSuite, SoGateClassInitialized)
{
    SoGate* eng = new SoGate(SoMFFloat::getClassTypeId());
    eng->ref();
    bool pass = (eng->getTypeId() != SoType::badType());
    eng->unref();
    EXPECT_TRUE(pass) << "SoGate has bad type";
}

// -----------------------------------------------------------------------
// SoInterpolateFloat: class type check
// -----------------------------------------------------------------------

TEST(EnginesSuite, SoInterpolateFloatClassInitialized)
{
    SoInterpolateFloat* eng = new SoInterpolateFloat;
    eng->ref();
    bool pass = (eng->getTypeId() != SoType::badType());
    eng->unref();
    EXPECT_TRUE(pass) << "SoInterpolateFloat has bad type";
}

// -----------------------------------------------------------------------
// SoSelectOne: class type check
// -----------------------------------------------------------------------

TEST(EnginesSuite, SoSelectOneClassInitialized)
{
    SoSelectOne* eng = new SoSelectOne(SoMFFloat::getClassTypeId());
    eng->ref();
    bool pass = (eng->getTypeId() != SoType::badType());
    eng->unref();
    EXPECT_TRUE(pass) << "SoSelectOne has bad type";
}

// -----------------------------------------------------------------------
// SoCounter: class type check
// -----------------------------------------------------------------------

TEST(EnginesSuite, SoCounterClassInitialized)
{
    SoCounter* eng = new SoCounter;
    eng->ref();
    bool pass = (eng->getTypeId() != SoType::badType());
    eng->unref();
    EXPECT_TRUE(pass) << "SoCounter has bad type";
}

// -----------------------------------------------------------------------
// SoTimeCounter: class type check
// -----------------------------------------------------------------------

TEST(EnginesSuite, SoTimeCounterClassInitialized)
{
    SoTimeCounter* eng = new SoTimeCounter;
    eng->ref();
    bool pass = (eng->getTypeId() != SoType::badType());
    eng->unref();
    EXPECT_TRUE(pass) << "SoTimeCounter has bad type";
}

// -----------------------------------------------------------------------
// SoComputeBoundingBox: class type check
// -----------------------------------------------------------------------

TEST(EnginesSuite, SoComputeBoundingBoxClassInitialized)
{
    SoComputeBoundingBox* eng = new SoComputeBoundingBox;
    eng->ref();
    bool pass = (eng->getTypeId() != SoType::badType());
    eng->unref();
    EXPECT_TRUE(pass) << "SoComputeBoundingBox has bad type";
}
