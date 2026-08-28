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

using namespace ObolTest;

template <typename FieldType>
class MultiFieldInitialization : public ::testing::Test { };

using MultiFieldTypes = ::testing::Types<
    SoMFFloat, SoMFDouble, SoMFInt32, SoMFShort, SoMFUInt32, SoMFUShort,
    SoMFVec2f, SoMFVec3f, SoMFVec4f, SoMFColor, SoMFString, SoMFRotation,
    SoMFBool, SoMFMatrix, SoMFName, SoMFTime, SoMFPlane, SoMFColorRGBA,
    SoMFEnum, SoMFBitMask, SoMFNode, SoMFVec2d, SoMFVec2i32, SoMFVec2s,
    SoMFVec3d, SoMFVec3i32, SoMFVec3s, SoMFVec4d, SoMFVec4i32, SoMFVec4s,
    SoMFVec2b, SoMFVec3b, SoMFVec4b, SoMFVec4ub, SoMFVec4us,
    SoMFVec4ui32, SoMFPath, SoMFEngine>;

TYPED_TEST_SUITE(MultiFieldInitialization, MultiFieldTypes);

TYPED_TEST(MultiFieldInitialization, StartsEmptyWithValidType)
{
    TypeParam field;
    EXPECT_NE(field.getTypeId(), SoType::badType());
    EXPECT_EQ(field.getNum(), 0);
}

TEST(FieldsMfFields, SoMFFloatSet1ValueGetNumOperator)
{
    SoMFFloat field;
    field.set1Value(0, 1.0f);
    field.set1Value(1, 2.0f);
    field.set1Value(2, 3.0f);
    EXPECT_TRUE((field.getNum() == 3) &&
                (field[0] == 1.0f) &&
                (field[1] == 2.0f) &&
                (field[2] == 3.0f)) << "SoMFFloat set/get values failed";
}

// -----------------------------------------------------------------------
// SoMFVec3f: set/get values
// -----------------------------------------------------------------------

TEST(FieldsMfFields, SoMFVec3fSet1ValueGetNumOperator)
{
    SoMFVec3f field;
    field.set1Value(0, SbVec3f(1.0f, 0.0f, 0.0f));
    field.set1Value(1, SbVec3f(0.0f, 1.0f, 0.0f));
    EXPECT_TRUE((field.getNum() == 2) &&
                (field[0] == SbVec3f(1.0f, 0.0f, 0.0f)) &&
                (field[1] == SbVec3f(0.0f, 1.0f, 0.0f))) << "SoMFVec3f set/get values failed";
}

// -----------------------------------------------------------------------
// SoMFString: set/get values
// -----------------------------------------------------------------------

TEST(FieldsMfFields, SoMFStringSet1ValueGetNumOperator)
{
    SoMFString field;
    field.set1Value(0, "foo");
    field.set1Value(1, "bar");
    EXPECT_TRUE((field.getNum() == 2) &&
                (field[0] == SbString("foo")) &&
                (field[1] == SbString("bar"))) << "SoMFString set/get values failed";
}

// -----------------------------------------------------------------------
// SoMFInt32: deleteValues
// -----------------------------------------------------------------------

TEST(FieldsMfFields, SoMFInt32DeleteValues)
{
    SoMFInt32 field;
    field.set1Value(0, 10);
    field.set1Value(1, 20);
    field.set1Value(2, 30);
    field.deleteValues(1, 1); // remove element at index 1
    EXPECT_TRUE((field.getNum() == 2) &&
                (field[0] == 10) &&
                (field[1] == 30)) << "SoMFInt32 deleteValues failed";
}

// -----------------------------------------------------------------------
// SoMFColor: set/get values
// -----------------------------------------------------------------------

TEST(FieldsMfFields, SoMFColorSet1ValueOperator)
{
    SoMFColor field;
    field.set1Value(0, SbColor(1.0f, 0.0f, 0.0f));
    field.set1Value(1, SbColor(0.0f, 1.0f, 0.0f));
    EXPECT_TRUE((field.getNum() == 2) &&
                (field[0] == SbColor(1.0f, 0.0f, 0.0f)) &&
                (field[1] == SbColor(0.0f, 1.0f, 0.0f))) << "SoMFColor set/get values failed";
}
