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
 * @file test_field_extras.cpp
 * @brief Deeper SoField tests targeting src/fields/SoField.cpp coverage.
 *
 * Covers (Tier 5, priority 50):
 *   SoField::getDirty / setDirty lifecycle
 *   SoField::isIgnored / setIgnored
 *   SoField::isDefault after setValue
 *   SoFieldContainer::getFields enumeration on SoCube
 *   SoFieldConverter class type registration
 *
 * Subsystems improved: fields
 */

#include "../test_utils.h"

#include <Inventor/fields/SoField.h>
#include <Inventor/lists/SoFieldList.h>
#include <Inventor/fields/SoSFFloat.h>
#include <Inventor/fields/SoSFInt32.h>
#include <Inventor/fields/SoMFFloat.h>
#include <Inventor/fields/SoFieldContainer.h>
#include <Inventor/engines/SoFieldConverter.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/SbName.h>
#include <Inventor/SoType.h>

using namespace ObolTest;

TEST(FieldsFieldExtras, SoSFFloatSetDirtyTRUEGetDirtyTRUE)
{
    SoCube * cube = new SoCube;
    cube->ref();
    SoField * w = cube->getField(SbName("width"));
    // Force clean then dirty again
    w->setDirty(FALSE);
    bool cleanOk = (w->getDirty() == FALSE);
    w->setDirty(TRUE);
    bool dirtyOk = (w->getDirty() == TRUE);
    cube->unref();
    bool pass = cleanOk && dirtyOk;
    EXPECT_TRUE(pass) << "SoField getDirty/setDirty round-trip failed";
}

TEST(FieldsFieldExtras, SoSFFloatSetDirtyFALSEGetDirtyFALSE)
{
    SoCube * cube = new SoCube;
    cube->ref();
    SoField * w = cube->getField(SbName("width"));
    w->setDirty(FALSE);
    bool pass = (w->getDirty() == FALSE);
    cube->unref();
    EXPECT_TRUE(pass) << "SoField getDirty(FALSE) failed";
}

// -----------------------------------------------------------------------
// SoField::isIgnored / setIgnored
// -----------------------------------------------------------------------

TEST(FieldsFieldExtras, SoSFFloatIsIgnoredDefaultsToFALSE)
{
    SoCube * cube = new SoCube;
    cube->ref();
    SoField * w = cube->getField(SbName("width"));
    bool pass = (w->isIgnored() == FALSE);
    cube->unref();
    EXPECT_TRUE(pass) << "SoField::isIgnored should default to FALSE";
}

TEST(FieldsFieldExtras, SoSFFloatSetIgnoredTRUEIsIgnoredTRUE)
{
    SoCube * cube = new SoCube;
    cube->ref();
    SoField * w = cube->getField(SbName("width"));
    w->setIgnored(TRUE);
    bool pass = (w->isIgnored() == TRUE);
    w->setIgnored(FALSE); // restore
    cube->unref();
    EXPECT_TRUE(pass) << "SoField::setIgnored(TRUE) failed";
}

TEST(FieldsFieldExtras, SoSFFloatSetIgnoredFALSERestoresToFALSE)
{
    SoCube * cube = new SoCube;
    cube->ref();
    SoField * w = cube->getField(SbName("width"));
    w->setIgnored(TRUE);
    w->setIgnored(FALSE);
    bool pass = (w->isIgnored() == FALSE);
    cube->unref();
    EXPECT_TRUE(pass) << "SoField::setIgnored(FALSE) restore failed";
}

// -----------------------------------------------------------------------
// SoField::isDefault after setValue
// -----------------------------------------------------------------------

TEST(FieldsFieldExtras, SoCubeWidthIsDefaultBeforeSetValue)
{
    SoCube * cube = new SoCube;
    cube->ref();
    // width field default value is 2.0; should be isDefault == TRUE initially
    bool pass = (cube->width.isDefault() == TRUE);
    cube->unref();
    EXPECT_TRUE(pass) << "SoCube width should be default initially";
}

TEST(FieldsFieldExtras, SoCubeWidthIsDefaultBecomesFALSEAfterSetValue)
{
    SoCube * cube = new SoCube;
    cube->ref();
    cube->width.setValue(5.0f);
    bool pass = (cube->width.isDefault() == FALSE);
    cube->unref();
    EXPECT_TRUE(pass) << "SoCube width should not be default after setValue(5)";
}

// -----------------------------------------------------------------------
// SoFieldContainer::getFields enumeration on SoCube
// -----------------------------------------------------------------------

TEST(FieldsFieldExtras, SoCubeGetFieldsReturnsAtLeast3Fields)
{
    SoCube * cube = new SoCube;
    cube->ref();
    SoFieldList fl;
    int n = cube->getFields(fl);
    // SoCube has width, height, depth (at least 3)
    bool pass = (n >= 3);
    cube->unref();
    EXPECT_TRUE(pass) << "SoCube should report at least 3 fields";
}

TEST(FieldsFieldExtras, SoCubeGetFieldsListContainsSbNameWidth)
{
    SoCube * cube = new SoCube;
    cube->ref();
    SoFieldList fl;
    int n = cube->getFields(fl);
    bool found = false;
    for (int i = 0; i < n; ++i) {
        SbName nm;
        cube->getFieldName(fl[i], nm);
        if (strcmp(nm.getString(), "width") == 0) {
            found = true;
            break;
        }
    }
    cube->unref();
    EXPECT_TRUE(found)
        << "SoCube::getFields should contain field named 'width'";
}

TEST(FieldsFieldExtras, SoMaterialGetFieldsReturnsAtLeast5Fields)
{
    SoMaterial * mat = new SoMaterial;
    mat->ref();
    SoFieldList fl;
    int n = mat->getFields(fl);
    bool pass = (n >= 5);
    mat->unref();
    EXPECT_TRUE(pass) << "SoMaterial should report at least 5 fields";
}

// -----------------------------------------------------------------------
// SoFieldConverter: class type registration
// -----------------------------------------------------------------------

TEST(FieldsFieldExtras, SoFieldConverterGetClassTypeIdIsNotBadType)
{
    bool pass = (SoFieldConverter::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoFieldConverter class type should be registered";
}

TEST(FieldsFieldExtras, SoFieldConverterIsDerivedFromSoEngine)
{
    bool pass = SoFieldConverter::getClassTypeId().isDerivedFrom(
        SoType::fromName(SbName("SoEngine")));
    EXPECT_TRUE(pass) << "SoFieldConverter should be derived from SoEngine";
}

// -----------------------------------------------------------------------
// SoField::isConnected state: freshly created field is not connected
// -----------------------------------------------------------------------

TEST(FieldsFieldExtras, SoCubeWidthIsConnectedReturnsFALSEInitially)
{
    SoCube * cube = new SoCube;
    cube->ref();
    bool pass = (cube->width.isConnected() == FALSE);
    cube->unref();
    EXPECT_TRUE(pass) << "Freshly created SoSFFloat should not be connected";
}

TEST(FieldsFieldExtras, SoCubeWidthIsConnectedFromFieldReturnsFALSEInitially)
{
    SoCube * cube = new SoCube;
    cube->ref();
    bool pass = (cube->width.isConnectedFromField() == FALSE);
    cube->unref();
    EXPECT_TRUE(pass) << "Freshly created SoSFFloat should not be connected from field";
}

TEST(FieldsFieldExtras, SoCubeWidthIsConnectedFromEngineReturnsFALSEInitially)
{
    SoCube * cube = new SoCube;
    cube->ref();
    bool pass = (cube->width.isConnectedFromEngine() == FALSE);
    cube->unref();
    EXPECT_TRUE(pass) << "Freshly created SoSFFloat should not be connected from engine";
}
