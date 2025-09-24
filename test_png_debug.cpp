/**
 * @file debug_rgb_vs_rgba.cpp  
 * @brief Simple test to debug RGB vs RGBA PNG saving differences
 */

#include <memory>
#include <iostream>
#include <iomanip>

// Include svpng for PNG output
#include "src/glue/svpng.h"

// Simple test data to create distinct RGB vs RGBA patterns
void createTestData() {
    const int width = 16;
    const int height = 16;
    
    // Create RGB test data (3 components)
    std::unique_ptr<unsigned char[]> rgb_data(new unsigned char[width * height * 3]);
    
    // Create RGBA test data (4 components) 
    std::unique_ptr<unsigned char[]> rgba_data(new unsigned char[width * height * 4]);
    
    // Fill RGB data with a simple pattern
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx3 = (y * width + x) * 3;
            int idx4 = (y * width + x) * 4;
            
            // Create a gradient pattern
            unsigned char r = (unsigned char)((x * 255) / (width - 1));
            unsigned char g = (unsigned char)((y * 255) / (height - 1));
            unsigned char b = 128;  // Constant blue
            
            // RGB data
            rgb_data[idx3 + 0] = r;
            rgb_data[idx3 + 1] = g;
            rgb_data[idx3 + 2] = b;
            
            // RGBA data 
            rgba_data[idx4 + 0] = r;
            rgba_data[idx4 + 1] = g;
            rgba_data[idx4 + 2] = b;
            rgba_data[idx4 + 3] = 255; // Full alpha
        }
    }
    
    // Test the two PNG saving approaches
    
    // 1. RGB direct save
    FILE* file1 = fopen("test_rgb_direct.png", "wb");
    if (file1) {
        svpng(file1, width, height, rgb_data.get(), 0);
        fclose(file1);
        std::cout << "Saved test_rgb_direct.png (RGB data direct)" << std::endl;
    }
    
    // 2. RGBA converted to RGB  
    std::unique_ptr<unsigned char[]> rgb_from_rgba(new unsigned char[width * height * 3]);
    for (int i = 0; i < width * height; i++) {
        rgb_from_rgba[i * 3 + 0] = rgba_data[i * 4 + 0]; // R
        rgb_from_rgba[i * 3 + 1] = rgba_data[i * 4 + 1]; // G
        rgb_from_rgba[i * 3 + 2] = rgba_data[i * 4 + 2]; // B
    }
    
    FILE* file2 = fopen("test_rgba_converted.png", "wb");
    if (file2) {
        svpng(file2, width, height, rgb_from_rgba.get(), 0);
        fclose(file2);
        std::cout << "Saved test_rgba_converted.png (RGBA data converted to RGB)" << std::endl;
    }
    
    // Print some pixel values for comparison
    std::cout << "\nPixel value comparison:" << std::endl;
    for (int i = 0; i < 5; i++) {
        int idx3 = i * 3;
        int idx4 = i * 4;
        std::cout << "Pixel " << i << ":" << std::endl;
        std::cout << "  RGB direct:     R=" << (int)rgb_data[idx3] << " G=" << (int)rgb_data[idx3+1] << " B=" << (int)rgb_data[idx3+2] << std::endl;
        std::cout << "  RGBA original:  R=" << (int)rgba_data[idx4] << " G=" << (int)rgba_data[idx4+1] << " B=" << (int)rgba_data[idx4+2] << " A=" << (int)rgba_data[idx4+3] << std::endl;
        std::cout << "  RGBA->RGB conv: R=" << (int)rgb_from_rgba[idx3] << " G=" << (int)rgb_from_rgba[idx3+1] << " B=" << (int)rgb_from_rgba[idx3+2] << std::endl;
    }
}

int main() {
    std::cout << "Testing RGB vs RGBA PNG output..." << std::endl;
    createTestData();
    return 0;
}