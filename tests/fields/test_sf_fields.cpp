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
 * @file test_sf_fields.cpp
 * @brief Tests for Coin3D single-value (SoSF*) field types.
 *
 * Baselined against coin_vanilla COIN_TEST_SUITE blocks.
 *
 * Vanilla sources:
 *   src/fields/SoSFBool.cpp   - initialized, textinput
 *   src/fields/SoSFFloat.cpp  - initialized
 *   src/fields/SoSFInt32.cpp  - initialized
 *   src/fields/SoSFVec3f.cpp  - initialized
 *   src/fields/SoSFColor.cpp  - initialized
 *   src/fields/SoSFString.cpp - initialized
 *   src/fields/SoSFRotation.cpp - initialized
 *   src/fields/SoSFDouble.cpp  - initialized
 *   src/fields/SoSFShort.cpp   - initialized
 *   src/fields/SoSFUInt32.cpp  - initialized
 *   src/fields/SoSFVec2f.cpp   - initialized
 *   src/fields/SoSFVec4f.cpp   - initialized
 *   src/fields/SoSFMatrix.cpp  - initialized
 *   src/fields/SoSFName.cpp    - initialized
 *   src/fields/SoSFTime.cpp    - initialized
 */

#include "../test_utils.h"

#include <Inventor/fields/SoSFBool.h>
#include <Inventor/fields/SoSFFloat.h>
#include <Inventor/fields/SoSFDouble.h>
#include <Inventor/fields/SoSFInt32.h>
#include <Inventor/fields/SoSFShort.h>
#include <Inventor/fields/SoSFUInt32.h>
#include <Inventor/fields/SoSFUShort.h>
#include <Inventor/fields/SoSFVec2f.h>
#include <Inventor/fields/SoSFVec3f.h>
#include <Inventor/fields/SoSFVec4f.h>
#include <Inventor/fields/SoSFColor.h>
#include <Inventor/fields/SoSFString.h>
#include <Inventor/fields/SoSFRotation.h>
#include <Inventor/fields/SoSFMatrix.h>
#include <Inventor/fields/SoSFName.h>
#include <Inventor/fields/SoSFTime.h>
#include <Inventor/SoType.h>
#include <Inventor/SbName.h>

using namespace SimpleTest;

// Macro to reduce boilerplate for "class initialized" tests that mirror
// the vanilla COIN_TEST_SUITE pattern.
#define TEST_SF_INITIALIZED(TestName, FieldType) \
    runner.startTest(TestName " class initialized"); \
    { \
        FieldType field; \
        bool pass = (FieldType::getClassTypeId() != SoType::badType()) && \
                    (field.getTypeId() != SoType::badType()); \
        runner.endTest(pass, pass ? "" : \
            TestName " class not initialized or instance has bad type"); \
    }

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // -----------------------------------------------------------------------
    // SoSFBool: class initialized
    // Baseline: src/fields/SoSFBool.cpp COIN_TEST_SUITE (initialized)
    // -----------------------------------------------------------------------
    TEST_SF_INITIALIZED("SoSFBool", SoSFBool)

    // -----------------------------------------------------------------------
    // SoSFBool: text input via set()
    // Baseline: src/fields/SoSFBool.cpp COIN_TEST_SUITE (textinput)
    // -----------------------------------------------------------------------
    runner.startTest("SoSFBool set TRUE/FALSE");
    {
        SoSFBool field;
        bool pass = true;

        if (field.set("TRUE") != TRUE)  { pass = false; }
        if (field.getValue() != TRUE)   { pass = false; }
        if (field.set("FALSE") != TRUE) { pass = false; }
        if (field.getValue() != FALSE)  { pass = false; }

        // Accept numeric 0/1 as well
        if (field.set("1") != TRUE)     { pass = false; }
        if (field.getValue() != TRUE)   { pass = false; }
        if (field.set("0") != TRUE)     { pass = false; }
        if (field.getValue() != FALSE)  { pass = false; }

        runner.endTest(pass, pass ? "" : "SoSFBool::set() failed for TRUE/FALSE/0/1");
    }

    // Note: SoSFBool::set("MAYBE") triggers SoReadError::post() which may
    // crash in the Obol limited-mode (context manager not set). Deferred.

    // -----------------------------------------------------------------------
    // Remaining SoSF* types: just verify class initialization
    // Baseline: individual COIN_TEST_SUITE (initialized) blocks
    // -----------------------------------------------------------------------
    TEST_SF_INITIALIZED("SoSFFloat",    SoSFFloat)
    TEST_SF_INITIALIZED("SoSFDouble",   SoSFDouble)
    TEST_SF_INITIALIZED("SoSFInt32",    SoSFInt32)
    TEST_SF_INITIALIZED("SoSFShort",    SoSFShort)
    TEST_SF_INITIALIZED("SoSFUInt32",   SoSFUInt32)
    TEST_SF_INITIALIZED("SoSFUShort",   SoSFUShort)
    TEST_SF_INITIALIZED("SoSFVec2f",    SoSFVec2f)
    TEST_SF_INITIALIZED("SoSFVec3f",    SoSFVec3f)
    TEST_SF_INITIALIZED("SoSFVec4f",    SoSFVec4f)
    TEST_SF_INITIALIZED("SoSFColor",    SoSFColor)
    TEST_SF_INITIALIZED("SoSFString",   SoSFString)
    TEST_SF_INITIALIZED("SoSFRotation", SoSFRotation)
    TEST_SF_INITIALIZED("SoSFMatrix",   SoSFMatrix)
    TEST_SF_INITIALIZED("SoSFName",     SoSFName)
    TEST_SF_INITIALIZED("SoSFTime",     SoSFTime)

    // -----------------------------------------------------------------------
    // SoSFFloat: set/get round-trip
    // -----------------------------------------------------------------------
    runner.startTest("SoSFFloat set/get round-trip");
    {
        SoSFFloat field;
        field.setValue(3.14f);
        bool pass = (field.getValue() == 3.14f);
        runner.endTest(pass, pass ? "" : "SoSFFloat set/get round-trip failed");
    }

    // -----------------------------------------------------------------------
    // SoSFInt32: set/get round-trip
    // -----------------------------------------------------------------------
    runner.startTest("SoSFInt32 set/get round-trip");
    {
        SoSFInt32 field;
        field.setValue(42);
        bool pass = (field.getValue() == 42);
        runner.endTest(pass, pass ? "" : "SoSFInt32 set/get round-trip failed");
    }

    // -----------------------------------------------------------------------
    // SoSFVec3f: set/get round-trip
    // -----------------------------------------------------------------------
    runner.startTest("SoSFVec3f set/get round-trip");
    {
        SoSFVec3f field;
        field.setValue(1.0f, 2.0f, 3.0f);
        SbVec3f v = field.getValue();
        bool pass = (v[0] == 1.0f && v[1] == 2.0f && v[2] == 3.0f);
        runner.endTest(pass, pass ? "" : "SoSFVec3f set/get round-trip failed");
    }

    // -----------------------------------------------------------------------
    // SoSFString: set/get round-trip
    // -----------------------------------------------------------------------
    runner.startTest("SoSFString set/get round-trip");
    {
        SoSFString field;
        field.setValue("hello");
        bool pass = (field.getValue() == SbString("hello"));
        runner.endTest(pass, pass ? "" : "SoSFString set/get round-trip failed");
    }

    // -----------------------------------------------------------------------
    // SoSFColor: set/get round-trip
    // -----------------------------------------------------------------------
    runner.startTest("SoSFColor set/get round-trip");
    {
        SoSFColor field;
        field.setValue(SbColor(0.5f, 0.25f, 0.75f));
        SbColor c = field.getValue();
        bool pass = (c[0] == 0.5f && c[1] == 0.25f && c[2] == 0.75f);
        runner.endTest(pass, pass ? "" : "SoSFColor set/get round-trip failed");
    }

    return runner.getSummary();
}
