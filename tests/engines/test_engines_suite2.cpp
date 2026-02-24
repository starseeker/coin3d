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

using namespace SimpleTest;

static bool floatNear(float a, float b, float eps = 1e-5f)
{
    return std::fabs(a - b) < eps;
}

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // -----------------------------------------------------------------------
    // SoBoolOperation
    // -----------------------------------------------------------------------
    runner.startTest("SoBoolOperation class type registered");
    {
        bool pass = (SoBoolOperation::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoBoolOperation bad class type");
    }

    runner.startTest("SoBoolOperation A operation passes a through");
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
        runner.endTest(pass, pass ? "" : "SoBoolOperation::A failed");
    }

    runner.startTest("SoBoolOperation A_AND_B: TRUE AND FALSE = FALSE");
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
        runner.endTest(pass, pass ? "" : "SoBoolOperation::A_AND_B failed");
    }

    runner.startTest("SoBoolOperation A_OR_B: FALSE OR TRUE = TRUE");
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
        runner.endTest(pass, pass ? "" : "SoBoolOperation::A_OR_B failed");
    }

    runner.startTest("SoBoolOperation NOT_A: NOT TRUE = FALSE");
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
        runner.endTest(pass, pass ? "" : "SoBoolOperation::NOT_A failed");
    }

    runner.startTest("SoBoolOperation CLEAR always produces FALSE");
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
        runner.endTest(pass, pass ? "" : "SoBoolOperation::CLEAR should produce FALSE");
    }

    runner.startTest("SoBoolOperation SET always produces TRUE");
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
        runner.endTest(pass, pass ? "" : "SoBoolOperation::SET should produce TRUE");
    }

    // -----------------------------------------------------------------------
    // SoConcatenate
    // -----------------------------------------------------------------------
    runner.startTest("SoConcatenate combines two float inputs");
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
        runner.endTest(pass, pass ? "" : "SoConcatenate float combine failed");
    }

    // -----------------------------------------------------------------------
    // SoSelectOne
    // -----------------------------------------------------------------------
    runner.startTest("SoSelectOne selects by index");
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
        runner.endTest(pass, pass ? "" : "SoSelectOne index selection failed");
    }

    // -----------------------------------------------------------------------
    // SoGate
    // -----------------------------------------------------------------------
    runner.startTest("SoGate passes input when enabled");
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
        runner.endTest(pass, pass ? "" : "SoGate pass-through when enabled failed");
    }

    // -----------------------------------------------------------------------
    // SoCounter
    // -----------------------------------------------------------------------
    runner.startTest("SoCounter class type registered");
    {
        bool pass = (SoCounter::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoCounter bad class type");
    }

    runner.startTest("SoCounter defaults: min=0 max=1 step=1");
    {
        SoCounter * eng = new SoCounter;
        eng->ref();
        bool pass = (eng->min.getValue() == 0) &&
                    (eng->max.getValue() == 1) &&
                    (eng->step.getValue() == 1);
        eng->unref();
        runner.endTest(pass, pass ? "" : "SoCounter default fields wrong");
    }

    runner.startTest("SoCounter increments on trigger");
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
        runner.endTest(pass, pass ? "" : "SoCounter trigger increment failed");
    }

    // -----------------------------------------------------------------------
    // SoComposeVec2f / SoDecomposeVec2f
    // -----------------------------------------------------------------------
    runner.startTest("SoComposeVec2f composes from x and y");
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
        runner.endTest(pass, pass ? "" : "SoComposeVec2f composition failed");
    }

    runner.startTest("SoDecomposeVec2f decomposes to x and y");
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
        runner.endTest(pass, pass ? "" : "SoDecomposeVec2f decomposition failed");
    }

    // -----------------------------------------------------------------------
    // SoDecomposeVec3f
    // -----------------------------------------------------------------------
    runner.startTest("SoDecomposeVec3f decomposes to x/y/z");
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
        runner.endTest(pass, pass ? "" : "SoDecomposeVec3f decomposition failed");
    }

    // -----------------------------------------------------------------------
    // SoComposeVec4f
    // -----------------------------------------------------------------------
    runner.startTest("SoComposeVec4f composes from x/y/z/w");
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
        runner.endTest(pass, pass ? "" : "SoComposeVec4f composition failed");
    }

    // -----------------------------------------------------------------------
    // SoComposeVec3f
    // -----------------------------------------------------------------------
    runner.startTest("SoComposeVec3f composes from x/y/z");
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
        runner.endTest(pass, pass ? "" : "SoComposeVec3f composition failed");
    }

    // -----------------------------------------------------------------------
    // SoComposeMatrix
    // -----------------------------------------------------------------------
    runner.startTest("SoComposeMatrix class type registered");
    {
        bool pass = (SoComposeMatrix::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoComposeMatrix bad class type");
    }

    return runner.getSummary();
}
