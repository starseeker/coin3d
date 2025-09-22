#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <string>
#include <cstdint>

// Test the Coin UTF-8 functions
extern "C" {
  size_t cc_string_utf8_decode(const char * src, size_t srclen, uint32_t * value);
  size_t cc_string_utf8_encode(char * buffer, size_t buflen, uint32_t value);
  uint32_t cc_string_utf8_get_char(const char * str);
  const char * cc_string_utf8_next_char(const char * str);
  size_t cc_string_utf8_validate_length(const char * str);
}

TEST_CASE("UTF-8 decode function", "[utf8][decode]") {
    uint32_t value;
    size_t result;
    
    SECTION("ASCII characters") {
        result = cc_string_utf8_decode("H", 1, &value);
        REQUIRE(result == 1);
        REQUIRE(value == 0x48);
    }
    
    SECTION("Two-byte UTF-8 characters") {
        // é in UTF-8 is 0xC3 0xA9
        result = cc_string_utf8_decode("\xC3\xA9", 2, &value);
        REQUIRE(result == 2);
        REQUIRE(value == 0xE9);
    }
    
    SECTION("Three-byte UTF-8 characters") {
        // 日 in UTF-8 is 0xE6 0x97 0xA5
        result = cc_string_utf8_decode("\xE6\x97\xA5", 3, &value);
        REQUIRE(result == 3);
        REQUIRE(value == 0x65E5);
    }
    
    SECTION("Four-byte UTF-8 characters") {
        // 🙂 in UTF-8 is 0xF0 0x9F 0x99 0x82
        result = cc_string_utf8_decode("\xF0\x9F\x99\x82", 4, &value);
        REQUIRE(result == 4);
        REQUIRE(value == 0x1F642);
    }
    
    SECTION("Invalid UTF-8 sequences") {
        // Overlong encoding
        result = cc_string_utf8_decode("\xC0\x80", 2, &value);
        REQUIRE(result == 0);
        
        // Invalid start byte
        result = cc_string_utf8_decode("\x80", 1, &value);
        REQUIRE(result == 0);
    }
}

TEST_CASE("UTF-8 encode function", "[utf8][encode]") {
    char buffer[5];
    size_t result;
    
    SECTION("ASCII characters") {
        result = cc_string_utf8_encode(buffer, 5, 0x48);
        REQUIRE(result == 1);
        REQUIRE(buffer[0] == 'H');
    }
    
    SECTION("Two-byte UTF-8 characters") {
        result = cc_string_utf8_encode(buffer, 5, 0xE9);
        REQUIRE(result == 2);
        REQUIRE(buffer[0] == '\xC3');
        REQUIRE(buffer[1] == '\xA9');
    }
    
    SECTION("Three-byte UTF-8 characters") {
        result = cc_string_utf8_encode(buffer, 5, 0x65E5);
        REQUIRE(result == 3);
        REQUIRE(buffer[0] == '\xE6');
        REQUIRE(buffer[1] == '\x97');
        REQUIRE(buffer[2] == '\xA5');
    }
    
    SECTION("Four-byte UTF-8 characters") {
        result = cc_string_utf8_encode(buffer, 5, 0x1F642);
        REQUIRE(result == 4);
        REQUIRE(buffer[0] == '\xF0');
        REQUIRE(buffer[1] == '\x9F');
        REQUIRE(buffer[2] == '\x99');
        REQUIRE(buffer[3] == '\x82');
    }
    
    SECTION("Buffer too small") {
        result = cc_string_utf8_encode(buffer, 1, 0xE9);
        REQUIRE(result == 0);
    }
}

TEST_CASE("UTF-8 get char function", "[utf8][get_char]") {
    uint32_t result;
    
    SECTION("ASCII string") {
        result = cc_string_utf8_get_char("Hello");
        REQUIRE(result == 0x48); // 'H'
    }
    
    SECTION("UTF-8 string with multibyte character") {
        result = cc_string_utf8_get_char("café");
        REQUIRE(result == 0x63); // 'c'
    }
    
    SECTION("Japanese character") {
        result = cc_string_utf8_get_char("日本語");
        REQUIRE(result == 0x65E5); // 日
    }
    
    SECTION("Emoji") {
        result = cc_string_utf8_get_char("🙂");
        REQUIRE(result == 0x1F642); // 🙂
    }
}

TEST_CASE("UTF-8 next char function", "[utf8][next_char]") {
    const char* result;
    
    SECTION("ASCII string") {
        result = cc_string_utf8_next_char("Hello");
        REQUIRE(result - "Hello" == 1);
    }
    
    SECTION("UTF-8 string with multibyte character") {
        result = cc_string_utf8_next_char("café");
        REQUIRE(result - "café" == 1); // 'c' is single byte
        
        // Skip to the 'é' character
        result = cc_string_utf8_next_char("café" + 3);
        REQUIRE(result - ("café" + 3) == 2); // 'é' is two bytes
    }
    
    SECTION("Japanese character") {
        result = cc_string_utf8_next_char("日本語");
        REQUIRE(result - "日本語" == 3); // 日 is 3 bytes
    }
    
    SECTION("Emoji") {
        result = cc_string_utf8_next_char("🙂");
        REQUIRE(result - "🙂" == 4); // 🙂 is 4 bytes
    }
}

TEST_CASE("UTF-8 validate length function", "[utf8][validate_length]") {
    size_t result;
    
    SECTION("ASCII string") {
        result = cc_string_utf8_validate_length("Hello");
        REQUIRE(result == 5);
    }
    
    SECTION("Mixed ASCII and UTF-8 string") {
        result = cc_string_utf8_validate_length("café");
        REQUIRE(result == 4); // 4 characters: c, a, f, é
    }
    
    SECTION("Japanese string") {
        result = cc_string_utf8_validate_length("日本語");
        REQUIRE(result == 3); // 3 characters: 日, 本, 語
    }
    
    SECTION("Emoji string") {
        result = cc_string_utf8_validate_length("🙂");
        REQUIRE(result == 1); // 1 character: 🙂
    }
    
    SECTION("Mixed string with emoji") {
        result = cc_string_utf8_validate_length("Hello 🙂");
        REQUIRE(result == 7); // 6 ASCII chars + 1 emoji
    }
}

TEST_CASE("UTF-8 roundtrip encoding/decoding", "[utf8][roundtrip]") {
    uint32_t test_codepoints[] = {
        0x48,      // ASCII 'H'
        0xE9,      // Latin-1 supplement 'é'
        0x65E5,    // CJK unified ideograph '日'
        0x1F642    // Emoji '🙂'
    };
    
    for (auto codepoint : test_codepoints) {
        SECTION("Roundtrip test for codepoint " + std::to_string(codepoint)) {
            char buffer[5] = {0};
            uint32_t decoded_value;
            
            // Encode
            size_t encoded_len = cc_string_utf8_encode(buffer, 5, codepoint);
            REQUIRE(encoded_len > 0);
            
            // Decode
            size_t decoded_len = cc_string_utf8_decode(buffer, encoded_len, &decoded_value);
            REQUIRE(decoded_len == encoded_len);
            REQUIRE(decoded_value == codepoint);
        }
    }
}