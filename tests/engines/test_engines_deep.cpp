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

using namespace SimpleTest;

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // -----------------------------------------------------------------------
    // SoDecomposeMatrix: identity matrix → translation ~(0,0,0)
    // -----------------------------------------------------------------------
    runner.startTest("SoDecomposeMatrix identity translation is zero");
    {
        SoDecomposeMatrix * eng = new SoDecomposeMatrix;
        eng->ref();
        eng->matrix.setValue(SbMatrix::identity());
        eng->center.setValue(SbVec3f(0.0f, 0.0f, 0.0f));

        SoMFVec3f result;
        result.connectFrom(&eng->translation);
        result.evaluate();

        bool pass = false;
        if (result.getNum() > 0) {
            const SbVec3f & t = result[0];
            pass = (std::fabs(t[0]) < 1e-5f) &&
                   (std::fabs(t[1]) < 1e-5f) &&
                   (std::fabs(t[2]) < 1e-5f);
        }
        eng->unref();
        runner.endTest(pass, pass ? "" :
            "SoDecomposeMatrix identity translation should be (0,0,0)");
    }

    runner.startTest("SoDecomposeMatrix identity scaleFactor is one");
    {
        SoDecomposeMatrix * eng = new SoDecomposeMatrix;
        eng->ref();
        eng->matrix.setValue(SbMatrix::identity());
        eng->center.setValue(SbVec3f(0.0f, 0.0f, 0.0f));

        SoMFVec3f result;
        result.connectFrom(&eng->scaleFactor);
        result.evaluate();

        bool pass = false;
        if (result.getNum() > 0) {
            const SbVec3f & s = result[0];
            pass = (std::fabs(s[0] - 1.0f) < 1e-4f) &&
                   (std::fabs(s[1] - 1.0f) < 1e-4f) &&
                   (std::fabs(s[2] - 1.0f) < 1e-4f);
        }
        eng->unref();
        runner.endTest(pass, pass ? "" :
            "SoDecomposeMatrix identity scaleFactor should be (1,1,1)");
    }

    // -----------------------------------------------------------------------
    // SoDecomposeRotation: identity quaternion → angle ~0
    // -----------------------------------------------------------------------
    runner.startTest("SoDecomposeRotation identity rotation angle is zero");
    {
        SoDecomposeRotation * eng = new SoDecomposeRotation;
        eng->ref();
        eng->rotation.setValue(SbRotation::identity());

        SoMFFloat result;
        result.connectFrom(&eng->angle);
        result.evaluate();

        bool pass = (result.getNum() > 0) &&
                    (std::fabs(result[0]) < 1e-5f);
        eng->unref();
        runner.endTest(pass, pass ? "" :
            "SoDecomposeRotation identity angle should be ~0");
    }

    // -----------------------------------------------------------------------
    // SoComposeRotationFromTo: from=(1,0,0), to=(0,1,0) → non-identity output
    // -----------------------------------------------------------------------
    runner.startTest("SoComposeRotationFromTo produces non-identity rotation");
    {
        SoComposeRotationFromTo * eng = new SoComposeRotationFromTo;
        eng->ref();
        eng->from.setValue(SbVec3f(1.0f, 0.0f, 0.0f));
        eng->to  .setValue(SbVec3f(0.0f, 1.0f, 0.0f));

        SoMFRotation result;
        result.connectFrom(&eng->rotation);
        result.evaluate();

        bool pass = false;
        if (result.getNum() > 0) {
            SbVec3f axis; float angle;
            result[0].getValue(axis, angle);
            pass = (std::fabs(angle) > 1e-3f);
        }
        eng->unref();
        runner.endTest(pass, pass ? "" :
            "SoComposeRotationFromTo should produce non-identity rotation");
    }

    // -----------------------------------------------------------------------
    // SoTransformVec3f: (1,0,0) × identity → point ~(1,0,0)
    // -----------------------------------------------------------------------
    runner.startTest("SoTransformVec3f identity transform preserves vector");
    {
        SoTransformVec3f * eng = new SoTransformVec3f;
        eng->ref();
        eng->vector.setValue(SbVec3f(1.0f, 0.0f, 0.0f));
        eng->matrix.setValue(SbMatrix::identity());

        SoMFVec3f result;
        result.connectFrom(&eng->point);
        result.evaluate();

        bool pass = false;
        if (result.getNum() > 0) {
            const SbVec3f & p = result[0];
            pass = (std::fabs(p[0] - 1.0f) < 1e-5f) &&
                   (std::fabs(p[1])         < 1e-5f) &&
                   (std::fabs(p[2])         < 1e-5f);
        }
        eng->unref();
        runner.endTest(pass, pass ? "" :
            "SoTransformVec3f identity should return ~(1,0,0)");
    }

    // -----------------------------------------------------------------------
    // SoInterpolateRotation: alpha=0 gives first input, alpha=1 gives second
    // -----------------------------------------------------------------------
    runner.startTest("SoInterpolateRotation alpha=0 returns first input");
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

        bool pass = false;
        if (result.getNum() > 0) {
            SbVec3f axis; float angle;
            result[0].getValue(axis, angle);
            pass = (std::fabs(angle) < 1e-3f);
        }
        eng->unref();
        runner.endTest(pass, pass ? "" :
            "SoInterpolateRotation alpha=0 should return identity rotation");
    }

    runner.startTest("SoInterpolateRotation alpha=1 returns second input");
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

        bool pass = false;
        if (result.getNum() > 0) {
            SbVec3f axis; float angle;
            result[0].getValue(axis, angle);
            pass = (std::fabs(std::fabs(angle) - target) < 0.01f);
        }
        eng->unref();
        runner.endTest(pass, pass ? "" :
            "SoInterpolateRotation alpha=1 should return ~90-degree rotation");
    }

    // -----------------------------------------------------------------------
    // SoOnOff: on/off/toggle triggers
    // -----------------------------------------------------------------------
    runner.startTest("SoOnOff: trigger 'on' sets isOn to TRUE");
    {
        SoOnOff * eng = new SoOnOff;
        eng->ref();
        eng->on.touch();

        SoSFBool result;
        result.connectFrom(&eng->isOn);
        result.evaluate();

        bool pass = (result.getValue() == TRUE);
        eng->unref();
        runner.endTest(pass, pass ? "" : "SoOnOff: isOn should be TRUE after on trigger");
    }

    runner.startTest("SoOnOff: trigger 'off' sets isOn to FALSE");
    {
        SoOnOff * eng = new SoOnOff;
        eng->ref();
        eng->on.touch();   // turn on first
        eng->off.touch();  // then turn off

        SoSFBool result;
        result.connectFrom(&eng->isOn);
        result.evaluate();

        bool pass = (result.getValue() == FALSE);
        eng->unref();
        runner.endTest(pass, pass ? "" : "SoOnOff: isOn should be FALSE after off trigger");
    }

    runner.startTest("SoOnOff: toggle flips state");
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

        bool pass = initial && afterToggle;
        eng->unref();
        runner.endTest(pass, pass ? "" :
            "SoOnOff: toggle should flip state from FALSE to TRUE");
    }

    // -----------------------------------------------------------------------
    // SoTriggerAny: class type check
    // -----------------------------------------------------------------------
    runner.startTest("SoTriggerAny class initialized");
    {
        SoTriggerAny * eng = new SoTriggerAny;
        eng->ref();
        bool pass = (eng->getTypeId() != SoType::badType());
        eng->unref();
        runner.endTest(pass, pass ? "" : "SoTriggerAny has bad type");
    }

    return runner.getSummary();
}
