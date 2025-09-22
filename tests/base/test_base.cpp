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

#include "utils/test_common.h"
#include <Inventor/SbBox3s.h>
#include <Inventor/SbBox3f.h>
#include <Inventor/SbBSPTree.h>

using namespace CoinTestUtils;

// Tests for SbBox3s class (ported from src/base/SbBox3s.cpp)
TEST_CASE("SbBox3s basic functionality", "[base][SbBox3s]") {
    CoinTestFixture fixture;

    SECTION("checkSize") {
        SbVec3s min(1, 2, 3);
        SbVec3s max(3, 4, 5);
        SbVec3s diff = max - min;
        SbBox3s box(min, max);

        CHECK(box.getSize() == diff);
    }

    SECTION("checkGetClosestPoint") {
        SbVec3f point(1524, 13794, 851);
        SbVec3s min(1557, 3308, 850);
        SbVec3s max(3113, 30157, 1886);

        SbBox3s box(min, max);
        SbVec3f expected(1557, 13794, 851);

        CHECK(box.getClosestPoint(point) == expected);

        SbVec3s sizes = box.getSize();
        SbVec3f expectedCenterQuery(sizes[0]/2.0f, sizes[1]/2.0f, max[2]);

        CHECK(box.getClosestPoint(box.getCenter()) == expectedCenterQuery);
    }
}

// Tests for SbBox3f class (ported from src/base/SbBox3f.cpp)
TEST_CASE("SbBox3f basic functionality", "[base][SbBox3f]") {
    CoinTestFixture fixture;

    SECTION("checkGetClosestPoint") {
        SbVec3f point(1524.0f, 13794.0f, 851.0f);
        SbVec3f min(1557.0f, 3308.0f, 850.0f);
        SbVec3f max(3113.0f, 30157.0f, 1886.0f);

        SbBox3f box(min, max);
        SbVec3f expected(1557.0f, 13794.0f, 851.0f);

        CHECK(box.getClosestPoint(point) == expected);

        SbVec3f sizes = box.getSize();
        SbVec3f expectedCenterQuery(sizes[0]/2.0f, sizes[1]/2.0f, max[2]);

        CHECK(box.getClosestPoint(box.getCenter()) == expectedCenterQuery);
    }
}

// Tests for SbBSPTree class (ported from src/base/SbBSPTree.cpp)
TEST_CASE("SbBSPTree basic functionality", "[base][SbBSPTree]") {
    CoinTestFixture fixture;

    SECTION("initialized") {
        SbBSPTree bsp;
        CHECK(bsp.numPoints() == 0);
    }
}

// Test the Coin UTF-8 functions
extern "C" {
  size_t cc_string_utf8_decode(const char * src, size_t srclen, uint32_t * value);
  size_t cc_string_utf8_encode(char * buffer, size_t buflen, uint32_t value);
  uint32_t cc_string_utf8_get_char(const char * str);
  const char * cc_string_utf8_next_char(const char * str);
  size_t cc_string_utf8_validate_length(const char * str);
}

TEST_CASE("UTF-8 decode function", "[base][utf8][decode]") {
    CoinTestFixture fixture;
    uint32_t value;
    size_t result;
    
    // Check if UTF-8 is disabled via environment variable
    bool utf8_disabled = (getenv("COIN_DISABLE_UTF8") != nullptr);
    
    SECTION("ASCII characters") {
        result = cc_string_utf8_decode("H", 1, &value);
        REQUIRE(result == 1);
        REQUIRE(value == 0x48);
    }
    
    if (!utf8_disabled) {
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
    } else {
        SECTION("UTF-8 disabled - fallback to byte interpretation") {
            // When UTF-8 is disabled, it should decode single bytes
            result = cc_string_utf8_decode("\xC3\xA9", 2, &value);
            REQUIRE(result == 1);
            REQUIRE(value == 0xC3); // First byte only
        }
    }
}

TEST_CASE("UTF-8 encode function", "[base][utf8][encode]") {
    CoinTestFixture fixture;
    char buffer[5];
    size_t result;
    
    // Check if UTF-8 is disabled via environment variable
    bool utf8_disabled = (getenv("COIN_DISABLE_UTF8") != nullptr);
    
    SECTION("ASCII characters") {
        result = cc_string_utf8_encode(buffer, 5, 0x48);
        REQUIRE(result == 1);
        REQUIRE(buffer[0] == 'H');
    }
    
    if (!utf8_disabled) {
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
    } else {
        SECTION("UTF-8 disabled - only ASCII encoding") {
            // When UTF-8 is disabled, non-ASCII should fail
            result = cc_string_utf8_encode(buffer, 5, 0xE9);
            REQUIRE(result == 0); // Should fail for non-ASCII
        }
    }
}

TEST_CASE("UTF-8 get char function", "[base][utf8][get_char]") {
    CoinTestFixture fixture;
    uint32_t result;
    
    // Check if UTF-8 is disabled via environment variable
    bool utf8_disabled = (getenv("COIN_DISABLE_UTF8") != nullptr);
    
    SECTION("ASCII string") {
        result = cc_string_utf8_get_char("Hello");
        REQUIRE(result == 0x48); // 'H'
    }
    
    SECTION("UTF-8 string with multibyte character") {
        result = cc_string_utf8_get_char("café");
        REQUIRE(result == 0x63); // 'c'
    }
    
    if (!utf8_disabled) {
        SECTION("Japanese character") {
            result = cc_string_utf8_get_char("日本語");
            REQUIRE(result == 0x65E5); // 日
        }
        
        SECTION("Emoji") {
            result = cc_string_utf8_get_char("🙂");
            REQUIRE(result == 0x1F642); // 🙂
        }
    } else {
        SECTION("UTF-8 disabled - byte interpretation") {
            result = cc_string_utf8_get_char("日本語");
            REQUIRE(result == 0xE6); // First byte of "日"
            
            result = cc_string_utf8_get_char("🙂");
            REQUIRE(result == 0xF0); // First byte of "🙂"
        }
    }
}

TEST_CASE("UTF-8 next char function", "[base][utf8][next_char]") {
    CoinTestFixture fixture;
    const char* result;
    
    // Check if UTF-8 is disabled via environment variable
    bool utf8_disabled = (getenv("COIN_DISABLE_UTF8") != nullptr);
    
    SECTION("ASCII string") {
        result = cc_string_utf8_next_char("Hello");
        REQUIRE(result - "Hello" == 1);
    }
    
    if (!utf8_disabled) {
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
    } else {
        SECTION("UTF-8 disabled - single byte advancement") {
            result = cc_string_utf8_next_char("café" + 3);
            REQUIRE(result - ("café" + 3) == 1); // Single byte advancement
            
            result = cc_string_utf8_next_char("日本語");
            REQUIRE(result - "日本語" == 1); // Single byte advancement
            
            result = cc_string_utf8_next_char("🙂");
            REQUIRE(result - "🙂" == 1); // Single byte advancement
        }
    }
}

TEST_CASE("UTF-8 validate length function", "[base][utf8][validate_length]") {
    CoinTestFixture fixture;
    size_t result;
    
    // Check if UTF-8 is disabled via environment variable
    bool utf8_disabled = (getenv("COIN_DISABLE_UTF8") != nullptr);
    
    SECTION("ASCII string") {
        result = cc_string_utf8_validate_length("Hello");
        REQUIRE(result == 5);
    }
    
    if (!utf8_disabled) {
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
    } else {
        SECTION("UTF-8 disabled - byte count") {
            result = cc_string_utf8_validate_length("café");
            REQUIRE(result == 5); // 5 bytes: c, a, f, é(2 bytes)
            
            result = cc_string_utf8_validate_length("日本語");
            REQUIRE(result == 9); // 9 bytes: 3 chars × 3 bytes each
            
            result = cc_string_utf8_validate_length("🙂");
            REQUIRE(result == 4); // 4 bytes
            
            result = cc_string_utf8_validate_length("Hello 🙂");
            REQUIRE(result == 10); // 6 ASCII + 4 emoji bytes
        }
    }
}

TEST_CASE("UTF-8 roundtrip encoding/decoding", "[base][utf8][roundtrip]") {
    CoinTestFixture fixture;
    
    // Check if UTF-8 is disabled via environment variable
    bool utf8_disabled = (getenv("COIN_DISABLE_UTF8") != nullptr);
    
    uint32_t ascii_codepoints[] = { 0x48 }; // ASCII 'H'
    uint32_t utf8_codepoints[] = {
        0xE9,      // Latin-1 supplement 'é'
        0x65E5,    // CJK unified ideograph '日'
        0x1F642    // Emoji '🙂'
    };
    
    SECTION("ASCII roundtrip") {
        for (auto codepoint : ascii_codepoints) {
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
    
    if (!utf8_disabled) {
        SECTION("UTF-8 roundtrip") {
            for (auto codepoint : utf8_codepoints) {
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
    } else {
        SECTION("UTF-8 disabled - non-ASCII encoding should fail") {
            for (auto codepoint : utf8_codepoints) {
                char buffer[5] = {0};
                
                // Encode should fail for non-ASCII when UTF-8 is disabled
                size_t encoded_len = cc_string_utf8_encode(buffer, 5, codepoint);
                REQUIRE(encoded_len == 0);
            }
        }
    }
}