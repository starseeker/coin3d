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

/*!
 * \file CoinTidbits.inl
 * \brief Template implementations for CoinTidbits utilities
 * 
 * This file contains template implementations that need to be included
 * in the header to avoid linker issues.
 */

#ifndef COIN_TIDBITS_INL
#define COIN_TIDBITS_INL

#include <cstdio>
#include <cstdarg>
#include <vector>

namespace CoinTidbits {
namespace StringFormat {

template<typename... Args>
int safeSprintf(char* buffer, std::size_t size, const char* format, Args&&... args) {
    if (size == 0) return -1;
    
    int result = std::snprintf(buffer, size, format, std::forward<Args>(args)...);
    
    // Ensure null termination even on overflow
    if (result >= static_cast<int>(size)) {
        buffer[size - 1] = '\0';
        return -1; // Indicate overflow
    }
    
    return result;
}

template<typename... Args>
std::string formatString(const char* format, Args&&... args) {
    // First pass: determine required size
    int size = std::snprintf(nullptr, 0, format, std::forward<Args>(args)...);
    if (size <= 0) {
        return std::string{};
    }
    
    // Second pass: format into appropriately sized buffer
    std::vector<char> buffer(size + 1);
    std::snprintf(buffer.data(), buffer.size(), format, std::forward<Args>(args)...);
    
    return std::string(buffer.data(), size);
}

} // namespace StringFormat
} // namespace CoinTidbits

#endif // !COIN_TIDBITS_INL