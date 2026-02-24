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

using namespace SimpleTest;

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // -----------------------------------------------------------------------
    // SoField::getDirty / setDirty
    // -----------------------------------------------------------------------
    runner.startTest("SoSFFloat: setDirty(TRUE) -> getDirty() == TRUE");
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
        runner.endTest(pass, pass ? "" : "SoField getDirty/setDirty round-trip failed");
    }

    runner.startTest("SoSFFloat: setDirty(FALSE) -> getDirty() == FALSE");
    {
        SoCube * cube = new SoCube;
        cube->ref();
        SoField * w = cube->getField(SbName("width"));
        w->setDirty(FALSE);
        bool pass = (w->getDirty() == FALSE);
        cube->unref();
        runner.endTest(pass, pass ? "" : "SoField getDirty(FALSE) failed");
    }

    // -----------------------------------------------------------------------
    // SoField::isIgnored / setIgnored
    // -----------------------------------------------------------------------
    runner.startTest("SoSFFloat: isIgnored defaults to FALSE");
    {
        SoCube * cube = new SoCube;
        cube->ref();
        SoField * w = cube->getField(SbName("width"));
        bool pass = (w->isIgnored() == FALSE);
        cube->unref();
        runner.endTest(pass, pass ? "" : "SoField::isIgnored should default to FALSE");
    }

    runner.startTest("SoSFFloat: setIgnored(TRUE) -> isIgnored() == TRUE");
    {
        SoCube * cube = new SoCube;
        cube->ref();
        SoField * w = cube->getField(SbName("width"));
        w->setIgnored(TRUE);
        bool pass = (w->isIgnored() == TRUE);
        w->setIgnored(FALSE); // restore
        cube->unref();
        runner.endTest(pass, pass ? "" : "SoField::setIgnored(TRUE) failed");
    }

    runner.startTest("SoSFFloat: setIgnored(FALSE) restores to FALSE");
    {
        SoCube * cube = new SoCube;
        cube->ref();
        SoField * w = cube->getField(SbName("width"));
        w->setIgnored(TRUE);
        w->setIgnored(FALSE);
        bool pass = (w->isIgnored() == FALSE);
        cube->unref();
        runner.endTest(pass, pass ? "" : "SoField::setIgnored(FALSE) restore failed");
    }

    // -----------------------------------------------------------------------
    // SoField::isDefault after setValue
    // -----------------------------------------------------------------------
    runner.startTest("SoCube width isDefault before setValue");
    {
        SoCube * cube = new SoCube;
        cube->ref();
        // width field default value is 2.0; should be isDefault == TRUE initially
        bool pass = (cube->width.isDefault() == TRUE);
        cube->unref();
        runner.endTest(pass, pass ? "" : "SoCube width should be default initially");
    }

    runner.startTest("SoCube width isDefault becomes FALSE after setValue");
    {
        SoCube * cube = new SoCube;
        cube->ref();
        cube->width.setValue(5.0f);
        bool pass = (cube->width.isDefault() == FALSE);
        cube->unref();
        runner.endTest(pass, pass ? "" :
            "SoCube width should not be default after setValue(5)");
    }

    // -----------------------------------------------------------------------
    // SoFieldContainer::getFields enumeration on SoCube
    // -----------------------------------------------------------------------
    runner.startTest("SoCube::getFields returns at least 3 fields");
    {
        SoCube * cube = new SoCube;
        cube->ref();
        SoFieldList fl;
        int n = cube->getFields(fl);
        // SoCube has width, height, depth (at least 3)
        bool pass = (n >= 3);
        cube->unref();
        runner.endTest(pass, pass ? "" :
            "SoCube should report at least 3 fields");
    }

    runner.startTest("SoCube::getFields list contains SbName 'width'");
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
        runner.endTest(found, found ? "" :
            "SoCube::getFields should contain field named 'width'");
    }

    runner.startTest("SoMaterial::getFields returns at least 5 fields");
    {
        SoMaterial * mat = new SoMaterial;
        mat->ref();
        SoFieldList fl;
        int n = mat->getFields(fl);
        bool pass = (n >= 5);
        mat->unref();
        runner.endTest(pass, pass ? "" :
            "SoMaterial should report at least 5 fields");
    }

    // -----------------------------------------------------------------------
    // SoFieldConverter: class type registration
    // -----------------------------------------------------------------------
    runner.startTest("SoFieldConverter::getClassTypeId is not badType");
    {
        bool pass = (SoFieldConverter::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" :
            "SoFieldConverter class type should be registered");
    }

    runner.startTest("SoFieldConverter is derived from SoEngine");
    {
        bool pass = SoFieldConverter::getClassTypeId().isDerivedFrom(
            SoType::fromName(SbName("SoEngine")));
        runner.endTest(pass, pass ? "" :
            "SoFieldConverter should be derived from SoEngine");
    }

    // -----------------------------------------------------------------------
    // SoField::isConnected state: freshly created field is not connected
    // -----------------------------------------------------------------------
    runner.startTest("SoCube width: isConnected() returns FALSE initially");
    {
        SoCube * cube = new SoCube;
        cube->ref();
        bool pass = (cube->width.isConnected() == FALSE);
        cube->unref();
        runner.endTest(pass, pass ? "" :
            "Freshly created SoSFFloat should not be connected");
    }

    runner.startTest("SoCube width: isConnectedFromField() returns FALSE initially");
    {
        SoCube * cube = new SoCube;
        cube->ref();
        bool pass = (cube->width.isConnectedFromField() == FALSE);
        cube->unref();
        runner.endTest(pass, pass ? "" :
            "Freshly created SoSFFloat should not be connected from field");
    }

    runner.startTest("SoCube width: isConnectedFromEngine() returns FALSE initially");
    {
        SoCube * cube = new SoCube;
        cube->ref();
        bool pass = (cube->width.isConnectedFromEngine() == FALSE);
        cube->unref();
        runner.endTest(pass, pass ? "" :
            "Freshly created SoSFFloat should not be connected from engine");
    }

    return runner.getSummary();
}
