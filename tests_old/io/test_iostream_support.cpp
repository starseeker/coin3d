/**
 * @file test_iostream_simple.cpp
 * @brief Simple test for iostream support
 */

#include "utils/test_common.h"
#include <Inventor/SoInput.h>
#include <Inventor/SoOutput.h>
#include <Inventor/actions/SoWriteAction.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <iostream>
#include <sstream>

using namespace CoinTestUtils;

TEST_CASE("SoInput stringstream support", "[io][stringstream]") {
    CoinTestFixture fixture;
    
    // Test data: simple cube in Inventor format
    const char* sceneString = 
        "#Inventor V2.1 ascii\n"
        "Separator {\n"
        "  Cube { width 2.0 }\n"
        "}\n";
    
    // Test reading from stringstream
    std::stringstream ss(sceneString);
    SoInput input;
    input.setStream(&ss);
    
    CHECK(input.isValidBuffer() == TRUE);
    
    SoSeparator* root = SoDB::readAll(&input);
    CHECK(root != nullptr);
    
    if (root) {
        root->ref();
        CHECK(root->getNumChildren() == 1);
        
        SoNode* child = root->getChild(0);
        CHECK(child->getTypeId() == SoCube::getClassTypeId());
        
        root->unref();
    }
}

TEST_CASE("SoOutput stringstream support", "[io][stringstream]") {
    CoinTestFixture fixture;
    
    // Create a simple scene
    SoSeparator* root = new SoSeparator;
    root->ref();
    
    SoCube* cube = new SoCube;
    cube->width.setValue(3.0f);
    root->addChild(cube);
    
    // Write to stringstream
    std::stringstream ss;
    SoOutput output;
    output.setStream(&ss);
    
    SoWriteAction writeAction(&output);
    writeAction.apply(root);
    output.closeFile();
    
    std::string result = ss.str();
    CHECK(result.length() > 0);
    CHECK(result.find("Cube") != std::string::npos);
    CHECK(result.find("width") != std::string::npos);
    
    root->unref();
}

TEST_CASE("Round-trip stringstream test", "[io][stringstream][roundtrip]") {
    CoinTestFixture fixture;
    
    // Create original scene
    SoSeparator* originalRoot = new SoSeparator;
    originalRoot->ref();
    
    SoCube* originalCube = new SoCube;
    originalCube->width.setValue(4.5f);
    originalCube->height.setValue(2.5f);
    originalRoot->addChild(originalCube);
    
    // Write to stringstream
    std::stringstream ss;
    SoOutput output;
    output.setStream(&ss);
    
    SoWriteAction writeAction(&output);
    writeAction.apply(originalRoot);
    output.closeFile();
    
    std::string ivContent = ss.str();
    CHECK(ivContent.length() > 0);
    
    // Read back from stringstream
    std::stringstream inputStream(ivContent);
    SoInput input;
    input.setStream(&inputStream);
    
    SoSeparator* readRoot = SoDB::readAll(&input);
    CHECK(readRoot != nullptr);
    
    if (readRoot) {
        readRoot->ref();
        CHECK(readRoot->getNumChildren() == 1);
        
        SoNode* child = readRoot->getChild(0);
        CHECK(child->getTypeId() == SoCube::getClassTypeId());
        
        SoCube* readCube = (SoCube*)child;
        CHECK(readCube->width.getValue() == 4.5f);
        CHECK(readCube->height.getValue() == 2.5f);
        
        readRoot->unref();
    }
    
    originalRoot->unref();
}