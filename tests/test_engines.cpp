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
 * @file test_engines.cpp
 * @brief Simple tests for Coin3D engines API
 *
 * Tests basic functionality of engine classes without external frameworks.
 * Returns 0 for success, non-zero for failure.
 */

#include "test_utils.h"

// Core engine classes
#include <Inventor/engines/SoEngine.h>
#include <Inventor/engines/SoCalculator.h>
#include <Inventor/engines/SoCompose.h>
#include <Inventor/engines/SoDecomposeVec3f.h>
#include <Inventor/engines/SoElapsedTime.h>
#include <Inventor/engines/SoInterpolate.h>

using namespace SimpleTest;

int main() {
    TestFixture fixture;
    TestRunner runner;
    
    // Test 1: Basic engine type checking
    runner.startTest("Basic engine type checking");
    try {
        SoCalculator* calc = new SoCalculator;
        calc->ref();
        
        SoElapsedTime* timer = new SoElapsedTime;
        timer->ref();
        
        if (calc->getTypeId() == SoType::badType()) {
            runner.endTest(false, "SoCalculator has bad type");
            calc->unref();
            timer->unref();
            return 1;
        }
        
        if (timer->getTypeId() == SoType::badType()) {
            runner.endTest(false, "SoElapsedTime has bad type");
            calc->unref();
            timer->unref();
            return 1;
        }
        
        // Check inheritance
        if (!calc->isOfType(SoEngine::getClassTypeId())) {
            runner.endTest(false, "SoCalculator is not an SoEngine");
            calc->unref();
            timer->unref();
            return 1;
        }
        
        calc->unref();
        timer->unref();
        
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    // Test 2: Calculator engine basic setup
    runner.startTest("Calculator engine basic setup");
    try {
        SoCalculator* calc = new SoCalculator;
        calc->ref();
        
        // Test setting a simple expression - note this is a multi-field
        calc->expression.setValue("oa = a + b");
        calc->a.setValue(5.0f);
        calc->b.setValue(3.0f);
        
        // The calculator should be evaluatable
        if (calc->expression.getNum() == 0) {
            runner.endTest(false, "Calculator expression not set - no values");
            calc->unref();
            return 1;
        }
        
        SbString expr = calc->expression[0];
        if (expr != "oa = a + b") {
            runner.endTest(false, "Calculator expression not set correctly");
            calc->unref();
            return 1;
        }
        
        calc->unref();
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    // Test 3: Elapsed time engine
    runner.startTest("Elapsed time engine");
    try {
        SoElapsedTime* timer = new SoElapsedTime;
        timer->ref();
        
        // Test basic properties
        timer->speed.setValue(1.0f);
        if (timer->speed.getValue() != 1.0f) {
            runner.endTest(false, "Timer speed not set correctly");
            timer->unref();
            return 1;
        }
        
        // Test pause functionality
        timer->pause.setValue(TRUE);
        if (timer->pause.getValue() != TRUE) {
            runner.endTest(false, "Timer pause not set correctly");
            timer->unref();
            return 1;
        }
        
        timer->unref();
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    // Test 4: Decompose vector engine
    runner.startTest("Decompose vector engine");
    try {
        SoDecomposeVec3f* decomp = new SoDecomposeVec3f;
        decomp->ref();
        
        // Test setting input vector - this is also multi-field
        decomp->vector.setValue(SbVec3f(1.0f, 2.0f, 3.0f));
        
        if (decomp->vector.getNum() == 0) {
            runner.endTest(false, "Decompose vector input not set - no values");
            decomp->unref();
            return 1;
        }
        
        SbVec3f inputVec = decomp->vector[0];
        if (inputVec[0] != 1.0f || inputVec[1] != 2.0f || inputVec[2] != 3.0f) {
            runner.endTest(false, "Decompose vector input not set correctly");
            decomp->unref();
            return 1;
        }
        
        decomp->unref();
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
        return 1;
    }
    
    return runner.getSummary();
}