#ifndef COIN_UTF8_H
#define COIN_UTF8_H

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

/*
  Modern UTF-8 support using neacsum/utf8 library.
  Replaces legacy cc_string UTF-8 functions with C++17 standard library
  backed implementation.
*/

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

// Modern UTF-8 API using neacsum/utf8 library internally
// These replace the legacy cc_string_utf8_* functions

/// Validate UTF-8 string and return number of Unicode code points
/// Replaces: cc_string_utf8_validate_length
size_t coin_utf8_validate_length(const char* str);

/// Get Unicode code point at current position
/// Replaces: cc_string_utf8_get_char
uint32_t coin_utf8_get_char(const char* str);

/// Advance to next Unicode code point
/// Replaces: cc_string_utf8_next_char
const char* coin_utf8_next_char(const char* str);

/// Decode UTF-8 bytes into Unicode code point
/// Replaces: cc_string_utf8_decode  
size_t coin_utf8_decode(const char* src, size_t srclen, uint32_t* value);

/// Encode Unicode code point into UTF-8 bytes
/// Replaces: cc_string_utf8_encode
size_t coin_utf8_encode(char* buffer, size_t buflen, uint32_t value);

#ifdef __cplusplus
}
#endif

#endif /* COIN_UTF8_H */