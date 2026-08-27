#ifndef OBOL_TEST_UTILS_H
#define OBOL_TEST_UTILS_H

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
 * @file test_utils.h
 * @brief Shared support for Obol GTest suites.
 */

#include <string>
#include <vector>
#include <cstdio>
#include <gtest/gtest.h>
#include <Inventor/SoOffscreenRenderer.h>

namespace ObolTest {

// RGB output utilities to replace PNG functions
// Uses SoOffscreenRenderer::writeToRGB() for SGI RGB format
namespace RGBOutput {

/**
 * @brief Save RGB image data to SGI RGB file format
 * @param filename Output filename (should end in .rgb)
 * @param buffer RGB pixel data (3 bytes per pixel, no alpha)
 * @param width Image width
 * @param height Image height
 * @param flip_vertically If true, flip image vertically (for OpenGL output)
 * @return true if successful, false on error
 */
bool saveRGB(const std::string& filename, const unsigned char* buffer,
             int width, int height, bool flip_vertically = true);

/**
 * @brief Save RGBA image data to SGI RGB file format (alpha channel discarded)
 * @param filename Output filename (should end in .rgb)
 * @param buffer RGBA pixel data (4 bytes per pixel)
 * @param width Image width
 * @param height Image height
 * @param flip_vertically If true, flip image vertically (for OpenGL output)
 * @return true if successful, false on error
 */
bool saveRGBA_toRGB(const std::string& filename, const unsigned char* buffer,
                    int width, int height, bool flip_vertically = true);

/**
 * @brief Helper to create RGB buffer from framebuffer data
 * Utility function to strip alpha channel from RGBA data
 */
std::vector<unsigned char> convertRGBA_toRGB(const unsigned char* rgba_buffer,
                                            int width, int height);

} // namespace RGBOutput

} // namespace ObolTest

#endif // OBOL_TEST_UTILS_H
