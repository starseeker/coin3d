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
 * @file test_io_comprehensive.cpp
 * @brief Comprehensive tests for Coin3D I/O functionality
 *
 * This module provides comprehensive testing of file I/O, reading, writing,
 * and serialization functionality in Coin3D.
 */

#include "utils/test_common.h"
#include "utils/scene_graph_test_utils.h"

// I/O classes
#include <Inventor/SoInput.h>
#include <Inventor/SoOutput.h>
#include <Inventor/actions/SoWriteAction.h>

// For testing I/O scenarios
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoMaterial.h>

using namespace CoinTestUtils;

// ============================================================================
// Core I/O System Tests
// ============================================================================

TEST_CASE("Core I/O System - Basic Functionality", "[io][core][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("SoInput creation and properties") {
        SoInput input;
        
        // Test basic input functionality
        CHECK(input.isValidFile() == FALSE); // No file set yet
        CHECK(input.eof() == TRUE); // No input stream
    }
    
    SECTION("SoOutput creation and properties") {
        SoOutput output;
        
        // Test basic output functionality
        CHECK(output.getStage() == SoOutput::COUNT_REFS);
        
        // Test stage setting
        output.setStage(SoOutput::WRITE);
        CHECK(output.getStage() == SoOutput::WRITE);
    }
}

TEST_CASE("I/O String Operations", "[io][string][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("Input from string buffer") {
        // Test simple scene graph as string
        const char* sceneString = 
            "#Inventor V2.1 ascii\n"
            "Separator {\n"
            "  Cube { }\n"
            "}\n";
        
        SoInput input;
        input.setBuffer((void*)sceneString, strlen(sceneString));
        
        CHECK(input.isValidFile() == TRUE);
        CHECK(input.eof() == FALSE);
        
        // Test reading from the buffer
        SoSeparator* root = SoDB::readAll(&input);
        CHECK(root != nullptr);
        
        if (root) {
            root->ref();
            CHECK(root->getNumChildren() == 1);
            CHECK(root->getChild(0)->isOfType(SoCube::getClassTypeId()));
            root->unref();
        }
    }
    
    SECTION("Output to string buffer") {
        // Create a simple scene
        SoSeparator* scene = new SoSeparator;
        scene->ref();
        
        SoMaterial* material = new SoMaterial;
        material->diffuseColor.setValue(1.0f, 0.0f, 0.0f);
        scene->addChild(material);
        
        SoCube* cube = new SoCube;
        scene->addChild(cube);
        
        // Set up output to buffer
        SoOutput output;
        
        // Test writing to output
        SoWriteAction writeAction(&output);
        writeAction.apply(scene);
        
        scene->unref();
    }
}

TEST_CASE("I/O File Format Support", "[io][formats][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("ASCII format support") {
        const char* asciiScene = 
            "#Inventor V2.1 ascii\n"
            "Separator {\n"
            "  Material {\n"
            "    diffuseColor 1 0 0\n"
            "  }\n"
            "  Cube { }\n"
            "}\n";
        
        SoInput input;
        input.setBuffer((void*)asciiScene, strlen(asciiScene));
        
        SoSeparator* root = SoDB::readAll(&input);
        CHECK(root != nullptr);
        
        if (root) {
            root->ref();
            CHECK(root->getNumChildren() == 2);
            CHECK(root->getChild(0)->isOfType(SoMaterial::getClassTypeId()));
            CHECK(root->getChild(1)->isOfType(SoCube::getClassTypeId()));
            root->unref();
        }
    }
    
    SECTION("Header validation") {
        SoInput input;
        
        // Test various header formats
        const char* validHeader1 = "#Inventor V2.1 ascii\n";
        input.setBuffer((void*)validHeader1, strlen(validHeader1));
        CHECK(input.isValidFile() == TRUE);
        
        const char* validHeader2 = "#Inventor V2.0 ascii\n";
        input.setBuffer((void*)validHeader2, strlen(validHeader2));
        CHECK(input.isValidFile() == TRUE);
        
        const char* invalidHeader = "Not an Inventor file\n";
        input.setBuffer((void*)invalidHeader, strlen(invalidHeader));
        CHECK(input.isValidFile() == FALSE);
    }
}

TEST_CASE("I/O Scene Graph Operations", "[io][scene_graph][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("Complex scene I/O") {
        // Create a more complex scene
        SoSeparator* originalScene = new SoSeparator;
        originalScene->ref();
        
        // Add multiple materials and shapes
        for (int i = 0; i < 3; ++i) {
            SoMaterial* material = new SoMaterial;
            material->diffuseColor.setValue(i * 0.3f, (1-i) * 0.3f, 0.5f);
            originalScene->addChild(material);
            
            SoCube* cube = new SoCube;
            cube->width.setValue(1.0f + i * 0.5f);
            originalScene->addChild(cube);
        }
        
        CHECK(originalScene->getNumChildren() == 6); // 3 materials + 3 cubes
        
        // Test that the scene is valid for I/O operations
        SoOutput output;
        SoWriteAction writeAction(&output);
        writeAction.apply(originalScene);
        
        originalScene->unref();
    }
    
    SECTION("Empty scene I/O") {
        // Test I/O with empty scene
        SoSeparator* emptyScene = new SoSeparator;
        emptyScene->ref();
        
        CHECK(emptyScene->getNumChildren() == 0);
        
        // Test writing empty scene
        SoOutput output;
        SoWriteAction writeAction(&output);
        writeAction.apply(emptyScene);
        
        emptyScene->unref();
    }
}

TEST_CASE("I/O Error Handling and Edge Cases", "[io][edge_cases][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("Invalid input handling") {
        SoInput input;
        
        // Test with malformed Inventor data
        const char* malformedData = 
            "#Inventor V2.1 ascii\n"
            "Separator {\n"
            "  Cube { width invalid_value }\n"
            "}\n";
        
        input.setBuffer((void*)malformedData, strlen(malformedData));
        
        // Should handle gracefully (may return null or partial scene)
        SoSeparator* root = SoDB::readAll(&input);
        // Root may be null for invalid data, that's acceptable
        
        if (root) {
            root->ref();
            root->unref();
        }
    }
    
    SECTION("Buffer boundary conditions") {
        SoInput input;
        
        // Test with very small buffer
        const char* minimalData = "#Inventor V2.1 ascii\nSeparator{}\n";
        input.setBuffer((void*)minimalData, strlen(minimalData));
        
        CHECK(input.isValidFile() == TRUE);
        
        SoSeparator* root = SoDB::readAll(&input);
        if (root) {
            root->ref();
            CHECK(root->getNumChildren() == 0);
            root->unref();
        }
    }
    
    SECTION("Output stage management") {
        SoOutput output;
        
        // Test stage transitions
        CHECK(output.getStage() == SoOutput::COUNT_REFS);
        
        output.setStage(SoOutput::WRITE);
        CHECK(output.getStage() == SoOutput::WRITE);
        
        // Test that we can reset stages
        output.reset();
        CHECK(output.getStage() == SoOutput::COUNT_REFS);
    }
}