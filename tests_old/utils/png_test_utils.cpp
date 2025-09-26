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

#include "png_test_utils.h"
#include "../../src/glue/svpng.h"
#include <fstream>
#include <cstdio>
#include <iostream>

namespace CoinTestUtils {

bool writePNG(const std::string& filename, const unsigned char* pixels, 
              int width, int height, bool flip_vertically) {
    if (!pixels || width <= 0 || height <= 0) {
        return false;
    }
    
    FILE* fp = fopen(filename.c_str(), "wb");
    if (!fp) {
        std::cerr << "Failed to create PNG file: " << filename << std::endl;
        return false;
    }
    
    if (flip_vertically) {
        // Create a temporary buffer to flip the image vertically
        const int pixel_size = 4; // RGBA
        const int row_size = width * pixel_size;
        unsigned char* flipped_data = new unsigned char[width * height * pixel_size];
        
        // Copy rows in reverse order
        for (int y = 0; y < height; y++) {
            const unsigned char* src_row = pixels + (height - 1 - y) * row_size;
            unsigned char* dst_row = flipped_data + y * row_size;
            for (int x = 0; x < row_size; x++) {
                dst_row[x] = src_row[x];
            }
        }
        
        // Write PNG with flipped data
        svpng(fp, width, height, flipped_data, 1); // 1 = has alpha
        delete[] flipped_data;
    } else {
        // Write PNG directly without flipping
        svpng(fp, width, height, pixels, 1); // 1 = has alpha
    }
    
    fclose(fp);
    
    std::cout << "PNG image saved to: " << filename << std::endl;
    return true;
}

bool writePNG_RGB(const std::string& filename, const unsigned char* pixels,
                  int width, int height, bool flip_vertically) {
    if (!pixels || width <= 0 || height <= 0) {
        return false;
    }
    
    FILE* fp = fopen(filename.c_str(), "wb");
    if (!fp) {
        std::cerr << "Failed to create PNG file: " << filename << std::endl;
        return false;
    }
    
    if (flip_vertically) {
        // Create a temporary buffer to flip the image vertically
        const int pixel_size = 3; // RGB
        const int row_size = width * pixel_size;
        unsigned char* flipped_data = new unsigned char[width * height * pixel_size];
        
        // Copy rows in reverse order
        for (int y = 0; y < height; y++) {
            const unsigned char* src_row = pixels + (height - 1 - y) * row_size;
            unsigned char* dst_row = flipped_data + y * row_size;
            for (int x = 0; x < row_size; x++) {
                dst_row[x] = src_row[x];
            }
        }
        
        // Write PNG with flipped data
        svpng(fp, width, height, flipped_data, 0); // 0 = no alpha
        delete[] flipped_data;
    } else {
        // Write PNG directly without flipping
        svpng(fp, width, height, pixels, 0); // 0 = no alpha
    }
    
    fclose(fp);
    
    std::cout << "PNG image saved to: " << filename << std::endl;
    return true;
}

} // namespace CoinTestUtils