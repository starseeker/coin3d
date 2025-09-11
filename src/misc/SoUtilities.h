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

#ifndef COIN_SOUTILITIES_H
#define COIN_SOUTILITIES_H

/*!
 * \file SoUtilities.h
 * \brief Modern C++17 utility functions for Coin3D internal use
 * 
 * This file provides C++17-based replacements for legacy C utility functions,
 * using standard library features and modern C++ practices.
 */

#include <string>
#include <cctype>
#include <algorithm>

// Internal namespace for Coin3D implementation details
namespace CoinInternal {

/*!
 * \brief Check if character is ASCII
 * 
 * Modern C++17 replacement for coin_isascii().
 * 
 * \param c Character to check
 * \return true if character is ASCII (0x00-0x7F), false otherwise
 */
inline bool isAscii(int c) {
    return (c >= 0x00) && (c < 0x80);
}

/*!
 * \brief Check if character is whitespace
 * 
 * Modern C++17 replacement for coin_isspace().
 * This implementation matches POSIX and C locales.
 * 
 * \param c Character to check
 * \return true if character is whitespace, false otherwise
 */
inline bool isSpace(char c) {
    return (c == ' ') || (c == '\n') || (c == '\t') ||
           (c == '\r') || (c == '\f') || (c == '\v');
}

/*!
 * \brief Case-insensitive string comparison
 * 
 * Modern C++17 replacement for coin_strncasecmp().
 * 
 * \param s1 First string
 * \param s2 Second string  
 * \param len Maximum number of characters to compare
 * \return 0 if strings are equal (case-insensitive), 
 *         negative if s1 < s2, positive if s1 > s2
 */
inline int stringCompareIgnoreCase(const char* s1, const char* s2, size_t len) {
    if (!s1 || !s2) return s1 ? 1 : (s2 ? -1 : 0);
    
    for (size_t i = 0; i < len; ++i) {
        int c1 = std::tolower(static_cast<unsigned char>(s1[i]));
        int c2 = std::tolower(static_cast<unsigned char>(s2[i]));
        
        if (c1 != c2) return c1 - c2;
        if (c1 == 0) break; // End of string
    }
    return 0;
}

/*!
 * \brief Case-insensitive string comparison (std::string version)
 * 
 * Convenience overload for std::string.
 * 
 * \param s1 First string
 * \param s2 Second string
 * \param len Maximum number of characters to compare
 * \return 0 if strings are equal (case-insensitive), 
 *         negative if s1 < s2, positive if s1 > s2
 */
inline int stringCompareIgnoreCase(const std::string& s1, const std::string& s2, size_t len) {
    return stringCompareIgnoreCase(s1.c_str(), s2.c_str(), len);
}

} // namespace CoinInternal

#endif // COIN_SOUTILITIES_H