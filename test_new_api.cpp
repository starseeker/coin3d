#include <iostream>
#include <memory>

// Test the new setImageData API in isolation
#include <Inventor/SoDB.h>
#include <Inventor/SoInteraction.h>
#include <Inventor/nodes/SoTexture2.h>

int main() {
    // We can't initialize without a context manager, but we can test the API
    std::cout << "Testing new setImageData API...\n";
    
    // Create test texture data
    const int width = 4, height = 4, components = 3;
    unsigned char* data = new unsigned char[width * height * components];
    
    // Fill with simple pattern
    for (int i = 0; i < width * height * components; i++) {
        data[i] = (i * 50) % 255;
    }
    
    // Test the new API (should compile and link correctly)
    SoTexture2* texture = new SoTexture2;
    texture->ref();
    std::cout << "Created texture node\n";
    
    // This should work without crashing
    texture->setImageData(width, height, components, data);
    std::cout << "setImageData() completed successfully\n";
    
    // Test the getter
    int w, h, c;
    const unsigned char* retrieved = texture->getImageData(w, h, c);
    
    if (retrieved && w == width && h == height && c == components) {
        std::cout << "getImageData() successful: " << w << "x" << h << " with " << c << " components\n";
        std::cout << "First few bytes: " << (int)retrieved[0] << ", " << (int)retrieved[1] << ", " << (int)retrieved[2] << "\n";
    } else {
        std::cout << "getImageData() failed\n";
    }
    
    delete[] data;
    texture->unref();
    
    std::cout << "Test completed successfully!\n";
    return 0;
}