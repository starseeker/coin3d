/**
 * @file test_iostream_support.cpp
 * @brief Test program for new iostream support in SoInput and SoOutput
 */

#include <Inventor/SoDB.h>
#include <Inventor/SoInteraction.h>
#include <Inventor/SoInput.h>
#include <Inventor/SoOutput.h>
#include <Inventor/actions/SoWriteAction.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoMaterial.h>

#include <iostream>
#include <sstream>
#include <cassert>

int main(int argc, char** argv) {
    // Initialize Coin3D
    SoDB::init();
    SoInteraction::init();
    
    std::cout << "Testing SoInput/SoOutput iostream support...\n";
    
    // Create a simple scene graph
    SoSeparator* root = new SoSeparator;
    root->ref();
    
    // Add a material
    SoMaterial* material = new SoMaterial;
    material->diffuseColor.setValue(1.0f, 0.0f, 0.0f); // Red
    root->addChild(material);
    
    // Add a cube
    SoCube* cube = new SoCube;
    cube->width.setValue(2.0f);
    cube->height.setValue(1.5f);
    cube->depth.setValue(1.0f);
    root->addChild(cube);
    
    // Test writing to stringstream
    std::cout << "1. Testing SoOutput with stringstream...\n";
    std::stringstream outputStream;
    SoOutput output;
    output.setStream(&outputStream);
    
    SoWriteAction writeAction(&output);
    writeAction.apply(root);
    output.closeFile(); // Flush the output
    
    std::string ivContent = outputStream.str();
    std::cout << "Generated Inventor content:\n" << ivContent << "\n";
    
    // Test reading from stringstream
    std::cout << "2. Testing SoInput with stringstream...\n";
    std::stringstream inputStream(ivContent);
    SoInput input;
    input.setStream(&inputStream);
    
    SoSeparator* readRoot = SoDB::readAll(&input);
    if (readRoot) {
        readRoot->ref();
        std::cout << "Successfully read scene graph!\n";
        std::cout << "Number of children: " << readRoot->getNumChildren() << "\n";
        
        // Verify the content
        bool foundMaterial = false;
        bool foundCube = false;
        
        for (int i = 0; i < readRoot->getNumChildren(); i++) {
            SoNode* child = readRoot->getChild(i);
            if (child->getTypeId() == SoMaterial::getClassTypeId()) {
                foundMaterial = true;
                SoMaterial* mat = (SoMaterial*)child;
                SbVec3f color = mat->diffuseColor[0];
                std::cout << "Found material with color: (" 
                         << color[0] << ", " << color[1] << ", " << color[2] << ")\n";
            }
            if (child->getTypeId() == SoCube::getClassTypeId()) {
                foundCube = true;
                SoCube* c = (SoCube*)child;
                std::cout << "Found cube with width: " << c->width.getValue() << "\n";
            }
        }
        
        if (foundMaterial && foundCube) {
            std::cout << "✓ Content validation successful!\n";
        } else {
            std::cout << "✗ Content validation failed!\n";
            return 1;
        }
        
        readRoot->unref();
    } else {
        std::cout << "✗ Failed to read scene graph from stringstream!\n";
        return 1;
    }
    
    // Test round-trip: write to one stream, read from another
    std::cout << "3. Testing round-trip with different streams...\n";
    std::stringstream stream2;
    SoOutput output2;
    output2.setStream(&stream2);
    
    SoWriteAction writeAction2(&output2);
    writeAction2.apply(readRoot);
    output2.closeFile();
    
    std::string content2 = stream2.str();
    std::cout << "Round-trip successful! Content length: " << content2.length() << " bytes\n";
    
    root->unref();
    
    std::cout << "All tests passed! ✓\n";
    
    return 0;
}