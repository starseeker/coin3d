/**
 * @file test_texture_memory.cpp  
 * @brief Test program for texture memory management improvements
 */

#include "../utils/test_common.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/nodes/SoTexture3.h>
#include <Inventor/nodes/SoCube.h>
#include <cstring>

using namespace CoinTestUtils;

// Helper function to create test texture data
unsigned char* createTestTexture2D(int width, int height, int components) {
    size_t size = width * height * components;
    unsigned char* data = new unsigned char[size];
    
    // Create a simple gradient pattern
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int index = (y * width + x) * components;
            data[index] = (unsigned char)((x * 255) / width);     // Red gradient
            if (components > 1) data[index + 1] = (unsigned char)((y * 255) / height); // Green gradient  
            if (components > 2) data[index + 2] = 128; // Blue constant
            if (components > 3) data[index + 3] = 255; // Alpha opaque
        }
    }
    
    return data;
}

// Helper function to create test texture data for 3D textures
unsigned char* createTestTexture3D(int width, int height, int depth, int components) {
    size_t size = width * height * depth * components;
    unsigned char* data = new unsigned char[size];
    
    // Create a simple 3D gradient pattern
    for (int z = 0; z < depth; z++) {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int index = ((z * height + y) * width + x) * components;
                data[index] = (unsigned char)((x * 255) / width);         // Red gradient
                if (components > 1) data[index + 1] = (unsigned char)((y * 255) / height);   // Green gradient
                if (components > 2) data[index + 2] = (unsigned char)((z * 255) / depth);    // Blue gradient  
                if (components > 3) data[index + 3] = 255;                // Alpha opaque
            }
        }
    }
    
    return data;
}

TEST_CASE("SoTexture2 setImageData", "[texture][memory]") {
    CoinTestFixture fixture;
    
    SoTexture2* texture = new SoTexture2;
    texture->ref();
    
    // Create test texture data
    const int width = 64, height = 64, components = 3;
    unsigned char* testData = createTestTexture2D(width, height, components);
    
    // Test setImageData 
    texture->setImageData(width, height, components, testData);
    
    // Verify the data was set correctly
    int retWidth, retHeight, retComponents;
    const unsigned char* retrievedData = texture->getImageData(retWidth, retHeight, retComponents);
    
    CHECK(retWidth == width);
    CHECK(retHeight == height);  
    CHECK(retComponents == components);
    CHECK(retrievedData != nullptr);
    
    // Verify some pixel data
    CHECK(retrievedData[0] == testData[0]);
    CHECK(retrievedData[1] == testData[1]);
    CHECK(retrievedData[2] == testData[2]);
    
    // Check that filename field was cleared
    CHECK(texture->filename.getValue() == "");
    CHECK(texture->image.isDefault() == FALSE);
    
    delete[] testData;
    texture->unref();
}

TEST_CASE("SoTexture2 setImageDataNoCopy", "[texture][memory][nocopy]") {
    CoinTestFixture fixture;
    
    SoTexture2* texture = new SoTexture2;
    texture->ref();
    
    // Create test texture data  
    const int width = 32, height = 32, components = 4;
    unsigned char* testData = createTestTexture2D(width, height, components);
    
    // Test setImageDataNoCopy without auto-free
    texture->setImageDataNoCopy(width, height, components, testData, FALSE);
    
    // Verify the data was set correctly
    int retWidth, retHeight, retComponents;
    const unsigned char* retrievedData = texture->getImageData(retWidth, retHeight, retComponents);
    
    CHECK(retWidth == width);
    CHECK(retHeight == height);
    CHECK(retComponents == components);
    CHECK(retrievedData == testData); // Should be the same pointer for NO_COPY
    
    texture->unref();
    delete[] testData; // We need to clean up manually since we used FALSE for freeOnDestroy
}

TEST_CASE("SoTexture3 setImageData", "[texture][memory][3d]") {
    CoinTestFixture fixture;
    
    SoTexture3* texture3d = new SoTexture3;
    texture3d->ref();
    
    // Create test 3D texture data
    const int width = 16, height = 16, depth = 16, components = 2;
    unsigned char* testData = createTestTexture3D(width, height, depth, components);
    
    // Test setImageData
    texture3d->setImageData(width, height, depth, components, testData);
    
    // Verify the data was set correctly
    int retWidth, retHeight, retDepth, retComponents;
    const unsigned char* retrievedData = texture3d->getImageData(retWidth, retHeight, retDepth, retComponents);
    
    CHECK(retWidth == width);
    CHECK(retHeight == height);
    CHECK(retDepth == depth);
    CHECK(retComponents == components);
    CHECK(retrievedData != nullptr);
    
    // Verify some pixel data
    CHECK(retrievedData[0] == testData[0]);
    CHECK(retrievedData[1] == testData[1]);
    
    // Check that filenames field was cleared
    CHECK(texture3d->filenames.getNum() == 0);
    CHECK(texture3d->images.isDefault() == FALSE);
    
    delete[] testData;
    texture3d->unref();
}

TEST_CASE("Texture memory integration test", "[texture][memory][integration]") {
    CoinTestFixture fixture;
    
    // Create a scene with both 2D and 3D textures
    SoSeparator* root = new SoSeparator;
    root->ref();
    
    // Add 2D texture
    SoTexture2* tex2d = new SoTexture2;
    unsigned char* data2d = createTestTexture2D(64, 64, 3);
    tex2d->setImageData(64, 64, 3, data2d);
    root->addChild(tex2d);
    
    // Add 3D texture  
    SoTexture3* tex3d = new SoTexture3;
    unsigned char* data3d = createTestTexture3D(16, 16, 16, 4);
    tex3d->setImageData(16, 16, 16, 4, data3d);
    root->addChild(tex3d);
    
    // Add some geometry
    SoCube* cube = new SoCube;
    root->addChild(cube);
    
    // Validate scene structure
    CHECK(root->getNumChildren() == 3);
    
    // Test that both textures have their data correctly set
    int w, h, d, c;
    const unsigned char* data;
    
    data = tex2d->getImageData(w, h, c);
    CHECK(w == 64);
    CHECK(h == 64);
    CHECK(c == 3);
    CHECK(data != nullptr);
    
    data = tex3d->getImageData(w, h, d, c);
    CHECK(w == 16);
    CHECK(h == 16); 
    CHECK(d == 16);
    CHECK(c == 4);
    CHECK(data != nullptr);
    
    delete[] data2d;
    delete[] data3d;
    root->unref();
}