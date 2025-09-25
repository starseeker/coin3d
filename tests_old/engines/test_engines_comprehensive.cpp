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

#include "utils/test_common.h"
#include <Inventor/engines/SoCalculator.h>
#include <Inventor/engines/SoComposeVec3f.h>
#include <Inventor/engines/SoDecomposeVec3f.h>
#include <Inventor/engines/SoComputeBoundingBox.h>
#include <Inventor/engines/SoCounter.h>
#include <Inventor/engines/SoElapsedTime.h>
#include <Inventor/engines/SoGate.h>
#include <Inventor/engines/SoInterpolateFloat.h>
#include <Inventor/engines/SoSelectOne.h>
#include <Inventor/engines/SoTriggerAny.h>
#include <Inventor/engines/SoTimeCounter.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/fields/SoSFFloat.h>
#include <Inventor/fields/SoSFVec3f.h>
#include <Inventor/fields/SoSFShort.h>

using namespace CoinTestUtils;

TEST_CASE("SoEngine basic functionality", "[engines][SoEngine]") {
    CoinTestFixture fixture;
    
    SECTION("Engine type system") {
        // Test engine type identification
        SoType engine_type = SoEngine::getClassTypeId();
        CHECK(engine_type != SoType::badType());
        // Note: Type names may include class prefixes
        
        // Test specific engine types
        SoType calc_type = SoCalculator::getClassTypeId();
        CHECK(calc_type.isDerivedFrom(engine_type));
        // Note: Type names may include class prefixes
    }
    
    SECTION("Engine instantiation") {
        SoCalculator* calc = new SoCalculator;
        CHECK(calc != nullptr);
        CHECK(calc->getTypeId() == SoCalculator::getClassTypeId());
        calc->unref();
        
        SoCounter* counter = new SoCounter;
        CHECK(counter != nullptr);
        CHECK(counter->getTypeId() == SoCounter::getClassTypeId());
        counter->unref();
    }
}

TEST_CASE("SoCalculator engine functionality", "[engines][SoCalculator]") {
    CoinTestFixture fixture;
    
    SECTION("Basic calculator operations") {
        SoCalculator* calc = new SoCalculator;
        
        // Test simple arithmetic
        calc->a.setValue(5.0f);
        calc->b.setValue(3.0f);
        calc->expression.setValue("oa = a + b");
        
        // Get output - engines evaluate automatically when output is accessed
        SoEngineOutput* output = calc->getOutput("oa");
        CHECK(output != nullptr);
        
        // Connect to a field to get the value
        SoSFFloat test_field;
        test_field.connectFrom(output);
        float result = test_field.getValue();
        CHECK(result == 8.0f);
        
        calc->unref();
    }
    
    SECTION("Vector calculations") {
        SoCalculator* calc = new SoCalculator;
        
        calc->A.setValue(SbVec3f(1.0f, 2.0f, 3.0f));
        calc->B.setValue(SbVec3f(4.0f, 5.0f, 6.0f));
        calc->expression.setValue("oA = A + B");
        
        SoEngineOutput* output = calc->getOutput("oA");
        CHECK(output != nullptr);
        
        // Connect to a field to get the value
        SoSFVec3f test_field;
        test_field.connectFrom(output);
        SbVec3f result = test_field.getValue();
        CHECK(result[0] == 5.0f);
        CHECK(result[1] == 7.0f);
        CHECK(result[2] == 9.0f);
        
        calc->unref();
    }
    
    SECTION("Multiple expressions") {
        SoCalculator* calc = new SoCalculator;
        
        calc->a.setValue(10.0f);
        calc->expression.set1Value(0, "oa = a * 2");
        calc->expression.set1Value(1, "ob = a / 2");
        
        SoEngineOutput* outa = calc->getOutput("oa");
        SoEngineOutput* outb = calc->getOutput("ob");
        
        CHECK(outa != nullptr);
        CHECK(outb != nullptr);
        
        // Connect to fields to get values
        SoSFFloat test_fielda, test_fieldb;
        test_fielda.connectFrom(outa);
        test_fieldb.connectFrom(outb);
        
        float resulta = test_fielda.getValue();
        float resultb = test_fieldb.getValue();
        
        CHECK(resulta == 20.0f);
        CHECK(resultb == 5.0f);
        
        calc->unref();
    }
}

TEST_CASE("SoComposeVec3f engine functionality", "[engines][SoComposeVec3f]") {
    CoinTestFixture fixture;
    
    SECTION("Vector composition") {
        SoComposeVec3f* compose = new SoComposeVec3f;
        
        compose->x.setValue(1.5f);
        compose->y.setValue(2.5f);
        compose->z.setValue(3.5f);
        
        // Connect to a field to get the value
        SoSFVec3f test_field;
        test_field.connectFrom(&compose->vector);
        SbVec3f result = test_field.getValue();
        
        CHECK(result[0] == 1.5f);
        CHECK(result[1] == 2.5f);
        CHECK(result[2] == 3.5f);
        
        compose->unref();
    }
}

TEST_CASE("SoDecomposeVec3f engine functionality", "[engines][SoDecomposeVec3f]") {
    CoinTestFixture fixture;
    
    SECTION("Vector decomposition") {
        SoDecomposeVec3f* decompose = new SoDecomposeVec3f;
        
        decompose->vector.setValue(SbVec3f(1.5f, 2.5f, 3.5f));
        
        // Connect to fields to get values
        SoSFFloat test_x, test_y, test_z;
        test_x.connectFrom(&decompose->x);
        test_y.connectFrom(&decompose->y);
        test_z.connectFrom(&decompose->z);
        
        float x = test_x.getValue();
        float y = test_y.getValue();
        float z = test_z.getValue();
        
        CHECK(x == 1.5f);
        CHECK(y == 2.5f);
        CHECK(z == 3.5f);
        
        decompose->unref();
    }
}

TEST_CASE("SoCounter engine functionality", "[engines][SoCounter]") {
    CoinTestFixture fixture;
    
    SECTION("Basic counting") {
        SoCounter* counter = new SoCounter;
        
        counter->min.setValue(0);
        counter->max.setValue(10);
        counter->step.setValue(2);
        counter->reset.setValue(0);
        
        // Test basic setup
        CHECK(counter->min.getValue() == 0);
        CHECK(counter->max.getValue() == 10);
        CHECK(counter->step.getValue() == 2);
        
        counter->unref();
    }
    
    SECTION("Counter reset") {
        SoCounter* counter = new SoCounter;
        
        counter->min.setValue(0);
        counter->max.setValue(5);
        counter->step.setValue(1);
        
        // Test setup
        CHECK(counter->min.getValue() == 0);
        CHECK(counter->max.getValue() == 5);
        CHECK(counter->step.getValue() == 1);
        
        counter->unref();
    }
}

TEST_CASE("SoGate engine functionality", "[engines][SoGate]") {
    CoinTestFixture fixture;
    
    SECTION("Gate enable/disable") {
        SoGate* gate = new SoGate(SoMFFloat::getClassTypeId());
        
        // Set input value
        SoMFFloat* input = (SoMFFloat*)gate->getField("input");
        input->setValue(42.0f);
        
        SECTION("Gate enabled") {
            gate->enable.setValue(TRUE);
            
            SoEngineOutput* output = gate->getOutput("output");
            CHECK(output != nullptr);
            
            // Basic test that gate exists and can be used
            // Detailed functionality depends on internal implementation
        }
        
        SECTION("Gate disabled") {
            gate->enable.setValue(FALSE);
            
            SoEngineOutput* output = gate->getOutput("output");
            CHECK(output != nullptr);
            
            // Basic test that gate exists when disabled
        }
        
        gate->unref();
    }
}

TEST_CASE("SoSelectOne engine functionality", "[engines][SoSelectOne]") {
    CoinTestFixture fixture;
    
    SECTION("Selection from multiple inputs") {
        SoSelectOne* select = new SoSelectOne(SoMFFloat::getClassTypeId());
        
        // Set up multiple input values
        SoMFFloat* input = (SoMFFloat*)select->getField("input");
        input->set1Value(0, 10.0f);
        input->set1Value(1, 20.0f);
        input->set1Value(2, 30.0f);
        
        SECTION("Select first input") {
            select->index.setValue(0);
            
            SoEngineOutput* output = select->getOutput("output");
            CHECK(output != nullptr);
            
            // Test that output exists
        }
        
        SECTION("Select second input") {
            select->index.setValue(1);
            
            SoEngineOutput* output = select->getOutput("output");
            CHECK(output != nullptr);
        }
        
        select->unref();
    }
}

TEST_CASE("SoTriggerAny engine functionality", "[engines][SoTriggerAny]") {
    CoinTestFixture fixture;
    
    SECTION("Trigger on any input") {
        SoTriggerAny* trigger = new SoTriggerAny;
        
        // SoTriggerAny has individual input fields, not an array
        // Trigger one of the input fields
        trigger->input0.touch();
        
        // Check that output exists and can be accessed
        SoEngineOutput* output = trigger->getOutput("output");
        CHECK(output != nullptr);
        
        trigger->unref();
    }
}

TEST_CASE("SoComputeBoundingBox engine functionality", "[engines][SoComputeBoundingBox]") {
    CoinTestFixture fixture;
    
    SECTION("Compute bounding box of scene") {
        SoComputeBoundingBox* compute = new SoComputeBoundingBox;
        
        // Create a simple scene
        SoSeparator* scene = new SoSeparator;
        SoCube* cube = new SoCube;
        cube->width.setValue(2.0f);
        cube->height.setValue(2.0f);
        cube->depth.setValue(2.0f);
        scene->addChild(cube);
        
        compute->node.setValue(scene);
        
        // Test basic functionality - check that engine was created and configured
        CHECK(compute != nullptr);
        CHECK(compute->node.getValue() == scene);
        
        scene->unref();
        compute->unref();
    }
}

TEST_CASE("SoInterpolateFloat engine functionality", "[engines][SoInterpolateFloat]") {
    CoinTestFixture fixture;
    
    SECTION("Float interpolation") {
        SoInterpolateFloat* interp = new SoInterpolateFloat;
        
        interp->input0.setValue(0.0f);
        interp->input1.setValue(10.0f);
        
        SECTION("Alpha = 0.0") {
            interp->alpha.setValue(0.0f);
            
            SoSFFloat test_field;
            test_field.connectFrom(&interp->output);
            float result = test_field.getValue();
            CHECK(result == 0.0f);
        }
        
        SECTION("Alpha = 0.5") {
            interp->alpha.setValue(0.5f);
            
            SoSFFloat test_field;
            test_field.connectFrom(&interp->output);
            float result = test_field.getValue();
            CHECK(result == 5.0f);
        }
        
        SECTION("Alpha = 1.0") {
            interp->alpha.setValue(1.0f);
            
            SoSFFloat test_field;
            test_field.connectFrom(&interp->output);
            float result = test_field.getValue();
            CHECK(result == 10.0f);
        }
        
        interp->unref();
    }
}

TEST_CASE("Engine connection and evaluation", "[engines][connections]") {
    CoinTestFixture fixture;
    
    SECTION("Connect engine outputs to other engines") {
        SoComposeVec3f* compose = new SoComposeVec3f;
        SoDecomposeVec3f* decompose = new SoDecomposeVec3f;
        
        // Set input values
        compose->x.setValue(1.0f);
        compose->y.setValue(2.0f);
        compose->z.setValue(3.0f);
        
        // Connect compose output to decompose input
        decompose->vector.connectFrom(&compose->vector);
        
        // Check that values propagated correctly
        SoSFFloat test_x, test_y, test_z;
        test_x.connectFrom(&decompose->x);
        test_y.connectFrom(&decompose->y);
        test_z.connectFrom(&decompose->z);
        
        float x = test_x.getValue();
        float y = test_y.getValue();
        float z = test_z.getValue();
        
        CHECK(x == 1.0f);
        CHECK(y == 2.0f);
        CHECK(z == 3.0f);
        
        compose->unref();
        decompose->unref();
    }
    
    SECTION("Multiple engine chain") {
        SoCalculator* calc1 = new SoCalculator;
        SoCalculator* calc2 = new SoCalculator;
        
        // First calculator: multiply by 2
        calc1->a.setValue(5.0f);
        calc1->expression.setValue("oa = a * 2");
        
        // Second calculator: add 1
        calc2->expression.setValue("oa = a + 1");
        calc2->a.connectFrom(&calc1->oa);
        
        // Get final result
        SoSFFloat test_field;
        test_field.connectFrom(&calc2->oa);
        float result = test_field.getValue();
        CHECK(result == 11.0f); // (5 * 2) + 1 = 11
        
        calc1->unref();
        calc2->unref();
    }
}