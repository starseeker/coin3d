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
 * @file test_field_advanced.cpp
 * @brief Advanced field tests targeting functionality not covered in existing suites.
 *
 * Covers:
 *   SoSFRotation: setValue(axis,angle)/getValue(axis,angle) round-trip,
 *                 composition of two rotations
 *   SoSFMatrix:   setValue(SbMatrix)/getValue round-trip, makeIdentity
 *   SoMFString:   setValues, deleteValues, order preservation
 *   SoMFVec3f:    find() present / absent
 *   SoMFInt32:    setValues from C array, find()
 *   SoMFBool:     set1Value, getNum, value verification
 *   SoField:      isReadOnly, touch, getName
 *   SoFieldContainer: getField, getFields count, getFieldName round-trip
 *
 * Subsystems improved: fields
 */

#include "../test_utils.h"
#include <Inventor/fields/SoSFRotation.h>
#include <Inventor/fields/SoSFMatrix.h>
#include <Inventor/fields/SoMFString.h>
#include <Inventor/fields/SoMFVec3f.h>
#include <Inventor/fields/SoMFInt32.h>
#include <Inventor/fields/SoMFBool.h>
#include <Inventor/fields/SoSFFloat.h>
#include <Inventor/SbRotation.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbName.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/lists/SoFieldList.h>
#include <cmath>
#include <cstring>

using namespace SimpleTest;

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // -----------------------------------------------------------------------
    // SoSFRotation: setValue(axis, angle) / getValue(axis, angle) round-trip
    // -----------------------------------------------------------------------
    runner.startTest("SoSFRotation: setValue(axis,angle) / getValue(axis,angle) round-trip");
    {
        SoSFRotation field;
        SbVec3f axis(0.0f, 1.0f, 0.0f);
        float angle = static_cast<float>(M_PI / 4.0); // 45 degrees
        field.setValue(axis, angle);

        SbVec3f gotAxis;
        float gotAngle;
        field.getValue(gotAxis, gotAngle);

        // Normalize both axes before comparing
        SbVec3f normAxis = axis;
        normAxis.normalize();
        SbVec3f normGot = gotAxis;
        normGot.normalize();

        bool pass = (fabsf(normGot[0] - normAxis[0]) < 1e-4f) &&
                    (fabsf(normGot[1] - normAxis[1]) < 1e-4f) &&
                    (fabsf(normGot[2] - normAxis[2]) < 1e-4f) &&
                    (fabsf(gotAngle  - angle)         < 1e-4f);
        runner.endTest(pass, pass ? "" : "SoSFRotation axis/angle round-trip failed");
    }

    runner.startTest("SoSFRotation: compose two rotations, verify result");
    {
        // Rotate 90 deg around Z then 90 deg around Z again -> 180 deg around Z
        SbRotation r1(SbVec3f(0.0f, 0.0f, 1.0f), static_cast<float>(M_PI / 2.0));
        SbRotation r2(SbVec3f(0.0f, 0.0f, 1.0f), static_cast<float>(M_PI / 2.0));
        SbRotation composed = r1 * r2;

        SoSFRotation field;
        field.setValue(composed);

        SbVec3f gotAxis;
        float gotAngle;
        field.getValue(gotAxis, gotAngle);
        gotAxis.normalize();

        // Result should be ~180 deg around Z (or -Z with angle ~0 edge case handled)
        bool axisZ = (fabsf(fabsf(gotAxis[2]) - 1.0f) < 1e-4f) &&
                     (fabsf(gotAxis[0]) < 1e-4f) &&
                     (fabsf(gotAxis[1]) < 1e-4f);
        bool anglePi = fabsf(fabsf(gotAngle) - static_cast<float>(M_PI)) < 1e-4f;
        bool pass = axisZ && anglePi;
        runner.endTest(pass, pass ? "" : "SoSFRotation composed rotation incorrect");
    }

    // -----------------------------------------------------------------------
    // SoSFMatrix: setValue(SbMatrix) / getValue round-trip
    // -----------------------------------------------------------------------
    runner.startTest("SoSFMatrix: setValue(SbMatrix) / getValue round-trip");
    {
        SbMat mat;
        // Build a simple scaling matrix
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c)
                mat[r][c] = (r == c) ? 2.0f : 0.0f;

        SbMatrix sbm(mat);
        SoSFMatrix field;
        field.setValue(sbm);

        const SbMatrix & got = field.getValue();
        SbMat gotMat;
        got.getValue(gotMat);

        bool pass = true;
        for (int r = 0; r < 4 && pass; ++r)
            for (int c = 0; c < 4 && pass; ++c)
                if (fabsf(gotMat[r][c] - mat[r][c]) > 1e-5f)
                    pass = false;
        runner.endTest(pass, pass ? "" : "SoSFMatrix setValue/getValue round-trip failed");
    }

    runner.startTest("SoSFMatrix: makeIdentity on contained SbMatrix yields identity");
    {
        SbMat mat;
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c)
                mat[r][c] = static_cast<float>(r * 4 + c);

        SbMatrix sbm(mat);
        SoSFMatrix field;
        field.setValue(sbm);

        // Obtain a mutable copy, make identity, put it back
        SbMatrix m = field.getValue();
        m.makeIdentity();
        field.setValue(m);

        const SbMatrix & identity = field.getValue();
        SbMatrix ref = SbMatrix::identity();
        SbMat idMat, refMat;
        identity.getValue(idMat);
        ref.getValue(refMat);

        bool pass = true;
        for (int r = 0; r < 4 && pass; ++r)
            for (int c = 0; c < 4 && pass; ++c)
                if (fabsf(idMat[r][c] - refMat[r][c]) > 1e-5f)
                    pass = false;
        runner.endTest(pass, pass ? "" : "SoSFMatrix makeIdentity failed");
    }

    // -----------------------------------------------------------------------
    // SoMFString: setValues, order preserved, deleteValues
    // -----------------------------------------------------------------------
    runner.startTest("SoMFString: setValues preserves order and count");
    {
        SoMFString field;
        const char * strs[] = { "alpha", "beta", "gamma", "delta" };
        field.setValues(0, 4, strs);
        bool pass = (field.getNum() == 4) &&
                    (strcmp(field[0].getString(), "alpha") == 0) &&
                    (strcmp(field[1].getString(), "beta")  == 0) &&
                    (strcmp(field[2].getString(), "gamma") == 0) &&
                    (strcmp(field[3].getString(), "delta") == 0);
        runner.endTest(pass, pass ? "" : "SoMFString setValues order/count failed");
    }

    runner.startTest("SoMFString: deleteValues reduces count correctly");
    {
        SoMFString field;
        const char * strs[] = { "one", "two", "three", "four", "five" };
        field.setValues(0, 5, strs);
        // Delete 2 values starting at index 1: removes "two","three"
        field.deleteValues(1, 2);
        bool pass = (field.getNum() == 3) &&
                    (strcmp(field[0].getString(), "one")  == 0) &&
                    (strcmp(field[1].getString(), "four") == 0) &&
                    (strcmp(field[2].getString(), "five") == 0);
        runner.endTest(pass, pass ? "" : "SoMFString deleteValues failed");
    }

    runner.startTest("SoMFString: set1Value at multiple indices, getNum, operator[]");
    {
        SoMFString field;
        field.set1Value(0, SbString("x"));
        field.set1Value(1, SbString("y"));
        field.set1Value(2, SbString("z"));
        bool pass = (field.getNum() == 3) &&
                    (strcmp(field[0].getString(), "x") == 0) &&
                    (strcmp(field[1].getString(), "y") == 0) &&
                    (strcmp(field[2].getString(), "z") == 0);
        runner.endTest(pass, pass ? "" : "SoMFString set1Value multi-index failed");
    }

    // -----------------------------------------------------------------------
    // SoMFVec3f: find() present and absent
    // -----------------------------------------------------------------------
    runner.startTest("SoMFVec3f: find() returns correct index when value present");
    {
        SoMFVec3f field;
        field.set1Value(0, SbVec3f(1.0f, 0.0f, 0.0f));
        field.set1Value(1, SbVec3f(0.0f, 1.0f, 0.0f));
        field.set1Value(2, SbVec3f(0.0f, 0.0f, 1.0f));
        int idx = field.find(SbVec3f(0.0f, 1.0f, 0.0f));
        bool pass = (idx == 1);
        runner.endTest(pass, pass ? "" : "SoMFVec3f find() did not return index 1");
    }

    runner.startTest("SoMFVec3f: find() returns -1 when value absent");
    {
        SoMFVec3f field;
        field.set1Value(0, SbVec3f(1.0f, 0.0f, 0.0f));
        field.set1Value(1, SbVec3f(0.0f, 1.0f, 0.0f));
        int idx = field.find(SbVec3f(9.0f, 9.0f, 9.0f));
        bool pass = (idx == -1);
        runner.endTest(pass, pass ? "" : "SoMFVec3f find() should return -1 for absent value");
    }

    // -----------------------------------------------------------------------
    // SoMFInt32: setValues from C array, find()
    // -----------------------------------------------------------------------
    runner.startTest("SoMFInt32: setValues from C array preserves count and values");
    {
        SoMFInt32 field;
        const int32_t arr[] = { 10, 20, 30, 40, 50 };
        field.setValues(0, 5, arr);
        bool pass = (field.getNum() == 5) &&
                    (field[0] == 10) &&
                    (field[2] == 30) &&
                    (field[4] == 50);
        runner.endTest(pass, pass ? "" : "SoMFInt32 setValues from array failed");
    }

    runner.startTest("SoMFInt32: find() returns correct index");
    {
        SoMFInt32 field;
        field.set1Value(0, 100);
        field.set1Value(1, 200);
        field.set1Value(2, 300);
        int idx = field.find(200);
        bool pass = (idx == 1);
        runner.endTest(pass, pass ? "" : "SoMFInt32 find() did not return index 1");
    }

    runner.startTest("SoMFInt32: find() returns -1 for absent value");
    {
        SoMFInt32 field;
        field.set1Value(0, 1);
        field.set1Value(1, 2);
        int idx = field.find(999);
        bool pass = (idx == -1);
        runner.endTest(pass, pass ? "" : "SoMFInt32 find() should return -1 for absent value");
    }

    // -----------------------------------------------------------------------
    // SoMFBool: set1Value, getNum, verify values
    // -----------------------------------------------------------------------
    runner.startTest("SoMFBool: set1Value, getNum, verify stored values");
    {
        SoMFBool field;
        field.set1Value(0, TRUE);
        field.set1Value(1, FALSE);
        field.set1Value(2, TRUE);
        bool pass = (field.getNum() == 3) &&
                    (field[0] == TRUE)    &&
                    (field[1] == FALSE)   &&
                    (field[2] == TRUE);
        runner.endTest(pass, pass ? "" : "SoMFBool set1Value/getNum/values failed");
    }

    // -----------------------------------------------------------------------
    // SoField generic: isReadOnly, touch, getName
    // -----------------------------------------------------------------------
    runner.startTest("SoField::isReadOnly() returns FALSE for normal field");
    {
        SoCube * cube = new SoCube;
        cube->ref();
        SoField * w = cube->getField(SbName("width"));
        bool pass = (w != NULL) && (w->isReadOnly() == FALSE);
        cube->unref();
        runner.endTest(pass, pass ? "" : "SoField::isReadOnly should be FALSE for normal field");
    }

    runner.startTest("SoField::touch() propagates dirty to connected slave");
    {
        SoCube * cube = new SoCube;
        cube->ref();
        SoField * w = cube->getField(SbName("width"));
        // Connect a standalone slave field so touch() has an auditor to notify
        SoSFFloat slave;
        slave.connectFrom(w);
        slave.setDirty(FALSE);
        w->touch();
        // The slave should be marked dirty because the master was touched
        bool pass = (slave.getDirty() == TRUE);
        slave.disconnect();
        cube->unref();
        runner.endTest(pass, pass ? "" : "SoField::touch() did not dirty connected slave");
    }

    runner.startTest("SoField::getName() returns correct field name");
    {
        SoCube * cube = new SoCube;
        cube->ref();
        SoField * h = cube->getField(SbName("height"));
        SbName name;
        SbBool ok = cube->getFieldName(h, name);
        bool pass = ok && (strcmp(name.getString(), "height") == 0);
        cube->unref();
        runner.endTest(pass, pass ? "" : "SoField getName via getFieldName failed");
    }

    // -----------------------------------------------------------------------
    // SoFieldContainer: getField, getFields count, getFieldName round-trip
    // (exercised on SoSphere for variety)
    // -----------------------------------------------------------------------
    runner.startTest("SoFieldContainer::getField returns non-null for valid SoSphere field");
    {
        SoSphere * sphere = new SoSphere;
        sphere->ref();
        SoField * r = sphere->getField(SbName("radius"));
        bool pass = (r != NULL);
        sphere->unref();
        runner.endTest(pass, pass ? "" : "SoSphere::getField('radius') returned NULL");
    }

    runner.startTest("SoFieldContainer::getField returns NULL for nonexistent field");
    {
        SoSphere * sphere = new SoSphere;
        sphere->ref();
        SoField * f = sphere->getField(SbName("__no_such_field__"));
        bool pass = (f == NULL);
        sphere->unref();
        runner.endTest(pass, pass ? "" : "getField should return NULL for unknown name");
    }

    runner.startTest("SoFieldContainer::getFields returns correct count for SoSphere");
    {
        SoSphere * sphere = new SoSphere;
        sphere->ref();
        SoFieldList fl;
        int n = sphere->getFields(fl);
        // SoSphere has at least the 'radius' field
        bool pass = (n >= 1);
        sphere->unref();
        runner.endTest(pass, pass ? "" : "SoSphere::getFields returned 0 fields");
    }

    runner.startTest("SoFieldContainer::getFieldName round-trip on SoSphere");
    {
        SoSphere * sphere = new SoSphere;
        sphere->ref();
        SoField * r = sphere->getField(SbName("radius"));
        SbName name;
        SbBool ok = sphere->getFieldName(r, name);
        bool pass = ok && (strcmp(name.getString(), "radius") == 0);
        sphere->unref();
        runner.endTest(pass, pass ? "" : "SoSphere getFieldName round-trip failed");
    }

    return runner.getSummary() != 0 ? 1 : 0;
}
