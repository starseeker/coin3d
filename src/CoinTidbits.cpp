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
 * \brief Consolidated C++17 implementation for all tidbits utilities
 * 
 * This file consolidates all functionality from:
 * - src/tidbits.cpp (legacy implementation)
 * - src/C/CoinTidbits.cpp (previous modern implementation)
 * 
 * Uses modern C++17 features where possible while maintaining 
 * backward compatibility with the C API.
 */

#include "CoinTidbits.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

// Standard C++17 includes
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cfloat>
#include <clocale>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

// Platform-specific includes
#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_IO_H
#include <io.h>
#endif

#ifdef HAVE_IEEEFP_H
#include <ieeefp.h>
#endif

#ifdef HAVE_DIRECT_H
#include <direct.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// Coin includes
#include "C/base/string.h"
#include "base/list.h"
#include "C/errors/debugerror.h"
#include "coindefs.h"

// ===== INTERNAL HELPERS AND GLOBAL STATE =====

namespace {

// Thread-safe global state for cleanup functions
struct CleanupEntry {
    std::string name;
    CoinTidbits::Cleanup::CleanupFunction function;
    CoinTidbits::Cleanup::Priority priority;
    
    CleanupEntry(const char* n, CoinTidbits::Cleanup::CleanupFunction f, CoinTidbits::Cleanup::Priority p)
        : name(n ? n : ""), function(f), priority(p) {}
};

static std::mutex cleanup_mutex;
static std::vector<CleanupEntry> cleanup_functions;
static std::atomic<bool> is_exiting{false};

// Debug state
static std::atomic<int> debug_extra{-1};
static std::atomic<int> debug_normalize{-1};

// Helper to get environment variable safely
const char* getEnvVar(const char* name) {
    return std::getenv(name);
}

// Byte swapping utilities
template<typename T>
T byteSwap(T value) {
    static_assert(sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8, "Unsupported type size");
    
    if constexpr (sizeof(T) == 2) {
        return ((value & 0x00FF) << 8) | ((value & 0xFF00) >> 8);
    } else if constexpr (sizeof(T) == 4) {
        return ((value & 0x000000FF) << 24) |
               ((value & 0x0000FF00) << 8)  |
               ((value & 0x00FF0000) >> 8)  |
               ((value & 0xFF000000) >> 24);
    } else if constexpr (sizeof(T) == 8) {
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

// Prime number generation utility
bool isPrime(unsigned long n) {
    if (n < 2) return false;
    if (n == 2) return true;
    if (n % 2 == 0) return false;
    
    for (unsigned long i = 3; i * i <= n; i += 2) {
        if (n % i == 0) return false;
    }
    return true;
}

} // anonymous namespace

// ===== MODERN C++17 NAMESPACE IMPLEMENTATIONS =====

namespace CoinTidbits {

namespace Endianness {

Type getHostEndianness() noexcept {
    static Type cached = Type::Unknown;
    
    if (cached == Type::Unknown) {
        union {
            std::uint32_t i;
            char c[4];
        } test = {0x01020304};
        
        cached = (test.c[0] == 1) ? Type::Big : Type::Little;
    }
    
    return cached;
}

} // namespace Endianness

namespace ByteOrder {

std::uint16_t hostToNetwork(std::uint16_t value) noexcept {
    return Endianness::isBigEndian() ? value : byteSwap(value);
}

std::uint16_t networkToHost(std::uint16_t value) noexcept {
    return hostToNetwork(value); // Same operation
}

std::uint32_t hostToNetwork(std::uint32_t value) noexcept {
    return Endianness::isBigEndian() ? value : byteSwap(value);
}

std::uint32_t networkToHost(std::uint32_t value) noexcept {
    return hostToNetwork(value); // Same operation
}

std::uint64_t hostToNetwork(std::uint64_t value) noexcept {
    return Endianness::isBigEndian() ? value : byteSwap(value);
}

std::uint64_t networkToHost(std::uint64_t value) noexcept {
    return hostToNetwork(value); // Same operation
}

void hostToNetworkBytes(float value, char* result) noexcept {
    union { float f; std::uint32_t i; } converter;
    converter.f = value;
    std::uint32_t netValue = hostToNetwork(converter.i);
    std::memcpy(result, &netValue, sizeof(netValue));
}

float networkToHostFloatBytes(const char* value) noexcept {
    std::uint32_t netValue;
    std::memcpy(&netValue, value, sizeof(netValue));
    union { float f; std::uint32_t i; } converter;
    converter.i = networkToHost(netValue);
    return converter.f;
}

void hostToNetworkBytes(double value, char* result) noexcept {
    union { double d; std::uint64_t i; } converter;
    converter.d = value;
    std::uint64_t netValue = hostToNetwork(converter.i);
    std::memcpy(result, &netValue, sizeof(netValue));
}

double networkToHostDoubleBytes(const char* value) noexcept {
    std::uint64_t netValue;
    std::memcpy(&netValue, value, sizeof(netValue));
    union { double d; std::uint64_t i; } converter;
    converter.i = networkToHost(netValue);
    return converter.d;
}

} // namespace ByteOrder

namespace StringFormat {

int safeVsnprintf(char* dst, unsigned int n, const char* fmtstr, va_list args) {
    if (n == 0) return -1;
    
    int result = std::vsnprintf(dst, n, fmtstr, args);
    
    // Ensure null termination even on overflow
    if (result >= static_cast<int>(n)) {
        dst[n - 1] = '\0';
        return -1; // Indicate overflow
    }
    
    return result;
}

} // namespace StringFormat

namespace Environment {

std::string getVariable(const std::string& name) {
    const char* value = getEnvVar(name.c_str());
    return value ? std::string(value) : std::string{};
}

const char* getVariableRaw(const char* name) {
    return getEnvVar(name);
}

bool setVariable(const std::string& name, const std::string& value, bool overwrite) {
#ifdef _WIN32
    return SetEnvironmentVariableA(name.c_str(), value.c_str()) != 0;
#else
    return setenv(name.c_str(), value.c_str(), overwrite ? 1 : 0) == 0;
#endif
}

void unsetVariable(const std::string& name) {
#ifdef _WIN32
    SetEnvironmentVariableA(name.c_str(), nullptr);
#else
    unsetenv(name.c_str());
#endif
}

} // namespace Environment

namespace Math {

bool isPowerOfTwo(std::uint32_t x) noexcept {
    return x > 0 && (x & (x - 1)) == 0;
}

std::uint32_t nextPowerOfTwo(std::uint32_t x) noexcept {
    if (x == 0) return 1;
    
    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return ++x;
}

std::uint32_t geqPowerOfTwo(std::uint32_t x) noexcept {
    return isPowerOfTwo(x) ? x : nextPowerOfTwo(x);
}

bool isInfinite(double value) noexcept {
    return std::isinf(value);
}

bool isNaN(double value) noexcept {
    return std::isnan(value);
}

bool isFinite(double value) noexcept {
    return std::isfinite(value);
}

void generateViewvolumeJitter(int numpasses, int curpass, const int* vpsize, float* jitter) {
    // Based on the jitter patterns from the OpenGL Programming Guide
    static const float jitter2[][2] = {
        {0.246490f, 0.249999f}, {-0.246490f, -0.249999f}
    };
    static const float jitter3[][2] = {
        {-0.373411f, -0.250550f}, {0.256263f, 0.368119f}, {0.117148f, -0.117570f}
    };
    static const float jitter4[][2] = {
        {-0.208147f, 0.353730f}, {0.203849f, -0.353780f},
        {-0.292626f, -0.549541f}, {0.296924f, 0.549291f}
    };
    
    jitter[0] = jitter[1] = 0.0f;
    
    if (numpasses <= 1 || curpass >= numpasses) return;
    
    const float (*pattern)[2] = nullptr;
    int maxPasses = 0;
    
    if (numpasses <= 2) {
        pattern = jitter2;
        maxPasses = 2;
    } else if (numpasses <= 3) {
        pattern = jitter3;
        maxPasses = 3;
    } else if (numpasses <= 4) {
        pattern = jitter4;
        maxPasses = 4;
    } else {
        // For higher pass counts, generate pseudo-random jitter
        float x = static_cast<float>(curpass) / numpasses;
        float y = static_cast<float>((curpass * 7) % numpasses) / numpasses;
        jitter[0] = (x - 0.5f) / vpsize[0];
        jitter[1] = (y - 0.5f) / vpsize[1];
        return;
    }
    
    int index = curpass % maxPasses;
    jitter[0] = pattern[index][0] / vpsize[0];
    jitter[1] = pattern[index][1] / vpsize[1];
}

} // namespace Math

namespace Cleanup {

void registerFunction(const char* name, CleanupFunction func, Priority priority) {
    std::lock_guard<std::mutex> lock(cleanup_mutex);
    
    cleanup_functions.emplace_back(name, func, priority);
    
    // Keep functions sorted by priority (highest first)
    std::sort(cleanup_functions.begin(), cleanup_functions.end(),
              [](const CleanupEntry& a, const CleanupEntry& b) {
                  return static_cast<int>(a.priority) > static_cast<int>(b.priority);
              });
}

void registerFunction(CleanupFunction func) {
    registerFunction("", func, Priority::Normal);
}

void cleanup() {
    is_exiting.store(true);
    
    std::lock_guard<std::mutex> lock(cleanup_mutex);
    
    // Execute cleanup functions in priority order
    for (const auto& entry : cleanup_functions) {
        try {
            entry.function();
        } catch (...) {
            // Ignore exceptions during cleanup
        }
    }
    
    cleanup_functions.clear();
}

bool isExiting() {
    return is_exiting.load();
}

} // namespace Cleanup

namespace FileSystem {

std::string getCurrentWorkingDirectory() {
    std::vector<char> buffer(1024);
    
    while (true) {
        const char* result = 
#ifdef _WIN32
            _getcwd(buffer.data(), static_cast<int>(buffer.size()));
#else
            getcwd(buffer.data(), buffer.size());
#endif
        
        if (result) {
            return std::string(buffer.data());
        }
        
        if (errno != ERANGE) {
            return std::string{};
        }
        
        buffer.resize(buffer.size() * 2);
    }
}

FILE* getStdin() noexcept {
    return stdin;
}

FILE* getStdout() noexcept {
    return stdout;
}

FILE* getStderr() noexcept {
    return stderr;
}

} // namespace FileSystem

namespace Platform {

OSType getRuntimeOS() {
#ifdef _WIN32
    return OSType::Windows;
#elif defined(__APPLE__)
    return OSType::MacOSX;
#else
    return OSType::Unix;
#endif
}

} // namespace Platform

namespace Debug {

bool isExtraEnabled() {
#if COIN_DEBUG
    int value = debug_extra.load();
    if (value < 0) {
        const char* env = getEnvVar("COIN_DEBUG_EXTRA");
        value = (env && std::atoi(env) == 1) ? 1 : 0;
        debug_extra.store(value);
    }
    return value == 1;
#else
    return false;
#endif
}

bool isNormalizeEnabled() {
#if COIN_DEBUG
    int value = debug_normalize.load();
    if (value < 0) {
        const char* env = getEnvVar("COIN_DEBUG_NORMALIZE");
        value = (env && std::atoi(env) == 1) ? 1 : 0;
        debug_normalize.store(value);
    }
    return value == 1;
#else
    return false;
#endif
}

int getCachingLevel() {
#if COIN_DEBUG
    static std::atomic<int> debug_caching{-1};
    int value = debug_caching.load();
    if (value < 0) {
        const char* env = getEnvVar("COIN_DEBUG_CACHING");
        value = env ? std::atoi(env) : 0;
        debug_caching.store(value);
    }
    return value;
#else
    return 0;
#endif
}

} // namespace Debug

namespace Misc {

unsigned long getEqualOrGreaterPrime(unsigned long num) {
    if (num <= 2) return 2;
    
    // Make odd if even
    if (num % 2 == 0) ++num;
    
    while (!isPrime(num)) {
        num += 2;
    }
    
    return num;
}

bool parseVersionString(const char* versionstr, int* major, int* minor, int* patch) {
    if (!versionstr || !major) return false;
    
    *major = 0;
    if (minor) *minor = 0;
    if (patch) *patch = 0;
    
    char* endptr;
    *major = static_cast<int>(std::strtol(versionstr, &endptr, 10));
    
    if (endptr == versionstr) return false;
    
    if (*endptr == '.' && minor) {
        const char* minorStart = endptr + 1;
        *minor = static_cast<int>(std::strtol(minorStart, &endptr, 10));
        
        if (*endptr == '.' && patch) {
            const char* patchStart = endptr + 1;
            *patch = static_cast<int>(std::strtol(patchStart, &endptr, 10));
        }
    }
    
    return true;
}

double portableAtof(const char* ptr) {
    if (!ptr) return 0.0;
    
    // Use strtod which is more robust than atof
    char* endptr;
    double result = std::strtod(ptr, &endptr);
    
    return (endptr == ptr) ? 0.0 : result;
}

void outputAscii85(FILE* fp, const unsigned char val, unsigned char* tuple,
                  unsigned char* linebuf, int* tuplecnt, int* linecnt,
                  const int rowlen, const bool flush) {
    // ASCII85 encoding implementation for PostScript output
    tuple[(*tuplecnt)++] = val;
    
    if (*tuplecnt == 4 || flush) {
        std::uint32_t value = 0;
        
        for (int i = 0; i < *tuplecnt; ++i) {
            value = (value << 8) | tuple[i];
        }
        
        // Pad if necessary
        for (int i = *tuplecnt; i < 4; ++i) {
            value <<= 8;
        }
        
        if (value == 0 && *tuplecnt == 4) {
            linebuf[(*linecnt)++] = 'z';
        } else {
            char encoded[5];
            for (int i = 4; i >= 0; --i) {
                encoded[i] = static_cast<char>((value % 85) + 33);
                value /= 85;
            }
            
            int numChars = *tuplecnt + 1;
            for (int i = 0; i < numChars; ++i) {
                linebuf[(*linecnt)++] = encoded[i];
            }
        }
        
        *tuplecnt = 0;
        
        if (*linecnt >= rowlen || flush) {
            linebuf[*linecnt] = '\0';
            fprintf(fp, "%s\n", linebuf);
            *linecnt = 0;
        }
    }
}

void flushAscii85(FILE* fp, unsigned char* tuple, unsigned char* linebuf,
                 int* tuplecnt, int* linecnt, const int rowlen) {
    if (*tuplecnt > 0) {
        outputAscii85(fp, 0, tuple, linebuf, tuplecnt, linecnt, rowlen, true);
    }
    
    if (*linecnt > 0) {
        linebuf[*linecnt] = '\0';
        fprintf(fp, "%s\n", linebuf);
        *linecnt = 0;
    }
    
    fprintf(fp, "~>\n");
}

} // namespace Misc

namespace Locale {

bool setPortable(cc_string* storeold) {
    // Implementation would need access to cc_string internals
    // For now, return false to indicate no change
    (void)storeold;
    return false;
}

void reset(cc_string* storedold) {
    // Implementation would need access to cc_string internals
    (void)storedold;
}

} // namespace Locale

} // namespace CoinTidbits

// ===== C API COMPATIBILITY IMPLEMENTATIONS =====

extern "C" {

// Initialize tidbits subsystem
void coin_init_tidbits(void) {
    // Initialize debug flags
    CoinTidbits::Debug::isExtraEnabled();
    CoinTidbits::Debug::isNormalizeEnabled();
}

// Endianness functions
int coin_host_get_endianness(void) {
    auto endianness = CoinTidbits::Endianness::getHostEndianness();
    return static_cast<int>(endianness);
}

// String formatting functions
int coin_snprintf(char* dst, unsigned int n, const char* fmtstr, ...) {
    va_list args;
    va_start(args, fmtstr);
    int result = CoinTidbits::StringFormat::safeVsnprintf(dst, n, fmtstr, args);
    va_end(args);
    return result;
}

int coin_vsnprintf(char* dst, unsigned int n, const char* fmtstr, va_list args) {
    return CoinTidbits::StringFormat::safeVsnprintf(dst, n, fmtstr, args);
}

// Environment functions
const char* coin_getenv(const char* name) {
    return CoinTidbits::Environment::getVariableRaw(name);
}

SbBool coin_setenv(const char* name, const char* value, int overwrite) {
    return CoinTidbits::Environment::setVariable(name, value, overwrite != 0);
}

void coin_unsetenv(const char* name) {
    CoinTidbits::Environment::unsetVariable(name);
}

// Network byte order functions
uint16_t coin_hton_uint16(uint16_t value) {
    return CoinTidbits::ByteOrder::hostToNetwork(value);
}

uint16_t coin_ntoh_uint16(uint16_t value) {
    return CoinTidbits::ByteOrder::networkToHost(value);
}

uint32_t coin_hton_uint32(uint32_t value) {
    return CoinTidbits::ByteOrder::hostToNetwork(value);
}

uint32_t coin_ntoh_uint32(uint32_t value) {
    return CoinTidbits::ByteOrder::networkToHost(value);
}

uint64_t coin_hton_uint64(uint64_t value) {
    return CoinTidbits::ByteOrder::hostToNetwork(value);
}

uint64_t coin_ntoh_uint64(uint64_t value) {
    return CoinTidbits::ByteOrder::networkToHost(value);
}

void coin_hton_float_bytes(float value, char* result) {
    CoinTidbits::ByteOrder::hostToNetworkBytes(value, result);
}

float coin_ntoh_float_bytes(const char* value) {
    return CoinTidbits::ByteOrder::networkToHostFloatBytes(value);
}

void coin_hton_double_bytes(double value, char* result) {
    CoinTidbits::ByteOrder::hostToNetworkBytes(value, result);
}

double coin_ntoh_double_bytes(const char* value) {
    return CoinTidbits::ByteOrder::networkToHostDoubleBytes(value);
}

// Power of two functions
SbBool coin_is_power_of_two(uint32_t x) {
    return CoinTidbits::Math::isPowerOfTwo(x);
}

uint32_t coin_next_power_of_two(uint32_t x) {
    return CoinTidbits::Math::nextPowerOfTwo(x);
}

uint32_t coin_geq_power_of_two(uint32_t x) {
    return CoinTidbits::Math::geqPowerOfTwo(x);
}

// Viewvolume jitter
void coin_viewvolume_jitter(int numpasses, int curpass, const int* vpsize, float* jitter) {
    CoinTidbits::Math::generateViewvolumeJitter(numpasses, curpass, vpsize, jitter);
}

// Cleanup functions
void cc_coin_atexit(coin_atexit_f* fp) {
    CoinTidbits::Cleanup::registerFunction(fp);
}

void cc_coin_atexit_static_internal(coin_atexit_f* fp) {
    CoinTidbits::Cleanup::registerFunction("static_internal", fp, CoinTidbits::Cleanup::Priority::StaticData);
}

void coin_atexit_func(const char* name, coin_atexit_f* fp, int priority) {
    auto modernPriority = static_cast<CoinTidbits::Cleanup::Priority>(priority);
    CoinTidbits::Cleanup::registerFunction(name, fp, modernPriority);
}

void coin_atexit_cleanup(void) {
    CoinTidbits::Cleanup::cleanup();
}

SbBool coin_is_exiting(void) {
    return CoinTidbits::Cleanup::isExiting();
}

// File functions
FILE* coin_get_stdin(void) {
    return CoinTidbits::FileSystem::getStdin();
}

FILE* coin_get_stdout(void) {
    return CoinTidbits::FileSystem::getStdout();
}

FILE* coin_get_stderr(void) {
    return CoinTidbits::FileSystem::getStderr();
}

// Locale functions (stubbed for now - would need cc_string implementation)
SbBool coin_locale_set_portable(cc_string* storeold) {
    return CoinTidbits::Locale::setPortable(storeold);
}

void coin_locale_reset(cc_string* storedold) {
    CoinTidbits::Locale::reset(storedold);
}

double coin_atof(const char* ptr) {
    return CoinTidbits::Misc::portableAtof(ptr);
}

// ASCII85 functions
void coin_output_ascii85(FILE* fp, const unsigned char val, unsigned char* tuple,
                        unsigned char* linebuf, int* tuplecnt, int* linecnt,
                        const int rowlen, const SbBool flush) {
    CoinTidbits::Misc::outputAscii85(fp, val, tuple, linebuf, tuplecnt, linecnt, rowlen, flush != 0);
}

void coin_flush_ascii85(FILE* fp, unsigned char* tuple, unsigned char* linebuf,
                       int* tuplecnt, int* linecnt, const int rowlen) {
    CoinTidbits::Misc::flushAscii85(fp, tuple, linebuf, tuplecnt, linecnt, rowlen);
}

// Version parsing
SbBool coin_parse_versionstring(const char* versionstr, int* major, int* minor, int* patch) {
    return CoinTidbits::Misc::parseVersionString(versionstr, major, minor, patch);
}

// Directory functions
SbBool coin_getcwd(cc_string* str) {
    // Would need cc_string implementation
    (void)str;
    return false;
}

// Math functions
int coin_isinf(double value) {
    return CoinTidbits::Math::isInfinite(value) ? 1 : 0;
}

int coin_isnan(double value) {
    return CoinTidbits::Math::isNaN(value) ? 1 : 0;
}

int coin_finite(double value) {
    return CoinTidbits::Math::isFinite(value) ? 1 : 0;
}

unsigned long coin_geq_prime_number(unsigned long num) {
    return CoinTidbits::Misc::getEqualOrGreaterPrime(num);
}

// OS detection
int coin_runtime_os(void) {
    auto osType = CoinTidbits::Platform::getRuntimeOS();
    switch (osType) {
        case CoinTidbits::Platform::OSType::Unix: return COIN_UNIX;
        case CoinTidbits::Platform::OSType::MacOSX: return COIN_OS_X;
        case CoinTidbits::Platform::OSType::Windows: return COIN_MSWINDOWS;
        default: return COIN_UNIX;
    }
}

// Debug functions
int coin_debug_extra(void) {
    return CoinTidbits::Debug::isExtraEnabled() ? 1 : 0;
}

int coin_debug_normalize(void) {
    return CoinTidbits::Debug::isNormalizeEnabled() ? 1 : 0;
}

int coin_debug_caching_level(void) {
    return CoinTidbits::Debug::getCachingLevel();
}

} // extern "C"