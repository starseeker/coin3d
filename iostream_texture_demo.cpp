/**
 * @file iostream_texture_demo.cpp
 * @brief Demonstration of new iostream and texture memory features in Coin3D
 */

#include <iostream>
#include <sstream>
#include <cstring>

// This would be the usage example for developers:

/*
// Example 1: Using iostream support for scene I/O
#include <Inventor/SoDB.h>
#include <Inventor/SoInput.h>
#include <Inventor/SoOutput.h>
#include <Inventor/actions/SoWriteAction.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <iostream>
#include <sstream>

void iostream_demo() {
    // Initialize Coin3D with proper context manager
    // SoDB::init(&context_manager);
    
    // Create a simple scene
    SoSeparator* root = new SoSeparator;
    root->ref();
    
    SoCube* cube = new SoCube;
    cube->width.setValue(2.0f);
    root->addChild(cube);
    
    // NEW: Write scene to stringstream using iostream interface
    std::stringstream sceneStream;
    SoOutput output;
    output.setStream(&sceneStream);  // <-- New iostream support!
    
    SoWriteAction writeAction(&output);
    writeAction.apply(root);
    output.closeFile();
    
    std::string ivContent = sceneStream.str();
    std::cout << "Generated Inventor scene:\n" << ivContent << std::endl;
    
    // NEW: Read scene from stringstream using iostream interface
    std::stringstream inputStream(ivContent);
    SoInput input;
    input.setStream(&inputStream);  // <-- New iostream support!
    
    SoSeparator* readScene = SoDB::readAll(&input);
    if (readScene) {
        readScene->ref();
        std::cout << "Successfully read scene from stringstream!" << std::endl;
        readScene->unref();
    }
    
    root->unref();
}

// Example 2: Using improved texture memory management
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/nodes/SoTexture3.h>

void texture_memory_demo() {
    // Create texture data in memory
    const int width = 256, height = 256, components = 3;
    unsigned char* textureData = new unsigned char[width * height * components];
    
    // Fill with a simple pattern
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int index = (y * width + x) * components;
            textureData[index] = (x * 255) / width;     // Red gradient
            textureData[index + 1] = (y * 255) / height; // Green gradient  
            textureData[index + 2] = 128;               // Blue constant
        }
    }
    
    // Create texture node and set data directly from memory
    SoTexture2* texture = new SoTexture2;
    texture->ref();
    
    // NEW: Easy way to set texture data from memory
    texture->setImageData(width, height, components, textureData);
    // <-- New convenience method for in-memory textures!
    
    // Verify the data was set correctly
    int w, h, c;
    const unsigned char* data = texture->getImageData(w, h, c);
    // <-- New convenience method for getting texture data!
    
    std::cout << "Texture set from memory: " << w << "x" << h 
              << " with " << c << " components" << std::endl;
    
    // For performance-critical applications, use no-copy version
    unsigned char* noCopyData = new unsigned char[64 * 64 * 4];
    // ... fill noCopyData ...
    
    SoTexture2* fastTexture = new SoTexture2;
    fastTexture->ref();
    
    // NEW: Zero-copy texture data for maximum performance
    fastTexture->setImageDataNoCopy(64, 64, 4, noCopyData, TRUE); 
    // <-- New zero-copy method with automatic memory management!
    
    // Similar API for 3D textures
    SoTexture3* texture3d = new SoTexture3;
    texture3d->ref();
    
    unsigned char* data3d = new unsigned char[32 * 32 * 32 * 4];
    // ... fill 3D texture data ...
    
    // NEW: Easy 3D texture creation from memory
    texture3d->setImageData(32, 32, 32, 4, data3d);
    // <-- New 3D texture convenience method!
    
    delete[] textureData;
    // noCopyData will be freed automatically since we used TRUE flag
    delete[] data3d;
    
    texture->unref();
    fastTexture->unref();
    texture3d->unref();
}
*/

int main() {
    std::cout << "Coin3D iostream and Texture Memory Enhancement Demo\n";
    std::cout << "===================================================\n\n";
    
    std::cout << "This demo shows the new features added to Coin3D:\n\n";
    
    std::cout << "1. IOSTREAM SUPPORT:\n";
    std::cout << "   - SoInput::setStream(std::istream*) for reading from C++ streams\n";
    std::cout << "   - SoOutput::setStream(std::ostream*) for writing to C++ streams\n";
    std::cout << "   - Full stringstream support for in-memory scene graph I/O\n";
    std::cout << "   - Compatible with std::ifstream, std::stringstream, etc.\n\n";
    
    std::cout << "2. TEXTURE MEMORY MANAGEMENT:\n";
    std::cout << "   - SoTexture2::setImageData() for easy 2D texture creation from memory\n";
    std::cout << "   - SoTexture2::setImageDataNoCopy() for zero-copy performance\n";
    std::cout << "   - SoTexture2::getImageData() for retrieving texture data\n";
    std::cout << "   - SoTexture3::setImageData() for 3D texture memory management\n";
    std::cout << "   - SoTexture3::getImageData() for 3D texture data access\n";
    std::cout << "   - Flexible copy policies: COPY, NO_COPY, NO_COPY_AND_FREE\n\n";
    
    std::cout << "BENEFITS:\n";
    std::cout << "✓ Modern C++ stream interfaces\n";
    std::cout << "✓ Efficient in-memory texture handling\n";
    std::cout << "✓ Zero-copy options for performance\n";
    std::cout << "✓ Backward compatible with existing APIs\n";
    std::cout << "✓ Reduces need for temporary files\n";
    std::cout << "✓ Simplifies texture memory management\n\n";
    
    std::cout << "Example usage shown in commented code above.\n";
    std::cout << "Run the comprehensive tests to see the features in action!\n";
    
    return 0;
}