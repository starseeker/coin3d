/*
 Modern UTF-8 implementation using neacsum/utf8 library.
 Replaces legacy cc_string UTF-8 functions with proper Unicode support.
*/

#include "Inventor/C/base/utf8.h"
#include "utf8/utf8.h"
#include <cstring>

#ifdef __cplusplus
extern "C" {
#endif

size_t coin_utf8_validate_length(const char* str) {
  if (!str) return 0;
  try {
    return utf8::length(str);
  } catch (const utf8::exception&) {
    return 0;
  }
}

uint32_t coin_utf8_get_char(const char* str) {
  if (!str || !*str) return 0;
  try {
    return utf8::rune(str);
  } catch (const utf8::exception&) {
    return 0;
  }
}

const char* coin_utf8_next_char(const char* str) {
  if (!str || !*str) return str;
  try {
    const char* p = str;
    utf8::next(p);
    return p;
  } catch (const utf8::exception&) {
    return str + 1; // Skip invalid byte
  }
}

size_t coin_utf8_decode(const char* src, size_t srclen, uint32_t* value) {
  if (!src || !value || srclen == 0) {
    if (value) *value = 0;
    return 0;
  }
  
  try {
    *value = utf8::rune(src);
    // Calculate bytes consumed
    const char* next = src;
    utf8::next(next);
    return next - src;
  } catch (const utf8::exception&) {
    if (value) *value = 0;
    return 0;
  }
}

size_t coin_utf8_encode(char* buffer, size_t buflen, uint32_t value) {
  if (!buffer || buflen == 0) return 0;
  
  try {
    std::string encoded = utf8::narrow(static_cast<char32_t>(value));
    if (encoded.length() <= buflen) {
      memcpy(buffer, encoded.c_str(), encoded.length());
      return encoded.length();
    }
  } catch (const utf8::exception&) {
    // Fall through to error case
  }
  
  return 0;
}

#ifdef __cplusplus
}
#endif
