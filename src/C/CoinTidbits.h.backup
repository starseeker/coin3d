#ifndef COIN_TIDBITS_INTERNAL_H
#define COIN_TIDBITS_INTERNAL_H

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
 * \file CoinTidbits.h
 * \brief C++17 replacement for Inventor/C/CoinTidbits.h
 * 
 * This file provides C++17-based replacements for the utility functions
 * from the legacy C tidbits API, using standard library features and
 * modern C++ practices.
 */

#include "../base/CoinBasic.h"
#include "../misc/CoinUtilities.h"
#include <cstdlib>
#include <cstring>
#include <cstdarg>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace CoinInternal {

// ===== ATEXIT FUNCTIONALITY =====

/*!
 * \brief Function pointer type for cleanup functions
 * 
 * C++17 equivalent of coin_atexit_f
 */
using cleanup_function_t = void(*)(void);

/*!
 * \brief Register a function to be called at exit
 * 
 * C++17 replacement for cc_coin_atexit()
 */
inline void coin_atexit(cleanup_function_t func) {
    if (func) {
        AtExit::registerCleanup(func);
    }
}

/*!
 * \brief Register internal static cleanup function
 * 
 * C++17 replacement for cc_coin_atexit_static_internal()
 */
inline void coin_atexit_static_internal(cleanup_function_t func) {
    if (func) {
        AtExit::registerStaticCleanup(func);
    }
}

// ===== ENDIANNESS UTILITIES =====

/*!
 * \brief Get host endianness
 * 
 * C++17 replacement for coin_host_get_endianness()
 */
inline int coin_host_get_endianness() {
    return isBigEndian() ? 1 : 0;  // 1 = big endian, 0 = little endian
}

// ===== STRING UTILITIES =====

/*!
 * \brief Safe sprintf replacement
 * 
 * C++17 replacement for coin_snprintf()
 */
template<typename... Args>
inline int coin_snprintf(char* dst, unsigned int n, const char* fmtstr, Args&&... args) {
    return safeSprintf(dst, n, fmtstr, std::forward<Args>(args)...);
}

/*!
 * \brief Safe vsprintf replacement
 * 
 * C++17 replacement for coin_vsnprintf()
 */
inline int coin_vsnprintf(char* dst, unsigned int n, const char* fmtstr, va_list args) {
    if (n == 0) return -1;
    
    int result = std::vsnprintf(dst, n, fmtstr, args);
    
    // Ensure null termination
    if (result >= 0 && static_cast<unsigned int>(result) >= n) {
        dst[n - 1] = '\0';
        return -1; // Indicate truncation
    }
    
    return result;
}

// ===== ENVIRONMENT UTILITIES =====

/*!
 * \brief Get environment variable
 * 
 * C++17 replacement for coin_getenv()
 */
inline const char* coin_getenv(const char* name) {
    return std::getenv(name);
}

/*!
 * \brief Set environment variable
 * 
 * C++17 replacement for coin_setenv()
 */
inline SbBool coin_setenv(const char* name, const char* value, int overwrite) {
    if (!name || !value) return false;
    
#ifdef _WIN32
    // Windows doesn't have setenv, use _putenv_s
    if (!overwrite && std::getenv(name)) return true;
    return _putenv_s(name, value) == 0;
#else
    return setenv(name, value, overwrite) == 0;
#endif
}

/*!
 * \brief Unset environment variable
 * 
 * C++17 replacement for coin_unsetenv()
 */
inline void coin_unsetenv(const char* name) {
    if (!name) return;
    
#ifdef _WIN32
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

// ===== BYTE ORDER UTILITIES =====

/*!
 * \brief Convert 16-bit value to network byte order
 */
inline std::uint16_t coin_hton_uint16(std::uint16_t value) {
    return ByteOrder::hostToNetwork(value);
}

/*!
 * \brief Convert 16-bit value from network byte order
 */
inline std::uint16_t coin_ntoh_uint16(std::uint16_t value) {
    return ByteOrder::networkToHost(value);
}

/*!
 * \brief Convert 32-bit value to network byte order
 */
inline std::uint32_t coin_hton_uint32(std::uint32_t value) {
    return ByteOrder::hostToNetwork(value);
}

/*!
 * \brief Convert 32-bit value from network byte order
 */
inline std::uint32_t coin_ntoh_uint32(std::uint32_t value) {
    return ByteOrder::networkToHost(value);
}

/*!
 * \brief Convert 64-bit value to network byte order
 */
inline std::uint64_t coin_hton_uint64(std::uint64_t value) {
    return ByteOrder::hostToNetwork(value);
}

/*!
 * \brief Convert 64-bit value from network byte order
 */
inline std::uint64_t coin_ntoh_uint64(std::uint64_t value) {
    return ByteOrder::networkToHost(value);
}

// ===== FLOAT/DOUBLE BYTE ORDER UTILITIES =====

/*!
 * \brief Convert float to network byte order bytes
 */
inline void coin_hton_float_bytes(float value, char* result) {
    static_assert(sizeof(float) == 4, "Float must be 4 bytes");
    std::uint32_t int_value;
    std::memcpy(&int_value, &value, sizeof(float));
    int_value = coin_hton_uint32(int_value);
    std::memcpy(result, &int_value, sizeof(float));
}

/*!
 * \brief Convert network byte order bytes to float
 */
inline float coin_ntoh_float_bytes(const char* value) {
    static_assert(sizeof(float) == 4, "Float must be 4 bytes");
    std::uint32_t int_value;
    std::memcpy(&int_value, value, sizeof(float));
    int_value = coin_ntoh_uint32(int_value);
    float result;
    std::memcpy(&result, &int_value, sizeof(float));
    return result;
}

/*!
 * \brief Convert double to network byte order bytes
 */
inline void coin_hton_double_bytes(double value, char* result) {
    static_assert(sizeof(double) == 8, "Double must be 8 bytes");
    std::uint64_t int_value;
    std::memcpy(&int_value, &value, sizeof(double));
    int_value = coin_hton_uint64(int_value);
    std::memcpy(result, &int_value, sizeof(double));
}

/*!
 * \brief Convert network byte order bytes to double
 */
inline double coin_ntoh_double_bytes(const char* value) {
    static_assert(sizeof(double) == 8, "Double must be 8 bytes");
    std::uint64_t int_value;
    std::memcpy(&int_value, value, sizeof(double));
    int_value = coin_ntoh_uint64(int_value);
    double result;
    std::memcpy(&result, &int_value, sizeof(double));
    return result;
}

// ===== MATH UTILITIES =====

/*!
 * \brief Check if a number is a power of two
 */
inline SbBool coin_is_power_of_two(std::uint32_t x) {
    return isPowerOfTwo(x);
}

/*!
 * \brief Get the next power of two >= x
 */
inline std::uint32_t coin_next_power_of_two(std::uint32_t x) {
    return nextPowerOfTwo(x);
}

/*!
 * \brief Get smallest power of two >= x 
 */
inline std::uint32_t coin_geq_power_of_two(std::uint32_t x) {
    return geqPowerOfTwo(x);
}

/*!
 * \brief Generate jitter values for viewport sampling
 * 
 * C++17 replacement for coin_viewvolume_jitter()
 */
inline void coin_viewvolume_jitter(int numpasses, int curpass, const int* vpsize, float* jitter) {
    // Implementation for jitter generation - simplified version
    if (!jitter || !vpsize || numpasses <= 0 || curpass < 0 || curpass >= numpasses) {
        if (jitter) {
            jitter[0] = jitter[1] = 0.0f;
        }
        return;
    }
    
    // Simple jitter pattern - can be enhanced later
    float dx = 1.0f / vpsize[0];
    float dy = 1.0f / vpsize[1];
    
    // Create a simple jitter pattern based on pass number
    jitter[0] = dx * ((curpass % 4) - 1.5f) * 0.5f;
    jitter[1] = dy * ((curpass / 4) - 1.5f) * 0.5f;
}

// ===== DEBUG UTILITIES =====

// COIN_DEBUG is typically defined for debug builds
#ifndef COIN_DEBUG
#if !defined(NDEBUG) || defined(_DEBUG) || defined(DEBUG)
#define COIN_DEBUG 1
#else
#define COIN_DEBUG 0
#endif
#endif

/*!
 * \brief Get debug caching level
 * 
 * C++17 replacement for coin_debug_caching_level()
 */
inline int coin_debug_caching_level() {
#if COIN_DEBUG
    static int debug_level = -1;
    if (debug_level < 0) {
        const char* env = coin_getenv("COIN_DEBUG_CACHING");
        debug_level = env ? std::atoi(env) : 0;
    }
    return debug_level;
#else
    return 0;
#endif
}

/*!
 * \brief Check if extra debugging is enabled
 * 
 * C++17 replacement for coin_debug_extra()
 */
inline int coin_debug_extra() {
#if COIN_DEBUG
    static int debug_extra = -1;
    if (debug_extra < 0) {
        const char* env = coin_getenv("COIN_DEBUG_EXTRA");
        debug_extra = (env && std::atoi(env) == 1) ? 1 : 0;
    }
    return debug_extra;
#else
    return 0;
#endif
}

/*!
 * \brief Check if normalize debugging is enabled
 * 
 * C++17 replacement for coin_debug_normalize()
 */
inline int coin_debug_normalize() {
#if COIN_DEBUG
    static int debug_normalize = -1;
    if (debug_normalize < 0) {
        const char* env = coin_getenv("COIN_DEBUG_NORMALIZE");
        debug_normalize = (env && std::atoi(env) == 1) ? 1 : 0;
    }
    return debug_normalize;
#else
    return 0;
#endif
}

} // namespace CoinInternal

// ===== C API COMPATIBILITY LAYER =====

extern "C" {

// Make sure SbBool and constants are available in global scope
using ::SbBool;

// Legacy C types for compatibility
typedef void coin_atexit_f(void);

// C API functions that wrap the C++ implementation
COIN_DLL_API int coin_host_get_endianness(void);
COIN_DLL_API int coin_snprintf(char* dst, unsigned int n, const char* fmtstr, ...);
COIN_DLL_API int coin_vsnprintf(char* dst, unsigned int n, const char* fmtstr, va_list args);

COIN_DLL_API const char* coin_getenv(const char* name);
COIN_DLL_API SbBool coin_setenv(const char* name, const char* value, int overwrite);
COIN_DLL_API void coin_unsetenv(const char* name);

COIN_DLL_API std::uint16_t coin_hton_uint16(std::uint16_t value);
COIN_DLL_API std::uint16_t coin_ntoh_uint16(std::uint16_t value);
COIN_DLL_API std::uint32_t coin_hton_uint32(std::uint32_t value);
COIN_DLL_API std::uint32_t coin_ntoh_uint32(std::uint32_t value);
COIN_DLL_API std::uint64_t coin_hton_uint64(std::uint64_t value);
COIN_DLL_API std::uint64_t coin_ntoh_uint64(std::uint64_t value);

COIN_DLL_API void coin_hton_float_bytes(float value, char* result);
COIN_DLL_API float coin_ntoh_float_bytes(const char* value);
COIN_DLL_API void coin_hton_double_bytes(double value, char* result);
COIN_DLL_API double coin_ntoh_double_bytes(const char* value);

COIN_DLL_API SbBool coin_is_power_of_two(std::uint32_t x);
COIN_DLL_API std::uint32_t coin_next_power_of_two(std::uint32_t x);
COIN_DLL_API std::uint32_t coin_geq_power_of_two(std::uint32_t x);

COIN_DLL_API void coin_viewvolume_jitter(int numpasses, int curpass, const int* vpsize, float* jitter);

COIN_DLL_API void cc_coin_atexit(coin_atexit_f* fp);
COIN_DLL_API void cc_coin_atexit_static_internal(coin_atexit_f* fp);

COIN_DLL_API int coin_debug_caching_level(void);
COIN_DLL_API int coin_debug_extra(void);
COIN_DLL_API int coin_debug_normalize(void);

} // extern "C"

#endif // COIN_TIDBITS_INTERNAL_H