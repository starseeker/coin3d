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
 * @file test_utils.cpp
 * @brief Implementation of RGB output utilities for tests
 */

#include "test_utils.h"
#include <iostream>
#include <vector>
#include <cstdio>
#include <cstring>
#include <memory>

namespace SimpleTest {
namespace RGBOutput {

// Internal helper function to write SGI RGB header and data
static bool writeRGBData(FILE* fp, const unsigned char* rgb_data, 
                         int width, int height) {
    // Write SGI RGB header format - simplified version of SoOffscreenRendererP::writeToRGB
    
    // Write magic number
    unsigned short magic = 0x01da;
    unsigned short val = (magic >> 8) | ((magic & 0xff) << 8);  // endian swap
    if (fwrite(&val, 2, 1, fp) != 1) return false;
    
    // Write format (raw, no RLE)
    val = 0x0100;  // little endian for 0x0001
    if (fwrite(&val, 2, 1, fp) != 1) return false;
    
    // Write dimensions (3D for RGB)
    val = 0x0300;  // little endian for 0x0003
    if (fwrite(&val, 2, 1, fp) != 1) return false;
    
    // Write width
    val = (width >> 8) | ((width & 0xff) << 8);  // endian swap
    if (fwrite(&val, 2, 1, fp) != 1) return false;
    
    // Write height 
    val = (height >> 8) | ((height & 0xff) << 8);  // endian swap
    if (fwrite(&val, 2, 1, fp) != 1) return false;
    
    // Write number of components (3 for RGB)
    val = 0x0300;  // little endian for 0x0003
    if (fwrite(&val, 2, 1, fp) != 1) return false;
    
    // Write header padding (500 bytes total header)
    const size_t BUFSIZE = 500;
    unsigned char buf[BUFSIZE];
    memset(buf, 0, BUFSIZE);
    buf[7] = 255; // set maximum pixel value to 255
    strcpy((char*)buf + 8, "https://github.com/coin3d/");
    if (fwrite(buf, 1, BUFSIZE, fp) != BUFSIZE) return false;
    
    // Write RGB data in SGI format (component-planar)
    std::unique_ptr<unsigned char[]> tmpbuf(new unsigned char[width]);
    
    // Write each component separately (R, then G, then B)
    for (int component = 0; component < 3; component++) {
        for (int y = 0; y < height; y++) {
            // Extract component data for this row
            for (int x = 0; x < width; x++) {
                tmpbuf[x] = rgb_data[(x + y * width) * 3 + component];
            }
            if (fwrite(tmpbuf.get(), 1, width, fp) != (size_t)width) {
                return false;
            }
        }
    }
    
    return true;
}

bool saveRGB(const std::string& filename, const unsigned char* buffer, 
             int width, int height, bool flip_vertically) {
    if (!buffer || width <= 0 || height <= 0) {
        return false;
    }
    
    FILE* fp = fopen(filename.c_str(), "wb");
    if (!fp) {
        std::cerr << "Failed to create RGB file: " << filename << std::endl;
        return false;
    }
    
    bool success = false;
    if (flip_vertically) {
        // Create flipped buffer
        std::vector<unsigned char> flipped_data(width * height * 3);
        const int row_size = width * 3;
        
        // Copy rows in reverse order
        for (int y = 0; y < height; y++) {
            const unsigned char* src_row = buffer + (height - 1 - y) * row_size;
            unsigned char* dst_row = flipped_data.data() + y * row_size;
            memcpy(dst_row, src_row, row_size);
        }
        
        success = writeRGBData(fp, flipped_data.data(), width, height);
    } else {
        success = writeRGBData(fp, buffer, width, height);
    }
    
    fclose(fp);
    
    if (success) {
        std::cout << "RGB image saved to: " << filename << std::endl;
    }
    
    return success;
}

bool saveRGBA_toRGB(const std::string& filename, const unsigned char* buffer, 
                    int width, int height, bool flip_vertically) {
    if (!buffer || width <= 0 || height <= 0) {
        return false;
    }
    
    // Convert RGBA to RGB
    std::vector<unsigned char> rgb_data = convertRGBA_toRGB(buffer, width, height);
    
    return saveRGB(filename, rgb_data.data(), width, height, flip_vertically);
}

std::vector<unsigned char> convertRGBA_toRGB(const unsigned char* rgba_buffer, 
                                            int width, int height) {
    std::vector<unsigned char> rgb_data(width * height * 3);
    
    for (int i = 0; i < width * height; i++) {
        rgb_data[i * 3 + 0] = rgba_buffer[i * 4 + 0]; // R
        rgb_data[i * 3 + 1] = rgba_buffer[i * 4 + 1]; // G
        rgb_data[i * 3 + 2] = rgba_buffer[i * 4 + 2]; // B
        // Skip alpha channel
    }
    
    return rgb_data;
}

} // namespace RGBOutput
} // namespace SimpleTest