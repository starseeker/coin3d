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
 * Baselined against upstream OBOL_TEST_SUITE blocks.
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
#include <Inventor/fields/SoSFColorRGBA.h>
#include <Inventor/fields/SoSFString.h>
#include <Inventor/fields/SoSFRotation.h>
#include <Inventor/fields/SoSFMatrix.h>
#include <Inventor/fields/SoSFName.h>
#include <Inventor/fields/SoSFTime.h>
#include <Inventor/fields/SoSFEnum.h>
#include <Inventor/fields/SoSFBitMask.h>
#include <Inventor/fields/SoSFPlane.h>
#include <Inventor/fields/SoSFNode.h>
#include <Inventor/fields/SoSFTrigger.h>
#include <Inventor/fields/SoSFImage.h>
#include <Inventor/fields/SoSFImage3.h>
#include <Inventor/fields/SoSFVec2d.h>
#include <Inventor/fields/SoSFVec2i32.h>
#include <Inventor/fields/SoSFVec2s.h>
#include <Inventor/fields/SoSFVec3d.h>
#include <Inventor/fields/SoSFVec3i32.h>
#include <Inventor/fields/SoSFVec3s.h>
#include <Inventor/fields/SoSFVec4d.h>
#include <Inventor/fields/SoSFVec4i32.h>
#include <Inventor/fields/SoSFVec4s.h>
#include <Inventor/fields/SoSFVec2b.h>
#include <Inventor/fields/SoSFVec3b.h>
#include <Inventor/fields/SoSFVec4b.h>
#include <Inventor/fields/SoSFVec4ub.h>
#include <Inventor/fields/SoSFVec4us.h>
#include <Inventor/fields/SoSFVec4ui32.h>
#include <Inventor/fields/SoSFPath.h>
#include <Inventor/fields/SoSFEngine.h>
#include <Inventor/fields/SoSFBox2d.h>
#include <Inventor/fields/SoSFBox2f.h>
#include <Inventor/fields/SoSFBox2i32.h>
#include <Inventor/fields/SoSFBox2s.h>
#include <Inventor/fields/SoSFBox3d.h>
#include <Inventor/fields/SoSFBox3f.h>
#include <Inventor/fields/SoSFBox3i32.h>
#include <Inventor/fields/SoSFBox3s.h>
#include <Inventor/SoType.h>
#include <Inventor/SbName.h>

using namespace ObolTest;

template <typename FieldType>
class SingleFieldInitialization : public ::testing::Test { };

using SingleFieldTypes = ::testing::Types<
    SoSFBool, SoSFFloat, SoSFDouble, SoSFInt32, SoSFShort, SoSFUInt32,
    SoSFUShort, SoSFVec2f, SoSFVec3f, SoSFVec4f, SoSFColor, SoSFString,
    SoSFRotation, SoSFMatrix, SoSFName, SoSFTime, SoSFColorRGBA, SoSFEnum,
    SoSFBitMask, SoSFPlane, SoSFNode, SoSFTrigger, SoSFImage, SoSFImage3,
    SoSFVec2d, SoSFVec2i32, SoSFVec2s, SoSFVec3d, SoSFVec3i32, SoSFVec3s,
    SoSFVec4d, SoSFVec4i32, SoSFVec4s, SoSFVec2b, SoSFVec3b, SoSFVec4b,
    SoSFVec4us, SoSFVec4ui32, SoSFPath, SoSFEngine, SoSFBox2d, SoSFBox2f,
    SoSFBox2i32, SoSFBox2s, SoSFBox3d, SoSFBox3f, SoSFBox3i32, SoSFBox3s>;

TYPED_TEST_SUITE(SingleFieldInitialization, SingleFieldTypes);

TYPED_TEST(SingleFieldInitialization, ClassAndInstanceHaveValidTypes)
{
    TypeParam field;
    EXPECT_NE(TypeParam::getClassTypeId(), SoType::badType());
    EXPECT_NE(field.getTypeId(), SoType::badType());
}

TEST(FieldsSfFields, SoSFBoolSetTRUEFALSE)
{
    SoSFBool field;
    EXPECT_TRUE(field.set("TRUE"));
    EXPECT_TRUE(field.getValue());
    EXPECT_TRUE(field.set("FALSE"));
    EXPECT_FALSE(field.getValue());

    // Accept numeric 0/1 as well
    EXPECT_TRUE(field.set("1"));
    EXPECT_TRUE(field.getValue());
    EXPECT_TRUE(field.set("0"));
    EXPECT_FALSE(field.getValue());
}

// Note: SoSFBool::set("MAYBE") triggers SoReadError::post() which may
// crash in the Obol limited-mode (context manager not set). Deferred.

// -----------------------------------------------------------------------
// Remaining SoSF* types: just verify class initialization
// Baseline: individual OBOL_TEST_SUITE (initialized) blocks
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
// SoSFFloat: set/get round-trip
// -----------------------------------------------------------------------

TEST(FieldsSfFields, SoSFFloatSetGetRoundTrip)
{
    SoSFFloat field;
    field.setValue(3.14f);
    EXPECT_TRUE((field.getValue() == 3.14f)) << "SoSFFloat set/get round-trip failed";
}

// -----------------------------------------------------------------------
// SoSFInt32: set/get round-trip
// -----------------------------------------------------------------------

TEST(FieldsSfFields, SoSFInt32SetGetRoundTrip)
{
    SoSFInt32 field;
    field.setValue(42);
    EXPECT_TRUE((field.getValue() == 42)) << "SoSFInt32 set/get round-trip failed";
}

// -----------------------------------------------------------------------
// SoSFVec3f: set/get round-trip
// -----------------------------------------------------------------------

TEST(FieldsSfFields, SoSFVec3fSetGetRoundTrip)
{
    SoSFVec3f field;
    field.setValue(1.0f, 2.0f, 3.0f);
    SbVec3f v = field.getValue();
    EXPECT_TRUE((v[0] == 1.0f && v[1] == 2.0f && v[2] == 3.0f)) << "SoSFVec3f set/get round-trip failed";
}

// -----------------------------------------------------------------------
// SoSFString: set/get round-trip
// -----------------------------------------------------------------------

TEST(FieldsSfFields, SoSFStringSetGetRoundTrip)
{
    SoSFString field;
    field.setValue("hello");
    EXPECT_TRUE((field.getValue() == SbString("hello"))) << "SoSFString set/get round-trip failed";
}

// -----------------------------------------------------------------------
// SoSFColor: set/get round-trip
// -----------------------------------------------------------------------

TEST(FieldsSfFields, SoSFColorSetGetRoundTrip)
{
    SoSFColor field;
    field.setValue(SbColor(0.5f, 0.25f, 0.75f));
    SbColor c = field.getValue();
    EXPECT_TRUE((c[0] == 0.5f && c[1] == 0.25f && c[2] == 0.75f)) << "SoSFColor set/get round-trip failed";
}

// -----------------------------------------------------------------------
// Remaining SoSF* types: class initialized
// Baseline: individual OBOL_TEST_SUITE (initialized) blocks
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
// SoSFImage / SoSFImage3: class initialized
// Baseline: src/fields/SoSFImage.cpp, SoSFImage3.cpp OBOL_TEST_SUITE
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
// SoSFVec2/3/4 variant types: class initialized
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
// Byte/unsigned-integer SoSFVec variants: class initialized + set/get
// Baseline: src/fields/SoSFVec2b.cpp, SoSFVec3b.cpp, SoSFVec4b.cpp,
//           SoSFVec4ub.cpp, SoSFVec4us.cpp, SoSFVec4ui32.cpp
// -----------------------------------------------------------------------

// SoSFVec4ub: class initialized + set/get (mirroring upstream initialized test)

TEST(FieldsSfFields, SoSFVec4ubInitializedAndSetGet)
{
    SoSFVec4ub field;
    field.setValue(1, 2, 3, 4);
    EXPECT_TRUE((field.getTypeId() != SoType::badType()) &&
                (field.getValue() == SbVec4ub(1, 2, 3, 4))) << "SoSFVec4ub set/get failed";
}
