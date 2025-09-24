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
 * @file test_fields.cpp
 * @brief Simple tests for Coin3D fields API
 *
 * Tests basic functionality of field classes without external frameworks.
 * Returns 0 for success, non-zero for failure.
 */

#include "test_utils.h"

// Core field classes  
#include <Inventor/fields/SoSFFloat.h>
#include <Inventor/fields/SoSFInt32.h>
#include <Inventor/fields/SoSFVec3f.h>
#include <Inventor/fields/SoSFColor.h>
#include <Inventor/fields/SoSFString.h>
#include <Inventor/fields/SoSFBool.h>
#include <Inventor/fields/SoMFFloat.h>
#include <Inventor/fields/SoMFInt32.h>
#include <Inventor/fields/SoMFVec3f.h>

// For testing with nodes
#include <Inventor/nodes/SoCube.h>

using namespace SimpleTest;

int main() {
    TestFixture fixture;
    TestRunner runner;
    
    // Test 1: Single-value float field
    runner.startTest("Single-value float field");
    try {
        SoSFFloat floatField;
        
        // Test default value
        floatField.setValue(5.0f);
        if (floatField.getValue() != 5.0f) {
            runner.endTest(false, "Float field value not set correctly");
            return 1;
        }
        
        // Test connection detection
        if (floatField.isConnected()) {
            runner.endTest(false, "Field should not be connected initially");
            return 1;
        }
        
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    // Test 2: Single-value integer field
    runner.startTest("Single-value integer field");
    try {
        SoSFInt32 intField;
        
        intField.setValue(42);
        if (intField.getValue() != 42) {
            runner.endTest(false, "Int field value not set correctly");
            return 1;
        }
        
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    // Test 3: Single-value vector field
    runner.startTest("Single-value vector field");
    try {
        SoSFVec3f vecField;
        
        SbVec3f testVec(1.0f, 2.0f, 3.0f);
        vecField.setValue(testVec);
        
        SbVec3f result = vecField.getValue();
        if (result[0] != 1.0f || result[1] != 2.0f || result[2] != 3.0f) {
            runner.endTest(false, "Vec3f field value not set correctly");
            return 1;
        }
        
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    // Test 4: Single-value color field
    runner.startTest("Single-value color field");
    try {
        SoSFColor colorField;
        
        SbColor testColor(1.0f, 0.0f, 0.0f); // Red
        colorField.setValue(testColor);
        
        SbColor result = colorField.getValue();
        if (result[0] != 1.0f || result[1] != 0.0f || result[2] != 0.0f) {
            runner.endTest(false, "Color field value not set correctly");
            return 1;
        }
        
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    // Test 5: Single-value string field
    runner.startTest("Single-value string field");
    try {
        SoSFString stringField;
        
        stringField.setValue("test string");
        if (stringField.getValue() != SbString("test string")) {
            runner.endTest(false, "String field value not set correctly");
            return 1;
        }
        
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    // Test 6: Single-value boolean field
    runner.startTest("Single-value boolean field");
    try {
        SoSFBool boolField;
        
        boolField.setValue(TRUE);
        if (boolField.getValue() != TRUE) {
            runner.endTest(false, "Boolean field value not set correctly to TRUE");
            return 1;
        }
        
        boolField.setValue(FALSE);
        if (boolField.getValue() != FALSE) {
            runner.endTest(false, "Boolean field value not set correctly to FALSE");
            return 1;
        }
        
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    // Test 7: Multi-value float field
    runner.startTest("Multi-value float field");
    try {
        SoMFFloat mfFloatField;
        
        // Set multiple values
        mfFloatField.setNum(3);
        mfFloatField.set1Value(0, 1.0f);
        mfFloatField.set1Value(1, 2.0f);
        mfFloatField.set1Value(2, 3.0f);
        
        if (mfFloatField.getNum() != 3) {
            runner.endTest(false, "MFFloat field count not correct");
            return 1;
        }
        
        if (mfFloatField[0] != 1.0f || mfFloatField[1] != 2.0f || mfFloatField[2] != 3.0f) {
            runner.endTest(false, "MFFloat field values not set correctly");
            return 1;
        }
        
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    // Test 8: Multi-value vector field
    runner.startTest("Multi-value vector field");
    try {
        SoMFVec3f mfVecField;
        
        // Set multiple vectors
        mfVecField.setNum(2);
        mfVecField.set1Value(0, SbVec3f(1.0f, 0.0f, 0.0f));
        mfVecField.set1Value(1, SbVec3f(0.0f, 1.0f, 0.0f));
        
        if (mfVecField.getNum() != 2) {
            runner.endTest(false, "MFVec3f field count not correct");
            return 1;
        }
        
        SbVec3f vec0 = mfVecField[0];
        SbVec3f vec1 = mfVecField[1];
        
        if (vec0[0] != 1.0f || vec0[1] != 0.0f || vec0[2] != 0.0f) {
            runner.endTest(false, "First MFVec3f vector not set correctly");
            return 1;
        }
        
        if (vec1[0] != 0.0f || vec1[1] != 1.0f || vec1[2] != 0.0f) {
            runner.endTest(false, "Second MFVec3f vector not set correctly");
            return 1;
        }
        
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    // Test 9: Field in node context
    runner.startTest("Field in node context");
    try {
        SoCube* cube = new SoCube;
        cube->ref();
        
        // Test accessing fields from node
        cube->width.setValue(3.0f);
        cube->height.setValue(4.0f);
        cube->depth.setValue(5.0f);
        
        if (cube->width.getValue() != 3.0f ||
            cube->height.getValue() != 4.0f ||
            cube->depth.getValue() != 5.0f) {
            runner.endTest(false, "Node fields not set correctly");
            cube->unref();
            return 1;
        }
        
        runner.endTest(true);
        cube->unref();
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    return runner.getSummary();
}