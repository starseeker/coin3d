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
 * @file test_actions.cpp
 * @brief Simple tests for Coin3D actions API
 *
 * Tests basic functionality of action classes without external frameworks.
 * Returns 0 for success, non-zero for failure.
 */

#include "test_utils.h"

// Core action classes
#include <Inventor/actions/SoAction.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/actions/SoCallbackAction.h>
#include <Inventor/actions/SoPickAction.h>
#include <Inventor/actions/SoRayPickAction.h>
#include <Inventor/actions/SoWriteAction.h>

// Nodes for testing
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoTranslation.h>

// Other includes
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoOutput.h>

using namespace SimpleTest;

int main() {
    TestFixture fixture;
    TestRunner runner;
    
    // Test 1: Basic action type checking
    runner.startTest("Action type checking");
    try {
        SoSearchAction search;
        SoGetBoundingBoxAction bbox(SbViewportRegion(100, 100));
        SoCallbackAction callback;
        
        if (search.getTypeId() == SoType::badType()) {
            runner.endTest(false, "SoSearchAction has bad type");
            return 1;
        }
        
        if (!search.isOfType(SoAction::getClassTypeId())) {
            runner.endTest(false, "SoSearchAction is not an action");
            return 1;
        }
        
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    // Test 2: Basic scene traversal
    runner.startTest("Basic scene traversal");
    try {
        SoSeparator* scene = new SoSeparator;
        scene->ref();
        
        SoTranslation* trans = new SoTranslation;
        trans->translation.setValue(1.0f, 2.0f, 3.0f);
        SoCube* cube = new SoCube;
        
        scene->addChild(trans);
        scene->addChild(cube);
        
        SoSearchAction search;
        search.apply(scene);
        
        // Action should complete without crashing
        runner.endTest(true);
        
        scene->unref();
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    // Test 3: Bounding box computation
    runner.startTest("Bounding box computation");
    try {
        SoSeparator* scene = new SoSeparator;
        scene->ref();
        
        SoCube* cube = new SoCube;
        cube->width.setValue(2.0f);
        cube->height.setValue(2.0f);
        cube->depth.setValue(2.0f);
        scene->addChild(cube);
        
        SoGetBoundingBoxAction bbox_action(SbViewportRegion(100, 100));
        bbox_action.apply(scene);
        
        SbBox3f bbox = bbox_action.getBoundingBox();
        if (bbox.isEmpty()) {
            runner.endTest(false, "Bounding box is empty for cube");
            scene->unref();
            return 1;
        }
        
        runner.endTest(true);
        scene->unref();
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    // Test 4: Search action functionality
    runner.startTest("Search action functionality");
    try {
        SoSeparator* scene = new SoSeparator;
        scene->ref();
        
        SoCube* cube = new SoCube;
        cube->setName("TestCube");
        scene->addChild(cube);
        
        SoSearchAction search;
        search.setName(SbName("TestCube"));
        search.apply(scene);
        
        if (search.getPath() == NULL) {
            runner.endTest(false, "Search failed to find named cube");
            scene->unref();
            return 1;
        }
        
        runner.endTest(true);
        scene->unref();
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    // Test 5: Write action basic functionality
    runner.startTest("Write action basic functionality");
    try {
        SoSeparator* scene = new SoSeparator;
        scene->ref();
        
        SoCube* cube = new SoCube;
        scene->addChild(cube);
        
        SoOutput output;
        // Try to set up output to a buffer instead of file
        output.setBuffer(NULL, 0, NULL);
        
        SoWriteAction write_action(&output);
        write_action.apply(scene);
        
        // If we get here without crashing, consider it a pass
        runner.endTest(true);
        
        scene->unref();
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    return runner.getSummary();
}