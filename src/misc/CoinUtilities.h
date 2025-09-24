#ifndef COIN_UTILITIES_H
#define COIN_UTILITIES_H

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
 * \file CoinUtilities.h
 * \brief Modern C++17 utilities replacing legacy C API functions
 * 
 * This file provides C++17-based replacements for various legacy C utility
 * functions, using standard library features and modern C++ practices.
 */

#include <cstdint>
#include <string>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <type_traits>

// Internal namespace for Coin3D implementation details
namespace CoinInternal {

/*!
 * \brief Endianness enumeration for C++17 compatibility
 * 
 * Since std::endian is C++20, we provide a C++17 compatible version.
 */
enum class Endian {
    little = 0,
    big = 1,
    native = little  // Assume little endian as default for modern systems
};

/*!
 * \brief Get the native endianness of the system
 * 
 * C++17 compatible replacement for coin_host_get_endianness().
 * Since constexpr union access is complex, we use a runtime approach.
 * 
 * \return Endian enumeration value
 */
inline Endian getEndianness() noexcept {
    // Runtime endianness detection using a union
    union {
        std::uint32_t i;
        char c[4];
    } test = {0x01020304};
    
    return (test.c[0] == 1) ? Endian::big : Endian::little;
}

/*!
 * \brief Check if the system is big endian
 * 
 * \return true if big endian, false if little endian
 */
inline bool isBigEndian() noexcept {
    return getEndianness() == Endian::big;
}

/*!
 * \brief Check if the system is little endian
 * 
 * \return true if little endian, false if big endian
 */
inline bool isLittleEndian() noexcept {
    return getEndianness() == Endian::little;
}

/*!
 * \brief Safe sprintf replacement using C++17
 * 
 * Modern C++17 sprintf replacement that uses std::snprintf with
 * proper bounds checking.
 * 
 * \param buffer Output buffer
 * \param size Buffer size
 * \param format Format string
 * \param args Variadic arguments
 * \return Number of characters written, or negative on error
 */
template<typename... Args>
inline int safeSprintf(char* buffer, std::size_t size, const char* format, Args&&... args) {
    if (size == 0) return -1;
    
    int result = std::snprintf(buffer, size, format, std::forward<Args>(args)...);
    
    // Ensure null termination
    if (result >= 0 && static_cast<std::size_t>(result) >= size) {
        buffer[size - 1] = '\0';
        return -1; // Indicate truncation
    }
    
    return result;
}

/*!
 * \brief Safe string formatting to std::string
 * 
 * Modern C++17 string formatting that returns a std::string instead of
 * requiring manual buffer management.
 * 
 * \param format Format string
 * \param args Variadic arguments
 * \return Formatted string
 */
template<typename... Args>
inline std::string formatString(const char* format, Args&&... args) {
    // First, determine the required size
    int size = std::snprintf(nullptr, 0, format, std::forward<Args>(args)...);
    if (size <= 0) return {};
    
    // Allocate and format
    std::string result(size + 1, '\0');
    std::snprintf(&result[0], result.size(), format, std::forward<Args>(args)...);
    result.resize(size); // Remove the extra null terminator
    
    return result;
}

/*!
 * \brief C++17 replacement for coin_atexit functionality
 * 
 * Modern replacement for cc_coin_atexit using standard library features.
 * This provides both regular and static cleanup functionality.
 */
namespace AtExit {
    using cleanup_function = void(*)(void);
    
    /*!
     * \brief Register a function to be called at exit
     * 
     * C++17 replacement for cc_coin_atexit(). This is a simple wrapper
     * around std::atexit with error handling.
     * 
     * \param func Function to call at exit
     * \return true if successfully registered, false otherwise
     */
    inline bool registerCleanup(cleanup_function func) {
        return std::atexit(func) == 0;
    }
    
    /*!
     * \brief Register internal static cleanup function
     * 
     * C++17 replacement for cc_coin_atexit_static_internal().
     * For internal use only - registers cleanup functions for static data.
     * 
     * \param func Function to call for static cleanup
     * \return true if successfully registered, false otherwise
     */
    inline bool registerStaticCleanup(cleanup_function func) {
        // For now, use the same mechanism as regular cleanup
        // In the future, this could be enhanced with priority levels
        return registerCleanup(func);
    }
}

/*!
 * \brief Case-insensitive string comparison for C++17
 * 
 * Modern replacement for coin_strncasecmp that works consistently
 * across platforms.
 * 
 * \param str1 First string
 * \param str2 Second string
 * \param len Maximum number of characters to compare
 * \return Comparison result (< 0, 0, > 0)
 */
inline int stringsCompareNoCase(const char* str1, const char* str2, std::size_t len) {
    for (std::size_t i = 0; i < len; ++i) {
        char c1 = std::tolower(static_cast<unsigned char>(str1[i]));
        char c2 = std::tolower(static_cast<unsigned char>(str2[i]));
        
        if (c1 < c2) return -1;
        if (c1 > c2) return 1;
        if (c1 == '\0') return 0; // Both strings ended
    }
    return 0;
}

/*!
 * \brief Network byte order conversion utilities
 * 
 * Modern C++17 replacements for the coin_hton_* and coin_ntoh_* functions.
 */
namespace ByteOrder {

/*!
 * \brief Convert 16-bit value to network byte order
 */
inline std::uint16_t hostToNetwork(std::uint16_t value) noexcept {
    if (isBigEndian()) {
        return value;
    } else {
        return (value << 8) | (value >> 8);
    }
}

/*!
 * \brief Convert 32-bit value to network byte order
 */
inline std::uint32_t hostToNetwork(std::uint32_t value) noexcept {
    if (isBigEndian()) {
        return value;
    } else {
        return ((value & 0x000000FF) << 24) |
               ((value & 0x0000FF00) << 8)  |
               ((value & 0x00FF0000) >> 8)  |
               ((value & 0xFF000000) >> 24);
    }
}

/*!
 * \brief Convert 64-bit value to network byte order
 */
inline std::uint64_t hostToNetwork(std::uint64_t value) noexcept {
    if (isBigEndian()) {
        return value;
    } else {
        return ((value & 0x00000000000000FFULL) << 56) |
               ((value & 0x000000000000FF00ULL) << 40) |
               ((value & 0x0000000000FF0000ULL) << 24) |
               ((value & 0x00000000FF000000ULL) << 8)  |
               ((value & 0x000000FF00000000ULL) >> 8)  |
               ((value & 0x0000FF0000000000ULL) >> 24) |
               ((value & 0x00FF000000000000ULL) >> 40) |
               ((value & 0xFF00000000000000ULL) >> 56);
    }
}

// Network to host conversion is the same as host to network for these functions
inline std::uint16_t networkToHost(std::uint16_t value) noexcept { return hostToNetwork(value); }
inline std::uint32_t networkToHost(std::uint32_t value) noexcept { return hostToNetwork(value); }
inline std::uint64_t networkToHost(std::uint64_t value) noexcept { return hostToNetwork(value); }

} // namespace ByteOrder

/*!
 * \brief Modern C++17 math utilities
 * 
 * Replacements for coin_finite, coin_isinf, coin_isnan using std:: functions.
 */
namespace MathUtils {

/*!
 * \brief Check if floating point value is finite
 * 
 * Modern C++17 replacement for coin_finite().
 * 
 * \param value Value to check
 * \return true if value is finite (not infinite and not NaN), false otherwise
 */
inline bool isFinite(double value) noexcept {
    return std::isfinite(value);
}

/*!
 * \brief Check if floating point value is infinite
 * 
 * Modern C++17 replacement for coin_isinf().
 * 
 * \param value Value to check
 * \return true if value is infinite, false otherwise
 */
inline bool isInfinite(double value) noexcept {
    return std::isinf(value);
}

/*!
 * \brief Check if floating point value is NaN
 * 
 * Modern C++17 replacement for coin_isnan().
 * 
 * \param value Value to check
 * \return true if value is NaN, false otherwise
 */
inline bool isNaN(double value) noexcept {
    return std::isnan(value);
}

} // namespace MathUtils

} // namespace CoinInternal

#endif // COIN_UTILITIES_H