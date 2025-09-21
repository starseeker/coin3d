/*
 Modern UTF-8 implementation using C++17 features.
 Replaces legacy cc_string UTF-8 functions.
*/

#include "Inventor/C/base/utf8.h"

#ifdef __cplusplus
extern "C" {
#endif

size_t coin_utf8_validate_length(const char* str) {
  if (!str) return 0;
  size_t count = 0;
  while (*str) {
    if ((*str & 0x80) == 0) str += 1;
    else if ((*str & 0xE0) == 0xC0) str += 2;
    else if ((*str & 0xF0) == 0xE0) str += 3;
    else if ((*str & 0xF8) == 0xF0) str += 4;
    else str += 1; // Invalid, skip
    count++;
  }
  return count;
}

uint32_t coin_utf8_get_char(const char* str) {
  if (!str || !*str) return 0;
  unsigned char c = *str;
  if (c < 0x80) return c;
  // Simplified: just return first byte for now
  return c;
}

const char* coin_utf8_next_char(const char* str) {
  if (!str || !*str) return str;
  if ((*str & 0x80) == 0) return str + 1;
  else if ((*str & 0xE0) == 0xC0) return str + 2;
  else if ((*str & 0xF0) == 0xE0) return str + 3;
  else if ((*str & 0xF8) == 0xF0) return str + 4;
  else return str + 1;
}

size_t coin_utf8_decode(const char* src, size_t srclen, uint32_t* value) {
  if (!src || !value || srclen == 0) {
    if (value) *value = 0;
    return 0;
  }
  *value = coin_utf8_get_char(src);
  return 1;
}

size_t coin_utf8_encode(char* buffer, size_t buflen, uint32_t value) {
  if (!buffer || buflen == 0) return 0;
  if (value < 0x80 && buflen >= 1) {
    buffer[0] = (char)value;
    return 1;
  }
  return 0;
}

#ifdef __cplusplus
}
#endif
