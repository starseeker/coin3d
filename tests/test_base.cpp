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
 * @file test_base.cpp
 * @brief Simple tests for Coin3D base classes (SbVec*, SbBox*, etc.)
 *
 * Tests basic functionality of base/math classes without external frameworks.
 * Returns 0 for success, non-zero for failure.
 */

#include "test_utils.h"

// Core base classes
#include <Inventor/SbVec3f.h>
#include <Inventor/SbVec2f.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbRotation.h>
#include <Inventor/SbColor.h>
#include <Inventor/SbBox3f.h>
#include <Inventor/SbPlane.h>
#include <Inventor/SbSphere.h>
#include <Inventor/SbLine.h>

using namespace SimpleTest;

int main() {
    TestFixture fixture;
    TestRunner runner;
    
    // Test 1: SbVec3f basic operations
    runner.startTest("SbVec3f basic operations");
    try {
        SbVec3f vec1(1.0f, 2.0f, 3.0f);
        SbVec3f vec2(4.0f, 5.0f, 6.0f);
        
        // Test construction
        if (vec1[0] != 1.0f || vec1[1] != 2.0f || vec1[2] != 3.0f) {
            runner.endTest(false, "SbVec3f construction failed");
            return 1;
        }
        
        // Test addition
        SbVec3f result = vec1 + vec2;
        if (result[0] != 5.0f || result[1] != 7.0f || result[2] != 9.0f) {
            runner.endTest(false, "SbVec3f addition failed");
            return 1;
        }
        
        // Test dot product
        float dot = vec1.dot(vec2);
        if (dot != 32.0f) { // 1*4 + 2*5 + 3*6 = 32
            runner.endTest(false, "SbVec3f dot product failed");
            return 1;
        }
        
        // Test length
        SbVec3f unitVec(1.0f, 0.0f, 0.0f);
        if (unitVec.length() != 1.0f) {
            runner.endTest(false, "SbVec3f length calculation failed");
            return 1;
        }
        
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    // Test 2: SbVec2f basic operations
    runner.startTest("SbVec2f basic operations");
    try {
        SbVec2f vec1(3.0f, 4.0f);
        SbVec2f vec2(1.0f, 2.0f);
        
        // Test construction
        if (vec1[0] != 3.0f || vec1[1] != 4.0f) {
            runner.endTest(false, "SbVec2f construction failed");
            return 1;
        }
        
        // Test subtraction
        SbVec2f result = vec1 - vec2;
        if (result[0] != 2.0f || result[1] != 2.0f) {
            runner.endTest(false, "SbVec2f subtraction failed");
            return 1;
        }
        
        // Test length (3,4 should be 5.0)
        float len = vec1.length();
        if (len != 5.0f) {
            runner.endTest(false, "SbVec2f length calculation failed");
            return 1;
        }
        
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    // Test 3: SbMatrix basic operations
    runner.startTest("SbMatrix basic operations");
    try {
        SbMatrix mat;
        
        // Test identity matrix
        mat.makeIdentity();
        SbVec3f testVec(1.0f, 2.0f, 3.0f);
        SbVec3f result;
        mat.multVecMatrix(testVec, result);
        
        if (result[0] != 1.0f || result[1] != 2.0f || result[2] != 3.0f) {
            runner.endTest(false, "Identity matrix transformation failed");
            return 1;
        }
        
        // Test translation matrix
        SbMatrix transMat;
        transMat.setTranslate(SbVec3f(5.0f, 0.0f, 0.0f));
        transMat.multVecMatrix(testVec, result);
        
        if (result[0] != 6.0f) { // 1 + 5
            runner.endTest(false, "Translation matrix failed");
            return 1;
        }
        
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    // Test 4: SbRotation basic operations
    runner.startTest("SbRotation basic operations");
    try {
        // Test axis-angle construction
        SbRotation rot(SbVec3f(0, 0, 1), M_PI); // 180 degree rotation around Z
        
        SbVec3f axis;
        float angle;
        rot.getValue(axis, angle);
        
        if (axis[2] != 1.0f) {
            runner.endTest(false, "Rotation axis not set correctly");
            return 1;
        }
        
        if (fabs(angle - M_PI) > 1e-6) {
            runner.endTest(false, "Rotation angle not set correctly");
            return 1;
        }
        
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    // Test 5: SbColor basic operations  
    runner.startTest("SbColor basic operations");
    try {
        SbColor color1(1.0f, 0.0f, 0.0f); // Red
        SbColor color2(0.0f, 1.0f, 0.0f); // Green
        
        // Test construction
        if (color1[0] != 1.0f || color1[1] != 0.0f || color1[2] != 0.0f) {
            runner.endTest(false, "SbColor construction failed");
            return 1;
        }
        
        // Test HSV conversion
        float h, s, v;
        color1.getHSVValue(h, s, v);
        
        // Red should have hue 0, saturation 1, value 1
        if (fabs(h - 0.0f) > 1e-6 || fabs(s - 1.0f) > 1e-6 || fabs(v - 1.0f) > 1e-6) {
            runner.endTest(false, "SbColor HSV conversion failed");
            return 1;
        }
        
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    // Test 6: SbBox3f basic operations
    runner.startTest("SbBox3f basic operations");
    try {
        SbBox3f box;
        
        // Test empty box
        if (!box.isEmpty()) {
            runner.endTest(false, "New box should be empty");
            return 1;
        }
        
        // Test setting bounds
        SbVec3f min(0.0f, 0.0f, 0.0f);
        SbVec3f max(1.0f, 1.0f, 1.0f);
        box.setBounds(min, max);
        
        if (box.isEmpty()) {
            runner.endTest(false, "Box should not be empty after setting bounds");
            return 1;
        }
        
        SbVec3f center = box.getCenter();
        if (center[0] != 0.5f || center[1] != 0.5f || center[2] != 0.5f) {
            runner.endTest(false, "Box center calculation failed");
            return 1;
        }
        
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    // Test 7: SbPlane basic operations
    runner.startTest("SbPlane basic operations");
    try {
        // XY plane (normal pointing in Z direction)
        SbPlane plane(SbVec3f(0, 0, 1), 0.0f);
        
        SbVec3f normal = plane.getNormal();
        if (normal[0] != 0.0f || normal[1] != 0.0f || normal[2] != 1.0f) {
            runner.endTest(false, "Plane normal not set correctly");
            return 1;
        }
        
        float distance = plane.getDistanceFromOrigin();
        if (distance != 0.0f) {
            runner.endTest(false, "Plane distance from origin not correct");
            return 1;
        }
        
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    return runner.getSummary();
}