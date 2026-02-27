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
 * @file test_mf_fields.cpp
 * @brief Tests for Coin3D multi-value (SoMF*) field types.
 *
 * Baselined against upstream OBOL_TEST_SUITE blocks.
 *
 * Vanilla sources (all have "initialized" test verifying getTypeId + getNum == 0):
 *   src/fields/SoMFFloat.cpp, SoMFInt32.cpp, SoMFVec3f.cpp, SoMFString.cpp,
 *   SoMFBool.cpp, SoMFColor.cpp, SoMFDouble.cpp, SoMFRotation.cpp,
 *   SoMFShort.cpp, SoMFUInt32.cpp, SoMFVec2f.cpp, SoMFVec4f.cpp,
 *   SoMFMatrix.cpp, SoMFName.cpp, SoMFTime.cpp, SoMFPlane.cpp
 */

#include "../test_utils.h"

#include <Inventor/fields/SoMFFloat.h>
#include <Inventor/fields/SoMFDouble.h>
#include <Inventor/fields/SoMFInt32.h>
#include <Inventor/fields/SoMFShort.h>
#include <Inventor/fields/SoMFUInt32.h>
#include <Inventor/fields/SoMFUShort.h>
#include <Inventor/fields/SoMFVec2f.h>
#include <Inventor/fields/SoMFVec2d.h>
#include <Inventor/fields/SoMFVec2i32.h>
#include <Inventor/fields/SoMFVec2s.h>
#include <Inventor/fields/SoMFVec3f.h>
#include <Inventor/fields/SoMFVec3d.h>
#include <Inventor/fields/SoMFVec3i32.h>
#include <Inventor/fields/SoMFVec3s.h>
#include <Inventor/fields/SoMFVec4f.h>
#include <Inventor/fields/SoMFVec4d.h>
#include <Inventor/fields/SoMFVec4i32.h>
#include <Inventor/fields/SoMFVec4s.h>
#include <Inventor/fields/SoMFVec2b.h>
#include <Inventor/fields/SoMFVec3b.h>
#include <Inventor/fields/SoMFVec4b.h>
#include <Inventor/fields/SoMFVec4ub.h>
#include <Inventor/fields/SoMFVec4us.h>
#include <Inventor/fields/SoMFVec4ui32.h>
#include <Inventor/fields/SoMFPath.h>
#include <Inventor/fields/SoMFEngine.h>
#include <Inventor/fields/SoMFColor.h>
#include <Inventor/fields/SoMFColorRGBA.h>
#include <Inventor/fields/SoMFString.h>
#include <Inventor/fields/SoMFRotation.h>
#include <Inventor/fields/SoMFBool.h>
#include <Inventor/fields/SoMFMatrix.h>
#include <Inventor/fields/SoMFName.h>
#include <Inventor/fields/SoMFTime.h>
#include <Inventor/fields/SoMFPlane.h>
#include <Inventor/fields/SoMFEnum.h>
#include <Inventor/fields/SoMFBitMask.h>
#include <Inventor/fields/SoMFNode.h>
#include <Inventor/SoType.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbColor.h>
#include <Inventor/SbString.h>

using namespace SimpleTest;

// Macro mirroring each vanilla "initialized" test block:
//   BOOST_CHECK that getTypeId() != badType() and getNum() == 0
#define TEST_MF_INITIALIZED(TestName, FieldType) \
    runner.startTest(TestName " initialized"); \
    { \
        FieldType field; \
        bool pass = (field.getTypeId() != SoType::badType()) && \
                    (field.getNum() == 0); \
        runner.endTest(pass, pass ? "" : \
            TestName " not initialized or initial count != 0"); \
    }

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // -----------------------------------------------------------------------
    // All SoMF* types: class initialized with zero elements
    // Baseline: individual OBOL_TEST_SUITE (initialized) blocks
    // -----------------------------------------------------------------------
    TEST_MF_INITIALIZED("SoMFFloat",    SoMFFloat)
    TEST_MF_INITIALIZED("SoMFDouble",   SoMFDouble)
    TEST_MF_INITIALIZED("SoMFInt32",    SoMFInt32)
    TEST_MF_INITIALIZED("SoMFShort",    SoMFShort)
    TEST_MF_INITIALIZED("SoMFUInt32",   SoMFUInt32)
    TEST_MF_INITIALIZED("SoMFUShort",   SoMFUShort)
    TEST_MF_INITIALIZED("SoMFVec2f",    SoMFVec2f)
    TEST_MF_INITIALIZED("SoMFVec3f",    SoMFVec3f)
    TEST_MF_INITIALIZED("SoMFVec4f",    SoMFVec4f)
    TEST_MF_INITIALIZED("SoMFColor",    SoMFColor)
    TEST_MF_INITIALIZED("SoMFString",   SoMFString)
    TEST_MF_INITIALIZED("SoMFRotation", SoMFRotation)
    TEST_MF_INITIALIZED("SoMFBool",     SoMFBool)
    TEST_MF_INITIALIZED("SoMFMatrix",   SoMFMatrix)
    TEST_MF_INITIALIZED("SoMFName",     SoMFName)
    TEST_MF_INITIALIZED("SoMFTime",     SoMFTime)
    TEST_MF_INITIALIZED("SoMFPlane",    SoMFPlane)

    // -----------------------------------------------------------------------
    // SoMFFloat: set/get values
    // -----------------------------------------------------------------------
    runner.startTest("SoMFFloat set1Value/getNum/operator[]");
    {
        SoMFFloat field;
        field.set1Value(0, 1.0f);
        field.set1Value(1, 2.0f);
        field.set1Value(2, 3.0f);
        bool pass = (field.getNum() == 3) &&
                    (field[0] == 1.0f) &&
                    (field[1] == 2.0f) &&
                    (field[2] == 3.0f);
        runner.endTest(pass, pass ? "" : "SoMFFloat set/get values failed");
    }

    // -----------------------------------------------------------------------
    // SoMFVec3f: set/get values
    // -----------------------------------------------------------------------
    runner.startTest("SoMFVec3f set1Value/getNum/operator[]");
    {
        SoMFVec3f field;
        field.set1Value(0, SbVec3f(1.0f, 0.0f, 0.0f));
        field.set1Value(1, SbVec3f(0.0f, 1.0f, 0.0f));
        bool pass = (field.getNum() == 2) &&
                    (field[0] == SbVec3f(1.0f, 0.0f, 0.0f)) &&
                    (field[1] == SbVec3f(0.0f, 1.0f, 0.0f));
        runner.endTest(pass, pass ? "" : "SoMFVec3f set/get values failed");
    }

    // -----------------------------------------------------------------------
    // SoMFString: set/get values
    // -----------------------------------------------------------------------
    runner.startTest("SoMFString set1Value/getNum/operator[]");
    {
        SoMFString field;
        field.set1Value(0, "foo");
        field.set1Value(1, "bar");
        bool pass = (field.getNum() == 2) &&
                    (field[0] == SbString("foo")) &&
                    (field[1] == SbString("bar"));
        runner.endTest(pass, pass ? "" : "SoMFString set/get values failed");
    }

    // -----------------------------------------------------------------------
    // SoMFInt32: deleteValues
    // -----------------------------------------------------------------------
    runner.startTest("SoMFInt32 deleteValues");
    {
        SoMFInt32 field;
        field.set1Value(0, 10);
        field.set1Value(1, 20);
        field.set1Value(2, 30);
        field.deleteValues(1, 1); // remove element at index 1
        bool pass = (field.getNum() == 2) &&
                    (field[0] == 10) &&
                    (field[1] == 30);
        runner.endTest(pass, pass ? "" : "SoMFInt32 deleteValues failed");
    }

    // -----------------------------------------------------------------------
    // SoMFColor: set/get values
    // -----------------------------------------------------------------------
    runner.startTest("SoMFColor set1Value/operator[]");
    {
        SoMFColor field;
        field.set1Value(0, SbColor(1.0f, 0.0f, 0.0f));
        field.set1Value(1, SbColor(0.0f, 1.0f, 0.0f));
        bool pass = (field.getNum() == 2) &&
                    (field[0] == SbColor(1.0f, 0.0f, 0.0f)) &&
                    (field[1] == SbColor(0.0f, 1.0f, 0.0f));
        runner.endTest(pass, pass ? "" : "SoMFColor set/get values failed");
    }

    // -----------------------------------------------------------------------
    // Remaining SoMF* types: class initialized with zero elements
    // Baseline: individual OBOL_TEST_SUITE (initialized) blocks
    // -----------------------------------------------------------------------
    TEST_MF_INITIALIZED("SoMFColorRGBA", SoMFColorRGBA)
    TEST_MF_INITIALIZED("SoMFEnum",      SoMFEnum)
    TEST_MF_INITIALIZED("SoMFBitMask",   SoMFBitMask)
    TEST_MF_INITIALIZED("SoMFNode",      SoMFNode)

    // -----------------------------------------------------------------------
    // SoMFVec2/3/4 variant types: class initialized with zero elements
    // -----------------------------------------------------------------------
    TEST_MF_INITIALIZED("SoMFVec2d",    SoMFVec2d)
    TEST_MF_INITIALIZED("SoMFVec2i32",  SoMFVec2i32)
    TEST_MF_INITIALIZED("SoMFVec2s",    SoMFVec2s)
    TEST_MF_INITIALIZED("SoMFVec3d",    SoMFVec3d)
    TEST_MF_INITIALIZED("SoMFVec3i32",  SoMFVec3i32)
    TEST_MF_INITIALIZED("SoMFVec3s",    SoMFVec3s)
    TEST_MF_INITIALIZED("SoMFVec4d",    SoMFVec4d)
    TEST_MF_INITIALIZED("SoMFVec4i32",  SoMFVec4i32)
    TEST_MF_INITIALIZED("SoMFVec4s",    SoMFVec4s)

    // -----------------------------------------------------------------------
    // Byte/unsigned-integer SoMFVec variants: class initialized
    // Baseline: src/fields/SoMFVec2b.cpp, SoMFVec3b.cpp, SoMFVec4b.cpp,
    //           SoMFVec4ub.cpp, SoMFVec4us.cpp, SoMFVec4ui32.cpp
    // -----------------------------------------------------------------------
    TEST_MF_INITIALIZED("SoMFVec2b",    SoMFVec2b)
    TEST_MF_INITIALIZED("SoMFVec3b",    SoMFVec3b)
    TEST_MF_INITIALIZED("SoMFVec4b",    SoMFVec4b)
    TEST_MF_INITIALIZED("SoMFVec4ub",   SoMFVec4ub)
    TEST_MF_INITIALIZED("SoMFVec4us",   SoMFVec4us)
    TEST_MF_INITIALIZED("SoMFVec4ui32", SoMFVec4ui32)

    // -----------------------------------------------------------------------
    // SoMFPath / SoMFEngine: class initialized
    // Baseline: src/fields/SoMFPath.cpp, SoMFEngine.cpp
    // -----------------------------------------------------------------------
    TEST_MF_INITIALIZED("SoMFPath",     SoMFPath)
    TEST_MF_INITIALIZED("SoMFEngine",   SoMFEngine)

    return runner.getSummary();
}
