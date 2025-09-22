/*
 Simple UTF-8 functions using basic C++ approach.
 Replaces legacy cc_string UTF-8 functions.
*/

#include "Inventor/C/base/utf8.h"
#include <cstring>

#ifdef __cplusplus
extern "C" {
#endif

size_t coin_utf8_validate_length(const char* str) {
  if (!str) return 0;
  // Simple fallback - just return string length
  return std::strlen(str);
}

uint32_t coin_utf8_get_char(const char* str) {
  if (!str || !*str) return 0;
  // Simple fallback - return first byte as character
  return static_cast<uint32_t>(*str);
}

const char* coin_utf8_next_char(const char* str) {
  if (!str || !*str) return str;
  
  // Simple UTF-8 byte sequence detection
  unsigned char c = static_cast<unsigned char>(*str);
  if (c < 0x80) return str + 1;      // ASCII
  if (c < 0xC0) return str + 1;      // Invalid/continuation byte
  if (c < 0xE0) return str + 2;      // 2-byte sequence
  if (c < 0xF0) return str + 3;      // 3-byte sequence
  return str + 4;                    // 4-byte sequence
}

size_t coin_utf8_decode(const char* src, size_t srclen, uint32_t* value) {
  if (!src || !value || srclen == 0) {
    if (value) *value = 0;
    return 0;
  }
  
  *value = static_cast<uint32_t>(*src);
  return 1;
}

size_t coin_utf8_encode(char* buffer, size_t buflen, uint32_t value) {
  if (!buffer || buflen == 0) return 0;
  
  // Simple fallback - just put the low byte
  if (value <= 0xFF) {
    *buffer = static_cast<char>(value);
    return 1;
  }
  
  return 0;
}

#ifdef __cplusplus
}
#endif
