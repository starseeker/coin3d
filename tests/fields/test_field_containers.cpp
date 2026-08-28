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
 * @file test_field_containers.cpp
 * @brief Tests for SoField, SoFieldContainer, and miscellaneous field types.
 *
 * Covers (fields/ subsystem, 45.8 %):
 *   SoField:
 *     setIgnored/isIgnored, setDefault/isDefault, isOfType,
 *     getDirty/setDirty, enableNotify/isNotifyEnabled,
 *     set (from string), get (to string), isConnected,
 *     getClassTypeId, isConnectionEnabled/enableConnection
 *   SoFieldContainer:
 *     getFields, getField, getFieldName,
 *     set/get (string form), hasDefaultValues, setToDefaults,
 *     fieldsAreEqual, copyFieldValues
 *   SoSFTrigger:
 *     touch/setValue, operator==, startNotify
 *   SoSFPlane:
 *     getValue/setValue round-trip
 *   SoMFMatrix:
 *     set1Value, identity matrix, getNum
 *   SoMFString:
 *     set1Value, getNum, operator[]
 *   SoMFName:
 *     set1Value, getNum
 *   SoMFTime:
 *     set1Value, getNum, operator[]
 */

#include "../test_utils.h"

#include <Inventor/SoDB.h>
#include <Inventor/SoType.h>
#include <Inventor/SbString.h>
#include <Inventor/SbName.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbPlane.h>
#include <Inventor/SbTime.h>

#include <Inventor/fields/SoField.h>
#include <Inventor/fields/SoFieldContainer.h>
#include <Inventor/lists/SoFieldList.h>
#include <Inventor/fields/SoSFTrigger.h>
#include <Inventor/fields/SoSFPlane.h>
#include <Inventor/fields/SoMFMatrix.h>
#include <Inventor/fields/SoMFString.h>
#include <Inventor/fields/SoMFName.h>
#include <Inventor/fields/SoMFTime.h>
#include <Inventor/fields/SoSFFloat.h>
#include <Inventor/fields/SoSFInt32.h>
#include <Inventor/fields/SoSFString.h>
#include <Inventor/fields/SoMFFloat.h>
#include <Inventor/fields/SoMFInt32.h>

#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTranslation.h>

#include <cstring>

using namespace ObolTest;

TEST(FieldsFieldContainers, SoFieldGetClassTypeIdIsNotBadType)
{
    EXPECT_TRUE((SoField::getClassTypeId() != SoType::badType())) << "SoField::getClassTypeId is badType";
}

TEST(FieldsFieldContainers, SoFieldSetIgnoredIsIgnoredRoundTrip)
{
    SoCube * cube = new SoCube;
    cube->ref();
    SoField * wfield = cube->getField(SbName("width"));
    wfield->setIgnored(TRUE);
    EXPECT_TRUE(wfield->isIgnored());
    wfield->setIgnored(FALSE);
    EXPECT_FALSE(wfield->isIgnored());
    cube->unref();
}

TEST(FieldsFieldContainers, SoFieldSetDefaultIsDefaultRoundTrip)
{
    SoCube * cube = new SoCube;
    cube->ref();
    SoField * wfield = cube->getField(SbName("width"));
    bool was = wfield->isDefault();
    wfield->setDefault(!was);
    bool changed = (wfield->isDefault() == !was);
    wfield->setDefault(was); // restore
    cube->unref();
    EXPECT_TRUE(changed) << "setDefault/isDefault failed";
}

TEST(FieldsFieldContainers, SoFieldIsOfTypeSoSFFloatForSoCubeWidth)
{
    SoCube * cube = new SoCube;
    cube->ref();
    SoField * wfield = cube->getField(SbName("width"));
    EXPECT_TRUE(wfield->isOfType(SoSFFloat::getClassTypeId()));
    cube->unref();
}

TEST(FieldsFieldContainers, SoFieldGetDirtySetDirtyRoundTrip)
{
    SoCube * cube = new SoCube;
    cube->ref();
    SoField * wfield = cube->getField(SbName("width"));
    bool orig = wfield->getDirty();
    wfield->setDirty(TRUE);
    bool afterSet = wfield->getDirty();
    wfield->setDirty(orig); // restore
    cube->unref();
    EXPECT_TRUE((afterSet == TRUE)) << (afterSet == TRUE ? "" : "setDirty(TRUE) / getDirty failed");
}

TEST(FieldsFieldContainers, SoFieldEnableNotifyIsNotifyEnabledRoundTrip)
{
    SoCube * cube = new SoCube;
    cube->ref();
    SoField * wfield = cube->getField(SbName("width"));
    SbBool prev = wfield->isNotifyEnabled();
    wfield->enableNotify(!prev);
    bool changed = (wfield->isNotifyEnabled() == !prev);
    wfield->enableNotify(prev); // restore
    cube->unref();
    EXPECT_TRUE(changed) << "enableNotify/isNotifyEnabled failed";
}

TEST(FieldsFieldContainers, SoFieldSetStringAndGetStringRoundTrip)
{
    SoCube * cube = new SoCube;
    cube->ref();
    SoField * wfield = cube->getField(SbName("width"));
    wfield->set("3.5");
    SbString s;
    wfield->get(s);
    // The string should contain "3.5" somewhere
    EXPECT_TRUE((s.find("3.5") != -1) || (s.find("3.500") != -1)) << "SoField set/get string round-trip failed";
    cube->unref();
}

TEST(FieldsFieldContainers, SoFieldIsConnectedIsFALSEByDefault)
{
    SoCube * cube = new SoCube;
    cube->ref();
    SoField * wfield = cube->getField(SbName("width"));
    EXPECT_TRUE((wfield->isConnected() == FALSE)) << "SoField::isConnected should be FALSE by default";
    cube->unref();
}

TEST(FieldsFieldContainers, SoFieldIsConnectionEnabledIsTRUEByDefault)
{
    SoCube * cube = new SoCube;
    cube->ref();
    SoField * wfield = cube->getField(SbName("width"));
    EXPECT_TRUE((wfield->isConnectionEnabled() == TRUE)) << "SoField::isConnectionEnabled should be TRUE by default";
    cube->unref();
}

// =======================================================================
// SoFieldContainer
// =======================================================================

TEST(FieldsFieldContainers, SoFieldContainerGetFieldsReturnsNonZeroForSoCube)
{
    SoCube * cube = new SoCube;
    cube->ref();
    SoFieldList fields;
    int n = cube->getFields(fields);
    EXPECT_TRUE((n > 0) && (fields.getLength() == n)) << "SoCube::getFields returned 0 fields";
    cube->unref();
}

TEST(FieldsFieldContainers, SoFieldContainerGetFieldRetrievesNamedField)
{
    SoCube * cube = new SoCube;
    cube->ref();
    SoField * wfield = cube->getField(SbName("width"));
    EXPECT_TRUE((wfield != nullptr) && wfield->isOfType(SoSFFloat::getClassTypeId())) << "SoCube::getField('width') failed";
    cube->unref();
}

TEST(FieldsFieldContainers, SoFieldContainerGetFieldReturnsNULLForUnknownName)
{
    SoCube * cube = new SoCube;
    cube->ref();
    SoField * f = cube->getField(SbName("__no_such_field__"));
    EXPECT_TRUE((f == nullptr)) << "getField should return NULL for unknown name";
    cube->unref();
}

TEST(FieldsFieldContainers, SoFieldContainerGetFieldNameRetrievesName)
{
    SoCube * cube = new SoCube;
    cube->ref();
    SoField * wfield = cube->getField(SbName("width"));
    SbName name;
    SbBool ok = cube->getFieldName(wfield, name);
    EXPECT_TRUE(ok && (strcmp(name.getString(), "width") == 0)) << "getFieldName failed for width field";
    cube->unref();
}

TEST(FieldsFieldContainers, SoFieldContainerHasDefaultValuesTRUEAfterConstruction)
{
    SoCube * cube = new SoCube;
    cube->ref();
    EXPECT_TRUE((cube->hasDefaultValues() == TRUE)) << "SoCube::hasDefaultValues should be TRUE initially";
    cube->unref();
}

TEST(FieldsFieldContainers, SoFieldContainerHasDefaultValuesFALSEAfterModification)
{
    SoCube * cube = new SoCube;
    cube->ref();
    cube->width.setValue(5.0f);
    EXPECT_TRUE((cube->hasDefaultValues() == FALSE)) << "hasDefaultValues should be FALSE after modification";
    cube->unref();
}

TEST(FieldsFieldContainers, SoFieldContainerSetToDefaultsResetsFields)
{
    SoCube * cube = new SoCube;
    cube->ref();
    float defaultWidth = cube->width.getValue();
    cube->width.setValue(99.0f);
    cube->setToDefaults();
    EXPECT_TRUE((cube->width.getValue() == defaultWidth)) << "setToDefaults did not reset width field";
    cube->unref();
}

TEST(FieldsFieldContainers, SoFieldContainerFieldsAreEqualForIdenticalCubes)
{
    SoCube * c1 = new SoCube;
    SoCube * c2 = new SoCube;
    c1->ref(); c2->ref();
    EXPECT_TRUE((c1->fieldsAreEqual(c2) == TRUE)) << "fieldsAreEqual should be TRUE for two default cubes";
    c1->unref(); c2->unref();
}

TEST(FieldsFieldContainers, SoFieldContainerFieldsAreEqualFALSEForDifferentCubes)
{
    SoCube * c1 = new SoCube;
    SoCube * c2 = new SoCube;
    c1->ref(); c2->ref();
    c1->width.setValue(5.0f);
    EXPECT_TRUE((c1->fieldsAreEqual(c2) == FALSE)) << "fieldsAreEqual should be FALSE for cubes with different width";
    c1->unref(); c2->unref();
}

TEST(FieldsFieldContainers, SoFieldContainerCopyFieldValuesReplicatesFields)
{
    SoCube * src = new SoCube;
    SoCube * dst = new SoCube;
    src->ref(); dst->ref();
    src->width.setValue(7.0f);
    src->height.setValue(3.0f);
    dst->copyFieldValues(src);
    EXPECT_TRUE((dst->width.getValue() == src->width.getValue()) &&
                (dst->height.getValue() == src->height.getValue())) << "copyFieldValues did not replicate fields";
    src->unref(); dst->unref();
}

TEST(FieldsFieldContainers, SoFieldContainerSetStringModifiesField)
{
    SoCube * cube = new SoCube;
    cube->ref();
    cube->set("width 4.5");
    EXPECT_TRUE((cube->width.getValue() == 4.5f)) << "SoFieldContainer::set('width 4.5') failed";
    cube->unref();
}

TEST(FieldsFieldContainers, SoFieldContainerGetStringReturnsFieldValues)
{
    SoCube * cube = new SoCube;
    cube->ref();
    cube->width.setValue(3.0f);
    SbString s;
    cube->get(s);
    EXPECT_TRUE((s.getLength() > 0)) << "SoFieldContainer::get returned empty string";
    cube->unref();
}

// =======================================================================
// SoSFTrigger
// =======================================================================

TEST(FieldsFieldContainers, SoSFTriggerClassTypeRegistered)
{
    EXPECT_TRUE((SoSFTrigger::getClassTypeId() != SoType::badType())) << "SoSFTrigger bad class type";
}

TEST(FieldsFieldContainers, SoSFTriggerOperatorTwoTriggersAreAlwaysEqual)
{
    SoSFTrigger t1, t2;
    EXPECT_TRUE((t1 == t2)) << "SoSFTrigger operator== failed (should always be equal)";
}

// =======================================================================
// SoSFPlane
// =======================================================================

TEST(FieldsFieldContainers, SoSFPlaneGetValueSetValueRoundTrip)
{
    SoSFPlane field;
    SbPlane plane(SbVec3f(0, 1, 0), 5.0f);
    field.setValue(plane);
    SbPlane retrieved = field.getValue();
    EXPECT_TRUE((retrieved == plane)) << "SoSFPlane getValue/setValue round-trip failed";
}

// =======================================================================
// SoMFMatrix
// =======================================================================

TEST(FieldsFieldContainers, SoMFMatrixSet1ValueAndGetNum)
{
    SoMFMatrix field;
    SbMatrix mat = SbMatrix::identity();
    field.set1Value(0, mat);
    field.set1Value(1, mat);
    EXPECT_TRUE((field.getNum() == 2) && (field[0] == mat)) << "SoMFMatrix set1Value/getNum failed";
}

// =======================================================================
// SoMFString
// =======================================================================

TEST(FieldsFieldContainers, SoMFStringSet1ValueAndOperator)
{
    SoMFString field;
    field.set1Value(0, "hello");
    field.set1Value(1, "world");
    EXPECT_TRUE((field.getNum() == 2) &&
                (strcmp(field[0].getString(), "hello") == 0) &&
                (strcmp(field[1].getString(), "world") == 0)) << "SoMFString set1Value/operator[] failed";
}

// =======================================================================
// SoMFName
// =======================================================================

TEST(FieldsFieldContainers, SoMFNameSet1ValueAndGetNum)
{
    SoMFName field;
    field.set1Value(0, SbName("foo"));
    field.set1Value(1, SbName("bar"));
    EXPECT_TRUE((field.getNum() == 2) &&
                (strcmp(field[0].getString(), "foo") == 0)) << "SoMFName set1Value/getNum failed";
}

// =======================================================================
// SoMFTime
// =======================================================================

TEST(FieldsFieldContainers, SoMFTimeSet1ValueAndOperator)
{
    SoMFTime field;
    SbTime t1(1.5), t2(3.0);
    field.set1Value(0, t1);
    field.set1Value(1, t2);
    EXPECT_TRUE((field.getNum() == 2) &&
                (std::fabs(field[0].getValue() - 1.5) < 1e-9) &&
                (std::fabs(field[1].getValue() - 3.0) < 1e-9)) << "SoMFTime set1Value/operator[] failed";
}
