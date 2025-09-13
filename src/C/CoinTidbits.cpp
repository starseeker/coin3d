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
 * \file CoinTidbits.cpp
 * \brief C++17 implementation for C API compatibility functions
 * 
 * This file provides the actual implementation for the C API compatibility
 * layer, allowing the header to contain only declarations to avoid
 * circular inclusion problems.
 */

#include "C/CoinTidbits.h"
#include "misc/CoinUtilities.h"
#include "base/CoinBasic.h"
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#include <mutex>
#include <cassert>

// Forward declare the enum from tidbitsp.h to avoid circular includes
enum coin_atexit_priorities {
  CC_ATEXIT_EXTERNAL = 2147483647,
  CC_ATEXIT_NORMAL = 0,
  CC_ATEXIT_DYNLIBS = -2147483647,
  CC_ATEXIT_REALTIME_FIELD = 10,
  CC_ATEXIT_DRAGGERDEFAULTS = 2,
  CC_ATEXIT_TRACK_SOBASE_INSTANCES = 1,
  CC_ATEXIT_NORMAL_LOWPRIORITY = -1,
  CC_ATEXIT_STATIC_DATA = -10,
  CC_ATEXIT_SODB = -20,
  CC_ATEXIT_SOBASE = -30,
  CC_ATEXIT_SOTYPE = -40,
  CC_ATEXIT_FONT_SUBSYSTEM = -100,
  CC_ATEXIT_FONT_SUBSYSTEM_HIGHPRIORITY = -99,
  CC_ATEXIT_FONT_SUBSYSTEM_LOWPRIORITY = -101,
  CC_ATEXIT_MSG_SUBSYSTEM = -200,
  CC_ATEXIT_SBNAME = -500,
  CC_ATEXIT_THREADING_SUBSYSTEM = -1000,
  CC_ATEXIT_THREADING_SUBSYSTEM_LOWPRIORITY = -1001,
  CC_ATEXIT_THREADING_SUBSYSTEM_VERYLOWPRIORITY = -1002,
  CC_ATEXIT_ENVIRONMENT = -2147483637
};

// ===== C API Implementation Functions =====

extern "C" {

int coin_host_get_endianness(void) {
    // Runtime endianness detection
    union {
        std::uint32_t i;
        char c[4];
    } test = {0x01020304};
    
    return (test.c[0] == 1) ? 1 : 0;  // 1 = big endian, 0 = little endian
}

// Helper function to check if system is big endian
static bool is_big_endian() {
    return coin_host_get_endianness() == 1;
}

// Byte swapping macros
static std::uint16_t bswap_16(std::uint16_t x) {
    return (x << 8) | (x >> 8);
}

static std::uint32_t bswap_32(std::uint32_t x) {
    return ((x & 0x000000FF) << 24) |
           ((x & 0x0000FF00) << 8)  |
           ((x & 0x00FF0000) >> 8)  |
           ((x & 0xFF000000) >> 24);
}

static std::uint64_t bswap_64(std::uint64_t x) {
    return ((x & 0x00000000000000FFULL) << 56) |
           ((x & 0x000000000000FF00ULL) << 40) |
           ((x & 0x0000000000FF0000ULL) << 24) |
           ((x & 0x00000000FF000000ULL) << 8)  |
           ((x & 0x000000FF00000000ULL) >> 8)  |
           ((x & 0x0000FF0000000000ULL) >> 24) |
           ((x & 0x00FF000000000000ULL) >> 40) |
           ((x & 0xFF00000000000000ULL) >> 56);
}

int coin_snprintf(char* dst, unsigned int n, const char* fmtstr, ...) {
    va_list args;
    va_start(args, fmtstr);
    int result = coin_vsnprintf(dst, n, fmtstr, args);
    va_end(args);
    return result;
}

int coin_vsnprintf(char* dst, unsigned int n, const char* fmtstr, va_list args) {
    if (n == 0) return -1;
    
    int result = std::vsnprintf(dst, n, fmtstr, args);
    
    // Ensure null termination
    if (result >= 0 && static_cast<unsigned int>(result) >= n) {
        dst[n - 1] = '\0';
        return -1; // Indicate truncation
    }
    
    return result;
}

const char* coin_getenv(const char* name) {
    return std::getenv(name);
}

SbBool coin_setenv(const char* name, const char* value, int overwrite) {
    if (!name || !value) return false;
    
#ifdef _WIN32
    // Windows doesn't have setenv, use _putenv_s
    if (!overwrite && std::getenv(name)) return true;
    return _putenv_s(name, value) == 0;
#else
    return setenv(name, value, overwrite) == 0;
#endif
}

void coin_unsetenv(const char* name) {
    if (!name) return;
    
#ifdef _WIN32
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

std::uint16_t coin_hton_uint16(std::uint16_t value) {
    return is_big_endian() ? value : bswap_16(value);
}

std::uint16_t coin_ntoh_uint16(std::uint16_t value) {
    return coin_hton_uint16(value);
}

std::uint32_t coin_hton_uint32(std::uint32_t value) {
    return is_big_endian() ? value : bswap_32(value);
}

std::uint32_t coin_ntoh_uint32(std::uint32_t value) {
    return coin_hton_uint32(value);
}

std::uint64_t coin_hton_uint64(std::uint64_t value) {
    return is_big_endian() ? value : bswap_64(value);
}

std::uint64_t coin_ntoh_uint64(std::uint64_t value) {
    return coin_hton_uint64(value);
}

void coin_hton_float_bytes(float value, char* result) {
    static_assert(sizeof(float) == 4, "Float must be 4 bytes");
    std::uint32_t int_value;
    memcpy(&int_value, &value, sizeof(float));
    int_value = coin_hton_uint32(int_value);
    memcpy(result, &int_value, sizeof(float));
}

float coin_ntoh_float_bytes(const char* value) {
    static_assert(sizeof(float) == 4, "Float must be 4 bytes");
    std::uint32_t int_value;
    memcpy(&int_value, value, sizeof(float));
    int_value = coin_ntoh_uint32(int_value);
    float result;
    memcpy(&result, &int_value, sizeof(float));
    return result;
}

void coin_hton_double_bytes(double value, char* result) {
    static_assert(sizeof(double) == 8, "Double must be 8 bytes");
    std::uint64_t int_value;
    memcpy(&int_value, &value, sizeof(double));
    int_value = coin_hton_uint64(int_value);
    memcpy(result, &int_value, sizeof(double));
}

double coin_ntoh_double_bytes(const char* value) {
    static_assert(sizeof(double) == 8, "Double must be 8 bytes");
    std::uint64_t int_value;
    memcpy(&int_value, value, sizeof(double));
    int_value = coin_ntoh_uint64(int_value);
    double result;
    memcpy(&result, &int_value, sizeof(double));
    return result;
}

SbBool coin_is_power_of_two(std::uint32_t x) {
    // Direct implementation of power of two check
    return (x != 0) && ((x & (x - 1)) == 0);
}

std::uint32_t coin_next_power_of_two(std::uint32_t x) {
    // Direct implementation of next power of two
    if (x == 0) return 1;
    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return ++x;
}

std::uint32_t coin_geq_power_of_two(std::uint32_t x) {
    // Direct implementation of geq power of two
    return coin_is_power_of_two(x) ? x : coin_next_power_of_two(x);
}

void coin_viewvolume_jitter(int numpasses, int curpass, const int* vpsize, float* jitter) {
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

// ===== Priority-based atexit system =====

struct tb_atexit_data {
    char* name;
    coin_atexit_f* func;
    std::int32_t priority;
    std::uint32_t cnt;
};

static std::vector<tb_atexit_data*> atexit_list;
static bool isexiting = false;
static std::mutex atexit_list_mutex;

static int atexit_qsort_cb(const void* q0, const void* q1) {
    tb_atexit_data* p0 = *((tb_atexit_data**)q0);
    tb_atexit_data* p1 = *((tb_atexit_data**)q1);

    /* sort list on ascending priorities, so that high priority
       callbacks are called first  */
    if (p0->priority < p1->priority) return -1;
    if (p0->priority > p1->priority) return 1;

    /* when priority is equal, use LIFO */
    if (p0->cnt < p1->cnt) return -1;
    return 1;
}

void coin_atexit_cleanup(void) {
    if (atexit_list.empty()) return;

    isexiting = true;

    const char* debugstr = coin_getenv("COIN_DEBUG_CLEANUP");
    bool debug = debugstr && (std::atoi(debugstr) > 0);

    // Sort the atexit list by priority
    std::sort(atexit_list.begin(), atexit_list.end(), [](tb_atexit_data* a, tb_atexit_data* b) {
        if (a->priority != b->priority) {
            return a->priority < b->priority;  // Lower priority values come first
        }
        return a->cnt < b->cnt;  // LIFO for same priority
    });

    // Call cleanup functions in reverse order (high priority first)
    for (auto it = atexit_list.rbegin(); it != atexit_list.rend(); ++it) {
        tb_atexit_data* data = *it;
        if (debug) {
            std::fprintf(stdout, "coin_atexit_cleanup: invoking %s()\n", data->name);
        }
        data->func();
        std::free(data->name);
        std::free(data);
    }

    atexit_list.clear();
    isexiting = false;

    if (debug) {
        std::fprintf(stdout, "coin_atexit_cleanup: fini\n");
    }
}

void coin_atexit_func(const char* name, coin_atexit_f* func, coin_atexit_priorities priority) {
    std::lock_guard<std::mutex> lock(atexit_list_mutex);
    
    assert(!isexiting && "tried to attach an atexit function while exiting");

    tb_atexit_data* data = (tb_atexit_data*)std::malloc(sizeof(tb_atexit_data));
    data->name = strdup(name);
    data->func = func;
    data->priority = priority;
    data->cnt = static_cast<std::uint32_t>(atexit_list.size());

    atexit_list.push_back(data);
}

SbBool coin_is_exiting(void) {
    return isexiting;
}

void cc_coin_atexit(coin_atexit_f* fp) {
    if (fp) {
        coin_atexit_func("cc_coin_atexit", fp, CC_ATEXIT_EXTERNAL);
    }
}

void cc_coin_atexit_static_internal(coin_atexit_f* fp) {
    if (fp) {
        coin_atexit_func("cc_coin_atexit_static_internal", fp, CC_ATEXIT_STATIC_DATA);
    }
}

// COIN_DEBUG is typically defined for debug builds
#ifndef COIN_DEBUG
#if !defined(NDEBUG) || defined(_DEBUG) || defined(DEBUG)
#define COIN_DEBUG 1
#else
#define COIN_DEBUG 0
#endif
#endif

int coin_debug_caching_level(void) {
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

int coin_debug_extra(void) {
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

int coin_debug_normalize(void) {
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

} // extern "C"

// ===== Additional internal functions that were in tidbits.cpp =====
// These are not part of the public API but are used internally

extern "C" {

// Simple implementations for missing internal functions
void coin_init_tidbits(void) {
    // Initialize tidbits - now mostly empty since we use standard C++17
}

// The atexit functions are now implemented above with priority support

// Simple implementations for file descriptor functions
FILE* coin_get_stdin(void) {
    return stdin;
}

FILE* coin_get_stdout(void) {
    return stdout;
}

FILE* coin_get_stderr(void) {
    return stderr;
}

// Simple implementations for missing utility functions
int coin_finite(double value) {
    return std::isfinite(value);
}

int coin_runtime_os(void) {
#if defined(__APPLE__)
    return 1; // COIN_OS_X
#elif defined(_WIN32)
    return 2; // COIN_MSWINDOWS  
#else
    return 0; // COIN_UNIX
#endif
}

unsigned long coin_geq_prime_number(unsigned long num) {
    // Simple prime number table for basic functionality
    static const unsigned long primes[] = {
        2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71,
        73, 79, 83, 89, 97, 101, 103, 107, 109, 113, 127, 131, 137, 139, 149, 151,
        157, 163, 167, 173, 179, 181, 191, 193, 197, 199, 211, 223, 227, 229, 233,
        239, 241, 251, 257, 263, 269, 271, 277, 281, 283, 293, 307, 311, 313, 317,
        331, 337, 347, 349, 353, 359, 367, 373, 379, 383, 389, 397, 401, 409, 419,
        421, 431, 433, 439, 443, 449, 457, 461, 463, 467, 479, 487, 491, 499, 503,
        509, 521, 523, 541, 547, 557, 563, 569, 571, 577, 587, 593, 599, 601, 607,
        613, 617, 619, 631, 641, 643, 647, 653, 659, 661, 673, 677, 683, 691, 701,
        709, 719, 727, 733, 739, 743, 751, 757, 761, 769, 773, 787, 797, 809, 811,
        821, 823, 827, 829, 839, 853, 857, 859, 863, 877, 881, 883, 887, 907, 911,
        919, 929, 937, 941, 947, 953, 967, 971, 977, 983, 991, 997, 1009, 1013,
        1019, 1021, 1031, 1033, 1039, 1049, 1051, 1061, 1063, 1069, 1087, 1091,
        1093, 1097, 1103, 1109, 1117, 1123, 1129, 1151, 1153, 1163, 1171, 1181,
        1187, 1193, 1201, 1213, 1217, 1223, 1229, 1231, 1237, 1249, 1259, 1277,
        4294967291UL // Large prime for hash tables
    };
    
    for (size_t i = 0; i < sizeof(primes) / sizeof(primes[0]); ++i) {
        if (primes[i] >= num) {
            return primes[i];
        }
    }
    return num; // Fallback
}

// Placeholder implementations for locale and other functions
struct cc_string; // Forward declaration

SbBool coin_locale_set_portable(struct cc_string* storeold) {
    // Simplified - return false meaning no change needed
    return false;
}

void coin_locale_reset(struct cc_string* storedold) {
    // Simplified - do nothing
}

double coin_atof(const char* ptr) {
    // Use standard atof which should be equivalent for most uses
    return std::atof(ptr);
}

SbBool coin_getcwd(struct cc_string* str) {
    // Simplified - return false for now
    return false;
}

int coin_isinf(double value) {
    return std::isinf(value);
}

int coin_isnan(double value) {
    return std::isnan(value);
}

SbBool coin_parse_versionstring(const char* versionstr, int* major, int* minor, int* patch) {
    if (!versionstr) return false;
    
    // Simple version string parsing
    int maj = 0, min = 0, pat = 0;
    int parsed = std::sscanf(versionstr, "%d.%d.%d", &maj, &min, &pat);
    
    if (major) *major = maj;
    if (minor) *minor = (parsed >= 2) ? min : 0;
    if (patch) *patch = (parsed >= 3) ? pat : 0;
    
    return parsed >= 1;
}

// Placeholder implementations for ASCII85 functions  
void coin_output_ascii85(FILE* fp, const unsigned char val, unsigned char* tuple,
                         unsigned char* linebuf, int* tuplecnt, int* linecnt,
                         const int rowlen, const bool flush) {
    // Simplified placeholder
}

void coin_flush_ascii85(FILE* fp, unsigned char* tuple, unsigned char* linebuf,
                        int* tuplecnt, int* linecnt, const int rowlen) {
    // Simplified placeholder
}

} // extern "C"