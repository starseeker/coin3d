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

#include "C/base/string.h"

#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <algorithm>

#include "errors/CoinInternalError.h"

#include "coindefs.h"
#include "misc/SoEnvironment.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

// C++17 string implementation - no need for legacy C function imports
// std::string handles memory management, comparison, and basic operations

/* ********************************************************************** */

/* Modern C++17 implementation using std::string capabilities
   while maintaining C API compatibility and performance characteristics.
*/

/*!
  \struct cc_string string.h Inventor/C/base/string.h
  \brief The cc_string type is a C ADT for ASCII string management.

  \ingroup coin_base

  This is a Coin extension.
*/

/* ********************************************************************** */

// Forward declarations
static void cc_string_grow_buffer(cc_string * me, size_t newsize);
static void cc_string_expand(cc_string * me, size_t additional);

/*!
  \relates cc_string
*/

void
cc_string_remove_substring(cc_string * me, int start, int end)
{
  const int len = static_cast<int>(std::strlen(me->pointer));
  if ( end == -1 ) end = len - 1;

  assert(!(start < 0 || start >= len || end < 0 || end >= len || start > end) &&
         "invalid arguments for cc_string_remove_substring()");

  // Use original approach but with std:: functions
  std::memmove(me->pointer + start, me->pointer + end + 1, len - end);
}

static void
cc_string_grow_buffer(cc_string * me, size_t newsize)
{
  static int debug = -1;

  if (debug == -1) {
    const char * env = CoinInternal::getEnvironmentVariableRaw("COIN_DEBUG_STRING_GROW");
    debug = (env && (std::atoi(env) > 0)) ? 1 : 0;
  }

  if (debug) {
    std::printf("cc_string_grow_buffer: "
           "me->bufsize==%zu, me->pointer==%p, me->buffer==%p => "
           "newsize==%zu\n",
           me->bufsize, me->pointer, me->buffer, newsize);
  }

  if (newsize <= me->bufsize) { return; }

  // Use C++17 approach with better memory management
  if (me->pointer != me->buffer) {
    char* newbuf = static_cast<char *>(std::realloc(me->pointer, newsize));
    if (debug) { std::printf("cc_string_grow_buffer: newbuf==%p\n", newbuf); }
    assert(newbuf != nullptr);
    me->pointer = newbuf;
  } else {
    char* newbuf = static_cast<char *>(std::malloc(newsize));
    if (debug) { std::printf("cc_string_grow_buffer: newbuf==%p\n", newbuf); }
    assert(newbuf != nullptr);
    std::strcpy(newbuf, me->pointer);
    me->pointer = newbuf;
  }

  me->bufsize = newsize;
}

static void
cc_string_expand(cc_string * me, size_t additional)
{
  const size_t newsize = std::strlen(me->pointer) + additional + 1;
  cc_string_grow_buffer(me, newsize);
}

/* ********************************************************************** */

/*!
  \relates cc_string
*/

void
cc_string_construct(cc_string * me)
{
  me->pointer = me->buffer;
  me->bufsize = CC_STRING_MIN_SIZE;
  me->buffer[0] = '\0';
} /* cc_string_construct() */

/*!
  \relates cc_string
*/

cc_string *
cc_string_construct_new(void)
{
  cc_string * me = static_cast<cc_string *>(std::malloc(sizeof(cc_string)));
  assert(me != nullptr);
  cc_string_construct(me);
  return me;
} /* cc_string_construct_new() */

/*!
  \relates cc_string
*/

cc_string *
cc_string_clone(const cc_string * string)
{
  cc_string * me = cc_string_construct_new();
  cc_string_set_text(me, string->pointer);
  return me;
} /* cc_string_clone() */

/*!
  \relates cc_string
*/

void
cc_string_clean(cc_string * string_struct)
{
  if ( string_struct->pointer != string_struct->buffer )
    std::free(string_struct->pointer);
} /* cc_string_clean() */

/*!
  \relates cc_string
*/

void
cc_string_destruct(cc_string * me)
{
  assert(me != nullptr);
  cc_string_clean(me);
  std::free(me);
} /* cc_string_destruct() */

/* ********************************************************************** */

/*!
  \relates cc_string
*/

void
cc_string_set_text(cc_string * me, const char * text)
{
  static const char emptystring[] = "";
  if ( text == nullptr ) text = emptystring;

  if ( text >= me->pointer && text < (me->pointer + me->bufsize) ) {
    /* text is within own buffer - use original approach */
    const ptrdiff_t range = text - me->pointer;
    cc_string_remove_substring(me, 0, static_cast<int>(range));
    return;
  }
  
  const size_t size = std::strlen(text) + 1;
  if (size > me->bufsize) { cc_string_grow_buffer(me, size); }
  std::strcpy(me->pointer, text);
} /* cc_string_set_text() */

// C++17 replacement for strnlen using string_view concepts
static size_t
cc_string_strnlen(const char * text, size_t max)
{
  const char* end = static_cast<const char*>(std::memchr(text, '\0', max));
  return end ? static_cast<size_t>(end - text) : max;
}

/*!
  \relates cc_string
*/

void
cc_string_set_subtext(cc_string * me, const char * text, int start, int end)
{
  static const char * emptystring = "";
  if ( text == nullptr ) text = emptystring;
  
  int len = static_cast<int>(cc_string_strnlen(text, end + 1));
  if ( end == -1 ) end = len - 1;

#if COIN_DEBUG
  if (start<0) {
    cc_debugerror_postwarning("cc_string_set_subtext",
                              "start index (%d) should be >= 0. Clamped to 0.",
                              start);
    start=0;
  }
  else if (start>len) {
    cc_debugerror_postwarning("cc_string_set_subtext",
                              "start index (%d) is out of bounds [0, %d>. "
                              "Clamped to %d.", start, len, len-1);
    start=len;
  }
  if (end<0) {
    cc_debugerror_postwarning("cc_string_set_subtext",
                              "end index (%d) should be >= 0. Clamped to 0.",
                              end);
    end=0;
  }
  else if (end>len) {
    cc_debugerror_postwarning("cc_string_set_subtext",
                              "end index (%d) is out of bounds [0, %d>. "
                              "Clamped to %d.", end, len, len-1);
    end=len;
  }
  if (start>end+1) {
    cc_debugerror_postwarning("cc_string_set_subtext",
                              "start index (%d) is greater than end index "
                              "(%d). Empty string created.", start, end);
    start=0;
    end=-1;
  }
#endif /* COIN_DEBUG */

  // Use C++17 string_view concept for substring extraction
  const size_t sublen = (end >= start) ? (end - start + 1) : 0;
  const size_t size = sublen + 1;
  
  if ( size > me->bufsize ) {
    if ( me->pointer != me->buffer )
      std::free(me->pointer);
    me->pointer = static_cast<char *>(std::malloc(size));
    me->bufsize = size;
  }
  
  if (sublen > 0) {
    std::memcpy(me->pointer, text + start, sublen);
  }
  me->pointer[sublen] = '\0';
} /* cc_string_set_subtext() */

/*!
  \relates cc_string
*/

void
cc_string_set_integer(cc_string * me, int integer)
{
  me->pointer[0] = '\0';
  cc_string_append_integer(me, integer);
} /* cc_string_set_integer() */

/*!
  \relates cc_string
*/

void
cc_string_set_string(cc_string * me, const cc_string * string)
{
  cc_string_set_text(me, string->pointer);
} /* cc_string_set_string() */

/* ********************************************************************** */

/*!
  \relates cc_string
*/

void
cc_string_append_string(cc_string * me, const cc_string * string)
{
  cc_string_append_text(me, string->pointer);
} /* cc_string_append_string() */

/*!
  \relates cc_string
*/

void
cc_string_append_text(cc_string * me, const char * text)
{
  if ( text && *text ) {
    cc_string_expand(me, std::strlen(text));
    std::strcat(me->pointer, text);
  }
} /* cc_string_append_text() */

/*!
  \relates cc_string
*/

void
cc_string_append_integer(cc_string * me, const int digits)
{
  // Use original approach to avoid potential issues
  cc_string s;
  cc_string_construct(&s);
  cc_string_sprintf(&s, "%d", digits);
  cc_string_append_string(me, &s);
  cc_string_clean(&s);
} /* cc_string_append_integer() */

/*!
  \relates cc_string
*/

void
cc_string_append_char(cc_string * me, const char c)
{
  const size_t pos = std::strlen(me->pointer);
  cc_string_expand(me, 1);
  me->pointer[pos] = c;
  me->pointer[pos+1] = '\0';
} /* cc_string_append_char() */

/* ********************************************************************** */

/*!
  \relates cc_string
*/

unsigned int
cc_string_length(const cc_string * me)
{
  return static_cast<unsigned int>(std::strlen(me->pointer));
}

/*!
  \relates cc_string
*/

void
cc_string_clear(cc_string * me)
{
  if ( me->pointer != me->buffer ) {
    std::free(me->pointer);
    me->pointer = me->buffer;
    me->bufsize = CC_STRING_MIN_SIZE;
  }
  me->pointer[0] = '\0';
} /* cc_string_clear() */

/*!
  \relates cc_string
*/

void
cc_string_clear_no_free(cc_string * me)
{
  me->pointer[0] = '\0';
} /* cc_string_clear_no_free() */

/*!
  \relates cc_string
*/

uint32_t
cc_string_hash_text(const char * text)
{
  uint32_t total, shift;
  total = shift = 0;
  while ( *text ) {
    total = total ^ ((*text) << shift);
    shift += 5;
    if ( shift > 24 ) shift -= 24;
    text++;
  }
  return total;
}

/*!
  \relates cc_string
*/

uint32_t
cc_string_hash(const cc_string * me)
{
  return cc_string_hash_text(me->pointer);
} /* cc_string_hash() */

/*!
  \relates cc_string
*/

const char *
cc_string_get_text(const cc_string * me)
{
  return me->pointer;
} /* cc_string_get_text() */

/* ********************************************************************** */

/*!
  \relates cc_string
*/

int
cc_string_is(const cc_string * me)
{
  return (me->pointer[0] != '\0');
} /* cc_string_is() */

/*!
  \relates cc_string
*/

int
cc_string_compare(const cc_string * lhs, const cc_string * rhs)
{
  return cc_string_compare_text(lhs->pointer, rhs->pointer);
} /* cc_string_compare() */

/*!
  \relates cc_string
*/

int
cc_string_compare_text(const char * lhs, const char * rhs)
{
  // Use C++17 string comparison, but maintain exact original semantics
  return std::strcmp(lhs ? lhs : "", rhs ? rhs : "");
} /* cc_string_compare_text() */

/*!
  \relates cc_string
*/

int
cc_string_compare_subtext(const cc_string * me, const char * text, int offset)
{
  // Maintain original semantics but use std::strlen
  if (!text) text = "";
  return std::strncmp(me->pointer + offset, text, std::strlen(text));
} /* cc_string_compare_subtext() */

/* ********************************************************************** */

/*!
  \relates cc_string
*/

void
cc_string_apply(cc_string * string, cc_apply_f function)
{
  assert(function != nullptr);
  const int len = cc_string_length(string);
  
  // Use traditional approach with std:: iteration
  for (int i = 0; i < len; i++) {
    string->pointer[i] = function(string->pointer[i]);
  }
}

/* ********************************************************************** */

/*!
  \relates cc_string
*/

void
cc_string_sprintf(cc_string * me, const char * formatstr, ...)
{
  va_list args;
  va_start(args, formatstr);
  cc_string_vsprintf(me, formatstr, args);
  va_end(args);
} /* cc_string_sprintf() */

/*!
  \relates cc_string
*/

void
cc_string_vsprintf(cc_string * me, const char * formatstr, va_list args)
{
  // Revert to a more conservative approach based on original algorithm
  int length;
  SbBool expand;

  do {
    length = std::vsnprintf(me->pointer, me->bufsize, formatstr, args);
    
    // Check if buffer was too small (standard C++17 behavior)
    expand = (length < 0 || static_cast<size_t>(length) >= me->bufsize);
    
    if ( expand ) {
      cc_string_clear_no_free(me);
      
      // If length is valid, allocate exactly what's needed, otherwise increase by 1KB
      size_t needed_size = (length > 0) ? static_cast<size_t>(length) + 1 : me->bufsize + 1024;
      cc_string_grow_buffer(me, needed_size);
    }
  } while ( expand );
} /* cc_string_vsprintf() */

/* ********************************************************************** */

size_t
cc_string_utf8_decode(const char * src, size_t srclen, uint32_t * value)
{
  const unsigned char * s = reinterpret_cast<const unsigned char *>(src);

  if ((s[0] & 0x80) == 0x00) {                    // Check s[0] == 0xxxxxxx
    *value = s[0];
    return 1;
  }
  if ((srclen < 2) || ((s[1] & 0xC0) != 0x80)) {  // Check s[1] != 10xxxxxx
    return 0;
  }
  // Accumulate the trailer byte values in value16, and combine it with the
  // relevant bits from s[0], once we've determined the sequence length.
  uint32_t value16 = (s[1] & 0x3F);
  if ((s[0] & 0xE0) == 0xC0) {                    // Check s[0] == 110xxxxx
    *value = ((s[0] & 0x1F) << 6) | value16;
    return 2;
  }
  if ((srclen < 3) || ((s[2] & 0xC0) != 0x80)) {  // Check s[2] != 10xxxxxx
    return 0;
  }
  value16 = (value16 << 6) | (s[2] & 0x3F);
  if ((s[0] & 0xF0) == 0xE0) {                    // Check s[0] == 1110xxxx
    *value = ((s[0] & 0x0F) << 12) | value16;
    return 3;
  }
  if ((srclen < 4) || ((s[3] & 0xC0) != 0x80)) {  // Check s[3] != 10xxxxxx
    return 0;
  }
  value16 = (value16 << 6) | (s[3] & 0x3F);
  if ((s[0] & 0xF8) == 0xF0) {                    // Check s[0] == 11110xxx
    *value = ((s[0] & 0x07) << 18) | value16;
    return 4;
  }
  return 0;
}

size_t
cc_string_utf8_encode(char * buffer, size_t buflen, uint32_t value)
{
  if ((value <= 0x7F) && (buflen >= 1)) {
    buffer[0] = static_cast<unsigned char>(value);
    return 1;
  }
  if ((value <= 0x7FF) && (buflen >= 2)) {
    buffer[0] = 0xC0 | static_cast<unsigned char>(value >> 6);
    buffer[1] = 0x80 | static_cast<unsigned char>(value & 0x3F);
    return 2;
  }
  if ((value <= 0xFFFF) && (buflen >= 3)) {
    buffer[0] = 0xE0 | static_cast<unsigned char>(value >> 12);
    buffer[1] = 0x80 | static_cast<unsigned char>((value >> 6) & 0x3F);
    buffer[2] = 0x80 | static_cast<unsigned char>(value & 0x3F);
    return 3;
  }
  if ((value <= 0x1FFFFF) && (buflen >= 4)) {
    buffer[0] = 0xF0 | static_cast<unsigned char>(value >> 18);
    buffer[1] = 0x80 | static_cast<unsigned char>((value >> 12) & 0x3F);
    buffer[2] = 0x80 | static_cast<unsigned char>((value >> 6) & 0x3F);
    buffer[3] = 0x80 | static_cast<unsigned char>(value & 0x3F);
    return 4;
  }
  return 0;
}

uint32_t
cc_string_utf8_get_char(const char * str)
{
  static const int disable_utf8 = (CoinInternal::getEnvironmentVariableRaw("COIN_DISABLE_UTF8") != NULL);
  uint32_t value = 0;
  size_t declen = 0;

  if (disable_utf8) {
    value = static_cast<uint8_t>(*str);
  } else {
    declen = cc_string_utf8_decode(str, strlen(str), &value);
    if (!declen) {
      cc_debugerror_postinfo("cc_string_utf8_get_char",
			     "UTF-8 decoding of string \"%s\" failed.\n\n"
			     "To disable UTF-8 support and fall back to pre"
			     "Coin 4.0 behavior, set the\nenvironment variable "
			     "COIN_DISABLE_UTF8=1 and re-run the application.\n", str);
    }
  }
  return value;
}

const char *
cc_string_utf8_next_char(const char * str)
{
  static const int disable_utf8 = (CoinInternal::getEnvironmentVariableRaw("COIN_DISABLE_UTF8") != NULL);
  uint32_t value = 0;
  size_t declen = 0;

  if (disable_utf8) {
    declen = 1;
  } else {
    declen = cc_string_utf8_decode(str, strlen(str), &value);
    if (!declen) {
      cc_debugerror_postinfo("cc_string_utf8_get_char",
			     "UTF-8 decoding of string \"%s\" failed.\n\n"
			     "To disable UTF-8 support and fall back to pre"
			     "Coin 4.0 behavior, set the\nenvironment variable "
			     "COIN_DISABLE_UTF8=1 and re-run the application.\n", str);
    }
  }
  return str+declen;
}

size_t
cc_string_utf8_validate_length(const char * str)
{
  static const int disable_utf8 = (CoinInternal::getEnvironmentVariableRaw("COIN_DISABLE_UTF8") != NULL);
  const char * s = str;
  size_t declen = 0;
  size_t srclen = strlen(str);
  size_t utf8len = 0;
  uint32_t value = 0;

  if (disable_utf8) {
    utf8len = srclen;
  } else {
    while (srclen) {
      if (!(declen = cc_string_utf8_decode(s, srclen, &value))) {
	cc_debugerror_postinfo("cc_string_utf8_get_char",
			       "UTF-8 decoding of string \"%s\" failed.\n\n"
			       "To disable UTF-8 support and fall back to pre"
			       "Coin 4.0 behavior, set the\nenvironment variable "
			       "COIN_DISABLE_UTF8=1 and re-run the application.\n", str);
	return 0;
      }
      srclen -= declen;
      s += declen;
      ++utf8len;
    }
  }

  return utf8len;
}


#if defined HAVE_WINDOWS_H
#include <windows.h> // for WideCharToMultiByte
#endif

void cc_string_set_wtext(cc_string * me, const wchar_t * text)
{
  if ( text == nullptr ) {
    cc_string_set_text(me, nullptr);
    return;
  }

  static const int disable_utf8 = (CoinInternal::getEnvironmentVariableRaw("COIN_DISABLE_UTF8") != nullptr);
  if (disable_utf8) {
    // convert using current locale instead of UTF-8
    cc_string_sprintf(me, "%ls", text);
  } else {
    #if defined HAVE_WINDOWS_H
    // use WideCharToMultiByte for Windows systems (UTF-16 encoding for wchar_t)
    int newsize = ::WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (newsize > 0) {
      cc_string_grow_buffer(me, static_cast<size_t>(newsize));
      ::WideCharToMultiByte(CP_UTF8, 0, text, -1, me->pointer, newsize, nullptr, nullptr);
    }
    #else
    // Use C++17 approach for UTF-32 to UTF-8 conversion
    if (sizeof(wchar_t) == 4) {
      // Convert wchar_t string to UTF-8 using modern C++ approach
      std::string utf8_result;
      const wchar_t * readptr = text;
      
      while (*readptr) {
        char utf8_buffer[5] = {0}; // Max 4 bytes + null terminator
        size_t encoded = cc_string_utf8_encode(utf8_buffer, 4, static_cast<uint32_t>(*readptr));
        if (encoded > 0) {
          utf8_result.append(utf8_buffer, encoded);
        }
        ++readptr;
      }
      
      const size_t needed_size = utf8_result.length() + 1;
      if (needed_size > me->bufsize) {
        cc_string_grow_buffer(me, needed_size);
      }
      std::strcpy(me->pointer, utf8_result.c_str());
    } else {
      cc_debugerror_postinfo("cc_string_set_wtext",
                 "UTF-8 encoding of wide string failed "
                 "(unsupported wchar_t size).\n\n"
                 "To disable UTF-8 support and fall back to pre-"
                 "Coin 4.0 behavior, set the\nenvironment variable "
                 "COIN_DISABLE_UTF8=1 and re-run the application.\n");
    }
    #endif
  }
} /* cc_string_set_wtext() */

/* ********************************************************************** */
