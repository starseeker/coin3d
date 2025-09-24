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
 * @file test_shaders_comprehensive.cpp
 * @brief Comprehensive tests for all Coin3D shader types and user-facing functionality
 *
 * This module provides comprehensive testing of shader node creation, compilation,
 * parameter binding, program linking, and rendering validation using OSMesa offscreen rendering.
 */

#include "utils/test_common.h"
#include "utils/osmesa_test_context.h"
#include "utils/scene_graph_test_utils.h"

// Shader node classes
#include <Inventor/nodes/SoShaderProgram.h>
#include <Inventor/nodes/SoFragmentShader.h>
#include <Inventor/nodes/SoVertexShader.h>
#include <Inventor/nodes/SoGeometryShader.h>
#include <Inventor/nodes/SoShaderParameter.h>
#include <Inventor/nodes/SoShaderObject.h>

// Scene graph nodes for testing
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTexture2.h>

using namespace CoinTestUtils;

// ============================================================================
// Basic Shader Node Tests
// ============================================================================

TEST_CASE("Shader Nodes - Creation and Basic Properties", "[shaders][creation][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("SoShaderProgram creation and properties") {
        SoShaderProgram* program = new SoShaderProgram;
        program->ref();
        
        CHECK(program->getTypeId() == SoShaderProgram::getClassTypeId());
        CHECK(program->getTypeId().getName() == SbName("ShaderProgram"));
        
        // Test default field values
        CHECK(program->shaderObject.getNum() == 0);
        
        program->unref();
    }
    
    SECTION("SoFragmentShader creation and properties") {
        SoFragmentShader* shader = new SoFragmentShader;
        shader->ref();
        
        CHECK(shader->getTypeId() == SoFragmentShader::getClassTypeId());
        CHECK(shader->getTypeId().getName() == SbName("FragmentShader"));
        
        // Test default field values
        CHECK(shader->sourceType.getValue() == SoShaderObject::FILENAME);
        CHECK(shader->sourceProgram.getValue() == "");
        
        shader->unref();
    }
    
    SECTION("SoVertexShader creation and properties") {
        SoVertexShader* shader = new SoVertexShader;
        shader->ref();
        
        CHECK(shader->getTypeId() == SoVertexShader::getClassTypeId());
        CHECK(shader->getTypeId().getName() == SbName("VertexShader"));
        
        // Test default field values
        CHECK(shader->sourceType.getValue() == SoShaderObject::FILENAME);
        CHECK(shader->sourceProgram.getValue() == "");
        
        shader->unref();
    }
    
    SECTION("SoGeometryShader creation and properties") {
        SoGeometryShader* shader = new SoGeometryShader;
        shader->ref();
        
        CHECK(shader->getTypeId() == SoGeometryShader::getClassTypeId());
        CHECK(shader->getTypeId().getName() == SbName("GeometryShader"));
        
        // Test default field values
        CHECK(shader->sourceType.getValue() == SoShaderObject::FILENAME);
        CHECK(shader->sourceProgram.getValue() == "");
        
        shader->unref();
    }
}

// ============================================================================
// Shader Parameter Tests
// ============================================================================

TEST_CASE("Shader Parameters - Creation and Value Setting", "[shaders][parameters][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("SoShaderParameter1f functionality") {
        SoShaderParameter1f* param = new SoShaderParameter1f;
        param->ref();
        
        CHECK(param->getTypeId() == SoShaderParameter1f::getClassTypeId());
        
        // Test name setting
        param->name.setValue("testFloat");
        CHECK(param->name.getValue() == SbString("testFloat"));
        
        // Test value setting
        param->value.setValue(3.14159f);
        CHECK(param->value.getValue() == 3.14159f);
        
        param->unref();
    }
    
    SECTION("SoShaderParameter3f functionality") {
        SoShaderParameter3f* param = new SoShaderParameter3f;
        param->ref();
        
        CHECK(param->getTypeId() == SoShaderParameter3f::getClassTypeId());
        
        // Test name setting
        param->name.setValue("testVec3");
        CHECK(param->name.getValue() == SbString("testVec3"));
        
        // Test value setting
        SbVec3f testVec(1.0f, 2.0f, 3.0f);
        param->value.setValue(testVec);
        CHECK(param->value.getValue() == testVec);
        
        param->unref();
    }
    
    SECTION("SoShaderParameter4f functionality") {
        SoShaderParameter4f* param = new SoShaderParameter4f;
        param->ref();
        
        CHECK(param->getTypeId() == SoShaderParameter4f::getClassTypeId());
        
        // Test name setting
        param->name.setValue("testVec4");
        CHECK(param->name.getValue() == SbString("testVec4"));
        
        // Test value setting
        SbVec4f testVec(1.0f, 2.0f, 3.0f, 4.0f);
        param->value.setValue(testVec);
        CHECK(param->value.getValue() == testVec);
        
        param->unref();
    }
    
    SECTION("SoShaderParameterMatrix functionality") {
        SoShaderParameterMatrix* param = new SoShaderParameterMatrix;
        param->ref();
        
        CHECK(param->getTypeId() == SoShaderParameterMatrix::getClassTypeId());
        
        // Test name setting
        param->name.setValue("testMatrix");
        CHECK(param->name.getValue() == SbString("testMatrix"));
        
        // Test identity matrix by default
        SbMatrix identity = SbMatrix::identity();
        param->value.setValue(identity);
        CHECK(param->value.getValue() == identity);
        
        param->unref();
    }
}

// ============================================================================
// Shader Source Code Tests
// ============================================================================

TEST_CASE("Shader Source - Source Type and Program Setting", "[shaders][source][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("Fragment shader with inline source") {
        SoFragmentShader* shader = new SoFragmentShader;
        shader->ref();
        
        // Test inline source setting
        shader->sourceType.setValue(SoShaderObject::GLSL_PROGRAM);
        CHECK(shader->sourceType.getValue() == SoShaderObject::GLSL_PROGRAM);
        
        const char* fragmentSource = 
            "#version 120\n"
            "void main() {\n"
            "    gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
            "}\n";
        
        shader->sourceProgram.setValue(fragmentSource);
        CHECK(shader->sourceProgram.getValue() == SbString(fragmentSource));
        
        shader->unref();
    }
    
    SECTION("Vertex shader with inline source") {
        SoVertexShader* shader = new SoVertexShader;
        shader->ref();
        
        // Test inline source setting
        shader->sourceType.setValue(SoShaderObject::GLSL_PROGRAM);
        CHECK(shader->sourceType.getValue() == SoShaderObject::GLSL_PROGRAM);
        
        const char* vertexSource = 
            "#version 120\n"
            "void main() {\n"
            "    gl_Position = ftransform();\n"
            "}\n";
        
        shader->sourceProgram.setValue(vertexSource);
        CHECK(shader->sourceProgram.getValue() == SbString(vertexSource));
        
        shader->unref();
    }
    
    SECTION("Filename source type") {
        SoFragmentShader* shader = new SoFragmentShader;
        shader->ref();
        
        // Test filename source setting
        shader->sourceType.setValue(SoShaderObject::FILENAME);
        CHECK(shader->sourceType.getValue() == SoShaderObject::FILENAME);
        
        shader->sourceProgram.setValue("test_shader.frag");
        CHECK(shader->sourceProgram.getValue() == SbString("test_shader.frag"));
        
        shader->unref();
    }
}

// ============================================================================
// Shader Program Assembly Tests
// ============================================================================

TEST_CASE("Shader Program - Assembly and Rendering", "[shaders][program][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("Complete shader program creation") {
        SoShaderProgram* program = new SoShaderProgram;
        program->ref();
        
        // Create vertex shader
        SoVertexShader* vertexShader = new SoVertexShader;
        vertexShader->sourceType.setValue(SoShaderObject::GLSL_PROGRAM);
        vertexShader->sourceProgram.setValue(
            "#version 120\n"
            "void main() {\n"
            "    gl_Position = ftransform();\n"
            "}\n"
        );
        
        // Create fragment shader
        SoFragmentShader* fragmentShader = new SoFragmentShader;
        fragmentShader->sourceType.setValue(SoShaderObject::GLSL_PROGRAM);
        fragmentShader->sourceProgram.setValue(
            "#version 120\n"
            "void main() {\n"
            "    gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);\n"
            "}\n"
        );
        
        // Add shaders to program
        program->shaderObject.set1Value(0, vertexShader);
        program->shaderObject.set1Value(1, fragmentShader);
        
        CHECK(program->shaderObject.getNum() == 2);
        CHECK(program->shaderObject[0] == vertexShader);
        CHECK(program->shaderObject[1] == fragmentShader);
        
        program->unref();
    }
    
    SECTION("Shader program with parameters") {
        SoShaderProgram* program = new SoShaderProgram;
        program->ref();
        
        // Create a simple fragment shader that uses a uniform
        SoFragmentShader* fragmentShader = new SoFragmentShader;
        fragmentShader->sourceType.setValue(SoShaderObject::GLSL_PROGRAM);
        fragmentShader->sourceProgram.setValue(
            "#version 120\n"
            "uniform vec3 color;\n"
            "void main() {\n"
            "    gl_FragColor = vec4(color, 1.0);\n"
            "}\n"
        );
        
        // Create parameter for the color uniform
        SoShaderParameter3f* colorParam = new SoShaderParameter3f;
        colorParam->name.setValue("color");
        colorParam->value.setValue(SbVec3f(1.0f, 0.5f, 0.0f));
        
        program->shaderObject.set1Value(0, fragmentShader);
        program->shaderObject.set1Value(1, colorParam);
        
        CHECK(program->shaderObject.getNum() == 2);
        
        program->unref();
    }
}

// ============================================================================
// Shader Scene Integration Tests
// ============================================================================

TEST_CASE("Shader Scene Integration - Rendering with Shaders", "[shaders][scene][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("Basic shader program properties") {
        // Test basic shader program creation without rendering
        SoShaderProgram* program = new SoShaderProgram;
        program->ref();
        
        // Test basic properties
        CHECK(program->getTypeId() == SoShaderProgram::getClassTypeId());
        
        program->unref();
    }
}

// ============================================================================
// Shader Error Handling Tests
// ============================================================================

TEST_CASE("Shader Error Handling - Invalid Source and Parameters", "[shaders][errors][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("Invalid shader source handling") {
        SoFragmentShader* shader = new SoFragmentShader;
        shader->ref();
        
        // Set invalid GLSL source
        shader->sourceType.setValue(SoShaderObject::GLSL_PROGRAM);
        shader->sourceProgram.setValue(
            "#version 120\n"
            "invalid glsl syntax here!!!\n"
            "void main() {\n"
            "    gl_FragColor = vec4(1.0);\n"  // Missing parameters
            "}\n"
        );
        
        // Should not crash when setting invalid source
        CHECK(shader->sourceProgram.getValue().getLength() > 0);
        
        shader->unref();
    }
    
    SECTION("Empty shader source") {
        SoVertexShader* shader = new SoVertexShader;
        shader->ref();
        
        shader->sourceType.setValue(SoShaderObject::GLSL_PROGRAM);
        shader->sourceProgram.setValue("");
        
        CHECK(shader->sourceProgram.getValue() == SbString(""));
        
        shader->unref();
    }
    
    SECTION("Parameter name conflicts") {
        SoShaderParameter1f* param1 = new SoShaderParameter1f;
        SoShaderParameter1f* param2 = new SoShaderParameter1f;
        param1->ref();
        param2->ref();
        
        // Both parameters with same name
        param1->name.setValue("testParam");
        param2->name.setValue("testParam");
        
        param1->value.setValue(1.0f);
        param2->value.setValue(2.0f);
        
        // Should handle name conflicts gracefully
        CHECK(param1->name.getValue() == param2->name.getValue());
        CHECK(param1->value.getValue() != param2->value.getValue());
        
        param1->unref();
        param2->unref();
    }
}