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

#ifndef COIN_TIDBITS_H
#define COIN_TIDBITS_H

/*!
 * \file CoinTidbits.h
 * \brief Consolidated C++17 tidbits utilities for Coin3D
 * 
 * This file consolidates all tidbits functionality into a single modern C++17
 * implementation while maintaining backward compatibility with the C API.
 * 
 * This consolidates functionality from:
 * - src/C/tidbits.h (legacy C API header)
 * - src/tidbitsp.h (private header with additional functions)
 * - src/C/CoinTidbits.h (previous modern header)
 * - include/Inventor/C/tidbits.h (public C API header)
 */

// Modern C++17 includes
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <memory>

// Coin includes
#include "C/basic.h"

// Forward declarations to avoid header conflicts
struct cc_string;

#ifndef SBBOOL_HEADER_FILE
typedef bool SbBool;
#endif

// ===== MODERN C++17 UTILITIES NAMESPACE =====

namespace CoinTidbits {

/*!
 * \brief Modern C++17 endianness utilities
 */
namespace Endianness {
    enum class Type : int {
        Unknown = -1,
        Little = 0,
        Big = 1
    };

    /// Get the host system's endianness at runtime
    Type getHostEndianness() noexcept;
    
    /// Check if host is big endian
    inline bool isBigEndian() noexcept {
        return getHostEndianness() == Type::Big;
    }
    
    /// Check if host is little endian
    inline bool isLittleEndian() noexcept {
        return getHostEndianness() == Type::Little;
    }
}

/*!
 * \brief Modern C++17 network byte order conversion utilities
 */
namespace ByteOrder {
    /// Convert 16-bit value to network byte order
    std::uint16_t hostToNetwork(std::uint16_t value) noexcept;
    /// Convert 16-bit value from network to host byte order
    std::uint16_t networkToHost(std::uint16_t value) noexcept;
    
    /// Convert 32-bit value to network byte order
    std::uint32_t hostToNetwork(std::uint32_t value) noexcept;
    /// Convert 32-bit value from network to host byte order
    std::uint32_t networkToHost(std::uint32_t value) noexcept;
    
    /// Convert 64-bit value to network byte order
    std::uint64_t hostToNetwork(std::uint64_t value) noexcept;
    /// Convert 64-bit value from network to host byte order
    std::uint64_t networkToHost(std::uint64_t value) noexcept;
    
    /// Convert float to network byte order (as raw bytes)
    void hostToNetworkBytes(float value, char* result) noexcept;
    /// Convert float from network byte order (from raw bytes)
    float networkToHostFloatBytes(const char* value) noexcept;
    
    /// Convert double to network byte order (as raw bytes)
    void hostToNetworkBytes(double value, char* result) noexcept;
    /// Convert double from network byte order (from raw bytes)
    double networkToHostDoubleBytes(const char* value) noexcept;
}

/*!
 * \brief Modern C++17 string formatting utilities
 */
namespace StringFormat {
    /// Safe sprintf replacement using C++17
    template<typename... Args>
    int safeSprintf(char* buffer, std::size_t size, const char* format, Args&&... args);
    
    /// Format string to std::string using C++17
    template<typename... Args>
    std::string formatString(const char* format, Args&&... args);
    
    /// Safe vsnprintf wrapper
    int safeVsnprintf(char* dst, unsigned int n, const char* fmtstr, va_list args);
}

/*!
 * \brief Modern C++17 environment variable utilities
 */
namespace Environment {
    /// Get environment variable as std::string (returns empty if not found)
    std::string getVariable(const std::string& name);
    
    /// Get environment variable as C string (nullptr if not found)
    const char* getVariableRaw(const char* name);
    
    /// Set environment variable
    bool setVariable(const std::string& name, const std::string& value, bool overwrite = true);
    
    /// Unset environment variable
    void unsetVariable(const std::string& name);
}

/*!
 * \brief Modern C++17 math utilities
 */
namespace Math {
    /// Check if value is power of two
    bool isPowerOfTwo(std::uint32_t x) noexcept;
    
    /// Get next power of two >= x
    std::uint32_t nextPowerOfTwo(std::uint32_t x) noexcept;
    
    /// Get power of two >= x
    std::uint32_t geqPowerOfTwo(std::uint32_t x) noexcept;
    
    /// Check if double is infinite
    bool isInfinite(double value) noexcept;
    
    /// Check if double is NaN
    bool isNaN(double value) noexcept;
    
    /// Check if double is finite
    bool isFinite(double value) noexcept;
    
    /// Generate viewvolume jitter values
    void generateViewvolumeJitter(int numpasses, int curpass, const int* vpsize, float* jitter);
}

/*!
 * \brief Modern C++17 cleanup/atexit utilities
 */
namespace Cleanup {
    using CleanupFunction = void(*)();
    
    enum class Priority : int {
        External = 2147483647,          // Client code cleanup
        Normal = 0,                     // Standard cleanup
        RealTimeField = 10,             // Before normal
        DraggerDefaults = 2,            // Before normal
        TrackSoBaseInstances = 1,       // Before normal
        NormalLowPriority = -1,         // After normal
        StaticData = -10,               // Static cleanup
        SoDB = -20,                     // SoDB cleanup
        SoBase = -30,                   // SoBase cleanup
        SoType = -40,                   // Type system cleanup
        FontSubsystem = -100,           // Font cleanup
        FontSubsystemHigh = -99,        // Font cleanup (high priority)
        FontSubsystemLow = -101,        // Font cleanup (low priority)
        MsgSubsystem = -200,            // Message system cleanup
        SbName = -500,                  // SbName dictionary cleanup
        ThreadingSubsystem = -1000,     // Threading cleanup
        ThreadingSubsystemLow = -1001,  // Threading cleanup (low)
        ThreadingSubsystemVeryLow = -1002, // Threading cleanup (very low)
        DynLibs = -2147483647,          // Dynamic libraries (last)
        Environment = -2147483637       // Environment cleanup
    };
    
    /// Register cleanup function with priority
    void registerFunction(const char* name, CleanupFunction func, Priority priority);
    
    /// Register cleanup function with normal priority
    void registerFunction(CleanupFunction func);
    
    /// Cleanup all registered functions (internal use)
    void cleanup();
    
    /// Check if we're in the process of exiting
    bool isExiting();
}

/*!
 * \brief File system utilities
 */
namespace FileSystem {
    /// Get current working directory
    std::string getCurrentWorkingDirectory();
    
    /// Get stdin FILE pointer
    FILE* getStdin() noexcept;
    
    /// Get stdout FILE pointer
    FILE* getStdout() noexcept;
    
    /// Get stderr FILE pointer  
    FILE* getStderr() noexcept;
}

/*!
 * \brief Platform detection utilities
 */
namespace Platform {
    enum class OSType {
        Unix,
        MacOSX,
        Windows
    };
    
    /// Get the runtime operating system
    OSType getRuntimeOS();
}

/*!
 * \brief Debug utilities
 */
namespace Debug {
    /// Check if extra debugging is enabled
    bool isExtraEnabled();
    
    /// Check if normalize debugging is enabled
    bool isNormalizeEnabled();
    
    /// Get caching debug level
    int getCachingLevel();
}

/*!
 * \brief Miscellaneous utilities
 */
namespace Misc {
    /// Find smallest prime number >= input
    unsigned long getEqualOrGreaterPrime(unsigned long num);
    
    /// Parse version string (major.minor.patch format)
    bool parseVersionString(const char* versionstr, int* major, int* minor = nullptr, int* patch = nullptr);
    
    /// Portable atof that doesn't depend on locale
    double portableAtof(const char* ptr);
    
    /// ASCII85 encoding for PostScript output
    void outputAscii85(FILE* fp, const unsigned char val, unsigned char* tuple,
                      unsigned char* linebuf, int* tuplecnt, int* linecnt,
                      const int rowlen, const bool flush);
    
    /// Flush ASCII85 encoding
    void flushAscii85(FILE* fp, unsigned char* tuple, unsigned char* linebuf,
                     int* tuplecnt, int* linecnt, const int rowlen);
}

/*!
 * \brief Locale management utilities
 */
namespace Locale {
    /// Set portable "C" locale for numeric operations, store old locale
    bool setPortable(cc_string* storeold);
    
    /// Reset locale to previously stored value
    void reset(cc_string* storedold);
}

} // namespace CoinTidbits

// ===== C API COMPATIBILITY LAYER =====

extern "C" {

// Legacy C type definitions for compatibility
typedef void coin_atexit_f(void);

// Legacy endianness enum
enum CoinEndiannessValues {
    COIN_HOST_IS_UNKNOWNENDIAN = -1,
    COIN_HOST_IS_LITTLEENDIAN = 0,
    COIN_HOST_IS_BIGENDIAN = 1
};

// Legacy atexit priorities enum (for backward compatibility)
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

// Core utility functions (public API)
COIN_DLL_API int coin_host_get_endianness(void);
COIN_DLL_API int coin_snprintf(char* dst, unsigned int n, const char* fmtstr, ...);
COIN_DLL_API int coin_vsnprintf(char* dst, unsigned int n, const char* fmtstr, va_list args);

COIN_DLL_API const char* coin_getenv(const char* name);
COIN_DLL_API SbBool coin_setenv(const char* name, const char* value, int overwrite);
COIN_DLL_API void coin_unsetenv(const char* name);

COIN_DLL_API uint16_t coin_hton_uint16(uint16_t value);
COIN_DLL_API uint16_t coin_ntoh_uint16(uint16_t value);
COIN_DLL_API uint32_t coin_hton_uint32(uint32_t value);
COIN_DLL_API uint32_t coin_ntoh_uint32(uint32_t value);
COIN_DLL_API uint64_t coin_hton_uint64(uint64_t value);
COIN_DLL_API uint64_t coin_ntoh_uint64(uint64_t value);

COIN_DLL_API void coin_hton_float_bytes(float value, char* result);
COIN_DLL_API float coin_ntoh_float_bytes(const char* value);
COIN_DLL_API void coin_hton_double_bytes(double value, char* result);
COIN_DLL_API double coin_ntoh_double_bytes(const char* value);

COIN_DLL_API SbBool coin_is_power_of_two(uint32_t x);
COIN_DLL_API uint32_t coin_next_power_of_two(uint32_t x);
COIN_DLL_API uint32_t coin_geq_power_of_two(uint32_t x);

COIN_DLL_API void coin_viewvolume_jitter(int numpasses, int curpass, const int* vpsize, float* jitter);

COIN_DLL_API void cc_coin_atexit(coin_atexit_f* fp);
COIN_DLL_API void cc_coin_atexit_static_internal(coin_atexit_f* fp);

// Private/internal functions (from tidbitsp.h)
void coin_init_tidbits(void);

FILE* coin_get_stdin(void);
FILE* coin_get_stdout(void);
FILE* coin_get_stderr(void);

// Atexit with priority support
#define coin_atexit(func, priority) \
        coin_atexit_func(SO__QUOTE(func), func, priority)

void coin_atexit_func(const char* name, coin_atexit_f* fp, int priority);
void coin_atexit_cleanup(void);
SbBool coin_is_exiting(void);

// Locale functions
SbBool coin_locale_set_portable(cc_string* storeold);
void coin_locale_reset(cc_string* storedold);
double coin_atof(const char* ptr);

// ASCII85 encoding functions
void coin_output_ascii85(FILE* fp, const unsigned char val, unsigned char* tuple,
                        unsigned char* linebuf, int* tuplecnt, int* linecnt,
                        const int rowlen, const SbBool flush);
void coin_flush_ascii85(FILE* fp, unsigned char* tuple, unsigned char* linebuf,
                       int* tuplecnt, int* linecnt, const int rowlen);

// Version parsing
SbBool coin_parse_versionstring(const char* versionstr, int* major, int* minor, int* patch);

// Utility functions
SbBool coin_getcwd(cc_string* str);
int coin_isinf(double value);
int coin_isnan(double value);
int coin_finite(double value);
unsigned long coin_geq_prime_number(unsigned long num);

// OS detection
enum CoinOSType {
    COIN_UNIX,
    COIN_OS_X,
    COIN_MSWINDOWS
};
int coin_runtime_os(void);

#define COIN_MAC_FRAMEWORK_IDENTIFIER_CSTRING ("org.coin3d.Coin.framework")

// Debug functions
int coin_debug_extra(void);
int coin_debug_normalize(void);
int coin_debug_caching_level(void);

} // extern "C"

// Include template implementations
#include "CoinTidbits.inl"

#endif // !COIN_TIDBITS_H