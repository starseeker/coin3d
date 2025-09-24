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
 * @file test_nodes.cpp
 * @brief Simple tests for Coin3D nodes API
 *
 * Tests basic functionality of node classes without external frameworks.
 * Returns 0 for success, non-zero for failure.
 */

#include "test_utils.h"

// Core node classes
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoRotation.h>
#include <Inventor/nodes/SoScale.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoCamera.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>

using namespace SimpleTest;

int main() {
    TestFixture fixture;
    TestRunner runner;
    
    // Test 1: Basic node creation and type checking
    runner.startTest("Basic node creation and type checking");
    try {
        SoSeparator* sep = new SoSeparator;
        SoCube* cube = new SoCube;
        SoSphere* sphere = new SoSphere;
        
        if (sep->getTypeId() == SoType::badType()) {
            runner.endTest(false, "SoSeparator has bad type");
            return 1;
        }
        
        if (cube->getTypeId() == SoType::badType()) {
            runner.endTest(false, "SoCube has bad type");
            return 1;
        }
        
        if (sphere->getTypeId() == SoType::badType()) {
            runner.endTest(false, "SoSphere has bad type");
            return 1;
        }
        
        // Clean up
        sep->ref();
        cube->ref();
        sphere->ref();
        
        sep->unref();
        cube->unref();
        sphere->unref();
        
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    // Test 2: Scene graph construction
    runner.startTest("Scene graph construction");
    try {
        SoSeparator* root = new SoSeparator;
        root->ref();
        
        SoTranslation* trans = new SoTranslation;
        trans->translation.setValue(1.0f, 2.0f, 3.0f);
        
        SoCube* cube = new SoCube;
        cube->width.setValue(2.0f);
        cube->height.setValue(2.0f);
        cube->depth.setValue(2.0f);
        
        root->addChild(trans);
        root->addChild(cube);
        
        if (root->getNumChildren() != 2) {
            runner.endTest(false, "Scene graph has wrong number of children");
            root->unref();
            return 1;
        }
        
        if (root->getChild(0) != trans) {
            runner.endTest(false, "First child is not the translation node");
            root->unref();
            return 1;
        }
        
        runner.endTest(true);
        root->unref();
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    // Test 3: Node field access
    runner.startTest("Node field access");
    try {
        SoCube* cube = new SoCube;
        cube->ref();
        
        // Test default values
        if (cube->width.getValue() != 2.0f) {
            runner.endTest(false, "Default cube width is not 2.0");
            cube->unref();
            return 1;
        }
        
        // Test setting values
        cube->width.setValue(5.0f);
        cube->height.setValue(3.0f);
        cube->depth.setValue(4.0f);
        
        if (cube->width.getValue() != 5.0f ||
            cube->height.getValue() != 3.0f ||
            cube->depth.getValue() != 4.0f) {
            runner.endTest(false, "Field values not set correctly");
            cube->unref();
            return 1;
        }
        
        runner.endTest(true);
        cube->unref();
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    // Test 4: Camera node functionality
    runner.startTest("Camera node functionality");
    try {
        SoPerspectiveCamera* pcam = new SoPerspectiveCamera;
        pcam->ref();
        
        SoOrthographicCamera* ocam = new SoOrthographicCamera;
        ocam->ref();
        
        // Test basic field access
        pcam->position.setValue(0, 0, 5);
        pcam->orientation.setValue(SbVec3f(0, 1, 0), 0.0f);
        pcam->nearDistance.setValue(1.0f);
        pcam->farDistance.setValue(100.0f);
        
        SbVec3f pos = pcam->position.getValue();
        if (pos[2] != 5.0f) {
            runner.endTest(false, "Camera position not set correctly");
            pcam->unref();
            ocam->unref();
            return 1;
        }
        
        runner.endTest(true);
        pcam->unref();
        ocam->unref();
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    // Test 5: Material node functionality  
    runner.startTest("Material node functionality");
    try {
        SoMaterial* mat = new SoMaterial;
        mat->ref();
        
        // Test setting material properties - note these are multi-value fields
        mat->diffuseColor.setValue(SbColor(1.0f, 0.0f, 0.0f)); // Red
        mat->transparency.setValue(0.5f);
        
        if (mat->diffuseColor.getNum() < 1) {
            runner.endTest(false, "Material diffuse color not set - no values");
            mat->unref();
            return 1;
        }
        
        SbColor diffuse = mat->diffuseColor[0];
        if (diffuse[0] != 1.0f || diffuse[1] != 0.0f || diffuse[2] != 0.0f) {
            runner.endTest(false, "Material diffuse color not set correctly");
            mat->unref();
            return 1;
        }
        
        if (mat->transparency.getNum() < 1 || mat->transparency[0] != 0.5f) {
            runner.endTest(false, "Material transparency not set correctly");
            mat->unref();
            return 1;
        }
        
        runner.endTest(true);
        mat->unref();
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    return runner.getSummary();
}