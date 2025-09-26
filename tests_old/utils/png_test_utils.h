#ifndef PNG_TEST_UTILS_H
#define PNG_TEST_UTILS_H

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
 * @file png_test_utils.h
 * @brief Minimalist PNG utility functions for test output using svpng
 * 
 * Provides simple PNG writing functions for test output and validation,
 * using the integrated svpng library for easier debugging and inspection
 * compared to PPM format.
 */

#include <string>

namespace CoinTestUtils {

/**
 * @brief Write RGBA pixel data to PNG file using svpng
 * @param filename Output PNG filename
 * @param pixels RGBA pixel data (bottom-left origin)
 * @param width Image width
 * @param height Image height
 * @param flip_vertically If true, flip image vertically (for OpenGL output)
 * @return true if successful, false on error
 */
bool writePNG(const std::string& filename, const unsigned char* pixels, 
              int width, int height, bool flip_vertically = true);

/**
 * @brief Write RGB pixel data to PNG file using svpng
 * @param filename Output PNG filename
 * @param pixels RGB pixel data (bottom-left origin)
 * @param width Image width
 * @param height Image height
 * @param flip_vertically If true, flip image vertically (for OpenGL output)
 * @return true if successful, false on error
 */
bool writePNG_RGB(const std::string& filename, const unsigned char* pixels,
                  int width, int height, bool flip_vertically = true);

} // namespace CoinTestUtils

#endif // PNG_TEST_UTILS_H