/*
 Modern UTF-8 implementation using standard C++17 functionality.
 Replaces legacy cc_string UTF-8 functions with proper Unicode support.
*/

#include "Inventor/C/base/utf8.h"
#include <cstring>
#include <locale>
#include <codecvt>

#ifdef __cplusplus
extern "C" {
#endif

size_t coin_utf8_validate_length(const char* str) {
  if (!str) return 0;
  try {
    // Simple validation by checking if conversion works
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
    auto u32str = converter.from_bytes(str);
    return u32str.length();
  } catch (...) {
    return std::strlen(str); // Fallback to byte length
  }
}

uint32_t coin_utf8_get_char(const char* str) {
  if (!str || !*str) return 0;
  try {
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
    auto u32str = converter.from_bytes(str, str + std::min(4u, (unsigned)std::strlen(str)));
    return u32str.empty() ? 0 : static_cast<uint32_t>(u32str[0]);
  } catch (...) {
    return static_cast<uint32_t>(*str); // Fallback to raw byte
  }
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
  
  try {
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
    std::string input(src, std::min(srclen, (size_t)4));
    auto u32str = converter.from_bytes(input);
    if (!u32str.empty()) {
      *value = static_cast<uint32_t>(u32str[0]);
      // Return number of bytes consumed
      const char* next = coin_utf8_next_char(src);
      return next - src;
    }
  } catch (...) {
    // Fallback
  }
  
  *value = static_cast<uint32_t>(*src);
  return 1;
}

size_t coin_utf8_encode(char* buffer, size_t buflen, uint32_t value) {
  if (!buffer || buflen == 0) return 0;
  
  try {
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
    std::u32string u32str(1, static_cast<char32_t>(value));
    std::string encoded = converter.to_bytes(u32str);
    if (encoded.length() <= buflen) {
      memcpy(buffer, encoded.c_str(), encoded.length());
      return encoded.length();
    }
  } catch (...) {
    // Fall through to error case
  }
  
  return 0;
}

#ifdef __cplusplus
}
#endif
